// The launcher outside Windows (platform.h): what POSIX has. The macOS-only parts are platform_mac.mm.

#include "platform.h"

#ifndef _WIN32

#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <thread>

extern char** environ;

namespace launcher::platform {

std::string Utf8(const std::wstring& text) {
  std::string out;
  for (wchar_t wc : text) {
    uint32_t c = uint32_t(wc);
    if (c < 0x80) {
      out += char(c);
    } else if (c < 0x800) {
      out += char(0xC0 | (c >> 6));
      out += char(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
      out += char(0xE0 | (c >> 12));
      out += char(0x80 | ((c >> 6) & 0x3F));
      out += char(0x80 | (c & 0x3F));
    } else {
      out += char(0xF0 | (c >> 18));
      out += char(0x80 | ((c >> 12) & 0x3F));
      out += char(0x80 | ((c >> 6) & 0x3F));
      out += char(0x80 | (c & 0x3F));
    }
  }
  return out;
}

std::wstring Wide(const std::string& text) {
  std::wstring out;
  for (size_t i = 0; i < text.size();) {
    uint8_t b = uint8_t(text[i]);
    uint32_t c = 0;
    size_t n = 0;
    if (b < 0x80) {
      c = b;
      n = 1;
    } else if ((b >> 5) == 0x6) {
      c = b & 0x1F;
      n = 2;
    } else if ((b >> 4) == 0xE) {
      c = b & 0x0F;
      n = 3;
    } else if ((b >> 3) == 0x1E) {
      c = b & 0x07;
      n = 4;
    } else {
      out += L'�';
      ++i;
      continue;
    }
    if (i + n > text.size()) {
      out += L'�';
      break;
    }
    for (size_t k = 1; k < n; ++k) c = (c << 6) | (uint8_t(text[i + k]) & 0x3F);
    out += wchar_t(c);
    i += n;
  }
  return out;
}

std::filesystem::path DataFolder(const char* folder_name) {
  std::filesystem::path exe_dir = ExecutableFolder();
  if (exe_dir.filename() == "MacOS" && exe_dir.parent_path().filename() == "Contents") {
    std::filesystem::path folder = ApplicationSupportFolder() / folder_name;
    std::error_code ec;
    std::filesystem::create_directories(folder, ec);
    if (!ec) return folder;
  }
  return exe_dir;
}

std::filesystem::path GameExecutable(const char* name) { return ExecutableFolder() / name; }

std::filesystem::path HomeFolder() {
  const char* home = std::getenv("HOME");
  return home && *home ? std::filesystem::path(home) : std::filesystem::path("/tmp");
}

std::string ErrorMessage(int error_number) { return std::strerror(error_number); }

std::optional<std::string> Environment(const char* name) {
  const char* value = std::getenv(name);
  if (!value) return std::nullopt;
  return std::string(value);
}

Process::~Process() = default;

bool Process::Start(const std::filesystem::path& exe, const std::vector<std::string>& arguments,
                    const std::filesystem::path& folder, std::string* error) {
  exited_ = false;
  exit_code_ = 0;
  pid_ = 0;
  std::vector<std::string> storage;
  storage.push_back(exe.string());
  storage.insert(storage.end(), arguments.begin(), arguments.end());
  std::vector<char*> argv;
  for (std::string& s : storage) argv.push_back(s.data());
  argv.push_back(nullptr);
  posix_spawn_file_actions_t actions;
  posix_spawn_file_actions_init(&actions);
#ifdef __APPLE__
  if (!folder.empty()) posix_spawn_file_actions_addchdir_np(&actions, folder.c_str());
#else
  (void)folder;
#endif
  pid_t pid = 0;
  int result = posix_spawn(&pid, exe.c_str(), &actions, nullptr, argv.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  if (result != 0) {
    if (error) *error = ErrorMessage(result);
    return false;
  }
  pid_ = pid;
  return true;
}

bool Process::Exited() {
  if (pid_ <= 0) return true;
  if (exited_) return true;
  int status = 0;
  pid_t result = waitpid(pid_, &status, WNOHANG);
  if (result == pid_ || (result < 0 && errno == ECHILD)) {
    exited_ = true;
    exit_code_ = result == pid_ && WIFEXITED(status) ? uint32_t(WEXITSTATUS(status))
                 : result == pid_ && WIFSIGNALED(status) ? uint32_t(128 + WTERMSIG(status))
                                                         : 0;
  }
  return exited_;
}

bool Process::Wait(std::chrono::milliseconds timeout) {
  auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!Exited()) {
    if (timeout.count() >= 0 && std::chrono::steady_clock::now() >= deadline) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return true;
}

void Process::Terminate() {
  if (pid_ > 0 && !exited_) kill(pid_, SIGTERM);
}

namespace {
std::vector<std::string> g_arguments;
}

void SetArguments(int argc, char** argv) {
  g_arguments.clear();
  for (int i = 1; i < argc; ++i) {
    // macOS adds -psn_... when Finder starts an app on old systems.
    if (std::strncmp(argv[i], "-psn_", 5) == 0) continue;
    g_arguments.emplace_back(argv[i]);
  }
}

const std::vector<std::string>& Arguments() { return g_arguments; }

std::string ArgumentValue(const std::string& name) {
  std::string prefix = name;
  if (prefix.empty() || prefix.back() != '=') prefix += '=';
  for (const std::string& argument : g_arguments) {
    if (argument.rfind(prefix, 0) == 0) return argument.substr(prefix.size());
  }
  return {};
}

bool HasArgument(const std::string& prefix) {
  for (const std::string& argument : g_arguments) {
    if (argument.rfind(prefix, 0) == 0) return true;
  }
  return false;
}

}  // namespace launcher::platform

#endif  // !_WIN32
