// The types of a game's profile (the game's launcher/game.h): what each of its lists holds. The same for every game.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace launcher::game {

// The Graphics page's frame rate row (game.h's kFrameRates): a value of kFrameRateCvar and the frame rate it gives. A
// boolean cvar (kFrameRateIsBool: the values are 0 and 1, as a game's 60 FPS switch) or a number (the emulated vblank's
// rate, video_mode_refresh_rate, which some games follow). A rate above 60 is offered only where the monitor reaches
// it, and marked experimental.
struct FrameRate {
  int value;
  int fps;
};

// A language of the game (game.h's kLanguages, the Game page's language row): an XLanguage id (user_language:
// 1 English, 2 Japanese, 3 German, 4 French, 5 Spanish, 6 Italian, 7 Korean, 8 Traditional Chinese, 9 Portuguese,
// 10 Simplified Chinese, 11 Polish, 12 Russian) with its name in its own words (UTF-8; the launcher draws other scripts
// with Windows' fonts). A game with two versions of a language lists it twice, the second with a boolean cvar set when
// it is chosen (Kameo's Latin American Spanish). Until one is chosen, the Windows display language's: the entry whose
// `locales` (a locale name prefix, "es-ES") matches the user's locale best, or the first with that id.
struct Language {
  uint32_t id;
  const char* name;
  const char* variant_cvar;  // null: none
  const char* locales;       // null: any locale of the language
};

// Where a switch of the game's own is shown in the Settings tab (launcher 3.28, Gustavo's "propuesta A v2": Display,
// Graphics, Effects, Audio, Game, Mods, Online):
// - kGame: the Game category, under "Other" (before 3.28 the Game category's only place; a game that has not chosen
//   another keeps its switches there);
// - kScreenEffects: the Effects category, the game's post-processing (each turned off by a cvar of its src/);
// - kStart: the Game category, "Starting the game" (the intro videos);
// - kSmoothness: the Display category, "Smoothness", after the frame rate and V-Sync;
// - kUpscaling: the Graphics category, under Upscaling (NativeXRenderer only; `shown` says with which upscaling);
// - kModsHighway, kModsHighwayLook, kModsStage, kModsSound, kModsTools: the Mods tab's sections (launcher 3.30): the
//   Highway section's "Shape" and "Look" groups, Stage, Sound, and Tools (before the launcher's Debug keys).
enum class ToggleGroup {
  kGame, kScreenEffects, kStart, kSmoothness, kUpscaling, kModsSound, kModsTools, kModsHighway, kModsStage,
  kModsHighwayLook
};

// When a switch under Upscaling is shown (it is ignored otherwise; its value is kept): always, while something enlarges
// the 3D scene (AMD FSR 3.1 or NVIDIA DLSS, their antialiasing alone included, or a lower resolution on the 3D scene
// only), or while NVIDIA DLSS upscales (not DLAA).
enum class ToggleShown { kAlways, kSceneUpscaled, kDlssUpscales };

// A switch of the game's own (game.h's kToggles): a boolean cvar with a row in the Settings tab; its title, hint and help
// are game.cpp's (GameTexts::toggles and toggle_help, in the same order). `needs`: another switch's cvar that must
// be on for this one to be (GH Metallica's grain needs its color correction); while that one is off, this one is off
// and cannot be changed, and its row is drawn under that one's, indented.
struct Toggle {
  const char* cvar;
  bool default_value;
  ToggleGroup group = ToggleGroup::kGame;
  const char* needs = nullptr;
  ToggleShown shown = ToggleShown::kAlways;
};

// A row of the game's own with several values (game.h's kChoices, launcher 3.29): a cvar written as one of `values`
// (as they go into the options file: a number, or with `quoted` a string in quotes), `default_value` one of them. Its
// title, hint and help are game.cpp's (GameTexts::choices and choice_help, in kChoices' order); each value is shown
// by its label in GameTexts::choice_values, or where that has none by the value itself followed by `suffix` (" %").
// In its group the rows come after the group's switches. A value in the file that is none of them reads as the
// default.
struct Choice {
  const char* cvar;
  std::span<const char* const> values;
  const char* default_value;
  ToggleGroup group = ToggleGroup::kGame;
  const char* suffix = "";
  bool quoted = false;
  // Launcher 3.30: a switch's cvar this row is under (indented); while that switch is off the row is locked.
  const char* needs = nullptr;
};

