// The mod packs (launcher next; the Mods tab's Packs section, page_mod_packs.cpp; game::kHasModPacks): folders of files
// that the game mounts over its own when it starts (the SDK's game_data_overlays), each switched on or off.
// - The Mods folder: Mods\ in the game's user data folder (GameUserDataFolder), made when a pack is first added.
// - A pack: Mods\<folder>\ with a mod.toml of flat `key = value` lines (format = 1, id, name, version, author,
//   description, title_id; name_<language> and description_<language> optional), and files\ (whole files at the game
//   data's relative paths), patches\ (the same paths with .delta, each turning one of the player's own game files into
//   the pack's), or both.
// - Preparing a pack applies each patch to the player's file (GameDataRoot) and writes the result into files\.
//   prepared.txt beside files\ records each one (the patch's size and time, the result's size and SHA-256): a pack
//   updated is prepared again, one prepared is not read again at every start. A patch refused leaves the results made
//   before it in files\ (recorded, kept for the next try); the pack can be on only once every patch is prepared.
// - Mods\enabled.txt: the packs on, a folder a line in load order (by folder name: a later one's file wins), written
//   whole by the launcher; the game reads it when it starts (src/startup.cpp, ConfigureModPacks).
// Nothing here writes the game's own files, nor through a link. The format of mod.toml and of the patches:
// docs/mods.md.

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace launcher {

struct ModPack {
  enum class State {
    kReady,           // nothing to prepare: it can be switched on
    kNeedsPreparing,  // patches not applied yet, or applied from an earlier version of the pack
    kOtherFiles,      // made for other game files: a patch's base is not the player's file (`detail`: its path)
    kOtherGame,       // made for another game (`detail`: its title_id)
    kBroken,          // `broken` says why; never switched on
    kUnavailable,     // its mod.toml is there but could not be read now (another program has it): left as it was
  };
  enum class Broken {
    kNone,
    kModToml,     // no readable mod.toml, or one without its format, id or title_id
    kFormat,      // a format other than 1 (`detail`)
    kEmpty,       // neither files\ nor patches\ holds a file
    kPatch,       // a patch that is not one (`detail`: its path)
    kTooLarge,    // a patch whose result would be over kMaxModPatchResult (`detail`: its path)
    kFolderName,  // a folder enabled.txt cannot name: starting with '#' or '.', or starting or ending with a space or
                  // a dot
  };
  std::string folder;  // its folder's name, UTF-8: the line of enabled.txt
  std::string id, name, version, author, description;  // mod.toml's; `name` the folder's when it has none
  // mod.toml's name_<language> and description_<language>, by the launcher's language key ("de", "es", "fr", "it").
  std::map<std::string, std::string> names, descriptions;
  State state = State::kBroken;
  Broken broken = Broken::kNone;
  std::string detail;
  // The game data's paths it replaces (its files and patches), lower case with '/' separators: two packs on that
  // replace the same file.
  std::vector<std::string> files;
  uint64_t prepare_bytes = 0;  // the results still to write (saturating)
  bool on = false;             // in enabled.txt
};

// The largest result a patch may have (16 GiB, far above any file of the game): one over it makes its pack broken.
inline constexpr uint64_t kMaxModPatchResult = uint64_t(16) << 30;

struct ModPacks {
  std::filesystem::path folder;  // the Mods folder
  std::vector<ModPack> packs;    // by folder name
  // In enabled.txt although it cannot be mounted as it is (to be prepared, made for other files or another game,
  // broken, gone): taken out of it by this reading.
  std::vector<std::string> turned_off;
  size_t On() const;
};

// <the game's user data>\Mods.
std::filesystem::path ModsFolder(const std::filesystem::path& exe_dir);
// The Mods folder's packs, checked against the game files (GameDataRoot): reads each mod.toml, lists files\ and
// patches\, reads each patch's header and the player's file's size (no file is read whole). A pack in enabled.txt
// that cannot be mounted as it is is taken out of it. Run it off the UI thread.
ModPacks ScanModPacks(const std::filesystem::path& exe_dir);
// Switches a ready pack on, or a pack off: enabled.txt written again. False, the switch put back as it was, when it
// could not be written.
bool SetModPackOn(const std::filesystem::path& exe_dir, ModPacks& packs, size_t index, bool on);

// A pack deleted (Mods > Packs, its Remove): its folder first renamed to Mods\.removing <folder>, which Windows refuses
// while a program has one of its files open (nothing changed then), taken out of enabled.txt, then deleted without
// following a link inside it (a link is removed, never what it points to); taken out of `packs`. kFilesLeft: out of
// the list and of enabled.txt, but a file of it could not be deleted (`error`: its path); the next removal tries
// those again. The launcher's own .removing folders are the only ones it deletes besides the pack's.
struct ModPackRemove {
  enum class Result { kRemoved, kFilesLeft, kFailed };
  Result result = Result::kFailed;
  std::string error;  // in English (the log), or the system's message
};
ModPackRemove RemoveModPack(const std::filesystem::path& exe_dir, ModPacks& packs, size_t index);

// A pack added from a .zip or a folder that holds a mod.toml at its top or one folder down, installed as Mods\<id>.
struct ModPackAdd {
  enum class Result {
    kAdded,
    kReplaced,
    kAsk,        // Mods\<id> holds the same pack: replaced only once the player says so (`existing_version`)
    kOtherPack,  // Mods\<id> holds another pack, or a folder that is not one: nothing written
    kNotAPack,   // no mod.toml, or one whose id cannot name a folder
    kNotAZip,    // another kind of archive (.7z, .rar): a .zip or a folder is needed
    kFailed,     // `error`: what could not be read or written
  };
  Result result = Result::kFailed;
  std::string id, name, version;
  std::map<std::string, std::string> names;  // its name_<language>
  std::string existing_version;
  std::string error;
};
// `replace`: a pack of the same id is replaced (its prepared files with it). Reads the whole zip: run it off the UI
// thread. Never throws: what goes wrong is kFailed, with no half pack left in the Mods folder.
ModPackAdd AddModPack(const std::filesystem::path& exe_dir, const std::filesystem::path& source, bool replace);

// Applies one patch (RCMPDLT1, docs/mods.md) to `base`, writing `out`: streamed, the base checked (its size and
// SHA-256) before anything is written, the result written as <out>.part<process id> and renamed to `out` only once its
// size and SHA-256 are the patch's. `progress` is told the bytes read of the base and written of the result so far, and
// stops the work when it returns false. Refused, nothing is left at `out` nor at the temporary name.
struct ModPatchResult {
  bool ok = false;
  bool base_differs = false;  // the base is not the patch's (its size or SHA-256), or missing
  bool damaged = false;       // the patch is not a valid one
  std::string error;          // why it was refused, in English (the log), or the system's message for a write
  std::string sha256;         // ok: the result's, lower-case hex
};
ModPatchResult ApplyModPatch(const std::filesystem::path& base, const std::filesystem::path& patch,
                             const std::filesystem::path& out, const std::function<bool(uint64_t)>& progress);

// A pack's patches applied on a thread of its own, one file after another; cancelled between files and within one.
// Never throws: what goes wrong is kFailed.
class ModPackPreparation {
 public:
  enum class State { kIdle, kRunning, kDone, kFailed, kCancelled };
  struct Progress {
    State state = State::kIdle;
    std::string folder;   // the pack's
    size_t index = 0;     // the file being prepared, of `count`
    size_t count = 0;
    std::string current;  // its path, UTF-8
    uint64_t done = 0;    // bytes read and written so far, of `total` (each base and each result)
    uint64_t total = 0;
    double seconds = 0;
    // kFailed: a base not the player's file (kOtherFiles), a damaged patch (kBroken), or `error` (the system's
    // message, the game files missing, a link in the way); `detail` the file's path.
    ModPack::State failed_as = ModPack::State::kReady;
    std::string detail;
    std::string error;
  };

  ~ModPackPreparation();
  void Start(const std::filesystem::path& exe_dir, const std::string& folder);
  void Cancel();
  Progress Get() const;
  // Back to kIdle once its end has been taken.
  void Reset();

 private:
  void Run(std::filesystem::path exe_dir, std::string folder);

  std::thread worker_;
  std::atomic<bool> cancel_{false};
  mutable std::mutex mutex_;
  Progress progress_;
};

}  // namespace launcher
