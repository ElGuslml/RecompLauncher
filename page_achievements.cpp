// The Achievements tab (Gustavo's 2026-10-28 mockup), from the list the game writes for launchers (achievements.h):
// the totals and the gamerscore's progress by the title, the filters (X), a grid of every achievement with the game's
// own icon, and the detail of the one in focus: its description, Reveal for a secret one (Y), its unlock date.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

// An achievement's icon in `box`: the game's own, dimmed with a lock while locked; a question mark for a secret
// one still locked.
void DrawAchievementIcon(ImTextureID texture, const ImRect& box, bool unlocked, bool secret) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  if (secret || texture == ImTextureID_Invalid) {
    FillRect(box, col::kGlyph, 8.0f);
    constexpr Type kMark{Font::kDisplayBold, 26};
    const char* mark = secret ? "?" : "G";
    DrawLabel(kMark, ImVec2(box.GetCenter().x - MeasureText(kMark, mark).x * 0.5f, box.Min.y), col::kTextDim, mark,
              box.GetHeight());
  } else {
    draw->AddImageRounded(texture, box.Min, box.Max, ImVec2(0, 0), ImVec2(1, 1),
                          unlocked ? IM_COL32_WHITE : IM_COL32(92, 96, 104, 255), Radius(8.0f));
  }
  StrokeRect(box, Fade(col::kText, 0.08f), 8.0f);
  if (!unlocked && !secret) {
    // The lock, in a badge on the corner.
    float badge = Px(20.0f);
    ImVec2 center(box.Max.x - badge * 0.5f + Px(3.0f), box.Max.y - badge * 0.5f + Px(3.0f));
    draw->AddCircleFilled(center, badge * 0.5f, col::kCard);
    draw->AddCircleFilled(center, badge * 0.5f - Px(1.5f), col::kChip);
    DrawIcon(Icon::kLock, ImVec2(center.x - Px(6.0f), center.y - Px(6.0f)), 12.0f, col::kTextMuted, 2.0f);
  }
}

