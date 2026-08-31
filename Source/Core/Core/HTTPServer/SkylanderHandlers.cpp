// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/HTTPServer/SkylanderHandlers.h"

#include <algorithm>
#include <array>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/StringUtil.h"
#include "Core/Config/MainSettings.h"
#include "Core/HTTPServer/HTTPServer.h"
#include "Core/IOS/USB/Emulated/Skylanders/Skylander.h"
#include "Core/IOS/USB/Emulated/Skylanders/SkylanderFigure.h"
#include "Core/System.h"

namespace Core
{

static std::string GetSkylandersDirectory()
{
  std::string path = Config::Get(Config::MAIN_SKYLANDERS_PATH);
  if (path.empty())
  {
    path = File::GetUserPath(D_USER_IDX) + "Skylanders" + DIR_SEP;
  }
  else if (path.back() != '/' && path.back() != '\\')
  {
    path += DIR_SEP;
  }
  File::CreateFullPath(path);
  return path;
}

static std::string SanitizeFilename(std::string_view name)
{
  std::string result(name);
  for (char& c : result)
  {
    if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' ||
        c == '>' || c == '|')
    {
      c = '_';
    }
  }
  return result;
}

static const char* GameToString(IOS::HLE::USB::Game game)
{
  switch (game)
  {
  case IOS::HLE::USB::Game::SpyrosAdv:
    return "Spyro's Adventure";
  case IOS::HLE::USB::Game::Giants:
    return "Giants";
  case IOS::HLE::USB::Game::SwapForce:
    return "SWAP Force";
  case IOS::HLE::USB::Game::TrapTeam:
    return "Trap Team";
  case IOS::HLE::USB::Game::Superchargers:
    return "SuperChargers";
  default:
    return "Unknown";
  }
}

static const char* ElementToString(IOS::HLE::USB::Element element)
{
  switch (element)
  {
  case IOS::HLE::USB::Element::Magic:
    return "Magic";
  case IOS::HLE::USB::Element::Fire:
    return "Fire";
  case IOS::HLE::USB::Element::Air:
    return "Air";
  case IOS::HLE::USB::Element::Life:
    return "Life";
  case IOS::HLE::USB::Element::Undead:
    return "Undead";
  case IOS::HLE::USB::Element::Earth:
    return "Earth";
  case IOS::HLE::USB::Element::Water:
    return "Water";
  case IOS::HLE::USB::Element::Tech:
    return "Tech";
  case IOS::HLE::USB::Element::Dark:
    return "Dark";
  case IOS::HLE::USB::Element::Light:
    return "Light";
  default:
  case IOS::HLE::USB::Element::Other:
    return "Other";
  }
}

static const char* TypeToString(IOS::HLE::USB::Type type)
{
  switch (type)
  {
  case IOS::HLE::USB::Type::Skylander:
    return "Core";
  case IOS::HLE::USB::Type::Giant:
    return "Giant";
  case IOS::HLE::USB::Type::Swapper:
    return "Swapper";
  case IOS::HLE::USB::Type::TrapMaster:
    return "Trap Master";
  case IOS::HLE::USB::Type::Mini:
    return "Mini";
  case IOS::HLE::USB::Type::Item:
    return "Item";
  case IOS::HLE::USB::Type::Trophy:
    return "Trophy";
  case IOS::HLE::USB::Type::Vehicle:
    return "Vehicle";
  case IOS::HLE::USB::Type::Trap:
    return "Trap";
  default:
  case IOS::HLE::USB::Type::Unknown:
    return "Unknown";
  }
}

static std::optional<std::string> ExtractJsonString(std::string_view json, std::string_view key)
{
  const std::string pattern = "\"" + std::string(key) + "\"";
  size_t pos = json.find(pattern);
  if (pos == std::string_view::npos)
    return std::nullopt;

  pos = json.find(':', pos + pattern.size());
  if (pos == std::string_view::npos)
    return std::nullopt;

  pos = json.find('"', pos + 1);
  if (pos == std::string_view::npos)
    return std::nullopt;

  const size_t start = pos + 1;
  const size_t end = json.find('"', start);
  if (end == std::string_view::npos)
    return std::nullopt;

  return std::string(json.substr(start, end - start));
}

