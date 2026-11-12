// The profile's page (launcher 3.18, Gustavo's 2026-11-11 mockup "Camerino", boards 16 and 17): the avatar, the name,
// the motto and the level at the left; the two sections between LB and RB and Edit (X) at the right; then the section:
// the summary (a strip of the totals over every game, the week's activity, the recent achievements, the showcase) or
// the games (a row each: its icon, playtime, achievements and gamerscore, its achievements and Play or its launcher).

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

// A section's heading (the mockup's ".h2": Oswald 500 22).
constexpr Type kHeading{Font::kDisplayMedium, 22, 0.015f};

}  // namespace

void App::ProfilePage(const ImRect& rect) {
  const profiles::Profile* profile = store_.Current();
  if (!profile) {
    Navigate(Page::kPlay, false);
    return;
  }
  ProfileTotals totals = Totals();
  const float left = rect.Min.x + Px(56.0f);
  const float right = rect.Max.x - Px(56.0f);
  const float top = rect.Min.y;

  // Right: the sections between LB and RB (text tabs, the current one with the accent under it), then Edit (X).
  float sections_left;
  {
    KeyButtonSpec edit{KeyLook::kGhost, 'X', Type{Font::kSemiBold, 16}, 44, 20, 12, 22, 10};
    float edit_width = KeyButtonWidth(edit, t_.edit_profile);
    float row_center = top + Px(32.0f) + Px(22.0f);
    if (KeyButton("##profile_edit", ImVec2(right - edit_width, row_center - Px(22.0f)), edit, t_.edit_profile,
                  !store_.read_only)
            .pressed) {
      OpenProfileEdit();
    }
    page_focus_ = ImGui::GetItemID();
    if (!store_.read_only && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ButtonPressed(PadButton::kX)) {
      OpenProfileEdit();
    }
    constexpr Type kTab{Font::kSemiBold, 18};
    constexpr float kPill = 22.0f;
    float x = right - edit_width - Px(40.0f);
    x -= GlyphWidth(PadButton::kRB, kPill);
    DrawGlyph(PadButton::kRB, ImVec2(x, row_center - Px(kPill) * 0.5f), kPill);
    x -= Px(14.0f);
    const char* tabs[] = {t_.profile_tab_summary, t_.profile_tab_games};
    for (int i = 1; i >= 0; --i) {
      float width = MeasureText(kTab, tabs[i]).x;
      x -= width;
      ImRect tab(ImVec2(x, row_center - Px(20.0f)), ImVec2(x + width, row_center + Px(20.0f)));
      ImGui::PushID(i);
      ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
      ItemState state = Item("##section", tab);
      ImGui::PopItemFlag();
      ImGui::PopID();
      bool on = profile_tab_ == i;
      DrawLabel(kTab, ImVec2(tab.Min.x, tab.Min.y), on ? col::kText : state.hovered ? col::kTextSoft : col::kTextMuted,
                tabs[i], tab.GetHeight());
      if (on) {
        FillRect(ImRect(ImVec2(tab.Min.x, tab.Max.y - Px(3.0f)), tab.Max), col::kAccent, 3.0f, ImDrawFlags_RoundCornersTop);
      }
      if (state.pressed) profile_tab_ = i;
      x -= i > 0 ? Px(30.0f) : Px(14.0f);
    }
    x -= GlyphWidth(PadButton::kLB, kPill);
    DrawGlyph(PadButton::kLB, ImVec2(x, row_center - Px(kPill) * 0.5f), kPill);
    sections_left = x;
  }

  // Left: the avatar (80 dp, its ring), the name, the motto, the level with its bar and XP.
  {
    DrawAvatar(ImVec2(left, top + Px(22.0f)), 80.0f, profile->avatar, AvatarTexture(*profile), profile->color);
    constexpr Type kName{Font::kDisplaySemiBold, 44, 0.015f};
    constexpr Type kMotto{Font::kRegular, 15};
    constexpr Type kLevel{Font::kSemiBold, 14};
    constexpr Type kXp{Font::kRegular, 14};
    float tx = left + Px(100.0f);
    float tw = sections_left - Px(24.0f) - tx;
    DrawLabel(kName, ImVec2(tx, top + Px(14.0f)), col::kText, Ellipsize(kName, Title(profile->name), tw), Px(44.0f * 1.1f));
    if (!profile->motto.empty()) {
      DrawLabel(kMotto, ImVec2(tx, top + Px(64.0f)), col::kTextMuted, Ellipsize(kMotto, profile->motto, tw));
    }
    float ly = top + Px(94.0f);
    char level[48];
    std::snprintf(level, sizeof(level), t_.level, totals.Level());
    DrawLabel(kLevel, ImVec2(tx, ly), col::kTextSoft, level);
    float bar_x = tx + MeasureText(kLevel, level).x + Px(12.0f);
    float mid = ly + LineHeight(kLevel) * 0.5f;
    Progress(ImRect(ImVec2(bar_x, mid - Px(3.0f)), ImVec2(bar_x + Px(160.0f), mid + Px(3.0f))),
             float(totals.gamerscore % 100) / 100.0f, col::kChip, col::kAccent);
    std::string xp = std::to_string(totals.gamerscore % 100) + " / 100 XP";
    DrawLabel(kXp, ImVec2(bar_x + Px(160.0f) + Px(12.0f), ly), col::kTextDim, xp);
  }

  ImRect body(ImVec2(left, top + Px(142.0f)), ImVec2(right, rect.Max.y - Px(28.0f)));
  if (profile_tab_ == 0) {
    ProfileSummary(body);
  } else {
    ProfileGames(ImRect(ImVec2(left, top + Px(160.0f)), ImVec2(right, rect.Max.y - Px(20.0f))));
  }
}

