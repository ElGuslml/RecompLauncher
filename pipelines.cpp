// RecompLauncher - see pipelines.h.

#include "pipelines.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#include "dlc.h"
#include "game.h"
#include "launcher_log.h"

namespace launcher {

namespace {

std::wstring TitleName() {
  wchar_t title[16];
  std::swprintf(title, std::size(title), L"%08X", game::kTitleId);
  return title;
}

std::filesystem::path PreparationFolder(const std::filesystem::path& exe_dir) {
  return GameUserDataFolder(exe_dir) / L"cache" / L"native_x_renderer";
}

std::filesystem::path ListPath(const std::filesystem::path& exe_dir) {
  return exe_dir / (TitleName() + L".pipelines.list");
}

// "<size> <last write time>", as the game's tool writes it (NativeXRenderer's tools.cpp, FileIdentity).
std::string FileIdentity(const std::filesystem::path& path) {
  std::error_code error;
  uint64_t size = std::filesystem::file_size(path, error);
  if (error) return {};
  auto time = std::filesystem::last_write_time(path, error);
  if (error) return {};
  return std::to_string(size) + " " + std::to_string(int64_t(time.time_since_epoch().count()));
}

// The Vulkan devices of this machine as the game's tool names them ("<vendor> <device> <driver version> <pipeline
// cache UUID>", NativeXRenderer's Device::DriverIdentity): what a Vulkan pipeline cache is valid for. Through the
// Vulkan loader, with the few types used declared as the Vulkan ABI has them, and the implicit layers (overlays) off.
std::vector<std::string> DriverIdentities() {
  static std::vector<std::string> identities;
  static bool read = false;
  if (read) return identities;
  read = true;
  HMODULE loader = LoadLibraryW(L"vulkan-1.dll");
  if (!loader) return identities;
  struct ApplicationInfo {
    uint32_t type;  // VK_STRUCTURE_TYPE_APPLICATION_INFO = 0
    const void* next;
    const char* application_name;
    uint32_t application_version;
    const char* engine_name;
    uint32_t engine_version;
    uint32_t api_version;
  };
  struct InstanceCreateInfo {
    uint32_t type;  // VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1
    const void* next;
    uint32_t flags;
    const ApplicationInfo* application;
    uint32_t layer_count;
    const char* const* layers;
    uint32_t extension_count;
    const char* const* extensions;
  };
  using Instance = void*;
  using PhysicalDevice = void*;
  using CreateInstance = int32_t(__stdcall*)(const InstanceCreateInfo*, const void*, Instance*);
  using DestroyInstance = void(__stdcall*)(Instance, const void*);
  using EnumeratePhysicalDevices = int32_t(__stdcall*)(Instance, uint32_t*, PhysicalDevice*);
  using GetPhysicalDeviceProperties = void(__stdcall*)(PhysicalDevice, void*);
  auto create = reinterpret_cast<CreateInstance>(GetProcAddress(loader, "vkCreateInstance"));
  auto destroy = reinterpret_cast<DestroyInstance>(GetProcAddress(loader, "vkDestroyInstance"));
  auto enumerate = reinterpret_cast<EnumeratePhysicalDevices>(GetProcAddress(loader, "vkEnumeratePhysicalDevices"));
  auto properties = reinterpret_cast<GetPhysicalDeviceProperties>(GetProcAddress(loader, "vkGetPhysicalDeviceProperties"));
  if (!create || !destroy || !enumerate || !properties) return identities;
  // No overlay in the launcher: the loader leaves the implicit layers out (Vulkan loader 1.3.234 and later). Put back
  // as it was before the game starts, which inherits the environment.
  wchar_t previous[256];
  DWORD had = GetEnvironmentVariableW(L"VK_LOADER_LAYERS_DISABLE", previous, DWORD(std::size(previous)));
  SetEnvironmentVariableW(L"VK_LOADER_LAYERS_DISABLE", L"~implicit~");
  ApplicationInfo application{0, nullptr, "launcher", 1, "launcher", 1, (1u << 22)};  // Vulkan 1.0
  InstanceCreateInfo info{1, nullptr, 0, &application, 0, nullptr, 0, nullptr};
  Instance instance = nullptr;
  if (create(&info, nullptr, &instance) == 0 && instance) {
    uint32_t count = 0;
    enumerate(instance, &count, nullptr);
    std::vector<PhysicalDevice> devices(count);
    if (count && enumerate(instance, &count, devices.data()) >= 0) {
      for (PhysicalDevice device : devices) {
        // VkPhysicalDeviceProperties: apiVersion, driverVersion, vendorID, deviceID, deviceType, deviceName[256],
        // pipelineCacheUUID[16] at 276, then the limits (824 bytes in all); a larger buffer.
        alignas(8) uint8_t p[2048] = {};
        properties(device, p);
        uint32_t words[4];
        std::memcpy(words, p, sizeof(words));
        char text[96];
        std::snprintf(text, sizeof(text), "%04x %04x %08x ", words[2], words[3], words[1]);
        std::string identity = text;
        for (int i = 0; i < 16; ++i) {
          std::snprintf(text, sizeof(text), "%02x", p[276 + i]);
          identity += text;
        }
        identities.push_back(identity);
      }
    }
    destroy(instance, nullptr);
  }
  SetEnvironmentVariableW(L"VK_LOADER_LAYERS_DISABLE", had && had < std::size(previous) ? previous : nullptr);
  return identities;
}

}  // namespace

bool PipelinesNeedPreparing(const std::filesystem::path& exe_dir) {
  if constexpr (!game::kHasPipelineList) {
    return false;
  }
  std::filesystem::path folder = PreparationFolder(exe_dir);
  std::error_code error;
  const bool has_list = std::filesystem::exists(ListPath(exe_dir), error);
  const bool has_storage = std::filesystem::exists(folder / (TitleName() + L".pipelines"), error);
  if (!has_list && !has_storage) {
    return false;  // nothing to compile: the game learns its pipelines as it draws
  }
  std::ifstream stamp(folder / (TitleName() + L".prepared"));
  if (!stamp) {
    Log("pipelines: no preparation stamp yet");
    return true;
  }
  std::string line, header;
  std::getline(stamp, header);
  if (header != "native_x_renderer prepared 1") {
    Log("pipelines: a stamp of another version");
    return true;
  }
  bool list_named = false, driver_ok = false;
  const std::string list_name = ListPath(exe_dir).filename().string();
  while (std::getline(stamp, line)) {
    std::istringstream fields(line);
    std::string kind;
    fields >> kind;
    if (kind == "file") {
      std::string size, time, name;
      fields >> size >> time;
      std::getline(fields >> std::ws, name);
      if (FileIdentity(exe_dir / std::filesystem::u8path(name)) != size + " " + time) {
        Log("pipelines: %s changed since the last preparation", name.c_str());
        return true;
      }
      list_named |= name == list_name;
    } else if (kind == "driver") {
      std::string identity;
      std::getline(fields >> std::ws, identity);
      for (const std::string& current : DriverIdentities()) {
        driver_ok |= current == identity;
      }
      if (!driver_ok) {
        Log("pipelines: the graphics driver changed since the last preparation (%s)", identity.c_str());
        return true;
      }
    }
  }
  if (has_list && !list_named) {
    Log("pipelines: a pipeline list the last preparation did not have");
    return true;
  }
  return !driver_ok;
}

PipelinePreparation::~PipelinePreparation() {
  Skip();
  if (worker_.joinable()) {
    worker_.join();
  }
}

void PipelinePreparation::Start(const std::filesystem::path& exe_dir, std::wstring arguments) {
  if (worker_.joinable()) {
    worker_.join();
  }
  {
    std::lock_guard lock(mutex_);
    progress_ = {};
    progress_.state = State::kRunning;
    skip_ = false;
  }
  worker_ = std::thread(&PipelinePreparation::Run, this, exe_dir, std::move(arguments));
}

PipelinePreparation::Progress PipelinePreparation::Get() const {
  std::lock_guard lock(mutex_);
  return progress_;
}

void PipelinePreparation::Skip() {
  std::lock_guard lock(mutex_);
  skip_ = true;
}

void PipelinePreparation::Reset() {
  std::lock_guard lock(mutex_);
  if (progress_.state != State::kRunning) {
    progress_ = {};
  }
}

void PipelinePreparation::Run(std::filesystem::path exe_dir, std::wstring arguments) {
  std::filesystem::path game = exe_dir / game::kRecompExecutable;
  std::filesystem::path progress_file = PreparationFolder(exe_dir) / (TitleName() + L".prepare_progress");
  std::error_code error;
  std::filesystem::create_directories(progress_file.parent_path(), error);
  std::filesystem::remove(progress_file, error);
  std::wstring command_line = L"\"" + game.wstring() + L"\"" + arguments +
                              L" \"--native_x_renderer_prepare_pipelines=" + progress_file.wstring() + L"\"";
  Log("pipelines: compiling them first: %s", LogText(command_line).c_str());
  const auto start = std::chrono::steady_clock::now();
  // Tied to the launcher: closed with it.
  HANDLE job = CreateJobObjectW(nullptr, nullptr);
  if (job) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
  }
  STARTUPINFOW startup = {sizeof(startup)};
  PROCESS_INFORMATION process = {};
  State result = State::kFailed;
  std::string note;
  if (CreateProcessW(game.c_str(), command_line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                     nullptr, exe_dir.c_str(), &startup, &process)) {
    if (job) AssignProcessToJobObject(job, process.hProcess);
    ResumeThread(process.hThread);
    CloseHandle(process.hThread);
    // The game's tool replaces the file whole: "<state> <done> <total> [<note>]". A preparation that has not finished
    // in 15 minutes has failed.
    auto deadline = start + std::chrono::minutes(15);
    bool skipped = false;
    std::string state;
    for (;;) {
      bool exited = WaitForSingleObject(process.hProcess, 100) != WAIT_TIMEOUT;
      std::ifstream file(progress_file);
      uint64_t done = 0, total = 0;
      if (file >> state >> done >> total) {
        std::getline(file >> std::ws, note);
        std::lock_guard lock(mutex_);
        progress_.done = done;
        progress_.total = total;
      }
      if (exited) break;
      bool skip;
      {
        std::lock_guard lock(mutex_);
        skip = skip_;
      }
      if (skip || std::chrono::steady_clock::now() > deadline) {
        TerminateProcess(process.hProcess, 1);
        WaitForSingleObject(process.hProcess, 5000);
        skipped = skip;
        break;
      }
    }
    CloseHandle(process.hProcess);
    result = skipped ? State::kSkipped : state == "done" ? State::kDone : State::kFailed;
  } else {
    note = "CreateProcess " + std::to_string(GetLastError());
  }
  if (job) CloseHandle(job);
  std::filesystem::remove(progress_file, error);
  Log("pipelines: %s after %.1f s%s%s",
      result == State::kDone ? "compiled" : result == State::kSkipped ? "skipped by the player" : "failed",
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count(), note.empty() ? "" : ": ",
      note.c_str());
  std::lock_guard lock(mutex_);
  progress_.state = result;
  progress_.note = note;
}

}  // namespace launcher
