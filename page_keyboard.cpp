// The Controls tab's keyboard (page_controls.cpp draws the tab): playing with the keyboard and the mouse (the SDK's
// mnk driver), the keys of each of the game's functions or Xbox 360 controls (keybind_*: a primary and a secondary key
// each) and the mouse's camera.
//
// The keys are the SDK's names (thirdparty/rexglue-sdk/src/ui/keybinds.cpp) of physical keys: the SDK maps
// SDL scancodes, as on a US keyboard (src/ui/sdl_virtual_key.cpp). The page shows each with its label in the
// user's layout. A bind may need Shift, Ctrl or Alt held ("Shift+Up"); a bare modifier never fires there, so
// the page does not accept one alone.

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

constexpr uint8_t kShift = 1;
constexpr uint8_t kCtrl = 2;
constexpr uint8_t kAlt = 4;

uint8_t ModifierBit(SDL_Scancode scancode) {
  switch (scancode) {
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
      return kShift;
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
      return kCtrl;
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
      return kAlt;
    default:
      return 0;
  }
}

// The SDK's key names by scancode: the ones keybinds.cpp parses that sdl_virtual_key.cpp produces.
const std::map<SDL_Scancode, std::string>& KeyNames() {
  static const std::map<SDL_Scancode, std::string> names = [] {
    std::map<SDL_Scancode, std::string> m;
    for (int i = 0; i < 26; ++i) m[SDL_Scancode(SDL_SCANCODE_A + i)] = std::string(1, char('A' + i));
    for (int i = 0; i < 9; ++i) m[SDL_Scancode(SDL_SCANCODE_1 + i)] = std::string(1, char('1' + i));
    m[SDL_SCANCODE_0] = "0";
    for (int i = 0; i < 12; ++i) m[SDL_Scancode(SDL_SCANCODE_F1 + i)] = "F" + std::to_string(i + 1);
    for (int i = 0; i < 12; ++i) m[SDL_Scancode(SDL_SCANCODE_F13 + i)] = "F" + std::to_string(i + 13);
    for (int i = 0; i < 9; ++i) m[SDL_Scancode(SDL_SCANCODE_KP_1 + i)] = "Numpad" + std::to_string(i + 1);
    m[SDL_SCANCODE_KP_0] = "Numpad0";
    m[SDL_SCANCODE_GRAVE] = "Backtick";
    m[SDL_SCANCODE_MINUS] = "Minus";
    m[SDL_SCANCODE_EQUALS] = "Plus";
    m[SDL_SCANCODE_COMMA] = "Comma";
    m[SDL_SCANCODE_PERIOD] = "Period";
    m[SDL_SCANCODE_SEMICOLON] = "Semicolon";
    m[SDL_SCANCODE_SLASH] = "Slash";
    m[SDL_SCANCODE_BACKSLASH] = "Backslash";
    m[SDL_SCANCODE_LEFTBRACKET] = "LBracket";
    m[SDL_SCANCODE_RIGHTBRACKET] = "RBracket";
    m[SDL_SCANCODE_APOSTROPHE] = "Quote";
    m[SDL_SCANCODE_RETURN] = "Return";
    m[SDL_SCANCODE_KP_ENTER] = "NumpadEnter";
    m[SDL_SCANCODE_SPACE] = "Space";
    m[SDL_SCANCODE_TAB] = "Tab";
    m[SDL_SCANCODE_BACKSPACE] = "Backspace";
    m[SDL_SCANCODE_DELETE] = "Delete";
    m[SDL_SCANCODE_INSERT] = "Insert";
    m[SDL_SCANCODE_HOME] = "Home";
    m[SDL_SCANCODE_END] = "End";
    m[SDL_SCANCODE_PAGEUP] = "PageUp";
    m[SDL_SCANCODE_PAGEDOWN] = "PageDown";
    m[SDL_SCANCODE_LEFT] = "Left";
    m[SDL_SCANCODE_RIGHT] = "Right";
    m[SDL_SCANCODE_UP] = "Up";
    m[SDL_SCANCODE_DOWN] = "Down";
    m[SDL_SCANCODE_KP_PLUS] = "NumpadPlus";
    m[SDL_SCANCODE_KP_MINUS] = "NumpadMinus";
    m[SDL_SCANCODE_KP_MULTIPLY] = "NumpadStar";
    m[SDL_SCANCODE_KP_DIVIDE] = "NumpadSlash";
    m[SDL_SCANCODE_PRINTSCREEN] = "PrintScreen";
    m[SDL_SCANCODE_PAUSE] = "Pause";
    m[SDL_SCANCODE_CAPSLOCK] = "CapsLock";
    m[SDL_SCANCODE_NUMLOCKCLEAR] = "NumLock";
    m[SDL_SCANCODE_SCROLLLOCK] = "ScrollLock";
    return m;
  }();
  return names;
}

