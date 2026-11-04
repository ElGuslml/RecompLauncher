// The game's launcher (game.h names it and the game): options, the first run from the player's ISO,
// achievements and updates, in a separate executable, so that the game's own starts right away (for example
// from Steam Big Picture).
//
// Launcher 2 (made for POTCAWERecomp; the RecompLauncher repository, a submodule of every game, launcher_version.h):
// Gustavo's mockup on SDL3 and Dear ImGui, drawn with Direct3D 11, all from the SDK tree and linked statically, so it
// is one executable without the SDK's DLLs.
// It reads the game's base settings and the options file (game::kConfigFile, game::kOptionsFile, next to this
// exe and the game's), saves every change to the options file at once (settings.cpp), which the game loads at
// startup (src/options.cpp), and works with the mouse, the keyboard and any controller SDL knows (the
// DualSense included).
//
// The window scales with its size (the mockup's 1280x760 at 100 %) and fills the screen by default (always on the
// Steam Deck and in Big Picture); F11 or Alt+Enter switch it. The texts (English, German, French, Spanish, Italian)
// are in the language chosen in Settings > Game, or else the Windows display language's; the game's own language is
// another row there, or the game asks for it in its first menu.
//
// "Play" starts the game (with the arguments the launcher received) and the launcher stays:
// it shows "Starting the game…" until the game's window appears, hides, and comes back on the Play page when the
// game exits (with a notice if it closed with an error); the time between goes to the profile's playtime (App::GameTick,
// App::GameExited). So the launcher and the game look like one program, and
// Steam sees it running from the launcher's start to its end. The game runs in a job object that is closed with
// the launcher: if Steam stops the launcher, the game stops too. The launcher lets go of the controllers while the
// game runs.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellscalingapi.h>

#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED  // wWinMain is this file's; SDL is told with SDL_SetMainReady()
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_sdl3.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <initializer_list>
#include <string>
#include <unordered_map>

#include "app.h"
#include "game.h"
#include "launcher_log.h"
#include "launcher_version.h"
#include "ui/gfx.h"
#include "ui/ui.h"

namespace {

using launcher::App;

// The mockup's window and the narrowest layout the pages are made for, in dp: the scale follows the
// window's height, unless the window is narrower than that layout.
constexpr float kDesignWidth = 1280.0f;
constexpr float kDesignHeight = 760.0f;
constexpr float kLayoutWidth = 1200.0f;
// The smallest window, in dp.
constexpr float kMinWidth = 1000.0f;
constexpr float kMinHeight = 600.0f;

float ScaleFor(int width, int height) {
  float scale = std::min(float(height) / kDesignHeight, float(width) / kLayoutWidth);
  // In steps of 1/40: fonts and icons are drawn again only when it changes.
  return std::clamp(std::round(scale * 40.0f) / 40.0f, 0.5f, 4.0f);
}

bool EnvironmentIs(const char* name, const char* value) {
  char text[64];
  DWORD length = GetEnvironmentVariableA(name, text, sizeof(text));
  return length > 0 && length < sizeof(text) && std::string(text, length) == value;
}

// Steam sets SteamDeck=1 on the Steam Deck and SteamGamepadUI=1 in Big Picture (Proton passes both to the
// Windows environment): the launcher fills the screen there.
bool SteamWantsFullscreen() { return EnvironmentIs("SteamDeck", "1") || EnvironmentIs("SteamGamepadUI", "1"); }

std::filesystem::path ExecutableFolder() {
  std::wstring path(MAX_PATH, L'\0');
  for (;;) {
    DWORD length = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    if (length < path.size()) {
      path.resize(length);
      break;
    }
    path.resize(path.size() * 2);
  }
  return std::filesystem::path(path).parent_path();
}

// What follows the program name on the command line, verbatim (with the leading space).
std::wstring ForwardedArguments() {
  const wchar_t* p = GetCommandLineW();
  if (*p == L'"') {
    ++p;
    while (*p && *p != L'"') {
      ++p;
    }
    if (*p) {
      ++p;
    }
  } else {
    while (*p && *p != L' ' && *p != L'\t') {
      ++p;
    }
  }
  return p;
}

// The arguments the launcher received, each with its leading space, without those starting with one of `drop`
// (a quoted argument stays whole).
std::wstring ArgumentsWithout(std::initializer_list<const wchar_t*> drop) {
  std::wstring all = ForwardedArguments();
  std::wstring kept;
  size_t at = 0;
  while (at < all.size()) {
    while (at < all.size() && (all[at] == L' ' || all[at] == L'\t')) ++at;
    if (at >= all.size()) break;
    size_t end = at;
    bool quoted = false;
    while (end < all.size() && (quoted || (all[end] != L' ' && all[end] != L'\t'))) {
      if (all[end] == L'"') quoted = !quoted;
      ++end;
    }
    std::wstring argument = all.substr(at, end - at);
    bool dropped = false;
    for (const wchar_t* prefix : drop) {
      dropped = dropped || argument.rfind(prefix, 0) == 0;
    }
    if (!dropped) kept += L" " + argument;
    at = end;
  }
  return kept;
}

// A text with its %s filled.
std::string Formatted(const char* format, const char* a, const char* b = "") {
  char text[1024];
  std::snprintf(text, sizeof(text), format, a, b);
  return text;
}

void ShowError(const char* text) {
  launcher::Log("error shown: %s", text);
  launcher::SetWaiting(true);  // the message box waits for the player
  SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, launcher::game::kLauncherName, text, nullptr);
  launcher::SetWaiting(false);
}

