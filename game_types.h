// The types of a game's profile (the game's launcher/game.h): what each of its lists holds. The same for every game.

#pragma once

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

// Where a switch of the game's own is shown in the Settings tab: the Game category, or the Graphics category's screen
// effects (the game's post-processing, each turned off by a cvar of its src/).
enum class ToggleGroup { kGame, kScreenEffects };

// A switch of the game's own (game.h's kToggles): a boolean cvar with a row in the Settings tab; its title, hint and help
// are game.cpp's (GameTexts::toggles and toggle_help, in the same order). `needs`: another switch's cvar that must
// be on for this one to be (GH Metallica's grain needs its color correction); while that one is off, this one is off
// and cannot be changed.
struct Toggle {
  const char* cvar;
  bool default_value;
  ToggleGroup group = ToggleGroup::kGame;
  const char* needs = nullptr;
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
// The Video page's AMD FSR 3.1 and NVIDIA DLSS, in a game that has them (kHasDlaa). False: the Upscaling row offers Off,
// FSR 1 and NIS only, and FSR 3.1 or DLSS stays an option of the options file (native_x_renderer_antialiasing =
// fsr_native, dlaa, fsr_quality, dlss_quality...): the row shows it while it is the one in use, the launcher writes it
// back as it is, and it goes once the player chooses another.
inline constexpr bool kOffersTemporalUpscalers = true;
}  // namespace defaults
using namespace defaults;

}  // namespace launcher::game