// A key as the page shows it: text, and an arrow icon after it for the arrow keys.
struct KeyLabel {
  std::string text;
  Icon icon = Icon::kNone;
};

KeyLabel LabelOf(std::string_view token, const Texts& t) {
  KeyLabel label;
  // The modifiers first, as the SDK parses them.
  for (;;) {
    size_t plus = token.find('+');
    if (plus == std::string_view::npos || plus == 0) break;
    std::string_view head = token.substr(0, plus);
    const char* name = head == "Shift" ? t.key_shift
                       : head == "Ctrl" || head == "Control" ? t.key_ctrl
                       : head == "Alt" ? t.key_alt
                                       : nullptr;
    if (!name) break;
    label.text += std::string(name) + " + ";
    token.remove_prefix(plus + 1);
  }
  struct Named {
    std::string_view name;
    const char* Texts::*text;
  };
  static constexpr Named kNamed[] = {
      {"Space", &Texts::key_space},       {"Return", &Texts::key_enter},
      {"Backspace", &Texts::key_backspace}, {"Delete", &Texts::key_delete},
      {"Insert", &Texts::key_insert},     {"Home", &Texts::key_home},
      {"End", &Texts::key_end},           {"PageUp", &Texts::key_page_up},
      {"PageDown", &Texts::key_page_down}, {"CapsLock", &Texts::key_caps_lock},
      {"NumLock", &Texts::key_num_lock},  {"ScrollLock", &Texts::key_scroll_lock},
      {"PrintScreen", &Texts::key_print_screen}, {"Pause", &Texts::key_pause},
      {"LMB", &Texts::mouse_left},        {"RMB", &Texts::mouse_right},
      {"MMB", &Texts::mouse_middle},
  };
  for (const Named& named : kNamed) {
    if (token == named.name) {
      label.text += t.*named.text;
      return label;
    }
  }
  if (token == "Up" || token == "Down" || token == "Left" || token == "Right") {
    label.icon = token == "Up" ? Icon::kArrowUp : token == "Down" ? Icon::kArrowDown
                 : token == "Left" ? Icon::kArrowLeft : Icon::kArrowRight;
    return label;
  }
  if (token == "Tab" || token == "Escape") {
    label.text += token == "Tab" ? "Tab" : "Esc";
    return label;
  }
  if (token.starts_with("Numpad")) {
    std::string_view rest = token.substr(6);
    std::string key = rest == "Enter" ? t.key_enter : rest == "Plus" ? "+" : rest == "Minus" ? "-"
                      : rest == "Star" ? "*" : rest == "Slash" ? "/" : std::string(rest);
    label.text += std::string(t.key_numpad) + " " + key;
    return label;
  }
  if (token.size() >= 2 && token[0] == 'F' && token[1] >= '0' && token[1] <= '9') {
    label.text += token;
    return label;
  }
  // Letters, digits and punctuation: the character on that key in the user's layout.
  for (const auto& [scancode, name] : KeyNames()) {
    if (name == token) {
      SDL_Keycode key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
      const char* text = SDL_GetKeyName(key);
      label.text += text && *text ? text : name;
      return label;
    }
  }
  label.text += token;
  return label;
}

std::vector<std::string> Alternatives(const std::string& list) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start <= list.size()) {
    size_t comma = list.find(',', start);
    std::string item = list.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
    size_t first = item.find_first_not_of(' ');
    size_t last = item.find_last_not_of(' ');
    out.push_back(first == std::string::npos ? std::string() : item.substr(first, last - first + 1));
    if (comma == std::string::npos) break;
    start = comma + 1;
  }
  return out;
}

