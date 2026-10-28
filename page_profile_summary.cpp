// The profile's page (Gustavo's 2026-10-28 mockup, "Perfil · Resumen" and "Perfil · Mis juegos"): a card with its
// avatar, name, motto and level, the two sections and Edit; then the section: the summary (the totals over every game,
// the week's activity, the recent achievements, the showcase) or the games (each with its playtime, achievements and
// gamerscore, its achievements and Play or its launcher). LB and RB change the section.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <vector>

#include "app.h"
#include "game.h"
#include "pages.h"
#include "profile_art.h"

namespace launcher {

using namespace ui;

namespace {

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

// The texts' "Mon,Tue,...": seven names.
std::vector<std::string> Weekdays(const char* list) {
  std::vector<std::string> names;
  std::string text = list;
  size_t at = 0;
  while (names.size() < 7) {
    size_t comma = text.find(',', at);
    names.push_back(text.substr(at, comma == std::string::npos ? std::string::npos : comma - at));
    if (comma == std::string::npos) break;
    at = comma + 1;
  }
  names.resize(7);
  return names;
}

std::string Capitalized(std::string text) {
  if (!text.empty()) text[0] = char(std::toupper(static_cast<unsigned char>(text[0])));
  return text;
}

// A panel over the art, and its title in Oswald.
constexpr Type kPanelTitle{Font::kDisplayBold, 20, 0.02f};

void Panel(const ImRect& rect, const char* title) {
  FillRect(rect, col::kPanel, 14.0f);
  StrokeRect(rect, col::kHairline, 14.0f);
  DrawLabel(kPanelTitle, ImVec2(rect.Min.x + Px(22.0f), rect.Min.y + Px(22.0f)), col::kText, Upper(title));
}

}  // namespace

void App::ProfilePage(const ImRect& rect) {
  const profiles::Profile* profile = store_.Current();
  if (!profile) {
    Navigate(Page::kPlay, false);
    return;
  }
  ImRect main(ImVec2(rect.Min.x + Px(40.0f), rect.Min.y + Px(24.0f)), ImVec2(rect.Max.x - Px(40.0f), rect.Max.y - Px(20.0f)));
  ProfileTotals totals = Totals();

  // The card: the avatar, the name and motto, the level and its XP; the sections and Edit right.
  ImRect card(main.Min, ImVec2(main.Max.x, main.Min.y + Px(112.0f)));
  FillRect(card, col::kPanel, 14.0f);
  StrokeRect(card, col::kHairline, 14.0f);
  {
    DrawAvatar(ImVec2(card.Min.x + Px(20.0f), card.GetCenter().y - Px(40.0f)), 80.0f, profile->avatar,
               AvatarTexture(*profile), profile->color);
    // Right: the sections as pills, then Edit (Y).
    constexpr Type kPill{Font::kDisplaySemiBold, 17, 0.04f};
    KeyButtonSpec edit{KeyLook::kGhost, 'Y', Type{Font::kSemiBold, 15}, 46, 16, 23, 22, 8};
    float edit_width = KeyButtonWidth(edit, t_.edit_profile);
    float right = card.Max.x - Px(24.0f);
    float pill_y = card.GetCenter().y - Px(23.0f);
    if (KeyButton("##profile_edit", ImVec2(right - edit_width, pill_y), edit, t_.edit_profile, !store_.read_only).pressed) {
      OpenProfileEdit();
    }
    float px = right - edit_width - Px(20.0f);
    const char* tabs[] = {t_.profile_tab_summary, t_.profile_tab_games};
    for (int i = 1; i >= 0; --i) {
      std::string label = Upper(tabs[i]);
      float width = MeasureText(kPill, label).x + Px(40.0f);
      px -= width;
      ImRect pill(ImVec2(px, pill_y), ImVec2(px + width, pill_y + Px(46.0f)));
      ImGui::PushID(i);
      ItemState state = Item("##section", pill);
      ImGui::PopID();
      bool on = profile_tab_ == i;
      if (on) {
        FillRect(pill, state.hovered ? Brighten(col::kAccent) : col::kAccent, 23.0f);
      } else {
        FillRect(pill, state.hovered ? Brighten(col::kGhostFill) : col::kGhostFill, 23.0f);
        StrokeRect(pill, col::kGhostBorder, 23.0f);
      }
      DrawLabel(kPill, ImVec2(pill.Min.x + Px(20.0f), pill.Min.y), on ? col::kOnAccent : col::kText, label, pill.GetHeight());
      if (state.focused) FocusRing(pill, 23.0f);
      if (state.pressed) profile_tab_ = i;
      px -= Px(8.0f);
    }
    // The name and the motto on one line, the level and its bar under them.
    constexpr Type kName{Font::kDisplayBold, 34};
    constexpr Type kMotto{Font::kRegular, 14};
    constexpr Type kLevel{Font::kBold, 13, 0.06f};
    constexpr Type kXp{Font::kRegular, 13};
    float tx = card.Min.x + Px(20.0f) + Px(80.0f) + Px(20.0f);
    float tw = px - Px(16.0f) - tx;
    float block = LineHeight(kName) - Px(8.0f) + Px(4.0f) + Px(24.0f);
    float ty = card.GetCenter().y - block * 0.5f;
    std::string name = Ellipsize(kName, Upper(profile->name), tw);
    DrawLabel(kName, ImVec2(tx, ty - Px(4.0f)), col::kText, name);
    float name_width = MeasureText(kName, name).x;
    if (!profile->motto.empty() && name_width + Px(12.0f) < tw) {
      // On the name's baseline.
      float baseline = ty - Px(4.0f) + (LineHeight(kName) - Px(34.0f) * 1.482f) * 0.5f + Px(34.0f) * 1.193f;
      float motto_y = baseline - (LineHeight(kMotto) - Px(14.0f) * 1.424f) * 0.5f - Px(14.0f) * 1.024f;
      DrawLabel(kMotto, ImVec2(tx + name_width + Px(12.0f), motto_y), col::kTabText,
                Ellipsize(kMotto, profile->motto, tw - name_width - Px(12.0f)));
    }
    float ly = ty + LineHeight(kName) - Px(8.0f) + Px(4.0f);
    char level[48];
    std::snprintf(level, sizeof(level), t_.level, totals.Level());
    std::string badge = Upper(level);
    float badge_width = MeasureText(kLevel, badge).x + Px(20.0f);
    ImRect chip(ImVec2(tx, ly), ImVec2(tx + badge_width, ly + Px(24.0f)));
    FillRect(chip, col::kAccent, 4.0f);
    DrawLabel(kLevel, ImVec2(chip.Min.x + Px(10.0f), chip.Min.y), col::kOnAccent, badge, chip.GetHeight());
    float bar_x = chip.Max.x + Px(12.0f);
    float bar_width = std::min(Px(220.0f), std::max(Px(60.0f), tw - badge_width - Px(120.0f)));
    Progress(ImRect(ImVec2(bar_x, chip.GetCenter().y - Px(4.0f)), ImVec2(bar_x + bar_width, chip.GetCenter().y + Px(4.0f))),
             float(totals.gamerscore % 100) / 100.0f, col::kPanelTrack, col::kAccent);
    std::string xp = std::to_string(totals.gamerscore % 100) + " / 100 XP";
    DrawLabel(kXp, ImVec2(bar_x + bar_width + Px(12.0f), chip.Min.y), col::kTabText, xp, chip.GetHeight());
  }

  ImRect body(ImVec2(main.Min.x, card.Max.y + Px(16.0f)), main.Max);
  if (profile_tab_ == 0) {
    ProfileSummary(body);
  } else {
    ProfileGames(body);
  }
}

void App::ProfileSummary(const ImRect& body) {
  const profiles::Profile* profile = store_.Current();
  ProfileTotals totals = Totals();
  const ProfileActivity& activity = profile_activity_;
  constexpr Type kRowName{Font::kBold, 15};
  constexpr Type kRowSub{Font::kRegular, 13};
  constexpr Type kRowScore{Font::kDisplayBold, 18};

  // The totals: achievements, gamerscore, hours, completion, the streak.
  float y = body.Min.y;
  {
    struct Stat {
      const char* label;
      std::string value;
      std::string sub;
      bool accent;
    };
    char achievements_of[64], gamerscore_of[64], hours_in[64], best[96];
    std::snprintf(achievements_of, sizeof(achievements_of), "%s", ("/ " + std::to_string(totals.total)).c_str());
    std::snprintf(gamerscore_of, sizeof(gamerscore_of), "%s", ("/ " + std::to_string(totals.total_gamerscore)).c_str());
    std::snprintf(hours_in, sizeof(hours_in), activity.played == 1 ? t_.stat_hours_in_one : t_.stat_hours_in,
                  activity.played);
    std::snprintf(best, sizeof(best), t_.stat_streak_best, DaysText(activity.best_streak).c_str());
    // The completion: the games' percentages averaged, over the games with a list.
    int percent_sum = 0, with_list = 0;
    for (const ProfileGame& game : profile_games_) {
      if (game.achievements && game.achievements->total_gamerscore) {
        percent_sum += Percent(*game.achievements);
        ++with_list;
      }
    }
    Stat stats[] = {{t_.stat_achievements, std::to_string(totals.unlocked), achievements_of, false},
                    {t_.stat_gamerscore, std::to_string(totals.gamerscore) + " G", gamerscore_of, true},
                    {t_.stat_hours, HoursText(activity.seconds), hours_in, false},
                    {t_.stat_completed, std::to_string(with_list ? percent_sum / with_list : 0) + " %", t_.stat_average, false},
                    {t_.stat_streak_short, DaysText(activity.streak), best, false}};
    constexpr int kStats = int(std::size(stats));
    constexpr Type kLabel{Font::kBold, 12, 0.12f};
    constexpr Type kValue{Font::kDisplayBold, 30};
    constexpr Type kSub{Font::kRegular, 13};
    float gap = Px(12.0f);
    float width = (body.GetWidth() - gap * float(kStats - 1)) / float(kStats);
    float height = Px(88.0f);
    for (int i = 0; i < kStats; ++i) {
      ImRect tile(ImVec2(body.Min.x + float(i) * (width + gap), y), ImVec2(body.Min.x + float(i) * (width + gap) + width, y + height));
      FillRect(tile, col::kPanel, 12.0f);
      StrokeRect(tile, col::kHairline, 12.0f);
      float tx = tile.Min.x + Px(18.0f);
      float tw = width - Px(36.0f);
      DrawLabel(kLabel, ImVec2(tx, tile.Min.y + Px(14.0f)), col::kTabText, Ellipsize(kLabel, Upper(stats[i].label), tw));
      float vy = tile.Min.y + Px(14.0f) + LineHeight(kLabel) + Px(2.0f);
      std::string value = Ellipsize(kValue, stats[i].value, tw);
      DrawLabel(kValue, ImVec2(tx, vy), stats[i].accent ? col::kAccent : col::kText, value);
      float value_width = MeasureText(kValue, value).x;
      if (value_width + Px(10.0f) < tw) {
        float baseline = vy + (LineHeight(kValue) - Px(30.0f) * 1.482f) * 0.5f + Px(30.0f) * 1.193f;
        float sub_y = baseline - (LineHeight(kSub) - Px(13.0f) * 1.424f) * 0.5f - Px(13.0f) * 1.024f;
        DrawLabel(kSub, ImVec2(tx + value_width + Px(10.0f), sub_y), col::kTabText,
                  Ellipsize(kSub, stats[i].sub, tw - value_width - Px(10.0f)));
      }
    }
    y += height + Px(16.0f);
  }

  // Three panels: the week's activity, the recent achievements, the showcase.
  float gap = Px(16.0f);
  float total = body.GetWidth() - gap * 2.0f;
  ImRect week(ImVec2(body.Min.x, y), ImVec2(body.Min.x + total * 0.3f, body.Max.y));
  ImRect recent(ImVec2(week.Max.x + gap, y), ImVec2(week.Max.x + gap + total * 0.43f, body.Max.y));
  ImRect showcase(ImVec2(recent.Max.x + gap, y), ImVec2(body.Max.x, body.Max.y));

  // The week: a bar a day, Monday first, today lighter; the hours over each.
  Panel(week, t_.weekly_activity);
  {
    constexpr Type kTotal{Font::kBold, 13};
    constexpr Type kTotalRest{Font::kRegular, 13};
    std::string hours = HoursText(activity.week_seconds);
    char text[64];
    std::snprintf(text, sizeof(text), t_.week_total, "");
    std::string rest = text;
    float rest_width = MeasureText(kTotalRest, rest).x;
    float hours_width = MeasureText(kTotal, hours).x;
    float ty = week.Min.y + Px(22.0f) + (LineHeight(kPanelTitle) - LineHeight(kTotal)) * 0.5f;
    DrawLabel(kTotalRest, ImVec2(week.Max.x - Px(22.0f) - rest_width, ty), col::kTextSoft, rest);
    DrawLabel(kTotal, ImVec2(week.Max.x - Px(22.0f) - rest_width - hours_width, ty), col::kText, hours);

    std::vector<std::string> names = Weekdays(t_.weekdays);
    constexpr Type kDay{Font::kBold, 13};
    constexpr Type kHours{Font::kRegular, 12};
    float left = week.Min.x + Px(22.0f);
    float right = week.Max.x - Px(22.0f);
    float bottom = week.Max.y - Px(22.0f) - LineHeight(kDay) - Px(6.0f);
    float top = week.Min.y + Px(22.0f) + LineHeight(kPanelTitle) + Px(24.0f) + LineHeight(kHours) + Px(6.0f);
    float slot = (right - left) / 7.0f;
    float bar_width = slot - Px(8.0f);
    int64_t most = std::max<int64_t>(*std::max_element(std::begin(activity.week), std::end(activity.week)), 1);
    for (int i = 0; i < 7; ++i) {
      float bx = left + float(i) * slot + Px(4.0f);
      bool today = i == activity.today;
      if (activity.week[i] > 0) {
        float bar = std::max(Px(6.0f), (bottom - top) * float(activity.week[i]) / float(most));
        ImRect r(ImVec2(bx, bottom - bar), ImVec2(bx + bar_width, bottom));
        FillRect(r, today ? col::kAccentLight : col::kAccent, 3.0f, ImDrawFlags_RoundCornersTop);
        std::string value = HoursText(activity.week[i]);
        DrawLabel(kHours, ImVec2(bx + (bar_width - MeasureText(kHours, value).x) * 0.5f, r.Min.y - Px(4.0f) - LineHeight(kHours)),
                  col::kTabText, value);
      } else {
        FillRect(ImRect(ImVec2(bx, bottom - Px(4.0f)), ImVec2(bx + bar_width, bottom)), Hex(0xFFFFFF, 26), 2.0f);
        DrawLabel(kHours, ImVec2(bx + (bar_width - MeasureText(kHours, "\xE2\x80\x93").x) * 0.5f, bottom - Px(10.0f) - LineHeight(kHours)),
                  col::kTabText, "\xE2\x80\x93");
      }
      std::string day = names[size_t(i)];
      DrawLabel(kDay, ImVec2(bx + (bar_width - MeasureText(kDay, day).x) * 0.5f, bottom + Px(6.0f)),
                today ? col::kText : col::kTabText, day);
    }
  }

  // The recent achievements: as many rows as fit.
  Panel(recent, t_.recent_achievements);
  {
    std::vector<std::pair<size_t, size_t>> list = Recent(&profile_games_);
    float ry = recent.Min.y + Px(22.0f) + LineHeight(kPanelTitle) + Px(16.0f);
    float rx = recent.Min.x + Px(22.0f);
    float rw = recent.GetWidth() - Px(44.0f);
    float row_height = Px(56.0f);
    if (list.empty()) {
      DrawParagraph(kRowSub, ImVec2(rx, ry), rw, 1.5f, col::kTabText, t_.recent_empty);
    }
    for (const auto& [g, i] : list) {
      if (ry + row_height > recent.Max.y - Px(18.0f)) break;
      ProfileGame& game = profile_games_[g];
      const Achievement& a = game.achievements->items[i];
      ImRect row(ImVec2(rx, ry), ImVec2(rx + rw, ry + row_height));
      FillRect(row, Hex(0xFFFFFF, 10), 10.0f);
      ImRect icon(ImVec2(row.Min.x + Px(10.0f), row.GetCenter().y - Px(20.0f)), ImVec2(row.Min.x + Px(50.0f), row.GetCenter().y + Px(20.0f)));
      DrawAchievementIcon(ProfileGameIcon(game, i), icon, true, false);
      std::string score = std::to_string(a.gamerscore) + " G";
      float score_width = MeasureText(kRowScore, score).x;
      DrawLabel(kRowScore, ImVec2(row.Max.x - Px(14.0f) - score_width, row.Min.y), Accent(game.info), score, row_height);
      float tx = icon.Max.x + Px(12.0f);
      float tw = row.Max.x - Px(28.0f) - score_width - tx;
      float block = LineHeight(kRowName) + LineHeight(kRowSub);
      float ty = row.GetCenter().y - block * 0.5f;
      DrawLabel(kRowName, ImVec2(tx, ty), col::kText, Ellipsize(kRowName, a.label, tw));
      constexpr uint64_t kUnixEpoch = 116'444'736'000'000'000ULL;
      std::string when = Capitalized(WhenText(int64_t((a.unlocked - std::min(a.unlocked, kUnixEpoch)) / 10'000'000ULL)));
      DrawLabel(kRowSub, ImVec2(tx, ty + LineHeight(kRowName)), col::kTabText,
                Ellipsize(kRowSub, game.info.name + " \xC2\xB7 " + when, tw));
      ry += row_height + Px(8.0f);
    }
  }

  // The showcase: the pinned achievements in their games' colors; Change opens the current game's list.
  Panel(showcase, t_.showcase);
  {
    constexpr Type kChange{Font::kBold, 14};
    float title_y = showcase.Min.y + Px(22.0f);
    ImRect change(ImVec2(showcase.Max.x - Px(22.0f) - MeasureText(kChange, t_.edit).x, title_y),
                  ImVec2(showcase.Max.x - Px(22.0f), title_y + LineHeight(kPanelTitle)));
    ItemState state = Item("##showcase_change", change, !profile_games_.empty());
    DrawLabel(kChange, ImVec2(change.Min.x, change.Min.y), state.hovered ? col::kAccentLight : col::kAccent, t_.edit,
              change.GetHeight());
    if (state.focused) FocusRing(change, 4.0f);
    if (state.pressed) {
      profile_game_ = 0;
      Navigate(Page::kProfileGame, false);
    }
    float sy = title_y + LineHeight(kPanelTitle) + Px(16.0f);
    float sx = showcase.Min.x + Px(22.0f);
    float sw = showcase.GetWidth() - Px(44.0f);
    float row_height = Px(60.0f);
    if (profile) {
      for (const std::string& entry : profile->showcase) {
        uint32_t title = 0, id = 0;
        if (std::sscanf(entry.c_str(), "%x:%u", &title, &id) != 2) continue;
        for (ProfileGame& game : profile_games_) {
          if (game.info.title_id != title || !game.achievements) continue;
          for (const Achievement& a : game.achievements->items) {
            if (a.id != id) continue;
            ImU32 accent = Accent(game.info);
            ImRect row(ImVec2(sx, sy), ImVec2(sx + sw, sy + row_height));
            FillRect(row, Fade(accent, 0.15f), 10.0f);
            StrokeRect(row, Fade(accent, 0.35f), 10.0f);
            DrawIcon(Icon::kTrophy, ImVec2(row.Min.x + Px(14.0f), row.GetCenter().y - Px(11.0f)), 22.0f, accent, 1.8f);
            std::string score = std::to_string(a.gamerscore) + " G";
            float score_width = MeasureText(kRowScore, score).x;
            DrawLabel(kRowScore, ImVec2(row.Max.x - Px(14.0f) - score_width, row.Min.y), accent, score, row_height);
            float tx = row.Min.x + Px(50.0f);
            float tw = row.Max.x - Px(28.0f) - score_width - tx;
            float block = LineHeight(kRowName) + LineHeight(kRowSub);
            float ty = row.GetCenter().y - block * 0.5f;
            DrawLabel(kRowName, ImVec2(tx, ty), col::kText, Ellipsize(kRowName, a.label, tw));
            DrawLabel(kRowSub, ImVec2(tx, ty + LineHeight(kRowName)), col::kTextSoft, Ellipsize(kRowSub, game.info.name, tw));
            sy += row_height + Px(10.0f);
          }
        }
      }
    }
    float note_height = DrawParagraph(kRowSub, ImVec2(0, 0), sw, 1.5f, 0, t_.showcase_empty, false);
    DrawParagraph(kRowSub, ImVec2(sx, showcase.Max.y - Px(22.0f) - note_height), sw, 1.5f, col::kTabText,
                  t_.showcase_empty);
  }
}

void App::ProfileGames(const ImRect& body) {
  if (profile_games_.empty()) {
    DrawParagraph(Type{Font::kRegular, 15}, body.Min, body.GetWidth(), 1.5f, col::kTabText, t_.games_hint);
    return;
  }
  // Three cards a row, as high as the region; more than three scroll down.
  constexpr int kColumns = 3;
  float gap = Px(16.0f);
  BeginScroll("##profile_games", body);
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float width = body.GetWidth() - (ImGui::GetCurrentWindow()->ScrollbarY ? Px(14.0f) : 0.0f);
  float card_width = (width - gap * float(kColumns - 1)) / float(kColumns);
  float card_height = std::max(Px(400.0f), body.GetHeight());
  constexpr Type kTitle{Font::kDisplayBold, 22, 0.02f};
  constexpr Type kSub{Font::kRegular, 13};
  constexpr Type kLabel{Font::kRegular, 14};
  constexpr Type kValue{Font::kBold, 14};
  constexpr Type kButton{Font::kBold, 15};
  for (size_t n = 0; n < profile_games_.size(); ++n) {
    ProfileGame& game = profile_games_[n];
    ImU32 accent = Accent(game.info);
    float cx = origin.x + float(n % kColumns) * (card_width + gap);
    float cy = origin.y + float(n / kColumns) * (card_height + gap);
    ImRect card(ImVec2(cx, cy), ImVec2(cx + card_width, cy + card_height));
    FillRect(card, col::kPanel, 14.0f);
    StrokeRect(card, game.current ? Fade(accent, 0.5f) : col::kHairline, 14.0f);
    // The band: the game's color and its icon.
    ImRect band(card.Min, ImVec2(card.Max.x, card.Min.y + Px(132.0f)));
    FillRect(band, Fade(accent, 0.18f), 14.0f, ImDrawFlags_RoundCornersTop);
    GameTile(ImVec2(band.GetCenter().x - Px(36.0f), band.GetCenter().y - Px(36.0f)), 72.0f, game);
    if (game.current) {
      constexpr Type kTag{Font::kBold, 11, 0.08f};
      std::string tag = Upper(t_.this_launcher);
      ImRect chip(ImVec2(band.Min.x + Px(12.0f), band.Min.y + Px(12.0f)),
                  ImVec2(band.Min.x + Px(12.0f) + MeasureText(kTag, tag).x + Px(16.0f), band.Min.y + Px(12.0f) + LineHeight(kTag) + Px(6.0f)));
      FillRect(chip, accent, 4.0f);
      DrawLabel(kTag, ImVec2(chip.Min.x + Px(8.0f), chip.Min.y), col::kOnAccent, tag, chip.GetHeight());
    }
    float x = card.Min.x + Px(20.0f);
    float w = card_width - Px(40.0f);
    float y = band.Max.y + Px(18.0f);
    y += DrawParagraph(kTitle, ImVec2(x, y), w, 1.1f, col::kText, Upper(game.info.name)) + Px(4.0f);
    char hours[64], last[96];
    std::snprintf(hours, sizeof(hours), t_.hours_played, HoursText(game.playtime.seconds).c_str());
    std::snprintf(last, sizeof(last), t_.last_seen, Capitalized(WhenText(game.LastSeen())).c_str());
    DrawLabel(kSub, ImVec2(x, y), col::kTabText, Ellipsize(kSub, std::string(hours) + " \xC2\xB7 " + last, w));
    y += LineHeight(kSub) + Px(16.0f);
    // The completion ring, and the achievements and gamerscore with their bars.
    const Achievements* list = game.achievements ? &*game.achievements : nullptr;
    int percent = list ? Percent(*list) : 0;
    ImVec2 ring(x + Px(30.0f), y + Px(30.0f));
    Ring(ring, 28.0f, 5.0f, float(percent) / 100.0f, col::kPanelTrack, accent);
    constexpr Type kPercent{Font::kBold, 14};
    std::string percent_text = std::to_string(percent) + "%";
    DrawLabel(kPercent, ImVec2(ring.x - MeasureText(kPercent, percent_text).x * 0.5f, ring.y - LineHeight(kPercent) * 0.5f),
              col::kText, percent_text);
    float sx = x + Px(80.0f);
    float sw = x + w - sx;
    auto stat = [&](float top, const char* label, const std::string& value, ImU32 color, float fraction, bool bar) {
      DrawLabel(kLabel, ImVec2(sx, top), col::kTextSoft, label);
      DrawLabel(kValue, ImVec2(sx + sw - MeasureText(kValue, value).x, top), color, value);
      if (bar) {
        float by = top + LineHeight(kLabel) + Px(4.0f);
        Progress(ImRect(ImVec2(sx, by), ImVec2(sx + sw, by + Px(6.0f))), fraction, col::kPanelTrack, accent);
      }
    };
    int total = list ? int(list->items.size()) : 0;
    int unlocked = list ? list->unlocked : 0;
    stat(y - Px(4.0f), t_.nav_achievements, std::to_string(unlocked) + " / " + std::to_string(total), col::kText,
         total ? float(unlocked) / float(total) : 0.0f, true);
    stat(y + LineHeight(kLabel) + Px(16.0f), "Gamerscore",
         std::to_string(list ? list->gamerscore : 0) + " / " + std::to_string(list ? list->total_gamerscore : 0) + " G",
         accent, 0.0f, false);
    // The buttons at the foot: its achievements, and Play (this game) or its launcher.
    float by = card.Max.y - Px(20.0f) - Px(46.0f);
    float half = (w - Px(10.0f)) * 0.5f;
    ImGui::PushID(int(n));
    ImRect see(ImVec2(x, by), ImVec2(x + half, by + Px(46.0f)));
    ItemState see_state = Item("##see", see, list != nullptr);
    FillRect(see, see_state.hovered ? Brighten(col::kGhostFill) : col::kGhostFill, 10.0f);
    StrokeRect(see, col::kGhostBorder, 10.0f);
    std::string see_label = Ellipsize(kButton, t_.see_achievements, half - Px(16.0f));
    DrawLabel(kButton, ImVec2(see.GetCenter().x - MeasureText(kButton, see_label).x * 0.5f, see.Min.y),
              list ? col::kText : col::kTextDim, see_label, see.GetHeight());
    if (see_state.focused) FocusRing(see, 10.0f);
    if (see_state.pressed) {
      profile_game_ = n;
      Navigate(Page::kProfileGame, false);
    }
    ImRect play(ImVec2(x + half + Px(10.0f), by), ImVec2(x + w, by + Px(46.0f)));
    bool can = CanPlay(game);
    ItemState play_state = Item("##play", play, can);
    const char* play_text = game.current ? t_.play_game : can ? t_.open_its_launcher : t_.not_installed;
    if (game.current) {
      FillRect(play, play_state.hovered ? Brighten(col::kAccent) : col::kAccent, 10.0f);
    } else {
      FillRect(play, play_state.hovered ? Brighten(col::kGhostFill) : col::kGhostFill, 10.0f);
      StrokeRect(play, col::kGhostBorder, 10.0f);
    }
    std::string play_label = Ellipsize(kButton, play_text, half - Px(16.0f));
    DrawLabel(kButton, ImVec2(play.GetCenter().x - MeasureText(kButton, play_label).x * 0.5f, play.Min.y),
              game.current ? col::kOnAccent : can ? col::kText : col::kTextDim, play_label, play.GetHeight());
    if (play_state.focused) FocusRing(play, 10.0f);
    ImGui::PopID();
    if (play_state.pressed) {
      PlayProfileGame(game);
    }
  }
  size_t rows = (profile_games_.size() + kColumns - 1) / kColumns;
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(width, float(rows) * (card_height + gap) - gap));
  EndScroll();
}

}  // namespace launcher
