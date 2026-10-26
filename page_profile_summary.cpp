// The profile's page (the profiles mockup's "Perfil · Resumen" and "Perfil · Mis juegos"): the banner in the
// profile's color with its avatar, name, motto and level, then two tabs: the summary (the totals over every game,
// the games, the showcase, the recent achievements) and the games (each with its achievements and gamerscore).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "app.h"
#include "game.h"
#include "pages.h"
#include "profile_art.h"

namespace launcher {

using namespace ui;

namespace {

ImU32 ProfileColor(int index) {
  uint32_t rgb = profiles::kColors[size_t(std::clamp(index, 0, int(profiles::kColors.size()) - 1))];
  return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0xFF);
}

ImU32 Accent(const profiles::GameInfo& info) {
  return info.accent ? IM_COL32((info.accent >> 16) & 0xFF, (info.accent >> 8) & 0xFF, info.accent & 0xFF, 0xFF)
                     : col::kAccent;
}

// A game's completion: its gamerscore over its total, in percent.
int Percent(const Achievements& list) {
  return list.total_gamerscore ? int(std::lround(double(list.gamerscore) * 100.0 / double(list.total_gamerscore))) : 0;
}

// The unlocked achievements of every game, newest first: (game, achievement).
std::vector<std::pair<size_t, size_t>> Recent(const std::vector<App::ProfileGame>* games) {
  std::vector<std::pair<size_t, size_t>> recent;
  for (size_t g = 0; g < games->size(); ++g) {
    const auto& game = (*games)[g];
    if (!game.achievements) continue;
    for (size_t i = 0; i < game.achievements->items.size(); ++i) {
      if (game.achievements->items[i].unlocked) recent.emplace_back(g, i);
    }
  }
  std::sort(recent.begin(), recent.end(), [games](const auto& a, const auto& b) {
    return (*games)[a.first].achievements->items[a.second].unlocked >
           (*games)[b.first].achievements->items[b.second].unlocked;
  });
  return recent;
}

}  // namespace

