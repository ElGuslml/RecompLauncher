// A game's achievements in the profile (the profiles mockup's "Perfil · Logros de un juego"): any game whose launcher
// ran, picked at the top; its totals and completion; the unlocked, locked or all achievements, each unlocked one with
// a pin for the profile's showcase (at most profiles::kShowcaseMax).

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

void App::ProfileGamePage(const ImRect& rect) {
  profiles::Profile* profile = store_.Find(store_.current);
  if (!profile || profile_games_.empty()) {
    Navigate(profile ? Page::kProfile : Page::kPlay, false);
    return;
  }
  if (profile_game_ >= profile_games_.size()) {
    profile_game_ = 0;
  }
  ImRect content = PageContent(rect);
  content.Max.x = rect.Max.x - Px(40.0f);
  float x = content.Min.x;
  float width = content.GetWidth();
  float y = content.Min.y;

  // Back to the games, and the game picker.
  {
    constexpr Type kBack{Font::kSemiBold, 15};
    ImRect back(ImVec2(x - Px(6.0f), y), ImVec2(x + Px(18.0f) + MeasureText(kBack, t_.my_games).x + Px(6.0f), y + Px(40.0f)));
    ItemState state = Item("##back", back);
    DrawIcon(Icon::kArrowLeft, ImVec2(x, y + Px(11.0f)), 18.0f, state.hovered ? col::kText : col::kTextMuted, 2.0f);
    DrawLabel(kBack, ImVec2(x + Px(24.0f), y), state.hovered ? col::kText : col::kTextSoft, t_.my_games, Px(40.0f));
    if (state.focused) {
      FocusRing(back, 8.0f);
    }
    if (state.pressed) {
      profile_tab_ = 1;
      Navigate(Page::kProfile, false);
    }
    std::vector<std::string> names;
    for (const ProfileGame& game : profile_games_) {
      names.push_back(game.info.name);
    }
    int selected = int(profile_game_);
    float combo_width = std::min(Px(320.0f), width * 0.4f);
    ImRect combo(ImVec2(x + width - combo_width, y), ImVec2(x + width, y + Px(40.0f)));
    if (Combo("##profile_game", combo, names, &selected)) {
      profile_game_ = size_t(selected);
    }
    constexpr Type kLabel{Font::kRegular, 14};
    DrawLabel(kLabel, ImVec2(combo.Min.x - Px(12.0f) - MeasureText(kLabel, t_.game_label).x, y), col::kTextMuted,
              t_.game_label, Px(40.0f));
    y += Px(40.0f) + Px(18.0f);
  }

  ProfileGame& game = profile_games_[profile_game_];
  ImU32 accent = game.info.accent ? IM_COL32((game.info.accent >> 16) & 0xFF, (game.info.accent >> 8) & 0xFF,
                                             game.info.accent & 0xFF, 0xFF)
                                  : col::kAccent;
  const Achievements* list = game.achievements ? &*game.achievements : nullptr;
  int total = list ? int(list->items.size()) : 0;
  int unlocked = list ? list->unlocked : 0;

  // The header: the game, its totals, its completion.
  {
    float height = Px(108.0f);
    ImRect card(ImVec2(x, y), ImVec2(x + width, y + height));
    FillRect(card, col::kCard, 14.0f);
    GameTile(ImVec2(card.Min.x + Px(22.0f), card.Min.y + (height - Px(72.0f)) * 0.5f), 72.0f, game);
    // The ring, right; the numbers left of it.
    int percent = list && list->total_gamerscore
                      ? int(std::lround(double(list->gamerscore) * 100.0 / double(list->total_gamerscore)))
                      : 0;
    ImVec2 ring_center(card.Max.x - Px(22.0f + 34.0f), card.GetCenter().y);
    Ring(ring_center, 32.0f, 6.0f, float(percent) / 100.0f, col::kTrack, accent);
    std::string percent_text = std::to_string(percent) + "%";
    constexpr Type kPercent{Font::kBold, 16};
    DrawLabel(kPercent, ImVec2(ring_center.x - MeasureText(kPercent, percent_text).x * 0.5f, ring_center.y - LineHeight(kPercent) * 0.5f),
              col::kText, percent_text);
    constexpr Type kBig{Font::kCondensedBold, 30};
    constexpr Type kRest{Font::kCondensedBold, 20};
    constexpr Type kUnder{Font::kRegular, 13};
    float right = ring_center.x - Px(34.0f) - Px(28.0f);
    auto number = [&](float right_edge, const std::string& value, const std::string& rest, const char* under, ImU32 color) {
      float value_width = MeasureText(kBig, value).x;
      float rest_width = MeasureText(kRest, rest).x;
      float left = right_edge - value_width - Px(4.0f) - rest_width;
      float top = card.Min.y + (height - LineHeight(kBig) - LineHeight(kUnder)) * 0.5f;
      DrawLabel(kBig, ImVec2(left, top), color, value);
      DrawLabel(kRest, ImVec2(left + value_width + Px(4.0f), top + LineHeight(kBig) - LineHeight(kRest) - Px(3.0f)),
                col::kTextMuted, rest);
      float under_width = MeasureText(kUnder, under).x;
      DrawLabel(kUnder, ImVec2(left + (value_width + Px(4.0f) + rest_width - under_width) * 0.5f, top + LineHeight(kBig)),
                col::kTextMuted, under);
      return left;
    };
    float left = number(right, std::to_string(list ? list->gamerscore : 0),
                        "/ " + std::to_string(list ? list->total_gamerscore : 0) + " G", t_.gamerscore, accent);
    left = number(left - Px(28.0f), std::to_string(unlocked), "/ " + std::to_string(total), t_.achievements_word, col::kText);
    constexpr Type kTitle{Font::kCondensedBold, 30};
    constexpr Type kChip{Font::kMedium, 13};
    float tx = card.Min.x + Px(22.0f + 72.0f + 20.0f);
    float tw = left - Px(24.0f) - tx;
    float block = LineHeight(kTitle) + Px(8.0f) + Px(26.0f);
    float ty = card.Min.y + (height - block) * 0.5f;
    DrawLabel(kTitle, ImVec2(tx, ty), col::kText, Ellipsize(kTitle, Title(game.info.name), tw));
    char last[96];
    std::snprintf(last, sizeof(last), t_.last_seen, WhenText(game.info.seen).c_str());
    std::string chip = Ellipsize(kChip, last, tw - Px(20.0f));
    ImRect chip_rect(ImVec2(tx, ty + LineHeight(kTitle) + Px(8.0f)),
                     ImVec2(tx + MeasureText(kChip, chip).x + Px(20.0f), ty + LineHeight(kTitle) + Px(8.0f) + Px(26.0f)));
    FillRect(chip_rect, col::kChip, 6.0f);
    DrawLabel(kChip, ImVec2(chip_rect.Min.x + Px(10.0f), chip_rect.Min.y), col::kTextSoft, chip, chip_rect.GetHeight());
    y += height + Px(20.0f);
  }

  // The filters, and the showcase's hint right.
  {
    char filter_unlocked[64], filter_locked[64], filter_all[64];
    std::snprintf(filter_unlocked, sizeof(filter_unlocked), t_.filter_unlocked, unlocked);
    std::snprintf(filter_locked, sizeof(filter_locked), t_.filter_locked, total - unlocked);
    std::snprintf(filter_all, sizeof(filter_all), t_.filter_all, total);
    std::string labels[] = {filter_unlocked, filter_locked, filter_all};
    SegmentedSpec spec{40, 16, 0, col::kCard};
    ImVec2 size = SegmentedSize(spec, labels);
    Segmented("##profile_filter", ImVec2(x, y), spec, labels, &profile_game_filter_);
    constexpr Type kHint{Font::kRegular, 14};
    float hint_width = MeasureText(kHint, t_.pin_hint).x;
    DrawIcon(Icon::kPin, ImVec2(x + width - hint_width - Px(22.0f), y + (size.y - Px(14.0f)) * 0.5f), 14.0f,
             col::kTextMuted, 2.0f);
    DrawLabel(kHint, ImVec2(x + width - hint_width, y), col::kTextMuted, t_.pin_hint, size.y);
    y += size.y + Px(18.0f);
  }
  if (!list) {
    return;
  }

  // The list, with the pins.
  std::vector<size_t> shown;
  for (size_t i = 0; i < list->items.size(); ++i) {
    bool is_unlocked = list->items[i].unlocked != 0;
    if (profile_game_filter_ == 2 || (profile_game_filter_ == 0) == is_unlocked) {
      shown.push_back(i);
    }
  }
  char title_hex[16];
  std::snprintf(title_hex, sizeof(title_hex), "%08X", game.info.title_id);
  auto showcase_key = [&](uint32_t id) { return std::string(title_hex) + ":" + std::to_string(id); };
  ImRect area(ImVec2(x, y), ImVec2(x + width, content.Max.y));
  if (shown.empty()) {
    constexpr Type kText{Font::kRegular, 15};
    DrawLabel(kText, ImVec2(area.GetCenter().x - MeasureText(kText, t_.achievements_filter_empty).x * 0.5f, area.Min.y + Px(40.0f)),
              col::kTextMuted, t_.achievements_filter_empty);
    return;
  }
  BeginScroll("##profile_game_list", area);
  {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float inner = ImGui::GetContentRegionAvail().x - Px(16.0f);
    float gap_x = Px(12.0f);
    float gap_y = Px(10.0f);
    size_t columns = std::clamp(size_t((inner + gap_x) / (Px(480.0f) + gap_x)), size_t(1), size_t(4));
    float card_width = std::floor((inner - gap_x * float(columns - 1)) / float(columns));
    float card_height = Px(84.0f);
    bool full = profile->showcase.size() >= profiles::kShowcaseMax;
    for (size_t n = 0; n < shown.size(); ++n) {
      size_t index = shown[n];
      const Achievement& a = list->items[index];
      float cx = origin.x + float(n % columns) * (card_width + gap_x);
      float cy = origin.y + float(n / columns) * (card_height + gap_y);
      std::string key = showcase_key(a.id);
      auto pinned = std::find(profile->showcase.begin(), profile->showcase.end(), key);
      bool is_pinned = pinned != profile->showcase.end();
      if (AchievementCard(ImRect(ImVec2(cx, cy), ImVec2(cx + card_width, cy + card_height)), a,
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
    ImGui::Dummy(ImVec2(inner, float(rows) * (card_height + gap_y)));
  }
  EndScroll();
}

}  // namespace launcher
