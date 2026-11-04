// The launcher's own log and its watchdog.
//
// The log: `logs/launcher_NNN.log` next to the executable (the game's logs are in the same folder; in
// Documents\<game::kDocumentsFolder>\logs where that folder cannot be written), one per run, numbered after the last
// one; the newest 20 are kept, with their dumps. It says what the launcher did and when: its
// start, each run of the game (started, its window shown, exited, the launcher back in front), the pages opened, the
// first input after the game, Direct3D's errors, the errors shown, why it closed and each step of closing.
//
// The watchdog: a thread that asks Windows every half second whether the window is answering (IsHungAppWindow, what
// makes Windows say "Not responding"); when it has not for 10 seconds, it writes a minidump of the launcher
// (`launcher_NNN_hang.dmp`, every thread's stack) and logs it, and logs when the window answers again. While the
// launcher closes it gives the closing 10 seconds, then writes `launcher_NNN_closing.dmp`. A crash writes
// `launcher_NNN_crash.dmp`. Gustavo, 2026-11-04: the launcher stopped responding after he closed the game, and nothing
// said where.

#pragma once

#include <filesystem>
#include <string>

namespace launcher {

// Opens this run's log in `exe_dir`/logs (made if missing) and installs the crash dump writer. Without a log (a folder
// that cannot be written), Log does nothing.
void OpenLog(const std::filesystem::path& exe_dir);
// A line in the log, printf's format, with the time and the thread. Any thread.
#if defined(__clang__) || defined(__GNUC__)
__attribute__((format(printf, 1, 2)))
#endif
void Log(const char* format, ...);
// A path or other wide text for the log, in UTF-8.
std::string LogText(const std::wstring& text);

// The main loop's heartbeat (main.cpp, each pass): a pass that does not come within 3 s while the loop is not waiting on
// purpose is logged, and the first one of a run dumped (`launcher_NNN_stall.dmp`). Windows calls a window not
// responding only after 5 s without its messages, and a thread blocked a few seconds at a time (in Present, say) never
// gets there.
void Heartbeat();
// The main loop waits on purpose (the game running, a message box): no stall meanwhile.
void SetWaiting(bool waiting);

// The watchdog (above). One at a time; stops when destroyed, after the launcher has closed.
class Watchdog {
 public:
  explicit Watchdog(void* hwnd);
  ~Watchdog();
  Watchdog(const Watchdog&) = delete;
  Watchdog& operator=(const Watchdog&) = delete;
  // The launcher is closing: the window may go now; a closing that takes more than 10 seconds is dumped.
  void Closing();
};

}  // namespace launcher
