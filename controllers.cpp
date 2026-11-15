#include "controllers.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_guid.h>
#include <SDL3/SDL_stdinc.h>

#include <imgui_impl_sdl3.h>

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>

#include "game.h"
#include "launcher_log.h"

#if defined(LAUNCHER_HAS_GAMEINPUT)
#include "gameinput_instruments.h"
namespace gameinput = rex::input::sdl::gameinput;
#endif

namespace launcher {

namespace {

std::unordered_map<SDL_JoystickID, SDL_Gamepad*> g_open;
SDL_JoystickID g_navigating = 0;

// GameInput (Xbox One guitars): looked for once, at the first OpenControllers, and again after its installer.
bool g_gameinput_looked = false;
bool g_gameinput_on = false;
bool g_gameinput_failed = false;  // its installer ran and GameInput still does not answer
HANDLE g_gameinput_installer = nullptr;
// The Xbox One guitars open, and their GameInput devices (gameinput_instruments.h).
std::unordered_map<SDL_JoystickID, void*> g_gip_devices;

void LookForGameInput() {
  g_gameinput_looked = true;
#if defined(LAUNCHER_HAS_GAMEINPUT)
  if (!game::kHasWhammy) return;
  std::string status;
  g_gameinput_on = gameinput::Start(status);
  Log("GameInput 3: %s (Xbox One guitars and drum kits such as the PDP Riffmaster)%s",
      g_gameinput_on ? "on" : status.c_str(),
      g_gameinput_on ? "" : GameInputInstallerFound() ? "; its installer is beside the launcher" : "; no installer here");
#endif
}

void CloseGipDevices() {
#if defined(LAUNCHER_HAS_GAMEINPUT)
  for (const auto& device : g_gip_devices) gameinput::CloseDevice(device.second);
#endif
  g_gip_devices.clear();
}

std::filesystem::path LauncherDir() {
  wchar_t exe[MAX_PATH] = {};
  DWORD length = GetModuleFileNameW(nullptr, exe, MAX_PATH);
  return length ? std::filesystem::path(exe).parent_path() : std::filesystem::path();
}

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

namespace {

// A plain controller SDL reads through GameInput (its GUID's driver byte 'g'), not an instrument.
bool IsGameInputGamepad(SDL_JoystickID id) {
  const SDL_JoystickType type = SDL_GetJoystickTypeForID(id);
  return SDL_GetJoystickGUIDForID(id).data[14] == 'g' && type != SDL_JOYSTICK_TYPE_GUITAR &&
         type != SDL_JOYSTICK_TYPE_DRUM_KIT;
}

// Of SDL's gamepads, the GameInput twins: for each pair of USB IDs, as many of GameInput's as other drivers read
// (which of SDL's drivers takes a controller first varies, and an Xbox controller may come through GameInput alone).
std::vector<SDL_JoystickID> Twins(const SDL_JoystickID* pads, int count) {
  std::map<uint32_t, int> others;
  auto usb = [](SDL_JoystickID id) {
    return (uint32_t(SDL_GetJoystickVendorForID(id)) << 16) | SDL_GetJoystickProductForID(id);
  };
  for (int i = 0; pads && i < count; ++i) {
    if (!IsGameInputGamepad(pads[i])) others[usb(pads[i])]++;
  }
  std::vector<SDL_JoystickID> twins;
  for (int i = 0; pads && i < count; ++i) {
    if (IsGameInputGamepad(pads[i]) && others[usb(pads[i])] > 0) {
      others[usb(pads[i])]--;
      twins.push_back(pads[i]);
    }
  }
  return twins;
}

}  // namespace

bool IsGameInputTwin(SDL_JoystickID id) {
  int count = 0;
  SDL_JoystickID* pads = SDL_GetGamepads(&count);
  const std::vector<SDL_JoystickID> twins = Twins(pads, count);
  SDL_free(pads);
  return std::find(twins.begin(), twins.end(), id) != twins.end();
}

SDL_JoystickID* Gamepads(int* count) {
  int all = 0;
  SDL_JoystickID* pads = SDL_GetGamepads(&all);
  const std::vector<SDL_JoystickID> twins = Twins(pads, all);
  int kept = 0;
  for (int i = 0; pads && i < all; ++i) {
    if (std::find(twins.begin(), twins.end(), pads[i]) == twins.end()) pads[kept++] = pads[i];
  }
  if (pads) pads[kept] = 0;  // SDL's array ends with a 0
  if (count) *count = kept;
  return pads;
}

void OpenControllers() {
  if (!g_gameinput_looked) LookForGameInput();
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
  SDL_JoystickID* pads = Gamepads(&count);
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

  // The Xbox One guitars' GameInput devices: of those open, by SDL's path (the device's id).
  std::unordered_map<SDL_JoystickID, void*> gip;
  for (const auto& pad : g_open) {
    if (!IsXboxOneGuitar(pad.first)) continue;
    auto it = g_gip_devices.find(pad.first);
    if (it != g_gip_devices.end()) {
      gip[pad.first] = it->second;
      g_gip_devices.erase(it);
#if defined(LAUNCHER_HAS_GAMEINPUT)
    } else if (void* device = gameinput::OpenDevice(SDL_GetGamepadPath(pad.second))) {
      gip[pad.first] = device;
      Log("an Xbox One guitar: its tilt read through GameInput");
#endif
    }
  }
  CloseGipDevices();
  g_gip_devices.swap(gip);
}

void CloseControllers() {
  CloseGipDevices();
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
            KindText(SDL_GetJoystickTypeForID(ids[i])) + ", " + usb + ", " + GuidText(ids[i]) +
            (SDL_IsGamepad(ids[i]) && IsGameInputTwin(ids[i]) ? ", GameInput's twin: left out" : "") + ")";
  }
  SDL_free(ids);
  return list.empty() ? "none" : list;
}

GameInputState GameInputStatus() {
#if defined(LAUNCHER_HAS_GAMEINPUT)
  if (!game::kHasWhammy) return GameInputState::kNotNeeded;
  if (g_gameinput_installer) return GameInputState::kInstalling;
  return g_gameinput_on ? GameInputState::kOn : GameInputState::kMissing;
#else
  return GameInputState::kNotNeeded;
#endif
}

bool GameInputInstallerFound() {
  std::error_code error;
  return std::filesystem::is_regular_file(LauncherDir() / L"GameInputRedist.msi", error);
}

bool InstallGameInput() {
  if (g_gameinput_installer || !GameInputInstallerFound()) return false;
  // msiexec with its progress bar only (/passive): the package installs for the whole PC, so Windows asks for an
  // administrator's consent itself.
  std::wstring parameters = L"/i \"" + (LauncherDir() / L"GameInputRedist.msi").wstring() + L"\" /passive /norestart";
  SHELLEXECUTEINFOW info = {};
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOCLOSEPROCESS;
  info.lpVerb = L"open";
  info.lpFile = L"msiexec.exe";
  info.lpParameters = parameters.c_str();
  info.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExW(&info) || !info.hProcess) {
    Log("GameInput's installer did not start (%lu)", GetLastError());
    return false;
  }
  g_gameinput_installer = info.hProcess;
  Log("GameInput's installer started");
  return true;
}

