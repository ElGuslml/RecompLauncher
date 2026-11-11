// The Play page (launcher 3.18, Gustavo's 2026-11-11 mockup "Camerino"): the art in full, the game's name over it, its
// line under it, one main action (Play) beside Quit, and under them a slim strip of the state: the last session, the
// achievements, the screen and frame rate, the updates; each of the last three opens its tab.

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "app.h"

namespace launcher {

using namespace ui;

namespace {

std::string SizeText(Size size) { return std::to_string(size.first) + " × " + std::to_string(size.second); }

}  // namespace

void App::PlayPage(const ImRect& rect) {
  const Options& o = settings_.o;

  // From the bottom (dp, the mockup's board): the strip 28 above the footer and 44 high, the buttons 48 above it and
  // 64 high, the line 44 above them, the name 8 above the line.
  constexpr Type kName{Font::kDisplayBold, game::kArtTitleSize, 0.01f};
  constexpr Type kTagline{Font::kRegular, 18};
  const float left = rect.Min.x + Px(56.0f);
  const float strip_height = Px(44.0f);
  const float strip_y = rect.Max.y - Px(28.0f) - strip_height;
  const float buttons_height = Px(64.0f);
  const float buttons_y = strip_y - Px(48.0f) - buttons_height;
  const float tagline_y = buttons_y - Px(44.0f);
  const float name_box = Px(game::kArtTitleSize);  // "line-height: 1"
  const float name_y = tagline_y - Px(8.0f) - name_box;

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

  // The name (4 dp further left: Oswald's capitals sit right of their box) and its line: the brand and the tagline.
  DrawLabel(kName, ImVec2(left - Px(4.0f), name_y), col::kText, Upper(game::kArtTitle), name_box);
  std::string tagline = t_.tagline;
  if (*game::kBrandTitle && std::string_view(game::kBrandTitle) != game::kArtTitle) {
    tagline = std::string(game::kBrandTitle) + " \xC2\xB7 " + tagline;
  }
  DrawLabel(kTagline, ImVec2(left, tagline_y), col::kTextSoft, tagline);

  // Play: the accent, the A button's glyph before its name; the window's first focus (Enter or A plays at once).
  {
    constexpr Type kPlay{Font::kDisplaySemiBold, 28, 0.02f};
    constexpr float kGlyph = 28.0f;
    const char* label = t_.nav_play;
    float glyph = GlyphWidth(PadButton::kA, kGlyph);
    float width = Px(20.0f) + glyph + Px(14.0f) + MeasureText(kPlay, label).x + Px(30.0f);
    ImRect button(ImVec2(left + Px(2.0f), buttons_y), ImVec2(left + Px(2.0f) + width, buttons_y + buttons_height));
    ItemState state = Item("##play", button);
    FillRect(button, state.hovered ? Brighten(col::kAccent) : col::kAccent, 14.0f);
    DrawGlyph(PadButton::kA, ImVec2(button.Min.x + Px(20.0f), button.GetCenter().y - Px(kGlyph) * 0.5f), kGlyph, true);
    DrawLabel(kPlay, ImVec2(button.Min.x + Px(20.0f) + glyph + Px(14.0f), button.Min.y), col::kOnAccent, label,
              buttons_height);
    ImGui::SetItemDefaultFocus();
    page_focus_ = ImGui::GetItemID();
    if (state.focused) {
      FocusRing(button, 14.0f);
    }
    if (state.pressed) {
      result_ = Result::kPlay;
    }
    // Quit: an outline, 18 dp right of Play and centred on it.
    ButtonSpec quit{ButtonKind::kGhost, Type{Font::kSemiBold, 16}, 48, 24, 12};
    if (Button("##quit", ImVec2(button.Max.x + Px(18.0f), button.GetCenter().y - Px(24.0f)), quit, t_.quit)) {
      result_ = Result::kQuit;
    }
  }

  // The strip: the panel over the art, its parts separated by hairlines.
  struct Part {
    Icon icon;
    ImU32 icon_color;
    std::string text;
    Page opens;  // kCount: none
  };
  std::vector<Part> parts;
  {
    std::string session = t_.no_session;
    for (const ProfileGame& game : profile_games_) {
      if (game.current && game.playtime.last) {
        char text[160];
        std::snprintf(text, sizeof(text), t_.last_session, WhenText(game.playtime.last).c_str(),
                      HoursText(game.playtime.seconds).c_str());
        session = text;
      }
    }
    parts.push_back({Icon::kClock, col::kTextDim, session, Page::kCount});
  }
  if (achievements_ && !achievements_->items.empty()) {
    char text[96];
    std::snprintf(text, sizeof(text), t_.strip_achievements, achievements_->unlocked, int(achievements_->items.size()));
    parts.push_back({Icon::kTrophy, col::kTextDim, text, Page::kAchievements});
  }
  {
    std::string screen = SizeText(settings_.OutputSize());
    if (*game::kFrameRateCvar) {
      for (const game::FrameRate& rate : game::kFrameRates) {
        if (rate.value == o.frame_rate) screen += " \xC2\xB7 " + std::to_string(rate.fps) + " FPS";
      }
    }
    parts.push_back({Icon::kSliders, col::kTextDim, screen, Page::kVideo});
  }
  if (!update_url_.empty()) {
    const Release* newer = updates_ ? updates_->Newer(Version::Parse(kVersion)) : nullptr;
    if (newer) {
      std::string number = newer->tag.size() > 1 && (newer->tag[0] == 'v' || newer->tag[0] == 'V') ? newer->tag.substr(1)
                                                                                                    : newer->tag;
      char text[128];
      std::snprintf(text, sizeof(text), t_.update_card_available, number.c_str());
      parts.push_back({Icon::kArrowDown, col::kAccent, text, Page::kUpdates});
    } else {
      parts.push_back({Icon::kCheck, col::kSuccess, t_.all_up_to_date, Page::kUpdates});
    }
  }
  constexpr Type kPart{Font::kMedium, 15};
  float strip_width = Px(8.0f);
  for (const Part& part : parts) strip_width += Px(32.0f) + Px(18.0f) + Px(8.0f) + MeasureText(kPart, part.text).x;
  ImRect strip(ImVec2(left, strip_y), ImVec2(left + strip_width, strip_y + strip_height));
  FillRect(strip, col::kPanel, 12.0f);
  StrokeRect(strip, col::kHairline, 12.0f);
  float x = strip.Min.x + Px(4.0f);
  for (size_t i = 0; i < parts.size(); ++i) {
    const Part& part = parts[i];
    float width = Px(32.0f) + Px(18.0f) + Px(8.0f) + MeasureText(kPart, part.text).x;
    ImRect r(ImVec2(x, strip.Min.y), ImVec2(x + width, strip.Max.y));
    ItemState state;
    if (part.opens != Page::kCount) {
      ImGui::PushID(int(i));
      state = Item("##part", r);
      ImGui::PopID();
      if (state.pressed) Navigate(part.opens, PressedByNav());
    }
    if (i > 0) {
      FillRect(ImRect(ImVec2(r.Min.x, r.Min.y + Px(13.0f)), ImVec2(r.Min.x + Snap(Px(1.0f)), r.Max.y - Px(13.0f))),
               col::kHairline);
    }
    DrawIcon(part.icon, ImVec2(r.Min.x + Px(16.0f), r.GetCenter().y - Px(9.0f)), 18.0f, part.icon_color, 1.75f);
    DrawLabel(kPart, ImVec2(r.Min.x + Px(16.0f) + Px(18.0f) + Px(8.0f), r.Min.y),
              state.hovered ? col::kText : col::kTextSoft, part.text, r.GetHeight());
    if (state.focused) {
      FocusRing(ImRect(ImVec2(r.Min.x + Px(4.0f), r.Min.y + Px(4.0f)), ImVec2(r.Max.x - Px(4.0f), r.Max.y - Px(4.0f))),
                8.0f);
    }
    x += width;
  }
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
