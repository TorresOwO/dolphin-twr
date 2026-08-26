// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/SocketContext.h"

namespace Core
{
class System;

struct HTTPRequest
{
  std::string method;
  std::string path;
  std::string body;
};

struct HTTPResponse
{
  int status_code = 200;
  std::string content_type = "application/json; charset=utf-8";
  std::string body;
};

struct RouteInfo
{
  std::string method;
  std::string path;
  std::string description;
};

using HTTPHandler = std::function<HTTPResponse(const HTTPRequest&)>;

class HTTPServer
{
public:
  HTTPServer();
  ~HTTPServer();

  bool Start(Core::System& system, u16 port = 9090);
  void Stop();
  bool IsRunning() const { return m_is_running; }

  void RegisterHandler(const std::string& method, const std::string& path, HTTPHandler handler,
                       const std::string& description = "");

private:
  void RegisterDefaultRoutes(Core::System& system);
  void ServerLoop();
  void HandleClient(uintptr_t client_socket);

  bool m_is_running = false;
  u16 m_port = 9090;
  uintptr_t m_server_socket = 0;

  std::optional<Common::SocketContext> m_socket_context;
  std::unique_ptr<std::thread> m_server_thread;
  std::map<std::string, HTTPHandler> m_handlers;
  std::vector<RouteInfo> m_routes;
};

}  // namespace Core