// A program the launcher starts and stays for: the game from Play, or the launcher's new version after an update.
class GameProcess {
 public:
  ~GameProcess() { Close(); }

  // `arguments` with their leading spaces. False with a message shown when it could not start.
  bool Start(const std::filesystem::path& exe, const std::wstring& arguments) {
    Close();
    std::wstring command_line = L"\"" + exe.wstring() + L"\"" + arguments;
    // Killed with the launcher: when Steam stops the program it started (this one), the game goes too. Nested
    // in Steam's own job, if any (Windows 8 and later).
    job_ = CreateJobObjectW(nullptr, nullptr);
    if (job_) {
      JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
      limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
      SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    }
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {};
    AllowSetForegroundWindow(ASFW_ANY);
    std::filesystem::path folder = exe.parent_path();
    if (!CreateProcessW(exe.c_str(), command_line.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED, nullptr,
                        folder.c_str(), &startup, &process)) {
      std::string text = "CreateProcess: " + std::to_string(GetLastError());
      ShowError(text.c_str());
      Close();
      return false;
    }
    if (job_ && !AssignProcessToJobObject(job_, process.hProcess)) {
      CloseHandle(job_);  // the game still runs, only not tied to the launcher
      job_ = nullptr;
    }
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    process_ = process.hProcess;
    id_ = process.dwProcessId;
    return true;
  }

  HANDLE handle() const { return process_; }
  uint32_t id() const { return id_; }
  bool Exited() const { return !process_ || WaitForSingleObject(process_, 0) == WAIT_OBJECT_0; }
  uint32_t ExitCode() const {
    DWORD code = 0;
    return process_ && GetExitCodeProcess(process_, &code) ? uint32_t(code) : 0;
  }

  // A window of the game is on screen: its own, or a message box it shows before it (a different version of
  // default.xex).
  bool WindowShown() const {
    struct Search {
      DWORD id;
      bool found;
    } search{id_, false};
    EnumWindows(
        [](HWND hwnd, LPARAM data) -> BOOL {
          auto* s = reinterpret_cast<Search*>(data);
          DWORD id = 0;
          GetWindowThreadProcessId(hwnd, &id);
          if (id != s->id || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) ||
              (GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOOLWINDOW)) {
            return TRUE;
          }
          s->found = true;
          return FALSE;
        },
        LPARAM(&search));
    return search.found;
  }

  void Close() {
    if (process_) CloseHandle(process_);
    if (job_) CloseHandle(job_);
    process_ = nullptr;
    job_ = nullptr;
    id_ = 0;
  }

 private:
  HANDLE process_ = nullptr;
  HANDLE job_ = nullptr;
  DWORD id_ = 0;
};

// The launcher's window in front again after the game: Windows lets only the foreground window's thread give the
// foreground away, so this one joins its input for the moment (the game's window is gone; Big Picture or the
// desktop has it).
void BringToFront(HWND hwnd) {
  HWND foreground = GetForegroundWindow();
  DWORD foreground_thread = foreground ? GetWindowThreadProcessId(foreground, nullptr) : 0;
  DWORD this_thread = GetCurrentThreadId();
  bool attached = foreground_thread && foreground_thread != this_thread &&
                  AttachThreadInput(foreground_thread, this_thread, TRUE);
  BringWindowToTop(hwnd);
  SetForegroundWindow(hwnd);
  if (attached) {
    AttachThreadInput(foreground_thread, this_thread, FALSE);
  }
}