// Launcher 3.30: the Mods tab's Highway section (game.h's kHighwayPreview): a gallery of the player's pictures for the
// note highway (a folder, one picture or a random one at each song, favorites, bright ones darkened toward black) and a
// preview drawn from the installed game's own files. The cvars the game reads (its src/), and where its files are.
struct HighwayGallery {
  const char* folder_cvar;      // the pictures' folder
  const char* image_cvar;       // a picture of it, "<random>", or "" for the game's own
  const char* random_from_cvar; // "all" or "favorites"
  const char* favorites_cvar;   // names separated by |
  const char* dim_cvar;         // "off", "auto" or a percent, for every picture
  const char* image_dims_cvar;  // name:value separated by |
  const wchar_t* folder;        // the default folder's name in the game's user data folder
  // The rows the preview follows (game::kChoices' cvars, percentages): the scale, the length, the perspective, the
  // opacity; the style's and its value for the black highway.
  const char* scale_cvar;
  const char* length_cvar;
  const char* perspective_cvar;
  const char* opacity_cvar;
  const char* style_cvar;
  const char* dark_style;
};

// The preview's assets in the installed game (launcher 3.30): a Neversoft pak (CHNK-compressed), the texture
// dictionary in it, and the textures by their keys (QBKeys of their names); and the highway's shape, the game's own
// values (in the game's 1280 x 720; its src/mods.cpp changes them the same way).
struct HighwayAssets {
  const wchar_t* pak;           // relative to the game's files, e.g. data/compressed/ZONES/z_in_game.pak.xen
  uint32_t dictionary;          // the .tex entry's full name key
  // The highway, a sidebar, a string, a beat line, the strike line's ring, a gem's rim, its colored body and its cap,
  // a sustain.
  uint32_t highway, sidebar, string, fretbar, collar, gem_base, gem_color, gem_cap, sustain;
  float playline;               // the strike line's y
  float height;                 // from the far end to it
  float top_width, bottom_width;
  double perspective_fact, perspective_exp;  // GeneratePosTable's f and e
  int rows;                     // its rows (real_highway_lines)
  float crossing_time;          // seconds a note takes over the highway (Expert)
  float highest_top;            // the far end no higher than this y (the game's length limit)
  float tiling;                 // the texture's repeats over the highway (ghighwaytiling)
};

// A control's keys by default where they are not the SDK's (game.h's kKeyboardDefaults: the Keyboard page's "Reset to
// defaults", and a control the files do not set): a keybind_* cvar and its keys, as the game's src/startup.cpp writes
// them.
struct KeyDefault {
  const char* cvar;
  const char* keys;
};

// A function of the game, as the Keyboard and Gamepad pages show it in place of the Xbox 360 control it is on (game.cpp's
// GameControlsFor; GH Metallica's mockup: "Green fret" where the launcher said "A button"): its chip, a square of a
// color or a short text on grey, and its names.
struct GameControl {
  int bind;              // the Xbox 360 control: a Bind of settings.h
  const char* chip;      // the chip's text (UTF-8; an up or down arrow draws one), or "": color only
  uint32_t chip_color;   // 0xRRGGBB
  uint32_t chip_text;    // 0xRRGGBB
  const char* name;      // the function, in the launcher's language
  // The Controls tab's value for a control the controllers cannot move (the D-pad, a stick): its name on the
  // controller ("Right stick").
  const char* on_pad = nullptr;
};
struct GameControlGroup {
  const char* title;
  std::vector<GameControl> controls;
};
// A real guitar's functions, for a game that sets kGuitarControlsApart (game.cpp defines it then): a controller's are
// GameControlsFor, on the controller's own layout.
const std::vector<GameControlGroup>& GameGuitarControlsFor(uint32_t language);

