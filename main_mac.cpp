// The game's launcher on macOS: main.cpp's program (read it for the why of each step), with what differs here.
//
// - The app bundle's executable is the launcher; the game's is next to it (Contents/MacOS). Their settings, user data
//   and game files are in ~/Library/Application Support/<game::kDataFolder> (platform::DataFolder), where the game
//   looks for them too (src/startup.cpp), since a bundle is signed and may be read-only.
// - Drawn with Metal (ui/gfx_metal.mm). Sizes are in points, as SDL's windows and the SDK's window_width take them;
//   the window has high pixel density, and the scale follows its size in pixels as on Windows.
// - "Play" starts the game with posix_spawn; the launcher hides once the game's window is on screen and comes back
//   when it exits. An update opens the new version's disk image (updater.cpp) and the launcher closes.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <vector>

#include "app.h"
#include "game.h"
#include "platform.h"
#include "ui/gfx.h"
#include "ui/ui.h"

namespace {

using launcher::App;
namespace platform = launcher::platform;

constexpr float kDesignWidth = 1280.0f;
constexpr float kDesignHeight = 760.0f;
constexpr float kLayoutWidth = 1200.0f;
constexpr float kMinWidth = 1000.0f;
constexpr float kMinHeight = 600.0f;

float ScaleFor(int width, int height) {
  float scale = std::min(float(height) / kDesignHeight, float(width) / kLayoutWidth);
  return std::clamp(std::round(scale * 40.0f) / 40.0f, 0.5f, 4.0f);
}

bool SteamWantsFullscreen() {
  return platform::Environment("SteamDeck").value_or("") == "1" ||
         platform::Environment("SteamGamepadUI").value_or("") == "1";
}

// The launcher's arguments without those starting with one of `drop`.
std::vector<std::string> ArgumentsWithout(std::initializer_list<const char*> drop) {
  std::vector<std::string> kept;
  for (const std::string& argument : platform::Arguments()) {
    bool dropped = false;
    for (const char* prefix : drop) dropped = dropped || argument.rfind(prefix, 0) == 0;
    if (!dropped) kept.push_back(argument);
  }
  return kept;
}

std::string Formatted(const char* format, const char* a, const char* b = "") {
  char text[1024];
  std::snprintf(text, sizeof(text), format, a, b);
  return text;
}

// The launcher lays out in pixels (as on Windows, DPI-aware); SDL gives a Retina window's mouse in points. The mouse
// events go to ImGui in pixels.
void ScaleMouseEvent(SDL_Event& event, float density) {
  switch (event.type) {
    case SDL_EVENT_MOUSE_MOTION:
      event.motion.x *= density;
      event.motion.y *= density;
      event.motion.xrel *= density;
      event.motion.yrel *= density;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      event.button.x *= density;
      event.button.y *= density;
      break;
    case SDL_EVENT_MOUSE_WHEEL:
      event.wheel.mouse_x *= density;
      event.wheel.mouse_y *= density;
      break;
    default:
      break;
  }
}

void ShowError(const char* text) {
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, launcher::game::kLauncherName, text, nullptr);
}

}  // namespace

