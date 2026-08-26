// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HTTPServer/HTTPServer.h"

#include <cstring>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SSIZE_T ssize_t;
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

#include "Common/Logging/Log.h"

namespace Core
{

namespace
{

SOCKET CreateServerSocket(u16 port)
{
  SOCKET server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd == INVALID_SOCKET)
  {
    ERROR_LOG_FMT(CORE, "HTTPServer: Failed to create socket TCP");
    return INVALID_SOCKET;
  }

  int opt = 1;
#ifdef _WIN32
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) == SOCKET_ERROR ||
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

  size_t query_pos = request.path.find('?');
  if (query_pos != std::string::npos)
    request.path = request.path.substr(0, query_pos);

  return request;
}

std::string GetStatusText(int status_code)
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
  std::ostringstream response_stream;
  std::string status_text = GetStatusText(res.status_code);
  response_stream << "HTTP/1.1 " << res.status_code << " " << status_text << "\r\n"
                  << "Content-Type: " << res.content_type << "\r\n"
                  << "Content-Length: " << res.body.size() << "\r\n"
                  << "Access-Control-Allow-Origin: *\r\n"
                  << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                  << "Access-Control-Allow-Headers: Content-Type\r\n"
                  << "Connection: close\r\n\r\n"
                  << res.body;

  return response_stream.str();
}
}  // namespace

HTTPServer::HTTPServer() = default;

HTTPServer::~HTTPServer()
{
  Stop();
}

bool HTTPServer::Start(u16 port)
{
  if (m_is_running)
    return true;

  m_port = port;
  m_socket_context.emplace();

  SOCKET server_fd = CreateServerSocket(m_port);
  if (server_fd == INVALID_SOCKET)
  {
    ERROR_LOG_FMT(COMMON, "HTTPServer: Failed to create server socket on port {}", port);
    m_socket_context.reset();
    return false;
  }

  m_server_socket = (uintptr_t)server_fd;
  m_is_running = true;

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
    CLOSE_SOCKET((SOCKET)m_server_socket);
    m_server_socket = 0;
  }

  if (m_server_thread && m_server_thread->joinable())
  {
    m_server_thread->join();
    m_server_thread.reset();
  }

  m_socket_context.reset();
  INFO_LOG_FMT(CORE, "HTTPServer: Stopped");
}

void HTTPServer::RegisterHandler(const std::string& method, const std::string& path,
                                 HTTPHandler handler)
{
  std::string key = method + " " + path;
  m_handlers[key] = std::move(handler);
}

void HTTPServer::ServerLoop()
{
  while (m_is_running)
  {
    sockaddr_in client_address{};
    socklen_t client_address_len = sizeof(client_address);
    SOCKET client_fd =
        accept((SOCKET)m_server_socket, (struct sockaddr*)&client_address, &client_address_len);
    if (client_fd == INVALID_SOCKET)
    {
      if (!m_is_running)
        break;
      continue;
    }

    HandleClient((uintptr_t)client_fd);
  }
}

void HTTPServer::HandleClient(uintptr_t client_socket)
{
  SOCKET client_fd = (SOCKET)client_socket;
  std::array<char, 4096> buffer{};
  ssize_t bytes_read = recv(client_fd, buffer.data(), (int)buffer.size() - 1, 0);

  if (bytes_read <= 0)
  {
    CLOSE_SOCKET(client_fd);
    return;
  }

  auto request = ParseRequest(std::string_view(buffer.data(), bytes_read));

  std::string route = request.method + " " + request.path;
  HTTPResponse response;

  if (request.method == "OPTIONS")
  {
    response.status_code = 204;
  }
  else if (auto it = m_handlers.find(route); it != m_handlers.end())
  {
    response = it->second(request);
  }
  else
  {
    response.status_code = 404;
    response.body = "{\"error\": \"Route not found\"}";
  }

  std::string response_str = FormatResponse(response);
  send(client_fd, response_str.c_str(), (int)response_str.size(), 0);

  CLOSE_SOCKET(client_fd);
}

}  // namespace Core
