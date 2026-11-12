// The launcher's shell (app.h): the game's art under every page, the header with the tabs, Donate and the profile's
// avatar, and the footer with the controller and the version on the left and its buttons' hints on the right. Gustavo's
// 2026-10-28 mockup ("GHMetallica Launcer 2"): a header 72 dp high and a footer 52 dp high over a 1280 x 760 window.

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_joystick.h>

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>

#include "app.h"
#include "game.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

// Where Donate leads (the header's button and Y on the Play page), the same for every game.
constexpr const char* kDonateUrl = "https://ko-fi.com/elguslml";

// The tabs, in the header's order (game.h decides whether the game has a DLC page). The Settings tab opens on its first
// category, Game (launcher 3.18's order: Game, Video, Graphics).
constexpr Page kTabs[] = {Page::kPlay, Page::kDlc, Page::kAchievements, Page::kControls, Page::kGame, Page::kUpdates};

bool HasPage(Page page) { return page != Page::kDlc || game::kHasDlcPage; }

// The tab a page is under: the settings' categories are one tab; the profile's pages none.
Page TabOf(Page page) {
  switch (page) {
    case Page::kVideo:
    case Page::kGraphics:
    case Page::kAudio:
      return Page::kGame;
    case Page::kProfiles:
    case Page::kProfile:
    case Page::kProfileGame:
    case Page::kProfileEdit:
      return Page::kCount;
    default:
      return page;
  }
}

// The connected controller the footer names: a guitar first, then a drum kit, then any other.
struct Device {
  Icon icon = Icon::kKeyboard;
  bool connected = false;
  int kind = 0;  // 0 keyboard and mouse, 1 guitar, 2 drum kit, 3 gamepad
};

Device ConnectedDevice() {
  Device device;
  int count = 0;
  SDL_JoystickID* pads = SDL_GetGamepads(&count);
  for (int i = 0; pads && i < count; ++i) {
    SDL_JoystickType type = SDL_GetJoystickTypeForID(pads[i]);
    int kind = type == SDL_JOYSTICK_TYPE_GUITAR ? 1 : type == SDL_JOYSTICK_TYPE_DRUM_KIT ? 2 : 3;
    if (!device.connected || kind < device.kind) {
      device.connected = true;
      device.kind = kind;
      device.icon = kind == 1 ? Icon::kGuitar : kind == 2 ? Icon::kDrum : Icon::kGamepad;
    }
  }
  SDL_free(pads);
  return device;
}

}  // namespace

bool App::OnTab(Page tab) const { return TabOf(page_) == tab; }

void App::NextTab(int step) {
  Page current = TabOf(page_);
  int count = int(std::size(kTabs));
  int index = int(std::find(std::begin(kTabs), std::end(kTabs), current) - std::begin(kTabs));
  if (index >= count) {
    index = step > 0 ? -1 : 0;  // from a profile page: the first or the last tab
  }
  int i = index;
  do {
    i = (i + count + step) % count;
  } while (!HasPage(kTabs[i]));
  Navigate(kTabs[i], true);
}

void App::Donate() { OpenUrl(kDonateUrl); }

bool App::ColumnNav(std::span<const std::array<ImGuiID, 2>> rows) {
  ImGuiContext& g = *GImGui;
  if (!g.NavMoveSubmitted || (g.NavMoveDir != ImGuiDir_Up && g.NavMoveDir != ImGuiDir_Down)) {
    return false;
  }
  for (size_t r = 0; r < rows.size(); ++r) {
    int column = rows[r][0] == g.NavId ? 0 : rows[r][1] == g.NavId ? 1 : -1;
    if (column < 0) {
      continue;
    }
    if (g.NavMoveDir == ImGuiDir_Down) {
      if (r + 1 < rows.size()) RequestFocus(rows[r + 1][size_t(column)]);
    } else if (r > 0) {
      RequestFocus(rows[r - 1][size_t(column)]);
    } else if (header_focus_) {
      RequestFocus(header_focus_);
    }
    ImGui::NavMoveRequestCancel();
    return true;
  }
  return false;
}

Page App::BackPage() const {
  switch (page_) {
    case Page::kProfile:
      return Page::kProfiles;
    case Page::kProfileGame:
      return Page::kProfile;
    case Page::kProfileEdit:
      return edit_back_;
    default:
      return Page::kPlay;
  }
}

