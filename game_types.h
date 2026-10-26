// The types of a game's profile (the game's launcher/game.h): what each of its lists holds. The same for every game.

#pragma once

#include <cstdint>
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

// A switch of the game's own (game.h's kToggles): a boolean cvar with a row in the Game page; its title and hint are
// game.cpp's (GameTexts::toggles, in the same order).
struct Toggle {
  const char* cvar;
  bool default_value;
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
  // The Gamepad page's value for a control the controllers cannot move (the D-pad, a stick): on a guitar ("Whammy
  // bar") and on any other controller ("Right stick"). On a guitar the moved buttons are named after the function they
  // press by default ("Green fret").
  const char* on_guitar = nullptr;
  const char* on_pad = nullptr;
};
struct GameControlGroup {
  const char* title;
  std::vector<GameControl> controls;
};

}  // namespace launcher::game
