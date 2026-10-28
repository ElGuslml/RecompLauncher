// The Controls tab (Gustavo's 2026-10-28 mockup): the devices on the left (the controllers SDL sees, then the keyboard,
// then the microphone where the game sings: page_microphone.cpp), the chosen one's mapping in the middle (each of the
// game's functions, or each Xbox 360 control, and what presses it; the microphone's device) and its help on the right,
// with Reset. Every change is saved at once.
//
// A controller's mapping is the SDK's gamepad_remap (recomp change 36,
// thirdparty/rexglue-sdk/src/input/sdl/sdl_input_driver.cpp): one layout per type of controller, kept in the options
// file, applied by the game to every controller of that type. The names, the standard layout and the order sections
// apply in are the SDK's, repeated here. The keyboard's rows are page_keyboard.cpp's.

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

// --- gamepad_remap, as the SDK reads it ---

constexpr int kInputLeftTrigger = 32;
constexpr int kInputRightTrigger = 33;
constexpr uint64_t Bit(int input) { return uint64_t(1) << input; }

constexpr int kControls = 13;
enum Control : int { kA, kB, kX, kY, kLB, kRB, kLT, kRT, kLS, kRS, kStart, kBack, kGuide };
constexpr const char* kControlNames[kControls] = {"a",  "b",  "x",  "y",     "lb",   "rb",   "lt",
                                                  "rt", "ls", "rs", "start", "back", "guide"};