void App::Background(ImVec2 size, float dim) {
  ImVec2 origin = ImGui::GetMainViewport()->Pos;
  ImRect window(origin, ImVec2(origin.x + size.x, origin.y + size.y));
  FillRect(window, col::kBackground);
  // The art covers the window: in another shape than the mockup's 1280 x 760 it is cropped around its focus (game.h).
  DrawArt(Art::kBackground, window, ImVec2(game::kArtFocusX, game::kArtFocusY));
  if (dim > 0.0f) {
    FillRect(window, Fade(col::kDim, dim));
  }
}

void App::BackgroundCorner(ImVec2 size) {
  // Launcher 3.18 ("Camerino"): the art in full only on Play; under the other pages the window's own color and a corner
  // of the art at 9 %, with no panel over it.
  ImVec2 origin = ImGui::GetMainViewport()->Pos;
  ImRect window(origin, ImVec2(origin.x + size.x, origin.y + size.y));
  FillRect(window, col::kBackground);
  const game::ArtPart& part = game::kArtCorner;
  // The art's units at half the size the full art has in a 760 dp high window.
  float unit = Px(1.0f) * 0.5f;
  float width = (part.right - part.left) * unit;
  float height = (part.bottom - part.top) * unit;
  float top = window.Max.y - Px(game::kArtCornerRise);
  DrawArtPart(Art::kBackground, ImRect(ImVec2(window.Max.x - width, top), ImVec2(window.Max.x, top + height)),
              ImRect(ImVec2(part.left, part.top), ImVec2(part.right, part.bottom)), 0.09f);
}