void App::ProfilePage(const ImRect& rect) {
  const profiles::Profile* profile = store_.Current();
  if (!profile) {
    Navigate(Page::kPlay, false);
    return;
  }
  ImRect content = PageContent(rect);
  content.Max.x = rect.Max.x - Px(40.0f);
  float x = content.Min.x;
  float width = content.GetWidth();
  float y = content.Min.y;

  // The banner, the avatar across its foot.
  const float banner = Px(96.0f);
  FillRect(ImRect(ImVec2(x, y), ImVec2(x + width, y + banner)), ProfileColor(profile->color), 14.0f);
  const float avatar = 112.0f;
  ImVec2 avatar_pos(x + Px(28.0f), y + banner - Px(avatar * 0.5f));
  ImGui::GetWindowDrawList()->AddCircleFilled(
      ImVec2(avatar_pos.x + Px(avatar * 0.5f), avatar_pos.y + Px(avatar * 0.5f)), Px(avatar * 0.5f + 5.0f),
      col::kBackground, 64);
  DrawAvatar(avatar_pos, avatar, profile->avatar, AvatarTexture(*profile), profile->color);

  // The name, the motto, the level; the buttons right.
  constexpr Type kName{Font::kCondensedBold, 34};
  constexpr Type kMotto{Font::kRegular, 15};
  constexpr Type kLevel{Font::kBold, 13};
  constexpr Type kXp{Font::kRegular, 13};
  ButtonSpec outline{ButtonKind::kGhost, Type{Font::kSemiBold, 14}, 40, 16, 8};
  float edit_width = ButtonWidth(outline, t_.edit_profile);
  float change_width = ButtonWidth(outline, t_.change_profile);
  float buttons_x = x + width - edit_width;
  float top = y + banner + Px(14.0f);
  if (Button("##profile_edit", ImVec2(buttons_x, top), outline, t_.edit_profile, !store_.read_only)) {
    OpenProfileEdit();
  }
  if (store_.profiles.size() > 1 || !store_.read_only) {
    buttons_x -= change_width + Px(10.0f);
    if (Button("##profile_change", ImVec2(buttons_x, top), outline, t_.change_profile)) {
      profile_menu_request_ = true;  // the menu is the sidebar card's (profile_view.cpp)
    }
  }
  float text_x = avatar_pos.x + Px(avatar + 22.0f);
  float text_width = buttons_x - Px(16.0f) - text_x;
  DrawLabel(kName, ImVec2(text_x, top - Px(4.0f)), col::kText, Ellipsize(kName, profile->name, text_width));
  float line = top - Px(4.0f) + LineHeight(kName);
  if (!profile->motto.empty()) {
    DrawLabel(kMotto, ImVec2(text_x, line), col::kTextMuted, Ellipsize(kMotto, profile->motto, text_width));
    line += LineHeight(kMotto);
  }
  ProfileTotals totals = Totals();
  {
    line += Px(10.0f);
    char level[48];
    std::snprintf(level, sizeof(level), t_.level, totals.Level());
    std::string badge = Upper(level);
    ImVec2 size = BadgeSize(badge, 12);
    Badge(ImVec2(text_x, line), badge, col::kAccentTint, col::kAccent, 12);
    float bar_x = text_x + size.x + Px(12.0f);
    float bar_width = std::min(Px(260.0f), std::max(Px(80.0f), text_width - size.x - Px(12.0f)));
    float bar_y = line + (size.y - Px(8.0f)) * 0.5f;
    Progress(ImRect(ImVec2(bar_x, bar_y), ImVec2(bar_x + bar_width, bar_y + Px(8.0f))),
             float(totals.gamerscore % 100) / 100.0f);
    char xp[96];
    std::snprintf(xp, sizeof(xp), t_.xp_next, int(totals.gamerscore % 100), totals.Level() + 1);
    DrawLabel(kXp, ImVec2(bar_x + bar_width + Px(12.0f), line), col::kTextMuted, xp, size.y);
    line += size.y + Px(6.0f);
    char total[128];
    std::snprintf(total, sizeof(total), t_.xp_total, int(totals.gamerscore));
    DrawLabel(Type{Font::kRegular, 12}, ImVec2(text_x, line), col::kTextDim, total);
    line += LineHeight(Type{Font::kRegular, 12});
  }
  y = std::max(line, avatar_pos.y + Px(avatar)) + Px(20.0f);

  // The tabs.
  {
    constexpr Type kTab{Font::kSemiBold, 15};
    const char* tabs[] = {t_.profile_tab_summary, t_.profile_tab_games};
    float tab_x = x;
    float height = Px(44.0f);
    for (int i = 0; i < 2; ++i) {
      float tab_width = MeasureText(kTab, tabs[i]).x + Px(32.0f);
      ImRect tab(ImVec2(tab_x, y), ImVec2(tab_x + tab_width, y + height));
      ImGui::PushID(i);
      ItemState state = Item("##tab", tab);
      ImGui::PopID();
      bool active = profile_tab_ == i;
      DrawLabel(kTab, ImVec2(tab.Min.x + Px(16.0f), tab.Min.y), active ? col::kText : (state.hovered ? col::kTextSoft : col::kTextMuted),
                tabs[i], height);
      if (active) {
        FillRect(ImRect(ImVec2(tab.Min.x, tab.Max.y - Px(3.0f)), ImVec2(tab.Max.x, tab.Max.y)), col::kAccent);
      }
      if (state.focused) {
        FocusRing(tab, 6.0f);
      }
      if (state.pressed) {
        profile_tab_ = i;
      }
      tab_x += tab_width + Px(4.0f);
    }
    FillRect(ImRect(ImVec2(x, y + height), ImVec2(x + width, y + height + Px(1.0f))), col::kDivider);
    y += height + Px(20.0f);
  }

  // The tab's content, in a scrolling region.
  BeginScroll("##profile_scroll", ImRect(ImVec2(x, y), ImVec2(x + width, content.Max.y)));
  {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float inner = ImGui::GetContentRegionAvail().x - Px(16.0f);
    if (profile_tab_ == 0) {
      ProfileSummary(origin, inner);
    } else {
      ProfileGames(origin, inner);
    }
  }
  EndScroll();
}

