// RecompLauncher - NativeXRenderer's pipeline preparation, "Compiling shaders" (its D129).
//
// A game with a pipeline list (game::kHasPipelineList) has <TITLEID>.pipelines.list beside its executable: the
// pipelines its developers' play made, named by hash. Before Play, when the game draws with NativeXRenderer, the
// launcher runs the game's executable with --native_x_renderer_prepare_pipelines: it rebuilds the list's shaders from the
// player's copy of the game, compiles them and every pipeline stored before for this GPU and driver, and leaves a stamp
// (<user data>/cache/native_x_renderer/<TITLEID>.prepared) naming the executable, the SDK's libraries, the list and the
// driver it compiled for. The launcher runs it again when one of those changed: after an update or a driver update.

#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace launcher {

// Whether Play compiles the pipelines first: the stamp is missing, or does not name this executable, its libraries,
// the list and a driver of this machine's GPUs. The caller checks that the game draws with NativeXRenderer.
bool PipelinesNeedPreparing(const std::filesystem::path& exe_dir);

class PipelinePreparation {
 public:
  enum class State { kIdle, kRunning, kDone, kFailed, kSkipped };
  struct Progress {
    State state = State::kIdle;
    uint64_t done = 0, total = 0;  // translations and pipelines, as the game counts them
    std::string note;              // the game's reason when it failed
  };

  ~PipelinePreparation();
  // Starts the game's executable in its tool mode, with `arguments` (the game's, each with its leading space).
  void Start(const std::filesystem::path& exe_dir, std::wstring arguments);
  Progress Get() const;
  // Stops the game's tool: what it compiled stays in the driver's caches; the next Play compiles the rest.
  void Skip();
  // Back to kIdle, once its result was used.
  void Reset();

 private:
  void Run(std::filesystem::path exe_dir, std::wstring arguments);

  std::thread worker_;
  mutable std::mutex mutex_;
  Progress progress_;
  bool skip_ = false;
};

}  // namespace launcher