// Windows' version, for the log (GetVersionEx tells programs without a manifest entry 6.2).
std::string WindowsVersion() {
  using RtlGetVersionFunction = LONG(WINAPI*)(OSVERSIONINFOW*);
  OSVERSIONINFOW version = {sizeof(version)};
  auto get_version = reinterpret_cast<RtlGetVersionFunction>(
      reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")));
  if (!get_version || get_version(&version) != 0) {
    return "?";
  }
  return std::to_string(version.dwMajorVersion) + "." + std::to_string(version.dwMinorVersion) + "." +
         std::to_string(version.dwBuildNumber);
}

// The controllers SDL has, in its order: ImGui navigates with the first (ImGui_ImplSDL3_GamepadMode_AutoFirst).
void LogGamepads(const char* when) {
  int count = 0;
  SDL_JoystickID* pads = SDL_GetGamepads(&count);
  std::string list;
  for (int i = 0; pads && i < count; ++i) {
    const char* name = SDL_GetGamepadNameForID(pads[i]);
    const char* type = SDL_GetGamepadStringForType(SDL_GetGamepadTypeForID(pads[i]));
    list += std::string(i ? ", " : "") + std::to_string(i + 1) + ". " + (name ? name : "?") + " (" +
            (type ? type : "?") + ")";
  }
  SDL_free(pads);
  launcher::Log("controllers %s: %s", when, list.empty() ? "none" : list.c_str());
}

// The kind of an input event, for the log's first input after the game; null for the others.
const char* InputKind(uint32_t type) {
  switch (type) {
    case SDL_EVENT_KEY_DOWN:
      return "a key";
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      return "a mouse button";
    case SDL_EVENT_MOUSE_WHEEL:
      return "the mouse wheel";
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
      return "a controller button";
    default:
      return nullptr;
  }
}

// The window that has the foreground, for the log: its program and class.
std::string ForegroundText() {
  HWND foreground = GetForegroundWindow();
  if (!foreground) {
    return "no window";
  }
  DWORD pid = 0;
  GetWindowThreadProcessId(foreground, &pid);
  std::wstring program = L"?";
  if (HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
    wchar_t path[MAX_PATH];
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(process, 0, path, &size)) {
      program = std::filesystem::path(std::wstring(path, size)).filename().wstring();
    }
    CloseHandle(process);
  }
  wchar_t class_name[128] = {};
  GetClassNameW(foreground, class_name, 128);
  return launcher::LogText(program) + " (" + launcher::LogText(class_name) + ")";
}

// Whether an event can change what the launcher shows. A controller's stick or trigger moving inside ImGui's dead zone
// (8000; a DualSense's sticks move by a few units at rest, all the time) and its sensors and touchpad do not: counted as
// input, they kept the launcher drawing at the display's rate, a CPU core busy (Gustavo, 2026-11-04). A stick crossing
// the dead zone does, either way, and one held outside it keeps the menus repeating.
bool ChangesSomething(const SDL_Event& event, std::unordered_map<uint64_t, bool>& outside) {
  constexpr int kDeadZone = 8000;
  auto axis = [&outside](uint64_t key, int value) {
    bool out = std::abs(value) > kDeadZone;
    bool& was = outside[key];
    bool changed = out != was;
    was = out;
    return changed;  // a stick held over is GamepadHeld's (the left one); the right one rests over on a guitar
  };
  switch (event.type) {
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      return axis((uint64_t(event.gaxis.which) << 8) | event.gaxis.axis, event.gaxis.value);
    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
      return axis((uint64_t(event.jaxis.which) << 8) | 0x80 | event.jaxis.axis, event.jaxis.value);
    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
    case SDL_EVENT_SENSOR_UPDATE:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
    case SDL_EVENT_JOYSTICK_BATTERY_UPDATED:
      return false;
    default:
      return true;
  }
}

