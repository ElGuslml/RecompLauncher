// The players' profiles, shared by every game's launcher and read by each game: one store in
// %APPDATA%\ElGuslml Recomps\. Compiled into the launchers and into the game executables (RecompLauncher.cmake's
// recomp_launcher_add_profiles), so nothing here uses SDL or Dear ImGui.
//
//   store.toml                   schema, the current profile, the launcher version that wrote it last
//   profiles\<id>\profile.toml   a profile: its name, motto, avatar, color, dates
//   profiles\<id>\               the profile's folder, the game's profile_root (SDK change 47): the saves
//                                (B13EBABEBABEBABE\<title>\...), the settings in each game (<title>\profile\), the
//                                achievement unlocks (achievements\<title>.toml), the playtime (games\<title>.toml)
//   games\<title>\               a game whose launcher ran: game.toml, a copy of its achievement list
//   backups\                     what a migration replaced
//
// A launcher keeps the store; a game only reads which profile it runs with (ForGame). The saves made before the
// profiles stay in each installation's user data folder until its launcher copies them into the current profile
// (Migrate): until then the game keeps using them there.

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace launcher::profiles {

// store.toml's schema: a launcher that finds a newer one only reads the store (a newer launcher wrote it).
inline constexpr int kSchema = 1;
// A profile's name, in characters: an Xbox 360 gamertag's length, so that the game shows it whole (the SDK passes
// it in ASCII, accents folded).
inline constexpr size_t kNameMax = 15;
inline constexpr size_t kMottoMax = 40;

struct Profile {
  std::string id;           // 16 hex digits: the folder's name
  std::string name;         // UTF-8
  std::string motto;        // UTF-8, may be empty
  std::string avatar;       // a built-in avatar's key ("fox", "owl", "robot", "cat", "skull") or "photo" (avatar.png)
  int color = 0;            // the profile's color: an index in the launcher's list
  int64_t created = 0;      // Unix time
  int64_t last_used = 0;    // Unix time: made current or played
  // The showcase: up to kShowcaseMax achievements, each "<TITLEID>:<achievement id>" ("41560830:65").
  std::vector<std::string> showcase;
  std::filesystem::path folder;
};

inline constexpr size_t kShowcaseMax = 3;

// A game whose launcher has run (games\<TITLEID>\game.toml): what the other launchers show of it.
struct GameInfo {
  uint32_t title_id = 0;
  std::string name;                // "Guitar Hero: Metallica"
  uint32_t accent = 0;             // its launcher's accent, 0xRRGGBB
  std::string icon;                // its mark: SVG elements in a 24-unit box, stroked (game.h's kBrandIcon)
  std::filesystem::path launcher;  // its launcher's executable, where it was last seen
  std::string version;             // the game's version there
  int64_t seen = 0;                // Unix time: its launcher last ran
  std::filesystem::path folder;    // games\<TITLEID>
};

struct Store {
  std::filesystem::path root;
  bool found = false;       // store.toml was there
  bool read_only = false;   // its schema is newer than kSchema
  std::string current;      // the current profile's id
  std::string writer;       // the launcher version that wrote store.toml last
  std::vector<Profile> profiles;

  const Profile* Current() const;
  Profile* Find(std::string_view id);
};

// %APPDATA%\ElGuslml Recomps (empty if Windows does not say where %APPDATA% is), or the folder the environment
// variable ELGUSLML_RECOMPS_DIR names (tests: a store apart from the player's; the game inherits it from the launcher).
std::filesystem::path StoreFolder();

// The store as it is on disk; an empty one (found false) if it does not exist yet.
Store Load();
// store.toml and each profile.toml, written whole and replaced at once (a reader never sees half a file). False on
// a read-only store or an error.
bool Save(const Store& store, std::string_view writer);
bool SaveProfile(const Store& store, const Profile& profile);
// A new profile with its folder, made current; null on a read-only store or an error.
Profile* Create(Store& store, std::string_view name, std::string_view avatar, int color, std::string_view writer);
// The profile's folder sent to the Recycle Bin (its saves with it) and its entry dropped; the current one becomes the
// one used last. False for the last profile, a read-only store or an error.
bool Delete(Store& store, std::string_view id, std::string_view writer);

// The games whose launcher ran (games\*\game.toml), and this one's entry written (each launcher at its start).
std::vector<GameInfo> Games(const Store& store);
bool RegisterGame(const Store& store, const GameInfo& game);
// The game's achievements list for the other launchers (games\<TITLEID>\achievements\: <TITLEID>.info.toml and the
// icons, as the game writes them in its user data folder): copied from `from` when newer.
std::filesystem::path GameAchievementsFolder(const Store& store, uint32_t title_id);
bool CopyAchievementList(const Store& store, uint32_t title_id, const std::filesystem::path& from);
// The profile's unlocks of a game (the SDK's file in the profile's folder), and what such a file holds: each
// achievement's id and its unlock time (FILETIME, 0 when unknown).
std::filesystem::path UnlocksFile(const Profile& profile, uint32_t title_id);
std::map<uint32_t, int64_t> Unlocks(const std::filesystem::path& file);

// A game's playtime in a profile (the profile's games\<TITLEID>.toml), counted by its launcher from Play to the game's
// exit: a game started without its launcher is not counted.
struct Playtime {
  int64_t seconds = 0;
  int64_t first = 0;   // Unix time: the first session's start
  int64_t last = 0;    // Unix time: the last session's end (or its last count, while it runs)
  int sessions = 0;
  std::map<std::string, int64_t> days;  // seconds on each local day, "2026-10-26"
};
std::filesystem::path PlaytimeFile(const std::filesystem::path& profile_folder, uint32_t title_id);
Playtime ReadPlaytime(const std::filesystem::path& file);
// A stretch of a session, `from` to `to` (Unix times), added to the profile's playtime of the game, split at local
// midnights; `new_session` counts a session. Only the game's launcher writes its file.
bool AddPlaytime(const std::filesystem::path& profile_folder, uint32_t title_id, int64_t from, int64_t to,
                 bool new_session);

// UTF-8 strings measured and cut in characters (code points), and trimmed of spaces.
size_t CountChars(std::string_view utf8);
std::string Clip(std::string_view utf8, size_t chars);
std::string Trim(std::string_view text);

// An installation's data from before the profiles, in its user data folder (a game's userdata\, or
// Documents\<game>\ when its folder could not be written).
struct LegacyData {
  std::vector<std::filesystem::path> saves;     // <user data>\B13EBABEBABEBABE\<title>\, each with files in it
  std::vector<std::filesystem::path> settings;  // <user data>\<title>\profile\User\, each with files in it
  std::vector<std::filesystem::path> unlocks;   // <user data>\achievements\<title>.toml
  bool empty() const { return saves.empty() && settings.empty() && unlocks.empty(); }
};
LegacyData FindLegacy(const std::filesystem::path& user_data);
// The installation's data was copied into a profile of `store` that still exists: its marker, profile_migration.toml.
bool IsMigrated(const std::filesystem::path& user_data, const Store& store);

// The profile a game runs with: the store's current one, once the installation has nothing left to copy into it
// (no data from before the profiles, or already copied). Otherwise nullopt: the game keeps its user data folder.
struct GameProfile {
  std::filesystem::path folder;
  std::string name;
};
std::optional<GameProfile> ForGame(const std::filesystem::path& user_data);

// The migration. The profile already has saves or settings of a title the installation has data of: the player
// chooses which to keep.
bool ProfileHasDataOf(const Profile& profile, const LegacyData& legacy);
enum class Keep { kInstallation, kProfile };
struct MigrationResult {
  bool ok = false;
  int files = 0;            // files copied
  std::string error;        // UTF-8, when not ok
};
// Copies the installation's saves and settings into the profile's folder (or, with Keep::kProfile, only where the
// profile has none of that title), checks every file against its original, merges the unlocks (the earliest date
// of each), then writes the marker and `note` (a text file for the player: where the saves went). The installation's
// data stays where it was. What the profile had and the copy replaces goes to backups\ first.
MigrationResult Migrate(const std::filesystem::path& user_data, const LegacyData& legacy, const Profile& profile,
                        Keep keep, const Store& store, std::string_view note);

}  // namespace launcher::profiles
