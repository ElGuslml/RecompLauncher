// The launcher's window contents (app.h): the frame (the shell is shell.cpp's), the pages' shared pieces and the
// controller's shortcuts.

#include "app.h"

#include <SDL3/SDL_messagebox.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iterator>

#include "game.h"
#include "launcher_version.h"
#include "ui/gfx.h"
#include "version.h"

namespace launcher {

using namespace ui;

const char* const kVersion = game::kVersion;
const char* const kLauncherVersion = RECOMP_LAUNCHER_VERSION_STR;

namespace {

// The value of a launcher option on the command line (--launcher_update_url=...), or empty.
std::string CommandLineValue(const std::wstring& name) {
  std::wstring line = GetCommandLineW();
  size_t at = line.find(name);
  if (at == std::wstring::npos) return {};
  at += name.size();
  bool quoted = at < line.size() && line[at] == L'"';
  if (quoted) ++at;
  size_t end = line.find(quoted ? L'"' : L' ', at);
  std::wstring value = line.substr(at, end == std::wstring::npos ? std::wstring::npos : end - at);
  int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), nullptr, 0, nullptr, nullptr);
  std::string out(size_t(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), int(value.size()), out.data(), length, nullptr, nullptr);
  return out;
}

}  // namespace

void OpenFolder(const std::filesystem::path& folder) {
  ShellExecuteW(nullptr, L"open", folder.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenUrl(const char* url) {
  // SDL_OpenURL is not in the SDK's SDL build (SDL_MISC off).
  int length = MultiByteToWideChar(CP_UTF8, 0, url, -1, nullptr, 0);
  std::wstring wide(size_t(std::max(length, 1)), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, url, -1, wide.data(), length);
  ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

App::App(Settings settings, uint32_t language)
    : settings_(std::move(settings)),
      t_(TextsFor(language)),
      language_(language),
      exe_dir_(settings_.options_path.parent_path()) {
  // The first run: the game's files are not there yet, or --launcher_first_run asks for it (to install again, or
  // to see it in a development build, whose game falls back to the repository's files).
  installing_ = !GameInstalled(exe_dir_) || std::wstring(GetCommandLineW()).find(L"--launcher_first_run") != std::wstring::npos;
  if (!installing_) {
    // The profiles first: the achievements' unlocks may be the profile's (page_profile.cpp).
    LoadProfiles();
    profiles_loaded_ = true;
    LoadAchievements();
  }
  // Updates: what an update left is cleaned up, and a new version is looked for if allowed.
  update_url_ = CommandLineValue(L"--launcher_update_url=");
  if (update_url_.empty()) {
    update_url_ = game::kReleasesApi;
  }
  updated_from_ = CommandLineValue(L"--launcher_updated_from=");
  CleanUpAfterUpdate(exe_dir_);
  if (!installing_ && settings_.o.check_updates && !update_url_.empty()) {
    CheckForUpdatesNow();
  }
  // PLAY has the focus from the first frame (ImGui::SetItemDefaultFocus in PlayPage does not show it).
  focus_page_ = true;
}

bool App::Busy() const {
  Update::State update = update_.Get().state;
  bool updating = update == Update::State::kDownloading || update == Update::State::kVerifying ||
                  update == Update::State::kInstalling;
  return listen_bind_ >= 0 || pad_listen_ >= 0 || FocusRequested() || focus_page_ || controls_focus_mapping_ ||
         std::chrono::steady_clock::now() < notice_until_ || achievements_loading_.valid() ||
         profile_games_loading_.valid() ||
         game_running_ || update_check_.valid() || updating || dlc_scan_.valid() || dlc_listing_.valid() ||
         dlc_install_.Get().state == DlcInstall::State::kRunning ||
         (installing_ && (iso_check_.valid() || extraction_.Get().state == Extraction::State::kRunning));
}

void App::LoadAchievements() {
  achievements_requested_ = true;
  std::filesystem::path exe_dir = exe_dir_;
  uint32_t language = language_;
  bool ask_icon = !game_icon_asked_ && game_icon_ == ImTextureID_Invalid;
  game_icon_asked_ = true;
  achievements_loading_ = std::async(std::launch::async, [exe_dir, language, ask_icon] {
    AchievementsRead read;
    read.list = launcher::LoadAchievements(exe_dir, language);
    read.icon = FindGameIcon(exe_dir);
    if ((!read.list || (!read.icon && ask_icon)) && WriteAchievementList(exe_dir)) {
      read.list = launcher::LoadAchievements(exe_dir, language);
      read.icon = FindGameIcon(exe_dir);
    }
    return read;
  });
}

void App::PollAchievements() {
  if (!achievements_loading_.valid() ||
      achievements_loading_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    return;
  }
  AchievementsRead read = achievements_loading_.get();
  if (read.icon) {
    SetGameIcon(std::move(*read.icon));
  }
  if (std::optional<Achievements>& list = read.list) {
    ReleaseAchievementIcons();
    achievements_ = std::move(list);
    achievement_icons_.assign(achievements_->items.size(), ImTextureID_Invalid);
    // The list for the other launchers, maybe just written by the game.
    if (store_.Current() && profiles::CopyAchievementList(store_, game::kTitleId, AchievementsFolder(exe_dir_)) &&
        profile_games_.empty()) {
      LoadProfileGames();
    }
  }
}

ImTextureID App::AchievementIcon(size_t index) {
  if (!achievements_ || index >= achievement_icons_.size()) {
    return ImTextureID_Invalid;
  }
  Achievement& a = achievements_->items[index];
  if (achievement_icons_[index] == ImTextureID_Invalid && !a.icon.empty()) {
    achievement_icons_[index] = ui::gfx::CreateTexture(a.icon.data(), a.icon_width, a.icon_height);
    a.icon.clear();  // on the GPU now
    a.icon.shrink_to_fit();
  }
  return achievement_icons_[index];
}

void App::ReleaseAchievementIcons() {
  for (ImTextureID& texture : achievement_icons_) {
    ui::gfx::DestroyTexture(texture);
    texture = ImTextureID_Invalid;
  }
}

void App::ReleaseTextures() {
  ReleaseAchievementIcons();
  ui::gfx::DestroyTexture(game_icon_);
  game_icon_ = ImTextureID_Invalid;
  for (ProfileGame& game : profile_games_) {
    ui::gfx::DestroyTexture(game.picture);
    game.picture = ImTextureID_Invalid;
  }
}

void App::SetGameIcon(GameIcon icon) {
  if (game_icon_ == ImTextureID_Invalid) {
    game_icon_ = ui::gfx::CreateTexture(icon.picture.pixels.data(), icon.picture.width, icon.picture.height);
  }
  game_icon_png_ = std::move(icon.png);
  // For the other launchers (once there is a store: RegisterThisGame writes it when the first profile makes one).
  if (store_.found && !store_.read_only) {
    profiles::SaveGameIcon(store_, game::kTitleId, game_icon_png_);
  }
}

void App::GameLogo(const ImRect& rect, float mark_size) {
  float rounding = game::kBrandRound ? 20.0f : 8.0f;
  if (game_icon_ != ImTextureID_Invalid) {
    DrawRoundedTexture(game_icon_, rect, rounding);
    StrokeRect(rect, Fade(col::kText, 0.08f), rounding);
    return;
  }
  FillRect(rect, col::kAccent, rounding);
  DrawIcon(Icon::kBrand, ImVec2(rect.GetCenter().x - Px(mark_size) * 0.5f, rect.GetCenter().y - Px(mark_size) * 0.5f),
           mark_size, col::kOnAccent, 2.0f);
}

void App::Changed() {
  static bool reported = false;
  if (!SaveOptions(settings_) && !reported) {
    reported = true;
    char text[512];
    std::snprintf(text, sizeof(text), t_.save_failed, game::kOptionsFile);
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, game::kLauncherName, text, nullptr);
  }
}

void App::CheckForUpdatesNow() {
  if (update_check_.valid() || update_url_.empty()) return;
  std::string url = update_url_;
  update_check_ = std::async(std::launch::async, [url] { return CheckForUpdates(url); });
}

void App::PollUpdates() {
  if (update_check_.valid() && update_check_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    updates_ = update_check_.get();
    if (updates_->ok) {
      settings_.o.last_update_check = int64_t(std::time(nullptr));
      Changed();
    }
  }
  // Installed: main.cpp starts the new launcher.
  if (update_.Get().state == Update::State::kDone && result_ == Result::kNone) {
    result_ = Result::kRestart;
  }
}

std::string App::UpdatedFromArgument() const {
  return std::string("--launcher_updated_from=") + kVersion;
}

void App::GameStarted() {
  result_ = Result::kNone;
  game_running_ = true;
  game_exit_code_ = 0;
  profile_notice_.clear();
  // The profile the game runs with was used now.
  if (profiles::Profile* profile = store_.Find(store_.current); profile && !store_.read_only) {
    profile->last_used = std::chrono::duration_cast<std::chrono::seconds>(
                             std::chrono::system_clock::now().time_since_epoch()).count();
    profiles::SaveProfile(store_, *profile);
  }
  BeginSession();
}

void App::GameExited(uint32_t exit_code) {
  result_ = Result::kNone;
  game_running_ = false;
  game_exit_code_ = exit_code;
  CountSession(true);
  Navigate(Page::kPlay, false);
  focus_page_ = true;
  // The game may have unlocked some, or written the list for the first time; the profile pages show the new time.
  LoadAchievements();
  LoadProfileGames();
}

void App::StartingOverlay(ImVec2 size) {
  // A window over the page, which takes no input (main.cpp does not pass it any meanwhile).
  ImVec2 origin = ImGui::GetMainViewport()->Pos;
  ImGui::SetNextWindowPos(origin);
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::Begin("##starting", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);
  ImGui::PopStyleVar(2);
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), Fade(col::kBackground, 0.88f));
  constexpr Type kText{Font::kSemiBold, 18};
  ImVec2 center(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f);
  // A spinning arc above the text.
  float radius = Px(18.0f);
  ImVec2 spinner(center.x, center.y - Px(28.0f));
  float start = float(ImGui::GetTime()) * 5.0f;
  draw->PathArcTo(spinner, radius, 0.0f, 6.2831853f, 48);
  draw->PathStroke(col::kTrack, ImDrawFlags_None, Px(3.0f));
  draw->PathArcTo(spinner, radius, start, start + 1.9f, 24);
  draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(3.0f));
  ImVec2 text_size = MeasureText(kText, t_.starting_game);
  ImVec2 text_pos(Snap(center.x - text_size.x * 0.5f), Snap(spinner.y + radius + Px(22.0f)));
  DrawLabel(kText, text_pos, col::kText, t_.starting_game);
  ImGui::End();
}

