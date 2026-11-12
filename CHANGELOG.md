# Changelog

The launcher's versions (`launcher_version.h`). Each game shows its own version beside it.

## 3.20.0

Drums on the keyboard and the controllers (Gustavo, 2026-11-11). Needs the SDK's change 87 for four lanes.

- The Instrument row has three parts: the game's instrument (the guitar), Drums (`game::kHasDrums`) and Vocals. A
  controller or the keyboard playing drums is XInput's drum kit in `gamepad_device_subtypes` (`8`).
- A Drum kit row under it, and on a real drum kit: 5 lanes (Guitar Hero) or 4 lanes (Rock Band, `8:1`: the kit's
  capability flag that Guitar Hero: World Tour's engine takes as a Rock Band kit, playing its own four-lane highway;
  the SDK's change 87). A real kit is named in the list only for four lanes.
- Its rows show the drums' functions (`game::GameDrumControlsFor`, five or four lanes): the pads, the kick pedal,
  Star Power and the menus' controls; no strum switch, whammy bar or tilt.
- The keyboard's drum keys are kept apart from the guitar's (`Options::drum_binds`, `launcher_drums_keybind_*`): while
  it plays drums they are written as `keybind_*` and the guitar's as `launcher_guitar_keybind_*`, and its frets'
  strum is off (`mnk_tap_target`). Their defaults are the game's (`game::kDrumKeyboardDefaults` and
  `...FourLanes`); changing the kit while they are the defaults takes the other kit's. The keyboard's Reset resets the
  keys of what it plays.
- `gamepad_device_subtypes` is written when the game has drums or microphones; the parsing moved to settings.cpp
  (`PartOf`, `FourLanes`, `WithPart`). A selector row (`App::SelectorRow`, 224 dp chip) serves the part and the kit.
- Texts: `part_drums`, `drum_kit`, `drum_kit_five`, `drum_kit_four`, `help_tip_drum_kit`; `help_tip_part` names the
  drums (five languages).

## 3.19.0

Settings > Audio and the GPU (Gustavo, 2026-11-11). Needs the SDK's changes 85 and 86 (on `ghmetal`): with an older
SDK the game ignores the new rows' settings.

