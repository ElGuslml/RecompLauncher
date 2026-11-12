// The Achievements tab (launcher 3.18, Gustavo's 2026-11-11 mockup "Camerino", board 5), from the list the game writes
// for launchers (achievements.h): the totals beside the title, the filters as text tabs (X steps them), a grid of four
// columns of every achievement with the game's own icon, and under it a strip with the one in focus: its description
// and when it was unlocked. A secret one still locked is revealed (or hidden again) with A.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

// "Unlocked (%d)" without its count: the tab's word (every language's text ends in " (%d)").
std::string FilterWord(const char* format) {
  std::string word = format;
  size_t at = word.find("(%d)");
  if (at != std::string::npos) word.erase(at, 4);
  while (!word.empty() && word.back() == ' ') word.pop_back();
  return word;
}

}  // namespace

// An achievement's icon in `box`: the game's own, faint while locked; a question mark for a secret one still locked.
void DrawAchievementIcon(ImTextureID texture, const ImRect& box, bool unlocked, bool secret) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  if (secret || texture == ImTextureID_Invalid) {
    FillRect(box, col::kChip, 12.0f);
    constexpr Type kMark{Font::kDisplaySemiBold, 26};
    const char* mark = secret ? "?" : "G";
    DrawLabel(kMark, ImVec2(box.GetCenter().x - MeasureText(kMark, mark).x * 0.5f, box.Min.y), col::kTextDim, mark,
              box.GetHeight());
  } else {
    // Launcher 3.18: a locked one at 45 % (the mockup's opacity), the lock beside its points.
    draw->AddImageRounded(texture, box.Min, box.Max, ImVec2(0, 0), ImVec2(1, 1),
                          unlocked ? IM_COL32_WHITE : IM_COL32(255, 255, 255, 115), Radius(12.0f));
  }
}