void App::AchievementsPage(const ImRect& rect) {
  ImRect main(ImVec2(rect.Min.x + Px(40.0f), rect.Min.y + Px(24.0f)), ImVec2(rect.Max.x - Px(40.0f), rect.Max.y - Px(20.0f)));
  constexpr Type kTitle{Font::kDisplayBold, 40};
  DrawLabel(kTitle, main.Min, col::kText, Upper(t_.achievements_title), Px(40.0f));
  float header_height = Px(40.0f);

  if (!achievements_ || achievements_->items.empty()) {
    // Nothing to list before the game has written the list (it is being asked to, or could not).
    constexpr Type kEmptyTitle{Font::kSemiBold, 18};
    constexpr Type kText{Font::kRegular, 15};
    float text_width = Px(520.0f);
    float text_height = DrawParagraph(kText, ImVec2(0, 0), text_width, 1.5f, 0, t_.achievements_empty, false);
    float height = Px(48.0f) * 2.0f + Px(64.0f) + Px(20.0f) + LineHeight(kEmptyTitle) + Px(8.0f) + text_height;
    ImRect card(ImVec2(main.Min.x, main.Min.y + header_height + Px(18.0f)),
                ImVec2(main.Max.x, main.Min.y + header_height + Px(18.0f) + height));
    FillRect(card, col::kPanel, 14.0f);
    StrokeRect(card, col::kHairline, 14.0f);
    float cx = card.GetCenter().x;
    float top = card.Min.y + Px(48.0f);
    ImRect icon(ImVec2(cx - Px(32.0f), top), ImVec2(cx + Px(32.0f), top + Px(64.0f)));
    FillRect(icon, col::kAccentTint, 12.0f);
    DrawIcon(Icon::kTrophy, ImVec2(icon.Min.x + Px(17.0f), icon.Min.y + Px(17.0f)), 30.0f, col::kAccent, 1.8f);
    top += Px(64.0f) + Px(20.0f);
    DrawLabel(kEmptyTitle, ImVec2(cx - MeasureText(kEmptyTitle, t_.achievements_empty_title).x * 0.5f, top), col::kText,
              t_.achievements_empty_title);
    top += LineHeight(kEmptyTitle) + Px(8.0f);
    DrawParagraph(kText, ImVec2(cx - text_width * 0.5f, top), text_width, 1.5f, col::kTabText, t_.achievements_empty,
                  true, 0.5f);
    return;
  }
  const Achievements& list = *achievements_;
  int total = int(list.items.size());

  // The totals right of the title: "8 / 50", "90 / 1000 G", the gamerscore's bar and its percentage.
  {
    constexpr Type kNumber{Font::kDisplayBold, 22};
    constexpr Type kRest{Font::kRegular, 14};
    float x = main.Min.x + MeasureText(kTitle, Upper(t_.achievements_title)).x + Px(28.0f);
    float mid = main.Min.y + Px(20.0f);
    // The two fonts on one baseline: the line's middle at the title's.
    float number_y = mid - LineHeight(kNumber) * 0.5f;
    float rest_y = number_y + Px(22.0f) * 1.193f - Px(14.0f) * 1.024f;
    auto pair = [&](const std::string& number, const std::string& rest, ImU32 color) {
      DrawLabel(kNumber, ImVec2(x, number_y), color, number);
      x += MeasureText(kNumber, number).x;
      DrawLabel(kRest, ImVec2(x, rest_y), col::kTextSoft, rest);
      x += MeasureText(kRest, rest).x + Px(16.0f);
    };
    pair(std::to_string(list.unlocked), " / " + std::to_string(total), col::kText);
    pair(std::to_string(list.gamerscore), " / " + std::to_string(list.total_gamerscore) + " G", col::kAccent);
    float fraction = list.total_gamerscore ? float(list.gamerscore) / float(list.total_gamerscore) : 0.0f;
    Progress(ImRect(ImVec2(x, mid - Px(4.0f)), ImVec2(x + Px(180.0f), mid + Px(4.0f))), fraction, col::kPanelTrack,
             col::kAccent);
    x += Px(180.0f) + Px(16.0f);
    std::string percent = std::to_string(int(std::lround(fraction * 100.0f))) + " %";
    DrawLabel(kRest, ImVec2(x, mid - LineHeight(kRest) * 0.5f), col::kTextSoft, percent);
  }

  // The filters, right: the X key, then All, Unlocked (n), Locked (n); X steps through them.
  {
    char unlocked[64], locked[64];
    std::snprintf(unlocked, sizeof(unlocked), t_.filter_unlocked, list.unlocked);
    std::snprintf(locked, sizeof(locked), t_.filter_locked, total - list.unlocked);
    const char* labels[] = {t_.filter_all_short, unlocked, locked};
    constexpr Type kPill{Font::kBold, 14};
    float x = main.Max.x;
    float widths[3];
    for (int i = 2; i >= 0; --i) {
      widths[i] = MeasureText(kPill, labels[i]).x + Px(32.0f);
      x -= widths[i] + (i < 2 ? Px(6.0f) : 0.0f);
    }
    float key_x = x - Px(6.0f) - Px(4.0f) - GlyphWidth(PadButton::kX, 24.0f);
    DrawGlyph(PadButton::kX, ImVec2(key_x, main.Min.y + Px(20.0f) - Px(12.0f)), 24.0f);
    for (int i = 0; i < 3; ++i) {
      ImRect pill(ImVec2(x, main.Min.y), ImVec2(x + widths[i], main.Min.y + Px(40.0f)));
      ImGui::PushID(i);
      ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
      ItemState state = Item("##filter", pill);
      ImGui::PopItemFlag();
      ImGui::PopID();
      bool on = achievement_filter_ == i;
      ImU32 background = on ? col::kAccent : col::kGhostFill;
      if (state.hovered) background = Brighten(background);
      FillRect(pill, background, 20.0f);
      StrokeRect(pill, on ? col::kAccent : Hex(0xFFFFFF, 36), 20.0f);
      DrawLabel(kPill, ImVec2(pill.Min.x + Px(16.0f), pill.Min.y), on ? col::kOnAccent : col::kTextSoft, labels[i],
                pill.GetHeight());
      if (state.pressed) achievement_filter_ = i;
      x += widths[i] + Px(6.0f);
    }
    if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ButtonPressed(PadButton::kX)) {
      achievement_filter_ = (achievement_filter_ + 1) % 3;
    }
  }

  // The list (a grid of four columns, scrolling) and the detail of the one in focus, right.
  float top = main.Min.y + header_height + Px(18.0f);
  ImRect detail(ImVec2(main.Max.x - Px(360.0f), top), main.Max);
  ImRect area(ImVec2(main.Min.x, top), ImVec2(detail.Min.x - Px(24.0f), main.Max.y));
  std::vector<size_t> shown;
  for (size_t i = 0; i < list.items.size(); ++i) {
    bool unlocked = list.items[i].unlocked != 0;
    if (achievement_filter_ == 0 || (achievement_filter_ == 1) == unlocked) {
      shown.push_back(i);
    }
  }
  // All: the unlocked ones first, each part in the game's order.
  std::stable_partition(shown.begin(), shown.end(), [&](size_t i) { return list.items[i].unlocked != 0; });
  if (std::find(shown.begin(), shown.end(), achievement_selected_) == shown.end()) {
    achievement_selected_ = shown.empty() ? 0 : shown.front();
  }
  if (shown.empty()) {
    constexpr Type kText{Font::kRegular, 15};
    DrawLabel(kText, ImVec2(area.GetCenter().x - MeasureText(kText, t_.achievements_filter_empty).x * 0.5f, area.Min.y + Px(40.0f)),
              col::kTabText, t_.achievements_filter_empty);
  } else {
    ImGuiContext& g = *GImGui;
    BeginScroll("##achievements", area);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    origin.x += Px(4.0f);
    origin.y += Px(4.0f);
    float width = area.GetWidth() - Px(8.0f) - (ImGui::GetCurrentWindow()->ScrollbarY ? Px(12.0f) : 0.0f);
    constexpr int kColumns = 4;
    float gap = Px(12.0f);
    float card_width = std::floor((width - gap * float(kColumns - 1)) / float(kColumns));
    float card_height = Px(150.0f);
    constexpr Type kName{Font::kSemiBold, 15};
    constexpr Type kPoints{Font::kDisplayBold, 17};
    for (size_t n = 0; n < shown.size(); ++n) {
      size_t index = shown[n];
      const Achievement& a = list.items[index];
      bool unlocked = a.unlocked != 0;
      bool hidden = !unlocked && a.secret() && !revealed_achievements_.count(a.id);
      float x = origin.x + float(n % kColumns) * (card_width + gap);
      float y = origin.y + float(n / kColumns) * (card_height + gap);
      ImRect card(ImVec2(x, y), ImVec2(x + card_width, y + card_height));
      ImGui::PushID(int(a.id));
      ItemState state = Item("##card", card);
      ImGui::PopID();
      if (g.NavId == ImGui::GetItemID() || state.pressed) {
        achievement_selected_ = index;
      }
      bool on = achievement_selected_ == index;
      if (on) {
        page_focus_ = ImGui::GetItemID();  // the controller starts on the chosen card
      }
      ImU32 background = on ? Fade(col::kAccent, 0.14f) : col::kPanel;
      if (state.hovered && !on) background = Brighten(background);
      FillRect(card, background, 10.0f);
      StrokeRect(card, col::kHairline, 10.0f);
      if (on) {
        float o = Px(3.5f);
        ImGui::GetWindowDrawList()->AddRect(ImVec2(card.Min.x - o, card.Min.y - o), ImVec2(card.Max.x + o, card.Max.y + o),
                                            col::kAccent, Radius(10.0f) + o, 0, Px(3.0f));
      }
      ImRect icon(ImVec2(card.Min.x + Px(14.0f), card.Min.y + Px(14.0f)), ImVec2(card.Min.x + Px(58.0f), card.Min.y + Px(58.0f)));
      DrawAchievementIcon(AchievementIcon(index), icon, unlocked, hidden);
      float text_x = card.Min.x + Px(14.0f);
      float text_width = card.GetWidth() - Px(28.0f);
      const std::string& name = hidden ? std::string(t_.achievement_secret) : a.label;
      ImGui::PushClipRect(ImVec2(text_x, icon.Max.y + Px(10.0f)), ImVec2(text_x + text_width, card.Max.y - Px(14.0f) - Px(26.0f)), true);
      DrawParagraph(kName, ImVec2(text_x, icon.Max.y + Px(10.0f)), text_width, 1.25f, unlocked ? col::kText : col::kTabText, name);
      ImGui::PopClipRect();
      float bottom_y = card.Max.y - Px(14.0f) - LineHeight(kPoints);
      std::string points = std::to_string(a.gamerscore) + " G";
      DrawLabel(kPoints, ImVec2(text_x, bottom_y), unlocked ? col::kAccent : col::kTextDim, points);
      if (a.secret()) {
        constexpr Type kSecret{Font::kBold, 11, 0.06f};
        std::string tag = Upper(t_.secret_tag);
        float tx = text_x + MeasureText(kPoints, points).x + Px(8.0f);
        ImRect badge(ImVec2(tx, bottom_y + (LineHeight(kPoints) - Px(19.0f)) * 0.5f),
                     ImVec2(tx + MeasureText(kSecret, tag).x + Px(14.0f), bottom_y + (LineHeight(kPoints) + Px(19.0f)) * 0.5f));
        FillRect(badge, Hex(0xB8A8E8, 41), 4.0f);
        DrawLabel(kSecret, ImVec2(badge.Min.x + Px(7.0f), badge.Min.y), Hex(0xCFC3F2), tag, badge.GetHeight());
      }
      if (unlocked) {
        DrawIcon(Icon::kCheck, ImVec2(card.Max.x - Px(14.0f) - Px(16.0f), bottom_y + (LineHeight(kPoints) - Px(16.0f)) * 0.5f),
                 16.0f, col::kSuccess, 2.6f);
      }
      if (state.focused && !on) FocusRing(card, 10.0f);
    }
    size_t rows = (shown.size() + kColumns - 1) / kColumns;
    ImGui::SetCursorScreenPos(ImGui::GetWindowPos());
    ImGui::Dummy(ImVec2(width, float(rows) * (card_height + gap) + Px(8.0f)));
    EndScroll();
  }

  // The detail: the icon, the state, the name, the description, Reveal for a secret one still locked, and at the foot
  // the unlock date and the gamerscore.
  FillRect(detail, col::kPanel, 14.0f);
  StrokeRect(detail, Hex(0xFFFFFF, 26), 14.0f);
  if (achievement_selected_ < list.items.size()) {
    size_t index = achievement_selected_;
    const Achievement& a = list.items[index];
    bool unlocked = a.unlocked != 0;
    bool can_reveal = !unlocked && a.secret();
    bool revealed = revealed_achievements_.count(a.id) != 0;
    bool hidden = can_reveal && !revealed;
    float x = detail.Min.x + Px(24.0f);
    float width = detail.GetWidth() - Px(48.0f);
    float y = detail.Min.y + Px(24.0f);
    DrawAchievementIcon(AchievementIcon(index), ImRect(ImVec2(x, y), ImVec2(x + Px(96.0f), y + Px(96.0f))), unlocked, hidden);
    y += Px(96.0f) + Px(16.0f);
    constexpr Type kKicker{Font::kBold, 12, 0.12f};
    std::string kicker = Upper(hidden ? t_.secret_tag : unlocked ? t_.unlocked : t_.locked);
    DrawLabel(kKicker, ImVec2(x, y), hidden ? Hex(0xCFC3F2) : unlocked ? col::kSuccess : col::kTabText, kicker);
    if (a.secret() && !hidden) {
      constexpr Type kSecret{Font::kBold, 11, 0.06f};
      std::string tag = Upper(revealed ? t_.secret_revealed_tag : t_.secret_tag);
      float tx = x + MeasureText(kKicker, kicker).x + Px(8.0f);
      ImRect badge(ImVec2(tx, y + (LineHeight(kKicker) - Px(19.0f)) * 0.5f),
                   ImVec2(tx + MeasureText(kSecret, tag).x + Px(14.0f), y + (LineHeight(kKicker) + Px(19.0f)) * 0.5f));
      FillRect(badge, Hex(0xB8A8E8, 41), 4.0f);
      DrawLabel(kSecret, ImVec2(badge.Min.x + Px(7.0f), badge.Min.y), Hex(0xCFC3F2), tag, badge.GetHeight());
    }
    y += LineHeight(kKicker) + Px(6.0f);
    constexpr Type kName{Font::kDisplayBold, 28};
    y += DrawParagraph(kName, ImVec2(x, y), width, 1.1f, col::kText, hidden ? std::string(t_.achievement_secret) : a.label) +
         Px(16.0f);
    constexpr Type kText{Font::kRegular, 16};
    const std::string& description =
        hidden ? std::string(t_.secret_detail) : (unlocked || revealed ? a.description : a.unachieved_description);
    y += DrawParagraph(kText, ImVec2(x, y), width, 1.5f, col::kTextSoft,
                       description.empty() ? a.description : description) + Px(16.0f);
    if (can_reveal) {
      constexpr Type kButton{Font::kBold, 15};
      const char* label = revealed ? t_.hide_again : t_.reveal_achievement;
      ImRect button(ImVec2(x, y), ImVec2(x + width, y + Px(46.0f)));
      ItemState state = Item("##reveal", button);
      ImU32 background = revealed ? col::kGhostFill : Hex(0xB8A8E8);
      if (state.hovered) background = Brighten(background);
      FillRect(button, background, 10.0f);
      StrokeRect(button, revealed ? col::kGhostBorder : Hex(0xB8A8E8), 10.0f);
      float glyph = GlyphWidth(PadButton::kY, 24.0f);
      float content = glyph + Px(10.0f) + MeasureText(kButton, label).x;
      float gx = button.GetCenter().x - content * 0.5f;
      DrawGlyph(PadButton::kY, ImVec2(gx, button.GetCenter().y - Px(12.0f)), 24.0f);
      DrawLabel(kButton, ImVec2(gx + glyph + Px(10.0f), button.Min.y), revealed ? col::kText : Hex(0x1B1430), label,
                button.GetHeight());
      if (state.focused) FocusRing(button, 10.0f);
      bool toggle = state.pressed || (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                                      ButtonPressed(PadButton::kY));
      if (toggle) {
        if (revealed) {
          revealed_achievements_.erase(a.id);
        } else {
          revealed_achievements_.insert(a.id);
        }
      }
      y += Px(46.0f) + Px(10.0f);
      DrawParagraph(Type{Font::kRegular, 13}, ImVec2(x, y), width, 1.4f, col::kLabel, revealed ? t_.hide_note : t_.reveal_note);
    }
    // The foot.
    constexpr Type kStatus{Font::kBold, 14};
    constexpr Type kPoints{Font::kDisplayBold, 30};
    float foot = detail.Max.y - Px(24.0f) - LineHeight(kPoints);
    FillRect(ImRect(ImVec2(x, foot - Px(16.0f)), ImVec2(x + width, foot - Px(16.0f) + Snap(Px(1.0f)))), Hex(0xFFFFFF, 26));
    std::string status = t_.still_locked;
    if (unlocked) {
      std::string date = FormatDate(a.unlocked, language_);
      char text[128];
      std::snprintf(text, sizeof(text), t_.unlocked_on, date.c_str());
      status = date.empty() ? std::string(t_.unlocked) : std::string(text);
    }
    DrawLabel(kStatus, ImVec2(x, foot + (LineHeight(kPoints) - LineHeight(kStatus)) * 0.5f),
              unlocked ? col::kSuccess : col::kTabText, Ellipsize(kStatus, status, width - Px(90.0f)));
    std::string points = std::to_string(a.gamerscore) + " G";
    DrawLabel(kPoints, ImVec2(x + width - MeasureText(kPoints, points).x, foot), unlocked ? col::kAccent : col::kTextDim,
              points);
  }
}

}  // namespace launcher