void App::Header(const ImRect& rect) {
  FillRect(rect, col::kHeader);
  FillRect(ImRect(ImVec2(rect.Min.x, rect.Max.y - Snap(Px(1.0f))), rect.Max), col::kHairline);
  float left = rect.Min.x + Px(28.0f);
  float right = rect.Max.x - Px(28.0f);

  // The brand: the game's logo (its icon, or its mark on accent) and its name on two lines, the second one the
  // launcher's ("METALLICA · LAUNCHER").
  ImRect logo(ImVec2(left, rect.GetCenter().y - Px(18.0f)), ImVec2(left + Px(36.0f), rect.GetCenter().y + Px(18.0f)));
  GameLogo(logo, 20.0f);
  constexpr Type kBrandTitle{Font::kDisplaySemiBold, 17, 0.05f};
  constexpr Type kBrandSubtitle{Font::kDisplayMedium, 12, 0.14f};
  // A game without a brand line (a name without a kicker): the art's title, and the launcher's word under it.
  bool brand = *game::kBrandTitle;
  std::string title = Upper(brand ? game::kBrandTitle : game::kArtTitle);
  std::string subtitle = brand && *game::kBrandSubtitle ? Upper(game::kBrandSubtitle) + " \xC2\xB7 LAUNCHER" : "LAUNCHER";
  // "line-height: 1.05", the second line 3 dp lower.
  float title_box = Px(17.0f * 1.05f);
  float subtitle_box = Px(12.0f * 1.05f) + Px(3.0f);
  float text_y = rect.GetCenter().y - (title_box + subtitle_box) * 0.5f;
  float text_x = logo.Max.x + Px(12.0f);
  DrawLabel(kBrandTitle, ImVec2(text_x, text_y), col::kText, Ellipsize(kBrandTitle, title, Px(190.0f)), title_box);
  DrawLabel(kBrandSubtitle, ImVec2(text_x, text_y + title_box + Px(3.0f)), col::kLabel,
            Ellipsize(kBrandSubtitle, subtitle, Px(190.0f)), subtitle_box - Px(3.0f));

  // Right: Donate (an outline) and the profile's avatar (none on the first run), 14 dp apart.
  constexpr float kAvatar = 40.0f;
  float avatar_x = right - Px(kAvatar);
  const profiles::Profile* profile = installing_ ? nullptr : store_.Current();
  if (profile) {
    ImRect avatar(ImVec2(avatar_x, rect.GetCenter().y - Px(kAvatar) * 0.5f),
                  ImVec2(right, rect.GetCenter().y + Px(kAvatar) * 0.5f));
    ItemState state = Item("##avatar", avatar);
    header_items_.push_back(ImGui::GetItemID());
    if (TabOf(page_) == Page::kCount) header_focus_ = ImGui::GetItemID();
    DrawAvatar(avatar.Min, kAvatar, profile->avatar, AvatarTexture(*profile), profile->color);
    if (TabOf(page_) == Page::kCount) {
      // On the profile's pages: the selection's mark, an accent dot under the avatar.
      float dot = Px(8.0f);
      ImVec2 c(avatar.GetCenter().x, avatar.Max.y + Px(6.0f) + dot * 0.5f);
      ImGui::GetWindowDrawList()->AddCircleFilled(c, dot * 0.5f, col::kAccent, 16);
    }
    if (state.focused) {
      FocusRing(avatar, kAvatar * 0.5f);
    }
    if (state.pressed) {
      profiles_selected_ = profile->id;
      Navigate(Page::kProfiles, PressedByNav());
    }
  } else {
    avatar_x = right + Px(14.0f);
  }
  ButtonSpec donate{ButtonKind::kDonate, Type{Font::kSemiBold, 14}, 36, 14, 18, Icon::kHeart, 18, 1.8f, false};
  float donate_width = ButtonWidth(donate, t_.donate);
  if (Button("##donate", ImVec2(avatar_x - Px(14.0f) - donate_width, rect.GetCenter().y - Px(18.0f)), donate,
             t_.donate)) {
    Donate();
  }
  header_items_.push_back(ImGui::GetItemID());

  float nav_left = left + Px(230.0f) + Px(24.0f);
  float nav_right = right - Px(230.0f) - Px(24.0f);
  if (installing_) {
    InstallSteps(rect, nav_left, nav_right);
    return;
  }

  // The tabs, in the window's middle (not left of the brand's block), between LB and RB; the profile's pages have no
  // bumpers (LB and RB change their sections there).
  const char* labels[] = {t_.nav_play, t_.nav_dlc, t_.nav_achievements, t_.nav_controls, t_.nav_settings, t_.nav_updates};
  constexpr Type kTab{Font::kDisplayMedium, 18, 0.02f};
  constexpr float kPill = 22.0f;
  constexpr float kGap = 6.0f;
  const bool bumpers = TabOf(page_) != Page::kCount;
  float tabs_width = 0.0f;
  int shown = 0;
  for (int i = 0; i < int(std::size(kTabs)); ++i) {
    if (HasPage(kTabs[i])) {
      tabs_width += MeasureText(kTab, labels[i]).x + Px(32.0f);
      ++shown;
    }
  }
  tabs_width += Px(kGap) * float(shown - 1);
  if (bumpers) {
    tabs_width += GlyphWidth(PadButton::kLB, kPill) + GlyphWidth(PadButton::kRB, kPill) + Px(kGap) * 2.0f;
  }
  float x = Snap(std::max(nav_left, rect.GetCenter().x - tabs_width * 0.5f));
  auto bumper = [&](PadButton button) {
    DrawGlyph(button, ImVec2(x, rect.GetCenter().y - Px(kPill) * 0.5f), kPill);
    x += GlyphWidth(button, kPill) + Px(kGap);
  };
  if (bumpers) bumper(PadButton::kLB);
  for (int i = 0; i < int(std::size(kTabs)); ++i) {
    if (!HasPage(kTabs[i])) {
      continue;
    }
    const char* label = labels[i];
    float width = MeasureText(kTab, label).x + Px(32.0f);
    ImRect tab(ImVec2(x, rect.Min.y), ImVec2(x + width, rect.Max.y));
    ImGui::PushID(i);
    ItemState state = Item("##tab", tab);
    header_items_.push_back(ImGui::GetItemID());
    ImGui::PopID();
    bool active = OnTab(kTabs[i]);
    if (active) header_focus_ = header_items_.back();
    ImU32 color = active ? col::kText : state.hovered ? col::kTextSoft : col::kTextMuted;
    DrawLabel(kTab, ImVec2(tab.Min.x + Px(16.0f), tab.Min.y), color, label, tab.GetHeight());
    if (active) {
      // The selection's mark: 3 dp of accent under the label, its top corners rounded.
      FillRect(ImRect(ImVec2(tab.Min.x + Px(16.0f), tab.Max.y - Px(3.0f)), ImVec2(tab.Max.x - Px(16.0f), tab.Max.y)),
               col::kAccent, 3.0f, ImDrawFlags_RoundCornersTop);
    }
    // A new version: a dot on Updates.
    if (kTabs[i] == Page::kUpdates && updates_ && updates_->Newer(Version::Parse(kVersion))) {
      float dot = Px(7.0f);
      ImVec2 c(tab.Max.x - Px(10.0f), rect.GetCenter().y - Px(11.0f));
      FillRect(ImRect(ImVec2(c.x - dot * 0.5f, c.y - dot * 0.5f), ImVec2(c.x + dot * 0.5f, c.y + dot * 0.5f)),
               col::kAccent, 4.0f);
    }
    if (state.focused) {
      FocusRing(ImRect(ImVec2(tab.Min.x + Px(6.0f), tab.Min.y + Px(18.0f)), ImVec2(tab.Max.x - Px(6.0f), tab.Max.y - Px(18.0f))),
                6.0f);
    }
    if (state.pressed) {
      // With A, the focus goes into the page (as with LB/RB); a click leaves it.
      Navigate(kTabs[i], PressedByNav());
    }
    x += width + Px(kGap);
  }
  if (bumpers) bumper(PadButton::kRB);
}