void App::ProfileSummary(const ImRect& body) {
  const profiles::Profile* profile = store_.Current();
  ProfileTotals totals = Totals();
  const ProfileActivity& activity = profile_activity_;
  constexpr Type kSmall{Font::kRegular, 14};
  constexpr Type kRowName{Font::kSemiBold, 17};
  constexpr Type kRowScore{Font::kDisplayMedium, 20, 0.01f};

  // The strip of totals: achievements, gamerscore, hours, completion, the streak.
  {
    char achievements[96], completed[64], streak[96];
    std::snprintf(achievements, sizeof(achievements), t_.strip_achievements, totals.unlocked, totals.total);
    // The completion: the games' percentages averaged, over the games with a list.
    int percent_sum = 0, with_list = 0;
    for (const ProfileGame& game : profile_games_) {
      if (game.achievements && game.achievements->total_gamerscore) {
        percent_sum += Percent(*game.achievements);
        ++with_list;
      }
    }
    std::snprintf(completed, sizeof(completed), t_.strip_completed, with_list ? percent_sum / with_list : 0);
    std::snprintf(streak, sizeof(streak), t_.strip_streak, DaysText(activity.streak).c_str());
    char hours_in[64];
    std::snprintf(hours_in, sizeof(hours_in), activity.played == 1 ? t_.stat_hours_in_one : t_.stat_hours_in,
                  activity.played);
    const StripPart parts[] = {
        {Icon::kTrophy, col::kTextDim, achievements},
        {Icon::kDisc, col::kTextDim, std::to_string(totals.gamerscore) + " / " + std::to_string(totals.total_gamerscore) + " G"},
        {Icon::kClock, col::kTextDim, HoursText(activity.seconds) + " " + hours_in},
        {Icon::kCheck, col::kTextDim, completed},
        {Icon::kFlame, col::kTextDim, streak},
    };
    DrawStrip(body.Min, parts);
  }

  // Three columns 76 dp under the strip, 60 apart: the week (340 dp), the recent achievements, the showcase (268 dp).
  const float y = body.Min.y + Px(76.0f);
  const float gap = Px(60.0f);
  ImRect week(ImVec2(body.Min.x, y), ImVec2(body.Min.x + Px(340.0f), body.Max.y));
  ImRect showcase(ImVec2(body.Max.x - Px(268.0f), y), ImVec2(body.Max.x, body.Max.y));
  ImRect recent(ImVec2(week.Max.x + gap, y), ImVec2(showcase.Min.x - gap, body.Max.y));
  // Hairlines between them, as the other pages' columns.
  FillRect(ImRect(ImVec2(week.Max.x + gap * 0.5f, y), ImVec2(week.Max.x + gap * 0.5f + Snap(Px(1.0f)), body.Max.y - Px(8.0f))),
           col::kHairline);
  FillRect(ImRect(ImVec2(recent.Max.x + gap * 0.5f, y),
                  ImVec2(recent.Max.x + gap * 0.5f + Snap(Px(1.0f)), body.Max.y - Px(8.0f))),
           col::kHairline);

  // The week: its heading and its hours; a bar a day, Monday first: neutral, today in the accent, a stub without play.
  {
    DrawLabel(kHeading, week.Min, col::kText, t_.weekly_activity);
    constexpr Type kTotal{Font::kSemiBold, 14};
    std::string hours = HoursText(activity.week_seconds);
    char text[64];
    std::snprintf(text, sizeof(text), t_.week_total, "");
    std::string rest = text;
    float rest_width = MeasureText(kSmall, rest).x;
    float baseline = week.Min.y + Px(22.0f) * 1.193f + (LineHeight(kHeading) - Px(22.0f) * 1.482f) * 0.5f;
    float ty = baseline - Px(14.0f) * 1.024f - (LineHeight(kSmall) - Px(14.0f) * 1.424f) * 0.5f;
    DrawLabel(kSmall, ImVec2(week.Max.x - rest_width, ty), col::kTextDim, rest);
    DrawLabel(kTotal, ImVec2(week.Max.x - rest_width - MeasureText(kTotal, hours).x, ty), col::kTextSoft, hours);

    std::vector<std::string> names = Weekdays(t_.weekdays);
    constexpr Type kDay{Font::kRegular, 14};
    constexpr Type kDayToday{Font::kSemiBold, 14};
    constexpr Type kHours{Font::kRegular, 13};
    constexpr Type kHoursToday{Font::kSemiBold, 13};
    const float slot = Px(36.0f);
    const float slot_gap = Px(14.0f);
    float bars_top = week.Min.y + Px(58.0f);
    float bars_bottom = bars_top + Px(200.0f);
    float max_bar = Px(200.0f) - LineHeight(kHours) - Px(6.0f);
    int64_t most = std::max<int64_t>(*std::max_element(std::begin(activity.week), std::end(activity.week)), 1);
    for (int i = 0; i < 7; ++i) {
      float bx = week.Min.x + float(i) * (slot + slot_gap);
      bool today = i == activity.today;
      float bar = activity.week[i] > 0 ? std::max(Px(10.0f), max_bar * float(activity.week[i]) / float(most)) : Px(6.0f);
      ImRect r(ImVec2(bx, bars_bottom - bar), ImVec2(bx + slot, bars_bottom));
      ImU32 fill = activity.week[i] <= 0 ? col::kChip : today ? col::kAccent : col::kTextMuted;
      FillRect(r, fill, activity.week[i] > 0 ? 8.0f : 3.0f, activity.week[i] > 0 ? ImDrawFlags_RoundCornersTop : 0);
      std::string value = activity.week[i] > 0 ? HoursText(activity.week[i]) : std::string("\xE2\x80\x93");
      const Type& hours_type = today ? kHoursToday : kHours;
      DrawLabel(hours_type, ImVec2(bx + (slot - MeasureText(hours_type, value).x) * 0.5f, r.Min.y - Px(6.0f) - LineHeight(hours_type)),
                today ? col::kText : col::kTextDim, value);
      const Type& day_type = today ? kDayToday : kDay;
      std::string day = names[size_t(i)];
      DrawLabel(day_type, ImVec2(bx + (slot - MeasureText(day_type, day).x) * 0.5f, bars_bottom + Px(12.0f)),
                today ? col::kText : col::kTextMuted, day);
    }
  }

  // The recent achievements: rows 64 dp high between hairlines, as many as fit.
  {
    DrawLabel(kHeading, recent.Min, col::kText, t_.recent_achievements);
    std::vector<std::pair<size_t, size_t>> list = Recent(&profile_games_);
    float ry = recent.Min.y + Px(46.0f);
    FillRect(ImRect(ImVec2(recent.Min.x, ry), ImVec2(recent.Max.x, ry + Snap(Px(1.0f)))), col::kHairline);
    const float row_height = Px(64.0f);
    if (list.empty()) {
      DrawParagraph(kSmall, ImVec2(recent.Min.x, ry + Px(16.0f)), recent.GetWidth(), 1.5f, col::kTextDim, t_.recent_empty);
    }
    for (const auto& [g, i] : list) {
      if (ry + row_height > recent.Max.y) break;
      ProfileGame& game = profile_games_[g];
      const Achievement& a = game.achievements->items[i];
      ImRect row(ImVec2(recent.Min.x, ry), ImVec2(recent.Max.x, ry + row_height));
      FillRect(ImRect(ImVec2(row.Min.x, row.Max.y - Snap(Px(1.0f))), row.Max), col::kHairline);
      ImRect icon(ImVec2(row.Min.x, row.GetCenter().y - Px(22.0f)), ImVec2(row.Min.x + Px(44.0f), row.GetCenter().y + Px(22.0f)));
      DrawAchievementIcon(ProfileGameIcon(game, i), icon, true, false);
      std::string score = std::to_string(a.gamerscore) + " G";
      float score_width = MeasureText(kRowScore, score).x;
      DrawLabel(kRowScore, ImVec2(row.Max.x - score_width, row.GetCenter().y - LineHeight(kRowScore) * 0.5f), col::kTextSoft,
                score);
      float tx = icon.Max.x + Px(14.0f);
      float tw = row.Max.x - Px(14.0f) - score_width - tx;
      float block = LineHeight(kRowName) + LineHeight(kSmall);
      float ty = row.GetCenter().y - block * 0.5f;
      DrawLabel(kRowName, ImVec2(tx, ty), col::kText, Ellipsize(kRowName, a.label, tw));
      constexpr uint64_t kUnixEpoch = 116'444'736'000'000'000ULL;
      std::string when = Capitalized(WhenText(int64_t((a.unlocked - std::min(a.unlocked, kUnixEpoch)) / 10'000'000ULL)));
      DrawLabel(kSmall, ImVec2(tx, ty + LineHeight(kRowName)), col::kTextMuted,
                Ellipsize(kSmall, game.info.name + " \xC2\xB7 " + when, tw));
      ry += row_height;
    }
  }

  // The showcase: the pinned achievements as rows with a neutral trophy; Edit opens the current game's list.
  {
    DrawLabel(kHeading, showcase.Min, col::kText, t_.showcase);
    constexpr Type kLink{Font::kSemiBold, 14};
    ImVec2 link_pos(showcase.Max.x - MeasureText(kLink, t_.edit).x - Px(4.0f) - Px(14.0f * 0.95f),
                    showcase.Min.y + (LineHeight(kHeading) - LineHeight(kLink)) * 0.5f);
    ImGui::BeginDisabled(profile_games_.empty());
    if (Link("##showcase_change", link_pos, t_.edit, 14.0f) && !profile_games_.empty()) {
      profile_game_ = 0;
      for (size_t n = 0; n < profile_games_.size(); ++n) {
        if (profile_games_[n].current) profile_game_ = n;
      }
      Navigate(Page::kProfileGame, false);
    }
    ImGui::EndDisabled();
    float sy = showcase.Min.y + Px(46.0f);
    FillRect(ImRect(ImVec2(showcase.Min.x, sy), ImVec2(showcase.Max.x, sy + Snap(Px(1.0f)))), col::kHairline);
    constexpr Type kName{Font::kSemiBold, 16};
    constexpr Type kGame{Font::kRegular, 13};
    constexpr Type kScore{Font::kDisplayMedium, 18, 0.01f};
    const float row_height = Px(50.0f);
    if (profile) {
      for (const std::string& entry : profile->showcase) {
        uint32_t title = 0, id = 0;
        if (std::sscanf(entry.c_str(), "%x:%u", &title, &id) != 2) continue;
        for (ProfileGame& game : profile_games_) {
          if (game.info.title_id != title || !game.achievements) continue;
          for (const Achievement& a : game.achievements->items) {
            if (a.id != id) continue;
            ImRect row(ImVec2(showcase.Min.x, sy), ImVec2(showcase.Max.x, sy + row_height));
            FillRect(ImRect(ImVec2(row.Min.x, row.Max.y - Snap(Px(1.0f))), row.Max), col::kHairline);
            DrawIcon(Icon::kTrophy, ImVec2(row.Min.x, row.GetCenter().y - Px(11.0f)), 22.0f, col::kTextMuted, 1.75f);
            std::string score = std::to_string(a.gamerscore) + " G";
            float score_width = MeasureText(kScore, score).x;
            DrawLabel(kScore, ImVec2(row.Max.x - score_width, row.GetCenter().y - LineHeight(kScore) * 0.5f), col::kTextSoft,
                      score);
            float tx = row.Min.x + Px(36.0f);
            float tw = row.Max.x - Px(12.0f) - score_width - tx;
            float block = LineHeight(kName) + LineHeight(kGame);
            float ty = row.GetCenter().y - block * 0.5f;
            DrawLabel(kName, ImVec2(tx, ty), col::kText, Ellipsize(kName, a.label, tw));
            DrawLabel(kGame, ImVec2(tx, ty + LineHeight(kName)), col::kTextMuted, Ellipsize(kGame, game.info.name, tw));
            sy += row_height;
          }
        }
      }
    }
    DrawParagraph(Type{Font::kRegular, 13}, ImVec2(showcase.Min.x, sy + Px(18.0f)), showcase.GetWidth(), 1.5f, col::kTextDim,
                  t_.showcase_empty);
  }
}

