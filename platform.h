// What the launcher needs from the system outside Windows (macOS; platform_posix.cpp, platform_mac.mm). The Windows
// build keeps its own calls in each file and does not use this.

#pragma once

#ifndef _WIN32

#include <strings.h>

#include <cerrno>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace launcher::platform {

// UTF-8 <-> wide (UTF-32 on macOS) for the code that keeps Windows' wide strings.
std::string Utf8(const std::wstring& text);
std::wstring Wide(const std::string& text);

// The folder of the running executable (inside an app bundle: Contents/MacOS).
std::filesystem::path ExecutableFolder();

// The folder of the launcher's and the game's own files: settings, user data, logs and the game files. The executable's
// folder, as on Windows; inside an app bundle, ~/Library/Application Support/<game folder> (the game's DataFolder,
// src/startup.cpp), since a bundle is signed and may be read-only.
std::filesystem::path DataFolder(const char* folder_name);

// The game's executable, next to the launcher's.
std::filesystem::path GameExecutable(const char* name);

std::filesystem::path HomeFolder();
std::filesystem::path DocumentsFolder();
// Per-user application data (Windows' roaming AppData): ~/Library/Application Support.
std::filesystem::path ApplicationSupportFolder();

// The user's preferred language and region, as a BCP 47 tag ("es-MX", "zh-Hans-CN", "en-US").
std::string PreferredLanguage();
// The user's full name, or the account name, or empty.
std::string UserName();
// Upper case as the system maps it, accented letters included ("é" -> "É").
std::string Upper(const std::string& text);

std::string ErrorMessage(int error_number);
std::optional<std::string> Environment(const char* name);
bool OpenInSystem(const std::string& path_or_url);
// Moves a file or folder to the Trash (Windows' Recycle Bin).
bool MoveToTrash(const std::filesystem::path& path);

// SHA-256 of a file, lowercase hex, or empty if it cannot be read.
std::string Sha256File(const std::filesystem::path& path);

// A program the launcher starts and waits for.
class Process {
 public:
  Process() = default;
  ~Process();
  Process(const Process&) = delete;
  Process& operator=(const Process&) = delete;

  // `arguments` without the program itself. False (and `error` set) if it could not start.
  bool Start(const std::filesystem::path& exe, const std::vector<std::string>& arguments,
             const std::filesystem::path& folder, std::string* error = nullptr);
  bool running() const { return pid_ > 0 && !exited_; }
  // Polls; true once it has exited.
  bool Exited();
  // Waits up to `timeout` (forever when negative); true once it has exited.
  bool Wait(std::chrono::milliseconds timeout);
  uint32_t ExitCode() const { return exit_code_; }
  void Terminate();
  // A window of the process is on screen.
  bool WindowShown() const;
  int pid() const { return pid_; }

 private:
  int pid_ = 0;
  bool exited_ = false;
  uint32_t exit_code_ = 0;
};

// The launcher's own arguments (argv without the program), as main received them.
void SetArguments(int argc, char** argv);
const std::vector<std::string>& Arguments();
// The value of --name=value, or empty.
std::string ArgumentValue(const std::string& name);
bool HasArgument(const std::string& prefix);

// Brings this process's windows to the front.
void Activate();

}  // namespace launcher::platform

// The Microsoft C runtime's names the shared code uses.
inline int _stricmp(const char* a, const char* b) { return strcasecmp(a, b); }
inline int localtime_s(std::tm* out, const std::time_t* time) { return localtime_r(time, out) ? 0 : 1; }
inline std::time_t _mkgmtime(std::tm* time) { return timegm(time); }
#define sscanf_s sscanf

// Windows' atomic file replacement, which the shared code uses to write files whole: rename(), or a copy across
// volumes when MOVEFILE_COPY_ALLOWED says so. GetLastError() is errno.
constexpr unsigned MOVEFILE_REPLACE_EXISTING = 0x1;
constexpr unsigned MOVEFILE_COPY_ALLOWED = 0x2;
constexpr unsigned MOVEFILE_WRITE_THROUGH = 0x8;
inline int MoveFileExW(const char* from, const char* to, unsigned flags) {
  std::error_code ec;
  if (!(flags & MOVEFILE_REPLACE_EXISTING) && std::filesystem::exists(to, ec)) {
    errno = EEXIST;
    return 0;
  }
  if (std::rename(from, to) == 0) return 1;
  if (errno == EXDEV && (flags & MOVEFILE_COPY_ALLOWED)) {
    std::filesystem::copy_file(from, to, std::filesystem::copy_options::overwrite_existing, ec);
    if (!ec) {
      std::filesystem::remove(from, ec);
      return 1;
    }
    errno = ec.value();
  }
  return 0;
}
inline int MulDiv(int number, int numerator, int denominator) {
  if (denominator == 0) return -1;
  long long product = (long long)number * numerator;
  return int((product + (product >= 0 ? denominator / 2 : -denominator / 2)) / denominator);
}
// Time without the computer's sleep, in 100 ns units (macOS: the uptime clock, which stops during sleep).
using ULONGLONG = unsigned long long;
inline int QueryUnbiasedInterruptTime(ULONGLONG* time) {
  *time = ULONGLONG(clock_gettime_nsec_np(CLOCK_UPTIME_RAW) / 100);
  return 1;
}
inline unsigned long GetLastError() { return static_cast<unsigned long>(errno); }

#endif  // !_WIN32