// The first run's four steps (the mockup's install bar): done (a check on accent), current (an accent ring on a tint),
// to come (a grey ring); a line between each two, accent after a step done.
void App::InstallSteps(const ImRect& rect, float left, float right) {
  // Launcher 3.18 (the mockup's board 22): each step a numbered circle and its name in the tabs' type; the current one
  // white with the accent under it as a tab's, one done a check on the chips' grey; hairlines between them.
  const char* names[] = {t_.step_welcome, t_.step_iso, t_.step_install, t_.step_done};
  int current = int(install_step_);
  constexpr Type kName{Font::kDisplayMedium, 17, 0.02f};
  constexpr Type kNumber{Font::kSemiBold, 14};
  constexpr float kCircle = 28.0f;
  constexpr float kLine = 34.0f;
  constexpr float kGap = 6.0f;
  float width = 0.0f;
  for (int i = 0; i < 4; ++i) {
    width += Px(6.0f) * 2.0f + Px(kCircle) + Px(10.0f) + MeasureText(kName, names[i]).x;
    if (i < 3) width += Px(kGap) * 2.0f + Px(kLine);
  }
  float x = Snap(std::max(left, rect.GetCenter().x - width * 0.5f));
  float cy = rect.GetCenter().y;
  ImDrawList* draw = ImGui::GetWindowDrawList();
  for (int i = 0; i < 4; ++i) {
    bool done = i < current;
    bool now = i == current;
    float step_left = x;
    x += Px(6.0f);
    ImVec2 center(x + Px(kCircle) * 0.5f, cy);
    if (done) {
      draw->AddCircleFilled(center, Px(kCircle) * 0.5f, col::kChip, 32);
      DrawIcon(Icon::kCheck, ImVec2(center.x - Px(8.0f), cy - Px(8.0f)), 16.0f, col::kSuccess, 2.4f);
    } else {
      draw->AddCircle(center, Px(kCircle) * 0.5f - Px(0.75f), now ? col::kText : col::kOutline, 32, Px(1.5f));
      char number[4];
      std::snprintf(number, sizeof(number), "%d", i + 1);
      DrawLabel(kNumber, ImVec2(center.x - MeasureText(kNumber, number).x * 0.5f, cy - Px(kCircle) * 0.5f),
                now ? col::kText : col::kTextDim, number, Px(kCircle));
    }
    x += Px(kCircle) + Px(10.0f);
    DrawLabel(kName, ImVec2(x, rect.Min.y), now ? col::kText : done ? col::kTextMuted : col::kTextDim, names[i],
              rect.GetHeight());
    x += MeasureText(kName, names[i]).x + Px(6.0f);
    if (now) {
      FillRect(ImRect(ImVec2(step_left, rect.Max.y - Px(3.0f)), ImVec2(x, rect.Max.y)), col::kAccent, 3.0f,
               ImDrawFlags_RoundCornersTop);
    }
    if (i < 3) {
      x += Px(kGap);
      FillRect(ImRect(ImVec2(x, cy), ImVec2(x + Px(kLine), cy + Snap(Px(1.0f)))), col::kOutline);
      x += Px(kLine) + Px(kGap);
    }
  }
}

void App::Chrome(ImVec2 size) {
  ImVec2 origin = ImGui::GetMainViewport()->Pos;
  header_items_.clear();
  float header_height = Snap(Px(72.0f));
  float footer_height = Snap(Px(52.0f));
  // The header's tabs, Donate and the avatar are part of the page's navigation: up from the page's top reaches them.
  ImGui::SetCursorScreenPos(origin);
  ImGui::BeginChild("##header", ImVec2(size.x, header_height), ImGuiChildFlags_NavFlattened,
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  Header(ImRect(origin, ImVec2(origin.x + size.x, origin.y + header_height)));
  ImGui::EndChild();
  ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + size.y - footer_height));
  ImGui::BeginChild("##footer", ImVec2(size.x, footer_height), ImGuiChildFlags_None,
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
                        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
  std::vector<Hint> hints = PageHints();
  Footer(ImRect(ImVec2(origin.x, origin.y + size.y - footer_height), ImVec2(origin.x + size.x, origin.y + size.y)),
         hints);
  ImGui::EndChild();
}