static std::optional<int> ExtractJsonInt(std::string_view json, std::string_view key)
{
  const std::string pattern = "\"" + std::string(key) + "\"";
  size_t pos = json.find(pattern);
  if (pos == std::string_view::npos)
    return std::nullopt;

  pos = json.find(':', pos + pattern.size());
  if (pos == std::string_view::npos)
    return std::nullopt;

  pos = json.find_first_not_of(" \t\r\n", pos + 1);
  if (pos == std::string_view::npos)
    return std::nullopt;

  size_t end = json.find_first_of(",}\r\n \t", pos);
  if (end == std::string_view::npos)
    end = json.size();

  int val = 0;
  if (TryParse(std::string(json.substr(pos, end - pos)), &val))
    return val;

  return std::nullopt;
}

static std::string EscapeJsonString(std::string_view input)
{
  std::string output;
  output.reserve(input.size());
  for (char c : input)
  {
    if (c == '"')
      output += "\\\"";
    else if (c == '\\')
      output += "\\\\";
    else if (c == '\n')
      output += "\\n";
    else if (c == '\r')
      output += "\\r";
    else if (c == '\t')
      output += "\\t";
    else
      output += c;
  }
  return output;
}

static HTTPResponse HandleGetCatalog(const HTTPRequest& request)
{
  HTTPResponse response;
  response.content_type = "application/json";

  std::ostringstream json;
  json << "[\n";

  bool first = true;
  for (const auto& [id_pair, sky_data] : IOS::HLE::USB::list_skylanders)
  {
    if (!first)
      json << ",\n";
    first = false;

    json << fmt::format(
        "  {{\"id\": {}, \"variant\": {}, \"name\": \"{}\", \"game\": \"{}\", \"element\": "
        "\"{}\", \"type\": \"{}\"}}",
        id_pair.first, id_pair.second, EscapeJsonString(sky_data.name),
        GameToString(sky_data.game), ElementToString(sky_data.element),
        TypeToString(sky_data.type));
  }

  json << "\n]";
  response.body = json.str();
  return response;
}

static constexpr std::array<u32, 20> LEVEL_XP_THRESHOLDS = {
    0,       // Level 1
    1000,    // Level 2
    2200,    // Level 3
    3800,    // Level 4
    6000,    // Level 5
    9000,    // Level 6
    13000,   // Level 7
    18200,   // Level 8
    24800,   // Level 9
    33000,   // Level 10 (Max Spyro's Adventure)
    42700,   // Level 11
    53900,   // Level 12
    66600,   // Level 13
    80800,   // Level 14
    96500,   // Level 15 (Max Giants)
    113700,  // Level 16
    132400,  // Level 17
    152600,  // Level 18
    174300,  // Level 19
    197500   // Level 20 (Max SWAP Force+)
};

static u16 CalculateLevelFromXP(u32 xp)
{
  u16 level = 1;
  for (size_t i = 0; i < LEVEL_XP_THRESHOLDS.size(); i++)
  {
    if (xp >= LEVEL_XP_THRESHOLDS[i])
      level = static_cast<u16>(i + 1);
    else
      break;
  }
  return level;
}