std::string Join(const std::vector<std::string>& items) {
  std::string out;
  for (const std::string& item : items) {
    if (item.empty()) continue;
    if (!out.empty()) out += ",";
    out += item;
  }
  return out;
}

// A, B, X and Y's colors for their rows' swatch; 0 (an empty ring) for the other controls.
ImU32 SwatchOf(Bind bind) {
  switch (bind) {
    case Bind::kA:
      return col::kGlyphA;
    case Bind::kB:
      return col::kGlyphB;
    case Bind::kX:
      return col::kGlyphX;
    case Bind::kY:
      return col::kGlyphY;
    default:
      return 0;
  }
}

}  // namespace

void App::StartListening(int bind, int slot) {
  listen_bind_ = bind;
  listen_slot_ = slot;
  held_modifiers_ = 0;
  modifier_pressed_ = false;
}

void App::StopListening() {
  listen_bind_ = -1;
  pad_listen_ = -1;
  held_modifiers_ = 0;
  modifier_pressed_ = false;
}

void App::Assign(std::string token) {
  std::string& list = settings_.o.binds[size_t(listen_bind_)];
  std::vector<std::string> alternatives = Alternatives(list);
  while (int(alternatives.size()) <= listen_slot_) {
    alternatives.emplace_back();
  }
  alternatives[size_t(listen_slot_)] = std::move(token);
  list = Join(alternatives);
  StopListening();
  Changed();
}

bool App::OnKeyCapture(const SDL_Event& event) {
  if (listen_bind_ < 0) {
    return false;
  }
  auto prefix = [this] {
    std::string text;
    if (held_modifiers_ & kShift) text += "Shift+";
    if (held_modifiers_ & kCtrl) text += "Ctrl+";
    if (held_modifiers_ & kAlt) text += "Alt+";
    return text;
  };
  switch (event.type) {
    case SDL_EVENT_KEY_DOWN: {
      if (event.key.repeat) {
        return true;
      }
      if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
        StopListening();
        return true;
      }
      if (uint8_t bit = ModifierBit(event.key.scancode)) {
        held_modifiers_ |= bit;
        modifier_pressed_ = true;
        return true;
      }
      auto it = KeyNames().find(event.key.scancode);
      if (it != KeyNames().end()) {
        Assign(prefix() + it->second);
      }
      return true;  // a key the game cannot use: still waiting
    }
    case SDL_EVENT_KEY_UP:
      if (uint8_t bit = ModifierBit(event.key.scancode)) {
        held_modifiers_ &= uint8_t(~bit);
        if (!held_modifiers_ && modifier_pressed_) {
          // Shift, Ctrl or Alt pressed and released alone.
          modifier_pressed_ = false;
          notice_ = t_.modifier_alone;
          notice_until_ = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        }
      }
      return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN: {
      const char* name = event.button.button == SDL_BUTTON_LEFT     ? "LMB"
                         : event.button.button == SDL_BUTTON_RIGHT  ? "RMB"
                         : event.button.button == SDL_BUTTON_MIDDLE ? "MMB"
                                                                    : nullptr;
      if (name) {
        Assign(prefix() + name);
      }
      return true;
    }
    case SDL_EVENT_MOUSE_BUTTON_UP:
    case SDL_EVENT_TEXT_INPUT:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
      return true;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
      if (event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST) {
        StopListening();
      }
      return true;
    default:
      return false;
  }
}