void App::Footer(const ImRect& rect, std::span<const Hint> hints) {
  FillRect(rect, col::kFooter);
  FillRect(ImRect(rect.Min, ImVec2(rect.Max.x, rect.Min.y + Snap(Px(1.0f)))), col::kHairline);
  constexpr Type kText{Font::kRegular, 14};
  float x = rect.Min.x + Px(28.0f);

  // The controller that plays: a guitar, a drum kit, a gamepad, or the keyboard.
  Device device = ConnectedDevice();
  DrawIcon(device.icon, ImVec2(x, rect.GetCenter().y - Px(10.0f)), 20.0f, device.connected ? col::kSuccess : col::kLabel,
           1.75f);
  x += Px(20.0f) + Px(10.0f);
  const char* names[] = {t_.device_keyboard, t_.device_guitar, t_.device_drums, t_.device_gamepad};
  const char* name = names[device.kind];
  DrawLabel(kText, ImVec2(x, rect.Min.y), col::kTextMuted, name, rect.GetHeight());
  x += MeasureText(kText, name).x + Px(16.0f);
  char version[64];
  std::snprintf(version, sizeof(version), t_.version, kVersion);
  DrawLabel(kText, ImVec2(x, rect.Min.y), col::kTextDim, version, rect.GetHeight());

  // The hints, right-aligned, 28 dp apart: each the glyph of its button (the controller in use, or the keyboard) and
  // its label. Launcher 3.18: four at most, never the tabs' bumpers (the header shows them) nor Donate.
  constexpr float kGlyph = 22.0f;
  auto key_width = [&](HintKey key) {
    switch (key) {
      case HintKey::kBumpers:
        return GlyphWidth(PadButton::kLB, kGlyph) + Px(4.0f) + GlyphWidth(PadButton::kRB, kGlyph);
      case HintKey::kArrows:
        return Px(42.0f);
      default:
        return GlyphWidth(PadButton(int(key)), kGlyph);
    }
  };
  float width = 0.0f;
  for (size_t i = 0; i < hints.size(); ++i) {
    width += key_width(hints[i].key) + Px(8.0f) + MeasureText(kText, hints[i].label).x;
    width += i + 1 < hints.size() ? Px(28.0f) : 0.0f;
  }
  x = rect.Max.x - Px(28.0f) - width;
  for (const Hint& hint : hints) {
    float w = key_width(hint.key);
    ImVec2 at(x, rect.GetCenter().y - Px(kGlyph) * 0.5f);
    if (hint.key == HintKey::kBumpers) {
      DrawGlyph(PadButton::kLB, at, kGlyph);
      DrawGlyph(PadButton::kRB, ImVec2(at.x + GlyphWidth(PadButton::kLB, kGlyph) + Px(4.0f), at.y), kGlyph);
    } else if (hint.key == HintKey::kArrows) {
      // "‹ ›": two chevrons in a pill (the D-pad's, or the arrow keys).
      ImRect chip(at, ImVec2(at.x + w, at.y + Px(kGlyph)));
      FillRect(chip, col::kChip, kGlyph * 0.5f);
      ImVec2 c = chip.GetCenter();
      DrawIcon(Icon::kChevronLeft, ImVec2(c.x - Px(15.0f), c.y - Px(7.0f)), 14.0f, col::kTextMuted, 2.2f);
      DrawIcon(Icon::kChevronRight, ImVec2(c.x + Px(1.0f), c.y - Px(7.0f)), 14.0f, col::kTextMuted, 2.2f);
    } else {
      DrawGlyph(PadButton(int(hint.key)), at, kGlyph);
    }
    x += w + Px(8.0f);
    DrawLabel(kText, ImVec2(x, rect.Min.y), col::kTextMuted, hint.label, rect.GetHeight());
    x += MeasureText(kText, hint.label).x + Px(28.0f);
  }
}

