// The launcher's log and watchdog (launcher_log.h).

#include "launcher_log.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#include <share.h>
#include <shlobj.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdarg>
#include <cstdio>
#include <cwctype>
#include <iterator>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "game.h"

namespace launcher {
namespace {

// The runs whose log and dumps are kept.
constexpr int kKeptRuns = 20;
// How long the window may not answer, or the closing take, before a dump is written. Windows itself says "Not
// responding" after 5 seconds.
constexpr auto kDumpAfter = std::chrono::seconds(10);

std::mutex g_mutex;  // the file's lines
FILE* g_file = nullptr;
std::filesystem::path g_folder;
int g_run = 0;

using MiniDumpWriteDumpFunction = BOOL(WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION,
                                                PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);
MiniDumpWriteDumpFunction g_write_dump = nullptr;  // loaded with the log: not in a crashed process
LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter = nullptr;

// "launcher_NNN.log" and "launcher_NNN_<kind>.dmp": NNN, else -1.
int RunOf(const std::wstring& name) {
  const std::wstring prefix = L"launcher_";
  if (name.rfind(prefix, 0) != 0) {
    return -1;
  }
  size_t i = prefix.size();
  int run = 0;
  size_t digits = 0;
  while (i < name.size() && std::iswdigit(name[i]) && digits < 9) {
    run = run * 10 + int(name[i] - L'0');
    ++i;
    ++digits;
  }
  if (!digits || i >= name.size() || (name[i] != L'.' && name[i] != L'_')) {
    return -1;
  }
  return run;
}

std::filesystem::path DumpPath(const char* kind) {
  wchar_t name[64];
  std::swprintf(name, std::size(name), L"launcher_%03d_%hs.dmp", g_run, kind);
  return g_folder / name;
}

// A minidump of the launcher: every thread's stack and the memory they point at, enough to see where each one waits.
// `exception` from a crash, else null.
bool WriteDump(const char* kind, EXCEPTION_POINTERS* exception) {
  if (!g_write_dump || g_folder.empty()) {
    return false;
  }
  HANDLE file = CreateFileW(DumpPath(kind).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                            nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return false;
  }
  MINIDUMP_EXCEPTION_INFORMATION information = {GetCurrentThreadId(), exception, FALSE};
  auto type = MINIDUMP_TYPE(MiniDumpWithThreadInfo | MiniDumpWithIndirectlyReferencedMemory |
                            MiniDumpWithUnloadedModules | MiniDumpWithHandleData | MiniDumpWithProcessThreadData);
  BOOL written = g_write_dump(GetCurrentProcess(), GetCurrentProcessId(), file, type,
                              exception ? &information : nullptr, nullptr, nullptr);
  CloseHandle(file);
  return written != FALSE;
}

LONG WINAPI OnCrash(EXCEPTION_POINTERS* exception) {
  static std::atomic<bool> handled{false};
  if (!handled.exchange(true)) {
    // Not through Log: the crash may be in a thread holding the log's lock.
    if (g_mutex.try_lock()) {
      if (g_file) {
        SYSTEMTIME time;
        GetLocalTime(&time);
        std::fprintf(g_file,
                     "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [t%lu] crashed: exception 0x%08lX at %p; writing "
                     "launcher_%03d_crash.dmp\n",
                     time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, time.wMilliseconds,
                     GetCurrentThreadId(), exception->ExceptionRecord->ExceptionCode,
                     exception->ExceptionRecord->ExceptionAddress, g_run);
        std::fflush(g_file);
      }
      g_mutex.unlock();
    }
    WriteDump("crash", exception);
  }
  return g_previous_filter ? g_previous_filter(exception) : EXCEPTION_CONTINUE_SEARCH;
}

std::atomic<int64_t> g_beat{0};  // steady_clock nanoseconds of the main loop's last pass
std::atomic<bool> g_waiting{false};
constexpr auto kStallAfter = std::chrono::seconds(3);

int64_t NowNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// The watchdog's thread (one at a time).
struct WatchdogState {
  std::mutex mutex;
  std::condition_variable wake;
  std::thread thread;
  HWND hwnd = nullptr;
  bool stop = false;
  bool closing = false;
  std::chrono::steady_clock::time_point closing_since;
};
WatchdogState* g_watchdog = nullptr;

void WatchdogRun(WatchdogState* state) {
  using Clock = std::chrono::steady_clock;
  bool hung = false;
  bool hang_dumped = false;
  bool closing_dumped = false;
  bool stalled = false;
  bool stall_dumped = false;
  int64_t stall_beat = 0;
  Clock::time_point hung_since;
  std::unique_lock lock(state->mutex);
  while (!state->wake.wait_for(lock, std::chrono::milliseconds(500), [state] { return state->stop; })) {
    Clock::time_point now = Clock::now();
    if (state->closing) {
      if (!closing_dumped && now - state->closing_since >= kDumpAfter) {
        closing_dumped = true;
        lock.unlock();
        Log("watchdog: still closing after %lld s; writing launcher_%03d_closing.dmp",
            static_cast<long long>(kDumpAfter.count()), g_run);
        Log(WriteDump("closing", nullptr) ? "watchdog: dump written" : "watchdog: the dump could not be written");
        lock.lock();
      }
      continue;
    }
    HWND hwnd = state->hwnd;
    lock.unlock();
    // The main loop's heartbeat (Heartbeat): a pass late by kStallAfter, outside a wait on purpose.
    const int64_t beat = g_beat.load(std::memory_order_relaxed);
    const bool late = beat && !g_waiting.load(std::memory_order_relaxed) &&
                      NowNs() - beat > std::chrono::duration_cast<std::chrono::nanoseconds>(kStallAfter).count();
    if (late && !stalled) {
      stalled = true;
      stall_beat = beat;
      Log("watchdog: the main loop has not come round for %lld s%s", static_cast<long long>(kStallAfter.count()),
          stall_dumped ? "" : "; writing a dump");
      if (!stall_dumped) {
        stall_dumped = true;  // the first stall of the run: the others are only logged
        Log(WriteDump("stall", nullptr) ? "watchdog: launcher_%03d_stall.dmp written"
                                        : "watchdog: the dump (launcher_%03d_stall.dmp) could not be written",
            g_run);
      }
    } else if (stalled && beat != stall_beat) {
      stalled = false;
      Log("watchdog: the main loop came round again, after %.1f s", double(beat - stall_beat) / 1e9);
    }
    // Windows' own test: the window's thread has not taken its messages for 5 seconds (and is not waiting for
    // them). A message box, the window being moved or the wait while the game runs take them.
    bool now_hung = hwnd && IsHungAppWindow(hwnd);
    if (now_hung && !hung) {
      hung = true;
      hung_since = now;
      Log("watchdog: the window stopped answering Windows (5 s without taking its messages)");
    } else if (now_hung && !hang_dumped && now - hung_since >= kDumpAfter - std::chrono::seconds(5)) {
      hang_dumped = true;  // the first hang of the run: the others are only logged
      Log("watchdog: not answering for %lld s; writing launcher_%03d_hang.dmp",
          static_cast<long long>(kDumpAfter.count()), g_run);
      Log(WriteDump("hang", nullptr) ? "watchdog: dump written" : "watchdog: the dump could not be written");
    } else if (!now_hung && hung) {
      hung = false;
      Log("watchdog: the window answers again, after about %.1f s",
          5.0 + std::chrono::duration<double>(now - hung_since).count());
    }
    lock.lock();
  }
}

FILE* OpenIn(const std::filesystem::path& folder, int& run_out) {
  std::error_code error;
  std::filesystem::create_directories(folder, error);
  // The next run's number; the runs older than the kept ones go, logs and dumps.
  std::vector<std::pair<int, std::filesystem::path>> files;
  int last = 0;
  for (std::filesystem::directory_iterator it(folder, error), end; !error && it != end; it.increment(error)) {
    int run = RunOf(it->path().filename().wstring());
    if (run >= 0) {
      last = std::max(last, run);
      files.emplace_back(run, it->path());
    }
  }
  int run = last + 1;
  for (const auto& [old_run, path] : files) {
    if (old_run <= run - kKeptRuns) {
      std::filesystem::remove(path, error);
    }
  }
  wchar_t name[32];
  std::swprintf(name, std::size(name), L"launcher_%03d.log", run);
  run_out = run;
  // Others may read it while the launcher runs.
  return _wfsopen((folder / name).c_str(), L"wb", _SH_DENYWR);
}

}  // namespace

void OpenLog(const std::filesystem::path& exe_dir) {
  std::filesystem::path folder = exe_dir / "logs";
  int run = 0;
  FILE* file = OpenIn(folder, run);
  if (!file) {
    // A folder that cannot be written (Program Files): where the game keeps its user data then.
    PWSTR documents = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents))) {
      folder = std::filesystem::path(documents) / game::kDocumentsFolder / L"logs";
      file = OpenIn(folder, run);
    }
    CoTaskMemFree(documents);
  }
  if (!file) {
    return;
  }
  {
    std::lock_guard lock(g_mutex);
    g_file = file;
    g_folder = folder;
    g_run = run;
  }
  if (HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll")) {
    g_write_dump = reinterpret_cast<MiniDumpWriteDumpFunction>(GetProcAddress(dbghelp, "MiniDumpWriteDump"));
  }
  g_previous_filter = SetUnhandledExceptionFilter(OnCrash);
}