struct InputName {
  const char* name;
  int input;
};
constexpr InputName kInputNames[] = {
    {"south", SDL_GAMEPAD_BUTTON_SOUTH},
    {"east", SDL_GAMEPAD_BUTTON_EAST},
    {"west", SDL_GAMEPAD_BUTTON_WEST},
    {"north", SDL_GAMEPAD_BUTTON_NORTH},
    {"back", SDL_GAMEPAD_BUTTON_BACK},
    {"guide", SDL_GAMEPAD_BUTTON_GUIDE},
    {"start", SDL_GAMEPAD_BUTTON_START},
    {"ls", SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {"rs", SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {"lb", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
    {"rb", SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {"dpup", SDL_GAMEPAD_BUTTON_DPAD_UP},
    {"dpdown", SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {"dpleft", SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {"dpright", SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {"misc1", SDL_GAMEPAD_BUTTON_MISC1},
    {"paddle1", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1},
    {"paddle2", SDL_GAMEPAD_BUTTON_LEFT_PADDLE1},
    {"paddle3", SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2},
    {"paddle4", SDL_GAMEPAD_BUTTON_LEFT_PADDLE2},
    {"touchpad", SDL_GAMEPAD_BUTTON_TOUCHPAD},
    {"misc2", SDL_GAMEPAD_BUTTON_MISC2},
    {"misc3", SDL_GAMEPAD_BUTTON_MISC3},
    {"misc4", SDL_GAMEPAD_BUTTON_MISC4},
    {"misc5", SDL_GAMEPAD_BUTTON_MISC5},
    {"misc6", SDL_GAMEPAD_BUTTON_MISC6},
    {"lt", kInputLeftTrigger},
    {"rt", kInputRightTrigger},
};

using Sources = std::array<uint64_t, kControls>;

// The standard layout (the SDK's kDefaultSources).
constexpr Sources kDefaultSources = {
    Bit(SDL_GAMEPAD_BUTTON_SOUTH) | Bit(SDL_GAMEPAD_BUTTON_LEFT_PADDLE2),
    Bit(SDL_GAMEPAD_BUTTON_EAST) | Bit(SDL_GAMEPAD_BUTTON_LEFT_PADDLE1),
    Bit(SDL_GAMEPAD_BUTTON_WEST) | Bit(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2),
    Bit(SDL_GAMEPAD_BUTTON_NORTH) | Bit(SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1),
    Bit(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER),
    Bit(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER),
    Bit(kInputLeftTrigger),
    Bit(kInputRightTrigger),
    Bit(SDL_GAMEPAD_BUTTON_LEFT_STICK),
    Bit(SDL_GAMEPAD_BUTTON_RIGHT_STICK),
    Bit(SDL_GAMEPAD_BUTTON_START),
    Bit(SDL_GAMEPAD_BUTTON_BACK),
    Bit(SDL_GAMEPAD_BUTTON_GUIDE) | Bit(SDL_GAMEPAD_BUTTON_MISC1) | Bit(SDL_GAMEPAD_BUTTON_TOUCHPAD),
};

std::string_view Trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
  return s;
}

template <typename F>
void ForEachPiece(std::string_view s, char separator, F f) {
  while (true) {
    size_t end = s.find(separator);
    f(Trim(s.substr(0, end)));
    if (end == std::string_view::npos) return;
    s.remove_prefix(end + 1);
  }
}

bool EqualsCI(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
         });
}

// A section's type ("*", "ps5"...), or empty for a piece that is not a section.
std::string_view SectionType(std::string_view section) {
  size_t colon = section.find(':');
  return colon == std::string_view::npos ? std::string_view() : Trim(section.substr(0, colon));
}

// The sources of each control for `type`: the standard layout, then the '*' sections, then (with
// `with_type`) the sections of `type`.
Sources SourcesFor(std::string_view text, std::string_view type, bool with_type) {
  Sources sources = kDefaultSources;
  for (int pass = 0; pass < (with_type ? 2 : 1); ++pass) {
    ForEachPiece(text, ';', [&](std::string_view section) {
      std::string_view section_type = SectionType(section);
      if (section_type.empty() || !(pass == 0 ? section_type == "*" : EqualsCI(section_type, type))) {
        return;
      }
      ForEachPiece(section.substr(section.find(':') + 1), ',', [&](std::string_view entry) {
        size_t equals = entry.find('=');
        if (equals == std::string_view::npos) return;
        std::string_view control = Trim(entry.substr(0, equals));
        auto c = std::find_if(std::begin(kControlNames), std::end(kControlNames),
                              [&](const char* name) { return EqualsCI(control, name); });
        if (c == std::end(kControlNames)) return;
        uint64_t mask = 0;
        ForEachPiece(entry.substr(equals + 1), '|', [&](std::string_view input) {
          auto i = std::find_if(std::begin(kInputNames), std::end(kInputNames),
                                [&](const InputName& n) { return EqualsCI(input, n.name); });
          if (i != std::end(kInputNames)) mask |= Bit(i->input);
        });
        sources[size_t(c - std::begin(kControlNames))] = mask;
      });
    });
  }
  return sources;
}

// gamepad_remap with `type`'s sections replaced by one holding what `sources` changes from the rest.
std::string WithLayout(std::string_view text, std::string_view type, const Sources& sources) {
  Sources base = SourcesFor(text, type, false);
  std::string entries;
  for (int c = 0; c < kControls; ++c) {
    if (sources[size_t(c)] == base[size_t(c)]) continue;
    std::string inputs;
    for (const InputName& n : kInputNames) {
      if (sources[size_t(c)] & Bit(n.input)) inputs += (inputs.empty() ? "" : "|") + std::string(n.name);
    }
    entries += (entries.empty() ? "" : ",") + std::string(kControlNames[c]) + "=" + inputs;
  }
  std::string out;
  ForEachPiece(text, ';', [&](std::string_view section) {
    if (section.empty() || EqualsCI(SectionType(section), type)) return;
    out += (out.empty() ? "" : ";") + std::string(section);
  });
  if (!entries.empty()) {
    out += (out.empty() ? "" : ";") + std::string(type) + ":" + entries;
  }
  return out;
}

// The input a control shows: the first of its sources in the names' order (A shows South, not the Elite's
// paddle that also presses it), or -1.
int PrimaryInput(uint64_t sources) {
  for (const InputName& n : kInputNames) {
    if (sources & Bit(n.input)) return n.input;
  }
  return -1;
}

// --- The controllers ---

struct Pad {
  SDL_JoystickID id = 0;
  std::string name;
  SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN;
  SDL_JoystickType kind = SDL_JOYSTICK_TYPE_GAMEPAD;  // a guitar or a drum kit (XInput's subtype, through SDL)
};

std::vector<Pad> ConnectedPads() {
  std::vector<Pad> pads;
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  for (int i = 0; i < count; ++i) {
    Pad pad;
    pad.id = ids[i];
    const char* name = SDL_GetGamepadNameForID(ids[i]);
    pad.name = name && *name ? name : "";
    pad.type = SDL_GetGamepadTypeForID(ids[i]);
    pad.kind = SDL_GetJoystickTypeForID(ids[i]);
    pads.push_back(std::move(pad));
  }
  SDL_free(ids);
  return pads;
}

enum class Family { kXbox, kPlayStation, kNintendo };

Family FamilyOf(SDL_GamepadType type) {
  switch (type) {
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
      return Family::kPlayStation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
      return Family::kNintendo;
    default:
      return Family::kXbox;
  }
}

const char* TypeName(SDL_GamepadType type, const Texts& t) {
  switch (type) {
    case SDL_GAMEPAD_TYPE_XBOX360:
      return "Xbox 360";
    case SDL_GAMEPAD_TYPE_XBOXONE:
      return "Xbox One / Series";
    case SDL_GAMEPAD_TYPE_PS3:
      return "PlayStation 3";
    case SDL_GAMEPAD_TYPE_PS4:
      return "PlayStation 4";
    case SDL_GAMEPAD_TYPE_PS5:
      return "PlayStation 5";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
      return "Nintendo Switch Pro";
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
      return "Joy-Con";
    default:
      return t.controller;
  }
}

// How a physical input is labeled on a controller of `type`: text, or an icon (PlayStation symbols, the
// D-pad's arrows).
struct Label {
  std::string text;
  Icon icon = Icon::kNone;
  ImU32 color = col::kText;
};

Label LabelOf(int input, SDL_GamepadType type) {
  Family family = FamilyOf(type);
  bool ps4 = type == SDL_GAMEPAD_TYPE_PS4;
  bool ps5 = type == SDL_GAMEPAD_TYPE_PS5;
  bool one = type == SDL_GAMEPAD_TYPE_XBOXONE;
  // The face buttons by position: South, East, West, North.
  if (input >= 0 && input <= SDL_GAMEPAD_BUTTON_NORTH) {
    if (family == Family::kPlayStation) {
      constexpr Icon kIcons[] = {Icon::kPsCross, Icon::kPsCircle, Icon::kPsSquare, Icon::kPsTriangle};
      constexpr ImU32 kColors[] = {Hex(0x7CB2E8), Hex(0xF07B6B), Hex(0xE6A4D7), Hex(0x5BC489)};
      return {"", kIcons[input], kColors[input]};
    }
    if (family == Family::kNintendo) {
      constexpr const char* kNames[] = {"B", "A", "Y", "X"};
      return {kNames[input]};
    }
    constexpr const char* kNames[] = {"A", "B", "X", "Y"};
    constexpr ImU32 kColors[] = {col::kGlyphA, col::kGlyphB, col::kGlyphX, col::kGlyphY};
    return {kNames[input], Icon::kNone, kColors[input]};
  }
  auto pick = [family](const char* xbox, const char* playstation, const char* nintendo) {
    return Label{family == Family::kPlayStation ? playstation : family == Family::kNintendo ? nintendo : xbox};
  };
  switch (input) {
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
      return pick("LB", "L1", "L");
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
      return pick("RB", "R1", "R");
    case kInputLeftTrigger:
      return pick("LT", "L2", "ZL");
    case kInputRightTrigger:
      return pick("RT", "R2", "ZR");
    case SDL_GAMEPAD_BUTTON_LEFT_STICK:
      return pick("LS", "L3", "LS");
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
      return pick("RS", "R3", "RS");
    case SDL_GAMEPAD_BUTTON_START:
      return pick(one ? "Menu" : "Start", ps4 || ps5 ? "Options" : "Start", "+");
    case SDL_GAMEPAD_BUTTON_BACK:
      return pick(one ? "View" : "Back", ps5 ? "Create" : ps4 ? "Share" : "Select", "−");
    case SDL_GAMEPAD_BUTTON_GUIDE:
      return pick("Xbox", "PS", "Home");
    case SDL_GAMEPAD_BUTTON_MISC1:
      return pick("Share", "Mic", "Capture");
    case SDL_GAMEPAD_BUTTON_TOUCHPAD:
      return {"Touchpad"};
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
      return {"P1"};
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
      return {"P3"};
    case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
      return {"P2"};
    case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
      return {"P4"};
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
      return {"", Icon::kArrowUp};
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
      return {"", Icon::kArrowDown};
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
      return {"", Icon::kArrowLeft};
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
      return {"", Icon::kArrowRight};
    default:
      if (input >= SDL_GAMEPAD_BUTTON_MISC2 && input <= SDL_GAMEPAD_BUTTON_MISC6) {
        return {"M" + std::to_string(input - SDL_GAMEPAD_BUTTON_MISC2 + 2)};
      }
      return {"—", Icon::kNone, col::kTextDim};  // no button: the control is off
  }
}

// The rows without the game's functions: the original buttons, the D-pad (not remappable) before Guide.
constexpr int kRows = 14;
constexpr int kRowControl[kRows] = {kA, kB, kX, kY, kLB, kRB, kLT, kRT, kLS, kRS, kStart, kBack, -1, kGuide};

// The control of gamepad_remap a keyboard Bind is (settings.h), or -1 for one it cannot move (the D-pad, a stick's
// direction): the rows of the game's functions (game.h, GameControlsFor).
int ControlOf(int bind) {
  switch (Bind(bind)) {
    case Bind::kA:
      return kA;
    case Bind::kB:
      return kB;
    case Bind::kX:
      return kX;
    case Bind::kY:
      return kY;
    case Bind::kLeftShoulder:
      return kLB;
    case Bind::kRightShoulder:
      return kRB;
    case Bind::kLeftTrigger:
      return kLT;
    case Bind::kRightTrigger:
      return kRT;
    case Bind::kLStickPress:
      return kLS;
    case Bind::kRStickPress:
      return kRS;
    case Bind::kStart:
      return kStart;
    case Bind::kBack:
      return kBack;
    default:
      return -1;
  }
}

// gamepad_remap's name for a type of controller (SDL's: "ps5", "xboxone"...).
std::string TypeKey(SDL_GamepadType type) {
  const char* name = SDL_GetGamepadStringForType(type);
  return name ? name : "standard";
}

// A device's tile in the list, and its help's kicker: a guitar's green, a drum kit's red, the others' grey.
struct DeviceLook {
  Icon icon;
  ImU32 ink;
  ImU32 tile;
};

constexpr DeviceLook kMicrophoneLook = {Icon::kMicrophone, col::kTextSoft, col::kField};

DeviceLook LookOf(SDL_JoystickType kind, bool keyboard) {
  if (keyboard) {
    return {Icon::kKeyboard, col::kTextSoft, col::kField};
  }
  if (kind == SDL_JOYSTICK_TYPE_GUITAR) {
    return {Icon::kGuitar, col::kSuccess, col::kSuccessTint};
  }
  if (kind == SDL_JOYSTICK_TYPE_DRUM_KIT) {
    return {Icon::kDrum, col::kError, col::kErrorTint};
  }
  return {Icon::kGamepad, col::kTextSoft, col::kField};
}

}  // namespace

void MappingRow(const ImRect& row, ImU32 swatch, std::string_view label, float label_right, bool listening) {
  FillRect(row, listening ? Fade(col::kAccent, 0.12f) : Hex(0xFFFFFF, 8), 8.0f);
  ImVec2 center(row.Min.x + Px(14.0f) + Px(7.0f), row.GetCenter().y);
  if (swatch) {
    ImGui::GetWindowDrawList()->AddCircleFilled(center, Px(7.0f), swatch, 24);
  } else {
    ImGui::GetWindowDrawList()->AddCircle(center, Px(6.0f), Hex(0xFFFFFF, 46), 24, Px(2.0f));
  }
  constexpr Type kLabel{Font::kSemiBold, 15};
  float x = row.Min.x + Px(14.0f) + Px(14.0f) + Px(14.0f);
  DrawLabel(kLabel, ImVec2(x, row.Min.y), col::kText, Ellipsize(kLabel, label, label_right - x), row.GetHeight());
}

ItemState MappingChip(const char* id, const ImRect& rect, const ChipLook& look) {
  ItemState state = Item(id, rect, look.enabled);
  constexpr Type kChip{Font::kBold, 14};
  ImU32 text = col::kText;
  if (look.listening) {
    // The mockup's pulse: the chip's opacity between 1 and 0.45 once a second.
    float pulse = 0.725f + 0.275f * std::cos(float(ImGui::GetTime()) * 6.2831853f);
    FillRect(rect, Fade(col::kAccent, pulse), 6.0f);
    text = Fade(col::kOnAccent, pulse);
  } else if (look.empty) {
    DashedRect(rect, state.hovered ? Hex(0xFFFFFF, 102) : Hex(0xFFFFFF, 64), 6.0f);
    text = col::kLabel;
  } else if (!look.enabled) {
    StrokeRect(rect, col::kHairline, 6.0f);
    text = col::kTabText;
  } else {
    FillRect(rect, state.hovered ? Brighten(col::kPanelChip) : col::kPanelChip, 6.0f);
    StrokeRect(rect, Hex(0xFFFFFF, 31), 6.0f);
  }
  // The text and the icon after it, centered.
  float icon = look.icon != Icon::kNone ? Px(16.0f) : 0.0f;
  float gap = icon > 0.0f && !look.text.empty() ? Px(4.0f) : 0.0f;
  std::string shown = Ellipsize(kChip, look.text, rect.GetWidth() - Px(28.0f) - icon - gap);
  float text_width = MeasureText(kChip, shown).x;
  float x = rect.GetCenter().x - (text_width + gap + icon) * 0.5f;
  DrawLabel(kChip, ImVec2(x, rect.Min.y), text, shown, rect.GetHeight());
  if (icon > 0.0f) {
    DrawIcon(look.icon, ImVec2(x + text_width + gap, rect.GetCenter().y - Px(8.0f)), 16.0f,
             look.listening || !look.enabled ? text : look.icon_color, 2.2f);
  }
  if (state.focused) {
    FocusRing(rect, 6.0f);
  }
  return state;
}

bool ClearPressed(const ItemState& state) {
  return (state.focused && (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace) ||
                            ButtonPressed(PadButton::kX))) ||
         (state.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right));
}
bool App::OnPadCapture(const SDL_Event& event) {
  if (pad_listen_ < 0) {
    return false;
  }
  switch (event.type) {
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
      // Only a controller of the type being edited.
      if (SDL_GetGamepadTypeForID(event.gbutton.which) == pad_type_) {
        AssignPad(event.gbutton.button);
      }
      return true;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      if ((event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER || event.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) &&
          event.gaxis.value > 16000 && SDL_GetGamepadTypeForID(event.gaxis.which) == pad_type_) {
        AssignPad(event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER ? kInputLeftTrigger : kInputRightTrigger);
      }
      return true;
    case SDL_EVENT_KEY_DOWN:
      if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
        StopListening();
      }
      return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      StopListening();  // a click anywhere cancels
      return true;
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
    case SDL_EVENT_KEY_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_MOUSE_BUTTON_UP:
      return true;
    default:
      return false;
  }
}

