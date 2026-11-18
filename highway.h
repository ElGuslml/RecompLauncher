// The Mods tab's Highway section (launcher 3.30, game::kHighwayPreview): the game's own textures for the preview, read
// from the installed game's files (game::kHighwayAssets: a Neversoft pak and the texture dictionary in it, the Xbox 360
// textures untiled and decoded here), the player's folder of pictures with their thumbnails, and the animated preview
// of the note highway drawn with the projection the game uses (the game's src/mods.cpp makes the same one).

#pragma once

#include <imgui.h>
#include <imgui_internal.h>

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "game.h"

namespace launcher::highway {

// A texture ImGui draws: 0 until it exists.
struct Texture {
  ImTextureID id = 0;
  int width = 0;
  int height = 0;
};

// The game's textures the preview draws (game::kHighwayAssets' keys, in that order).
enum class Part { kHighway, kSidebar, kString, kFretbar, kCollar, kGemBase, kGemColor, kGemCap, kSustain, kCount };

class GameTextures {
 public:
  GameTextures() = default;
  GameTextures(const GameTextures&) = delete;
  GameTextures& operator=(const GameTextures&) = delete;
  ~GameTextures();
  // Starts reading them once, on a thread of their own: `pak` the game's pak (game::kHighwayAssets.pak in the game's
  // files).
  void Load(const std::filesystem::path& pak);
  // On the main thread, every frame: the textures made from what the thread decoded.
  void Update();
  bool Ready() const { return ready_; }
  bool Failed() const { return failed_; }
  const Texture& Get(Part part) const { return textures_[size_t(part)]; }

 private:
  struct Decoded {
    std::vector<uint8_t> rgba;
    int width = 0;
    int height = 0;
  };
  std::thread thread_;
  std::mutex mutex_;
  bool started_ = false;
  bool done_ = false;     // the thread finished (under mutex_)
  bool ok_ = false;       // and found every texture
  std::vector<Decoded> decoded_;
  bool ready_ = false;
  bool failed_ = false;
  Texture textures_[size_t(Part::kCount)];
};

// A picture of the player's folder: PNG, JPEG, BMP or TGA, as the game takes them.
struct Picture {
  std::string name;           // the file's name, UTF-8
  int64_t modified = 0;       // its last write, for the list's refresh
  bool tried = false;         // its thumbnail decoded or found unreadable
  bool unreadable = false;
  bool no_preview = false;    // a TGA, which the game reads and Windows does not: no thumbnail
  int width = 0;              // the file's own size
  int height = 0;
  float luminance = 0.0f;     // its average, 0-1 (the automatic darkening)
  Texture thumbnail;
};

class Pictures {
 public:
  Pictures();
  Pictures(const Pictures&) = delete;
  Pictures& operator=(const Pictures&) = delete;
  ~Pictures();
  // The folder (UTF-8): listed again when it changes, and every two seconds while this is called.
  void Refresh(const std::string& folder);
  // On the main thread, every frame: the textures made from what the thread decoded.
  void Update();
  const std::vector<Picture>& List() const { return list_; }
  // A picture's place in List(), or -1.
  int Find(std::string_view name) const;
  // A picture at the preview's size: its texture once decoded (asked for otherwise); nullptr until then or when it
  // cannot be read.
  const Texture* Large(std::string_view name);
  bool FolderExists() const { return exists_; }

 private:
  enum class Kind { kThumbnail, kLarge };
  struct Job {
    uint64_t generation;
    std::string name;
    Kind kind;
  };
  struct Result {
    uint64_t generation;
    std::string name;
    Kind kind;
    bool ok = false;
    std::vector<uint8_t> rgba;
    int width = 0, height = 0;            // the decoded size
    int file_width = 0, file_height = 0;  // the file's
    float luminance = 0.0f;
  };
  void Work();
  void List(const std::filesystem::path& folder);
  void FreeTextures();

  std::string folder_;
  std::filesystem::path path_;
  bool exists_ = false;
  double listed_at_ = -100.0;
  uint64_t generation_ = 0;
  std::vector<Picture> list_;
  struct LargeEntry {
    Texture texture;
    bool failed = false;
    bool asked = false;
  };
  std::map<std::string, LargeEntry> large_;
  std::list<std::string> large_order_;  // the most recently drawn first

  std::thread thread_;
  std::mutex mutex_;
  std::condition_variable wake_;
  bool quit_ = false;
  std::filesystem::path work_folder_;  // the thread's copy of path_ (under mutex_)
  uint64_t work_generation_ = 0;
  std::list<Job> jobs_;
  std::vector<Result> results_;
};

std::filesystem::path PathFromUtf8(const std::string& utf8);
std::string Utf8Of(const std::filesystem::path& path);

// What the Highway section's rows chose, as the preview draws it.
struct Look {
  double scale = 1.0;        // the one-player highway's scale (game::kChoices' ghwor_highway_scale)
  double length = 1.0;       // its length, the time on it
  double perspective = 1.0;  // its compression
  bool dark = false;         // the community's black highway: the texture's multiplier black
  float opacity = 1.0f;      // the highway's alpha (the venue through it)
  float brightness = 1.0f;   // the picture's darkening, 1 = as it is
};

// The brightness of a darkening's value ("off", "auto", a percent) for a picture of `luminance`, as NativeXRenderer's
// D177 computes it.
float Brightness(std::string_view value, float luminance);

// Draws the animated highway into `area` (clipped to it): the venue's dark behind, the highway with `picture` (nullptr:
// the game's own texture), its sidebars and strings, the beat lines, the strike line and a few notes coming down at
// the game's speed. `time` in seconds.
void Draw(const ImRect& area, const GameTextures& game, const Texture* picture, const Look& look, double time);

}  // namespace launcher::highway