std::vector<App::Hint> App::PageHints() const {
  // Launcher 3.18: four hints at most, the page's own buttons; LB and RB are the header's, Donate its button there.
  if (info_.open) {
    return {{HintKey::kB, t_.close}};
  }
  // A dialog's own (it keeps the footer under its veil).
  if (profile_dialog_ == ProfileDialog::kCreate) {
    if (profile_first_) {
      return {{HintKey::kA, t_.profile_create}, {HintKey::kArrows, t_.hint_choose}};
    }
    return {{HintKey::kA, t_.profile_create}, {HintKey::kArrows, t_.hint_choose}, {HintKey::kB, t_.cancel}};
  }
  if (profile_dialog_ == ProfileDialog::kConflict) {
    return {{HintKey::kA, t_.hint_select}};
  }
  if (installing_) {
    Extraction::State extraction = extraction_.Get().state;
    switch (install_step_) {
      case InstallStep::kWelcome:
        return {{HintKey::kA, t_.continue_label}, {HintKey::kB, t_.quit}};
      case InstallStep::kChooseIso:
        // A presses the step's main button: Browse until the file is valid, then Extract (X browses again); after a
        // wrong file, "Choose another file".
        if (iso_ && !iso_check_.valid() && iso_->result == IsoCheck::Result::kValid) {
          return {{HintKey::kA, t_.extract}, {HintKey::kX, t_.browse_file}, {HintKey::kB, t_.back}};
        }
        if (iso_ && !iso_check_.valid()) {
          return {{HintKey::kA, t_.choose_other}, {HintKey::kB, t_.back}};
        }
        return {{HintKey::kA, t_.browse_file}, {HintKey::kB, t_.back}};
      case InstallStep::kExtracting:
        if (extraction == Extraction::State::kRunning) {
          return {{HintKey::kB, t_.cancel_install}};
        }
        if (extraction == Extraction::State::kDone) {
          return {{HintKey::kA, t_.continue_label}};
        }
        return {{HintKey::kB, t_.back}};
      case InstallStep::kDone:
        return {{HintKey::kA, t_.nav_play}, {HintKey::kX, t_.review_settings}};
    }
  }
  switch (page_) {
    case Page::kPlay:
      return {{HintKey::kA, t_.hint_play}, {HintKey::kView, t_.hint_profile}, {HintKey::kB, t_.quit}};
    case Page::kVideo:
    case Page::kGraphics:
    case Page::kGame:
    case Page::kAudio:
      // A row being changed: left and right step it, A (or B) ends it.
      if (AnyRowEditing()) return {{HintKey::kArrows, t_.hint_change}, {HintKey::kA, t_.hint_done}};
      return {{HintKey::kA, t_.hint_edit}, {HintKey::kX, t_.hint_reset}, {HintKey::kY, t_.more_info},
              {HintKey::kB, t_.hint_back}};
    case Page::kProfiles:
      return {{HintKey::kA, t_.hint_use_profile}, {HintKey::kX, t_.edit}, {HintKey::kY, t_.create_profile},
              {HintKey::kB, t_.hint_back}};
    case Page::kProfile:
      if (profile_tab_ == 0) return {{HintKey::kX, t_.edit_profile}, {HintKey::kB, t_.hint_back}};
      return {{HintKey::kA, t_.hint_select}, {HintKey::kX, t_.edit_profile}, {HintKey::kB, t_.hint_back}};
    case Page::kProfileGame:
      return {{HintKey::kA, t_.hint_pin}, {HintKey::kX, t_.hint_filter}, {HintKey::kB, t_.my_games}};
    case Page::kProfileEdit:
      return {{HintKey::kA, edit_id_.empty() ? t_.create_profile : t_.save}, {HintKey::kArrows, t_.hint_choose},
              {HintKey::kB, t_.cancel}};
    case Page::kControls:
      if (AnyRowEditing()) return {{HintKey::kArrows, t_.hint_change}, {HintKey::kA, t_.hint_done}};
      if (game::kHasMicrophones && microphone_selected_) {
        return {{HintKey::kA, t_.hint_change}, {HintKey::kX, t_.hint_clear}, {HintKey::kY, t_.tips},
                {HintKey::kB, t_.hint_back}};
      }
      return {{HintKey::kA, t_.hint_reassign}, {HintKey::kX, t_.hint_clear}, {HintKey::kY, t_.tips},
              {HintKey::kB, t_.hint_back}};
    case Page::kDlc: {
      // The folder mode's page has "More info" (Y); the extracting mode's, its installed rows only.
      constexpr bool folder = game::kDlcMode == game::DlcMode::kFolder;
      switch (dlc_focus_kind_) {
        case 1:
          return {{HintKey::kA, t_.dlc_hint_fold},
                  {HintKey::kX, dlc_focus_all_on_ ? t_.dlc_hint_all_off : t_.dlc_hint_all_on},
                  {HintKey::kY, t_.more_info}, {HintKey::kB, t_.hint_back}};
        case 2:
          return {{HintKey::kA, t_.dlc_hint_toggle}, {HintKey::kY, t_.more_info}, {HintKey::kB, t_.hint_back}};
        case 3:
          if (!folder) {
            return {{HintKey::kA, t_.dlc_hint_toggle}, {HintKey::kX, t_.dlc_remove}, {HintKey::kB, t_.hint_back}};
          }
          return {{HintKey::kA, t_.dlc_hint_toggle}, {HintKey::kX, t_.dlc_remove}, {HintKey::kY, t_.more_info},
                  {HintKey::kB, t_.hint_back}};
        default:
          // The left column: its button, or the link to the DLC folder.
          if (!folder) return {{HintKey::kA, t_.hint_select}, {HintKey::kB, t_.hint_back}};
          return {{HintKey::kA, t_.hint_select}, {HintKey::kY, t_.more_info}, {HintKey::kB, t_.hint_back}};
      }
    }
    case Page::kUpdates: {
      // The main button's own words.
      const Release* newer = updates_ ? updates_->Newer(Version::Parse(kVersion)) : nullptr;
      if (update_url_.empty()) return {{HintKey::kB, t_.hint_back}};
      return {{HintKey::kA, newer ? t_.update_now : updates_ ? t_.check_again : t_.check_updates},
              {HintKey::kB, t_.hint_back}};
    }
    case Page::kAchievements:
      // A reveals a secret one in focus (or hides it again); the strip shows the one in focus without a key.
      if (achievement_focus_secret_) {
        return {{HintKey::kA, achievement_focus_secret_ == 1 ? t_.hint_reveal : t_.hide_again},
                {HintKey::kX, t_.hint_filter}, {HintKey::kB, t_.hint_back}};
      }
      return {{HintKey::kX, t_.hint_filter}, {HintKey::kB, t_.hint_back}};
    default:
      return {{HintKey::kA, t_.hint_select}, {HintKey::kB, t_.hint_back}};
  }
}

