# Changelog

The launcher's versions (`launcher_version.h`). Each game shows its own version beside it.

## 3.3.0

- Controls: a controller (not a guitar or a drum kit) has the keyboard's strum switch too, for a game with
  `game::kKeyboardTapTarget`: its frets strum as they are pressed, one switch for every controller playing guitar
  (the SDK's `gamepad_tap_target`, recomp change 57), with its tip in the help.

## 3.2.0

Two colors more in each game's palette (`launcher/palette.h`; every game's defines them), for a design whose accent
buttons are written in white (GH Van Halen's mockup, "GHVanHalen Launcher"):

- `col::kGlyphOnAccent`: the controller button's disc inside an accent button (PLAY, Choose a folder, Check for
  updates), and the key cap or pill there. It was `kOnAccent`, the button's text; a game whose text on the accent is
  dark sets it to `kOnAccent` and looks the same.
- `col::kDonateIcon`: Donate's heart. It was `kOnDonate`, the button's text; likewise.

## 3.1.1

- Settings: a click on a row's arrow steps its value at once; the mouse no longer needs the row started with A or
  Enter first (the row took the click: its arrows are drawn over it, and ImGui gave the click to the row).

## 3.1.0

Singing, for a game that sets `game::kHasMicrophones` (Guitar Hero: Metallica; every game's `game.h` defines it,
`false` where the game has no microphone, and `kMicrophoneGainDefault`):

