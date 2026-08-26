// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HTTPServer/HTTPServer.h"

#include <array>
#include <cstring>
#include <sstream>
#include <string_view>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#define CLOSE_SOCKET closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCKET close
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
typedef int SOCKET;
#endif

#include <fmt/format.h>

#include "Common/Logging/Log.h"
#include "Core/Config/MainSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HTTPServer/WebDashboard.h"
#include "Core/System.h"
#include "VideoCommon/PerformanceMetrics.h"

namespace Core
{

namespace
{

SOCKET CreateServerSocket(u16 port)
{
  SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == INVALID_SOCKET)
  {
    ERROR_LOG_FMT(CORE, "HTTPServer: Failed to create TCP socket");
    return INVALID_SOCKET;
  }

  int opt = 1;
#ifdef _WIN32
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) ==
          SOCKET_ERROR ||
      listen(server_fd, 10) == SOCKET_ERROR)
  {
    CLOSE_SOCKET(server_fd);
    return INVALID_SOCKET;
  }

  return server_fd;
}

HTTPRequest ParseRequest(std::string_view raw_request)
{
  std::istringstream stream{std::string(raw_request)};
  HTTPRequest request;
  std::string http_version;
  stream >> request.method >> request.path >> http_version;

  const size_t query_pos = request.path.find('?');
  if (query_pos != std::string::npos)
    request.path = request.path.substr(0, query_pos);

  return request;
}

std::string_view GetStatusText(int status_code)
{
  switch (status_code)
  {
  case 200:
    return "OK";
  case 204:
    return "No Content";
  case 400:
    return "Bad Request";
  case 404:
    return "Not Found";
  case 500:
    return "Internal Server Error";
  default:
    return "Unknown Status";
  }
}

std::string FormatResponse(const HTTPResponse& res)
{
  return fmt::format(
      "HTTP/1.1 {} {}\r\n"
      "Content-Type: {}\r\n"
      "Content-Length: {}\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
      "Access-Control-Allow-Headers: Content-Type\r\n"
      "Connection: close\r\n\r\n"
      "{}",
      res.status_code, GetStatusText(res.status_code), res.content_type, res.body.size(),
      res.body);
}

std::string_view StateToString(Core::State state)
{
  switch (state)
  {
  case State::Running:
    return "Running";
  case State::Paused:
    return "Paused";
  case State::Starting:
    return "Starting";
  case State::Stopping:
    return "Stopping";
  case State::Uninitialized:
  default:
    return "Uninitialized";
  }
}

}  // namespace

HTTPServer::HTTPServer() = default;

HTTPServer::~HTTPServer()
{
  Stop();
}

bool HTTPServer::Start(Core::System& system, u16 port)
{
  if (m_is_running)
    return true;

  m_port = port;
  m_socket_context.emplace();

  SOCKET server_fd = CreateServerSocket(m_port);
  if (server_fd == INVALID_SOCKET)
  {
    ERROR_LOG_FMT(CORE, "HTTPServer: Failed to create server socket on port {}", port);
    m_socket_context.reset();
    return false;
  }

  m_server_socket = static_cast<uintptr_t>(server_fd);
  m_is_running = true;

  RegisterDefaultRoutes(system);

  INFO_LOG_FMT(CORE, "HTTPServer: Listening on port {}", m_port);
  m_server_thread = std::make_unique<std::thread>(&HTTPServer::ServerLoop, this);
  return true;
}

void HTTPServer::Stop()
{
  if (!m_is_running)
    return;

  m_is_running = false;

  if (m_server_socket != 0)
  {
    CLOSE_SOCKET(static_cast<SOCKET>(m_server_socket));
    m_server_socket = 0;
  }

  if (m_server_thread && m_server_thread->joinable())
  {
    m_server_thread->join();
    m_server_thread.reset();
  }

  m_handlers.clear();
  m_routes.clear();
  m_socket_context.reset();
  INFO_LOG_FMT(CORE, "HTTPServer: Stopped");
}

void HTTPServer::RegisterHandler(const std::string& method, const std::string& path,
                                 HTTPHandler handler, const std::string& description)
{
  const std::string key = method + " " + path;
  m_handlers[key] = std::move(handler);
  m_routes.push_back({method, path, description});
}

