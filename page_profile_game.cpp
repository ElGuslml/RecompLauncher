// A game's achievements in the profile (Gustavo's 2026-10-28 mockup, "Perfil · Logros de un juego"): any game whose
// launcher ran, opened from the games; its completion and totals, the unlocked, locked or all achievements (X), each
// unlocked one with a star for the profile's showcase (at most profiles::kShowcaseMax; A on the card, or the star).

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
  ImU32 accent = game.info.accent ? IM_COL32((game.info.accent >> 16) & 0xFF, (game.info.accent >> 8) & 0xFF,
                                             game.info.accent & 0xFF, 0xFF)
                                  : col::kAccent;
  const Achievements* list = game.achievements ? &*game.achievements : nullptr;
  int total = list ? int(list->items.size()) : 0;
  int unlocked = list ? list->unlocked : 0;
  ImRect main(ImVec2(rect.Min.x + Px(40.0f), rect.Min.y + Px(24.0f)), ImVec2(rect.Max.x - Px(40.0f), rect.Max.y - Px(20.0f)));
  bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);

  // The card: back to the games (B), the completion, the game and its totals; the filters right (X).
  ImRect card(main.Min, ImVec2(main.Max.x, main.Min.y + Px(96.0f)));
  FillRect(card, col::kPanel, 14.0f);
  StrokeRect(card, col::kHairline, 14.0f);
  {
    KeyButtonSpec back{KeyLook::kGhost, 'B', Type{Font::kBold, 15}, 44, 16, 22, 22, 8};
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    float x = card.Min.x + Px(20.0f);
    if (KeyButton("##back", ImVec2(x, card.GetCenter().y - Px(22.0f)), back, t_.my_games).pressed) {
      profile_tab_ = 1;
      Navigate(Page::kProfile, false);
    }
    ImGui::PopItemFlag();
    x += KeyButtonWidth(back, t_.my_games) + Px(24.0f);
    int percent = list && list->total_gamerscore
                      ? int(std::lround(double(list->gamerscore) * 100.0 / double(list->total_gamerscore)))
                      : 0;
    ImVec2 ring(x + Px(30.0f), card.GetCenter().y);
    Ring(ring, 28.0f, 5.0f, float(percent) / 100.0f, col::kPanelTrack, accent);
    constexpr Type kPercent{Font::kBold, 14};
    std::string percent_text = std::to_string(percent) + "%";
    DrawLabel(kPercent, ImVec2(ring.x - MeasureText(kPercent, percent_text).x * 0.5f, ring.y - LineHeight(kPercent) * 0.5f),
              col::kText, percent_text);
    x += Px(60.0f) + Px(22.0f);

    // The filters, right: X steps through them.
    char filter_unlocked[64], filter_locked[64], filter_all[64];
    std::snprintf(filter_unlocked, sizeof(filter_unlocked), t_.filter_unlocked, unlocked);
    std::snprintf(filter_locked, sizeof(filter_locked), t_.filter_locked, total - unlocked);
    std::snprintf(filter_all, sizeof(filter_all), "%s", t_.filter_all_short);
    const char* filters[] = {filter_unlocked, filter_locked, filter_all};
    constexpr Type kPill{Font::kBold, 14};
    float right = card.Max.x - Px(20.0f);
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    for (int i = 2; i >= 0; --i) {
      float width = MeasureText(kPill, filters[i]).x + Px(32.0f);
      ImRect pill(ImVec2(right - width, card.GetCenter().y - Px(20.0f)), ImVec2(right, card.GetCenter().y + Px(20.0f)));
      ImGui::PushID(i);
      ItemState state = Item("##filter", pill);
      ImGui::PopID();
      bool on = profile_game_filter_ == i;
      if (on) {
        FillRect(pill, col::kAccent, 20.0f);
      } else {
        FillRect(pill, state.hovered ? Brighten(col::kGhostFill) : col::kGhostFill, 20.0f);
        StrokeRect(pill, col::kGhostBorder, 20.0f);
      }
      DrawLabel(kPill, ImVec2(pill.Min.x + Px(16.0f), pill.Min.y), on ? col::kOnAccent : col::kText, filters[i],
                pill.GetHeight());
      if (state.pressed) profile_game_filter_ = i;
      right = pill.Min.x - Px(8.0f);
    }
    ImGui::PopItemFlag();
    float glyph = GlyphWidth(PadButton::kX, 24.0f);
    DrawGlyph(PadButton::kX, ImVec2(right - glyph, card.GetCenter().y - Px(12.0f)), 24.0f);
    if (!popup && ButtonPressed(PadButton::kX)) {
      profile_game_filter_ = (profile_game_filter_ + 1) % 3;
    }
    right -= glyph + Px(20.0f);

    // The game and its totals.
    constexpr Type kTitle{Font::kDisplayBold, 34};
    constexpr Type kStats{Font::kRegular, 13};
    float tw = right - x;
    float block = LineHeight(kTitle) - Px(6.0f) + LineHeight(kStats);
    float ty = card.GetCenter().y - block * 0.5f;
    DrawLabel(kTitle, ImVec2(x, ty - Px(3.0f)), col::kText, Ellipsize(kTitle, Upper(game.info.name), tw));
    char stats[128], vitrine[48];
    std::snprintf(stats, sizeof(stats), t_.game_stats_line, unlocked, total, int(list ? list->gamerscore : 0),
                  int(list ? list->total_gamerscore : 0));
    std::snprintf(vitrine, sizeof(vitrine), t_.showcase_count, int(profile->showcase.size()));
    DrawLabel(kStats, ImVec2(x, ty + LineHeight(kTitle) - Px(6.0f)), col::kTabText,
              Ellipsize(kStats, std::string(stats) + " \xC2\xB7 " + vitrine, tw));
  }
  if (!list) {
    return;
  }

  // The list, two columns.
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
  ImRect area(ImVec2(main.Min.x, card.Max.y + Px(16.0f)), main.Max);
  if (shown.empty()) {
    constexpr Type kText{Font::kRegular, 15};
    DrawLabel(kText, ImVec2(area.GetCenter().x - MeasureText(kText, t_.achievements_filter_empty).x * 0.5f, area.Min.y + Px(40.0f)),
              col::kTabText, t_.achievements_filter_empty);
    return;
  }
  BeginScroll("##profile_game_list", area);
  {
    ImVec2 origin = ImGui::GetCursorScreenPos();
    // 4 dp around the cards for the focus outline.
    float inner = area.GetWidth() - Px(8.0f) - (ImGui::GetCurrentWindow()->ScrollbarY ? Px(14.0f) : 0.0f);
    float gap_x = Px(12.0f);
    float gap_y = Px(10.0f);
    size_t columns = inner >= Px(900.0f) ? 2 : 1;
    float card_width = std::floor((inner - gap_x * float(columns - 1)) / float(columns));
    float card_height = Px(82.0f);
    bool full = profile->showcase.size() >= profiles::kShowcaseMax;
    for (size_t n = 0; n < shown.size(); ++n) {
      size_t index = shown[n];
      const Achievement& a = list->items[index];
      float cx = origin.x + Px(4.0f) + float(n % columns) * (card_width + gap_x);
      float cy = origin.y + Px(4.0f) + float(n / columns) * (card_height + gap_y);
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
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(inner, Px(8.0f) + float(rows) * (card_height + gap_y) - gap_y));
  }
  EndScroll();
}