void App::AchievementsPage(const ImRect& rect) {
  const float page_left = rect.Min.x;
  const float page_top = rect.Min.y;
  const float left = page_left + Px(56.0f);
  const float right = rect.Max.x - Px(56.0f);
  constexpr Type kTitle{Font::kDisplaySemiBold, 44, 0.015f};
  const std::string title = Title(t_.achievements_title);
  const float title_box = Px(44.0f * 1.1f);
  DrawLabel(kTitle, ImVec2(left, page_top + Px(20.0f)), col::kText, title, title_box);
  const float title_center = page_top + Px(20.0f) + title_box * 0.5f;
  achievement_focus_secret_ = 0;

  if (!achievements_ || achievements_->items.empty()) {
    // Nothing to list before the game has written the list (it is being asked to, or could not): a trophy and two lines
    // in the page's middle.
    constexpr Type kEmptyTitle{Font::kSemiBold, 18};
    constexpr Type kText{Font::kRegular, 15};
    float text_width = Px(520.0f);
    float text_height = DrawParagraph(kText, ImVec2(0, 0), text_width, 1.5f, 0, t_.achievements_empty, false);
    float cx = (left + right) * 0.5f;
    float top = rect.GetCenter().y - (Px(40.0f) + Px(18.0f) + LineHeight(kEmptyTitle) + Px(8.0f) + text_height) * 0.5f;
    DrawIcon(Icon::kTrophy, ImVec2(cx - Px(20.0f), top), 40.0f, col::kTextDim, 1.6f);
    top += Px(40.0f) + Px(18.0f);
    DrawLabel(kEmptyTitle, ImVec2(cx - MeasureText(kEmptyTitle, t_.achievements_empty_title).x * 0.5f, top), col::kText,
              t_.achievements_empty_title);
    top += LineHeight(kEmptyTitle) + Px(8.0f);
    DrawParagraph(kText, ImVec2(cx - text_width * 0.5f, top), text_width, 1.5f, col::kTextMuted, t_.achievements_empty,
                  true, 0.5f);
    return;
  }
  const Achievements& list = *achievements_;
  int total = int(list.items.size());

  // The totals right of the title: "34 / 50", "735 / 1000 G", the gamerscore's bar and its percentage, 20 dp apart.
  {
    constexpr Type kStrong{Font::kSemiBold, 16};
    constexpr Type kSoft{Font::kRegular, 16};
    float x = left + MeasureText(kTitle, title).x + Px(48.0f);
    float y = title_center - LineHeight(kSoft) * 0.5f;
    auto pair = [&](const std::string& strong, const std::string& soft) {
      DrawLabel(kStrong, ImVec2(x, y), col::kText, strong);
      x += MeasureText(kStrong, strong).x;
      DrawLabel(kSoft, ImVec2(x, y), col::kTextMuted, soft);
      x += MeasureText(kSoft, soft).x + Px(20.0f);
    };
    pair(std::to_string(list.unlocked), " / " + std::to_string(total));
    pair(std::to_string(list.gamerscore), " / " + std::to_string(list.total_gamerscore) + " G");
    float fraction = list.total_gamerscore ? float(list.gamerscore) / float(list.total_gamerscore) : 0.0f;
    Progress(ImRect(ImVec2(x, title_center - Px(3.0f)), ImVec2(x + Px(200.0f), title_center + Px(3.0f))), fraction,
             col::kChip, col::kAccent);
    x += Px(200.0f) + Px(20.0f);
    DrawLabel(kSoft, ImVec2(x, y), col::kTextMuted, std::to_string(int(std::lround(fraction * 100.0f))) + " %");
  }

  // The filters at the right: All, Unlocked n, Locked n as text tabs (the current one with the accent under it); X
  // steps them, a click picks one.
  {
    constexpr Type kWord{Font::kSemiBold, 16};
    constexpr Type kCount{Font::kRegular, 16};
    std::string words[3] = {t_.filter_all_short, FilterWord(t_.filter_unlocked), FilterWord(t_.filter_locked)};
    std::string counts[3] = {"", std::to_string(list.unlocked), std::to_string(total - list.unlocked)};
    float widths[3];
    float x = right;
    for (int i = 2; i >= 0; --i) {
      widths[i] = MeasureText(kWord, words[i]).x + (counts[i].empty() ? 0.0f : Px(6.0f) + MeasureText(kCount, counts[i]).x);
      x -= widths[i] + (i < 2 ? Px(28.0f) : 0.0f);
    }
    for (int i = 0; i < 3; ++i) {
      ImRect tab(ImVec2(x, title_center - Px(18.0f)), ImVec2(x + widths[i], title_center + Px(18.0f)));
      ImGui::PushID(i);
      ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
      ItemState state = Item("##filter", tab);
      ImGui::PopItemFlag();
      ImGui::PopID();
      bool on = achievement_filter_ == i;
      DrawLabel(kWord, ImVec2(tab.Min.x, tab.GetCenter().y - LineHeight(kWord) * 0.5f),
                on ? col::kText : state.hovered ? col::kTextSoft : col::kTextMuted, words[i]);
      if (!counts[i].empty()) {
        DrawLabel(kCount, ImVec2(tab.Min.x + MeasureText(kWord, words[i]).x + Px(6.0f), tab.GetCenter().y - LineHeight(kCount) * 0.5f),
                  col::kTextDim, counts[i]);
      }
      if (on) {
        FillRect(ImRect(ImVec2(tab.Min.x, tab.Max.y - Px(3.0f)), tab.Max), col::kAccent, 3.0f, ImDrawFlags_RoundCornersTop);
      }
      if (state.pressed) achievement_filter_ = i;
      x += widths[i] + Px(28.0f);
    }
    if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ButtonPressed(PadButton::kX)) {
      achievement_filter_ = (achievement_filter_ + 1) % 3;
    }
  }

  // The grid (four columns of cards 100 dp high, 16 apart, scrolling) and the strip under it.
  const float strip_height = Px(56.0f);
  const float strip_y = rect.Max.y - Px(12.0f) - strip_height;
  ImRect area(ImVec2(left, page_top + Px(96.0f)), ImVec2(right + Px(16.0f), strip_y - Px(24.0f)));
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
    DrawLabel(kText, ImVec2((left + right) * 0.5f - MeasureText(kText, t_.achievements_filter_empty).x * 0.5f,
                            area.Min.y + Px(40.0f)),
              col::kTextMuted, t_.achievements_filter_empty);
  } else {
    ImGuiContext& g = *GImGui;
    BeginScroll("##achievements", area);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    const float width = right - left;
    constexpr int kColumns = 4;
    const float gap = Px(16.0f);
    const float card_width = std::floor((width - gap * float(kColumns - 1)) / float(kColumns));
    const float card_height = Px(100.0f);
    constexpr Type kName{Font::kSemiBold, 17};
    constexpr Type kMeta{Font::kRegular, 14};
    for (size_t n = 0; n < shown.size(); ++n) {
      size_t index = shown[n];
      const Achievement& a = list.items[index];
      bool unlocked = a.unlocked != 0;
      bool secret_locked = !unlocked && a.secret();
      bool hidden = secret_locked && !revealed_achievements_.count(a.id);
      float x = origin.x + float(n % kColumns) * (card_width + gap);
      float y = origin.y + float(n / kColumns) * (card_height + gap);
      ImRect card(ImVec2(x, y), ImVec2(x + card_width, y + card_height));
      ImGui::PushID(int(a.id));
      ItemState state = Item("##card", card);
      ImGui::PopID();
      bool was_selected = achievement_selected_ == index;
      if (g.NavId == ImGui::GetItemID() || state.pressed) {
        achievement_selected_ = index;
      }
      bool on = achievement_selected_ == index;
      if (on) {
        page_focus_ = ImGui::GetItemID();  // the controller starts on the chosen card
        if (state.focused && secret_locked) achievement_focus_secret_ = hidden ? 1 : 2;
      }
      // A (or a click on the chosen card) reveals a secret one still locked, or hides it again.
      if (state.pressed && secret_locked && (PressedByNav() || was_selected)) {
        if (hidden) {
          revealed_achievements_.insert(a.id);
        } else {
          revealed_achievements_.erase(a.id);
        }
        hidden = !hidden;
      }
      // The card (the mockup's ".ach"): the card's color, raised when chosen, with the focus's outline when focused.
      FillRect(card, on ? col::kCardRaised : state.hovered ? Brighten(col::kCard) : col::kCard, 14.0f);
      if (state.focused) FocusRow(card, 14.0f);
      ImRect icon(ImVec2(card.Min.x + Px(20.0f), card.GetCenter().y - Px(28.0f)),
                  ImVec2(card.Min.x + Px(76.0f), card.GetCenter().y + Px(28.0f)));
      DrawAchievementIcon(AchievementIcon(index), icon, unlocked, hidden);
      float text_x = icon.Max.x + Px(16.0f);
      float text_width = card.Max.x - Px(16.0f) - text_x;
      const std::string name = hidden ? std::string(t_.achievement_secret) : a.label;
      // The name on two lines at most, the points under it.
      float name_height = std::min(DrawParagraph(kName, ImVec2(0, 0), text_width, 1.25f, 0, name, false),
                                   Px(17.0f) * 1.25f * 2.0f);
      float block = name_height + Px(5.0f) + LineHeight(kMeta);
      float ty = card.GetCenter().y - block * 0.5f;
      ImGui::PushClipRect(ImVec2(text_x, ty), ImVec2(text_x + text_width, ty + name_height), true);
      DrawParagraph(kName, ImVec2(text_x, ty), text_width, 1.25f, unlocked ? col::kText : col::kTextDim, name);
      ImGui::PopClipRect();
      float my = ty + name_height + Px(5.0f);
      std::string points = std::to_string(a.gamerscore) + " G";
      DrawLabel(kMeta, ImVec2(text_x, my), unlocked ? col::kTextMuted : col::kTextDim, points);
      float mx = text_x + MeasureText(kMeta, points).x + Px(8.0f);
      if (a.secret()) {
        const char* tag = revealed_achievements_.count(a.id) && !unlocked ? t_.secret_revealed_tag : t_.secret_tag;
        DrawChip(ImVec2(mx, my + LineHeight(kMeta) * 0.5f), tag);
      } else if (!unlocked) {
        DrawIcon(Icon::kLock, ImVec2(mx, my + (LineHeight(kMeta) - Px(14.0f)) * 0.5f), 14.0f, col::kTextDim, 2.0f);
      }
    }
    size_t rows = (shown.size() + kColumns - 1) / kColumns;
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(width, float(rows) * (card_height + gap) - gap));
    EndScroll();
  }

  // The strip: the chosen one's name and description, and at the right when it was unlocked and its points.
  FillRect(ImRect(ImVec2(left, strip_y), ImVec2(right, strip_y + Snap(Px(1.0f)))), col::kHairline);
  if (achievement_selected_ < list.items.size()) {
    const Achievement& a = list.items[achievement_selected_];
    bool unlocked = a.unlocked != 0;
    bool hidden = !unlocked && a.secret() && !revealed_achievements_.count(a.id);
    constexpr Type kName{Font::kSemiBold, 17};
    constexpr Type kText{Font::kRegular, 15};
    constexpr Type kWhen{Font::kRegular, 14};
    constexpr Type kPoints{Font::kDisplayMedium, 20, 0.01f};
    float cy = strip_y + strip_height * 0.5f;
    std::string points = std::to_string(a.gamerscore) + " G";
    float x_points = right - MeasureText(kPoints, points).x;
    DrawLabel(kPoints, ImVec2(x_points, cy - LineHeight(kPoints) * 0.5f), col::kTextSoft, points);
    std::string when = t_.still_locked;
    if (unlocked) {
      std::string date = FormatDate(a.unlocked, language_);
      char text[128];
      std::snprintf(text, sizeof(text), t_.unlocked_on, date.c_str());
      when = date.empty() ? std::string(t_.unlocked) : std::string(text);
    }
    float x_when = x_points - Px(18.0f) - MeasureText(kWhen, when).x;
    DrawLabel(kWhen, ImVec2(x_when, cy - LineHeight(kWhen) * 0.5f), col::kTextDim, when);
    const std::string name = hidden ? std::string(t_.achievement_secret) : a.label;
    std::string shown_name = Ellipsize(kName, name, (x_when - left) * 0.45f);
    DrawLabel(kName, ImVec2(left, cy - LineHeight(kName) * 0.5f), col::kText, shown_name);
    float desc_x = left + MeasureText(kName, shown_name).x + Px(16.0f);
    // A locked one's text for players who have not got it, where the game has one; a secret one's only once revealed.
    std::string description;
    if (hidden) {
      description = t_.achievement_secret_hint;
    } else if (unlocked || revealed_achievements_.count(a.id)) {
      description = a.description;
    } else {
      description = a.unachieved_description;
    }
    if (description.empty()) description = a.description;
    DrawLabel(kText, ImVec2(desc_x, cy - LineHeight(kText) * 0.5f), col::kTextMuted,
              Ellipsize(kText, description, x_when - Px(24.0f) - desc_x));
  }
}

}  // namespace launcher