bool GameInputInstallEnded(bool* installed) {
  if (!g_gameinput_installer || WaitForSingleObject(g_gameinput_installer, 0) != WAIT_OBJECT_0) return false;
  DWORD code = 1;
  GetExitCodeProcess(g_gameinput_installer, &code);
  CloseHandle(g_gameinput_installer);
  g_gameinput_installer = nullptr;
  // 0 installed, 3010 installed but asks for a restart, 1602 cancelled, 1638 a newer version is there already.
  *installed = code == 0 || code == 3010 || code == 1638;
  Log("GameInput's installer ended: %lu", code);
  return true;
}

void RestartGameInput() {
  CloseGipDevices();
#if defined(LAUNCHER_HAS_GAMEINPUT)
  gameinput::Stop();
#endif
  g_gameinput_on = false;
  LookForGameInput();
  g_gameinput_failed = !g_gameinput_on;
}

bool GameInputInstallFailed() { return g_gameinput_failed && !g_gameinput_on; }

bool IsXboxOneGuitar(SDL_JoystickID id) {
  if (!game::kHasWhammy || SDL_GetJoystickTypeForID(id) != SDL_JOYSTICK_TYPE_GUITAR ||
      SDL_GetJoystickGUIDForID(id).data[14] != 'g') {
    return false;  // 'g': SDL's GameInput driver
  }
  const Uint16 vendor = SDL_GetJoystickVendorForID(id);
  const Uint16 product = SDL_GetJoystickProductForID(id);
  return (vendor == 0x0E6F && (product == 0x0248 || product == 0x0170)) ||  // PDP Riffmaster, Jaguar
         (vendor == 0x0738 && product == 0x4161) ||                         // Mad Catz Stratocaster
         ((vendor == 0x3651 || vendor == 0x3958) && product == 0x4161);     // CRKD / RedOctane Stage Tour
}

int XboxOneGuitarTilt(SDL_JoystickID id) {
#if defined(LAUNCHER_HAS_GAMEINPUT)
  auto it = g_gip_devices.find(id);
  if (it == g_gip_devices.end()) return -1;
  std::optional<uint8_t> tilt = gameinput::ReadGuitarTilt(it->second);
  return tilt ? int(*tilt) : -1;
#else
  (void)id;
  return -1;
#endif
}

}  // namespace launcher
