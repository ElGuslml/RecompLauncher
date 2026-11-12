// A game's achievements in the profile (launcher 3.18, Gustavo's 2026-11-11 mockup "Camerino", board 18): any game
// whose launcher ran, opened from the games; back (B), its name, its completion and totals with a bar, the unlocked,
// locked or all achievements as text tabs (X), and the list in two columns of flat rows, each unlocked one with a star
// for the profile's showcase (at most profiles::kShowcaseMax; A on the row, or the star).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "app.h"
#include "launcher_version.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

constexpr const char* kStar =
    "<path d='M12 3.2l2.7 5.6 6.1.8-4.5 4.2 1.1 6.1L12 17l-5.4 2.9 1.1-6.1-4.5-4.2 6.1-.8z'/>";

// "Unlocked (%d)" without its count (every language's text ends in " (%d)").
std::string FilterWord(const char* format) {
  std::string word = format;
  size_t at = word.find("(%d)");
  if (at != std::string::npos) word.erase(at, 4);
  while (!word.empty() && word.back() == ' ') word.pop_back();
  return word;
}

}  // namespace

void App::ProfileGamePage(const ImRect& rect) {
  profiles::Profile* profile = store_.Find(store_.current);
  if (!profile || profile_games_.empty()) {
    Navigate(profile ? Page::kProfile : Page::kPlay, false);
    return;
  }
  if (profile_game_ >= profile_games_.size()) {
    profile_game_ = 0;
  }
  ProfileGame& game = profile_games_[profile_game_];
  const Achievements* list = game.achievements ? &*game.achievements : nullptr;
  int total = list ? int(list->items.size()) : 0;
  int unlocked = list ? list->unlocked : 0;
  bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
  const float left = rect.Min.x + Px(56.0f);
  const float right = rect.Max.x - Px(56.0f);
  const float top = rect.Min.y;

  // Back to the games (B), an outline at the left.
  KeyButtonSpec back{KeyLook::kGhost, 'B', Type{Font::kSemiBold, 16}, 44, 20, 12, 22, 10};
  ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
  if (KeyButton("##back", ImVec2(left, top + Px(28.0f)), back, t_.my_games).pressed) {
    profile_tab_ = 1;
    Navigate(Page::kProfile, false);
  }
  ImGui::PopItemFlag();
  const float text_x = left + KeyButtonWidth(back, t_.my_games) + Px(24.0f);

  // The filters at the right: Unlocked n, Locked n, All as text tabs; X steps them.
  float tabs_left;
  {
    constexpr Type kWord{Font::kSemiBold, 18};
    constexpr Type kCount{Font::kRegular, 18};
    std::string words[3] = {FilterWord(t_.filter_unlocked), FilterWord(t_.filter_locked), t_.filter_all_short};
    std::string counts[3] = {std::to_string(unlocked), std::to_string(total - unlocked), ""};
    float widths[3];
    float x = right;
    for (int i = 2; i >= 0; --i) {
      widths[i] = MeasureText(kWord, words[i]).x + (counts[i].empty() ? 0.0f : Px(6.0f) + MeasureText(kCount, counts[i]).x);
      x -= widths[i] + (i < 2 ? Px(30.0f) : 0.0f);
    }
    tabs_left = x;
    float center_y = top + Px(36.0f) + Px(20.0f);
    for (int i = 0; i < 3; ++i) {
      ImRect tab(ImVec2(x, center_y - Px(20.0f)), ImVec2(x + widths[i], center_y + Px(20.0f)));
      ImGui::PushID(i);
      ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
      ItemState state = Item("##filter", tab);
      ImGui::PopItemFlag();
      ImGui::PopID();
      bool on = profile_game_filter_ == i;
      DrawLabel(kWord, ImVec2(tab.Min.x, tab.Min.y), on ? col::kText : state.hovered ? col::kTextSoft : col::kTextMuted,
                words[i], tab.GetHeight());
      if (!counts[i].empty()) {
        DrawLabel(kCount, ImVec2(tab.Min.x + MeasureText(kWord, words[i]).x + Px(6.0f), tab.Min.y), col::kTextDim,
                  counts[i], tab.GetHeight());
      }
      if (on) {
        FillRect(ImRect(ImVec2(tab.Min.x, tab.Max.y - Px(3.0f)), tab.Max), col::kAccent, 3.0f, ImDrawFlags_RoundCornersTop);
      }
      if (state.pressed) profile_game_filter_ = i;
      x += widths[i] + Px(30.0f);
    }
    if (!popup && ButtonPressed(PadButton::kX)) {
      profile_game_filter_ = (profile_game_filter_ + 1) % 3;
    }
  }

  // The game, its completion and totals, and its bar.
  {
    constexpr Type kTitle{Font::kDisplaySemiBold, 40, 0.015f};
    constexpr Type kStrong{Font::kSemiBold, 15};
    constexpr Type kStats{Font::kRegular, 15};
    float tw = tabs_left - Px(24.0f) - text_x;
    DrawLabel(kTitle, ImVec2(text_x, top + Px(14.0f)), col::kText, Ellipsize(kTitle, Title(game.info.name), tw),
              Px(40.0f * 1.1f));
    int percent = list && list->total_gamerscore
                      ? int(std::lround(double(list->gamerscore) * 100.0 / double(list->total_gamerscore)))
                      : 0;
    std::string percent_text = std::to_string(percent) + " %";
    char stats[128], vitrine[48];
    std::snprintf(stats, sizeof(stats), t_.game_stats_line, unlocked, total, int(list ? list->gamerscore : 0),
                  int(list ? list->total_gamerscore : 0));
    std::snprintf(vitrine, sizeof(vitrine), t_.showcase_count, int(profile->showcase.size()));
    float sy = top + Px(64.0f);
    DrawLabel(kStrong, ImVec2(text_x, sy), col::kText, percent_text);
    float px = text_x + MeasureText(kStrong, percent_text).x;
    DrawLabel(kStats, ImVec2(px, sy), col::kTextMuted,
              Ellipsize(kStats, std::string(" \xC2\xB7 ") + stats + " \xC2\xB7 " + vitrine, text_x + tw - px));
    Progress(ImRect(ImVec2(text_x, top + Px(98.0f)), ImVec2(text_x + Px(260.0f), top + Px(104.0f))), float(percent) / 100.0f,
             col::kChip, col::kAccent);
  }
  if (!list) {
    return;
  }

  // The list: two columns 40 dp apart, rows 80 dp high between hairlines.
  std::vector<size_t> shown;
  for (size_t i = 0; i < list->items.size(); ++i) {
    bool is_unlocked = list->items[i].unlocked != 0;
    if (profile_game_filter_ == 2 || (profile_game_filter_ == 0) == is_unlocked) {
      shown.push_back(i);
    }
  }
  // All: the unlocked ones first, each part in the game's order.
  std::stable_partition(shown.begin(), shown.end(), [&](size_t i) { return list->items[i].unlocked != 0; });
  char title_hex[16];
  std::snprintf(title_hex, sizeof(title_hex), "%08X", game.info.title_id);
  auto showcase_key = [&](uint32_t id) { return std::string(title_hex) + ":" + std::to_string(id); };
  ImRect area(ImVec2(left, top + Px(132.0f)), ImVec2(right + Px(16.0f), rect.Max.y - Px(20.0f)));
  if (shown.empty()) {
    constexpr Type kText{Font::kRegular, 15};
    DrawLabel(kText, ImVec2((left + right) * 0.5f - MeasureText(kText, t_.achievements_filter_empty).x * 0.5f,
                            area.Min.y + Px(40.0f)),
              col::kTextMuted, t_.achievements_filter_empty);
    return;
  }
  BeginScroll("##profile_game_list", area);
  {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float inner = right - left;
    float gap_x = Px(40.0f);
    size_t columns = inner >= Px(900.0f) ? 2 : 1;
    float row_width = std::floor((inner - gap_x * float(columns - 1)) / float(columns));
    float row_height = Px(80.0f);
    bool full = profile->showcase.size() >= profiles::kShowcaseMax;
    for (size_t n = 0; n < shown.size(); ++n) {
      size_t index = shown[n];
      const Achievement& a = list->items[index];
      float cx = origin.x + float(n % columns) * (row_width + gap_x);
      float cy = origin.y + float(n / columns) * row_height;
      std::string key = showcase_key(a.id);
      auto pinned = std::find(profile->showcase.begin(), profile->showcase.end(), key);
      bool is_pinned = pinned != profile->showcase.end();
      if (AchievementCard(ImRect(ImVec2(cx, cy), ImVec2(cx + row_width, cy + row_height)), a,
                          ProfileGameIcon(game, index), is_pinned ? 1 : 0, !store_.read_only && (is_pinned || !full))) {
        if (is_pinned) {
          profile->showcase.erase(pinned);
        } else {
          profile->showcase.push_back(key);
        }
        profiles::SaveProfile(store_, *profile);
      }
    }
    size_t rows = (shown.size() + columns - 1) / columns;
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(inner, float(rows) * row_height));
  }
  EndScroll();
}