void Log(const char* format, ...) {
  std::lock_guard lock(g_mutex);
  if (!g_file) {
    return;
  }
  SYSTEMTIME time;
  GetLocalTime(&time);
  std::fprintf(g_file, "[%04u-%02u-%02u %02u:%02u:%02u.%03u] [t%lu] ", time.wYear, time.wMonth, time.wDay, time.wHour,
               time.wMinute, time.wSecond, time.wMilliseconds, GetCurrentThreadId());
  va_list arguments;
  va_start(arguments, format);
  std::vfprintf(g_file, format, arguments);
  va_end(arguments);
  std::fputc('\n', g_file);
  std::fflush(g_file);
}

std::string LogText(const std::wstring& text) {
  if (text.empty()) {
    return {};
  }
  int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
  std::string utf8(size_t(std::max(size, 0)), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), utf8.data(), size, nullptr, nullptr);
  return utf8;
}

void Heartbeat() { g_beat.store(NowNs(), std::memory_order_relaxed); }

void SetWaiting(bool waiting) { g_waiting.store(waiting, std::memory_order_relaxed); }

Watchdog::Watchdog(void* hwnd) {
  if (g_watchdog) {
    return;
  }
  g_watchdog = new WatchdogState;
  g_watchdog->hwnd = HWND(hwnd);
  g_watchdog->thread = std::thread(WatchdogRun, g_watchdog);
}

Watchdog::~Watchdog() {
  WatchdogState* state = g_watchdog;
  if (!state) {
    return;
  }
  {
    std::lock_guard lock(state->mutex);
    state->stop = true;
  }
  state->wake.notify_all();
  state->thread.join();
  g_watchdog = nullptr;
  delete state;
}

void Watchdog::Closing() {
  if (WatchdogState* state = g_watchdog) {
    std::lock_guard lock(state->mutex);
    state->closing = true;
    state->closing_since = std::chrono::steady_clock::now();
  }
}

}  // namespace launcher
