# Changelog

The launcher's versions (`launcher_version.h`). Each game shows its own version beside it.

## 3.39.0

The Mods tab's highway preview without the player's pictures (2026-11-27, GH Metallica: Gustavo wanted the preview, not
the gallery):
- `kHighwayPictures` (settings.h): the Highway section's pictures (Background's Folder, Image and Random among, Dim
  bright pictures, the gallery, the options file's picture keys and `launcher_highway_recent`) only where the game reads
  them, `kHighwayGallery.folder_cvar` not null. A game that leaves the gallery's first seven fields null keeps the
  preview, drawn with its own highway, and the shape and look rows; the caption starts with the shape.
- A preview part not in `kHighwayAssets.dictionary` is looked up in the pak's other .tex dictionaries (GH Metallica's
  string is in z_in_game.tex, its gems in basic_gems.tex).
- `HighwayAssets::gem_cap_scale` (1 by default): the gem's cap drawn narrower than the gem (GH Metallica's chrome cap
  inside its cup's ring, 0.6).
Nothing changes for a game with the pictures.

## 3.38.0

The Mods tab's **Gameplay** section (2026-11-26, GH Warriors of Rock's infinite front-end and sustain drop leniency):
`Page::kModsGameplay` after Highway, `game::ToggleGroup::kModsGameplay` for its switches and rows with several values,
shown where the game has a row in it (a game without them needs no change); its name and intro in the five languages
(`group_gameplay`, `mods_intro_gameplay`), Reset with it (`ResetModsGameplay`).

## 3.37.0

Two groups of a game's own switches in the Mods tab, and the art under the other pages darker (2026-11-26, GH Warriors
of Rock's note effects and venues; Gustavo: "el gráfico de fondo en otras pantallas ... más oscura").
- `game::ToggleGroup::kModsNoteEffects`: the Highway section's last group, **Note effects** (the flames and sparks on
  the strike line), after Look. `kModsVenues`: the Stage section's **Venues in Quickplay**, after its own switches
  (the venues the game may pick at random, a switch each). Each group's title is drawn only where the game has a
  switch in it, and Reset puts them back with their section. Titles in the five languages (`group_note_effects`,
  `group_venues`). A game without them needs no change.
- The art under the pages other than Play (3.35's `App::BackgroundFaint`) at 4 % instead of 9 %, in every game: in GH
  Warriors of Rock's bright orange art the mockup's 9 % stood out against the rows.

## 3.36.0

Mod packs: the players' mods as folders of files the game reads in place of its own (2026-11-24, GH Warriors of
Rock's mod packs).

- `game::kHasModPacks` (false in `game_types.h`; a game that reads the packs, the SDK's `game_data_overlays` set by the
  game from `enabled.txt` when it starts, sets it in its game.h): the Mods tab's last section, **Packs**
  (`Page::kModsPacks`, `mod_packs.h`, `page_mod_packs.cpp`). A pack is a folder of `Mods\` in the game's user data
  folder (`ModsFolder`, beside `Highways`; made when a first pack is added): its `mod.toml` (`format = 1`, `id`,
  `name`, `version`, `author`, `description`, `title_id`; optional `name_<language>` and `description_<language>`, the
  launcher's keys `en`, `de`, `fr`, `es`, `it`), and `files\` (whole files at the game data's paths), `patches\` (the
  same paths with `.delta`) or both. Without a Mods folder nothing is read and nothing logged.
- The section: the Mods folder's card (its path, how many packs and how many on; **Add mod…**, a file dialog for a
  pack's `.zip` or its `mod.toml`, and the folder in Explorer; a zip or a folder dropped on the window is added too),
  then a row for each pack: its name, version and author, its description as the second line, and its switch. The
  second line says the pack's state instead when there is one: to be prepared (and how much it writes), its
  preparation's progress (with a bar; A cancels it), made for other game files (the first file that is not the
  player's), made for another game, broken (no readable `mod.toml`, a newer format, no files, a patch that is none or
  whose result would be over 16 GiB, a folder whose name starts with `#`), "cannot be read right now" (a `mod.toml`
  another program holds: left as it was), or "replaces files that <pack> also replaces" when two packs on hold the same
  file. At the foot: mods never change the game's files, they apply when the game starts, and online they may not work
  with players who lack them. Y: what a pack is, how one is added and prepared, the load order. Its hints: Turn on or
  off, Cancel, More info. The rows wait while the folder is read again or a pack is added.
- A pack's **Delete** (its row, or X; the confirmation's card first): its folder renamed to `Mods\.removing <folder>` (refused while
  a file of it is open), out of `enabled.txt`, then deleted without following a link inside it; the list read again.
