// The controllers connected: kept open, so that each of them reports its buttons and axes.
//
// SDL reports only an open controller's, and ImGui's backend, left to itself, opens just the first one. The Controls
// page listens to the controller it shows, whichever that is, so anything after the first assigned nothing and its
// whammy bar read 0: a DualSense, which SDL lists before any XInput device, left a guitar deaf.
//
// ImGui navigates with one controller at a time: not all of them merged, a guitar's whammy bar and tilt rest over.

#pragma once

#include <SDL3/SDL_joystick.h>

#include <string>

namespace launcher {

// SDL's gamepads less the GameInput twins (launcher 3.23): with GameInput's raw reports on (for the Xbox One guitars)
// SDL's GameInput driver also takes a plain controller that another of its drivers (XInput, HIDAPI, raw input) reads,
// and the same controller came twice. A plain gamepad SDL reads through GameInput (its GUID's driver byte 'g') is left
// out while another driver reads one of the same USB IDs (it may be the only one: which driver comes first varies), as
// the SDK's sdl_input_driver.cpp (EnumerateDevices). As SDL_GetGamepads: an array to SDL_free, `count` set.
SDL_JoystickID* Gamepads(int* count);
bool IsGameInputTwin(SDL_JoystickID id);

// Opens each controller not open yet and closes the ones gone. Call it at the start, back from the game and when one
// connects or disconnects.
void OpenControllers();
// Before the gamepad subsystem quits (the controllers are the game's while it runs).
void CloseControllers();

// The controller ImGui navigates with: the last one that pressed a button, at first SDL's first.
bool NavigatesWith(SDL_JoystickID id);
void NavigateWith(SDL_JoystickID id);

// A PS3 Guitar Hero guitar in a game with a whammy bar: USB 12BA:0100, as a CRKD guitar on its P3 setting should be.
// As the SDK's guitar_ps3_protocol (recomp change 71, thirdparty/rexglue-sdk/src/input/sdl/sdl_input_driver.cpp),
// repeated here: its yellow and blue swapped back in SDL's mapping, and its bar at rest at the middle of its axis.
bool IsPs3Guitar(SDL_JoystickID id);
inline constexpr int kPs3GuitarWhammyRest = 128;
inline constexpr int kPs3GuitarWhammyFull = 32767;

// Every joystick SDL has, for the log: its name, type, kind, USB IDs and GUID, and whether it is a gamepad at all (one
// that is not reaches neither the launcher nor the game).
std::string ControllersText();

// --- Xbox One guitars and drum kits (launcher 3.23): GIP instruments such as the PDP Riffmaster, neither XInput nor
// HID devices. SDL reads them through GameInput's raw reports (the SDK's thirdparty/gameinput, change 110), which needs
// the Microsoft GameInput redistributable, API 3: GameInputRedist.msi, beside the launcher in a release. ---

enum class GameInputState {
  kNotNeeded,   // a game without guitars, or an SDK without GameInput's reader
  kOn,          // GameInput answers: those instruments are read
  kMissing,     // the redistributable is not installed (or too old)
  kInstalling,  // InstallGameInput's installer runs
};
GameInputState GameInputStatus();
// GameInputRedist.msi beside the launcher.
bool GameInputInstallerFound();
// Runs the redistributable's installer (Windows asks for an administrator's consent); false if it did not start.
bool InstallGameInput();
// Once the installer InstallGameInput started has ended: true, with whether it installed. The gamepad subsystem is to
// start again then (SDL looks for GameInput as it starts), with RestartGameInput between.
bool GameInputInstallEnded(bool* installed);
// After the gamepad subsystem quits, before it starts again: GameInput looked for once more.
void RestartGameInput();
// Before SDL's gamepad subsystem starts: GameInput's raw reports (SDL_HINT_JOYSTICK_GAMEINPUT_RAW) on only while an
// Xbox One guitar or drum kit is connected (the SDK's CountRawInstruments): with them on, SDL's GameInput driver also
// takes Xbox 360 instruments and controllers, as gamepads or as well as XInput.
void ChooseGameInputReports();
// The installer ran and GameInput still does not answer.
bool GameInputInstallFailed();

// An Xbox One guitar SDL reads through GameInput as a Rock Band 4 guitar, as the SDK's IsGameInputRockBandGuitar
// (src/input/sdl/sdl_input_driver.cpp) judges it: its tilt is its report's byte, which SDL gives only as the right
// shoulder, and its pickup selector is on the right stick's Y.
bool IsXboxOneGuitar(SDL_JoystickID id);
// That guitar's tilt, its GameInput report's byte (0-255): -1 without a reading.
int XboxOneGuitarTilt(SDL_JoystickID id);
// The SDK's default for that tilt (kGipGuitarTilt): from 0 level to 0xD0 tilted.
inline constexpr int kXboxOneGuitarTiltRest = 0x00;
inline constexpr int kXboxOneGuitarTiltFull = 0xD0;

}  // namespace launcher