void App::ProfileGames(const ImRect& body) {
  if (profile_games_.empty()) {
    DrawParagraph(Type{Font::kRegular, 15}, body.Min, body.GetWidth(), 1.5f, col::kTextMuted, t_.games_hint);
    return;
  }
  // A row a game (the mockup's ".gm", 104 dp) between hairlines, scrolling when they do not fit; its two buttons the
  // controller's stops.
  FillRect(ImRect(body.Min, ImVec2(body.Max.x, body.Min.y + Snap(Px(1.0f)))), col::kHairline);
  ImRect area(ImVec2(body.Min.x, body.Min.y + Snap(Px(1.0f))), ImVec2(body.Max.x + Px(16.0f), body.Max.y));
  BeginScroll("##profile_games", area);
  ImVec2 origin = ImGui::GetCursorScreenPos();
  const float width = body.GetWidth();
  const float row_height = Px(104.0f);
  constexpr Type kTitle{Font::kDisplayMedium, 22, 0.01f};
  constexpr Type kSub{Font::kRegular, 14};
  constexpr Type kMark{Font::kSemiBold, 13};
  constexpr Type kStat{Font::kRegular, 14};
  constexpr Type kStatStrong{Font::kSemiBold, 14};
  constexpr Type kScore{Font::kSemiBold, 16};
  // The buttons' block as wide on every row (the widest Play or launcher button), so that the columns line up.
  KeyButtonSpec play_any{KeyLook::kGhost, 0, Type{Font::kSemiBold, 16}, 44, 20, 12, 22, 10};
  KeyButtonSpec play_this{KeyLook::kPrimary, 'A', Type{Font::kSemiBold, 16}, 44, 20, 12, 22, 10};
  const float widest_play = std::max({KeyButtonWidth(play_this, t_.play_game), KeyButtonWidth(play_any, t_.open_its_launcher),
                                      KeyButtonWidth(play_any, t_.not_installed)});
  const float see_block = KeyButtonWidth(play_any, t_.see_achievements);
  for (size_t n = 0; n < profile_games_.size(); ++n) {
    ProfileGame& game = profile_games_[n];
    ImRect row(ImVec2(origin.x, origin.y + float(n) * row_height), ImVec2(origin.x + width, origin.y + float(n + 1) * row_height));
    FillRect(ImRect(ImVec2(row.Min.x, row.Max.y - Snap(Px(1.0f))), row.Max), col::kHairline);
    float x = row.Min.x + Px(12.0f);
    GameTile(ImVec2(x, row.GetCenter().y - Px(36.0f)), 72.0f, game);
    x += Px(72.0f) + Px(20.0f);
    // The buttons at the right: its achievements, and Play (this game, the accent) or its launcher.
    ImGui::PushID(int(n));
    const Achievements* list = game.achievements ? &*game.achievements : nullptr;
    bool can = CanPlay(game);
    const char* play_text = game.current ? t_.play_game : can ? t_.open_its_launcher : t_.not_installed;
    KeyButtonSpec play_spec{game.current ? KeyLook::kPrimary : KeyLook::kGhost, game.current ? 'A' : char(0),
                            Type{Font::kSemiBold, 16}, 44, 20, 12, 22, 10};
    KeyButtonSpec see_spec{KeyLook::kGhost, 0, Type{Font::kSemiBold, 16}, 44, 20, 12, 22, 10};
    float buttons_right = row.Max.x - Px(12.0f);
    float play_width = KeyButtonWidth(play_spec, play_text);
    if (KeyButton("##play", ImVec2(buttons_right - play_width, row.GetCenter().y - Px(22.0f)), play_spec, play_text, can)
            .pressed) {
      PlayProfileGame(game);
    }
    float see_width = KeyButtonWidth(see_spec, t_.see_achievements);
    float see_x = buttons_right - play_width - Px(12.0f) - see_width;
    if (KeyButton("##see", ImVec2(see_x, row.GetCenter().y - Px(22.0f)), see_spec, t_.see_achievements, list != nullptr)
            .pressed) {
      profile_game_ = n;
      Navigate(Page::kProfileGame, false);
    }
    if (n == 0) page_focus_ = ImGui::GetItemID();
    ImGui::PopID();
    // The gamerscore, then the achievements' count, bar and percentage (the gamerscore's, as the Achievements tab's).
    float columns_right = buttons_right - widest_play - Px(12.0f) - see_block - Px(24.0f);
    std::string score = std::to_string(list ? list->gamerscore : 0) + " / " + std::to_string(list ? list->total_gamerscore : 0) + " G";
    float score_x = columns_right - Px(130.0f);
    DrawLabel(kScore, ImVec2(score_x, row.GetCenter().y - LineHeight(kScore) * 0.5f), col::kTextSoft,
              Ellipsize(kScore, score, Px(130.0f)));
    float progress_x = score_x - Px(20.0f) - Px(230.0f);
    int percent = list ? Percent(*list) : 0;
    {
      std::string count = list ? std::to_string(list->unlocked) + " / " + std::to_string(list->items.size()) : "\xE2\x80\x94";
      std::string word = std::string(t_.nav_achievements) + " ";
      float ly = row.GetCenter().y - (LineHeight(kStat) + Px(8.0f) + Px(6.0f)) * 0.5f;
      DrawLabel(kStat, ImVec2(progress_x, ly), col::kTextMuted, word);
      DrawLabel(kStatStrong, ImVec2(progress_x + MeasureText(kStat, word).x, ly), col::kText, count);
      std::string percent_text = std::to_string(percent) + " %";
      DrawLabel(kStatStrong, ImVec2(progress_x + Px(230.0f) - MeasureText(kStatStrong, percent_text).x, ly), col::kText,
                percent_text);
      float by = ly + LineHeight(kStat) + Px(8.0f);
      Progress(ImRect(ImVec2(progress_x, by), ImVec2(progress_x + Px(230.0f), by + Px(6.0f))), float(percent) / 100.0f,
               col::kChip, game.current ? col::kAccent : col::kTextMuted);
    }
    // The name, its playtime and last play, and "This launcher" by its own.
    float text_width = progress_x - Px(24.0f) - x;
    char hours[64], last[96];
    std::snprintf(hours, sizeof(hours), t_.hours_played, HoursText(game.playtime.seconds).c_str());
    std::snprintf(last, sizeof(last), t_.last_seen, Capitalized(WhenText(game.LastSeen())).c_str());
    float block = LineHeight(kTitle) + LineHeight(kSub) + (game.current ? Px(3.0f) + LineHeight(kMark) : 0.0f);
    float ty = row.GetCenter().y - block * 0.5f;
    DrawLabel(kTitle, ImVec2(x, ty), col::kText, Ellipsize(kTitle, Title(game.info.name), text_width));
    ty += LineHeight(kTitle);
    DrawLabel(kSub, ImVec2(x, ty), col::kTextMuted, Ellipsize(kSub, std::string(hours) + " \xC2\xB7 " + last, text_width));
    if (game.current) {
      ty += LineHeight(kSub) + Px(3.0f);
      ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(x + Px(4.0f), ty + LineHeight(kMark) * 0.5f), Px(4.0f), col::kAccent, 12);
      DrawLabel(kMark, ImVec2(x + Px(16.0f), ty), col::kTextSoft, t_.this_launcher);
    }
  }
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(width, float(profile_games_.size()) * row_height));
  EndScroll();
}

}  // namespace launcher
