# Changelog

The launcher's versions (`launcher_version.h`). Each game shows its own version beside it.

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