void App::Navigate(Page page, bool focus) {
  StopListening();
  settings_editing_ = false;
  page_ = page;
  if (focus) {
    focus_page_ = true;
  }
}

void App::UseGlyphsOf(SDL_GamepadType type) {
  glyphs_chosen_ = true;
  switch (type) {
    case SDL_GAMEPAD_TYPE_PS3:
      glyph_style_ = ui::GlyphStyle::kPlayStation;
      view_label_ = "Select";
      break;
    case SDL_GAMEPAD_TYPE_PS4:
      glyph_style_ = ui::GlyphStyle::kPlayStation;
      view_label_ = "Share";
      break;
    case SDL_GAMEPAD_TYPE_PS5:
      glyph_style_ = ui::GlyphStyle::kPlayStation;
      view_label_ = "Create";
      break;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
      glyph_style_ = ui::GlyphStyle::kNintendo;
      view_label_ = "\xE2\x88\x92";  // the Switch's minus
      break;
    default:
      glyph_style_ = ui::GlyphStyle::kXbox;
      view_label_ = type == SDL_GAMEPAD_TYPE_XBOXONE ? "View" : "Back";
      break;
  }
}

void App::Frame(ImVec2 size) {
  // ImGui's navigation must not take X (Square) held as its window switcher: the launcher's X is its own.
  GImGui->ConfigNavWindowingWithGamepad = false;
  if (!glyphs_chosen_) {
    int count = 0;
    SDL_JoystickID* pads = SDL_GetGamepads(&count);
    if (pads && count > 0) {
      UseGlyphsOf(SDL_GetGamepadTypeForID(pads[0]));
      glyphs_chosen_ = false;  // a connected controller's until an input says which
    } else {
      glyph_style_ = ui::GlyphStyle::kKeyboard;
    }
    SDL_free(pads);
  }
  ui::SetGlyphStyle(glyph_style_, view_label_);
  PollAchievements();
  PollProfileGames();
  PollUpdates();
  PollDlc();
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->Pos);
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::Begin("##launcher", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                   ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoScrollWithMouse);
  ImGui::PopStyleVar(3);

  if (installing_) {
    InstallScreen(size);
    ImGui::End();
    return;
  }
  // After the first run: the profiles (a first profile to make, this installation's saves).
  if (!profiles_loaded_) {
    profiles_loaded_ = true;
    LoadProfiles();
  }

  ImVec2 origin = viewport->Pos;
  // The art under the page, darkened under every page but Play's (the mockup's "oscurecer": ninguno, alto).
  Background(size, page_ == Page::kPlay ? 0.0f : 0.9f);
  float header_height = Snap(Px(72.0f));
  float footer_height = Snap(Px(52.0f));
  ImRect main(ImVec2(origin.x, origin.y + header_height), ImVec2(origin.x + size.x, origin.y + size.y - footer_height));
  page_focus_ = 0;
  back_focus_ = 0;
  CaptureFirstItem();
  const Page drawn = page_;
  switch (page_) {
    case Page::kPlay:
      PlayPage(main);
      break;
    case Page::kDlc:
      DlcPage(main);
      break;
    case Page::kAchievements:
      AchievementsPage(main);
      break;
    case Page::kControls:
      ControlsPage(main);
      break;
    case Page::kVideo:
    case Page::kGraphics:
    case Page::kGame:
      SettingsPage(main);
      break;
    case Page::kUpdates:
      UpdatesPage(main);
      break;
    case Page::kProfiles:
      ProfilesPage(main);
      break;
    case Page::kProfile:
      ProfilePage(main);
      break;
    case Page::kProfileGame:
      ProfileGamePage(main);
      break;
    case Page::kProfileEdit:
      ProfileEditPage(main);
      break;
    case Page::kCount:
      break;
  }
  // The control a controller enters the page on: the page's choice (PLAY), or its first.
  ImGuiID first = CapturedFirstItem();
  if (!page_focus_) {
    page_focus_ = first;
  }

  Chrome(size);

  ImGuiContext& g = *GImGui;
  // Up from the page's top (nothing above in the page) reaches the header, at its current tab; down from the header
  // with nothing found goes back to the page's control. ImGui does not score the header's controls from the page's,
  // nor the page's from the header's (sibling windows).
  if (g.NavMoveScoringItems && g.NavMoveResultLocal.ID == 0 && g.NavMoveResultLocalVisible.ID == 0 &&
      g.NavMoveResultOther.ID == 0) {
    bool in_header = std::find(header_items_.begin(), header_items_.end(), g.NavId) != header_items_.end();
    if (g.NavMoveDir == ImGuiDir_Up && !in_header && header_focus_) {
      RequestFocus(header_focus_);
      ImGui::NavMoveRequestCancel();
    } else if (g.NavMoveDir == ImGuiDir_Down && in_header && page_focus_) {
      RequestFocus(page_focus_);
      ImGui::NavMoveRequestCancel();
    }
  }
  // The controller's shortcuts: LB/RB change the tab, Start plays, B goes back to Play (on Play it quits), Y on Play
  // opens Donate. Not while a list is open (B closes it) or a key is being assigned.
  static bool popup_was_open = false;
  bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
  // The controller's buttons are ImGui's state, not events: one just assigned on the Gamepad page still looks
  // pressed. It does nothing until all are up.
  if (pad_release_wait_ && !AnyGamepadButtonDown()) {
    pad_release_wait_ = false;
  }
  if (!popup && !popup_was_open && !Listening() && !pad_release_wait_) {
    if (page_ == Page::kProfile && (ButtonPressed(PadButton::kLB) || ButtonPressed(PadButton::kRB))) {
      profile_tab_ = 1 - profile_tab_;  // the profile's sections
    } else if (ButtonPressed(PadButton::kLB)) {
      NextTab(-1);
    } else if (ButtonPressed(PadButton::kRB)) {
      NextTab(1);
    } else if (ImGui::IsKeyPressed(ImGuiKey_GamepadStart, false)) {
      result_ = Result::kPlay;
    } else if (ButtonPressed(PadButton::kView) && store_.Current()) {
      // View (Back, Select): the profiles, as the header's avatar.
      profiles_selected_ = store_.current;
      Navigate(Page::kProfiles, true);
    } else if (page_ == Page::kPlay && !g.NavCursorVisible &&
               (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false))) {
      // PLAY is the page's default button: Enter plays while no control shows the focus (ImGui would not activate
      // it then).
      result_ = Result::kPlay;
    } else if (page_ == Page::kPlay && ButtonPressed(PadButton::kY)) {
      Donate();
    } else if (ButtonPressed(PadButton::kB)) {
      if (settings_editing_) {
        settings_editing_ = false;  // B ends a Settings row's change
      } else if (back_focus_ && g.NavId != back_focus_) {
        // From a page's list back to its first column (the Settings tab's categories).
        RequestFocus(back_focus_);
        ImGui::SetNavCursorVisible(true);
      } else if (page_ == Page::kPlay) {
        result_ = Result::kQuit;
      } else {
        // A game's achievements go back to the games.
        if (page_ == Page::kProfileGame) profile_tab_ = 1;
        Navigate(BackPage(), true);
      }
    }
  }
  popup_was_open = popup;

  // When the launcher opens and back from the game: PLAY has the focus, shown when a controller is connected, so
  // that A (or Enter) plays at once (ImGui activates only an item whose focus is shown).
  // The page drawn this frame's: after a tab changed in it, the next frame's page gives it.
  if (focus_page_ && page_focus_ && drawn == page_) {
    // Shown when a controller is connected or was used (the page opened with the mouse shows none).
    RequestFocus(page_focus_, gamepad_used_ || SDL_HasGamepad());
    ImGui::SetNavCursorVisible(gamepad_used_ || SDL_HasGamepad());
    focus_page_ = false;
  }

  ImGui::End();
  ProfileDialogs(size);
  // Not while a profile dialog waits (its first frame, before its popup takes the input).
  if (profile_dialog_ != ProfileDialog::kNone && result_ == Result::kPlay) {
    result_ = Result::kNone;
  }
  if (game_running_) {
    StartingOverlay(size);
  }
}