- Controls: a Microphone among the devices, under the keyboard, marked experimental. Its rows choose the PC's
  recording device the game hears as its USB microphone (none, the Windows default, or any device; one not connected
  now stays chosen, marked), the channel of it (all of them mixed, or one: an audio interface's input), its gain
  (-24 to +6 dB in steps of 6; `game::kMicrophoneGainDefault` until changed) and show its level as the game hears it,
  from the device while the rows show. Its hint says which controllers sing. The SDK's `usb_microphone_1`,
  `usb_microphone_1_channel` and `usb_microphone_1_gain` (recomp change 55).
- Each gamepad, and the keyboard, has an Instrument row: the game's instrument, or the vocals (a plain gamepad, the
  singer's). A controller that sings shows no buttons to assign, only that row and the microphone it sings with. The
  controller is kept by its serial number, or its GUID where it has none, as the game's SDL reads them: the SDK's
  `gamepad_device_subtypes` (recomp change 56). Its status in the devices list says so. `GameTexts::part_instrument`
  and `part_vocals` give the game's words.

## 3.0.0

A new design (Gustavo's mockup of 2026-10-28, "GHMetallica Launcer 2"), the same for every game; each game gives its
colors, its art and its words.

- The shell: the game's art under every page (darkened under all but Play), a header with the game's logo and name,
  the tabs between LB and RB (Play, DLC, Achievements, Controls, Settings, Updates), Donate (Ko-fi) and the profile's
  avatar; a footer with the controller connected, the version and the controller's buttons for the page. The
  sidebar is gone.
- Fonts: Oswald for titles and Source Sans 3 for text (both SIL OFL, `fonts/`), in place of Barlow.
- Play: the game's name over the art with PLAY (A) and Quit, the last session; cards for the achievements, the
  settings in use and the updates. Y opens Donate.
- Settings: one tab with its categories (Game, Video, Graphics) on the left, each option a row stepped with left and
  right, the help of the row in focus on the right, Reset (Y). The game's switches can be screen effects
  (`game::ToggleGroup::kScreenEffects`, under Graphics); a switch can need another (`Toggle::needs`: off and locked
  while that one is off).
- Controls: one tab for every device: the controllers SDL sees and the keyboard on the left, the chosen one's mapping
  in the middle (the game's functions, or the Xbox 360 controls), its help on the right. A reassigns, X clears, Y
  resets. The keyboard keeps its two keys per function, its switches and the mouse.
- Achievements: a grid with the detail of the chosen one, secret ones revealed with Y, the filter on X.
- DLC: packages install as soon as a folder is chosen or dropped; the installed list marks the new ones.
- Updates: the version's notes with their headings as tags, the last check, Check again.
- The first run: four steps in the header and a card over the art for each (Welcome, Choose your ISO and its
  invalid variant, Extracting, Ready).
- Profiles: the avatar opens "Who's playing?" (every profile, switching, creating); the profile's page with its
  summary (totals, the week, the recent achievements, the showcase) and its games (LB and RB); a game's achievements
  with a star for the showcase; editing and creating a profile in one page with a preview. A profile's level shows
  on its card for every profile.
- The controller: up from a page's top reaches the header (the tabs, Donate, the avatar), and A on a tab opens it
  with the focus in the page; View (Back) opens the profiles. Each page starts on its own control (the chosen
  device, the chosen achievement, the current category), even inside a scrolling list (`ui::RequestFocus`).
  Settings: A starts changing a row (left and right step it until A or B), otherwise left goes back to the
  categories. Controls: up and down go through the devices, A or right goes to the chosen one's mapping, left
  comes back. Achievements: All lists the unlocked ones first.
- The buttons' glyphs follow what the player uses last: an Xbox controller's letters, a PlayStation controller's
  symbols (and Create, Share or Select), a Switch controller's letters by position, or the keyboard's keys, which
  now do the same: Enter (A), Esc (B), F (X), R (Y), Q and E (LB, RB), P (View). Only A, Enter, Space or the mouse
  press a control: Y on Play opened Donate and pressed PLAY too (ImGui's navigation takes Y as its "input").
- Settings: the chosen row is outlined only while the rows have the focus.
- Video, Format: the image's aspect ratio by name, 16:9, 21:9, 32:9 and the screen's own (marked "your screen")
  in place of "fill the screen" / "16:9 with bars"; each says whether it fills the screen or has bars at the
  sides or above and below. With NativeXRenderer every one can be chosen on any screen (its D104,
  `native_x_renderer_aspect`: 21:9 on a 16:9 screen, 32:9 on a 21:9 one); with Xenos the screen's or 16:9.
  `Options::native_aspect` replaces `native_fill`; `NativeRenderSize` / `NativeHeights` take the image's aspect
  ratio.

For a game moving to 3.0.0 (its `launcher/`):
- `game.h`: `kBrandTitle` and `kBrandSubtitle` (the header's two lines) in place of `kSidebarTitle` /
  `kSidebarSubtitle`; `kArtFocusX` / `kArtFocusY` for `art/background.svg` in place of `kHeroFocusX` / `kHeroFocusY`;
  no `kInstallTitleCard` nor `kHeroTitleBar`; `kToggles` entries are `{cvar, default, group, needs}` and `GameTexts`
  has `toggle_help`; `GameControl` has no `on_guitar`; `GameTexts` has no `gamepad_nav`, `gamepad_title`,
  `gamepad_subtitle` nor `functions_column`.
- `palette.h`: the shell's colors (`kHeader` to `kGhostBorder`, see the template's), `kRoundness` 1.
- `res/launcher.rc`: the fonts `IDR_FONT_REGULAR` .. `IDR_FONT_DISPLAY_BOLD` (`fonts/SourceSans3-*.ttf`,
  `fonts/Oswald-*.ttf`) and `IDR_ART_BACKGROUND` (`art/background.svg`, 1280 x 760); no `hero.svg` nor `install.svg`.

## 2.5.0

- The game's own icon, the one the Xbox 360 dashboard shows, in place of its mark: the sidebar's logo, the first
  run's, and the game's tile in the profile pages. It is read from the player's files once the game is installed,
  never carried by the launcher: the disc's `nxeart` package (its header's title thumbnail), else the one the game
  writes with its achievements (`achievements\<title ID>\title.png`, the SDK's change 52, for discs without
  `nxeart`). A list written before the game wrote its icon has the game asked once to write it again. Until then,
  or for a game with neither, the mark on accent as before.
- The icon goes to the store (`games\<title ID>\icon.png`), so that the other launchers show it on that game's tiles;
  a game whose launcher has not left one there keeps its mark.

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
