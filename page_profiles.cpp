// The profiles (Gustavo's 2026-10-28 mockup, "¿Quién va a tocar?"): the header's avatar opens it. A card per profile
// (its avatar, name, level and when it was last used; the current one tagged) and one to create another; under them
// the chosen card's actions: the current profile's page and editing, or switching to another.

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
  float y = rect.Min.y + Px(36.0f);
  float center = rect.GetCenter().x;

  constexpr Type kTitle{Font::kDisplayBold, 44};
  std::string title = Upper(t_.profiles_title);
  DrawLabel(kTitle, ImVec2(center - MeasureText(kTitle, title).x * 0.5f, y), col::kText, title, Px(44.0f));
  y += Px(44.0f) + Px(10.0f);
  constexpr Type kSubtitle{Font::kRegular, 16};
  DrawLabel(kSubtitle, ImVec2(center - MeasureText(kSubtitle, t_.profiles_subtitle).x * 0.5f, y), col::kTextSoft,
            t_.profiles_subtitle);
  y += LineHeight(kSubtitle) + Px(36.0f);

  // The cards: 220 x 290 dp, 20 dp apart, narrower when they do not fit.
  size_t count = store_.profiles.size() + (store_.read_only ? 0 : 1);
  float gap = Px(20.0f);
  float card_width = std::min(Px(220.0f), (rect.GetWidth() - Px(80.0f) - gap * float(count - 1)) / float(count));
  float card_height = Px(290.0f);
  float x = center - (card_width * float(count) + gap * float(count - 1)) * 0.5f;
  constexpr Type kName{Font::kDisplayBold, 24};
  constexpr Type kSub{Font::kRegular, 14};
  constexpr Type kTag{Font::kBold, 12, 0.08f};
  float avatar = std::min(120.0f, card_width / Scale() - 32.0f);
  for (const profiles::Profile& profile : store_.profiles) {
    ImRect card(ImVec2(x, y), ImVec2(x + card_width, y + card_height));
    ImGui::PushID(profile.id.c_str());
    ItemState state = Item("##card", card, !store_.read_only || profile.id == current->id);
    ImGuiID id = ImGui::GetItemID();
    ImGui::PopID();
    bool is_current = profile.id == current->id;
    // The focus chooses the card; A (or a click) uses it: the current profile's page, or switching to another.
    if (g.NavId == id && id) {
      profiles_selected_ = profile.id;
    }
    if (state.pressed) {
      profiles_selected_ = profile.id;
      if (is_current) {
        profile_tab_ = 0;
        Navigate(Page::kProfile, false);
      } else {
        SwitchProfile(profile.id);
      }
    }
    bool on = profiles_selected_ == profile.id;
    ImU32 background = on ? Fade(col::kAccent, 0.14f) : col::kPanel;
    FillRect(card, state.hovered ? Brighten(background) : background, 16.0f);
    StrokeRect(card, Hex(0xFFFFFF, 26), 16.0f);
    if (on) {
      // "outline: 3px solid accent; outline-offset: 3px".
      StrokeRect(ImRect(ImVec2(card.Min.x - Px(4.5f), card.Min.y - Px(4.5f)), ImVec2(card.Max.x + Px(4.5f), card.Max.y + Px(4.5f))),
                 col::kAccent, 20.5f, 3.0f);
    }
    float cy = card.Min.y + Px(24.0f);
    DrawAvatar(ImVec2(card.GetCenter().x - Px(avatar) * 0.5f, cy), avatar, profile.avatar, AvatarTexture(profile),
               profile.color);
    cy += Px(avatar) + Px(14.0f);
    std::string name = Ellipsize(kName, Upper(profile.name), card_width - Px(24.0f));
    DrawLabel(kName, ImVec2(card.GetCenter().x - MeasureText(kName, name).x * 0.5f, cy), col::kText, name);
    cy += LineHeight(kName) + Px(6.0f) - Px(8.0f);
    char level[48];
    auto score = profile_gamerscores_.find(profile.id);
    std::snprintf(level, sizeof(level), t_.level,
                  is_current ? Totals().Level() : score != profile_gamerscores_.end() ? int(score->second / 100) + 1 : 1);
    std::string used = is_current ? std::string(t_.in_use) : WhenText(profile.last_used);
    if (!used.empty()) used[0] = char(std::toupper(static_cast<unsigned char>(used[0])));
    std::string sub = Ellipsize(kSub, std::string(level) + " \xC2\xB7 " + used, card_width - Px(24.0f));
    DrawLabel(kSub, ImVec2(card.GetCenter().x - MeasureText(kSub, sub).x * 0.5f, cy), col::kTabText, sub);
    // The tag at the foot.
    std::string tag = Upper(is_current ? t_.profile_current_tag : t_.profile_switch_tag);
    float tag_width = MeasureText(kTag, tag).x + Px(20.0f);
    float tag_height = LineHeight(kTag) + Px(6.0f);
    ImRect chip(ImVec2(card.GetCenter().x - tag_width * 0.5f, card.Max.y - Px(18.0f) - tag_height),
                ImVec2(card.GetCenter().x + tag_width * 0.5f, card.Max.y - Px(18.0f)));
    FillRect(chip, is_current ? col::kAccent : Hex(0xFFFFFF, 20), 4.0f);
    DrawLabel(kTag, ImVec2(chip.Min.x + Px(10.0f), chip.Min.y), is_current ? col::kOnAccent : col::kTextSoft, tag,
              chip.GetHeight());
    if (state.focused) {
      FocusRing(card, 16.0f);
    }
    x += card_width + gap;
  }
  // A new profile.
  if (!store_.read_only) {
    ImRect card(ImVec2(x, y), ImVec2(x + card_width, y + card_height));
    ItemState state = Item("##create", card);
    FillRect(card, state.hovered ? Brighten(Fade(col::kPanel, 0.86f)) : Fade(col::kPanel, 0.86f), 16.0f);
    DashedRect(card, Hex(0xFFFFFF, 56), 16.0f, 6.0f, 4.0f);
    constexpr Type kCreate{Font::kDisplayBold, 22};
    constexpr Type kHint{Font::kRegular, 13};
    float block = Px(avatar) + Px(14.0f) + LineHeight(kCreate) + Px(14.0f) + Px(20.0f);
    float cy = card.GetCenter().y - block * 0.5f;
    ImVec2 circle(card.GetCenter().x, cy + Px(avatar) * 0.5f);
    ImGui::GetWindowDrawList()->AddCircleFilled(circle, Px(avatar) * 0.5f, Hex(0xFFFFFF, 15), 64);
    DrawIcon(Icon::kPlus, ImVec2(circle.x - Px(20.0f), circle.y - Px(20.0f)), 40.0f, col::kTextSoft, 1.6f);
    cy += Px(avatar) + Px(14.0f);
    std::string create = Ellipsize(kCreate, Upper(t_.create_profile), card_width - Px(24.0f));
    DrawLabel(kCreate, ImVec2(card.GetCenter().x - MeasureText(kCreate, create).x * 0.5f, cy), col::kTextSoft, create);
    cy += LineHeight(kCreate) + Px(14.0f);
    float glyph = GlyphWidth(PadButton::kX, 20.0f);
    float hint_width = glyph + Px(6.0f) + MeasureText(kHint, t_.new_player).x;
    float gx = card.GetCenter().x - hint_width * 0.5f;
    DrawGlyph(PadButton::kX, ImVec2(gx, cy), 20.0f);
    DrawLabel(kHint, ImVec2(gx + glyph + Px(6.0f), cy), col::kTabText, t_.new_player, Px(20.0f));
    if (state.focused) {
      FocusRing(card, 16.0f);
    }
    if (state.pressed) {
      OpenProfileCreate();
    }
  }
  y += card_height + Px(32.0f);

  // The chosen card's actions.
  const profiles::Profile* selected = store_.Find(profiles_selected_);
  bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
  KeyButtonSpec primary{KeyLook::kPrimary, 'A', Type{Font::kDisplayBold, 20, 0.05f}, 56, 28, 10, 26, 12};
  ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
  if (selected && selected->id == current->id) {
    KeyButtonSpec edit{KeyLook::kGhost, 'Y', Type{Font::kSemiBold, 16}, 56, 22};
    std::string see = Upper(t_.see_my_profile);
    float width = KeyButtonWidth(primary, see) + Px(14.0f) + KeyButtonWidth(edit, t_.edit_profile);
    float bx = center - width * 0.5f;
    if (KeyButton("##see", ImVec2(bx, y), primary, see).pressed) {
      profile_tab_ = 0;
      Navigate(Page::kProfile, false);
    }
    bool edit_pressed = KeyButton("##edit", ImVec2(bx + KeyButtonWidth(primary, see) + Px(14.0f), y), edit,
                                  t_.edit_profile, !store_.read_only)
                            .pressed;
    if (edit_pressed || (!popup && !store_.read_only && ButtonPressed(PadButton::kY))) {
      OpenProfileEdit();
    }
  } else if (selected) {
    char use[128];
    std::snprintf(use, sizeof(use), t_.use_profile_of, selected->name.c_str());
    std::string label = Upper(use);
    if (KeyButton("##use", ImVec2(center - KeyButtonWidth(primary, label) * 0.5f, y), primary, label).pressed) {
      SwitchProfile(selected->id);
    }
  }
  ImGui::PopItemFlag();
  if (!popup && !store_.read_only && ButtonPressed(PadButton::kX)) {
    OpenProfileCreate();
  }

  // The note at the foot.
  constexpr Type kNote{Font::kRegular, 14};
  float note_width = Px(16.0f) + Px(8.0f) + MeasureText(kNote, t_.profiles_shared).x;
  float note_y = rect.Max.y - Px(24.0f) - LineHeight(kNote);
  float note_x = center - note_width * 0.5f;
  DrawIcon(Icon::kInfo, ImVec2(note_x, note_y + (LineHeight(kNote) - Px(16.0f)) * 0.5f), 16.0f, col::kTabText, 2.0f);
  DrawLabel(kNote, ImVec2(note_x + Px(24.0f), note_y), col::kTabText, t_.profiles_shared);
}

}  // namespace launcher
