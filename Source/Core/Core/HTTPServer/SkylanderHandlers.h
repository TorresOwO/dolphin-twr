// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Core
{
class HTTPServer;
class System;

void RegisterSkylanderHandlers(HTTPServer& server, System& system);

}  // namespace Core
