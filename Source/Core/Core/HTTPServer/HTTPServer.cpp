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
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HTTPServer/SkylanderHandlers.h"
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

  // Set accept() timeout so Stop() can unblock quickly (200ms)
  DWORD timeout_ms = 200;
  setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms),
             sizeof(timeout_ms));
#else
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // Set accept() timeout so Stop() can unblock quickly (200ms)
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 200000;  // 200ms
  setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
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

  const size_t body_pos = raw_request.find("\r\n\r\n");
  if (body_pos != std::string_view::npos)
  {
    request.body = std::string(raw_request.substr(body_pos + 4));
  }
  else
  {
    const size_t alt_body_pos = raw_request.find("\n\n");
    if (alt_body_pos != std::string_view::npos)
      request.body = std::string(raw_request.substr(alt_body_pos + 2));
  }

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

  // Shutdown the socket first to unblock accept() immediately on all platforms
  if (m_server_socket != 0)
  {
#ifdef _WIN32
    shutdown(static_cast<SOCKET>(m_server_socket), SD_BOTH);
#else
    shutdown(static_cast<SOCKET>(m_server_socket), SHUT_RDWR);
#endif
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

  RegisterSkylanderHandlers(*this, system);
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
  std::string raw_data;
  std::array<char, 4096> buffer{};

  int bytes_read = recv(client_fd, buffer.data(), static_cast<int>(buffer.size()), 0);
  if (bytes_read <= 0)
  {
    CLOSE_SOCKET(client_fd);
    return;
  }
  raw_data.append(buffer.data(), bytes_read);

  // Check if we have received complete headers
  size_t header_end = raw_data.find("\r\n\r\n");
  size_t header_len = 4;
  if (header_end == std::string::npos)
  {
    header_end = raw_data.find("\n\n");
    header_len = 2;
  }

  // Parse Content-Length if present
  if (header_end != std::string::npos)
  {
    std::string_view headers(raw_data.data(), header_end);
    size_t cl_pos = headers.find("Content-Length:");
    if (cl_pos == std::string_view::npos)
      cl_pos = headers.find("content-length:");
    if (cl_pos != std::string_view::npos)
    {
      size_t val_start = headers.find_first_not_of(" \t", cl_pos + 15);
      if (val_start != std::string_view::npos)
      {
        size_t val_end = headers.find_first_of("\r\n", val_start);
        int cl = 0;
        if (TryParse(std::string(headers.substr(val_start, val_end - val_start)), &cl) && cl > 0)
        {
          const size_t content_length = static_cast<size_t>(cl);
          size_t body_received = raw_data.size() - (header_end + header_len);
          while (body_received < content_length)
          {
            const size_t to_read = std::min(buffer.size(), content_length - body_received);
            int extra_read = recv(client_fd, buffer.data(), static_cast<int>(to_read), 0);
            if (extra_read <= 0)
              break;
            raw_data.append(buffer.data(), extra_read);
            body_received += extra_read;
          }
        }
      }
    }
  }

  const auto request = ParseRequest(raw_data);
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