static HTTPResponse HandleGetStatus(const HTTPRequest& request, System& system)
{
  HTTPResponse response;
  response.content_type = "application/json";

  auto& portal = system.GetSkylanderPortal();

  std::ostringstream json;
  json << "{\n";
  json << "  \"success\": true,\n";
  json << "  \"slots\": [\n";

  for (u8 i = 0; i < MAX_SKYLANDERS; i++)
  {
    if (i > 0)
      json << ",\n";

    auto* skylander = portal.GetSkylander(i);
    const bool occupied = (skylander != nullptr) && (skylander->status & 1) && skylander->figure;

    if (occupied)
    {
      const auto figure_data = skylander->figure->GetData();
      std::string name = "Unknown";
      const auto found = IOS::HLE::USB::list_skylanders.find(
          std::make_pair(figure_data.figure_id, figure_data.variant_id));
      if (found != IOS::HLE::USB::list_skylanders.end())
        name = found->second.name;

      std::string nickname;
      if (figure_data.normalized_type == IOS::HLE::USB::Type::Skylander)
      {
        std::u16string u16_nick;
        for (u16 c : figure_data.skylander_data.nickname)
        {
          if (c == 0)
            break;
          u16_nick.push_back(static_cast<char16_t>(c));
        }
        nickname = UTF16ToUTF8(u16_nick);
      }

      u16 money = 0;
      u32 experience = 0;
      u16 level = 1;
      if (figure_data.normalized_type == IOS::HLE::USB::Type::Skylander)
      {
        money = figure_data.skylander_data.money;
        experience = figure_data.skylander_data.experience;
        level = CalculateLevelFromXP(experience);
      }

      json << fmt::format(
          "    {{\"slot\": {}, \"occupied\": true, \"id\": {}, \"variant\": {}, \"name\": \"{}\", "
          "\"nickname\": \"{}\", \"experience\": {}, \"level\": {}, \"money\": {}}}",
          i, figure_data.figure_id, figure_data.variant_id, EscapeJsonString(name),
          EscapeJsonString(nickname), experience, level, money);
    }
    else
    {
      json << fmt::format("    {{\"slot\": {}, \"occupied\": false}}", i);
    }
  }

  json << "\n  ]\n}";
  response.body = json.str();
  return response;
}

static HTTPResponse HandleLoadSkylander(const HTTPRequest& request, System& system)
{
  HTTPResponse response;
  response.content_type = "application/json";

  const auto slot_opt = ExtractJsonInt(request.body, "slot");
  const u8 slot = slot_opt.has_value() ? static_cast<u8>(*slot_opt) : 0;

  if (slot >= MAX_SKYLANDERS)
  {
    response.status_code = 400;
    response.body = "{\"success\": false, \"error\": \"Invalid slot index (must be 0-15)\"}";
    return response;
  }

  const auto path_opt = ExtractJsonString(request.body, "path");
  const auto name_opt = ExtractJsonString(request.body, "name");
  const auto id_opt = ExtractJsonInt(request.body, "id");
  const auto var_opt = ExtractJsonInt(request.body, "variant");

  std::string file_path;
  u16 target_id = 0;
  u16 target_var = 0;
  std::string figure_name = "Skylander";

  if (path_opt.has_value() && !path_opt->empty())
  {
    file_path = *path_opt;
  }
  else if (name_opt.has_value() && !name_opt->empty())
  {
    figure_name = *name_opt;
    bool found = false;
    for (const auto& [id_pair, data] : IOS::HLE::USB::list_skylanders)
    {
      if (Common::CaseInsensitiveEquals(data.name, figure_name))
      {
        target_id = id_pair.first;
        target_var = id_pair.second;
        figure_name = data.name;
        found = true;
        break;
      }
    }

    if (!found)
    {
      response.status_code = 404;
      response.body = fmt::format(
          "{{\"success\": false, \"error\": \"Skylander '{}' not found in database\"}}",
          EscapeJsonString(figure_name));
      return response;
    }

    file_path = GetSkylandersDirectory() + SanitizeFilename(figure_name) + ".sky";
  }
  else if (id_opt.has_value())
  {
    target_id = static_cast<u16>(*id_opt);
    target_var = var_opt.has_value() ? static_cast<u16>(*var_opt) : 0;

    const auto found =
        IOS::HLE::USB::list_skylanders.find(std::make_pair(target_id, target_var));
    if (found != IOS::HLE::USB::list_skylanders.end())
      figure_name = found->second.name;
    else
      figure_name = fmt::format("Unknown_{}_{}", target_id, target_var);

    file_path = GetSkylandersDirectory() + SanitizeFilename(figure_name) + ".sky";
  }
  else
  {
    response.status_code = 400;
    response.body =
        "{\"success\": false, \"error\": \"Missing parameter: must provide 'name', 'id', or "
        "'path'\"}";
    return response;
  }

  // If file doesn't exist on disk, create it
  if (!File::Exists(file_path))
  {
    IOS::HLE::USB::SkylanderFigure figure(file_path);
    if (!figure.Create(target_id, target_var))
    {
      response.status_code = 500;
      response.body =
          fmt::format("{{\"success\": false, \"error\": \"Failed to create figure file '{}'\"}}",
                      EscapeJsonString(file_path));
      return response;
    }
    figure.Close();
  }

  File::IOFile sky_file(file_path, "r+b");
  if (!sky_file)
  {
    response.status_code = 500;
    response.body =
        fmt::format("{{\"success\": false, \"error\": \"Failed to open figure file '{}'\"}}",
                    EscapeJsonString(file_path));
    return response;
  }

  auto& portal = system.GetSkylanderPortal();
  portal.RemoveSkylander(slot);

  const u8 portal_slot = portal.LoadSkylander(
      std::make_unique<IOS::HLE::USB::SkylanderFigure>(std::move(sky_file)));

  if (portal_slot == 0xFF)
  {
    response.status_code = 500;
    response.body = "{\"success\": false, \"error\": \"Failed to place Skylander on portal\"}";
    return response;
  }

  response.body = fmt::format(
      "{{\"success\": true, \"slot\": {}, \"portalSlot\": {}, \"name\": \"{}\", \"path\": "
      "\"{}\"}}",
      slot, portal_slot, EscapeJsonString(figure_name), EscapeJsonString(file_path));
  return response;
}