bool App::OnEvent(const SDL_Event& event) {
  switch (event.type) {
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
          std::abs(int(event.gaxis.value)) > 16000) {
        gamepad_used_ = true;
        // The glyphs of the controller pressed.
        UseGlyphsOf(SDL_GetGamepadTypeForID(event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ? event.gbutton.which
                                                                                          : event.gaxis.which));
      }
      break;
    case SDL_EVENT_MOUSE_MOTION:
      gamepad_used_ = false;
      break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_WHEEL:
    case SDL_EVENT_KEY_DOWN:
      gamepad_used_ = false;
      // The keyboard's keys (not on mouse motion alone: a nudged mouse does not change them).
      glyph_style_ = ui::GlyphStyle::kKeyboard;
      glyphs_chosen_ = true;
      break;
    case SDL_EVENT_DROP_FILE:
      // An ISO dropped on the window, on the first run's ISO step.
      if (installing_ && install_step_ == InstallStep::kChooseIso && event.drop.data) {
        ChooseIso(event.drop.data);
      }
      // A folder (or a package) dropped on the DLC page: searched for packages.
      if (!installing_ && page_ == Page::kDlc && event.drop.data) {
        ScanDlc(event.drop.data);
      }
      return true;
    default:
      break;
  }
  return OnKeyCapture(event) || OnPadCapture(event);
}

}  // namespace launcher