- Settings has a fourth category, Audio (`Page::kAudio`, after Game, Video, Graphics): the output device (Windows
  default, or one of SDL's playback devices by name; a chosen one not connected stays chosen, marked "not
  connected"; asked again every two seconds), the channels (Automatic, Stereo, Mono), the volume (0-100 % in tens),
  mute in the background, and low latency audio, moved here from Game. Its Reset (X) resets them. Options file:
  `audio_output_device`, `audio_channels`, `audio_volume`, `audio_mute_unfocused` (the SDK's change 85).
- Settings > Graphics: the graphics card (`gpu_adapter`, the SDK's change 86, and NativeXRenderer's D141): Automatic,
  or one of DXGI's hardware adapters by its PCI IDs (`EnumerateGpus`); for every renderer. Graphics' Reset sets it
  back to Automatic.
- Texts: `nav_audio`, `settings_intro_audio`, the audio rows' and the GPU row's (five languages).

## 3.18.0

Gustavo's redesign ("Camerino", his 2026-11-11 mockup, `local_docs/launcher_design` in GHMetalRecomp): a calmer
launcher, the accent kept for the main action, one focus style, flat rows instead of nested cards, long help on demand.

- One focus style for every item (`ui::FocusRing`, `ui::FocusRow`): a white outline 2 dp wide 3 dp outside the item, or
  on a row's own edge over a faint fill; drawn once a frame over every window (`ui::DrawFocusFrame`), sliding 120 ms
  from the item that had the focus. A page fades in over 160 ms (`ui::StartFade`, `ui::DrawFade`). Neither moves
  while Windows' animation effects are off (`ui::MotionEnabled`); `ui::Animating` keeps the launcher drawing meanwhile.
- The shell: the art in full only on Play; under the other pages the window's color and a faint corner of the art
  (`App::BackgroundCorner`, `ui::DrawArtPart`; a game's `kArtCorner` and `kArtCornerRise`, `game_types.h`'s defaults).
  The header's brand on two lines ("METALLICA · LAUNCHER"), the tabs in Oswald 500 without capitals between small LB
  and RB pills (none on the profile's pages), Donate an outline with its heart, the avatar 40 dp with an accent dot
  under it on the profile's pages. The footer: four hints at most, never the tabs' bumpers nor Donate.
- The controller's glyphs on a dark disc, each letter or symbol in its color; the bumpers and View on the chips' grey.
  Ghost buttons, links and the switch in the mockup's quieter style (`ui::DrawToggle`, a mixed state); chips beside a
  value (`ui::DrawChip`); a slimmer scroll bar.
- Play: the game's name (a game's `kArtTitleSize`, GH Metallica's 120 dp), its line, Play and Quit, and a strip of the
  state in place of the three cards: the last session, the achievements, the screen and frame rate, the updates (each
  of the last three opens its tab). Y no longer opens Donate: the header's button does.
- Settings: Game, Video, Graphics in that order (the tab opens on Game), as text with a dot on the current one; up and
  down on them change it at once. The options as flat rows, their value as text, on/off as a switch; the row in focus
  grows to show its line of help, the arrows and its place among the values. Left and right change the value, A (or a
  click) the next one, B goes back to the categories, X resets the category, Y opens the row's whole help. No help
  column, no "Current value" box.
- "More info" (`App::OpenInfo`): a panel at the right over a flat veil, with its own footer (B closes it).
- Controls (phase 2): two columns. The devices as rows (icon, name, a dot and the status), an accent dot by the chosen
  one; the mapping's title with Reset at its right (above the rows in the controller's path) and "Experimental" as a
  chip for the keyboard and the microphone. Flat rows 44 dp high (`MappingRow` with the focus's look, `RowFocused`),
  chips in the field's grey (`MappingChip`, `MappingChipRect`; the keyboard's two 128 dp), switches as whole rows
  (`SwitchRow`), the part a selector with a chevron, the whammy bar's and tilt's live meters 120 x 6. The microphone's
  rows like Settings' (56 dp, the value as text, left and right step it, its arrows when focused) and its level over
  three faint zones. No help column: Y opens "Tips" (the device's kind, what to do, its tips) in the panel; the footer
  A, X, Y, B.
- DLC (phase 2, the folder mode): two columns without cards. The folder's column: its heading, two sentences
  (`dlc_folder_short`; the long text in "More info"), the folder as a line with its count (the path cut in its
  middle), the drop hint, the copies' notice, Change folder (an outline once a folder is chosen, the accent before)
  and the Open link. The list: groups as rows (A folds, X or the switch sets all of it, the switch mixed while some are
  on) and packages as rows (A or a click toggles; "Not for this game" a quiet note; an extracted one's Remove by X).
  Right from the folder's column enters the list. The footer's hints follow the focus.
- Achievements (phase 3): the totals beside the title (unlocked, gamerscore, its bar and percentage), the filters as
  text tabs with their counts (X steps them), a grid of four columns of cards 100 dp high (the chosen one raised,
  a locked one faint with a lock by its points, "Secret" a chip), and a strip under the grid with the one in focus:
  its name, description, when it was unlocked and its points. No detail column: A reveals a secret one in focus (or
  hides it again), the footer saying so only then.
- Updates (phase 3): two columns without cards. The state as an icon and a heading (the version, up to date,
  checking, a new version, a failed check) with its line, the installed version (and the launcher's) and the last
  check as plain values, the main button as Play's (check, check again, update now; grey while it checks or updates)
  with the update's progress under it, checking at start as a switch row, the history link; the release notes at the
  right. The footer's A says what the button does.
- The profile (phase 4), no cards anywhere: "Who's playing" as tiles (the current one marked by a dot), See my profile
  the main action, Edit (X) beside it, Y creates one. The profile's page: the avatar, name, motto and level at the left,
  the sections as text tabs between LB and RB and Edit at the right; the summary a strip of the totals and three
  columns (the week's bars, neutral with today in the accent; the recent achievements and the showcase as rows,
  without the games' colors); My games a row each, its columns lined up (one bar and one percentage, the gamerscore's,
  as the Achievements tab), Play the accent only on this launcher's game. A game's achievements: back (B), its name,
  totals and bar, the filters as text tabs, two columns of flat rows (`AchievementCard`) with the showcase's star.
  Editing and creating: the preview without a card, the form with the shared pickers (`AvatarPicker` with an accent
  badge, `ColorPicker` with a check, `FieldLabel`); the last profile's Delete disabled with its reason shown (not on
  hover only). The first profile's dialog over a flat veil with its own footer; text fields show the focus's outline.
  LB and RB on the profile's page move the focus into the new section.
- The first run (phase 5): each step a column over the art without a card, the steps in the header (circles 28 dp,
  the current one white with the accent under it, a finished one with a check). Welcome: the title, what it needs as
  rows, Start and Quit. The ISO: a dashed zone to drop it on with Browse (the accent until the file is valid, then an
  outline on X), the file's line (checking, then valid with its size), a wrong file's block (the file and the game
  found, "Choose another file" the accent), Extract and install. Extracting: the percentage, the time left, the bar,
  the file being written and the check; Cancel in the error's color. Ready: a check, the title, deleting the ISO as a
  note, Play and Review settings (X). The footer's A says what the step's main button does. "Compiling shaders" the
  same column, Skip (B) an outline.
- The extracting DLC page (`kInstall`, phase 5) in the folder page's style: the column with the dashed zone, Choose
  folder and the Open link; the installed packages as rows with their switch and Remove (X); "Not for this game" a
  quiet note. Removing a DLC and a profile's conflict over the flat veil, each with its own footer.
- Checked in German (phase 5): the mapping chips 156 dp wide ("Steuerkreuz hoch"), the photo's caption on two lines
  where it needs them, the Settings title kept clear of its intro. A link shows the focus a little away from its words.
- Texts: `more_info`, `close`, `strip_achievements`, `strip_completed`, `strip_streak`, `tips`, `dlc_folder_short`,
  `dlc_drop_hint`, `dlc_not_for_game`, `dlc_hint_fold`, `dlc_hint_all_on`, `dlc_hint_all_off`, `dlc_hint_toggle`;
  `recommended`, `format_your_screen` and `screen_effects` in sentence case; `hint_choose_folder` gone (the column's A
  says Select).

## 3.17.0

- The DLC page's mode per game: `kDlcMode` in a game's `game.h` (`game_types.h`'s `DlcMode`). `kFolder` is 3.16's
  DLC folder, read by the game where it is (it needs the SDK's change 80); `kInstall`, the default, is 3.15's page
  back: the packages of a folder the player chooses extracted by the game (`--install_content`), each with its switch
  (`page_dlc_install.cpp`; `DlcPackage`, `FindDlcPackages`, `DlcInstall` and their texts again, in a block of their
  own). So a game whose DLC has not been tried in place keeps extracting it; the Guitar Heroes set `kFolder`
  (Gustavo, 2026-11-10). `DlcPage` and `PollDlc` choose by it, and a folder dropped on the page goes to either.
- With main's 3.14.3 (`kOffersTemporalUpscalers`), merged into this branch: one launcher for every game again.

## 3.16.0

- The DLC page's folder is the player's DLC folder: their Xbox 360 packages (LIVE, PIRS, CON), as many games' as they
  like, read by the game where they are (the SDK's `content_packages.txt`, its change 80), so nothing is copied and
  every game's launcher can use the same folder. Choose it (or drop it on the window); the launcher reads each
  package's header and file table (`ReadPackageInfo`: the names at its root, along the hash tables' chain), checks it
  against `kDlcRule` by those names (`FitsByNames`: the rule's files, the package's own manifest, the disc's list),
  and writes the game's list with the packages on: those the game reads, unless the player turned them off, and
  those it does not read that the player turned on (`dlc_library.txt`: the folder and the choices, by the path in the
  folder). Read again when the launcher opens, so new packages come in before Play.
- The list shows the folder's packages and those extracted by earlier launchers, each with its switch; Remove only on
  the extracted (the folder's are the player's files, never written or deleted).
- An extracted copy of a package of the folder (`FindCopies`: a folder named as the package's file, as earlier
  launchers named it, or one whose header has the package's name, the only one of that name on both sides, holding
  95-100 % of its content size) shows as one row, the copy's, with a note; the package stays out of the game's list
  while the copy is there (no song twice). "Remove copies" above the list (with their count and size) asks "Are you
  sure you want to delete these files?", saying which copies and where, and that the originals are not touched; then
  deletes the copies (`RemoveDlcCopies`) and the folder's packages take their place.
- The list is grouped by the game each package was made for (the header's title ID; the extracted copies a group of
  their own): a header that folds the group open or shut, the game's name as its headers spell it most, "N of M on",
  and a switch for the whole group; this game's group first, then the copies, then the games it reads, then the rest;
  folded from the start past 12 packages. The game's "where" text is a line under the title, and the copies' notice
  with Remove copies sits in the folder's card under the folder (a row: the icon, the folder and what it holds), so the
  list keeps the card's height. Installing by extraction is gone,
  with its texts (`dlc_install_*`, `dlc_installing_short`, `dlc_installed_*`, `dlc_failed`, `dlc_listed_hint`,
  `dlc_all_installed`, `dlc_new_tag`, `dlc_turned_off_*`); new: `dlc_library_*`, `dlc_list_title`,
  `dlc_change_folder`, `dlc_in_folder`.

## 3.15.0

- The DLC page turns packages on and off: a switch on each installed package. Off, the package's folder waits in
  `<title ID>\Disabled DLC`, beside the content folder, where the game does not look; on, it goes back.
- A package the game does not read is turned off when it is installed, and for one installed before when the launcher
  opens (not only when the DLC page is shown: before Play), with a note on what the game would do with it on; the
  player can turn it on, and it stays on ("turned on by the player.txt" in that folder). Which packages a game reads is `kDlcRule` in its `game.h` (`game_types.h`'s `DlcRule`): files a package
  must hold (GH Metallica: Death Magnetic's `adl1.pak.xen`) and Neversoft's manifests (Guitar Hero 5, GH Warriors of
  Rock: a package known by the QBKey of its display name, named in a `bmanifest_` / `cmanifest_` file of its own or of
  another package, or in the list on the game's disc). Optional: without it nothing is checked and every package stays
  on, as before. Found from a player's Guitar Hero 5, where a package of another game made the game report a damaged
  package and turn off every DLC.
## 3.14.3

- AMD FSR 3.1 and NVIDIA DLSS per game: `kOffersTemporalUpscalers` in a game's `game.h`, false to leave them out of
  Video > Upscaling (Off, FSR 1 and NIS) in a game that has them (`kHasDlaa`). They stay an option of the options file
  (`native_x_renderer_antialiasing = "dlaa"`, `"fsr_native"`, `"dlss_quality"`...): the row shows the one chosen there
  while it is in use, the launcher writes it back as it is, and it goes once the player chooses another; the
  Antialiasing row's hint no longer points to them. Pirates of the Caribbean: At World's End is the first. Optional,
  as `kArtTitleSize`: no other game changes.

## 3.14.2

- The Game page's "Low latency audio" row per game: `kHasLowLatencyAudio` in a game's `game.h`, false to hide it where
  the sound's delay does not matter (Pirates of the Caribbean: At World's End: no rhythm to play to, no audio
  calibration). Such a game reads and writes `audio_wasapi_exclusive` as off, whatever an older launcher left in its
  options file. Optional, as `kArtTitleSize`: without it the row is there as before, so no other game changes.

## 3.14.1

- The Play page's title size per game: `kArtTitleSize` in a game's `game.h`, in dp. Optional: without it the title is
  96 dp as before (`game_types.h`'s `defaults`, which a game's own definition hides), so no other game changes. Band
  Hero's 84: Oswald draws "BAND HERO" 464 dp wide at 96, past the start of its art's bass at 1280x760, where its mockup's
  narrower font ended beside it.

## 3.14.0

- "Compiling shaders" before Play, for a game with NativeXRenderer's pipeline list (`game::kHasPipelineList`, its D129;
  `pipelines.h`, `page_pipelines.cpp`). The game ships `<TITLEID>.pipelines.list` beside its executable, the pipelines its
  developers' play made, named by hash; Play first runs the game's `--native_x_renderer_prepare_pipelines`, which rebuilds
  the shaders from the player's copy of the game and compiles them and the player's own stored pipelines for this GPU and
  driver, with a progress bar and Skip (B, Escape or a click: the game then compiles as it draws, as before). It runs
  again only when the game's executable, the SDK's libraries, the list or the graphics driver changed (the stamp the game
  leaves, `<user data>/cache/native_x_renderer/<TITLEID>.prepared`; the driver read through the Vulkan loader, without its
  implicit layers), and only when the game draws with NativeXRenderer. GH Metallica on an RTX 4070 Ti: 0.7 s.
- Every game's `game.h` needs `kHasPipelineList` (false for a game without a list).

## 3.13.2

- `launcher_any_frame_rate = true` in a game's options file offers every frame rate the game has, also those above the
  monitor's refresh rate, and keeps the one chosen (`FrameRates(monitor, any)`). Before, a rate above the monitor's was
  never offered, and `FitFrameRate()` reset one written in the file to 60 when the launcher opened (GH5 at 240 on a
  165 Hz monitor, 2026-11-07). Off by default, and written back only when on. Such a rate needs V-Sync off: with V-Sync
  on GH5 at 240 ran at 156 FPS on that monitor, with 20 ms frames.

## 3.13.1

- A version with a suffix ("0.3.1-beta.1") comes before the same numbers without one, as in Semantic Versioning: the
  suffix was ignored, so a player on a beta was never offered that version's final release. Pre-releases themselves are
  still not offered by the Updates page.

## 3.13.0

- A Tilt row beside the Whammy bar's, on guitars and controllers playing guitar: how far the game reads it, and Assign:
  tilt the neck up as for Star Power and hold it, then go back to your playing position. Any axis of the controller, or
  its accelerometer (where a PS3 guitar's tilt is), becomes the right stick's Y (the SDK's `gamepad_tilt_calibration`,
  recomp change 72). A Wii guitar's reported through a program that shows it as an Xbox 360 controller, whose tilt axis
  ends with the neck straight up: the game takes only the axis's very end. X clears it, Reset too.

## 3.12.0

A CRKD guitar's player, 2026-11-05: no whammy bar, or yellow and blue swapped, and the launcher deaf to the guitar on
its P3 setting. Checked with SDL's virtual controllers, not yet with that guitar.
- Every controller is kept open (`controllers.cpp`). SDL reports only an open controller's buttons and axes, and ImGui's
  backend opens just the first, so the Controls page could assign nothing on any other and its whammy bar read 0 (the
  calibration always ended in "no movement"). SDL lists HIDAPI controllers first: with a DualSense connected, a guitar
  was never the first.
- The last controller that pressed a button navigates (at first SDL's first, as before): one at a time, not merged, as a
  guitar's whammy bar and tilt rest over. Before, only the first did, so a guitar behind another controller looked dead.
- A trigger is no longer "a button held" (`ui::AnyGamepadButtonDown`). A guitar's tilt rests on one past ImGui's
  threshold (an Xbox 360 guitar's right trigger at half), so after a button was assigned on it the launcher waited for
  all to be up forever: no navigation and no shortcuts on any controller until it was closed.
- A PS3 Guitar Hero guitar (12BA:0100, as a CRKD guitar on P3 should be), as the SDK's `guitar_ps3_protocol` (recomp change 71):
  yellow and blue put right in SDL's mapping, a mapping for it where SDL's PS3 driver did not take it (`game::kHasWhammy`
  games), the Whammy bar row's reading from the middle of the axis.
- The Whammy bar row assigns: "Assign" and "Assign again" for "Calibrate" and "Calibrate again", in five languages, and
  the axis it is on ("Whammy bar · assigned (RT)"). The steps are the same.
- The log: every joystick SDL sees with its gamepad type, kind, USB IDs and GUID, the ones without a gamepad mapping
  too; the whammy bar's assignment, or where the axes were when it found no movement.

## 3.11.4

Gustavo, 2026-11-04 (`launcher_007.log`): back from the game, every Present took exactly 3 s for half a minute, until
another window came in front; with his guitar the only controller, the log said "a controller held" all the time, and
the glyphs turned to the Xbox controller's at every mouse click.
- The swap chain is made again when the launcher comes back from the game, and after a Present of half a second or more
  (at most once every 5 s; `ui::gfx::RecreateSwapChain`, logged).
- A controller "held" (keeps the launcher drawing, for the menus' repeat) is one of the controls that navigate: the
  D-pad, the face buttons, LB, RB, Start, Back and the left stick. A guitar's whammy bar rests on the right stick, fully
  over, and its tilt on a trigger. A stick or trigger event counts as input when it crosses the dead zone.
- The glyphs follow a controller when a button is pressed or a stick or trigger crosses its threshold, not while it
  rests over it (the whammy bar's noise switched them back at once after the mouse).

## 3.11.3

- Settings > Graphics > V-Sync's help, in five languages: On is the one recommended, also with G-Sync or FreeSync (it
  never tears and, with the game's vertical blank following the display, adds none of V-Sync's usual delay: no queue);
  Off can tear without them and moves less evenly with them. 3.4-3.11.2 said Off was for G-Sync and FreeSync monitors:
  in GH Metallica, V-Sync off at 160 FPS on a 159.96 Hz G-Sync monitor tore (the game at the panel's ceiling), and at
  the display's exact rate it did not tear but its frames reached the screen as the renderer finished them, less even
  than with V-Sync (Gustavo, 2026-11-04).

## 3.11.2

- The Frame generation row hidden in every game (`kFrameGenerationRow = false`, settings.h; Gustavo, 2026-11-04: GH
  Metallica's note highway still shows artifacts with it, and the stage ghosts at camera cuts). The options file's
  `native_x_renderer_frame_generation` is written back as it was read, so a test by hand stays; the V-Sync row and the
  frame rate row's text are as without it.

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