// What the game does with an installed package it does not read (DlcRule): the DLC page's note on such a package.
enum class DlcForeign {
  kUnused,       // it passes it by (GH Metallica reads only Death Magnetic's adl1.pak)
  kDamaged,      // it reports the package damaged at start (GH Warriors of Rock)
  kDisablesAll,  // it reports a damaged package and turns off every DLC (Guitar Hero 5)
};

// Which installed packages the game reads (game.h's kDlcRule; the DLC page, dlc.cpp). One it does not read is turned
// off, moved out of the game's content folder, until the player turns it on. With neither `files` nor
// `manifest_prefix`, every package is read: no check.
struct DlcRule {
  // The package holds one of these files at its root, in lower case (GH Metallica: "adl1.pak.xen").
  std::span<const char* const> files;
  // Neversoft's manifests (Guitar Hero 5 and later): the game knows a package by the QBKey of its display name, made
  // lower case with every other character than a letter or a digit an underscore, of its first `name_length`
  // characters. It reads a package whose key is in a manifest: "<manifest_prefix><key, %x>.pak.xen" in the package
  // itself, another package's file of that prefix, or `known_names`, the list on the game's disc.
  const char* manifest_prefix = nullptr;
  size_t name_length = 128;
  std::span<const uint32_t> known_names;
  DlcForeign foreign = DlcForeign::kUnused;
};

// A part of the game's art, in the art's own units (its SVG's viewBox): game.h's kArtCorner.
struct ArtPart {
  float left, top, right, bottom;
};

// How the DLC page gives the game its downloadable content (game.h's kDlcMode).
enum class DlcMode {
  // The packages of a folder the player chooses are extracted into the game's content folder, each by the game itself
  // (its --install_content tool mode, the SDK's change 44), as the launchers before 3.16 did.
  kInstall,
  // The player's DLC folder, every game's packages in one place, read by the game where they are (the SDK's
  // content_packages.txt, its change 80: nothing copied); the packages extracted before are still read. The Guitar
  // Heroes.
  kFolder,
};

