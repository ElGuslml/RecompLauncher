// The Play page: the game's name over the art with PLAY and Quit, the last session under them, and on the right the
// achievements, the current settings and the updates, each a card that opens its tab (Gustavo's 2026-10-28 mockup).

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "app.h"

namespace launcher {

using namespace ui;

namespace {

constexpr Type kCardTitle{Font::kBold, 12, 0.12f};
constexpr Type kChip{Font::kSemiBold, 13};

std::string SizeText(Size size) { return std::to_string(size.first) + " × " + std::to_string(size.second); }

// A card of the right column: the panel over the art, its title and a chevron when it opens a tab. Returns true when
// pressed.
bool Card(const char* id, const ImRect& rect, const char* title, bool link) {
  ItemState state = link ? Item(id, rect) : ItemState{};
  FillRect(rect, state.hovered ? Brighten(col::kPanel) : col::kPanel, 12.0f);
  StrokeRect(rect, col::kHairline, 12.0f);
  if (title) {
    ImVec2 p(rect.Min.x + Px(20.0f), rect.Min.y + Px(18.0f));
    DrawLabel(kCardTitle, p, col::kTabText, title);
    if (link) {
      DrawIcon(Icon::kChevronRight, ImVec2(rect.Max.x - Px(20.0f) - Px(16.0f), p.y + (LineHeight(kCardTitle) - Px(16.0f)) * 0.5f),
               16.0f, col::kTabText, 2.0f);
    }
  }
  if (state.focused) {
    FocusRing(rect, 12.0f);
  }
  return state.pressed;
}

}  // namespace

void App::PlayPage(const ImRect& rect) {
  const Options& o = settings_.o;

  // The right column: 340 dp wide, 28 dp under the header, 32 dp from the window's right edge.
  float column_x = rect.Max.x - Px(32.0f) - Px(340.0f);
  float y = rect.Min.y + Px(28.0f);
  float column_right = rect.Max.x - Px(32.0f);

  // The achievements: "7 / 50", the gamerscore, the progress. Before the game has written the list, a line instead.
  {
    constexpr Type kCount{Font::kDisplayBold, 30};
    constexpr Type kScore{Font::kBold, 14};
    float height = Px(18.0f) + LineHeight(kCardTitle) + Px(10.0f) + LineHeight(kCount) + Px(10.0f) + Px(8.0f) + Px(18.0f);
    ImRect card(ImVec2(column_x, y), ImVec2(column_right, y + height));
    if (Card("##achievements", card, t_.achievements_card, true)) {
      Navigate(Page::kAchievements, false);
    }
    float inner_y = card.Min.y + Px(18.0f) + LineHeight(kCardTitle) + Px(10.0f);
    float left = card.Min.x + Px(20.0f);
    float right = card.Max.x - Px(20.0f);
    if (!achievements_ || achievements_->items.empty()) {
      DrawParagraph(Type{Font::kRegular, 14}, ImVec2(left, inner_y), right - left, 1.4f, col::kTextMuted,
                    t_.achievements_after_play);
    } else {
      const Achievements& list = *achievements_;
      int total = int(list.items.size());
      std::string count = std::to_string(list.unlocked) + " / " + std::to_string(total);
      DrawLabel(kCount, ImVec2(left, inner_y), col::kText, count);
      std::string score = std::to_string(list.gamerscore) + " / " + std::to_string(list.total_gamerscore) + " G";
      // Baselines together ("align-items: baseline"): each line's baseline is its top plus its font's ascent (Oswald
      // 1.193 em, Source Sans 3 1.024 em, ui.cpp).
      float score_y = inner_y + Px(30.0f) * 1.193f - Px(14.0f) * 1.024f;
      DrawLabel(kScore, ImVec2(right - MeasureText(kScore, score).x, score_y), col::kAccent, score);
      float bar_y = inner_y + LineHeight(kCount) + Px(10.0f);
      Progress(ImRect(ImVec2(left, bar_y), ImVec2(right, bar_y + Px(8.0f))),
               total ? float(list.unlocked) / float(total) : 0.0f, col::kPanelTrack, col::kAccent);
    }
    y = card.Max.y + Px(12.0f);
  }

  // The current settings, as chips that wrap.
  {
    std::vector<std::string> chips;
    chips.push_back(SizeText(settings_.OutputSize()));
    chips.push_back(o.display == kFullscreen ? t_.fullscreen : o.display == kBorderless ? t_.borderless : t_.window);
    if (game::kHasNativeXRenderer) {
      chips.push_back(o.native ? "NativeXRenderer" : "Xenos");
    }
    if (*game::kFrameRateCvar) {
      for (const game::FrameRate& rate : game::kFrameRates) {
        if (rate.value == o.frame_rate) chips.push_back(std::to_string(rate.fps) + " FPS");
      }
    }
    if (o.native) {
      if (o.native_aa < kNativeAntialiasingCount && o.native_aa != kAaOff) {
        constexpr const char* kNames[] = {"SMAA", "DLAA", "FXAA", ""};
        chips.push_back(kNames[o.native_aa]);
      } else if (o.native_aa >= kNativeAntialiasingCount && o.native_aa_experimental >= 0) {
        chips.push_back(kExperimentalAntialiasing[o.native_aa_experimental].label);
      }
    } else if (*game::kNoMsaaCvar && !o.no_msaa) {
      chips.push_back(game::kMsaaLabel);
    }
    float left = column_x + Px(20.0f);
    float right = column_right - Px(20.0f);
    float chip_height = LineHeight(kChip) + Px(10.0f);
    std::vector<ImRect> places;
    float chip_x = left;
    float chip_y = 0.0f;
    for (const std::string& chip : chips) {
      float width = MeasureText(kChip, chip).x + Px(20.0f);
      if (chip_x > left && chip_x + width > right) {
        chip_x = left;
        chip_y += chip_height + Px(6.0f);
      }
      places.emplace_back(ImVec2(chip_x, chip_y), ImVec2(chip_x + width, chip_y + chip_height));
      chip_x += width + Px(6.0f);
    }
    float top = Px(18.0f) + LineHeight(kCardTitle) + Px(12.0f);
    float height = top + (places.empty() ? 0.0f : places.back().Max.y) + Px(18.0f);
    ImRect card(ImVec2(column_x, y), ImVec2(column_right, y + height));
    if (Card("##settings", card, t_.settings_card, true)) {
      Navigate(Page::kVideo, false);
    }
    for (size_t i = 0; i < places.size(); ++i) {
      ImRect place(ImVec2(places[i].Min.x, card.Min.y + top + places[i].Min.y),
                   ImVec2(places[i].Max.x, card.Min.y + top + places[i].Max.y));
      FillRect(place, col::kPanelChip, 4.0f);
      DrawLabel(kChip, ImVec2(place.Min.x + Px(10.0f), place.Min.y), col::kText, chips[i], place.GetHeight());
    }
    y = card.Max.y + Px(12.0f);
  }

  // The updates: a dot (green, accent for a new version), the state and the version with the last check.
  {
    constexpr Type kState{Font::kSemiBold, 15};
    constexpr Type kDetail{Font::kRegular, 13};
    float height = Px(16.0f) * 2.0f + LineHeight(kState) + Px(2.0f) + LineHeight(kDetail);
    ImRect card(ImVec2(column_x, y), ImVec2(column_right, y + height));
    const Release* newer = updates_ ? updates_->Newer(Version::Parse(kVersion)) : nullptr;
    if (Card("##updates", card, nullptr, !update_url_.empty())) {
      Navigate(Page::kUpdates, false);
    }
    float dot = Px(10.0f);
    float left = card.Min.x + Px(20.0f);
    FillRect(ImRect(ImVec2(left, card.GetCenter().y - dot * 0.5f), ImVec2(left + dot, card.GetCenter().y + dot * 0.5f)),
             newer ? col::kAccent : col::kSuccess, 5.0f);
    float text_x = left + dot + Px(14.0f);
    float text_width = card.Max.x - Px(20.0f) - text_x;
    char state[128];
    if (newer) {
      std::string number = newer->tag.size() > 1 && (newer->tag[0] == 'v' || newer->tag[0] == 'V') ? newer->tag.substr(1)
                                                                                                    : newer->tag;
      std::snprintf(state, sizeof(state), t_.update_card_available, number.c_str());
    } else {
      std::snprintf(state, sizeof(state), "%s", t_.all_up_to_date);
    }
    char detail[192];
    if (update_url_.empty()) {
      std::snprintf(detail, sizeof(detail), "%s", t_.updates_off);
    } else {
      std::snprintf(detail, sizeof(detail), t_.version_checked, kVersion, LastCheckedText().c_str());
    }
    float text_y = card.Min.y + Px(16.0f);
    DrawLabel(kState, ImVec2(text_x, text_y), col::kText, Ellipsize(kState, state, text_width));
    DrawLabel(kDetail, ImVec2(text_x, text_y + LineHeight(kState) + Px(2.0f)), col::kTabText,
              Ellipsize(kDetail, detail, text_width));
  }

  // The game's name and the buttons: 56 dp from the left, their foot 44 dp above the footer.
  constexpr Type kKicker{Font::kBold, 14, 0.24f};
  constexpr Type kName{Font::kDisplayBold, 96, 0.01f};
  constexpr Type kTagline{Font::kRegular, 16};
  constexpr Type kSession{Font::kRegular, 13};
  float left = rect.Min.x + Px(56.0f);
  float name_box = Px(96.0f) * 0.95f;  // "line-height: 0.95"
  float buttons_height = Px(68.0f);
  float bottom = rect.Max.y - Px(44.0f);
  float session_y = bottom - LineHeight(kSession);
  float buttons_y = session_y - Px(18.0f) - buttons_height;
  float tagline_y = buttons_y - Px(28.0f) - LineHeight(kTagline);
  float name_y = tagline_y - Px(14.0f) - name_box;
  float kicker_y = name_y - Px(4.0f) - LineHeight(kKicker);

  // A notice above the name: the game closed with an error, the saves were copied, the launcher was just updated.
  ImRect notice(ImVec2(left, rect.Min.y + Px(28.0f)), ImVec2(left + Px(620.0f), rect.Min.y + Px(28.0f) + Px(52.0f)));
  if (game_exit_code_ != 0) {
    // A crash's code (0xC0000005) in hexadecimal, a small one as it is.
    char code[16];
    std::snprintf(code, sizeof(code), game_exit_code_ < 0x10000 ? "%u" : "0x%08X", game_exit_code_);
    char text[256];
    std::snprintf(text, sizeof(text), t_.game_closed_error, code);
    Notice(notice, true, text, t_.open_logs, Page::kCount);
  } else if (!profile_notice_.empty()) {
    Notice(notice, profile_notice_error_, profile_notice_.c_str(), nullptr, Page::kCount);
  } else if (!updated_from_.empty()) {
    char text[128];
    std::snprintf(text, sizeof(text), t_.updated_to, kVersion);
    Notice(notice, false, text, t_.see_whats_new, Page::kUpdates);
  }

  DrawLabel(kKicker, ImVec2(left, kicker_y), col::kAccent, Upper(game::kArtKicker));
  DrawLabel(kName, ImVec2(left, name_y), col::kText, Upper(game::kArtTitle), name_box);
  DrawLabel(kTagline, ImVec2(left, tagline_y), col::kTextSoft, t_.tagline);

  // PLAY: accent, the A button's glyph (dark) before its name.
  {
    constexpr Type kPlay{Font::kDisplayBold, 28, 0.06f};
    std::string label = Upper(t_.play);
    float glyph = GlyphWidth(PadButton::kA, 30.0f);
    float width = Px(44.0f) * 2.0f + glyph + Px(14.0f) + MeasureText(kPlay, label).x;
    ImRect button(ImVec2(left, buttons_y), ImVec2(left + width, buttons_y + buttons_height));
    ItemState state = Item("##play", button);
    FillRect(button, state.hovered ? Brighten(col::kAccent) : col::kAccent, 10.0f);
    DrawGlyph(PadButton::kA, ImVec2(button.Min.x + Px(44.0f), button.GetCenter().y - Px(15.0f)), 30.0f, true);
    DrawLabel(kPlay, ImVec2(button.Min.x + Px(44.0f) + glyph + Px(14.0f), button.Min.y), col::kOnAccent, label,
              buttons_height);
    // The window's first focus: Enter or A plays straight away.
    ImGui::SetItemDefaultFocus();
    page_focus_ = ImGui::GetItemID();
    if (state.focused) {
      FocusRing(button, 10.0f);
    }
    if (state.pressed) {
      result_ = Result::kPlay;
    }
    // Quit, dark over the art.
    constexpr Type kQuit{Font::kSemiBold, 16};
    float quit_width = Px(26.0f) * 2.0f + MeasureText(kQuit, t_.quit).x;
    ImRect quit(ImVec2(button.Max.x + Px(14.0f), buttons_y), ImVec2(button.Max.x + Px(14.0f) + quit_width, button.Max.y));
    ItemState quit_state = Item("##quit", quit);
    FillRect(quit, quit_state.hovered ? Brighten(col::kGhostFill) : col::kGhostFill, 10.0f);
    StrokeRect(quit, col::kGhostBorder, 10.0f);
    DrawLabel(kQuit, ImVec2(quit.Min.x + Px(26.0f), quit.Min.y), col::kText, t_.quit, quit.GetHeight());
    if (quit_state.focused) {
      FocusRing(quit, 10.0f);
    }
    if (quit_state.pressed) {
      result_ = Result::kQuit;
    }
  }

  // The last session with this profile, and the hours in all.
  std::string session;
  for (const ProfileGame& game : profile_games_) {
    if (game.current && game.playtime.last) {
      char text[160];
      std::snprintf(text, sizeof(text), t_.last_session, WhenText(game.playtime.last).c_str(),
                    HoursText(game.playtime.seconds).c_str());
      session = text;
    }
  }
  DrawLabel(kSession, ImVec2(left, session_y), col::kTabText, session.empty() ? t_.no_session : session);
}

void App::Notice(const ImRect& rect, bool error, const char* text, const char* link, Page link_page) {
  ImU32 color = error ? col::kError : col::kSuccess;
  FillRect(rect, error ? col::kErrorTint : col::kNoticeTint, 10.0f);
  StrokeRect(rect, Fade(color, 0.55f), 10.0f);
  ImVec2 icon(rect.Min.x + Px(16.0f), rect.Min.y + (rect.GetHeight() - Px(20.0f)) * 0.5f);
  DrawIcon(error ? Icon::kAlert : Icon::kCheck, icon, 20.0f, color, 2.0f);
  constexpr Type kText{Font::kMedium, 14};
  float x = rect.Min.x + Px(48.0f);
  DrawLabel(kText, ImVec2(x, rect.Min.y), col::kText, text, rect.GetHeight());
  constexpr Type kLinkType{Font::kSemiBold, 14};
  if (!link || !*link) {
    return;
  }
  float link_width = MeasureText(kLinkType, link).x + Px(20.0f);
  float link_x = std::max(x + MeasureText(kText, text).x + Px(24.0f), rect.Max.x - Px(16.0f) - link_width);
  if (Link("notice", ImVec2(link_x, rect.Min.y + (rect.GetHeight() - LineHeight(kLinkType)) * 0.5f), link)) {
    if (link_page == Page::kCount) {
      OpenFolder(exe_dir_ / L"logs");
    } else {
      Navigate(link_page, false);
    }
  }
}

}  // namespace launcher
