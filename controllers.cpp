#include "controllers.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_guid.h>
#include <SDL3/SDL_stdinc.h>

#include <imgui_impl_sdl3.h>

#include <cstdio>
#include <unordered_map>

#include "game.h"
#include "launcher_log.h"

namespace launcher {

namespace {

std::unordered_map<SDL_JoystickID, SDL_Gamepad*> g_open;
SDL_JoystickID g_navigating = 0;

void SetNavigating(SDL_JoystickID id, SDL_Gamepad* gamepad) {
  g_navigating = id;
  ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, gamepad ? &gamepad : nullptr, gamepad ? 1 : 0);
}

// The same report through DirectInput, for a guitar SDL's PS3 driver did not take (it asks the device for a feature
// report first): without a mapping of its own it is no gamepad at all.
constexpr char kPs3GuitarDirectInput[] =
    "PS3 Guitar Hero guitar,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,"
    "leftstick:b10,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,rightx:a2,righty:a3,start:b9,x:b3,y:b0,";

std::string GuidText(SDL_JoystickID id) {
  char guid[33] = {};
  SDL_GUIDToString(SDL_GetJoystickGUIDForID(id), guid, int(sizeof(guid)));
  return guid;
}

// Its mapping with yellow and blue put right. SDL's own has x:b2 and y:b3; any other (swapped already, as this runs
// again) stays as it is.
void MapPs3Guitar(SDL_JoystickID id) {
  std::string mapping;
  if (char* current = SDL_GetGamepadMappingForID(id)) {
    mapping = current;
    SDL_free(current);
    const size_t x = mapping.find(",x:b2,");
    const size_t y = mapping.find(",y:b3,");
    if (x == std::string::npos || y == std::string::npos) {
      return;
    }
    mapping[x + 4] = '3';
    mapping[y + 4] = '2';
  } else {
    mapping = GuidText(id) + "," + kPs3GuitarDirectInput;
  }
  if (SDL_SetGamepadMapping(id, mapping.c_str())) {
    Log("a PS3 Guitar Hero guitar: yellow and blue put right");
  } else {
    Log("a PS3 Guitar Hero guitar's mapping failed: %s", SDL_GetError());
  }
}

const char* KindText(SDL_JoystickType kind) {
  switch (kind) {
    case SDL_JOYSTICK_TYPE_GAMEPAD:
      return "gamepad";
    case SDL_JOYSTICK_TYPE_GUITAR:
      return "guitar";
    case SDL_JOYSTICK_TYPE_DRUM_KIT:
      return "drum kit";
    default:
      return "other";
  }
}

}  // namespace

void OpenControllers() {
  // Every joystick first: a PS3 guitar without a mapping is no gamepad until it has one.
  int count = 0;
  SDL_JoystickID* joysticks = SDL_GetJoysticks(&count);
  for (int i = 0; i < count; ++i) {
    if (IsPs3Guitar(joysticks[i])) {
      MapPs3Guitar(joysticks[i]);
    }
  }
  SDL_free(joysticks);

  std::unordered_map<SDL_JoystickID, SDL_Gamepad*> open;
  SDL_JoystickID first = 0;
  SDL_JoystickID* pads = SDL_GetGamepads(&count);
  for (int i = 0; i < count; ++i) {
    if (i == 0) first = pads[i];
    auto it = g_open.find(pads[i]);
    if (it != g_open.end()) {
      open[pads[i]] = it->second;
      g_open.erase(it);
    } else if (SDL_Gamepad* gamepad = SDL_OpenGamepad(pads[i])) {
      open[pads[i]] = gamepad;
    } else {
      Log("a controller did not open: %s", SDL_GetError());
    }
  }
  SDL_free(pads);
  // Before the ones gone close: ImGui holds a pointer to the one it reads.
  if (!open.count(g_navigating)) {
    auto it = open.find(first);
    SetNavigating(it != open.end() ? first : 0, it != open.end() ? it->second : nullptr);
  }
  for (const auto& gone : g_open) {
    SDL_CloseGamepad(gone.second);
  }
  g_open.swap(open);
}

void CloseControllers() {
  SetNavigating(0, nullptr);
  for (const auto& pad : g_open) {
    SDL_CloseGamepad(pad.second);
  }
  g_open.clear();
}

bool NavigatesWith(SDL_JoystickID id) { return g_navigating == id; }

void NavigateWith(SDL_JoystickID id) {
  auto it = g_open.find(id);
  if (id != g_navigating && it != g_open.end()) {
    SetNavigating(id, it->second);
  }
}

bool IsPs3Guitar(SDL_JoystickID id) {
  return game::kHasWhammy && SDL_GetJoystickVendorForID(id) == 0x12BA && SDL_GetJoystickProductForID(id) == 0x0100;
}

std::string ControllersText() {
  int count = 0;
  SDL_JoystickID* ids = SDL_GetJoysticks(&count);
  std::string list;
  for (int i = 0; i < count; ++i) {
    const char* name = SDL_GetJoystickNameForID(ids[i]);
    const char* type = SDL_GetGamepadStringForType(SDL_GetGamepadTypeForID(ids[i]));
    char usb[16];
    std::snprintf(usb, sizeof(usb), "%04X:%04X", SDL_GetJoystickVendorForID(ids[i]), SDL_GetJoystickProductForID(ids[i]));
    list += std::string(i ? ", " : "") + std::to_string(i + 1) + ". " + (name ? name : "?") + " (" +
            (SDL_IsGamepad(ids[i]) ? (type ? type : "?") : "no gamepad mapping") + ", " +
            KindText(SDL_GetJoystickTypeForID(ids[i])) + ", " + usb + ", " + GuidText(ids[i]) + ")";
  }
  SDL_free(ids);
  return list.empty() ? "none" : list;
}

}  // namespace launcher
