# Changelog

The launcher's versions (`launcher_version.h`). Each game shows its own version beside it.

## 2.4.0

- Playtime: the time from Play to the game's exit goes to the profile (`profiles\<id>\games\<title ID>.toml`: the
  seconds, the first and last session, the sessions, the seconds of each local day), written each minute while the
  game runs and at its exit; the computer's sleep is not counted, nor a game that closes within seconds, nor a game
  started without its launcher.
- The profile page shows it: hours played (and in how many games) and the best streak of days with play (and the
  current one) among the totals; each game's hours on its card; the week's activity (a bar a day, Monday first,
  the week's hours, the current streak) beside the showcase; the recent achievements across.
- My games: sorted by most played too; each game's hours; Continue on the game played last and Play on the others.
- Play or Continue on another game opens that game's launcher (where it last ran) and closes this one; a game whose
  launcher is gone shows no Play.

## 2.3.0

- The profile in every launcher: a card at the bottom of the sidebar (avatar, name, level) opens a menu with the
  profile, "See my profile", "Edit", the other profiles (switching sets the current one for every launcher) and
  "Create new profile".
- The profile page: banner in the profile's color, level (100 XP a level, 1 XP per Gamerscore point), and two tabs.
  Summary: achievements, Gamerscore and games of every game on this PC, the games' cards (completion, "See
  achievements", "Play" for this one), the showcase and the latest achievements. My games: the games sorted by last
  played, completion or name.
- A game's achievements in the profile, for any game whose launcher ran on this PC: totals, completion, unlocked /
  locked / all, and a pin on each unlocked one for the showcase (at most three, kept in `profile.toml`).
- Edit profile: a preview as the launchers show it, the name, a motto (up to 40 characters), the avatar or a photo,
  the color; Delete profile sends it to the Recycle Bin with its saves after a second press, never the last one.
- Each launcher registers its game in the store (`games\<title ID>\game.toml`: name, accent, icon, launcher path,
  game version, last seen) with a copy of its achievement list, so that the other launchers can show it.

## 2.2.0

- Profiles shared by every launcher, in `%APPDATA%\ElGuslml Recomps\` (`profile_store.h`): `store.toml` (the current
  profile, a schema: a launcher that finds a newer one only reads), `profiles\<id>\profile.toml` (name of at most 15
  characters, motto, avatar, color, dates) and each profile's folder, where the game keeps its saves, settings and
  achievement unlocks (the SDK's `profile_root`, change 47).
- The first start without a profile asks for one ("Create your profile": the Windows user's name proposed, five
  avatars or a photo cropped to a circle, six colors).
- The saves, settings and unlocks this installation had before the profiles are copied into the current profile,
  checked file by file, and the installation keeps its copy with a note; a profile that already has saves of the game
  asks which to keep (the other goes to `backups\`).
- The game runs with the current profile (`recomp_launcher_add_profiles`: `profile_store.cpp` in the game executable,
  which sets `profile_root` and `profile_name`), unless the installation's saves are not copied yet.
- The Achievements page reads the profile's unlocks.
- A text field, avatars drawn from SVG, photos (Windows Imaging Component) for the launcher's pages.
- `ELGUSLML_RECOMPS_DIR`: another folder for the profiles (tests).

## 2.1.0

- Its own repository, a submodule of each game (`recomp_launcher/`), built with `RecompLauncher.cmake`. The types of a
  game's profile are `game_types.h`'s.
- The launcher's version beside the game's: the sidebar ("Version 0.2.0 · Launcher 2.1.0") and the Updates page.
- DLC page (`game::kHasDlcPage`): Xbox 360 content packages from a folder, installed by the game's `--install_content`
  tool mode (SDK change 44), listed and removed; "Open the DLC folder".
- A game's own look: corner scale (`col::kRoundness`), first-run steps (`col::kStepRadius`), the sidebar's title
  (`game::kSidebarTitle`, `kSidebarSubtitle`), the Play page's title bar (`game::kHeroTitleBar`, `col::kTitleBar`),
  titles in capitals (`game::kUppercaseTitles`), first-run and Gamepad page texts (`GameTexts`).
- The Keyboard and Gamepad pages in the game's words (`game::GameControlsFor`): functions with colored chips, the
  other controls after them; a guitar's buttons named after its frets.
- The keyboard's defaults per game (`game::kKeyboardDefaults`) and a switch that taps a control on other keys' presses
  (`game::kKeyboardTapTarget`, the SDK's `mnk_tap_target`, change 45).
- The renderer chosen until the player picks one (`game::kNativeXRendererDefault`).

## 2.0.0

The launcher as published with KameoRecomp 0.1.0 and PiratesAtWorldsEndRecomp 0.2.0 (Launcher 2): Play, achievements,
keyboard and gamepad, video and graphics settings, the game's own options, updates from GitHub releases, the first run
from the player's ISO.