float App::KeyRows(ImVec2 origin, float width) {
  Options& o = settings_.o;
  ImGuiContext& g = *GImGui;
  bool changed = false;
  float y = origin.y;
  constexpr float kRow = 44.0f;
  constexpr float kGap = 6.0f;
  constexpr Type kGroup{Font::kBold, 12, 0.14f};
  auto group = [&](const char* title) {
    y += Px(10.0f);
    DrawLabel(kGroup, ImVec2(origin.x + Px(14.0f), y), col::kAccent, Upper(title));
    y += LineHeight(kGroup) + Px(8.0f);
  };
  // A switch: playing with the keyboard and the mouse at all, the game's own (game.h, kKeyboardTapTarget: the frets
  // strum as they are pressed), the mouse's camera.
  auto switch_row = [&](const char* id, const char* label, bool* value) {
    ImRect r(ImVec2(origin.x, y), ImVec2(origin.x + width, y + Px(kRow)));
    ImVec2 pos(r.Max.x - Px(6.0f) - Px(52.0f), r.GetCenter().y - Px(15.0f));
    MappingRow(r, 0, label, pos.x - Px(14.0f), false);
    if (Toggle(id, pos, value)) {
      changed = true;
    }
    if (!mapping_first_) {
      mapping_first_ = ImGui::GetItemID();
    }
    mapping_rows_.push_back({ImGui::GetItemID(), ImGui::GetItemID()});
    y += Px(kRow) + Px(kGap);
  };
  switch_row("mnk", t_.mnk, &o.mnk);
  const game::GameTexts& own = game::GameTextsFor(language_);
  if (*game::kKeyboardTapTarget && own.keyboard_tap_title) {
    switch_row("tap", own.keyboard_tap_title, &o.keyboard_tap);
  }
  // What the keyboard's controller plays (page_microphone.cpp): the instrument or the vocals.
  if (game::kHasMicrophones) {
    y += PartRow(ImVec2(origin.x, y), width, "keyboard") + Px(kGap);
  }

  // The rows: the game's functions in its groups (game.h, GameControlsFor), then the controls no function is on; or
  // the Xbox 360 controls in theirs. Each has a primary and a secondary key.
  struct Row {
    int bind;
    const game::GameControl* function;  // null: the Xbox 360 control itself
  };
  struct Group {
    const char* title;
    std::vector<Row> rows;
  };
  std::vector<Group> groups;
  const std::vector<game::GameControlGroup>& functions = game::GameControlsFor(language_);
  if (functions.empty()) {
    auto range = [](const char* title, Bind first, Bind last) {
      Group group{title, {}};
      for (int b = int(first); b <= int(last); ++b) group.rows.push_back({b, nullptr});
      return group;
    };
    groups.push_back(range(t_.group_movement, Bind::kLStickUp, Bind::kLStickPress));
    groups.push_back(range(t_.group_camera, Bind::kRStickUp, Bind::kRStickPress));
    groups.push_back(range(t_.group_buttons, Bind::kA, Bind::kRightTrigger));
    groups.push_back(range(t_.group_dpad, Bind::kDpadUp, Bind::kBack));
  } else {
    bool used[int(Bind::kCount)] = {};
    for (const game::GameControlGroup& g_group : functions) {
      Group group{g_group.title, {}};
      for (const game::GameControl& control : g_group.controls) {
        group.rows.push_back({control.bind, &control});
        used[control.bind] = true;
      }
      groups.push_back(std::move(group));
    }
    Group other{t_.group_other, {}};
    for (int b = 0; b < int(Bind::kCount); ++b) {
      if (!used[b]) other.rows.push_back({b, nullptr});
    }
    if (!other.rows.empty()) groups.push_back(std::move(other));
  }
  constexpr float kSlot = 150.0f;
  for (const Group& group_rows : groups) {
    group(group_rows.title);
    for (const Row& row : group_rows.rows) {
      int b = row.bind;
      ImRect r(ImVec2(origin.x, y), ImVec2(origin.x + width, y + Px(kRow)));
      ImRect slots[2];
      slots[1] = ImRect(ImVec2(r.Max.x - Px(6.0f) - Px(kSlot), r.Min.y + Px(4.0f)),
                        ImVec2(r.Max.x - Px(6.0f), r.Max.y - Px(4.0f)));
      slots[0] = ImRect(ImVec2(slots[1].Min.x - Px(6.0f) - Px(kSlot), slots[1].Min.y),
                        ImVec2(slots[1].Min.x - Px(6.0f), slots[1].Max.y));
      ImU32 swatch = row.function ? (*row.function->chip ? 0 : Hex(row.function->chip_color)) : SwatchOf(Bind(b));
      MappingRow(r, swatch, row.function ? row.function->name : t_.binds[b], slots[0].Min.x - Px(14.0f),
                 listen_bind_ == b);
      std::vector<std::string> alternatives = Alternatives(o.binds[size_t(b)]);
      std::array<ImGuiID, 2> keys{};
      for (int slot = 0; slot < 2; ++slot) {
        std::string token = slot < int(alternatives.size()) ? alternatives[size_t(slot)] : std::string();
        bool listening = listen_bind_ == b && listen_slot_ == slot;
        ChipLook look;
        if (listening) {
          // With Shift, Ctrl or Alt held: "Shift + …", waiting for the key.
          std::string text;
          if (held_modifiers_ & kShift) text += std::string(t_.key_shift) + " + ";
          if (held_modifiers_ & kCtrl) text += std::string(t_.key_ctrl) + " + ";
          if (held_modifiers_ & kAlt) text += std::string(t_.key_alt) + " + ";
          look.text = held_modifiers_ ? text + "\xE2\x80\xA6" : std::string(t_.press_key);
          look.listening = true;
        } else if (token.empty()) {
          look.text = t_.unbound;
          look.empty = true;
        } else {
          KeyLabel label = LabelOf(token, t_);
          look.text = label.text;
          look.icon = label.icon;
        }
        ImGui::PushID(b * 2 + slot);
        ItemState state = MappingChip("##slot", slots[slot], look);
        ImGui::PopID();
        keys[size_t(slot)] = ImGui::GetItemID();
        if (state.pressed) {
          if (listening) {
            StopListening();
          } else {
            StartListening(b, slot);
          }
        } else if (!listening && !token.empty() && ClearPressed(state)) {
          alternatives.resize(std::max<size_t>(alternatives.size(), 2));
          alternatives[size_t(slot)].clear();
          o.binds[size_t(b)] = Join(alternatives);
          changed = true;
        }
      }
      mapping_rows_.push_back(keys);
      y += Px(kRow) + Px(kGap);
    }
  }

  // The mouse: its camera, and how fast it turns (a chip stepped with left and right, or clicked on either half).
  group(t_.group_mouse);
  switch_row("mouse", t_.mouse_camera, &o.mnk_mouse);
  {
    ImRect r(ImVec2(origin.x, y), ImVec2(origin.x + width, y + Px(kRow)));
    ImRect chip(ImVec2(r.Max.x - Px(6.0f) - Px(190.0f), r.Min.y + Px(4.0f)), ImVec2(r.Max.x - Px(6.0f), r.Max.y - Px(4.0f)));
    MappingRow(r, 0, t_.mouse_sensitivity, chip.Min.x - Px(14.0f), false);
    char value[32];
    std::snprintf(value, sizeof(value), "%.1f×", double(o.mnk_sensitivity));
    ChipLook look;
    look.text = value;
    look.enabled = o.mnk_mouse;
    ItemState state = MappingChip("##sensitivity", chip, look);
    ImGuiID id = o.mnk_mouse ? ImGui::GetItemID() : 0;
    if (id) mapping_rows_.push_back({id, id});
    auto step = [&](float delta) {
      o.mnk_sensitivity = std::clamp(std::round((o.mnk_sensitivity + delta) * 10.0f) / 10.0f, 0.1f, 5.0f);
      changed = true;
    };
    if (id && g.NavId == id && g.NavMoveSubmitted && (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right)) {
      step(g.NavMoveDir == ImGuiDir_Left ? -0.1f : 0.1f);
      ImGui::NavMoveRequestCancel();
    }
    if (state.pressed) {
      bool down = !PressedByNav() && ImGui::GetIO().MousePos.x < chip.GetCenter().x;
      step(down ? -0.1f : 0.1f);
    }
    ImU32 arrows = o.mnk_mouse ? col::kTextSoft : col::kTextDim;
    DrawIcon(Icon::kChevronLeft, ImVec2(chip.Min.x + Px(8.0f), chip.GetCenter().y - Px(8.0f)), 16.0f, arrows, 2.2f);
    DrawIcon(Icon::kChevronRight, ImVec2(chip.Max.x - Px(24.0f), chip.GetCenter().y - Px(8.0f)), 16.0f, arrows, 2.2f);
    y += Px(kRow);
  }

  if (changed) {
    Changed();
  }
  return y - origin.y;
}

}  // namespace launcher