// The input pressed for the control being assigned. A button another control had moves over to it: the two
// swap, so every control stays on some button.
void App::AssignPad(int input) {
  std::string type = TypeKey(pad_type_);
  std::string& remap = settings_.o.gamepad_remap;
  Sources sources = SourcesFor(remap, type, true);
  int control = pad_listen_;
  int old = PrimaryInput(sources[size_t(control)]);
  if (sources[size_t(control)] != Bit(input)) {
    for (int c = 0; c < kControls; ++c) {
      if (c != control && (sources[size_t(c)] & Bit(input))) {
        sources[size_t(c)] = (sources[size_t(c)] & ~Bit(input)) | (old >= 0 && old != input ? Bit(old) : 0);
      }
    }
    sources[size_t(control)] = Bit(input);
    // A control back on its usual button gets all its usual inputs again (A's Elite paddle with South).
    Sources base = SourcesFor(remap, type, false);
    for (int c = 0; c < kControls; ++c) {
      if (PrimaryInput(sources[size_t(c)]) == PrimaryInput(base[size_t(c)])) {
        sources[size_t(c)] = base[size_t(c)];
      }
    }
    remap = WithLayout(remap, type, sources);
    Changed();
  }
  StopListening();
  pad_release_wait_ = true;
}

float App::PadRows(ImVec2 origin, float width, SDL_GamepadType type, SDL_JoystickType kind,
                   const std::string& part_key) {
  std::string type_key = TypeKey(type);
  std::string& remap = settings_.o.gamepad_remap;
  Sources sources = SourcesFor(remap, type_key, true);
  // The game's functions (game.h) on any controller but a drum kit, whose pads are other functions; else the Xbox 360
  // controller's buttons.
  const std::vector<game::GameControlGroup>& functions = game::GameControlsFor(language_);
  bool with_functions = !functions.empty() && kind != SDL_JOYSTICK_TYPE_DRUM_KIT;
  struct Row {
    int control;                         // -1: one the controllers cannot move
    const game::GameControl* function;   // null: the Xbox 360 control of kRowControl[xbox_row]
    int xbox_row;
  };
  std::vector<Row> rows;
  if (with_functions) {
    for (const game::GameControlGroup& group : functions) {
      for (const game::GameControl& control : group.controls) rows.push_back({ControlOf(control.bind), &control, -1});
    }
  } else {
    for (int row = 0; row < kRows; ++row) rows.push_back({kRowControl[row], nullptr, row});
  }
  constexpr ImU32 kGlyphColors[] = {col::kGlyphA, col::kGlyphB, col::kGlyphX, col::kGlyphY};
  float y = origin.y;
  // What it plays (page_microphone.cpp): a gamepad can sing; a guitar and a drum kit play their own.
  if (!part_key.empty() && kind != SDL_JOYSTICK_TYPE_GUITAR && kind != SDL_JOYSTICK_TYPE_DRUM_KIT) {
    y += PartRow(ImVec2(origin.x, y), width, part_key);
    // A controller that sings has no frets to assign.
    if (Sings(part_key)) {
      return y - origin.y;
    }
    y += Px(6.0f);
  }
  // Its frets strum as they are pressed (game.h's kKeyboardTapTarget, the SDK's gamepad_tap_target): one switch for
  // every controller playing guitar, as the keyboard's on its page.
  const game::GameTexts& own = game::GameTextsFor(language_);
  if (*game::kKeyboardTapTarget && own.keyboard_tap_title && kind != SDL_JOYSTICK_TYPE_GUITAR &&
      kind != SDL_JOYSTICK_TYPE_DRUM_KIT) {
    ImRect r(ImVec2(origin.x, y), ImVec2(origin.x + width, y + Px(44.0f)));
    ImVec2 pos(r.Max.x - Px(6.0f) - Px(52.0f), r.GetCenter().y - Px(15.0f));
    MappingRow(r, 0, own.keyboard_tap_title, pos.x - Px(14.0f), false);
    ImGui::PushID("gamepad_tap");
    if (Toggle("##toggle", pos, &settings_.o.gamepad_tap)) {
      Changed();
    }
    ImGui::PopID();
    if (!mapping_first_) mapping_first_ = ImGui::GetItemID();
    mapping_rows_.push_back({ImGui::GetItemID(), ImGui::GetItemID()});
    y += Px(44.0f) + Px(6.0f);
  }
  for (int i = 0; i < int(rows.size()); ++i) {
    const Row& row = rows[size_t(i)];
    ImRect r(ImVec2(origin.x, y), ImVec2(origin.x + width, y + Px(44.0f)));
    ImRect chip(ImVec2(r.Max.x - Px(6.0f) - Px(190.0f), r.Min.y + Px(4.0f)), ImVec2(r.Max.x - Px(6.0f), r.Max.y - Px(4.0f)));
    bool listening = row.control >= 0 && pad_listen_ == row.control;
    // A fret's color, or A, B, X and Y's; an empty ring for the rest.
    ImU32 swatch = row.function ? (*row.function->chip ? 0 : Hex(row.function->chip_color))
                                : row.xbox_row < 4 ? kGlyphColors[row.xbox_row] : 0;
    MappingRow(r, swatch, row.function ? row.function->name : t_.pad_buttons[row.xbox_row], chip.Min.x - Px(14.0f),
               listening);
    ChipLook look;
    int input = row.control >= 0 ? PrimaryInput(sources[size_t(row.control)]) : -1;
    if (listening) {
      look.text = t_.press_button;
      look.listening = true;
    } else if (row.control < 0) {
      // The D-pad and the sticks stay: their name on the controller.
      look.text = row.function ? (row.function->on_pad ? row.function->on_pad : row.function->name)
                               : t_.pad_buttons[12];
      look.enabled = false;
    } else if (input < 0) {
      look.text = t_.unbound;
      look.empty = true;
    } else {
      Label label = LabelOf(input, type);
      look.text = label.text;
      look.icon = label.icon;
      look.icon_color = label.color;
    }
    ImGui::PushID(i);
    ItemState state = MappingChip("##chip", chip, look);
    ImGui::PopID();
    if (look.enabled) {
      if (!mapping_first_) mapping_first_ = ImGui::GetItemID();
      mapping_rows_.push_back({ImGui::GetItemID(), ImGui::GetItemID()});
    }
    if (state.pressed) {
      StopListening();
      if (!listening) {
        pad_listen_ = row.control;
      }
    } else if (row.control >= 0 && !listening && input >= 0 && ClearPressed(state)) {
      // Cleared: no button presses it.
      sources[size_t(row.control)] = 0;
      remap = WithLayout(remap, type_key, sources);
      Changed();
    }
    y += Px(44.0f) + Px(6.0f);
  }
  return y - Px(6.0f) - origin.y;
}