void App::ProfileSummary(ImVec2 pos, float width) {
  float x = pos.x;
  float y = pos.y;
  ProfileTotals totals = Totals();
  constexpr Type kStatLabel{Font::kBold, 12, 0.1f};
  constexpr Type kStatValue{Font::kCondensedBold, 34};
  constexpr Type kStatSub{Font::kRegular, 13};
  constexpr Type kSection{Font::kCondensedBold, 24};

  // The totals: achievements, gamerscore, games.
  {
    struct Stat {
      const char* label;
      std::string value;
      std::string sub;
    };
    char sub0[64], sub1[64];
    std::snprintf(sub0, sizeof(sub0), t_.stat_achievements_of, totals.total);
    std::snprintf(sub1, sizeof(sub1), t_.stat_gamerscore_of, int(totals.total_gamerscore));
    Stat stats[] = {{t_.stat_achievements, std::to_string(totals.unlocked), sub0},
                    {t_.stat_gamerscore, std::to_string(totals.gamerscore) + " G", sub1},
                    {t_.stat_games, std::to_string(totals.games), t_.stat_games_sub}};
    float gap = Px(14.0f);
    float card_width = (width - gap * 2.0f) / 3.0f;
    float height = Px(18.0f) * 2.0f + LineHeight(kStatLabel) + Px(6.0f) + LineHeight(kStatValue) + Px(4.0f) +
                   LineHeight(kStatSub);
    for (int i = 0; i < 3; ++i) {
      ImRect card(ImVec2(x + float(i) * (card_width + gap), y), ImVec2(x + float(i) * (card_width + gap) + card_width, y + height));
      FillRect(card, col::kCard, 12.0f);
      float cx = card.Min.x + Px(20.0f);
      float cy = card.Min.y + Px(18.0f);
      DrawLabel(kStatLabel, ImVec2(cx, cy), col::kLabel, stats[i].label);
      cy += LineHeight(kStatLabel) + Px(6.0f);
      DrawLabel(kStatValue, ImVec2(cx, cy), col::kText, stats[i].value);
      cy += LineHeight(kStatValue) + Px(4.0f);
      DrawLabel(kStatSub, ImVec2(cx, cy), col::kTextMuted, stats[i].sub);
    }
    y += height + Px(30.0f);
  }

  // My games: a card each.
  {
    DrawLabel(kSection, ImVec2(x, y), col::kText, Title(t_.my_games));
    if (Link("##see_all_games", ImVec2(x + width - MeasureText(Type{Font::kSemiBold, 14}, t_.see_all).x - Px(24.0f), y + Px(6.0f)),
             t_.see_all)) {
      profile_tab_ = 1;
    }
    y += LineHeight(kSection) + Px(14.0f);
    size_t count = profile_games_.size();
    size_t columns = std::clamp(size_t(width / Px(330.0f)), size_t(1), size_t(4));
    float gap = Px(14.0f);
    float card_width = (width - gap * float(columns - 1)) / float(columns);
    float card_height = Px(176.0f);
    constexpr Type kTitle{Font::kSemiBold, 16};
    constexpr Type kSub{Font::kRegular, 13};
    constexpr Type kLine{Font::kMedium, 14};
    for (size_t n = 0; n < count; ++n) {
      ProfileGame& game = profile_games_[n];
      float cx = x + float(n % columns) * (card_width + gap);
      float cy = y + float(n / columns) * (card_height + gap);
      ImRect card(ImVec2(cx, cy), ImVec2(cx + card_width, cy + card_height));
      FillRect(card, col::kCard, 12.0f);
      float px = card.Min.x + Px(18.0f);
      float py = card.Min.y + Px(18.0f);
      GameTile(ImVec2(px, py), 44.0f, game.info);
      // The completion ring, right.
      ImU32 accent = Accent(game.info);
      int percent = game.achievements ? Percent(*game.achievements) : 0;
      ImVec2 ring_center(card.Max.x - Px(18.0f + 26.0f), py + Px(22.0f));
      Ring(ring_center, 22.0f, 5.0f, float(percent) / 100.0f, col::kTrack, accent);
      std::string percent_text = std::to_string(percent) + "%";
      constexpr Type kPercent{Font::kBold, 13};
      DrawLabel(kPercent, ImVec2(ring_center.x - MeasureText(kPercent, percent_text).x * 0.5f, ring_center.y - LineHeight(kPercent) * 0.5f),
                col::kText, percent_text);
      float text_x = px + Px(44.0f + 14.0f);
      float text_width = ring_center.x - Px(34.0f) - text_x;
      DrawLabel(kTitle, ImVec2(text_x, py + Px(2.0f)), col::kText, Ellipsize(kTitle, game.info.name, text_width));
      char last[96];
      std::snprintf(last, sizeof(last), t_.last_seen, WhenText(game.info.seen).c_str());
      DrawLabel(kSub, ImVec2(text_x, py + Px(2.0f) + LineHeight(kTitle) + Px(2.0f)), col::kTextMuted,
                Ellipsize(kSub, last, text_width));
      py += Px(44.0f) + Px(18.0f);
      if (game.achievements) {
        char line[64];
        std::snprintf(line, sizeof(line), t_.achievements_of, game.achievements->unlocked,
                      int(game.achievements->items.size()));
        DrawLabel(kLine, ImVec2(px, py), col::kTextSecondary, line);
        std::string score = std::to_string(game.achievements->gamerscore) + " / " +
                            std::to_string(game.achievements->total_gamerscore) + " G";
        DrawLabel(kLine, ImVec2(card.Max.x - Px(18.0f) - MeasureText(kLine, score).x, py), col::kTextSecondary, score);
      }
      py = card.Max.y - Px(18.0f) - Px(40.0f);
      ButtonSpec see{ButtonKind::kGhost, Type{Font::kSemiBold, 14}, 40, 14, 8};
      ButtonSpec play{ButtonKind::kPrimary, Type{Font::kBold, 14}, 40, 14, 8};
      float inner = card.GetWidth() - Px(36.0f);
      ImGui::PushID(int(n));
      if (game.current) {
        float half = (inner - Px(10.0f)) * 0.5f;
        if (Button("##see", ImVec2(px, py), see, t_.see_game_achievements, game.achievements.has_value(), half)) {
          profile_game_ = n;
          profile_game_filter_ = 0;
          Navigate(Page::kProfileGame, false);
        }
        if (Button("##play", ImVec2(px + half + Px(10.0f), py), play, t_.play, true, half)) {
          result_ = Result::kPlay;
        }
      } else if (Button("##see", ImVec2(px, py), see, t_.see_game_achievements, game.achievements.has_value(), inner)) {
        profile_game_ = n;
        profile_game_filter_ = 0;
        Navigate(Page::kProfileGame, false);
      }
      ImGui::PopID();
    }
    size_t rows = (count + columns - 1) / columns;
    y += float(rows) * (card_height + gap) + Px(16.0f);
  }

  // The showcase and the recent achievements, side by side.
  const profiles::Profile* profile = store_.Current();
  std::vector<std::pair<size_t, size_t>> recent = Recent(&profile_games_);
  float gap = Px(14.0f);
  float left_width = (width - gap) * 0.42f;
  float right_width = width - gap - left_width;
  constexpr Type kRowName{Font::kSemiBold, 15};
  constexpr Type kRowSub{Font::kRegular, 13};
  constexpr Type kRowScore{Font::kCondensedBold, 18};
  float row_height = Px(64.0f);
  float showcase_height = 0.0f;
  // Both cards' backgrounds go under their rows (the second channel), as tall as the taller.
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->ChannelsSplit(2);
  draw->ChannelsSetCurrent(1);
  {
    float cy = y + Px(22.0f);
    DrawLabel(kSection, ImVec2(x + Px(22.0f), cy), col::kText, Title(t_.showcase));
    ButtonSpec edit{ButtonKind::kGhost, Type{Font::kSemiBold, 13}, 34, 12, 8};
    float edit_width = ButtonWidth(edit, t_.edit_showcase);
    // The current game's achievements, where the pins are.
    size_t current = profile_games_.size();
    for (size_t g = 0; g < profile_games_.size(); ++g) {
      if (profile_games_[g].current) current = g;
    }
    if (Button("##edit_showcase", ImVec2(x + left_width - Px(22.0f) - edit_width, cy), edit, t_.edit_showcase,
               current < profile_games_.size() && !store_.read_only)) {
      profile_game_ = current;
      profile_game_filter_ = 0;
      Navigate(Page::kProfileGame, false);
    }
    cy += LineHeight(kSection) + Px(16.0f);
    int shown = 0;
    if (profile) {
      for (const std::string& entry : profile->showcase) {
        uint32_t title = 0, id = 0;
        if (std::sscanf(entry.c_str(), "%x:%u", &title, &id) != 2) continue;
        for (ProfileGame& game : profile_games_) {
          if (game.info.title_id != title || !game.achievements) continue;
          for (size_t i = 0; i < game.achievements->items.size(); ++i) {
            const Achievement& a = game.achievements->items[i];
            if (a.id != id) continue;
            ImRect row(ImVec2(x + Px(22.0f), cy), ImVec2(x + left_width - Px(22.0f), cy + row_height));
            FillRect(row, col::kCardRaised, 10.0f);
            ImRect icon(ImVec2(row.Min.x + Px(12.0f), row.Min.y + Px(10.0f)), ImVec2(row.Min.x + Px(56.0f), row.Min.y + Px(54.0f)));
            DrawAchievementIcon(ProfileGameIcon(game, i), icon, true, false);
            std::string score = std::to_string(a.gamerscore) + " G";
            float score_width = MeasureText(kRowScore, score).x;
            DrawLabel(kRowScore, ImVec2(row.Max.x - Px(14.0f) - score_width, row.Min.y), Accent(game.info), score, row_height);
            float tx = icon.Max.x + Px(14.0f);
            float tw = row.Max.x - Px(28.0f) - score_width - tx;
            float block = LineHeight(kRowName) + Px(2.0f) + LineHeight(kRowSub);
            float ty = row.Min.y + (row_height - block) * 0.5f;
            DrawLabel(kRowName, ImVec2(tx, ty), col::kText, Ellipsize(kRowName, a.label, tw));
            DrawLabel(kRowSub, ImVec2(tx, ty + LineHeight(kRowName) + Px(2.0f)), col::kTextMuted,
                      Ellipsize(kRowSub, game.info.name, tw));
            cy += row_height + Px(10.0f);
            ++shown;
          }
        }
      }
    }
    if (!shown) {
      cy += DrawParagraph(kRowSub, ImVec2(x + Px(22.0f), cy), left_width - Px(44.0f), 1.5f, col::kTextMuted,
                          t_.showcase_empty) + Px(10.0f);
    }
    showcase_height = cy + Px(12.0f) - y;
  }
  float recent_height = 0.0f;
  {
    float rx = x + left_width + gap;
    float cy = y + Px(22.0f);
    DrawLabel(kSection, ImVec2(rx + Px(22.0f), cy), col::kText, Title(t_.recent_achievements));
    cy += LineHeight(kSection) + Px(10.0f);
    size_t count = std::min(recent.size(), size_t(5));
    for (size_t n = 0; n < count; ++n) {
      ProfileGame& game = profile_games_[recent[n].first];
      const Achievement& a = game.achievements->items[recent[n].second];
      ImRect row(ImVec2(rx + Px(22.0f), cy), ImVec2(rx + right_width - Px(22.0f), cy + row_height));
      if (n) {
        FillRect(ImRect(ImVec2(row.Min.x, row.Min.y), ImVec2(row.Max.x, row.Min.y + Px(1.0f))), col::kDivider);
      }
      GameTile(ImVec2(row.Min.x, row.Min.y + Px(14.0f)), 36.0f, game.info);
      std::string score = std::to_string(a.gamerscore) + " G";
      float score_width = MeasureText(kRowScore, score).x;
      DrawLabel(kRowScore, ImVec2(row.Max.x - score_width, row.Min.y), Accent(game.info), score, row_height);
      constexpr uint64_t kUnixEpoch = 116'444'736'000'000'000ULL;
      std::string when = a.unlocked > kUnixEpoch ? WhenText(int64_t((a.unlocked - kUnixEpoch) / 10'000'000ULL)) : std::string();
      if (!when.empty()) {
        when[0] = char(std::toupper(static_cast<unsigned char>(when[0])));
      }
      float when_width = MeasureText(kRowSub, when).x;
      DrawLabel(kRowSub, ImVec2(row.Max.x - score_width - Px(20.0f) - when_width, row.Min.y), col::kTextMuted, when,
                row_height);
      float tx = row.Min.x + Px(36.0f + 14.0f);
      float tw = row.Max.x - score_width - when_width - Px(40.0f) - tx;
      float block = LineHeight(kRowName) + Px(2.0f) + LineHeight(kRowSub);
      float ty = row.Min.y + (row_height - block) * 0.5f;
      DrawLabel(kRowName, ImVec2(tx, ty), col::kText, Ellipsize(kRowName, a.label, tw));
      std::string sub = game.info.name + " \xC2\xB7 " + a.description;
      DrawLabel(kRowSub, ImVec2(tx, ty + LineHeight(kRowName) + Px(2.0f)), col::kTextMuted, Ellipsize(kRowSub, sub, tw));
      cy += row_height;
    }
    if (!count) {
      DrawLabel(kRowSub, ImVec2(rx + Px(22.0f), cy), col::kTextMuted, t_.recent_empty);
      cy += LineHeight(kRowSub);
    }
    recent_height = cy + Px(18.0f) - y;
  }
  float height = std::max(showcase_height, recent_height);
  draw->ChannelsSetCurrent(0);
  FillRect(ImRect(ImVec2(x, y), ImVec2(x + left_width, y + height)), col::kCard, 12.0f);
  FillRect(ImRect(ImVec2(x + left_width + gap, y), ImVec2(x + width, y + height)), col::kCard, 12.0f);
  draw->ChannelsMerge();
  y += height + Px(24.0f);
  ImGui::Dummy(ImVec2(width, y - pos.y));
}