void App::OpenInfo(std::string kicker, std::string title, std::string text, std::vector<std::string> bullets) {
  info_ = InfoPanel{};
  info_.open = true;
  info_.kicker = std::move(kicker);
  info_.title = std::move(title);
  info_.text = std::move(text);
  info_.bullets = std::move(bullets);
}

void App::InfoPanelWindow(ImVec2 size) {
  if (!info_.open) {
    return;
  }
  constexpr const char* kPopup = "##info";
  if (!info_.opened) {
    ImGui::OpenPopup(kPopup);
  }
  ImVec2 origin = ImGui::GetMainViewport()->Pos;
  ImGui::SetNextWindowPos(origin);
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 0));
  bool close = false;
  if (ImGui::BeginPopupModal(kPopup, nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoBackground)) {
    const bool first = !info_.opened;
    info_.opened = true;
    float footer_height = Snap(Px(52.0f));
    // The veil: flat, over the header and the page (the footer stays, with the panel's own hints).
    ImRect above(origin, ImVec2(origin.x + size.x, origin.y + size.y - footer_height));
    FillRect(above, Hex(0x000000, 158));  // rgba(0, 0, 0, 0.62)
    // The sheet: 400 dp wide at the right, 12 dp under the header, 12 dp over the footer.
    ImRect sheet(ImVec2(above.Max.x - Px(56.0f) - Px(400.0f), origin.y + Px(84.0f)),
                 ImVec2(above.Max.x - Px(56.0f), above.Max.y - Px(12.0f)));
    FillRect(sheet, col::kCardRaised, 18.0f);
    StrokeRect(sheet, col::kHairline, 18.0f);
    constexpr Type kKicker{Font::kSemiBold, 14};
    constexpr Type kTitle{Font::kDisplayMedium, 28, 0.015f};
    constexpr Type kText{Font::kRegular, 16};
    constexpr Type kBullet{Font::kRegular, 15};
    float left = sheet.Min.x + Px(28.0f);
    float width = sheet.GetWidth() - Px(56.0f);
    // Close: a small outlined button at the top right, the panel's only control.
    {
      ButtonSpec spec{ButtonKind::kGhost, Type{Font::kSemiBold, 14}, 34, 16, 10};
      float w = ButtonWidth(spec, t_.close);
      if (Button("##info_close", ImVec2(sheet.Max.x - Px(23.0f) - w, sheet.Min.y + Px(23.0f)), spec, t_.close)) {
        close = true;
      }
      if (first) RequestFocus(ImGui::GetItemID(), gamepad_used_ || SDL_HasGamepad());
    }
    float y = sheet.Min.y + Px(28.0f);
    DrawLabel(kKicker, ImVec2(left, y), col::kTextMuted, info_.kicker);
    y += LineHeight(kKicker) + Px(6.0f);
    // The text scrolls under the title when it is longer than the sheet.
    y += DrawParagraph(kTitle, ImVec2(left, y), width - Px(70.0f), 1.15f, col::kText, info_.title) + Px(14.0f);
    ImRect body(ImVec2(left, y), ImVec2(left + width + Px(10.0f), sheet.Max.y - Px(24.0f)));
    BeginScroll("##info_body", body);
    ImVec2 at = ImGui::GetCursorScreenPos();
    float by = at.y;
    if (!info_.text.empty()) {
      by += DrawParagraph(kText, ImVec2(at.x, by), width, 1.5f, col::kTextSoft, info_.text) + Px(18.0f);
    }
    for (const std::string& bullet : info_.bullets) {
      float dot = Px(6.0f);
      ImVec2 c(at.x + Px(2.0f) + dot * 0.5f, by + Px(9.0f) + dot * 0.5f);
      ImGui::GetWindowDrawList()->AddCircleFilled(c, dot * 0.5f, col::kTextDim, 12);
      by += DrawParagraph(kBullet, ImVec2(at.x + Px(20.0f), by), width - Px(20.0f), 1.5f, col::kTextMuted, bullet) +
            Px(14.0f);
    }
    ImGui::SetCursorScreenPos(at);
    ImGui::Dummy(ImVec2(width, by - at.y));
    EndScroll();
    // B or Y closes it, as does a click on the veil (not in the frame Y opened it).
    if (!first && (ButtonPressed(PadButton::kB) || ButtonPressed(PadButton::kY) ||
                   (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !sheet.Contains(ImGui::GetMousePos()) &&
                    above.Contains(ImGui::GetMousePos())))) {
      close = true;
    }
    // The footer over the veil: the panel's hints.
    std::vector<Hint> hints = PageHints();
    Footer(ImRect(ImVec2(origin.x, above.Max.y), ImVec2(origin.x + size.x, origin.y + size.y)), hints);
    if (close) {
      info_ = InfoPanel{};
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(3);
}

namespace {

PadButton ButtonOf(char key) {
  switch (key) {
    case 'A':
      return PadButton::kA;
    case 'B':
      return PadButton::kB;
    case 'X':
      return PadButton::kX;
    default:
      return PadButton::kY;
  }
}

}  // namespace

float KeyButtonWidth(const KeyButtonSpec& spec, std::string_view label) {
  return Px(spec.padding) * 2.0f + (spec.key ? GlyphWidth(ButtonOf(spec.key), spec.disc) + Px(spec.gap) : 0.0f) +
         MeasureText(spec.type, label).x;
}

ItemState KeyButton(const char* id, ImVec2 pos, const KeyButtonSpec& spec, std::string_view label, bool enabled) {
  ImRect r(pos, ImVec2(pos.x + KeyButtonWidth(spec, label), pos.y + Px(spec.height)));
  ItemState state = Item(id, r, enabled);
  // Launcher 3.18: the accent only on the main button; the others an outline on nothing, written soft.
  ImU32 text = col::kTextSoft;
  if (!enabled) {
    FillRect(r, col::kChip, spec.radius);
    text = col::kTextDim;
  } else if (spec.look == KeyLook::kPrimary) {
    FillRect(r, state.hovered ? Brighten(col::kAccent) : col::kAccent, spec.radius);
    text = col::kOnAccent;
  } else {
    if (state.hovered) FillRect(r, Fade(col::kFocus, 0.05f), spec.radius);
    StrokeRect(r, spec.look == KeyLook::kDanger ? Fade(col::kError, 0.45f) : col::kOutline, spec.radius);
    if (spec.look == KeyLook::kDanger) text = col::kError;
  }
  float x = r.Min.x + Px(spec.padding);
  if (spec.key && enabled) {
    DrawGlyph(ButtonOf(spec.key), ImVec2(x, r.GetCenter().y - Px(spec.disc) * 0.5f), spec.disc,
              spec.look == KeyLook::kPrimary);
    x += GlyphWidth(ButtonOf(spec.key), spec.disc) + Px(spec.gap);
  } else if (spec.key) {
    x += (GlyphWidth(ButtonOf(spec.key), spec.disc) + Px(spec.gap)) * 0.5f;  // disabled: the label centered
  }
  DrawLabel(spec.type, ImVec2(x, r.Min.y), text, label, r.GetHeight());
  if (state.focused) {
    FocusRing(r, spec.radius);
  }
  return state;
}

}  // namespace launcher