bool App::AchievementCard(const ImRect& card, const Achievement& a, ImTextureID texture, int pin, bool pin_enabled) {
  constexpr Type kName{Font::kBold, 15};
  constexpr Type kText{Font::kRegular, 13};
  constexpr Type kStatus{Font::kBold, 12};
  constexpr Type kScore{Font::kDisplayBold, 20};
  constexpr Type kTag{Font::kBold, 10, 0.08f};
  bool unlocked = a.unlocked != 0;
  bool hidden = !unlocked && a.secret();
  ImGuiContext& g = *GImGui;
  ImGui::PushID(int(a.id));
  // The star (a game's achievements, an unlocked one): a round button at the right, registered before the card under
  // it (the first item under the mouse takes it); the controller's A on the card does the same.
  float right = card.Max.x - Px(14.0f);
  ImRect star(ImVec2(right - Px(38.0f), card.GetCenter().y - Px(19.0f)), ImVec2(right, card.GetCenter().y + Px(19.0f)));
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
  FillRect(card, state.hovered ? Brighten(col::kPanel) : col::kPanel, 10.0f);
  StrokeRect(card, col::kHairline, 10.0f);

  ImRect icon(ImVec2(card.Min.x + Px(14.0f), card.GetCenter().y - Px(24.0f)), ImVec2(card.Min.x + Px(62.0f), card.GetCenter().y + Px(24.0f)));
  DrawAchievementIcon(texture, icon, unlocked, hidden);

  if (pin >= 0) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 c = star.GetCenter();
    if (pin == 1) {
      draw->AddCircleFilled(c, Px(19.0f), Fade(col::kAccent, 0.15f), 32);
      draw->AddCircle(c, Px(18.5f), col::kAccent, 32, Px(1.0f));
    } else {
      draw->AddCircleFilled(c, Px(19.0f), star_state.hovered ? Brighten(col::kGhostFill) : col::kGhostFill, 32);
      draw->AddCircle(c, Px(18.5f), col::kGhostBorder, 32, Px(1.0f));
    }
    ImU32 star_color = pin == 1 ? col::kAccent : pin_active ? col::kTextSoft : Fade(col::kTextDim, 0.5f);
    DrawIconSvg("star", kStar, ImVec2(c.x - Px(9.0f), c.y - Px(9.0f)), 18.0f, star_color, pin == 1 ? 3.0f : 1.8f);
    right = star.Min.x - Px(14.0f);
  }

  // The gamerscore, right.
  std::string score = std::to_string(a.gamerscore) + " G";
  float score_width = MeasureText(kScore, score).x;
  DrawLabel(kScore, ImVec2(right - score_width, card.Min.y), unlocked ? col::kAccent : col::kTextDim, score,
            card.GetHeight());

  // The name (and SECRET on a secret one unlocked), the description, the date or Locked.
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
  float block = LineHeight(kName) + LineHeight(kText) + LineHeight(kStatus);
  float ty = card.GetCenter().y - block * 0.5f;
  std::string shown_name = Ellipsize(kName, name, tw);
  DrawLabel(kName, ImVec2(tx, ty), col::kText, shown_name);
  if (unlocked && a.secret()) {
    std::string tag = Upper(t_.secret_tag);
    float tag_x = tx + MeasureText(kName, shown_name).x + Px(8.0f);
    ImRect chip(ImVec2(tag_x, ty + (LineHeight(kName) - Px(16.0f)) * 0.5f),
                ImVec2(tag_x + MeasureText(kTag, tag).x + Px(12.0f), ty + (LineHeight(kName) + Px(16.0f)) * 0.5f));
    if (chip.Max.x < tx + tw) {
      FillRect(chip, Hex(0x2C2840), 4.0f);
      DrawLabel(kTag, ImVec2(chip.Min.x + Px(6.0f), chip.Min.y), Hex(0xC9C0F2), tag, chip.GetHeight());
    }
  }
  DrawLabel(kText, ImVec2(tx, ty + LineHeight(kName)), col::kTabText, Ellipsize(kText, description, tw));
  DrawLabel(kStatus, ImVec2(tx, ty + LineHeight(kName) + LineHeight(kText)), unlocked ? col::kSuccess : col::kLabel,
            Ellipsize(kStatus, status, tw));
  if (state.focused) {
    FocusRing(card, 10.0f);
  }
  ImGui::PopID();
  return pressed;
}

}  // namespace launcher