void App::ProfileGames(ImVec2 pos, float width) {
  float x = pos.x;
  float y = pos.y;
  ProfileTotals totals = Totals();
  constexpr Type kSummary{Font::kRegular, 14};
  char summary[128];
  std::snprintf(summary, sizeof(summary), t_.games_summary, totals.games, totals.unlocked);
  std::vector<std::string> sorts = {t_.sort_recent, t_.sort_complete, t_.sort_name};
  float combo_width = Px(200.0f);
  ImRect combo(ImVec2(x + width - combo_width, y), ImVec2(x + width, y + Px(40.0f)));
  Combo("##profile_sort", combo, sorts, &profile_sort_);
  DrawLabel(kSummary, ImVec2(combo.Min.x - Px(12.0f) - MeasureText(kSummary, t_.sort_by).x, y), col::kTextMuted,
            t_.sort_by, Px(40.0f));
  DrawLabel(kSummary, ImVec2(x, y), col::kTextMuted, summary, Px(40.0f));
  y += Px(40.0f) + Px(18.0f);

  std::vector<size_t> order(profile_games_.size());
  for (size_t i = 0; i < order.size(); ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [this](size_t a, size_t b) {
    const ProfileGame& ga = profile_games_[a];
    const ProfileGame& gb = profile_games_[b];
    if (profile_sort_ == 1) {
      int pa = ga.achievements ? Percent(*ga.achievements) : -1;
      int pb = gb.achievements ? Percent(*gb.achievements) : -1;
      return pa > pb;
    }
    if (profile_sort_ == 2) return ga.info.name < gb.info.name;
    return ga.info.seen > gb.info.seen;
  });

  constexpr Type kTitle{Font::kSemiBold, 17};
  constexpr Type kChip{Font::kMedium, 13};
  constexpr Type kSmall{Font::kRegular, 12};
  float row_height = Px(108.0f);
  for (size_t n : order) {
    ProfileGame& game = profile_games_[n];
    ImRect card(ImVec2(x, y), ImVec2(x + width, y + row_height));
    FillRect(card, col::kCard, 12.0f);
    GameTile(ImVec2(card.Min.x + Px(20.0f), card.Min.y + (row_height - Px(68.0f)) * 0.5f), 68.0f, game.info);
    float tx = card.Min.x + Px(20.0f + 68.0f + 20.0f);
    // The buttons, right; then the ring and the progress left of them.
    ButtonSpec see{ButtonKind::kGhost, Type{Font::kSemiBold, 14}, 40, 14, 8};
    ButtonSpec play{ButtonKind::kPrimary, Type{Font::kBold, 14}, 40, 16, 8};
    float right = card.Max.x - Px(20.0f);
    float button_y = card.Min.y + (row_height - Px(40.0f)) * 0.5f;
    ImGui::PushID(int(n));
    if (game.current) {
      float play_width = ButtonWidth(play, t_.play);
      if (Button("##play", ImVec2(right - play_width, button_y), play, t_.play)) {
        result_ = Result::kPlay;
      }
      right -= play_width + Px(10.0f);
    }
    float see_width = ButtonWidth(see, t_.see_game_achievements);
    if (Button("##see", ImVec2(right - see_width, button_y), see, t_.see_game_achievements, game.achievements.has_value())) {
      profile_game_ = n;
      profile_game_filter_ = 0;
      Navigate(Page::kProfileGame, false);
    }
    ImGui::PopID();
    right -= see_width + Px(24.0f);
    ImU32 accent = Accent(game.info);
    int percent = game.achievements ? Percent(*game.achievements) : 0;
    ImVec2 ring_center(right - Px(30.0f), card.GetCenter().y);
    Ring(ring_center, 26.0f, 5.0f, float(percent) / 100.0f, col::kTrack, accent);
    std::string percent_text = std::to_string(percent) + "%";
    constexpr Type kPercent{Font::kBold, 14};
    DrawLabel(kPercent, ImVec2(ring_center.x - MeasureText(kPercent, percent_text).x * 0.5f, ring_center.y - LineHeight(kPercent) * 0.5f),
              col::kText, percent_text);
    right = ring_center.x - Px(30.0f) - Px(24.0f);
    float progress_width = std::min(Px(190.0f), std::max(Px(120.0f), right - tx - Px(200.0f)));
    float progress_x = right - progress_width;
    if (game.achievements) {
      char line[64];
      std::snprintf(line, sizeof(line), t_.achievements_of, game.achievements->unlocked, int(game.achievements->items.size()));
      std::string score = std::to_string(game.achievements->gamerscore) + " G";
      float py = card.GetCenter().y - Px(14.0f);
      DrawLabel(kSmall, ImVec2(progress_x, py), col::kTextMuted, line);
      DrawLabel(kSmall, ImVec2(right - MeasureText(kSmall, score).x, py), col::kTextMuted, score);
      Progress(ImRect(ImVec2(progress_x, py + LineHeight(kSmall) + Px(8.0f)), ImVec2(right, py + LineHeight(kSmall) + Px(14.0f))),
               game.achievements->items.empty() ? 0.0f
                                                : float(game.achievements->unlocked) / float(game.achievements->items.size()),
               col::kTrack, accent);
    }
    // The name and when it was last played.
    float text_width = progress_x - Px(20.0f) - tx;
    float block = LineHeight(kTitle) + Px(10.0f) + Px(26.0f);
    float ty = card.Min.y + (row_height - block) * 0.5f;
    DrawLabel(kTitle, ImVec2(tx, ty), col::kText, Ellipsize(kTitle, game.info.name, text_width));
    char last[96];
    std::snprintf(last, sizeof(last), t_.last_seen, WhenText(game.info.seen).c_str());
    std::string chip = Ellipsize(kChip, last, text_width - Px(20.0f));
    ImRect chip_rect(ImVec2(tx, ty + LineHeight(kTitle) + Px(10.0f)),
                     ImVec2(tx + MeasureText(kChip, chip).x + Px(20.0f), ty + LineHeight(kTitle) + Px(10.0f) + Px(26.0f)));
    FillRect(chip_rect, col::kChip, 6.0f);
    DrawLabel(kChip, ImVec2(chip_rect.Min.x + Px(10.0f), chip_rect.Min.y), col::kTextSoft, chip, chip_rect.GetHeight());
    y += row_height + Px(14.0f);
  }
  // Where the games come from.
  constexpr Type kHint{Font::kRegular, 13};
  DrawIcon(Icon::kInfo, ImVec2(x, y + (LineHeight(kHint) - Px(14.0f)) * 0.5f), 14.0f, col::kTextDim, 2.0f);
  DrawLabel(kHint, ImVec2(x + Px(22.0f), y), col::kTextDim, t_.games_hint);
  y += LineHeight(kHint) + Px(24.0f);
  ImGui::Dummy(ImVec2(width, y - pos.y));
}

}  // namespace launcher
