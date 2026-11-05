// The controllers connected: kept open, so that each of them reports its buttons and axes.
//
// SDL reports only an open controller's, and ImGui's backend opens just the first one (the one that navigates). The
// Controls page listens to the controller it shows, whichever that is, so anything after the first assigned nothing
// and its whammy bar read 0: a DualSense, which SDL lists before any XInput device, left a guitar deaf.

#pragma once

#include <SDL3/SDL_joystick.h>

#include <string>

namespace launcher {

// Opens each controller not open yet and closes the ones gone. Call it at the start, back from the game and when one
// connects or disconnects.
void OpenControllers();
// Before the gamepad subsystem quits (the controllers are the game's while it runs).
void CloseControllers();

// The controller ImGui navigates with: SDL's first.
bool NavigatesWith(SDL_JoystickID id);

// A PS3 Guitar Hero guitar in a game with a whammy bar: USB 12BA:0100, as a CRKD guitar on its P3 setting should be.
// As the SDK's guitar_ps3_protocol (recomp change 71, thirdparty/rexglue-sdk/src/input/sdl/sdl_input_driver.cpp),
// repeated here: its yellow and blue swapped back in SDL's mapping, and its bar at rest at the middle of its axis.
bool IsPs3Guitar(SDL_JoystickID id);
inline constexpr int kPs3GuitarWhammyRest = 128;
inline constexpr int kPs3GuitarWhammyFull = 32767;

// Every joystick SDL has, for the log: its name, type, kind, USB IDs and GUID, and whether it is a gamepad at all (one
// that is not reaches neither the launcher nor the game).
std::string ControllersText();

}  // namespace launcher