bool App::AchievementCard(const ImRect& card, const Achievement& a, ImTextureID texture, int pin, bool pin_enabled) {
  // Launcher 3.18 (the mockup's ".arow"): a flat row with a hairline under it, the icon, the name, the description and
  // the date, the points and the star (a round outline; filled in the accent when pinned).
  constexpr Type kName{Font::kSemiBold, 16};
  constexpr Type kText{Font::kRegular, 14};
  constexpr Type kDate{Font::kRegular, 13};
  constexpr Type kScore{Font::kDisplayMedium, 20, 0.01f};
  bool unlocked = a.unlocked != 0;
  bool hidden = !unlocked && a.secret();
  ImGui::PushID(int(a.id));
  // The star (a game's achievements, an unlocked one): registered before the row under it (the first item under the
  // mouse takes it); the controller's A on the row does the same.
  float right = card.Max.x - Px(12.0f);
  ImRect star(ImVec2(right - Px(36.0f), card.GetCenter().y - Px(18.0f)), ImVec2(right, card.GetCenter().y + Px(18.0f)));
  bool pin_active = pin >= 0 && pin_enabled && unlocked;
  ItemState star_state;
  if (pin >= 0) {
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    star_state = Item("star", star, pin_active);
    ImGui::PopItemFlag();
  }
  // Focusable, so that a controller moves through the list (the region scrolls to it).
  ItemState state = Item("card", card);
  bool pressed = star_state.pressed || (state.pressed && PressedByNav() && pin_active);
  if (state.focused) {
    FocusRow(card, 12.0f);
  } else {
    if (state.hovered) FillRect(card, Fade(col::kFocus, 0.03f), 12.0f);
    FillRect(ImRect(ImVec2(card.Min.x, card.Max.y - Snap(Px(1.0f))), card.Max), col::kHairline);
  }

  ImRect icon(ImVec2(card.Min.x + Px(12.0f), card.GetCenter().y - Px(24.0f)), ImVec2(card.Min.x + Px(60.0f), card.GetCenter().y + Px(24.0f)));
  DrawAchievementIcon(texture, icon, unlocked, hidden);

  if (pin >= 0) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 c = star.GetCenter();
    ImU32 ring = pin == 1 ? col::kAccent : star_state.hovered ? col::kTextDim : col::kOutline;
    draw->AddCircle(c, Px(17.5f), ring, 32, Px(1.0f));
    if (pin == 1) {
      // Filled: the star drawn thick enough to close.
      DrawIconSvg("star", kStar, ImVec2(c.x - Px(9.0f), c.y - Px(9.0f)), 18.0f, col::kAccent, 3.4f);
    } else {
      DrawIconSvg("star", kStar, ImVec2(c.x - Px(9.0f), c.y - Px(9.0f)), 18.0f,
                  pin_active ? col::kTextMuted : Fade(col::kTextDim, 0.5f), 1.8f);
    }
    right = star.Min.x - Px(14.0f);
  }

  // The points.
  std::string score = std::to_string(a.gamerscore) + " G";
  float score_width = MeasureText(kScore, score).x;
  DrawLabel(kScore, ImVec2(right - score_width, card.GetCenter().y - LineHeight(kScore) * 0.5f),
            unlocked ? col::kTextSoft : col::kTextDim, score);

  // The name (and "Secret" on a secret one unlocked), the description, the date or Locked.
  float tx = icon.Max.x + Px(14.0f);
  float tw = right - score_width - Px(14.0f) - tx;
  const char* name = hidden ? t_.achievement_secret : a.label.c_str();
  const char* description =
      hidden ? t_.achievement_secret_hint : (unlocked ? a.description.c_str() : a.unachieved_description.c_str());
  std::string status = t_.locked;
  if (unlocked) {
    std::string date = FormatDate(a.unlocked, language_);
    char text[128];
    std::snprintf(text, sizeof(text), t_.unlocked_on, date.c_str());
    status = date.empty() ? std::string(t_.unlocked) : std::string(text);
  }
  float block = LineHeight(kName) + LineHeight(kText) + LineHeight(kDate);
  float ty = card.GetCenter().y - block * 0.5f;
  std::string shown_name = Ellipsize(kName, name, tw);
  DrawLabel(kName, ImVec2(tx, ty), unlocked ? col::kText : col::kTextSoft, shown_name);
  if (unlocked && a.secret()) {
    float tag_x = tx + MeasureText(kName, shown_name).x + Px(8.0f);
    if (tag_x + ChipWidth(t_.secret_tag) < tx + tw) DrawChip(ImVec2(tag_x, ty + LineHeight(kName) * 0.5f), t_.secret_tag);
  }
  DrawLabel(kText, ImVec2(tx, ty + LineHeight(kName)), col::kTextMuted, Ellipsize(kText, description, tw));
  DrawLabel(kDate, ImVec2(tx, ty + LineHeight(kName) + LineHeight(kText)), col::kTextDim, Ellipsize(kDate, status, tw));
  ImGui::PopID();
  return pressed;
}

}  // namespace launcher
