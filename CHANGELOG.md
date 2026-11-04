# Changelog

The launcher's versions (`launcher_version.h`). Each game shows its own version beside it.

## 3.11.1

Gustavo, 2026-11-04: back from the game the launcher kept freezing for seconds and coming back; a CPU core was busy
while it was in front (sampled from outside: 300-580 ms of CPU every half second), and its log had one 5.5 s stop
(the watchdog's "stopped answering Windows").
- The launcher idles again. Every SDL event counted as input and reset the idle count: a DualSense's sticks move a few
  units at rest all the time, and its sensors report, so it drew at the display's rate without stopping. Now a stick or
  trigger counts only outside ImGui's dead zone (8000) or crossing it, sensors and the touchpad not at all, and such
  events wake the loop without drawing a frame (one every quarter of a second at most while idle). A controller held
  (as ImGui last read it) keeps it drawing, for the menus' repeat.
- The page's first focus (`focus_page_`, set at the start and back from the game) waited for a control to focus
  forever on a page with none, and during the first run's screens, keeping the launcher busy: it gives up after 30
  frames, and is not "busy" during the first run. On a hidden desktop: 1547-1600 frames every 10 s before, idle after
  (172 ms of CPU in 12 s with the game's files, 125 ms on the first run).
- The log: a Present over 100 ms (with how many and the longest, at most a line a second); the main loop's heartbeat
  (`Heartbeat`): a pass late by 3 s outside a wait on purpose (the game running, a message box) is logged and the
  run's first dumped (`launcher_NNN_stall.dmp`), as Windows' 5 s "not responding" misses a thread blocked a few
  seconds at a time; the focus lost (with the program that has the foreground) and back, the window occluded,
  minimized or restored; "drawing all the time" every 10 s while it draws more than 30 frames a second, with what
  keeps it busy (`App::BusyReasons`), whether a controller is held and the events' types.

## 3.11.0

- Settings > Video > **Frame generation** (where the game marks its views, `game::kHasDlaa`): Off or AMD FSR 3.1
  (experimental), NativeXRenderer's D123 (`native_x_renderer_frame_generation`). Locked to Off without AMD FSR 3.1 or
  NVIDIA DLSS in Upscaling; in a game with a frame rate row, below 120 FPS (the rate chosen there is the one shown and
  the game runs at half: the game's `src/options.cpp`, GH Metallica's first); in a game at its own rate, on a monitor
  under 120 Hz. On: the frame rate row says the rate is the one shown, V-Sync is locked on and written so
  (`native_x_renderer_present_mode = "vsync"`, `vblank_follows_display`), and the value says "the game at 80 FPS, 160
  on screen". Written as it applies (off when the upscaler or the rate does not allow it). Texts in five languages.
  Checked on copies of GH Metallica's options (DLSS at 160: on, V-Sync written on over "immediate"; DLSS at 60 and SMAA
  at 160: off).

## 3.10.0

The launcher's own log and a watchdog (`launcher_log.{h,cpp}`). Gustavo, 2026-11-04: back from the game, the launcher
showed but did nothing until he closed it from the taskbar, and nothing said why (the launcher had no log; Windows
logged no hang).

- `logs/launcher_NNN.log` next to the executable (in `Documents\<game::kDocumentsFolder>\logs` where that folder cannot
  be written), one per run, the newest 20 kept with their dumps: the launcher's and the game's versions, Windows', the
  window, the Direct3D adapter, the controllers in SDL's order (ImGui navigates with the first), each one connected or
  disconnected, the pages opened, each run of the game (started, its window shown, exited with its code, the launcher
  back in front or not, with or without the keyboard focus), the first input after the game, a controller that still
  reads a button as held 3 s after the game (the controller does not navigate until all are up), Present's and
  ResizeBuffers' errors (a lost device), the game run to write the achievements list, the errors shown, why it closed
  and each step of closing (the background reads still running named).
- The watchdog: a thread asks Windows every half second whether the window answers (`IsHungAppWindow`, Windows' "Not
  responding"); 10 s without an answer writes `launcher_NNN_hang.dmp` (a minidump: every thread's stack), and the log
  says when it answers again. A closing that takes more than 10 s writes `launcher_NNN_closing.dmp`, a crash
  `launcher_NNN_crash.dmp`. Checked with a test program (a hidden window that stops taking its messages for 17 s, a
  closing stuck 12 s, a null write: the three dumps open in cdb at the stuck or faulting line) and the launcher on a
  hidden desktop (its log from the start to "closed").

## 3.9.1

- Settings > Video > Internal resolution with AMD FSR 3.1 or NVIDIA DLSS: the size the 3D scene is drawn at (the
  quality's: 2293 × 960 for Quality at 3440 × 1440), "The 3D scene only", where 3.9 showed the screen's (the HUD's and
  the effects', which the upscaler rebuilds the scene at). Gustavo read 3.9's as the quality not lowering anything; the
  game's log says it does ("DLSS Quality: 2293x960 to 3440x1440"). DLSS's Balanced size is NGX's (58 % per axis:
  1995 × 835 at 3440 × 1440, as the game logs it), not AMD's 1 / 1.7.

## 3.9.0

The upscaling rows as other PC games name them (Gustavo, 2026-11-03: 3.8's Upscaling only did something when the
internal resolution was lowered, and its quality was a number of lines). The options file's keys are the same; a game
needs nothing new.

- Settings > Video, with NativeXRenderer: "Upscaling": Off, AMD FSR 1, NVIDIA NIS, and where the game marks its views
  (`game::kHasDlaa`) AMD FSR 3.1 and NVIDIA DLSS. Then "Quality": FSR 1 and NIS Ultra Quality, Quality, Balanced,
  Performance, Ultra Performance (77, 67, 59, 50, 33 % of the screen's lines, those above the game's 720; written as
  `native_x_renderer_resolution_scale`); FSR 3.1 and DLSS Native (Native AA, DLAA) and their four modes, marked
  experimental (`native_x_renderer_antialiasing`: `fsr_native`, `dlaa`, `fsr_quality`, `dlss_quality`...). Each value
  says the size the image is drawn at. The internal resolution is chosen only without upscaling; with it the row shows
  the one the quality sets. The quality is kept as `launcher_upscaling_quality`.
- "Sharpness": Off, Low, Medium, High, with each filter's own values (FSR 1's RCAS 1, 0.2 and 0 stops; NIS 0.25, 0.5,
  0.8; AMD CAS 0.3, 0.6, 1); Medium is AMD's and NVIDIA's recommendation. FSR 1 and NIS start at Medium when chosen
  with it off, and leave it off again.
- Settings > Graphics > Antialiasing: SMAA, FXAA, None; DLAA and FSR 3.1 moved to Upscaling (DLSS / FSR 3.1 at Native),
  and the row is locked to them while one is chosen. The Play page names the upscaler (FSR 1, NIS, FSR 3.1, DLSS or
  DLAA) beside the antialiasing; 3.8 read past its names with FSR 3.1.

## 3.8.0

Needs NativeXRenderer's D115 and D116 (its `ghmetal` branch until merged): a game on an older NativeXRenderer ignores
the two new keys and takes `fsr_native` for SMAA.

- Settings > Display, with NativeXRenderer: an "Upscaling" row, how the image is scaled to the screen when the
  internal resolution is lower than the screen's: Bilinear (as before), AMD FSR 1 or NVIDIA NIS
  (`native_x_renderer_scaling`: bilinear, fsr, nis). Then a "Sharpness" row: Default, Off, Low, Medium, High
  (`native_x_renderer_sharpness`: -1, 0, 0.3, 0.6, 1). Default is AMD's and NVIDIA's recommendation for FSR 1 and
  NIS, and none with the bilinear stretch; with it, AMD CAS sharpens the image. Sharpening works at the screen's own
  resolution too. The category's Reset puts both back (Bilinear, Default).
- Settings > Graphics > Antialiasing, where the game marks its views (`game::kHasDlaa`): "FSR 3.1" after DLAA
  (`native_x_renderer_antialiasing = "fsr_native"`, AMD FSR 3.1's native AA, NativeXRenderer's D116, on any GPU); the
  hint says so. The options file's FSR 3.1 modes (`fsr_quality` and the others) are kept as the DLSS ones, marked
  experimental.

## 3.7.0

Needs the SDK's recomp changes 67 and 68 (`ghmetal` branch until merged): a game on an older SDK ignores
`debug_hotkeys` and never fires a bare modifier key.

- Controls > Keyboard: Shift, Ctrl or Alt pressed and released alone is assigned, its side's key ("LAlt", shown as
  "Left Alt"), where it used to say that a modifier only works with another key. The SDK then takes it for that
  control's key and not a modifier (Alt held for Star Power leaves the frets as they are). When another control needs
  that modifier held ("Shift+Left"), a notice says those combinations now work with the other side's key.
- Settings > Game: a "Debug keys" row, off by default: the SDK's `debug_hotkeys`, F3 the debug overlay, F4 the SDK's
  settings window and ` the console. Off, those keys do nothing in the game, so they can be played. A key on one of
  the keyboard's controls only presses it either way (the SDK, change 68): on, the row names those keys and their
  controls; assigning one of them (or F7, the achievements) on the Keyboard page shows a notice. The SDK's default
  is on: a game that moves to this version has them off until the player turns them on.

## 3.6.0

Every game's `game.h` defines `game::kHasDlaa` (`true` only where the game's hooks mark its views for NativeXRenderer,
`native_x_renderer::BeginView` and `EndViewGeometry`, as POTCAWE's). `game::kHasGamePage` is no longer read (a game's
`game.h` may drop it and its `HasToggles`).

- Settings > Game: a "Launcher language" row, the first: Automatic (the Windows display language, as before, named
  in the value) or English, Deutsch, Español, Français, Italiano. The texts change at once, the game's own texts
  (`game.cpp`) and the achievements' names with them; the choice is `launcher_language` in the options file (0 =
  automatic) and the launcher starts in it. The Game category is now in every game's Settings, and the game's
  language row is titled "Game language" next to it. Its Reset also puts back the Low latency audio row (left out in
  3.5.0).
- Settings > Graphics > Antialiasing offers DLAA only in a game with `game::kHasDlaa`; elsewhere SMAA, FXAA and None,
  with a hint of its own. In a game that does not mark its views DLAA turned SMAA and FXAA off and DLSS never ran: no
  antialiasing (Guitar Hero: Warriors of Rock). An options file with `dlaa` there reads as SMAA.

## 3.5.0

- Settings > Graphics > V-Sync on also writes the SDK's `vblank_follows_display` (recomp change 61): the game's
  vertical blank follows the monitor's, so the game never runs ahead of the display; NativeXRenderer's "vsync" is FIFO
  (D107). Measured in GH Metallica at 160 FPS on a 160 Hz display: 4 ms from present to display, no frame dropped,
  where NVIDIA's MAILBOX queued three frames (16-19 ms). Its help text says the game runs in step with the screen.
- Settings > Game: a "Low latency audio" row (Windows), off by default: the SDK's `audio_wasapi_exclusive` (recomp
  change 64), the output device in exclusive mode, past Windows' audio engine; other programs cannot play on it
  meanwhile, and the shared output is used when it refuses. In GH Metallica on a Focusrite Scarlett 2i2: 77 ms from
  the game's frame to the driver playing it, 20 ms with it.

## 3.4.0

Every game's `game.h` defines `game::kHasWhammy` (`false` where the game has no whammy bar).

- Controls: a controller's assignments are its own, kept by its serial number or GUID (the SDK's
  `gamepad_device_remap`, recomp change 59), no longer shared by every controller of its type: an Xbox 360 guitar's
  and an Xbox 360 controller's were one layout. A layout saved per type (`gamepad_remap`) is still each controller's
  start. Only the controller being edited is listened to.
- Controls: a trigger is taken as the button pressed only if it was released when the wait began and is then pressed
  past 90 %. An Xbox 360 Guitar Hero guitar's accelerometer is on its triggers, at half when still: the slightest
  movement assigned a trigger before the fret was pressed.
- Controls, for a game with `game::kHasWhammy`: a Whammy bar row on each guitar and each controller playing guitar,
  with how far the game reads the bar now and a calibration: push the bar all the way and hold it, then let it go.
  The axis that moved, its value all the way and at rest go to the SDK's `gamepad_whammy_calibration` (recomp change
  60); X (or Delete) clears it, as Reset does. A help tip says when to use it.
- Settings > Graphics: a V-Sync row, on by default: no tearing (NativeXRenderer's `native_x_renderer_present_mode`
  `"vsync"`, D106: a game moves its NativeXRenderer with this version; the SDK's presenters without their tearing
  modes), or off: each frame as soon as it is ready (`"immediate"`, for G-Sync and FreeSync displays).

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