static HTTPResponse HandleRemoveSkylander(const HTTPRequest& request, System& system)
{
  HTTPResponse response;
  response.content_type = "application/json";

  const auto slot_opt = ExtractJsonInt(request.body, "slot");
  const u8 slot = slot_opt.has_value() ? static_cast<u8>(*slot_opt) : 0;

  if (slot >= MAX_SKYLANDERS)
  {
    response.status_code = 400;
    response.body = "{\"success\": false, \"error\": \"Invalid slot index (must be 0-15)\"}";
    return response;
  }

  auto& portal = system.GetSkylanderPortal();
  const bool removed = portal.RemoveSkylander(slot);

  response.body = fmt::format("{{\"success\": {}, \"slot\": {}}}", removed ? "true" : "false", slot);
  return response;
}

static HTTPResponse HandleClearPortal(const HTTPRequest& request, System& system)
{
  HTTPResponse response;
  response.content_type = "application/json";

  auto& portal = system.GetSkylanderPortal();
  for (u8 i = 0; i < MAX_SKYLANDERS; i++)
  {
    portal.RemoveSkylander(i);
  }

  response.body = "{\"success\": true, \"message\": \"All figures removed from portal\"}";
  return response;
}

void RegisterSkylanderHandlers(HTTPServer& server, System& system)
{
  server.RegisterHandler("GET", "/api/skylanders",
                         [](const HTTPRequest& req) { return HandleGetCatalog(req); },
                         "Returns the catalog of all supported Skylanders.");

  server.RegisterHandler(
      "GET", "/api/skylanders/status",
      [&system](const HTTPRequest& req) { return HandleGetStatus(req, system); },
      "Returns the current status of all 16 portal slots and figures.");

  server.RegisterHandler(
      "POST", "/api/skylanders/load",
      [&system](const HTTPRequest& req) { return HandleLoadSkylander(req, system); },
      "Places a figure on a portal slot (by name, id/variant, or file path).");

  server.RegisterHandler(
      "POST", "/api/skylanders/remove",
      [&system](const HTTPRequest& req) { return HandleRemoveSkylander(req, system); },
      "Removes the figure from the specified portal slot.");

  server.RegisterHandler(
      "POST", "/api/skylanders/clear",
      [&system](const HTTPRequest& req) { return HandleClearPortal(req, system); },
      "Removes all figures from all portal slots.");
}

}  // namespace Core