void App::ControlsPage(const ImRect& rect) {
  ImGuiContext& g = *GImGui;
  std::vector<Pad> pads = ConnectedPads();
  // The chosen device: the controller picked while it stays connected, else the first guitar, drum kit or other
  // controller (as the footer names them); the keyboard when it was picked or when no controller is connected.
  auto chosen = std::find_if(pads.begin(), pads.end(), [&](const Pad& p) { return p.id == selected_device_; });
  if (chosen == pads.end()) {
    auto rank = [](const Pad& p) {
      return p.kind == SDL_JOYSTICK_TYPE_GUITAR ? 0 : p.kind == SDL_JOYSTICK_TYPE_DRUM_KIT ? 1 : 2;
    };
    chosen = std::min_element(pads.begin(), pads.end(), [&](const Pad& a, const Pad& b) { return rank(a) < rank(b); });
  }
  bool microphone = game::kHasMicrophones && microphone_selected_;
  const Pad* pad = microphone || keyboard_selected_ || pads.empty() ? nullptr : &*chosen;
  SDL_GamepadType type = pad ? pad->type : SDL_GAMEPAD_TYPE_UNKNOWN;
  if (type != pad_type_) {
    if (pad_listen_ >= 0) StopListening();
    pad_type_ = type;
  }
  auto name_of = [&](const Pad& p) { return p.name.empty() ? std::string(TypeName(p.type, t_)) : p.name; };
  auto kind_of = [&](SDL_JoystickType kind) {
    return kind == SDL_JOYSTICK_TYPE_GUITAR     ? t_.help_guitar_kicker
           : kind == SDL_JOYSTICK_TYPE_DRUM_KIT ? t_.help_drums_kicker
                                                : t_.help_pad_kicker;
  };
  DeviceLook look = microphone ? kMicrophoneLook : LookOf(pad ? pad->kind : SDL_JOYSTICK_TYPE_GAMEPAD, !pad);

  // --- The layout: 24 40 20 dp of padding, the devices 280 dp wide, the help 300, 20 dp between ---
  ImRect main(ImVec2(rect.Min.x + Px(40.0f), rect.Min.y + Px(24.0f)), ImVec2(rect.Max.x - Px(40.0f), rect.Max.y - Px(20.0f)));
  ImRect devices(main.Min, ImVec2(main.Min.x + Px(280.0f), main.Max.y));
  ImRect help(ImVec2(main.Max.x - Px(300.0f), main.Min.y), main.Max);
  ImRect mapping(ImVec2(devices.Max.x + Px(20.0f), main.Min.y), ImVec2(help.Min.x - Px(20.0f), main.Max.y));

  // The devices: the title, a button each (the chosen one on accent), the note at the foot.
  ImGuiID device_focus = 0;
  std::vector<ImGuiID> device_ids;
  // A device chosen with A (or right on one): its mapping is drawn now; the focus goes to its first row after it, once
  // A is up (ImGui keeps the focus on an item held active).
  bool focus_mapping = controls_focus_mapping_ && g.ActiveId == 0;
  if (focus_mapping) controls_focus_mapping_ = false;
  mapping_first_ = 0;
  mapping_rows_.clear();
  {
    constexpr Type kTitle{Font::kDisplayBold, 40};
    DrawLabel(kTitle, devices.Min, col::kText, Upper(t_.nav_controls), Px(40.0f));
    float y = devices.Min.y + Px(40.0f) + Px(6.0f) + Px(10.0f) + Px(6.0f);
    constexpr Type kSection{Font::kBold, 12, 0.12f};
    DrawLabel(kSection, ImVec2(devices.Min.x, y), col::kTabText, Upper(t_.devices_detected));
    y += LineHeight(kSection) + Px(10.0f);
    constexpr Type kName{Font::kBold, 15};
    constexpr Type kStatus{Font::kRegular, 13};
    float height = Px(66.0f);
    int entries = int(pads.size()) + 1 + (game::kHasMicrophones ? 1 : 0);
    for (int i = 0; i < entries; ++i) {
      bool keyboard = i == int(pads.size());
      bool mic = i == int(pads.size()) + 1;
      const Pad* p = keyboard || mic ? nullptr : &pads[size_t(i)];
      ImRect r(ImVec2(devices.Min.x, y), ImVec2(devices.Max.x, y + height));
      if (keyboard && pads.empty()) {
        // No controller: a dashed place for one above the keyboard.
        DashedRect(r, Hex(0xFFFFFF, 46), 10.0f);
        DrawLabel(kStatus, ImVec2(r.GetCenter().x - MeasureText(kStatus, t_.no_controller_title).x * 0.5f, r.Min.y),
                  col::kLabel, t_.no_controller_title, r.GetHeight());
        y += height + Px(10.0f);
        r = ImRect(ImVec2(devices.Min.x, y), ImVec2(devices.Max.x, y + height));
      }
      ImGui::PushID(mic ? -2 : keyboard ? -1 : int(p->id));
      ItemState state = Item("##device", r);
      ImGuiID id = ImGui::GetItemID();
      ImGui::PopID();
      device_ids.push_back(id);
      bool on = mic ? microphone : keyboard ? !pad && !microphone : pad == p;
      if (on) {
        device_focus = id;
      }
      if (state.pressed) {
        if (!on) {
          StopListening();
          microphone_selected_ = mic;
          keyboard_selected_ = keyboard;
          if (p) selected_device_ = p->id;
        }
        controls_focus_mapping_ = PressedByNav();
      }
      ImU32 background = on ? Fade(col::kAccent, 0.14f) : col::kPanel;
      FillRect(r, state.hovered ? Brighten(background) : background, 10.0f);
      StrokeRect(r, col::kHairline, 10.0f);
      if (on) {
        // "outline: 3px solid accent; outline-offset: 2px".
        StrokeRect(ImRect(ImVec2(r.Min.x - Px(3.5f), r.Min.y - Px(3.5f)), ImVec2(r.Max.x + Px(3.5f), r.Max.y + Px(3.5f))),
                   col::kAccent, 13.5f, 3.0f);
      }
      DeviceLook device = mic ? kMicrophoneLook : LookOf(p ? p->kind : SDL_JOYSTICK_TYPE_GAMEPAD, keyboard);
      ImRect tile(ImVec2(r.Min.x + Px(14.0f), r.GetCenter().y - Px(20.0f)),
                  ImVec2(r.Min.x + Px(54.0f), r.GetCenter().y + Px(20.0f)));
      FillRect(tile, device.tile, 8.0f);
      DrawIcon(device.icon, ImVec2(tile.Min.x + Px(8.0f), tile.Min.y + Px(8.0f)), 24.0f, device.ink, 1.6f);
      // The name, and under it a dot and the status: connected (green) with its kind, or the keyboard on or off.
      float text_x = tile.Max.x + Px(12.0f);
      float text_width = r.Max.x - Px(14.0f) - text_x;
      std::string name = keyboard ? std::string(t_.device_keyboard) : mic ? std::string(t_.device_microphone) : name_of(*p);
      std::string status;
      ImU32 dot = col::kSuccess;
      if (mic) {
        status = MicStatus(&dot);
      } else if (keyboard) {
        status = settings_.o.mnk ? t_.keyboard_status_on : t_.keyboard_status_off;
        dot = settings_.o.mnk ? col::kSuccess : col::kTextDim;
      } else {
        status = kind_of(p->kind);
        const char* type_name = TypeName(p->type, t_);
        if (type_name != t_.controller) status += std::string(" \xC2\xB7 ") + type_name;
      }
      // The singer's controller says so (its part: page_microphone.cpp).
      if (game::kHasMicrophones && !mic && (keyboard ? settings_.o.mnk && Sings("keyboard")
                                                     : p->kind == SDL_JOYSTICK_TYPE_GAMEPAD && Sings(PadKey(p->id)))) {
        const game::GameTexts& own = game::GameTextsFor(language_);
        status += std::string(" \xC2\xB7 ") + (own.part_vocals ? own.part_vocals : t_.part_vocals);
      }
      float block = LineHeight(kName) + Px(2.0f) + LineHeight(kStatus);
      float top = r.GetCenter().y - block * 0.5f;
      DrawLabel(kName, ImVec2(text_x, top), col::kText, Ellipsize(kName, name, text_width));
      float status_y = top + LineHeight(kName) + Px(2.0f);
      FillRect(ImRect(ImVec2(text_x, status_y + LineHeight(kStatus) * 0.5f - Px(4.0f)),
                      ImVec2(text_x + Px(8.0f), status_y + LineHeight(kStatus) * 0.5f + Px(4.0f))),
               dot, 4.0f);
      DrawLabel(kStatus, ImVec2(text_x + Px(14.0f), status_y), col::kTabText,
                Ellipsize(kStatus, status, text_width - Px(14.0f)));
      if (state.focused) {
        FocusRing(r, 10.0f);
      }
      y += height + Px(10.0f);
    }
    // The controller starts on the chosen device; up and down go through the devices, right to the mapping.
    page_focus_ = device_focus;
    std::vector<std::array<ImGuiID, 2>> column;
    for (ImGuiID device : device_ids) column.push_back({device, device});
    ColumnNav(column);
    auto focused = std::find(device_ids.begin(), device_ids.end(), g.NavId);
    if (g.NavMoveSubmitted && g.NavMoveDir == ImGuiDir_Right && focused != device_ids.end()) {
      // Right chooses the device in focus, as A does, and goes to its mapping's first row next frame (its id is known
      // once that mapping is drawn).
      size_t index = size_t(focused - device_ids.begin());
      bool keyboard = index == pads.size();
      bool mic = index == pads.size() + 1;
      if (mic != microphone || keyboard != keyboard_selected_ ||
          (!keyboard && !mic && pads[index].id != selected_device_)) {
        StopListening();
        microphone_selected_ = mic;
        keyboard_selected_ = keyboard;
        if (!keyboard && !mic) selected_device_ = pads[index].id;
      }
      controls_focus_mapping_ = true;
      ImGui::NavMoveRequestCancel();
    }
    // "Connect another guitar…" at the foot.
    constexpr Type kNote{Font::kRegular, 13};
    float note_height = DrawParagraph(kNote, ImVec2(0, 0), devices.GetWidth() - Px(28.0f), 1.45f, 0, t_.connect_more, false);
    ImRect note(ImVec2(devices.Min.x, devices.Max.y - note_height - Px(24.0f)), devices.Max);
    FillRect(note, col::kPanel, 10.0f);
    StrokeRect(note, col::kHairline, 10.0f);
    DrawParagraph(kNote, ImVec2(note.Min.x + Px(14.0f), note.Min.y + Px(12.0f)), note.GetWidth() - Px(28.0f), 1.45f,
                  col::kTabText, t_.connect_more);
  }

  // The mapping: the title, the hint, the rows (scrolling when they do not fit).
  FillRect(mapping, col::kPanel, 14.0f);
  StrokeRect(mapping, col::kHairline, 14.0f);
  {
    float x = mapping.Min.x + Px(20.0f);
    float width = mapping.GetWidth() - Px(40.0f);
    float y = mapping.Min.y + Px(20.0f);
    constexpr Type kTitle{Font::kDisplayBold, 24};
    char title[192];
    if (microphone) {
      std::snprintf(title, sizeof(title), "%s", t_.device_microphone);
    } else {
      std::snprintf(title, sizeof(title), t_.mapping_of, pad ? name_of(*pad).c_str() : t_.device_keyboard);
    }
    DrawLabel(kTitle, ImVec2(x, y), col::kText, Ellipsize(kTitle, Upper(title), width));
    if (microphone) {
      // Singing is experimental: a badge after the title, as a Settings value's.
      constexpr Type kTag{Font::kBold, 10, 0.08f};
      std::string tag = Upper(t_.experimental);
      float tag_x = x + MeasureText(kTitle, Upper(title)).x + Px(12.0f);
      ImRect badge(ImVec2(tag_x, y + (LineHeight(kTitle) - Px(18.0f)) * 0.5f),
                   ImVec2(tag_x + MeasureText(kTag, tag).x + Px(12.0f), y + (LineHeight(kTitle) + Px(18.0f)) * 0.5f));
      FillRect(badge, Fade(col::kGlyphY, 0.18f), 4.0f);
      DrawLabel(kTag, ImVec2(badge.Min.x + Px(6.0f), badge.Min.y), col::kGlyphY, tag, badge.GetHeight());
    }
    y += LineHeight(kTitle);
    // The hint (the microphone's: who sings); for a moment after Shift, Ctrl or Alt was pressed alone, why it was not
    // taken.
    constexpr Type kHint{Font::kRegular, 13};
    bool notice = !pad && !microphone && std::chrono::steady_clock::now() < notice_until_;
    std::string hint = microphone ? SingersText() : pad ? t_.mapping_select_hint : t_.mapping_select_key_hint;
    if (game::kHasMicrophones && pad && pad->kind != SDL_JOYSTICK_TYPE_GUITAR &&
        pad->kind != SDL_JOYSTICK_TYPE_DRUM_KIT && Sings(PadKey(pad->id))) {
      // The singer's controller has nothing to assign: the microphone it sings with.
      ImU32 dot = 0;
      hint = std::string(t_.device_microphone) + ": " + MicStatus(&dot);
    }
    DrawLabel(kHint, ImVec2(x, y), notice ? col::kAccentLight : col::kTabText,
              Ellipsize(kHint, notice ? notice_ : hint, width));
    y += LineHeight(kHint) + Px(12.0f);
    // 3 dp around the rows for the focus outline.
    ImRect list(ImVec2(x - Px(3.0f), y), ImVec2(mapping.Max.x - Px(17.0f), mapping.Max.y - Px(17.0f)));
    BeginScroll("##mapping", list);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float rows_width = list.GetWidth() - Px(6.0f) - (ImGui::GetCurrentWindow()->ScrollbarY ? Px(12.0f) : 0.0f);
    ImVec2 rows_origin(origin.x + Px(3.0f), origin.y + Px(3.0f));
    float height = microphone ? MicRows(rows_origin, rows_width)
                   : pad      ? PadRows(rows_origin, rows_width, pad->type, pad->kind,
                                        game::kHasMicrophones ? PadKey(pad->id) : std::string())
                              : KeyRows(rows_origin, rows_width);
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(list.GetWidth(), height + Px(6.0f)));
    // The rows: up and down in their column; left from a row's first control (or B) back to the device.
    ColumnNav(mapping_rows_);
    if (g.NavWindow == ImGui::GetCurrentWindow()) {
      back_focus_ = device_focus;
      bool first_column = std::any_of(mapping_rows_.begin(), mapping_rows_.end(),
                                      [&](const std::array<ImGuiID, 2>& row) { return row[0] == g.NavId; });
      if (g.NavMoveSubmitted && g.NavMoveDir == ImGuiDir_Left && first_column) {
        RequestFocus(device_focus);
        ImGui::NavMoveRequestCancel();
      }
    }
    EndScroll();
    if (focus_mapping && mapping_first_) {
      RequestFocus(mapping_first_);
    }
  }

  // The help: the device's kind, what to do, its tips; Reset (Y) and "saved automatically" at the foot.
  FillRect(help, col::kPanel, 14.0f);
  StrokeRect(help, Hex(0xFFFFFF, 26), 14.0f);
  {
    ImVec2 p(help.Min.x + Px(22.0f), help.Min.y + Px(22.0f));
    float width = help.GetWidth() - Px(44.0f);
    SDL_JoystickType kind = pad ? pad->kind : SDL_JOYSTICK_TYPE_GAMEPAD;
    bool guitar = kind == SDL_JOYSTICK_TYPE_GUITAR;
    bool drums = kind == SDL_JOYSTICK_TYPE_DRUM_KIT;
    bool singer = game::kHasMicrophones && pad && !guitar && !drums && Sings(PadKey(pad->id));
    const char* kicker = microphone ? t_.help_mic_kicker : !pad ? t_.help_keyboard_kicker
                         : guitar ? t_.help_guitar_kicker : drums ? t_.help_drums_kicker : t_.help_pad_kicker;
    const char* title = microphone ? t_.help_mic_title : !pad ? t_.help_keyboard_title
                        : singer ? t_.help_vocals_title : guitar ? t_.help_guitar_title
                        : drums ? t_.help_drums_title : t_.help_pad_title;
    const char* body = microphone ? t_.help_mic_text : !pad ? t_.help_keyboard_text
                       : singer ? t_.help_vocals_text : guitar ? t_.help_guitar_text
                       : drums ? t_.help_drums_text : t_.help_pad_text;
    std::vector<const char*> tips;
    if (microphone) {
      tips = {t_.help_mic_tip_level, t_.help_mic_tip_channel, t_.help_mic_tip_privacy};
    } else if (singer) {
      tips = {t_.help_tip_part};
    } else if (pad) {
      if (game::kHasMicrophones && !guitar && !drums) tips.push_back(t_.help_tip_part);
      if (*game::kKeyboardTapTarget && !guitar && !drums) tips.push_back(t_.help_tip_gamepad_tap);
      // gamepad_remap keeps one layout per SDL type: a guitar and a drum kit are Xbox 360 controllers to it.
      tips.push_back(type == SDL_GAMEPAD_TYPE_XBOX360 ? t_.help_tip_shared_360 : t_.help_tip_same_type);
      tips.push_back(t_.help_tip_fixed);
    } else {
      if (game::kHasMicrophones) tips.push_back(t_.help_tip_part);
      tips.push_back(t_.mnk_hint);
      // The game's own switch (game.h, kKeyboardTapTarget): what it does.
      const game::GameTexts& own = game::GameTextsFor(language_);
      if (*game::kKeyboardTapTarget && own.keyboard_tap_hint) tips.push_back(own.keyboard_tap_hint);
      tips.push_back(t_.help_tip_escape);
      tips.push_back(t_.help_tip_clear);
    }
    constexpr Type kKicker{Font::kBold, 12, 0.12f};
    DrawLabel(kKicker, p, look.ink, Upper(kicker));
    p.y += LineHeight(kKicker) + Px(8.0f);
    constexpr Type kHelpTitle{Font::kDisplayBold, 26};
    p.y += DrawParagraph(kHelpTitle, p, width, 1.1f, col::kText, title) + Px(14.0f);
    constexpr Type kBody{Font::kRegular, 15};
    p.y += DrawParagraph(kBody, p, width, 1.5f, col::kTextSoft, body) + Px(14.0f);
    constexpr Type kTip{Font::kRegular, 14};
    for (const char* tip : tips) {
      float line = Px(14.0f) * 1.4f;
      FillRect(ImRect(ImVec2(p.x, p.y + line * 0.5f - Px(4.0f)), ImVec2(p.x + Px(8.0f), p.y + line * 0.5f + Px(4.0f))),
               col::kAccent);
      p.y += DrawParagraph(kTip, ImVec2(p.x + Px(16.0f), p.y), width - Px(16.0f), 1.4f, col::kTextSoft, tip) + Px(8.0f);
    }

    constexpr Type kNote{Font::kRegular, 13};
    float note_y = help.Max.y - Px(22.0f) - LineHeight(kNote);
    DrawLabel(kNote, ImVec2(help.GetCenter().x - MeasureText(kNote, t_.autosaved).x * 0.5f, note_y), col::kLabel,
              t_.autosaved);
    ImRect button(ImVec2(p.x, note_y - Px(8.0f) - Px(46.0f)), ImVec2(p.x + width, note_y - Px(8.0f)));
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    ItemState state = Item("##reset", button);
    ImGui::PopItemFlag();
    FillRect(button, state.hovered ? Brighten(col::kGhostFill) : col::kGhostFill, 10.0f);
    StrokeRect(button, col::kGhostBorder, 10.0f);
    constexpr Type kReset{Font::kBold, 15};
    float glyph = GlyphWidth(PadButton::kY, 24.0f);
    float content = glyph + Px(10.0f) + MeasureText(kReset, t_.reset_defaults).x;
    float gx = button.GetCenter().x - content * 0.5f;
    DrawGlyph(PadButton::kY, ImVec2(gx, button.GetCenter().y - Px(12.0f)), 24.0f);
    DrawLabel(kReset, ImVec2(gx + glyph + Px(10.0f), button.Min.y), col::kText, t_.reset_defaults, button.GetHeight());
    bool reset = state.pressed || (!Listening() && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                                   ButtonPressed(PadButton::kY));
    if (reset) {
      StopListening();
      if (microphone) {
        settings_.o.microphones = {};
        settings_.o.microphone_channels = {};
        settings_.o.microphone_gains = {game::kMicrophoneGainDefault, game::kMicrophoneGainDefault};
      } else if (pad) {
        // Back to the standard layout, for this type of controller.
        std::string type_key = TypeKey(type);
        settings_.o.gamepad_remap = WithLayout(settings_.o.gamepad_remap, type_key,
                                               SourcesFor(settings_.o.gamepad_remap, type_key, false));
      } else {
        ResetKeyboard(settings_.o);
      }
      Changed();
    }
  }
}

}  // namespace launcher