void HTTPServer::RegisterDefaultRoutes(Core::System& system)
{
  RegisterHandler(
      "GET", "/api/status",
      [&system](const HTTPRequest&) -> HTTPResponse {
        const auto state = Core::GetState(system);
        const std::string game_id = SConfig::GetInstance().GetGameID();
        const std::string game_title = SConfig::GetInstance().GetTitleName();
        const double fps = system.GetPerfMetrics().GetFPS();
        const double speed = system.GetPerfMetrics().GetSpeed() * 100.0;

        const std::string json = fmt::format(
            "{{"
            "\"success\":true,"
            "\"state\":\"{}\","
            "\"game_id\":\"{}\","
            "\"game_title\":\"{}\","
            "\"fps\":{:.1f},"
            "\"speed_percent\":{:.1f}"
            "}}",
            StateToString(state), game_id, game_title, fps, speed);

        return HTTPResponse{200, "application/json; charset=utf-8", json};
      },
      "Returns the current emulation status and game information");

  RegisterHandler(
      "POST", "/api/pause",
      [&system](const HTTPRequest&) -> HTTPResponse {
        Core::SetState(system, State::Paused);
        return HTTPResponse{200, "application/json; charset=utf-8",
                            "{\"success\":true,\"state\":\"Paused\"}"};
      },
      "Pauses emulation");

  RegisterHandler(
      "POST", "/api/resume",
      [&system](const HTTPRequest&) -> HTTPResponse {
        Core::SetState(system, State::Running);
        return HTTPResponse{200, "application/json; charset=utf-8",
                            "{\"success\":true,\"state\":\"Running\"}"};
      },
      "Resumes emulation");

  RegisterHandler(
      "GET", "/api/routes",
      [this](const HTTPRequest&) -> HTTPResponse {
        std::string json = "{\"success\":true,\"routes\":[";
        for (size_t i = 0; i < m_routes.size(); ++i)
        {
          if (i > 0)
            json += ",";
          json += fmt::format(
              "{{\"method\":\"{}\",\"path\":\"{}\",\"description\":\"{}\"}}",
              m_routes[i].method, m_routes[i].path, m_routes[i].description);
        }
        json += "]}";
        return HTTPResponse{200, "application/json; charset=utf-8", json};
      },
      "Lists all registered HTTP endpoints");

  RegisterHandler(
      "GET", "/",
      [](const HTTPRequest&) -> HTTPResponse {
        return HTTPResponse{200, "text/html; charset=utf-8", std::string(DASHBOARD_HTML)};
      },
      "Interactive web dashboard showing emulator status and endpoints");
}

void HTTPServer::ServerLoop()
{
  while (m_is_running)
  {
    sockaddr_in client_address{};
    socklen_t client_address_len = sizeof(client_address);
    SOCKET client_fd = accept(static_cast<SOCKET>(m_server_socket),
                              reinterpret_cast<struct sockaddr*>(&client_address),
                              &client_address_len);
    if (client_fd == INVALID_SOCKET)
    {
      if (!m_is_running)
        break;
      continue;
    }

    HandleClient(static_cast<uintptr_t>(client_fd));
  }
}

void HTTPServer::HandleClient(uintptr_t client_socket)
{
  SOCKET client_fd = static_cast<SOCKET>(client_socket);
  std::array<char, 4096> buffer{};
  const int bytes_read = recv(client_fd, buffer.data(), static_cast<int>(buffer.size() - 1), 0);

  if (bytes_read <= 0)
  {
    CLOSE_SOCKET(client_fd);
    return;
  }

  const auto request = ParseRequest(std::string_view(buffer.data(), bytes_read));
  const std::string route = request.method + " " + request.path;
  HTTPResponse response;

  if (request.method == "OPTIONS")
  {
    response.status_code = 204;
  }
  else if (const auto it = m_handlers.find(route); it != m_handlers.end())
  {
    response = it->second(request);
  }
  else
  {
    response.status_code = 404;
    response.body = "{\"error\": \"Route not found\"}";
  }

  const std::string response_str = FormatResponse(response);
  send(client_fd, response_str.c_str(), static_cast<int>(response_str.size()), 0);

  CLOSE_SOCKET(client_fd);
}

}  // namespace Core