// The profile's optional settings: what a game whose game.h does not define one gets. A game.h's own definition in
// launcher::game hides the default here, because a qualified name (game::kArtTitleSize) finds a namespace's own
// declaration before looking in the namespaces it uses; so the launcher's code names them qualified, always.
namespace defaults {
// The Play page's title (kArtTitle) in dp: 96, the mockups'. Smaller where the launcher's display font makes the name
// wider than in the game's mockup and it runs over the art (Band Hero's 84).
inline constexpr float kArtTitleSize = 96.0f;
// The Game page's "Low latency audio" row (the SDK's audio_wasapi_exclusive). False for a game where the sound's delay
// does not matter (no rhythm to play to, no audio calibration): the row is hidden and the option written off.
inline constexpr bool kHasLowLatencyAudio = true;
// The DLC page's check of installed packages: none, every package stays on.
inline constexpr DlcRule kDlcRule{};
// The DLC page's mode: extracting the packages, as before 3.16 (kFolder where the game has been tried reading them in
// place).
inline constexpr DlcMode kDlcMode = DlcMode::kInstall;
// Settings > Graphics' AMD FSR 3.1 and NVIDIA DLSS, in a game that has them (kHasDlaa). False: the Upscaling row offers
// Off, FSR 1 and NIS only, and FSR 3.1 or DLSS stays an option of the options file (native_x_renderer_antialiasing =
// fsr_native, dlaa, fsr_quality, dlss_quality...): the row shows it while it is the one in use, the launcher writes it
// back as it is, and it goes once the player chooses another.
inline constexpr bool kOffersTemporalUpscalers = true;
// The art's corner under the pages other than Play (launcher 3.18): kArtCorner, a part of the art (in its own units,
// launcher\art\background.svg's viewBox) drawn at half its size and faint, against the window's right edge, its top
// kArtCornerRise dp above the window's bottom (the footer covers what falls under it). By default the art's lower right
// quarter.
inline constexpr ArtPart kArtCorner{640.0f, 380.0f, 1280.0f, 760.0f};
inline constexpr float kArtCornerRise = 145.0f;
// Settings > Online (launcher 3.20): online play between friends, the SDK's online_play (its changes 89-97 on the
// online branch): the category and the options file's online_* keys. False: neither.
inline constexpr bool kHasOnline = false;
// Settings > Online (launcher 3.21): playing by address, the Connection row (hosting, or joining a friend's address),
// this PC's addresses and the port. False: through Steam only, which turns on with Online play (Gustavo's choice while
// online play is tried between homes; the options file then has no online_host and no UPnP).
inline constexpr bool kOnlineByAddress = false;
// Drums (launcher 3.19.1; optional since 3.23.1): whether the game plays a drum kit, its functions for a controller or the
// keyboard playing drums (GameDrumControlsFor, five or four lanes; game.cpp defines it then) and the keyboard's keys
// for them. False: no drums anywhere (the Instrument row, the Controls and Keyboard pages' drum rows, the microphone's
// page); the empty list and arrays only let the launcher's code build for a game without them (Kameo, POTCAWE, GRAW).
inline constexpr bool kHasDrums = false;
inline constexpr std::array<KeyDefault, 0> kDrumKeyboardDefaults{};
inline constexpr std::array<KeyDefault, 0> kDrumKeyboardDefaultsFourLanes{};
inline const std::vector<GameControlGroup>& GameDrumControlsFor(uint32_t, bool) {
  static const std::vector<GameControlGroup> kNone;
  return kNone;
}
// Drums (game::kHasDrums, launcher 3.19.1): whether the game also plays a Rock Band kit on a four-lane highway (the
// SDK's change 87, a kit's capability flags with bit 0; the World Tour engine's games and Guitar Hero 5's: GH5, Band
// Hero and Warriors of Rock all have it). False: the Drum kit row (five or four lanes) is not shown and every kit plays
// five lanes (a "8:1" left in the options file reads as five).
inline constexpr bool kHasFourLaneDrums = true;
// A real guitar's controls apart from a controller's (launcher 3.21.2): true for a game that plays a plain controller on
// a layout of its own (Guitar Hero III's engine: the frets on the triggers, the bumpers and A, no strum), so that a guitar's
// page names its frets with GameGuitarControlsFor and a controller's and the keyboard's with GameControlsFor. False: one
// list for both (a controller reported as a guitar, as GH Metallica's).
inline constexpr bool kGuitarControlsApart = false;
// Guitar Hero III's controller layout for a controller playing guitar (launcher 3.23, the SDK's gamepad_guitar_layout
// = "gh3", its change 112): for a game that has no layout of its own for a controller, which it is told is a guitar
// (Guitar Hero 5 and later give a controller the vocals). In a song the frets on LT, LB, RB, RT and A, a change of the
// frets held strums, Star Power on the D-pad or Back, the whammy bar on either stick; the menus as they are. True: a
// switch on each controller playing guitar (on until the player turns it off, as the game's src/startup.cpp), its
// buttons in a song apart from the menus' (the SDK's gamepad_song_binds, recomp change 113: "In a song" and
// "Navigation"), and the options file's gamepad_guitar_layout. False: none of them.
inline constexpr bool kHasGh3PadLayout = false;
// Settings > Graphics' DLSS model row (launcher 3.24), with NVIDIA DLSS: Transformer or CNN (NativeXRenderer's
// native_x_renderer_dlss_model). False: no row, and the options file gains no key.
inline constexpr bool kHasDlssModel = false;
// Resolutions below the game's own (launcher 3.27, NativeXRenderer's D174 and D175): the internal resolution also
// offers 540, 480 and 360 lines, and FSR 1's and NIS's Quality goes below the game's lines too (on a 1080p screen
// Performance draws 540 lines, Ultra Performance 360). A row chooses what a lower resolution lowers: the 3D scene only
// (D174, native_x_renderer_scene_scale; the HUD, the menus and the game's effects stay at the screen's resolution, as
// with FSR 3.1 and DLSS; only where NativeXRenderer finds the game's views, game::kHasDlaa) or the whole image (D175,
// native_x_renderer_resolution_scale below 1). False: the internal resolution and FSR 1 / NIS lower the whole image,
// at least to the game's own lines.
inline constexpr bool kLowResolutions = false;
}  // namespace defaults
using namespace defaults;

}  // namespace launcher::game
