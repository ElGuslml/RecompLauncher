// The profiles (launcher 3.18, Gustavo's 2026-11-11 mockup "Camerino", board 15, "¿Quién juega?"): the header's avatar
// opens it. A tile per profile (its avatar, name, level and when it was last used; the current one marked with a dot)
// and one to create another; under them the chosen tile's actions: the current profile's page (the main action) and
// editing it (X), or switching to another. Y creates a profile.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

void App::ProfilesPage(const ImRect& rect) {
  const profiles::Profile* current = store_.Current();
  if (!current) {
    Navigate(Page::kPlay, false);
    return;
  }
  if (!store_.Find(profiles_selected_)) {
    profiles_selected_ = current->id;
  }
  ImGuiContext& g = *GImGui;
  const float center = rect.GetCenter().x;

  // The title and its line, centred: 24 and 82 dp under the header.
  constexpr Type kTitle{Font::kDisplaySemiBold, 44, 0.015f};
  std::string title = Title(t_.profiles_title);
  DrawLabel(kTitle, ImVec2(center - MeasureText(kTitle, title).x * 0.5f, rect.Min.y + Px(24.0f)), col::kText, title,
            Px(44.0f * 1.1f));
  constexpr Type kSubtitle{Font::kRegular, 15};
  DrawLabel(kSubtitle, ImVec2(center - MeasureText(kSubtitle, t_.profiles_subtitle).x * 0.5f, rect.Min.y + Px(82.0f)),
            col::kTextMuted, t_.profiles_subtitle);

  // The tiles: 240 x 290 dp, 24 dp apart, narrower when they do not fit.
  float y = rect.Min.y + Px(156.0f);
  size_t count = store_.profiles.size() + (store_.read_only ? 0 : 1);
  float gap = Px(24.0f);
  float tile_width = std::min(Px(240.0f), (rect.GetWidth() - Px(80.0f) - gap * float(count - 1)) / float(count));
  float tile_height = Px(290.0f);
  float x = center - (tile_width * float(count) + gap * float(count - 1)) * 0.5f;
  constexpr Type kName{Font::kDisplayMedium, 34, 0.015f};
  constexpr Type kSub{Font::kRegular, 14};
  constexpr Type kMark{Font::kSemiBold, 14};
  float avatar = std::min(120.0f, tile_width / Scale() - 40.0f);
  for (const profiles::Profile& profile : store_.profiles) {
    ImRect tile(ImVec2(x, y), ImVec2(x + tile_width, y + tile_height));
    ImGui::PushID(profile.id.c_str());
    ItemState state = Item("##card", tile, !store_.read_only || profile.id == current->id);
    ImGuiID id = ImGui::GetItemID();
    ImGui::PopID();
    bool is_current = profile.id == current->id;
    // The focus chooses the tile; A (or a click) uses it: the current profile's page, or switching to another.
    if (g.NavId == id && id) {
      profiles_selected_ = profile.id;
    }
    if (state.pressed) {
      profiles_selected_ = profile.id;
      if (is_current) {
        profile_tab_ = 0;
        Navigate(Page::kProfile, PressedByNav());
      } else {
        SwitchProfile(profile.id);
      }
    }
    bool on = profiles_selected_ == profile.id;
    FillRect(tile, on ? col::kCardRaised : state.hovered ? Brighten(col::kCard) : col::kCard, 18.0f);
    if (state.focused) FocusRow(tile, 18.0f);
    float cy = tile.Min.y + Px(30.0f);
    DrawAvatar(ImVec2(tile.GetCenter().x - Px(avatar) * 0.5f, cy), avatar, profile.avatar, AvatarTexture(profile),
               profile.color);
    cy += Px(avatar) + Px(18.0f);
    std::string name = Ellipsize(kName, profile.name, tile_width - Px(24.0f));
    DrawLabel(kName, ImVec2(tile.GetCenter().x - MeasureText(kName, name).x * 0.5f, cy), col::kText, name);
    cy += LineHeight(kName) + Px(2.0f);
    char level[48];
    auto score = profile_gamerscores_.find(profile.id);
    std::snprintf(level, sizeof(level), t_.level,
                  is_current ? Totals().Level() : score != profile_gamerscores_.end() ? int(score->second / 100) + 1 : 1);
    std::string used = is_current ? std::string(t_.in_use) : WhenText(profile.last_used);
    if (!used.empty()) used[0] = char(std::toupper(static_cast<unsigned char>(used[0])));
    std::string sub = Ellipsize(kSub, std::string(level) + " \xC2\xB7 " + used, tile_width - Px(24.0f));
    DrawLabel(kSub, ImVec2(tile.GetCenter().x - MeasureText(kSub, sub).x * 0.5f, cy), col::kTextDim, sub);
    // At the foot: the current profile's mark (a dot and its words), or what A does on another.
    const char* mark = is_current ? t_.profile_current_tag : t_.profile_switch_tag;
    float mark_width = MeasureText(kMark, mark).x + (is_current ? Px(16.0f) : 0.0f);
    float mx = tile.GetCenter().x - mark_width * 0.5f;
    float my = tile.Max.y - Px(24.0f) - LineHeight(kMark);
    if (is_current) {
      ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(mx + Px(4.0f), my + LineHeight(kMark) * 0.5f), Px(4.0f), col::kAccent, 12);
      mx += Px(16.0f);
    }
    DrawLabel(kMark, ImVec2(mx, my), col::kTextSoft, mark);
    x += tile_width + gap;
  }
  // A new profile: a dashed tile.
  if (!store_.read_only) {
    ImRect tile(ImVec2(x, y), ImVec2(x + tile_width, y + tile_height));
    ItemState state = Item("##create", tile);
    if (state.hovered) FillRect(tile, Fade(col::kFocus, 0.03f), 18.0f);
    DashedRect(tile, state.hovered ? col::kTextDim : col::kOutline, 18.0f, 6.0f, 4.0f);
    if (state.focused) FocusRow(tile, 18.0f);
    float cy = tile.Min.y + Px(30.0f);
    ImVec2 circle(tile.GetCenter().x, cy + Px(avatar) * 0.5f);
    ImGui::GetWindowDrawList()->AddCircleFilled(circle, Px(avatar) * 0.5f, col::kChip, 64);
    DrawIcon(Icon::kPlus, ImVec2(circle.x - Px(20.0f), circle.y - Px(20.0f)), 40.0f, col::kTextMuted, 1.75f);
    cy += Px(avatar) + Px(18.0f);
    std::string create = Ellipsize(kName, t_.create_profile, tile_width - Px(24.0f));
    DrawLabel(kName, ImVec2(tile.GetCenter().x - MeasureText(kName, create).x * 0.5f, cy), col::kTextSoft, create);
    cy += LineHeight(kName) + Px(2.0f);
    float glyph = GlyphWidth(PadButton::kY, 20.0f);
    float hint_width = glyph + Px(8.0f) + MeasureText(kSub, t_.new_player).x;
    float gx = tile.GetCenter().x - hint_width * 0.5f;
    DrawGlyph(PadButton::kY, ImVec2(gx, cy + (LineHeight(kSub) - Px(20.0f)) * 0.5f), 20.0f);
    DrawLabel(kSub, ImVec2(gx + glyph + Px(8.0f), cy), col::kTextDim, t_.new_player);
    if (state.pressed) {
      OpenProfileCreate();
    }
  }
  y += tile_height + Px(34.0f);

  // The chosen tile's actions: See my profile (A) and Edit profile (X) for the current one; Use another's (A).
  const profiles::Profile* selected = store_.Find(profiles_selected_);
  bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
  float actions_left = center - (tile_width * float(count) + gap * float(count - 1)) * 0.5f;
  KeyButtonSpec primary{KeyLook::kPrimary, 'A', Type{Font::kDisplaySemiBold, 22, 0.02f}, 56, 26, 14, 26, 12};
  ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
  if (selected && selected->id == current->id) {
    KeyButtonSpec edit{KeyLook::kGhost, 'X', Type{Font::kSemiBold, 16}, 44, 20, 12, 22, 10};
    if (KeyButton("##see", ImVec2(actions_left, y), primary, t_.see_my_profile).pressed) {
      profile_tab_ = 0;
      Navigate(Page::kProfile, PressedByNav());
    }
    bool edit_pressed = KeyButton("##edit", ImVec2(actions_left + KeyButtonWidth(primary, t_.see_my_profile) + Px(24.0f), y + Px(6.0f)),
                                  edit, t_.edit_profile, !store_.read_only)
                            .pressed;
    if (edit_pressed || (!popup && !store_.read_only && ButtonPressed(PadButton::kX))) {
      OpenProfileEdit();
    }
  } else if (selected) {
    char use[128];
    std::snprintf(use, sizeof(use), t_.use_profile_of, selected->name.c_str());
    if (KeyButton("##use", ImVec2(actions_left, y), primary, use).pressed) {
      SwitchProfile(selected->id);
    }
  }
  ImGui::PopItemFlag();
  if (!popup && !store_.read_only && ButtonPressed(PadButton::kY)) {
    OpenProfileCreate();
  }

  // The note at the foot.
  constexpr Type kNote{Font::kRegular, 14};
  float note_width = Px(16.0f) + Px(8.0f) + MeasureText(kNote, t_.profiles_shared).x;
  float note_y = rect.Max.y - Px(32.0f) - LineHeight(kNote);
  float note_x = center - note_width * 0.5f;
  DrawIcon(Icon::kInfo, ImVec2(note_x, note_y + (LineHeight(kNote) - Px(16.0f)) * 0.5f), 16.0f, col::kTextDim, 1.8f);
  DrawLabel(kNote, ImVec2(note_x + Px(24.0f), note_y), col::kTextDim, t_.profiles_shared);
}

}  // namespace launcher