// A control that navigates held (as ImGui last read them: the D-pad, the face buttons, LB and RB, the left stick): the
// menus repeat, so keep drawing. Not the right stick or the triggers: a guitar's whammy bar rests on the right stick,
// fully over, and kept the launcher drawing all the time (launcher 3.11.4).
bool GamepadHeld() {
  constexpr ImGuiKey kNavigation[] = {
      ImGuiKey_GamepadDpadUp,     ImGuiKey_GamepadDpadDown,    ImGuiKey_GamepadDpadLeft,  ImGuiKey_GamepadDpadRight,
      ImGuiKey_GamepadFaceDown,   ImGuiKey_GamepadFaceRight,   ImGuiKey_GamepadFaceLeft,  ImGuiKey_GamepadFaceUp,
      ImGuiKey_GamepadL1,         ImGuiKey_GamepadR1,          ImGuiKey_GamepadStart,     ImGuiKey_GamepadBack,
      ImGuiKey_GamepadLStickUp,   ImGuiKey_GamepadLStickDown,  ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight};
  for (ImGuiKey key : kNavigation) {
    if (ImGui::IsKeyDown(key)) {
      return true;
    }
  }
  return false;
}

const char* FocusText(SDL_Window* window) {
  return SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS ? "with the keyboard focus"
                                                             : "without the keyboard focus";
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  // Physical pixels everywhere (monitors, modes, window) and sharp with display scaling.
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  std::filesystem::path exe_dir = ExecutableFolder();
  launcher::OpenLog(exe_dir);
  launcher::Log("%s %s, %s %s, Windows %s", launcher::game::kLauncherName, RECOMP_LAUNCHER_VERSION_STR,
                launcher::game::kRecompExecutable, launcher::game::kVersion, WindowsVersion().c_str());
  launcher::Log("folder: %s", launcher::LogText(exe_dir.wstring()).c_str());
  if (std::wstring arguments = ForwardedArguments(); !arguments.empty()) {
    launcher::Log("arguments:%s", launcher::LogText(arguments).c_str());
  }
  launcher::Settings settings = launcher::LoadSettings(exe_dir);
  uint32_t language = launcher::TextsLanguage(settings.o.launcher_language);
  const launcher::Texts& t = launcher::TextsFor(language);

  SDL_SetMainReady();
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    ShowError(SDL_GetError());
    return 1;
  }

  // On the monitor of the active window (Big Picture, Explorer...), centered, at the size it had (or the
  // mockup's) times the monitor's scale, and no larger than its work area.
  HMONITOR monitor = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO monitor_info = {sizeof(monitor_info)};
  GetMonitorInfoW(monitor, &monitor_info);
  const RECT& work = monitor_info.rcWork;
  int work_width = work.right - work.left;
  int work_height = work.bottom - work.top;
  float dpi_scale = float(GetDpiForSystem()) / 96.0f;
  if (UINT dpi = 0; SUCCEEDED(GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi, &dpi))) {
    dpi_scale = float(dpi) / 96.0f;
  }
  launcher::Size logical = settings.o.ui_size.first > 0 && settings.o.ui_size.second > 0
                               ? settings.o.ui_size
                               : launcher::Size{int(kDesignWidth), int(kDesignHeight)};
  float width = float(logical.first) * dpi_scale;
  float height = float(logical.second) * dpi_scale;
  float fit = std::min({1.0f, float(work_width) * 0.92f / width, float(work_height) * 0.92f / height});
  int window_width = int(width * fit);
  int window_height = int(height * fit);
  int min_width = std::min(int(kMinWidth * dpi_scale), work_width);
  int min_height = std::min(int(kMinHeight * dpi_scale), work_height);
  window_width = std::max(window_width, min_width);
  window_height = std::max(window_height, min_height);

  SDL_Window* window = SDL_CreateWindow(launcher::game::kLauncherName, window_width, window_height,
                                        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!window) {
    ShowError(SDL_GetError());
    SDL_Quit();
    return 1;
  }
  SDL_SetWindowPosition(window, work.left + (work_width - window_width) / 2, work.top + (work_height - window_height) / 2);
  SDL_SetWindowMinimumSize(window, min_width, min_height);
  bool fullscreen = settings.o.ui_fullscreen || SteamWantsFullscreen() ||
                    work_width < int(kMinWidth * dpi_scale) || work_height < int(kMinHeight * dpi_scale);
  if (settings.o.ui_maximized) {
    SDL_MaximizeWindow(window);
  }
  if (fullscreen) {
    SDL_SetWindowFullscreen(window, true);
  }

  HWND hwnd = HWND(SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
  launcher::Log("window %dx%d on a %dx%d work area at %.0f %%%s%s", window_width, window_height, work_width,
                work_height, dpi_scale * 100.0f, fullscreen ? ", fullscreen" : "",
                settings.o.ui_maximized ? ", maximized" : "");
  // A dump when the window stops answering or the closing takes too long (launcher_log.h). Declared before the App:
  // it outlives the App's background tasks, whose futures wait when the App goes.
  launcher::Watchdog watchdog(hwnd);
  if (!ui::gfx::Init(hwnd)) {
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
  // The widgets draw their own focus ring (ui::FocusRing).
  ImGui::GetStyle().Colors[ImGuiCol_NavCursor] = ImVec4(0, 0, 0, 0);
  ImGui_ImplSDL3_InitForD3D(window);
  ImGui_ImplDX11_Init(ui::gfx::Device(), ui::gfx::Context());
  if (!ui::LoadFonts()) {
    ShowError((std::string(launcher::game::kLauncherName) + ": the fonts are missing from the executable.").c_str());
    return 1;
  }

  App app(std::move(settings), language);
  app.SetWindow(window);
  // The window's size for the next time, in pixels at 100 % scaling, and every setting: before the game starts
  // and when the launcher closes.
  auto save = [&] {
    SDL_WindowFlags flags = SDL_GetWindowFlags(window);
    launcher::Options& o = app.settings().o;
    o.ui_maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    if (!(flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN))) {
      int w = 0;
      int h = 0;
      SDL_GetWindowSizeInPixels(window, &w, &h);
      o.ui_size = {int(float(w) / dpi_scale + 0.5f), int(float(h) / dpi_scale + 0.5f)};
    }
    return launcher::SaveOptions(app.settings());
  };
  LogGamepads("at the start");
  GameProcess game;
  uint64_t game_started_at = 0;  // SDL_GetTicks()
  uint64_t returned_at = 0;      // SDL_GetTicks() when the launcher came back from the game
  bool log_first_input = false;  // the first input after the game, and whether the window had the focus
  bool logged_unprimed = false;  // the controller still waits for its buttons to be up
  std::unordered_map<uint64_t, bool> axes_outside;  // ChangesSomething's sticks and triggers outside the dead zone
  uint64_t last_drawn = 0;                           // SDL_GetTicks() of the last frame drawn
  // Drawing all the time for 10 s (the log says which events kept it so).
  uint64_t window_start = 0;
  uint32_t window_frames = 0;
  std::unordered_map<uint32_t, uint32_t> window_events;
  launcher::Page logged_page = launcher::Page::kCount;
  bool hidden = false;  // the game's window is on screen
  bool shown = false;
  bool primed = false;
  int idle_frames = 0;
  float scale = 0.0f;
  for (;;) {
    // The watchdog's heartbeat (launcher_log.h); while the game runs the launcher waits for it on purpose.
    launcher::Heartbeat();
    launcher::SetWaiting(app.GameRunning());
    // Play: the game starts, and the launcher stays.
    if (app.result() == App::Result::kPlay) {
      if (!save()) {
        ShowError(Formatted(app.texts().save_failed, launcher::game::kOptionsFile).c_str());
      }
      // The controllers are the game's while it runs (both reading a DualSense would also set its lights twice).
      ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, nullptr, 0);
      SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
      std::filesystem::path exe = exe_dir / launcher::game::kRecompExecutable;
      bool found = std::filesystem::exists(exe);
      if (!found) {
        ShowError(Formatted(app.texts().game_missing, launcher::game::kRecompExecutable, launcher::game::kLauncherName)
                      .c_str());
      }
      // The launcher's own options are not the game's.
      std::wstring game_arguments = ArgumentsWithout({L"--launcher_"});
      launcher::Log("Play: starting %s%s", launcher::LogText(exe.wstring()).c_str(),
                    launcher::LogText(game_arguments).c_str());
      if (found && game.Start(exe, game_arguments)) {
        launcher::Log("the game runs (process %u)", game.id());
        game_started_at = SDL_GetTicks();
        app.GameStarted();
      } else {
        launcher::Log("the game did not start");
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoFirst, nullptr, 0);
        app.GameExited(0);
      }
    }
    if (app.GameRunning()) {
      app.GameTick();
      if (game.Exited()) {
        // Back: the launcher on the Play page, in front, with the controllers.
        uint32_t exit_code = game.ExitCode();
        launcher::Log("the game exited (code 0x%08X) after %.1f s", exit_code,
                      double(SDL_GetTicks() - game_started_at) / 1000.0);
        game.Close();
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoFirst, nullptr, 0);
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        // The releases of the key or click that started the game went unseen (the launcher took no input since).
        io.ClearInputKeys();
        io.ClearInputMouse();
        app.GameExited(exit_code);
        if (hidden) {
          // Shown without activating it: activated while another window still had the foreground, SDL would not
          // see the activation as its focus (its keyboard events then name no window, and ImGui drops them).
          // BringToFront activates it with the foreground.
          SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
          SDL_ShowWindow(window);
          SDL_ResetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN);
          hidden = false;
        }
        BringToFront(hwnd);
        // The screen as the game left it: a new swap chain (its flips stuck for minutes after the game's exclusive
        // fullscreen, every Present waiting 3 s; launcher 3.11.4).
        ui::gfx::RecreateSwapChain("back from the game");
        launcher::Log("back: %s, %s", GetForegroundWindow() == hwnd ? "in front" : "NOT in front (another window has it)",
                      FocusText(window));
        LogGamepads("after the game");
        returned_at = SDL_GetTicks();
        log_first_input = true;
        logged_unprimed = false;
        // The button that quit the game may still be held: nothing is pressed until all are up.
        primed = false;
        idle_frames = 0;
      } else if (!hidden && game.WindowShown()) {
        launcher::Log("the game's window is on screen: the launcher hides");
        SDL_HideWindow(window);
        hidden = true;
      }
      if (hidden) {
        // Nothing to draw: sleep until the game exits, answering Windows' messages meanwhile, and wake each minute
        // to write the playtime so far (App::GameTick).
        HANDLE process = game.handle();
        MsgWaitForMultipleObjects(1, &process, FALSE, 60'000, QS_ALLINPUT);
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        continue;
      }
    }

    // Nothing changed for a while: wait for input instead of drawing the same frame.
    const bool idle = idle_frames > 30 && !app.Busy() && !GamepadHeld();
    if (idle) {
      SDL_WaitEventTimeout(nullptr, 250);
    }
    bool input = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ++window_events[event.type];
      input = ChangesSomething(event, axes_outside) || input;
      if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        launcher::Log("focus lost: the foreground is %s", ForegroundText().c_str());
      } else if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
        launcher::Log("focus back");
      } else if (event.type == SDL_EVENT_WINDOW_OCCLUDED || event.type == SDL_EVENT_WINDOW_MINIMIZED ||
                 event.type == SDL_EVENT_WINDOW_RESTORED) {
        launcher::Log("window %s", event.type == SDL_EVENT_WINDOW_OCCLUDED   ? "occluded"
                                   : event.type == SDL_EVENT_WINDOW_MINIMIZED ? "minimized"
                                                                              : "restored");
      }
      if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        const char* name = SDL_GetGamepadNameForID(event.gdevice.which);
        launcher::Log("controller %s: %s", event.type == SDL_EVENT_GAMEPAD_ADDED ? "connected" : "disconnected",
                      name ? name : "?");
      }
      if (log_first_input && !app.GameRunning()) {
        if (const char* kind = InputKind(event.type)) {
          log_first_input = false;
          launcher::Log("first input after the game, %.1f s after it: %s, the window %s",
                        double(SDL_GetTicks() - returned_at) / 1000.0, kind, FocusText(window));
        }
      }
      if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        ui::gfx::Resize(event.window.data1, event.window.data2);
      }
      // While the game starts the launcher takes no input, and cannot be closed (the game would close with it).
      if (app.GameRunning()) {
        continue;
      }
      if (event.type == SDL_EVENT_QUIT ||
          (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))) {
        launcher::Log("closing: %s", event.type == SDL_EVENT_QUIT ? "Windows asked" : "the window was closed");
        app.Quit();
        break;
      }
      if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && !app.Listening() &&
          (event.key.key == SDLK_F11 || (event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT)))) {
        fullscreen = !fullscreen;
        launcher::Log("fullscreen %s", fullscreen ? "on" : "off");
        SDL_SetWindowFullscreen(window, fullscreen);
        if (!SteamWantsFullscreen()) {
          app.settings().o.ui_fullscreen = fullscreen;
          launcher::SaveOptions(app.settings());
        }
        continue;
      }
      if (app.OnEvent(event)) {
        continue;
      }
      ImGui_ImplSDL3_ProcessEvent(&event);
    }
    idle_frames = input ? 0 : idle_frames + 1;
    if (app.result() == App::Result::kQuit || app.result() == App::Result::kRestart) {
      break;
    }
    if (app.result() == App::Result::kPlay) {
      continue;
    }
    // Idle, and what came changes nothing on screen (a controller's noise): no frame, unless a quarter of a second passed
    // since the last one.
    if (idle && !input && SDL_GetTicks() - last_drawn < 250) {
      continue;
    }
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
    // The controller navigates once the buttons held at the start are up, and not while a key is being
    // assigned (B cancels that, App::OnKeyCapture).
    // A controller button held when the window opened (the A that started the launcher from Big Picture) must
    // not press anything; neither must the one just assigned on the Gamepad page (App::GamepadLocked), nor the
    // one that quit the game.
    if (!primed && !ui::AnyGamepadButtonDown()) {
      primed = true;
    }
    if (!primed && !logged_unprimed && returned_at && SDL_GetTicks() - returned_at > 3000) {
      // Still a button or a trigger read as held, 3 s after the game: a controller resting pressed would keep the
      // controller from navigating.
      logged_unprimed = true;
      std::string held;
      for (int key = ImGuiKey_GamepadStart; key <= ImGuiKey_GamepadR3; ++key) {
        if (ImGui::IsKeyDown(ImGuiKey(key))) {
          held += std::string(held.empty() ? "" : ", ") + ImGui::GetKeyName(ImGuiKey(key));
        }
      }
      launcher::Log("the controller does not navigate yet: %s held", held.c_str());
    }
    if (app.page() != logged_page) {
      logged_page = app.page();
      launcher::Log("page: %s", launcher::PageName(logged_page));
    }
    if (primed && !app.Listening() && !app.GamepadLocked() && !app.GameRunning()) {
      io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    } else {
      io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    app.Frame(ImVec2(float(pixel_width), float(pixel_height)));
    ImGui::Render();
    ui::gfx::BeginFrame(ui::col::kBackground);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    ui::gfx::Present();
    last_drawn = SDL_GetTicks();
    // Drawing all the time for 10 s (more than 30 frames a second): which events kept it so.
    ++window_frames;
    if (last_drawn - window_start >= 10'000) {
      if (window_frames > 300) {
        std::string events;
        for (const auto& [type, count] : window_events) {
          char item[48];
          std::snprintf(item, sizeof(item), "%s0x%X x%u", events.empty() ? "" : ", ", type, count);
          events += item;
        }
        std::string busy = app.BusyReasons();
        launcher::Log("drawing all the time: %u frames in %.1f s; busy: %s; a controller held: %s; events (SDL type x "
                      "count): %s",
                      window_frames, double(last_drawn - window_start) / 1000.0, busy.empty() ? "no" : busy.c_str(),
                      GamepadHeld() ? "yes" : "no", events.empty() ? "none" : events.c_str());
      }
      window_start = last_drawn;
      window_frames = 0;
      window_events.clear();
    }
    if (!shown) {
      SDL_ShowWindow(window);
      SDL_RaiseWindow(window);
      shown = true;
    }
  }

  launcher::Log(app.result() == App::Result::kRestart ? "closing for the new version" : "closing");
  watchdog.Closing();
  launcher::Log(save() ? "options saved" : "the options could not be saved");
  // After an update: the new launcher, in place of this one. This process waits for it (and is gone only then), so
  // that Steam still sees the program it started running.
  GameProcess next;
  bool restarted = false;
  if (app.result() == App::Result::kRestart) {
    std::wstring arguments = ArgumentsWithout({L"--launcher_updated_from", L"--launcher_first_run"});
    std::string from = app.UpdatedFromArgument();
    arguments += L" " + std::wstring(from.begin(), from.end());
    restarted = next.Start(exe_dir / launcher::game::kLauncherExecutable, arguments);
  }
  app.WaitForTasks();
  app.ReleaseTextures();
  ui::ReleasePictures();
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  ui::gfx::Shutdown();
  launcher::Log("closing: ImGui and Direct3D done");
  SDL_DestroyWindow(window);
  SDL_Quit();
  launcher::Log("closed");
  if (restarted) {
    WaitForSingleObject(next.handle(), INFINITE);
    return int(next.ExitCode());
  }
  return 0;
}