- Adding (`AddModPack`, on a thread, never throwing): the zip (the updater's reader: the whole archive in memory, no
  ZIP64) or the folder with its `mod.toml` at its top or one folder down, installed as `Mods\<id>\` through
  `Mods\.adding <id>\`. Every name of the zip is checked first (valid UTF-8, relative, a name Windows takes in each
  part: no device name, no trailing dot or space); the id too, and it cannot be `enabled.txt`. A folder of that name
  with another pack (or none) is left as it is; the same pack is replaced once the player says so (the version
  installed and the new one). A `.7z` or `.rar` gets a line saying a `.zip` or a folder is needed.
- Preparing (`ModPackPreparation`, on a thread, never throwing; `ApplyModPatch`): each patch applied to the player's
  own file, the base checked (size and SHA-256) before anything is written, the result streamed to `<file>.part<pid>`
  in `files\` and renamed once its SHA-256 is the patch's; cancelled between files and within one, leaving no file. It
  starts by deleting the `*.part<digits>` an earlier one left (never mounted), and never writes through a link (a
  `files` folder, a folder on the way or a result that is one, or a result outside the pack's folder). The free space
  is asked first; sums from the headers saturate. `prepared.txt` beside `files\` records what was made (the patch's
  size and time, the result's size and SHA-256) and the bases found to differ: a pack prepared is not read again at
  every start, an updated one is prepared again. A pack switched on with patches is prepared first and switched on when
  that ends; a patch refused leaves the results before it, and the pack off.
- `Mods\enabled.txt`: the packs on, a folder a line in the order of their names (a later one's file wins), written
  whole (a failed write puts the switch back and says so); the launcher's switches are read back from it. A pack
  listed that cannot be mounted as it is (to be prepared, made for other files or another game, broken, gone) is
  taken out of it when the folder is read (when the launcher opens and after each change), with a line saying so.
- `GameDataRoot` (install.h): the game files the game reads, a `--game_data_root` on the launcher's command line
  (which Play passes on) or `GameFilesFolder`; the patches' base. `GameFilesFolder` is unchanged.
- SHA-256 with Windows' CNG (bcrypt, already linked for the updater); no new dependency.
- Texts in the five languages: the section's name (Packs, Pakete, Packs, Paquetes, Pacchetti), its intro, the card,
  the rows' states, the replace question, the note and the help.

## 3.35.1

The first run's Quit works again (2026-11-25, Gustavo, GH Aerosmith's installer): since 3.22.0 Quit asks first
(`App::AskQuit`, a confirmation), but the install screens and "Compiling shaders" returned before the frame drew the
confirmation (`App::ConfirmWindow`), so the dialog never showed and the launcher waited for it, its footer showing the
dialog's keys; Donate's question before the browser did the same there. Both screens now draw it after themselves. No
game needs a change.

## 3.35.0

The art under the pages other than Play, as Gustavo's mockups draw it (2026-11-25, his GH Aerosmith mockup's "arte
atenuado en otras páginas", for every game, his word): the whole art, covering the window and cropped around
`kArtFocusX` / `kArtFocusY` as on Play, at 9 % over the window's color (`App::BackgroundFaint`, the mockups' "opacity:
.09"), no longer a corner of it at half its size (`App::BackgroundCorner`). `ui::DrawArt` gains an alpha (1 by default);
`ui::DrawArtPart` is gone. `game::kArtCorner` and `kArtCornerRise` are read no more, kept so that a game.h that sets
them (GH Metallica's) still builds: no game needs a change.

## 3.34.0

The Mods tab's highway preview reads Guitar Hero III's files (2026-11-24, Guitar Hero: Aerosmith's highway pictures):
- A pak that is one raw deflate stream (Guitar Hero III's engine), not CHNK chunks, is inflated whole, and its
  `.pab.xen` beside it is read after it (the pak's entries reach into it as if the two were one file).
- `game::HighwayAssets` gains `lane_gems` and `lane_collars` (five keys each, green to orange; nullptr by default):
  a game whose notes and strike line rings are a texture per lane draws them as they are, not GH5's grey layers
  tinted by the lane. The layers' keys may then be 0.
No game needs a change to build.

## 3.33.2

The keyboard's own functions for a game with `game::kGuitarControlsApart` (2026-11-24, Guitar Hero: Aerosmith): its
keyboard plays as a guitar in the menus (green selects, red goes back) and on the game's controller layout in a song
(the SDK's `keyboard_guitar_layout = "gh3"` for a keyboard the game is told is a gamepad, rexglue-sdk change 126), so
the Keyboard page names its keys with `game::GameKeyboardControlsFor` (new; game.cpp defines it with
`kGuitarControlsApart`), no longer with the controller's `GameControlsFor` (the frets on the triggers, the bumpers and
A). Only GH Aerosmith sets `kGuitarControlsApart`: no other game needs a change to build.

## 3.33.1

3.31.1 (GH5's, made on a branch beside 3.31.0) merged into 3.33: the Mods tab's highway preview reads the highway's
texture from another pak where a game keeps it apart from the gems (2026-11-18, GH5's port of GH Warriors of Rock's
mods): `game::HighwayAssets` gains `highway_pak` and `highway_dictionary` (nullptr / 0 by default: the highway from
`pak` and `dictionary`, as before). GH5 keeps its highway, `Highway_Judy_fm_01`, in
`data/compressed/ZONES/global.pak.xen`'s `judy_1_highway.tex` and the gems in `z_in_game.pak.xen`'s `basic_gems.tex`. No
game needs a change to build.

## 3.33.0

The keyboard plays on Guitar Hero III's controller layout too (2026-11-23, GH Warriors of Rock; the SDK's change 125,
`keyboard_guitar_layout`).

- With `game::kHasGh3PadLayout`, the Keyboard page has a **Play style** row while the keyboard plays guitar (not drums,
  not the vocals): Controller · GH III (the default) or Guitar · Xbox 360, the same values as a controller's.
  `Options::gh3_keyboard`, written as `keyboard_guitar_layout = "gh3"` or `"guitar"`; read as GH III unless the file
  says "guitar". Reset to defaults puts it back on GH III.
- On GH III the strum switch (`mnk_tap_target`) is hidden, as a controller's on its page; the help says what the
  layout does (`help_tip_gh3_keyboard`, five languages).

Nothing changes for a game without `kHasGh3PadLayout`. A game with it (GH5 too) gets the row when it moves to this
version; the keyboard plays on the layout only with an SDK that has `keyboard_guitar_layout` (rexglue-sdk change 125)
and the game's hooks that ask `PlaysGh3Layout` (an older SDK logs the line as an unknown setting and ignores it).

## 3.32.0

A switch under a row with several values (2026-11-19, GH Warriors of Rock's notes' speed with the highway perspective).

- `Toggle::needs` may name a row with several values (`kChoices`) of the same group: the switch is drawn right under
  that row, indented, and locked with its value while the row is at its default, its hint then "Only when <row> is not
  <default>" (`needs_other_value`, five languages). A switch that needs another switch is drawn as before.
- The Highway section's preview: `HighwayGallery::keep_speed_cvar` (null by default) names a switch that keeps the
  notes' speed at the strike line whatever the perspective; while it is on, the preview's notes take the time the game
  gives them (the time on the highway times the perspective's speed at the strike line: 0.78 at 75 %, 1.51 at 150 %).

Nothing changes for a game that does not use them.

## 3.31.0

GH Warriors of Rock's ambient occlusion, texture detail, speakers and frame rates above 240 (2026-11-18).

Settings > Graphics' Image group gains two rows, each behind a flag of `game_types.h` (off by default, so a game's
`game.h` turns it on), locked to their first value with Xenos:
- **Ambient occlusion** (`game::kAmbientOcclusion`): Original, Low, Medium, High, Ultra, written as NativeXRenderer's
  `native_x_renderer_ao` (`off`, `low`, `medium`, `high`, `ultra`). Original leaves the game's own.
- **Texture detail** (`game::kTextureDetail`): Normal, Sharper, Sharpest, written as
  `native_x_renderer_texture_lod_bias` (0, -0.5, -1): the main 3D view's textures half a mip level or a whole one
  sharper.
- **Extra texture detail** (under Upscaling, with DLSS or FSR 3.1; `game::kTextureDetail`): Off, On, written as
  `native_x_renderer_upscaler_lod_bias` (0, -1), the upscalers' vendors' one more level.

Settings > Audio's Channels row offers 5.1 where the game has `game::kSpeakerChoice` (the SDK's
`audio_speaker_config_from_output`, which the game sets): Automatic, 5.1, Stereo, Mono, written as `audio_channels`
auto, surround, stereo, mono (a game that keeps its timing whatever it is told, as GH Warriors of Rock's
`ghwor_dolby_timing`, says so in the row's help).

Frame rates above the monitor's: the Frame rate row offers every rate of `game::kFrameRates` whatever the monitor
(before, only with `launcher_any_frame_rate`), and a rate with `fps` 0 is "Unlimited". With V-Sync on a rate above the
monitor's runs at the monitor's (the game's side; Gustavo: "V-Sync manda"), as the row says; above 240 the row says it
is experimental.

Texts in the five languages; the Low / Medium / High labels are the sharpness row's.

## 3.30.1

The Highway section's folder and preview (2026-11-17, Gustavo's look at 3.30.0).

- **Folder** opens a list under its value, as the selectors' (A or a click; B, Esc or a click outside closes it):
  **Open in Explorer** (the folder made first when it does not exist yet) or **Change folder...** (the folder dialog).
  Its footer hint "Open or change"; its help says so.
- The preview:
  - the highway's picture no longer zigzags: each strip was a trapezoid drawn as two triangles, which bent the picture
    along their diagonal; each is now 16 narrow pieces;
  - the strike line's rings sit on the line, and the notes on their place: each sprite is placed by its visible part
    (the game's sprites leave room around theirs; a gem's three layers share theirs). `highway::Texture` gains
    `content_min` / `content_max`.

## 3.30.0

Mods as a tab of its own, Updates in Settings, and the note highway's gallery and preview (2026-11-17, Gustavo's
"mods-galeria" mockup; GH Warriors of Rock's highway pictures).

- The header's tabs: **Play · DLC · Achievements · Mods · Controls · Settings**. Mods leaves Settings: its sections
  **Highway, Stage, Sound, Tools** (`Page::kModsHighway` to `kModsTools`), each with its intro, its Reset and the note
  "Mods change the game itself. Some are experimental."; a section without rows in a game is left out (Tools always:
  the Debug keys). The Updates tab becomes Settings' last category, **Updates**: the version with the launcher's and
  the last check, a card with the state and its button (Update, Check again), Check when the launcher opens, Get beta
  versions, and Release notes (A or Y: the info sheet, 600 dp, with every version's notes and the full history link).
- Play: a new version is a button of its own at the strip's right ("Update 1.0.0-beta.3 →", Y too), which opens
  Settings > Updates; the strip no longer says "All up to date".
- `game::kHighwayPreview` (game.h): the Highway section's gallery and preview, with `game::kHighwayGallery` (the
  game's cvars: the pictures' folder, the picture, Random among, the favorites, the darkenings; the default folder's
  name in the game's user data folder; the choices the preview follows) and `game::kHighwayAssets` (the game's pak,
  its texture dictionary and the textures' keys, the highway's shape). Its rows:
  - Background: **Folder** (A: a folder dialog; the default `Highways` in the game's user data folder, written
    whole), **Image** (A: the gallery; its line counts the pictures and favorites), **Random among** (All,
    Favorites);
  - Look, after the game's choices: **Dim bright pictures** (Off, Automatic, 90-40 %).
- The preview (highway.cpp): the installed game's own textures, read from its pak (CHNK chunks of raw deflate), its
  `.tex` dictionary (Xenos fetch constants; mip 0 untiled, DXT1/3/5, 8888, 8_8, swizzled) on a thread, and drawn
  animated with the game's projection (rows linear in time, the length and perspective as the game changes them, the
  scale at the strike line): the highway with the picture (stretched; darkened, black with the dark style, the
  opacity's venue behind), its strings, sidebars, beat lines, the strike line's rings and notes with a sustain. No file
  of the game ships with the launcher. Without the game's files it says so.
- The gallery (over the preview, a popup): All (Original, Random, the folder's pictures by name), Favorites (Random
  among them, the favorites) and Recent (the last 12 used, `launcher_highway_recent`), on LB / RB or the chips; a tall
  preview of the tile in focus (Random: each picture of its pool for 3 s); A or a click uses it, X marks a favorite
  (a star), Y gives it its own darkening (as every picture's, Off, Automatic, 90-40 %: a half disc), B closes it
  unchanged. Thumbnails decoded on a thread (WIC, `LoadStretchedPicture`); a file that cannot be read has a red
  border and cannot be used; a TGA (which the game reads and Windows does not) shows no thumbnail. The line under
  them: the name, the file's size, favorite, the darkening. Five footer hints there.
- `game::Choice::needs`: a row under a switch, locked with its value while it is off (GH Warriors of Rock: the miss
  volume under "Lower the track on a miss").
- `ToggleGroup::kModsHighwayLook`: the Highway section's "Look" group.
- Texts (five languages): the Mods sections' intros and note, Updates' intro, the Highway rows, the preview, the
  gallery, its hints, Updates' card and Play's button.
- A game moving to 3.30.0 sets `kHighwayPreview = false` (and an empty `kHighwayGallery` / `kHighwayAssets`) unless
  it has the highway's pictures.

## 3.29.0

Rows of the game's own with several values, and the Mods category's Highway and Stage groups (2026-11-17, GH Warriors
of Rock's mods: the note highway's scale, length and style, the crowd).

- `game::Choice` (game_types.h) and `game::kChoices` (each game's game.h): a row of the game's own with several
  values. It is a cvar written as one of its `values` (a number, or with `quoted` a string in quotes). Its fields:
  - `default_value`, one of those values;
  - a `group` (`ToggleGroup`); in a group the rows come after its switches;
  - a `suffix` for the values shown as they are (" %").
  
  Its title, hint and help are `GameTexts::choices` and `choice_help`; its values' names are
  `GameTexts::choice_values` (null: the value and the suffix). The options file is read as text, or as the same
  number for an unquoted value; a value that is none of them reads as the default. The category's Reset sets the
  defaults back. `Options::choices` holds each row's index (`DefaultChoices()`).
- `game::ToggleGroup` gains `kModsHighway` and `kModsStage`: Mods' new first groups "Highway" and "Stage", before Sound
  and Tools. Reset mods covers them.
- Texts (five languages): `group_highway` (Highway, Notenbahn, Piste de notes, Highway, Pista delle note),
  `group_stage` (Stage, Bühne, Scène, Escenario, Palco).
- A game moving to 3.29.0 must add to its game.h:
  - `kChoices`, empty unless it has such rows: `inline constexpr std::array<Choice, 0> kChoices{};`;
  - in `GameTexts`: `choices`, `choice_help` and `choice_values`, as GH Warriors of Rock's.

## 3.28.0

The Settings tab reordered by intention (2026-11-17, Gustavo's "propuesta A v2"; the other tabs unchanged).

- Its categories: **Display, Graphics, Effects, Audio, Game, Mods, Online** (were Game, Video, Graphics, Audio,
  Online). The tab opens on Display. Effects only where the game has screen effects to turn off; Online only with
  `game::kHasOnline`.
  - **Display**: "Screen" (Monitor, Display mode, Resolution under it, Image format) and "Smoothness" (Frame rate,
    V-Sync, then the game's `kSmoothness` switches).
  - **Graphics**: "Renderer" (Renderer, Graphics API under it with Xenos, Graphics card); with NativeXRenderer
    "Upscaling" (Upscaling, then under it Quality, DLSS model, Internal resolution, Lower resolution for and the game's
    `kUpscaling` switches) and "Image" (Antialiasing, Sharpness, Anisotropic filtering, Shadow quality). With Xenos, a
    notice under Graphics card says what only NativeXRenderer has, and "Image" holds the internal resolution (the
    scale), the game's MSAA, Anisotropic filtering and Shadow quality. "Graphics API: ..." is no longer written at the
    intro's right.
  - **Effects**: the game's screen effects (`kScreenEffects`), a switch that needs another under it.
  - **Game**: "Language" (the launcher's, the game's), "Starting the game" (`kStart`), "Other" (`kGame`).
  - **Mods**: "Sound" (`kModsSound`) and "Tools" (`kModsTools`, then the SDK's Debug keys, moved from Game).
- The look: rows of 48 dp (were 60), the focused one 86 dp with 4 dp around it and 14 dp corners; its title 19 dp,
  its line of help 15 dp. Group titles in Oswald 18 dp with a line under them. A row that depends on the one above is
  indented under it (its text 46 dp in, its focus 38 dp in) with a line at its left. The categories in Oswald 26 dp,
  46 dp apart, from 112 dp; the title at 12 dp; the intro 17 dp at 28 dp; the Controls note and the foot's note 15 dp.
- `game::ToggleGroup` gains `kStart`, `kSmoothness`, `kUpscaling`, `kModsSound` and `kModsTools`; `kGame` is now the
  Game category's "Other" group, so a game that has not moved its switches keeps them in Game. A switch's
  `game::ToggleShown` (new, after `needs`) shows it always, while something enlarges the 3D scene (FSR 3.1, DLSS, their
  antialiasing alone, or `SceneFraction`), or while DLSS upscales (not DLAA); hidden, it keeps its value.
- `Page::kVideo` is `Page::kDisplay`; `Page::kEffects` and `Page::kMods` are new; `IsSettingsPage()`. Each category has
  its Reset: `ResetDisplay` (was `ResetVideo`, now with the frame rate and V-Sync), `ResetGraphics` (with the upscaling,
  the internal resolution and, new, Lower resolution for), `ResetEffects`, `ResetGame`, `ResetMods`.
- `SceneFraction()` and `SceneUpscaled()` in settings.h (`SaveOptions` writes `native_x_renderer_scene_scale` from the
  first).
- Not in it yet, from the proposal: a quality preset (Performance / Balanced / Quality / Custom, its values to define),
  and the Mods that do not exist (the highway's scale and style, the crowd, the whammy's sound, the track's volume on a
  miss).
- Texts (five languages):
  - renamed: `nav_video` -> `nav_display`, `settings_intro_video` -> `settings_intro_display`;
  - new: `nav_effects`, `nav_mods`, `settings_intro_effects`, `settings_intro_mods`, `group_screen`,
    `group_smoothness`, `group_image`, `group_language`, `group_start`, `group_game_other`, `group_sound`,
    `group_tools`, `xenos_notice`, `xenos_notice_dlss`, `xenos_notice_msaa`;
  - changed: `settings_intro_display`, `settings_intro_graphics`, `settings_intro_game`, `renderer_xenos_sub` ("Try it
    if the game looks wrong."), `antialiasing_hint` and `antialiasing_by_upscaler` (no "Video"),
    `frame_generation_help` and `frame_generation_needs_rate` (the frame rate is in Display), `done_text`;
  - removed: `screen_effects` (the Effects category has no title over its switches).
- A game moving to 3.28: give its switches their groups in game.h (the intro videos `kStart`, a debug menu
  `kModsTools`), and check that its own texts and README name the new places.

## 3.27.0

Resolutions below the game's own (2026-11-17), for a game that sets `game::kLowResolutions`; nothing changes for a
game without it. Needs NativeXRenderer's D174 (`native_x_renderer_scene_scale`) and D175
(`native_x_renderer_resolution_scale` below 1). With an older NativeXRenderer the game ignores the first key and draws
at least its own lines.

- Settings > Video > **Internal resolution** also offers 540, 480 and 360 lines, below the screen's.
- FSR 1's and NIS's **Quality** offers its five steps below the game's 720 lines too. On a 1080p screen: Ultra Quality
  831 lines, Quality 720, Balanced 635, Performance 540, Ultra Performance 360. Each step shows its size.
- **Lower resolution for**, a new row after Internal resolution, chooses what a lower internal resolution or FSR 1 /
  NIS lowers. It is shown with NativeXRenderer, without FSR 3.1 or DLSS, in a game whose views NativeXRenderer finds
  (`game::kHasDlaa`).
  - **The 3D scene only** (recommended). The scene is drawn smaller and enlarged before the HUD and the game's effects,
    which stay at the screen's resolution, as FSR 3.1 and DLSS do. Written as `native_x_renderer_scene_scale` (the
    scene's fraction of the screen's lines; FSR 1 / NIS: 1 / the step's ratio) with
    `native_x_renderer_resolution_scale = 0`. The internal resolution's row says "The 3D scene only" under it. Without
    FSR 1 or NIS the image is enlarged bilinearly, sharpened by AMD CAS when a sharpness is chosen.
  - **The whole image**, the lightest. Everything is drawn smaller, the HUD and the text too. Written as
    `native_x_renderer_resolution_scale` below 1, as before 3.27 above it.
  - Kept as `launcher_lower_resolution` (`scene` or `image`), with `launcher_internal_height`, the height chosen for
    the scene without FSR 1 or NIS (the internal resolution is the screen's then).
- `SceneLowerable()`, `LowersScene()`, `kLowestHeight` (360), `kLowestSpatialHeight` (180; NativeXRenderer's D175 takes
  a quarter of the game's lines) in settings.h. `NativeRenderSize` lets a chosen height go below the game's.
- Texts (five languages):
  - new: `lower_for`, `lower_for_hint`, `lower_for_help`, `lower_scene`, `lower_scene_sub`, `lower_image`,
    `lower_image_sub`, `scaling_help_low`, `quality_help_low`;
  - changed: `internal_scene_help` no longer names FSR 3.1 and DLSS.

## 3.26.0

DLSS's Transformer K model in every mode (2026-11-16), for a game that sets `game::kHasDlssModel` (nothing changes for
a game without it). Relies on NativeXRenderer's D156, the one 3.24.0 already needs: `native_x_renderer_dlss_model`
also takes a render preset's letter, A to Z, for every mode.

- Settings > Video > **DLSS model** has three choices: Transformer (recommended, as before), **Transformer K** and CNN.
  Transformer K is NVIDIA's first Transformer model (render preset K) in every mode, written as
  `native_x_renderer_dlss_model = "k"` and read back from `k` or `K`. Transformer, NVIDIA's own choice (preset 0), is K
  for DLAA, Quality and Balanced but the heavier M for Performance and L for Ultra Performance: K is lighter there and
  the same elsewhere, which helps older cards. Another letter or an unknown name still reads as Transformer, and
  Video's Reset still sets Transformer.
- The options file's comment above the key lists `k`.
- Texts: `dlss_model_transformer_k_sub` (five languages). `dlss_model_help` describes the new choice, and it and
  `dlss_model_transformer_sub` speak of NVIDIA's current models, one for each mode.

## 3.25.0

The volume up to 200 % (2026-11-15), for games that sound quiet. Needs the SDK's change 117 (`audio_volume` clamped to
0-200): with an older SDK the game takes anything above 100 as 100.

- Settings > Audio > **Volume** from 0 to 200 % in steps of 10 (21 values; it was 0 to 100). Written as
  `audio_volume`, read and written clamped to 0-200 (`settings.cpp`). The default stays 100 %.
- Above 100 % it is a gain above the game's own, with no limiter: the loudest moments can distort. The help says so
  (`audio_volume_help`, five languages).

## 3.24.0

DLSS's model for a game that sets `game::kHasDlssModel` (2026-11-12; false by default, nothing changes for a game that
leaves it out). Needs NativeXRenderer's D156 (`native_x_renderer_dlss_model`): with an older NativeXRenderer the game
ignores the key.

- Settings > Video > **DLSS model**, right after Quality while NVIDIA DLSS is the upscaler: Transformer (recommended:
  NVIDIA's own model for each mode, render preset 0) or CNN (the earlier one, preset F, which DLSS 310.9 takes),
  written as `native_x_renderer_dlss_model = "transformer"` or `"cnn"` whatever the upscaler (NativeXRenderer
  reads it with DLSS only). Video's Reset sets Transformer.
- Per game: `kHasDlssModel` in a game's `game.h`. Optional, as `kArtTitleSize`: false by default
  (`game_types.h`), so a game without it shows no row and its options file gains no key.
- Texts: `dlss_model`, `dlss_model_help`, `dlss_model_transformer_sub`, `dlss_model_cnn_sub` (five languages).

## 3.23.1

- A game without drums builds again (Kameo, the first since 3.19.1 to move): `game::kHasDrums`, the drum keys
  (`kDrumKeyboardDefaults`, `...FourLanes`) and `GameDrumControlsFor` have defaults in `game_types.h`'s
  `launcher::game::defaults` (no drums, empty lists), as the profile's other optional settings. A game.h that defines
  them, as every Guitar Hero's does, hides them: nothing changes there (GH Warriors of Rock's launcher built with it).

## 3.23.0

Gustavo's request of 2026-11-14 (a player's PDP Riffmaster for Xbox, seen by none of the games):

- **Xbox One guitars** (GIP instruments, neither XInput nor HID: the PDP Riffmaster and Jaguar, the Mad Catz
  Stratocaster, the CRKD / RedOctane Stage Tour guitar). SDL reads them through Microsoft GameInput's raw reports once
  the SDK builds it against GameInput's API 3 (the SDK's change 110, which this version needs for the rest; with an
  older SDK the launcher builds and behaves as 3.22.0). The Controls page reads their tilt as the game does, their
  GameInput report's byte (SDL gives it only as the right shoulder, and their pickup selector as the right stick's Y):
  the tilt's meter, and its calibration, which finds it as the source `gip`. Through `gameinput_instruments.cpp` of
  the SDK, compiled in here.
- **Microsoft GameInput missing** (`game::kHasWhammy` games): the Controls page lists "Xbox One guitars · GameInput
  missing · Install" last among the devices, when `GameInputRedist.msi` is beside the launcher (a release
  ships it). Pressed, it asks first, then runs the installer (`msiexec /passive`; Windows asks for an administrator's
  consent) and, when it ends, starts the gamepad subsystem again so that SDL finds GameInput; the row goes. A failure
  stays on the row ("did not install · Try again"). The log says at the start whether GameInput 3 answers.
- **GameInput's raw reports only with an Xbox One instrument connected** (the SDK's change 114): before each start of
  SDL's gamepads, `ChooseGameInputReports` counts the Xbox One guitars and drum kits GameInput sees
  (`CountRawInstruments`) and sets SDL_HINT_JOYSTICK_GAMEINPUT_RAW. With the reports on, SDL's GameInput driver also
  took Xbox 360 instruments: a wired X-plorer came as a gamepad, a wireless guitar twice. One connected later shows from
  the next start of the launcher.
- **GameInput's twins left out.** With GameInput's raw reports on, SDL also lists a plain controller a second time,
  through GameInput (its GUID's driver byte 'g'): the Controls page showed it twice. `Gamepads()` (controllers.h) is
  SDL's list less those that are not an instrument, everywhere the launcher lists the controllers; the log's
  controllers line marks them "GameInput's twin: left out". The SDK's change 110 does the same for the game.
- **Guitar Hero III's controller layout** (`game::kHasGh3PadLayout`, the SDK's change 112, `gamepad_guitar_layout`;
  Gustavo's request for GH Warriors of Rock, on by default there). A selector "Play style" on each controller playing
  guitar, its own (the SDK's `gamepad_device_guitar_layout`, its change 115; an entry only where it is not every
  controller's, `gamepad_guitar_layout`): "Controller · GH III" or "Guitar · Xbox 360" (a guitar through an adapter
  that Windows sees as a controller plays as before, while a DualSense plays as in GH III). As a controller, in a
  song the game plays it with Guitar Hero III's own controller code (the game's
  hooks), its frets on LT, LB, RB, RT and A, the open note on B, no strum (pressing a fret plays, releasing one never
  does), Star Power on the D-pad or Back, the whammy bar on either stick; the menus as they are. While on, the strum
  switch and the whammy bar's and the tilt's rows go (nothing to strum, no tilt; a calibration would make the SDK take
  the controller for a guitar), and its rows are two groups (Gustavo: an orange fret on Triangle must not move the
  menus' Accept). **In a song**: the five frets, the open note, Star Power and the whammy bar, each with two chips,
  any of the controller's buttons, triggers, D-pad directions or sticks (the SDK's `gamepad_song_binds`, its change
  113; GH III's own by default: Star Power on Back and the D-pad, the bar on the left and the right stick). An input
  given to one chip leaves any other (the D-pad's chip goes when one of its directions is taken); X, Delete or a right
  click clears one; a whammy bar on a button is all or nothing in the game. **Navigation**: the Xbox 360 controls of
  the menus and the pause menu, the controller's `gamepad_device_remap` as before. Reset puts both groups and the
  play style back. As a guitar:
  "guitar", as before. Texts in five languages, and a tip in the controller's help.
- **A selector opens a list** (Gustavo: "deberían desplegar una lista y poder seleccionar una opción"): Instrument,
  Drum kit and Play style. A or a click on the row opens its values under the chip (over it when the page ends first),
  the current one checked and focused; up and down move, A or a click picks, B, Escape or a click outside closes it
  unchanged (`SelectorRow` takes the values and the current index and answers the one picked; `ChoiceList`, drawn last
  on the Controls page). Before, A started a change stepped with left and right, and a click stepped to the next value.
- **Play from the first run's last step** (a player who already had a profile, e.g. from another game: GH Warriors of
  Rock's first two-PC test). Its Play left the first run on, and its screen was drawn before "Compiling shaders", which
  never showed; the preparation's poll, there only, never ran either. Play pressed again started a second compile and
  waited for the first on the main thread (8.5 s without answering, a watchdog dump); the game started at a later
  Play, and once more, unasked, when "Review settings" ended the first run. Now Play ends the first run, "Compiling
  shaders" comes before the install's screens, and a Play during a preparation waits for it. The busy line names
  "compiling shaders".

## 3.22.0

Gustavo's requests of 2026-11-14 (GH Metallica first; the other games move later):

- **Betas from the launcher.** Updates has a second switch, "Get beta versions" (`launcher_beta_updates`, off): with
  it, the newest release offered may be a GitHub pre-release; without it, only published ones, as before (a player on a
  beta stays on it until a newer final version). The check keeps the pre-releases (drafts still left out), so an
  installed beta now shows its own notes (they were left empty: "Check for updates to see the notes"). Versions with a
  suffix compare as Semantic Versioning says: `1.0.0-beta.2` < `1.0.0-beta.10` < `1.0.0-rc.1` < `1.0.0` (before, two
  betas of one version compared equal, and beta.2 was never offered over beta.1). Checked with `--launcher_update_url`
  on a local list: off, up to date with beta.1's notes; on, beta.10 offered with beta.10's and beta.2's notes.
- **Settings' rows as an accordion with the mouse.** The row under the mouse opens after resting on it 80 ms (crossing
  the list opens nothing): it grows over 200 ms, eased in and out, while the open one closes at the same pace, so the
  rows under the mouse stay where they are. Its line of help fades in under the label, the value's two looks (the
  small row's, the big row's between its arrows) crossfade; it stays open when the mouse leaves the rows. The
  controller and the keyboard are as before (the focus's row is the big one; a move or A gives the choice back to
  them). Windows' animation effects off: no animation.
- **A click changes a value only on its control.** In Settings and Updates a switch flips on a click on it, a value
  steps with its arrows; a click elsewhere on the row only opens it. Rows that open something (a list, a sheet) open
  from anywhere on them. The controller's A is unchanged.
- **Asked first** (`App::AskConfirm`, a card over a veil, Cancel with B, Escape or a click outside it; the action with
  A): a page opening in the browser (Donate, Updates' full history) names its address, and quitting the launcher (B or
  Escape on Play, the Quit buttons on Play and the first run's welcome) asks "Quit the launcher?". Not asked: the
  window's own close button, Alt+F4 and Windows shutting down.

## 3.21.3

The DLC tab emptied itself (2026-11-14, Gustavo with GH Metallica 1.0.0-beta.1: the DLC folder chosen, then "Remove
copies"; the log: "DLC folder: , 0 packages" right after the copies were deleted, though `dlc_library.txt` still named the
folder). `GameUserDataFolder` wrote and deleted `userdata\.write_test` at every call to see whether the folder can be
written, and the DLC page reads its two lists on two threads at once (after the copies are deleted, and when the
launcher opens): now and then one found the other deleting the file, took the folder for read-only and looked in
Documents, where there is no DLC folder setting, so the tab listed nothing and the game's `content_packages.txt` was
deleted (a Play then would start the game without the folder's DLC). Measured with the old test on two threads: 13
failures in 2,000 calls each, none on one. Now decided once per folder, under a lock, with a file of the launcher's own
(`.launcher_write_test_<process>`; the game's own test uses `.write_test`).

## 3.21.2

`game::kGuitarControlsApart` (2026-11-13; false by default, nothing changes for a game that leaves it out): for a game
whose controllers play on a layout of their own. GH Aerosmith (Guitar Hero III's engine) plays a plain controller with
the frets on LT, LB, RB, RT and A and no strum, and a guitar on its frets (A, B, Y, X, LB): a real guitar's page then
names its controls with the game's `GameGuitarControlsFor`, a controller's and the keyboard's with `GameControlsFor`.

## 3.21.1

`game::kHasFourLaneDrums` (2026-11-13; true by default, nothing changes for a game that leaves it out): false for a game
whose drums are five lanes only. The Drum kit row (five or four lanes) is then not shown on a controller's page nor the
keyboard's, and a kit's `:1` flag left in the options file reads as five lanes. Added for Guitar Hero 5's engine, which
then turned out to have the four-lane highway too (GH5, Band Hero and Warriors of Rock: a drum kit's type 11 kept
unless its capability flags have bit 0, and `UseFourLaneHighway`, as GH Metallica's): no game sets it false yet.

## 3.21.0

Settings > Online through Steam only (2026-11-13, Gustavo's choice after the first game between two homes): playing by
address waits until online play has been tried more widely.

- `game::kOnlineByAddress` (false by default): with it false, Settings > Online shows Online play and Through Steam
  only. Connection (Host / Join a friend), the friend's address, Your address, Internet and Port are not shown, and
  the Steam row is locked: on while online play is on (locked off with `steam_api64.dll` missing, as before). The
  options file gets `online_steam` = `online_play`, an empty `online_host` and `online_upnp = false` (no port asked of
  the router). Online play's hint and help, the Steam row's and the category's line speak of Steam friends (texts in
  the five languages). The friend's address typed before is kept in `launcher_online_address`.
- A game that sets `kOnlineByAddress` true gets 3.20's page.

## 3.20.0

Settings > Online (2026-11-13, on the `online` branch with the SDK's online play, changes 89-97): online play between
friends, experimental, for a game that sets `game::kHasOnline` (Guitar Hero: Metallica). Nothing changes for the
others.

- A fifth Settings category, Online (`Page::kOnline`, under the Settings tab), after Audio. Its rows: Online play
  (`online_play`, a switch marked experimental); Connection, Host or Join a friend; the friend's address while joining
  (`online_host`), or Your address while hosting (this PC's IPv4 address with the port; chosen, it goes to the
  clipboard, and its help lists every address with its adapter's name: `net::LocalAddresses`, iphlpapi); Port
  (`online_port`, 1024-65535, 3074 by default); Through Steam (`online_steam`, experimental; locked off where
  `rexsteam.exe` or Valve's `steam_api64.dll` is not next to the game). While online play is off the other rows are
  locked with their values. Reset sets them back but keeps the friend's address.
- An Internet row under Your address while hosting (the SDK's change 99 forwards the port by UPnP): the router's
  internet address as it reports it by UPnP (`net::AskRouter`: an SSDP search on the LAN, the router's description,
  GetExternalIPAddress; read only, on a thread, again a minute after each answer). A public address is shown with the
  port and copied when chosen; a private one is "Not reachable" with a CGNAT badge (the provider's carrier-grade NAT,
  or a second router: no forwarding reaches the PC); no answer is "Unknown" (UPnP off: the port by hand). Your
  address's help now points at it.
- The address and the port are typed in a sheet at the right (`App::OpenTextSheet`, `TextSheetWindow`: the field typed
  in from the start, Save with A or Enter, Cancel with B or Escape, what is wrong under the field). The address takes
  "a.b.c.d" or "a.b.c.d:port" (3074 when it names none), written as the SDK's `online_host` takes it
  (`OnlineEndpoint`); while hosting it is kept in `launcher_online_address`.
- A Settings row can do something when chosen instead of stepping a value (`OptionRow::activate`, with an icon before
  its value), and give the help panel a list (`OptionRow::bullets`); a switch shows its chip (EXPERIMENTAL) before it.
- Texts: `nav_online`, `settings_intro_online` and the `online_*` rows' (five languages).

## 3.19.1

A patch of 3.19 (Gustavo: these changes as 3.19.1; the commits before this one called it 3.20.0): drums on the
keyboard and the controllers (2026-11-11), rows of values changed after A, Settings' focus without a jump. Needs the
SDK's change 87 for four lanes.

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
- A row of values changes only after A, as before 3.18 (Gustavo: left on a Settings row changed its value instead of
  reaching the categories): A starts changing it, left and right then step it and up and down stay, until A or B or
  the focus leaves (`ui::RowEditing`, `SetRowEditing`, `EditingStep`; the arrows, or a selector's chip, in the
  accent meanwhile; the footer says "Change value" and "Done"). Otherwise left goes to the column at the left (the
  Settings categories, the Controls devices). A switch flips with A at once; the mouse steps a value with a click or
  the arrows. For Settings' rows, the microphone's and the Instrument and Drum kit selectors.
- Settings: the big row follows the focus from the start of a frame. Up and down through the rows go by
  `ColumnNav`'s `RequestFocus`, given when the target row is drawn, in the middle of the frame; the big row changed
  there, so the rows before it and after it were drawn small: one frame with no row outlined, a jump. Now chosen
  before the rows from the focus or the focus waiting to be given (`ui::PendingFocus`); checked with bursts of
  screenshots, ten moves up and down without that frame.
- The keyboard back on drums takes its kit's default keys while they are the other kit's (`FitDrumKeys`, also when
  the options file is read).
- Settings: the dot of the current category 8 dp further left, clear of the focus's outline.

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
