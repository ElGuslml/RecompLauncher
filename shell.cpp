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

// The tabs, in the header's order (game.h decides whether the game has a DLC page).
constexpr Page kTabs[] = {Page::kPlay, Page::kDlc, Page::kAchievements, Page::kControls, Page::kVideo, Page::kUpdates};

bool HasPage(Page page) { return page != Page::kDlc || game::kHasDlcPage; }

// The tab a page is under: the settings' categories are one tab; the profile's pages none.
Page TabOf(Page page) {
  switch (page) {
    case Page::kGraphics:
    case Page::kGame:
      return Page::kVideo;
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

void App::Header(const ImRect& rect) {
  FillRect(rect, col::kHeader);
  FillRect(ImRect(ImVec2(rect.Min.x, rect.Max.y - Snap(Px(1.0f))), rect.Max), col::kHairline);
  float left = rect.Min.x + Px(28.0f);
  float right = rect.Max.x - Px(28.0f);

  // The brand: the game's logo (its icon, or its mark on accent) and its name on two lines.
  ImRect logo(ImVec2(left, rect.GetCenter().y - Px(18.0f)), ImVec2(left + Px(36.0f), rect.GetCenter().y + Px(18.0f)));
  GameLogo(logo, 20.0f);
  constexpr Type kBrandTitle{Font::kDisplayBold, 17, 0.02f};
  constexpr Type kBrandSubtitle{Font::kBold, 11, 0.16f};
  // A game without a brand line (a name without a kicker): the art's title alone.
  bool brand = *game::kBrandTitle;
  std::string title = Upper(brand ? game::kBrandTitle : game::kArtTitle);
  std::string subtitle = brand ? Upper(game::kBrandSubtitle) : std::string();
  // "line-height: 1.05": the two lines touch.
  float title_box = Px(17.0f * 1.05f);
  float subtitle_box = subtitle.empty() ? 0.0f : Px(11.0f * 1.05f);
  float text_y = rect.GetCenter().y - (title_box + subtitle_box) * 0.5f;
  float text_x = logo.Max.x + Px(12.0f);
  DrawLabel(kBrandTitle, ImVec2(text_x, text_y), col::kText, Ellipsize(kBrandTitle, title, Px(182.0f)), title_box);
  DrawLabel(kBrandSubtitle, ImVec2(text_x, text_y + title_box), col::kLabel,
            Ellipsize(kBrandSubtitle, subtitle, Px(182.0f)), subtitle_box);

  // Right: Donate and the profile's avatar (none on the first run).
  constexpr float kAvatar = 42.0f;
  float avatar_x = right - Px(kAvatar);
  const profiles::Profile* profile = installing_ ? nullptr : store_.Current();
  if (profile) {
    ImRect avatar(ImVec2(avatar_x, rect.GetCenter().y - Px(kAvatar) * 0.5f),
                  ImVec2(right, rect.GetCenter().y + Px(kAvatar) * 0.5f));
    ItemState state = Item("##avatar", avatar);
    header_items_.push_back(ImGui::GetItemID());
    if (TabOf(page_) == Page::kCount) header_focus_ = ImGui::GetItemID();
    DrawAvatar(avatar.Min, kAvatar, profile->avatar, AvatarTexture(*profile), profile->color);
    bool on_profile = TabOf(page_) == Page::kCount;
    if (on_profile) {
      // "outline: 3px solid accent; outline-offset: 3px".
      ImGui::GetWindowDrawList()->AddCircle(avatar.GetCenter(), Px(kAvatar) * 0.5f + Px(4.5f), col::kAccent, 64,
                                            Px(3.0f));
    }
    if (state.focused) {
      ImGui::GetWindowDrawList()->AddCircle(avatar.GetCenter(), Px(kAvatar) * 0.5f + Px(5.5f), col::kFocus, 64,
                                            Px(3.0f));
    }
    if (state.pressed) {
      profiles_selected_ = profile->id;
      Navigate(Page::kProfiles, PressedByNav());
    }
  } else {
    avatar_x = right + Px(14.0f);
  }
  ButtonSpec donate{ButtonKind::kDonate, Type{Font::kBold, 14}, 40, 14, 20, Icon::kHeart, 16, 0, true};
  float donate_width = ButtonWidth(donate, t_.donate);
  if (Button("##donate", ImVec2(avatar_x - Px(14.0f) - donate_width, rect.GetCenter().y - Px(20.0f)), donate,
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

  // The tabs, centered between the brand's block and the right one (230 dp each), between LB and RB.
  const char* labels[] = {t_.nav_play, t_.nav_dlc, t_.nav_achievements, t_.nav_controls, t_.nav_settings, t_.nav_updates};
  constexpr Type kTab{Font::kDisplaySemiBold, 16, 0.04f};
  float tabs_width = GlyphWidth(PadButton::kLB, 24.0f) + GlyphWidth(PadButton::kRB, 24.0f) + Px(6.0f) * 2.0f;
  int shown = 0;
  for (int i = 0; i < int(std::size(kTabs)); ++i) {
    if (HasPage(kTabs[i])) {
      tabs_width += MeasureText(kTab, Upper(labels[i])).x + Px(26.0f);
      ++shown;
    }
  }
  tabs_width += Px(6.0f) * float(shown + 1);
  float x = Snap(std::max(nav_left, (nav_left + nav_right - tabs_width) * 0.5f));
  auto bumper = [&](PadButton button) {
    DrawGlyph(button, ImVec2(x, rect.GetCenter().y - Px(12.0f)), 24.0f);
    x += GlyphWidth(button, 24.0f);
  };
  bumper(PadButton::kLB);
  x += Px(6.0f) + Px(6.0f);
  for (int i = 0; i < int(std::size(kTabs)); ++i) {
    if (!HasPage(kTabs[i])) {
      continue;
    }
    std::string label = Upper(labels[i]);
    float width = MeasureText(kTab, label).x + Px(26.0f);
    ImRect tab(ImVec2(x, rect.Min.y), ImVec2(x + width, rect.Max.y));
    ImGui::PushID(i);
    ItemState state = Item("##tab", tab);
    header_items_.push_back(ImGui::GetItemID());
    ImGui::PopID();
    bool active = OnTab(kTabs[i]);
    if (active) header_focus_ = header_items_.back();
    ImU32 color = active ? col::kTabActive : state.hovered ? Brighten(col::kTabText) : col::kTabText;
    DrawLabel(kTab, ImVec2(tab.Min.x + Px(13.0f), tab.Min.y), color, label, tab.GetHeight());
    if (active) {
      FillRect(ImRect(ImVec2(tab.Min.x, tab.Max.y - Px(3.0f)), tab.Max), col::kAccent);
    }
    // A new version: a dot on Updates.
    if (kTabs[i] == Page::kUpdates && updates_ && updates_->Newer(Version::Parse(kVersion))) {
      float dot = Px(7.0f);
      ImVec2 c(tab.Max.x - Px(6.0f), rect.GetCenter().y - Px(12.0f));
      FillRect(ImRect(ImVec2(c.x - dot * 0.5f, c.y - dot * 0.5f), ImVec2(c.x + dot * 0.5f, c.y + dot * 0.5f)),
               col::kAccent, 4.0f);
    }
    if (state.focused) {
      FocusRing(ImRect(ImVec2(tab.Min.x, tab.Min.y + Px(16.0f)), ImVec2(tab.Max.x, tab.Max.y - Px(16.0f))), 4.0f);
    }
    if (state.pressed) {
      // With A, the focus goes into the page (as with LB/RB); a click leaves it.
      Navigate(kTabs[i], PressedByNav());
    }
    x += width + Px(6.0f);
  }
  x += Px(6.0f) - Px(6.0f);
  bumper(PadButton::kRB);
}

// The first run's four steps (the mockup's install bar): done (a check on accent), current (an accent ring on a tint),
// to come (a grey ring); a line between each two, accent after a step done.
void App::InstallSteps(const ImRect& rect, float left, float right) {
  const char* names[] = {t_.step_welcome, t_.step_iso, t_.step_install, t_.step_done};
  int current = int(install_step_);
  constexpr Type kName{Font::kDisplaySemiBold, 16, 0.04f};
  constexpr Type kNumber{Font::kDisplayBold, 15};
  float width = 0.0f;
  for (int i = 0; i < 4; ++i) {
    width += Px(30.0f) + Px(10.0f) + MeasureText(kName, Upper(names[i])).x;
    if (i < 3) width += Px(10.0f) + Px(4.0f) + Px(36.0f) + Px(4.0f) + Px(10.0f);
  }
  float x = Snap(std::max(left, (left + right - width) * 0.5f));
  float cy = rect.GetCenter().y;
  ImDrawList* draw = ImGui::GetWindowDrawList();
  for (int i = 0; i < 4; ++i) {
    bool done = i < current;
    bool now = i == current;
    ImVec2 center(x + Px(15.0f), cy);
    if (done) {
      draw->AddCircleFilled(center, Px(15.0f), col::kAccent, 32);
      DrawIcon(Icon::kCheck, ImVec2(center.x - Px(7.0f), cy - Px(7.0f)), 14.0f, col::kOnAccent, 3.0f);
    } else {
      if (now) draw->AddCircleFilled(center, Px(15.0f), col::kAccentTint, 32);
      draw->AddCircle(center, Px(14.0f), now ? col::kAccent : Hex(0xFFFFFF, 46), 32, Px(2.0f));
      char number[4];
      std::snprintf(number, sizeof(number), "%d", i + 1);
      DrawLabel(kNumber, ImVec2(center.x - MeasureText(kNumber, number).x * 0.5f, cy - Px(15.0f)),
                now ? col::kAccentLight : col::kTextDim, number, Px(30.0f));
    }
    x += Px(30.0f) + Px(10.0f);
    std::string name = Upper(names[i]);
    DrawLabel(kName, ImVec2(x, rect.Min.y), now ? col::kTabActive : done ? col::kTextSoft : col::kTextDim, name,
              rect.GetHeight());
    x += MeasureText(kName, name).x;
    if (i < 3) {
      x += Px(10.0f) + Px(4.0f);
      FillRect(ImRect(ImVec2(x, cy - Px(1.0f)), ImVec2(x + Px(36.0f), cy + Px(1.0f))),
               done ? col::kAccent : Hex(0xFFFFFF, 36));
      x += Px(36.0f) + Px(4.0f) + Px(10.0f);
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
           1.6f);
  x += Px(20.0f) + Px(8.0f);
  const char* names[] = {t_.device_keyboard, t_.device_guitar, t_.device_drums, t_.device_gamepad};
  const char* name = names[device.kind];
  DrawLabel(kText, ImVec2(x, rect.Min.y), col::kTextSoft, name, rect.GetHeight());
  x += MeasureText(kText, name).x + Px(16.0f);
  char version[64];
  std::snprintf(version, sizeof(version), t_.version, kVersion);
  DrawLabel(kText, ImVec2(x, rect.Min.y), col::kTextDim, version, rect.GetHeight());

  // The hints, right-aligned: each the glyph of its button (the controller in use, or the keyboard) and its label.
  constexpr float kGlyph = 24.0f;
  auto key_width = [&](HintKey key) {
    switch (key) {
      case HintKey::kBumpers:
        return GlyphWidth(PadButton::kLB, kGlyph) + Px(4.0f) + GlyphWidth(PadButton::kRB, kGlyph);
      case HintKey::kArrows:
        return Px(32.0f);
      default:
        return GlyphWidth(PadButton(int(key)), kGlyph);
    }
  };
  float width = 0.0f;
  for (size_t i = 0; i < hints.size(); ++i) {
    width += key_width(hints[i].key) + Px(8.0f) + MeasureText(kText, hints[i].label).x;
    width += i + 1 < hints.size() ? Px(20.0f) : 0.0f;
  }
  x = rect.Max.x - Px(28.0f) - width;
  for (const Hint& hint : hints) {
    float w = key_width(hint.key);
    ImVec2 at(x, rect.GetCenter().y - Px(kGlyph) * 0.5f);
    if (hint.key == HintKey::kBumpers) {
      DrawGlyph(PadButton::kLB, at, kGlyph);
      DrawGlyph(PadButton::kRB, ImVec2(at.x + GlyphWidth(PadButton::kLB, kGlyph) + Px(4.0f), at.y), kGlyph);
    } else if (hint.key == HintKey::kArrows) {
      // "◀ ▶": two small triangles (the D-pad's, or the arrow keys).
      ImRect chip(at, ImVec2(at.x + w, at.y + Px(kGlyph)));
      FillRect(chip, col::kHintBumper, kGlyph * 0.5f);
      ImDrawList* draw = ImGui::GetWindowDrawList();
      ImVec2 c = chip.GetCenter();
      float s = Px(4.0f);
      draw->AddTriangleFilled(ImVec2(c.x - Px(7.0f), c.y), ImVec2(c.x - Px(1.5f), c.y - s), ImVec2(c.x - Px(1.5f), c.y + s),
                              col::kOnHintBumper);
      draw->AddTriangleFilled(ImVec2(c.x + Px(7.0f), c.y), ImVec2(c.x + Px(1.5f), c.y - s), ImVec2(c.x + Px(1.5f), c.y + s),
                              col::kOnHintBumper);
    } else {
      DrawGlyph(PadButton(int(hint.key)), at, kGlyph);
    }
    x += w + Px(8.0f);
    DrawLabel(kText, ImVec2(x, rect.Min.y), col::kTextSoft, hint.label, rect.GetHeight());
    x += MeasureText(kText, hint.label).x + Px(20.0f);
  }
}

std::vector<App::Hint> App::PageHints() const {
  if (installing_) {
    Extraction::State extraction = extraction_.Get().state;
    switch (install_step_) {
      case InstallStep::kWelcome:
        return {{HintKey::kA, t_.continue_label}, {HintKey::kY, t_.donate}, {HintKey::kB, t_.quit}};
      case InstallStep::kChooseIso:
        return {{HintKey::kA, t_.continue_label}, {HintKey::kX, t_.browse_file}, {HintKey::kY, t_.donate},
                {HintKey::kB, t_.back}};
      case InstallStep::kExtracting:
        if (extraction == Extraction::State::kRunning) {
          return {{HintKey::kY, t_.donate}, {HintKey::kB, t_.cancel_install}};
        }
        if (extraction == Extraction::State::kDone) {
          return {{HintKey::kA, t_.continue_label}, {HintKey::kY, t_.donate}};
        }
        return {{HintKey::kY, t_.donate}, {HintKey::kB, t_.back}};
      case InstallStep::kDone:
        return {{HintKey::kA, t_.play}, {HintKey::kX, t_.review_settings}, {HintKey::kY, t_.donate}};
    }
  }
  switch (page_) {
    case Page::kPlay:
      return {{HintKey::kA, t_.hint_play}, {HintKey::kY, t_.donate}, {HintKey::kView, t_.hint_profile},
              {HintKey::kBumpers, t_.hint_tabs}, {HintKey::kB, t_.quit}};
    case Page::kVideo:
    case Page::kGraphics:
    case Page::kGame:
      if (settings_editing_) {
        return {{HintKey::kArrows, t_.hint_change}, {HintKey::kA, t_.hint_done}, {HintKey::kY, t_.hint_reset}};
      }
      return {{HintKey::kA, t_.hint_edit}, {HintKey::kY, t_.hint_reset}, {HintKey::kBumpers, t_.hint_tabs},
              {HintKey::kB, t_.hint_back}};
    case Page::kProfiles:
      return {{HintKey::kA, t_.hint_use_profile}, {HintKey::kY, t_.edit}, {HintKey::kX, t_.create_profile},
              {HintKey::kB, t_.hint_back}};
    case Page::kProfile:
      return {{HintKey::kA, t_.hint_select}, {HintKey::kY, t_.edit_profile}, {HintKey::kBumpers, t_.hint_sections},
              {HintKey::kB, t_.hint_back}};
    case Page::kProfileGame:
      return {{HintKey::kA, t_.hint_pin}, {HintKey::kX, t_.hint_filter}, {HintKey::kB, t_.my_games}};
    case Page::kProfileEdit:
      return {{HintKey::kA, t_.save}, {HintKey::kArrows, t_.hint_choose}, {HintKey::kB, t_.cancel}};
    case Page::kControls:
      return {{HintKey::kA, t_.hint_reassign}, {HintKey::kX, t_.hint_clear}, {HintKey::kY, t_.hint_reset},
              {HintKey::kBumpers, t_.hint_tabs}, {HintKey::kB, t_.hint_back}};
    case Page::kDlc:
      return {{HintKey::kA, t_.hint_choose_folder}, {HintKey::kBumpers, t_.hint_tabs}, {HintKey::kB, t_.hint_back}};
    case Page::kUpdates:
      return {{HintKey::kA, t_.check_updates}, {HintKey::kBumpers, t_.hint_tabs}, {HintKey::kB, t_.hint_back}};
    case Page::kAchievements:
      return {{HintKey::kA, t_.hint_view_detail}, {HintKey::kY, t_.hint_reveal}, {HintKey::kX, t_.hint_filter},
              {HintKey::kBumpers, t_.hint_tabs}, {HintKey::kB, t_.hint_back}};
    default:
      return {{HintKey::kA, t_.hint_select}, {HintKey::kBumpers, t_.hint_tabs}, {HintKey::kB, t_.hint_back}};
  }
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
  ImU32 text = col::kText;
  if (!enabled) {
    FillRect(r, Hex(0xFFFFFF, 20), spec.radius);
    text = col::kTextDim;
  } else if (spec.look == KeyLook::kPrimary) {
    FillRect(r, state.hovered ? Brighten(col::kAccent) : col::kAccent, spec.radius);
    text = col::kOnAccent;
  } else {
    FillRect(r, state.hovered ? Brighten(col::kGhostFill) : col::kGhostFill, spec.radius);
    StrokeRect(r, spec.look == KeyLook::kDanger ? Fade(col::kError, 0.45f) : col::kGhostBorder, spec.radius);
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
