// Layout pieces the pages share (page_*.cpp).

#pragma once

#include <algorithm>
#include <string>
#include <string_view>

#include "ui/ui.h"

namespace launcher {

// A frame rate's label: "60 FPS", "120 FPS (experimental)".
inline std::string FrameRateText(int fps, const char* experimental) {
  std::string text = std::to_string(fps) + " FPS";
  if (fps > 60) text += std::string(" (") + experimental + ")";
  return text;
}

// An achievement's icon in `box`: the game's own, dimmed with a lock while locked; a question mark for a secret one
// still locked (page_achievements.cpp).
void DrawAchievementIcon(ImTextureID texture, const ImRect& box, bool unlocked, bool secret);

// The Controls tab's rows (page_controls.cpp; the keyboard's in page_keyboard.cpp, the microphone's in
// page_microphone.cpp), launcher 3.18: flat rows 44 dp high with a hairline under them, a function's swatch (a fret's
// color, or 0 for none) and name, cut at `label_right`; faintly on accent while its chip waits for a button or a key;
// with the focus's fill and outline when `focused` (its chip, switch or value has the focus: RowFocused).
constexpr float kMappingRow = 44.0f;
void MappingRow(const ImRect& row, ImU32 swatch, std::string_view label, float label_right, bool listening,
                bool focused);
// The controller's focus is on the item `id` (an item registered later in this frame, its id from ImGui::GetID).
bool RowFocused(ImGuiID id);
// A row's chip: what presses the function.
struct ChipLook {
  std::string text;
  ui::Icon icon = ui::Icon::kNone;  // after the text: an arrow, a PlayStation symbol
  ImU32 icon_color = ui::col::kText;
  bool listening = false;  // waiting for a button or a key: on accent, pulsing
  bool empty = false;      // nothing presses it: dashed
  bool enabled = true;     // false: what cannot be moved (the D-pad, a stick), not focusable
};
// 140 dp wide, 32 high, 16 dp from the row's right edge (the keyboard's two 128 wide, 8 apart).
ImRect MappingChipRect(const ImRect& row, float width = 140.0f, float right_inset = 16.0f);
ui::ItemState MappingChip(const char* id, const ImRect& rect, const ChipLook& look);
// A row that is a switch (launcher 3.18): the whole row is its item, A or a click toggles it. True when toggled.
bool SwitchRow(const char* id, const ImRect& row, std::string_view label, bool* value, bool enabled = true);
// X on a controller, Delete or Backspace on the focused chip, or a right click on it: clear it.
bool ClearPressed(const ui::ItemState& state);

// The mockup's buttons with a controller key (shell.cpp): the main one on accent with the key in a dark disc and its
// label in capitals (the caller's), the others dark with a thin border and the key in its color, or written in the
// error color (Cancel). Disabled: grey, without its key, not focusable.
enum class KeyLook { kPrimary, kGhost, kDanger };
struct KeyButtonSpec {
  KeyLook look = KeyLook::kGhost;
  char key = 0;  // 'A', 'B', 'X', 'Y', or 0 for none
  ui::Type type{ui::Font::kSemiBold, 16};
  float height = 60;
  float padding = 24;
  float radius = 10;
  float disc = 24;  // the key's disc
  float gap = 10;
};
float KeyButtonWidth(const KeyButtonSpec& spec, std::string_view label);
ui::ItemState KeyButton(const char* id, ImVec2 pos, const KeyButtonSpec& spec, std::string_view label,
                        bool enabled = true);

// A card whose height is known after its rows: its background goes under them through a draw list channel.
class Card {
 public:
  Card() {
    ImGui::GetWindowDrawList()->ChannelsSplit(2);
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(1);
  }
  void End(const ImRect& rect, ImU32 color = ui::col::kCard, float rounding = 12.0f) {
    ImGui::GetWindowDrawList()->ChannelsSetCurrent(0);
    ui::FillRect(rect, color, rounding);
    ImGui::GetWindowDrawList()->ChannelsMerge();
  }
};

}  // namespace launcher