int main(int argc, char** argv) {
  platform::SetArguments(argc, argv);
  const std::filesystem::path data_dir = platform::DataFolder(launcher::game::kDataFolder);
  // SDL first: the settings list the monitors through it (settings.cpp, EnumerateMonitors), where Windows asks Win32.
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    ShowError(SDL_GetError());
    return 1;
  }
  launcher::Settings settings = launcher::LoadSettings(data_dir);
  uint32_t language = launcher::SystemLanguage();
  const launcher::Texts& t = launcher::TextsFor(language);

  // Centered on the primary display, at the size it had (or the mockup's), no larger than its usable area.
  SDL_DisplayID display = SDL_GetPrimaryDisplay();
  SDL_Rect work{0, 0, 1440, 900};
  SDL_GetDisplayUsableBounds(display, &work);
  launcher::Size logical = settings.o.ui_size.first > 0 && settings.o.ui_size.second > 0
                               ? settings.o.ui_size
                               : launcher::Size{int(kDesignWidth), int(kDesignHeight)};
  float width = float(logical.first);
  float height = float(logical.second);
  float fit = std::min({1.0f, float(work.w) * 0.92f / width, float(work.h) * 0.92f / height});
  int min_width = std::min(int(kMinWidth), work.w);
  int min_height = std::min(int(kMinHeight), work.h);
  int window_width = std::max(int(width * fit), min_width);
  int window_height = std::max(int(height * fit), min_height);

  SDL_Window* window = SDL_CreateWindow(launcher::game::kLauncherName, window_width, window_height,
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY |
                                            SDL_WINDOW_METAL);
  if (!window) {
    ShowError(SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_SetWindowPosition(window, work.x + (work.w - window_width) / 2, work.y + (work.h - window_height) / 2);
  SDL_SetWindowMinimumSize(window, min_width, min_height);
  bool fullscreen = settings.o.ui_fullscreen || SteamWantsFullscreen() || work.w < int(kMinWidth) ||
                    work.h < int(kMinHeight);
  if (settings.o.ui_maximized) SDL_MaximizeWindow(window);
  if (fullscreen) SDL_SetWindowFullscreen(window, true);

  if (!ui::gfx::Init(window)) {
    ShowError(Formatted(t.no_direct3d, launcher::game::kLauncherName).c_str());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.LogFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImGui::GetStyle().FontSizeBase = 16.0f;
  ImGui::GetStyle().Colors[ImGuiCol_NavCursor] = ImVec4(0, 0, 0, 0);
  ImGui_ImplSDL3_InitForMetal(window);
  if (!ui::LoadFonts()) {
    ShowError((std::string(launcher::game::kLauncherName) + ": the fonts are missing from the executable.").c_str());
    return 1;
  }

  App app(std::move(settings), language);
  app.SetWindow(window);
  auto save = [&] {
    SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    launcher::Options& o = app.settings().o;
    o.ui_maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    if (!(flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN))) {
      int w = 0;
      int h = 0;
      SDL_GetWindowSize(window, &w, &h);
      o.ui_size = {w, h};
    }
    return launcher::SaveOptions(app.settings());
  };
  platform::Process game;
  bool hidden = false;
  bool shown = false;
  bool primed = false;
  int idle_frames = 0;
  float scale = 0.0f;
  for (;;) {
    if (app.result() == App::Result::kPlay) {
      if (!save()) ShowError(Formatted(t.save_failed, launcher::game::kOptionsFile).c_str());
      ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, nullptr, 0);
      SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
      std::filesystem::path exe = platform::GameExecutable(launcher::game::kRecompExecutable);
      bool found = std::filesystem::exists(exe);
      if (!found) {
        ShowError(Formatted(t.game_missing, launcher::game::kRecompExecutable, launcher::game::kLauncherName).c_str());
      }
      std::string error;
      if (found && game.Start(exe, ArgumentsWithout({"--launcher_"}), data_dir, &error)) {
        app.GameStarted();
      } else {
        if (found) ShowError(("posix_spawn: " + error).c_str());
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoFirst, nullptr, 0);
        app.GameExited(0);
      }
    }
    if (app.GameRunning()) {
      app.GameTick();
      if (game.Exited()) {
        uint32_t exit_code = game.ExitCode();
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoFirst, nullptr, 0);
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        io.ClearInputKeys();
        io.ClearInputMouse();
        app.GameExited(exit_code);
        if (hidden) {
          SDL_ShowWindow(window);
          hidden = false;
        }
        SDL_RaiseWindow(window);
        platform::Activate();
        primed = false;
        idle_frames = 0;
      } else if (!hidden && game.WindowShown()) {
        SDL_HideWindow(window);
        hidden = true;
      }
      if (hidden) {
        // Nothing to draw: wake a few times a second to see whether the game has exited.
        SDL_WaitEventTimeout(nullptr, 250);
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        continue;
      }
    }

    if (idle_frames > 30 && !app.Busy()) SDL_WaitEventTimeout(nullptr, 250);
    bool input = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      input = true;
      if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) ui::gfx::Resize(event.window.data1, event.window.data2);
      if (app.GameRunning()) continue;
      if (event.type == SDL_EVENT_QUIT ||
          (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))) {
        app.Quit();
        break;
      }
      // F11, Alt+Enter, or the Mac's Ctrl+Cmd+F: fullscreen on and off.
      if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && !app.Listening() &&
          (event.key.key == SDLK_F11 || (event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT)) ||
           (event.key.key == SDLK_F && (event.key.mod & SDL_KMOD_GUI) && (event.key.mod & SDL_KMOD_CTRL)))) {
        fullscreen = !fullscreen;
        SDL_SetWindowFullscreen(window, fullscreen);
        if (!SteamWantsFullscreen()) {
          app.settings().o.ui_fullscreen = fullscreen;
          launcher::SaveOptions(app.settings());
        }
        continue;
      }
      ScaleMouseEvent(event, SDL_GetWindowPixelDensity(window));
      if (app.OnEvent(event)) continue;
      ImGui_ImplSDL3_ProcessEvent(&event);
    }
    idle_frames = input ? 0 : idle_frames + 1;
    if (app.result() == App::Result::kQuit || app.result() == App::Result::kRestart) break;
    if (app.result() == App::Result::kPlay) continue;
    if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) {
      SDL_WaitEventTimeout(nullptr, 100);
      continue;
    }

    int pixel_width = 0;
    int pixel_height = 0;
    SDL_GetWindowSizeInPixels(window, &pixel_width, &pixel_height);
    if (float s = ScaleFor(pixel_width, pixel_height); s != scale) {
      scale = s;
      ui::SetScale(scale);
      ui::ReleasePictures();
    }
    if (!primed && !ui::AnyGamepadButtonDown()) primed = true;
    if (primed && !app.Listening() && !app.GamepadLocked() && !app.GameRunning()) {
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    } else {
      io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    }

    ui::gfx::NewFrame();
    ImGui_ImplSDL3_NewFrame();
    // Pixels, as the launcher lays out (ScaleMouseEvent): no framebuffer scale.
    io.DisplaySize = ImVec2(float(pixel_width), float(pixel_height));
    io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
    if (SDL_GetMouseFocus() != window && !(SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(SDL_BUTTON_LEFT))) {
      // The backend's position for a mouse outside the window is in points: none instead.
      io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
    }
    ImGui::NewFrame();
    app.Frame(ImVec2(float(pixel_width), float(pixel_height)));
    ImGui::Render();
    ui::gfx::BeginFrame(ui::col::kBackground);
    ui::gfx::Render(ImGui::GetDrawData());
    ui::gfx::Present();
    if (!shown) {
      SDL_ShowWindow(window);
      SDL_RaiseWindow(window);
      shown = true;
    }
  }

  save();
  app.ReleaseTextures();
  ui::ReleasePictures();
  ImGui_ImplSDL3_Shutdown();
  ui::gfx::Shutdown();
  ImGui::DestroyContext();
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
