// The game's achievements for the Achievements page and the Play card.
//
// They live in the game's executable, which the launcher does not read: the game writes them for launchers each
// time it starts (the SDK's achievement cache, rex/system/achievement_cache.h), next to the unlocks, in its user
// data folder (userdata\ next to it, or Documents\<game::kDocumentsFolder> when that cannot be written):
// - achievements\<TITLEID>.info.toml: every achievement in every language the game has, and its icon;
// - achievements\<TITLEID>\<image id>.png: the icons, the game's own, and title.png, the game's (FindGameIcon);
// - achievements\<TITLEID>.toml: the unlocks, with their times.
// When the list is missing (the game has not run with this version yet, or was just installed), the game is asked
// to write it and quit (--achievements_cache_only): about a second, with no window.

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "picture.h"

namespace launcher {

struct Achievement {
  uint32_t id = 0;
  uint32_t gamerscore = 0;
  uint32_t flags = 0;
  // In the launcher's language, or the game's default one.
  std::string label;
  std::string description;
  std::string unachieved_description;
  uint64_t unlocked = 0;  // when it was unlocked (FILETIME, UTC); 0 = locked
  // The icon as RGBA pixels (straight alpha); empty if it has none.
  std::vector<uint8_t> icon;
  int icon_width = 0;
  int icon_height = 0;

  // Hidden until unlocked: the game's "show unachieved" flag (0x8) is clear.
  bool secret() const { return (flags & 0x8) == 0; }
};

struct Achievements {
  std::vector<Achievement> items;  // in the game's order
  int unlocked = 0;
  uint32_t gamerscore = 0;
  uint32_t total_gamerscore = 0;
};

// Reads the list and the unlocks for the language (an XLanguage). nullopt when the game has not written the list.
// Slow (files, icons): for a thread.
std::optional<Achievements> LoadAchievements(const std::filesystem::path& exe_dir, uint32_t language);
// The same from a list's folder (<TITLEID>.info.toml and its icons) and an unlocks file: another game's, as the
// profiles' store keeps them (profile_store.h).
std::optional<Achievements> LoadAchievementsFrom(const std::filesystem::path& folder, uint32_t title_id,
                                                 const std::filesystem::path& unlocks_file, uint32_t language);
// The folder where this game writes its list (its user data folder's achievements\).
std::filesystem::path AchievementsFolder(const std::filesystem::path& exe_dir);

// Starts the game to write the list, without a window, and waits for it (at most 30 s). False if it could not run.
bool WriteAchievementList(const std::filesystem::path& exe_dir);

// The game's own icon, the one the console's dashboard shows: the launcher's logo, and the game's picture in the
// other launchers (the profiles' store keeps a copy, profile_store.h). The launcher does not carry it; it is read
// from the player's files once the game is installed:
// - the disc's nxeart package (the dashboard's art for the game): its header's title thumbnail;
// - else the one the game writes with the list (the SDK's achievement cache: achievements\<TITLEID>\title.png, the
//   XDBF's title image), for a disc without nxeart.
// Until then, or for a game with neither, the launcher draws the game's mark (game.h's kBrandIcon).
struct GameIcon {
  std::vector<uint8_t> png;  // the file, as the game has it
  Rgba picture;
};
// nullopt when neither is there (or cannot be read). Reads files: for a thread.
std::optional<GameIcon> FindGameIcon(const std::filesystem::path& exe_dir);

// A FILETIME as a date in local time, in the language's order ("15/10/2026", "10/15/2026", "15.10.2026").
std::string FormatDate(uint64_t filetime, uint32_t language);

}  // namespace launcher
