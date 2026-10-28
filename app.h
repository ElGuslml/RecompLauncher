// The launcher's window contents: the sidebar and the pages of Gustavo's mockup (made for POTCAWERecomp),
// on the settings of settings.h. main.cpp owns the window and the loop.

#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_video.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "achievements.h"
#include "dlc.h"
#include "install.h"
#include "picture.h"
#include "profile_store.h"
#include "updater.h"
#include "settings.h"
#include "texts.h"
#include "ui/ui.h"

namespace launcher {

// The launcher's version (res/version.h), shown in the sidebar and the Updates page.
extern const char* const kVersion;
// The common launcher code's version (launcher_version.h), the same in every game: the sidebar, the Updates page.
extern const char* const kLauncherVersion;

// Opens a web page in the default browser.
void OpenUrl(const char* url);
// Opens a folder in Explorer.
void OpenFolder(const std::filesystem::path& folder);


// kDlc only when the game takes downloadable content (game::kHasDlcPage); kGame only when the game has options of its
// own (game::kHasGamePage).
// kProfile .. kProfileEdit are not in the sidebar's list: the profile's card opens them.
enum class Page : int {
  kPlay, kDlc, kAchievements, kKeyboard, kGamepad, kVideo, kGraphics, kGame, kUpdates,
  kProfile, kProfileGame, kProfileEdit, kCount
};

class App {
 public:
  // kRestart: an update installed the launcher's new version, which main.cpp starts in place of this one.
  enum class Result { kNone, kPlay, kQuit, kRestart };

  App(Settings settings, uint32_t language);

  // The whole window, between ImGui::NewFrame() and ImGui::Render(). `size` in pixels.
  void Frame(ImVec2 size);
  // An SDL event before ImGui sees it; true when the launcher used it (a key being assigned).
  bool OnEvent(const SDL_Event& event);

  Result result() const { return result_; }
  void Quit() { result_ = Result::kQuit; }
  // Something is changing on screen (a key being assigned, a message): keep drawing.
  bool Busy() const;
  // A key is being assigned: the controller does not navigate meanwhile (B cancels).
  bool Listening() const { return listen_bind_ >= 0 || pad_listen_ >= 0; }
  // A controller button was just assigned: the controller does nothing until its buttons are up.
  bool GamepadLocked() const { return pad_release_wait_; }
  Settings& settings() { return settings_; }
  // The window, for the file dialog.
  void SetWindow(SDL_Window* window) { window_ = window; }
  const Texts& texts() const { return t_; }
  // Before the graphics device goes: the achievement icons' and the games' icons' textures.
  void ReleaseTextures();

  // Play started the game (main.cpp): until its window shows, the launcher draws "Starting the game…" over its
  // page and takes no input.
  void GameStarted();
  bool GameRunning() const { return game_running_; }
  // The game exited (the launcher shows again): back on the Play page with PLAY focused, the achievements read
  // again, and a notice when the exit code is not 0.
  void GameExited(uint32_t exit_code);
  // While the game runs (main.cpp, at least once a minute): its playtime so far written to the profile.
  void GameTick();

  // The arguments of the launcher's next start after an update (main.cpp): it names the version it came from.
  std::string UpdatedFromArgument() const;

 private:
  // A setting changed: the options file (game::kOptionsFile) is written at once.
  void Changed();

  void Sidebar(const ImRect& rect);
  void ControllerHints(ImVec2 pos, float width);

  // Pages, each in its own file (page_*.cpp). `rect` is the area right of the sidebar.
  void PlayPage(const ImRect& rect);
  void DlcPage(const ImRect& rect);
  void AchievementsPage(const ImRect& rect);
  void KeyboardPage(const ImRect& rect);
  void GamepadPage(const ImRect& rect);
  void VideoPage(const ImRect& rect);
  void GraphicsPage(const ImRect& rect);
  void GamePage(const ImRect& rect);
  void UpdatesPage(const ImRect& rect);
  // The release notes (Markdown) at `pos`, `width` wide. Returns the height; draws only when `draw`.
  float ReleaseNotes(const std::string& markdown, ImVec2 pos, float width, bool draw);
  // A notice across the top of the Play page's art (an error exit code, an update installed).
  // `link_page` kCount: the link opens the logs folder; a null `link`: none.
  void Notice(const ImRect& rect, bool error, const char* text, const char* link, Page link_page);
  // When updates were last checked ("22/10/2026 18:04"), or "never".
  std::string LastCheckedText() const;

  // The pages' shared pieces. Each returns the y below it (pixels).
  // `badge`: a badge right of the title (the keyboard's "experimental"), or null.
  float Header(ImVec2 pos, const char* kicker, const char* title, const char* subtitle, const char* badge = nullptr);
  // A settings row in a card: the title and hint on the left, the control at `*control` (its height in
  // dp is `control_height`). Returns the row's height.
  float Row(ImVec2 pos, float width, float padding_y, const char* title, const char* hint, float control_height,
            bool last, ImVec2* control);
  // "Changes are saved automatically" and a reset button at the bottom of `rect`; true when reset.
  bool Footer(const ImRect& rect, const char* reset_label);

  void Navigate(Page page, bool focus_sidebar);
  void StartingOverlay(ImVec2 size);
  void FocusSidebar() { focus_sidebar_ = true; }

  // The keyboard page's key being assigned (page_keyboard.cpp).
  bool OnKeyCapture(const SDL_Event& event);
  // The Gamepad page's control being assigned a button (page_gamepad.cpp).
  bool OnPadCapture(const SDL_Event& event);
  void AssignPad(int input);
  void StartListening(int bind, int slot);
  void StopListening();
  void Assign(std::string token);

  Settings settings_;
  const Texts& t_;
  Page page_ = Page::kPlay;
  Result result_ = Result::kNone;
  bool focus_sidebar_ = false;
  bool gamepad_used_ = false;  // the last input came from a controller: the hints show
  ImGuiID page_focus_ = 0;     // the control the controller enters the page on (App::Frame)
  bool focus_page_ = false;    // focus page_focus_ on the next frame (back from the game)
  bool game_running_ = false;
  uint32_t game_exit_code_ = 0;  // the last game's, when not 0: a notice on the Play page

  // Keyboard page.
  int listen_bind_ = -1;  // the Bind whose key is being assigned, or -1
  int listen_slot_ = 0;   // 0 = primary, 1 = secondary
  uint8_t held_modifiers_ = 0;
  bool modifier_pressed_ = false;
  std::string notice_;  // a message under the keyboard page's title, for a moment
  std::chrono::steady_clock::time_point notice_until_{};

  // Gamepad page.
  int selected_pad_ = 0;
  SDL_GamepadType pad_type_ = SDL_GAMEPAD_TYPE_UNKNOWN;  // the chosen controller's type: its layout is edited
  int pad_listen_ = -1;  // the control waiting for a button, or -1
  bool pad_release_wait_ = false;

  // Achievements page.
  int achievement_filter_ = 0;

  // The achievements (achievements.h): read on a thread when the launcher opens, and once the first run has
  // extracted the game; the game is asked to write their list when it is missing. The game's icon with them.
  struct AchievementsRead {
    std::optional<Achievements> list;
    std::optional<GameIcon> icon;
  };
  void LoadAchievements();
  void PollAchievements();
  void ReleaseAchievementIcons();
  // The icon of achievements_->items[index], made into a texture the first time it is drawn.
  ImTextureID AchievementIcon(size_t index);
  void AchievementsCard(const ImRect& rect, float link_y);
  uint32_t language_;
  std::future<AchievementsRead> achievements_loading_;
  std::optional<Achievements> achievements_;
  std::vector<ImTextureID> achievement_icons_;
  bool achievements_requested_ = false;

  // The game's icon (achievements.h's FindGameIcon): the sidebar's and the first run's logo, and the game's picture
  // in the profile pages; until it is found, the game's mark on accent. A list the game wrote before it wrote its
  // icon (an SDK before change 52) has the game asked once to write it again.
  void SetGameIcon(GameIcon icon);
  // The logo in `rect`: the icon, or the mark (`mark_size` dp) on accent.
  void GameLogo(const ImRect& rect, float mark_size);
  ImTextureID game_icon_ = ImTextureID_Invalid;
  std::vector<uint8_t> game_icon_png_;  // for the profiles' store
  bool game_icon_asked_ = false;

  // Updates (updater.h): checked on a thread when the launcher opens (if allowed) and on request.
  void CheckForUpdatesNow();
  void PollUpdates();
  std::string update_url_;     // game::kReleasesApi, or --launcher_update_url for a test; empty: no updates
  std::string updated_from_;   // --launcher_updated_from: this start follows an update from that version
  std::future<UpdateCheck> update_check_;
  std::optional<UpdateCheck> updates_;
  Update update_;

  // The first run (page_install.cpp): the game's files extracted from the player's ISO.
  enum class InstallStep { kWelcome, kChooseIso, kExtracting, kDone };
  struct DialogResult {
    std::mutex mutex;
    std::optional<std::string> path;  // UTF-8
  };
  void InstallScreen(ImVec2 size);
  void SetStep(InstallStep step) {
    install_step_ = step;
    step_changed_ = true;
  }
  void InstallArt(const ImRect& rect);
  void Stepper(ImVec2 pos);
  void ChooseIso(const std::string& path);
  void BrowseIso();
  void PollInstall();
  std::string Gigabytes(uint64_t bytes) const;
  std::filesystem::path exe_dir_;
  bool installing_ = false;
  InstallStep install_step_ = InstallStep::kWelcome;
  bool step_changed_ = true;
  std::string iso_pending_;  // the file being checked, UTF-8
  std::future<IsoCheck> iso_check_;
  std::optional<IsoCheck> iso_;
  std::shared_ptr<DialogResult> dialog_ = std::make_shared<DialogResult>();
  Extraction extraction_;
  std::string install_error_;  // not enough space, under the ISO card
  std::chrono::steady_clock::time_point rate_time_{};
  uint64_t rate_done_ = 0;
  double rate_ = 0.0;  // bytes per second, smoothed
  SDL_Window* window_ = nullptr;

  // The DLC page (page_dlc.cpp, dlc.h): the packages found in a folder the player chooses (the folder dialog, or a
  // folder dropped on the window), installed by the game; the ones installed, read when the page opens.
  void BrowseDlc();
  void ScanDlc(const std::string& path);  // UTF-8
  void ListDlc();
  void PollDlc();
  void InstalledDlcCard(ImVec2 pos, float width);
  std::shared_ptr<DialogResult> dlc_dialog_ = std::make_shared<DialogResult>();
  std::string dlc_scanned_;  // the folder searched, UTF-8
  std::future<std::vector<DlcPackage>> dlc_scan_;
  std::optional<std::vector<DlcPackage>> dlc_found_;
  std::future<std::vector<InstalledDlc>> dlc_listing_;
  std::optional<std::vector<InstalledDlc>> dlc_installed_;
  DlcInstall dlc_install_;
  std::string dlc_confirm_;  // the installed folder whose Remove waits for a second press
  bool dlc_listed_ = false;  // read once the page is first shown, and after each change
  bool dlc_refresh_after_install_ = false;

  // The profiles (page_profile.cpp, profile_store.h): the first profile's dialog when there is none, and this
  // installation's saves from before the profiles copied into the current one.
  enum class ProfileDialog { kNone, kCreate, kConflict };
  void LoadProfiles();
  void CheckMigration();
  void RunMigration(profiles::Keep keep);
  void OpenCreateProfile(bool first);
  void ProfileDialogs(ImVec2 size);
  void CreateProfileCard(ImVec2 size);
  void ConflictCard(ImVec2 size);
  void BrowsePhoto();
  void PollPhoto();
  void SetPhoto(std::optional<Rgba> photo);
  // The avatar of the dialog or of a profile in a `size` dp circle at `pos`, with a ring of its color.
  void DrawAvatar(ImVec2 pos, float size, std::string_view avatar, ImTextureID photo, int color);
  // The profile pages (profile_view.cpp, page_profile_*.cpp): every game whose launcher ran, with the current
  // profile's achievements of it; the sidebar's card and its menu; the summary, the games, a game's achievements, the
  // profile's editing.
 public:
  struct ProfileGame {
    profiles::GameInfo info;
    std::optional<Achievements> achievements;  // nullopt: its list was not copied yet
    profiles::Playtime playtime;               // the current profile's
    std::vector<ImTextureID> icons;            // made the first time each is drawn
    std::optional<Rgba> picture_pixels;        // the game's icon (info.picture), until it is made a texture
    ImTextureID picture = ImTextureID_Invalid;
    bool current = false;                      // this launcher's game
    bool installed = false;                    // its launcher is where it last ran
    // When it was last played with this profile, or else when its launcher last ran.
    int64_t LastSeen() const { return playtime.last ? playtime.last : info.seen; }
  };
  struct ProfileTotals {
    int unlocked = 0;
    int total = 0;
    uint32_t gamerscore = 0;
    uint32_t total_gamerscore = 0;
    int games = 0;
    int Level() const { return int(gamerscore / 100) + 1; }
  };
  // The profile's playtime over every game: totals, streaks of days with play, and this week (Monday first).
  struct ProfileActivity {
    int64_t seconds = 0;
    int played = 0;          // games with playtime
    int streak = 0;          // days in a row with play, up to today (or yesterday, before today's first game)
    int best_streak = 0;
    int64_t week[7] = {};
    int64_t week_seconds = 0;
    int today = 0;           // today's index in week
  };

 private:
  void RegisterThisGame();
  void LoadProfileGames();
  void PollProfileGames();
  ProfileTotals Totals() const;
  // Playtime: the session counted from Play to the game's exit (profile_view.cpp).
  void BeginSession();
  void CountSession(bool end);
  // "86 h", "2,5 h", "45 min"; "12 días", "1 día".
  std::string HoursText(int64_t seconds) const;
  std::string DaysText(int days) const;
  // The game played last with this profile (profile_games_'s index), or profile_games_.size() if none was.
  size_t LastPlayedGame() const;
  // A game's Play or Continue: this launcher's game starts; another's launcher opens in place of this one.
  bool CanPlay(const ProfileGame& game) const { return game.current || game.installed; }
  void PlayProfileGame(const ProfileGame& game);
  ImTextureID ProfileGameIcon(ProfileGame& game, size_t index);
  ImTextureID AvatarTexture(const profiles::Profile& profile);
  void ForgetAvatar(const std::string& id);
  void SwitchProfile(const std::string& id);
  std::string UsedText(int64_t unix_time) const;    // "Usado ayer"
  std::string WhenText(int64_t unix_time) const;    // "hoy", "ayer", a date
  // A game's icon, else its mark on its accent, in a `size` dp square at `pos`.
  void GameTile(ImVec2 pos, float size, const ProfileGame& game);
  void SidebarProfileCard(const ImRect& rect);
  void ProfileMenu();
  void ProfilePage(const ImRect& rect);
  void ProfileSummary(ImVec2 pos, float width);
  void ProfileGames(ImVec2 pos, float width);
  void ProfileGamePage(const ImRect& rect);
  void ProfileEditPage(const ImRect& rect);
  void OpenProfileEdit();
  // One achievement's card (the Achievements page and a game's achievements): `pin` -1 none, 0 not pinned, 1 pinned;
  // true when the pin was pressed.
  bool AchievementCard(const ImRect& card, const Achievement& a, ImTextureID icon, int pin, bool pin_enabled);
  std::vector<ProfileGame> profile_games_;
  ProfileActivity profile_activity_;
  std::future<std::vector<ProfileGame>> profile_games_loading_;
  std::filesystem::path session_profile_;  // the running session's profile folder; empty: none is counted
  int64_t session_start_ = 0;              // Unix time
  uint64_t session_mark_ = 0;              // QueryUnbiasedInterruptTime of the last count (100 ns, sleep excluded)
  bool session_counted_ = false;           // its first stretch was written
  std::map<std::string, ImTextureID> avatar_textures_;
  bool profile_menu_open_ = false;
  bool profile_menu_request_ = false;   // another page's button asks the sidebar card to open its menu
  int profile_tab_ = 0;                 // 0 the summary, 1 the games
  int profile_sort_ = 0;                // the games: last played, most played, most complete, name
  size_t profile_game_ = 0;             // the game of the achievements page, in profile_games_
  int profile_game_filter_ = 0;         // its filter: unlocked, locked, all
  std::string edit_id_;                 // the profile the edit page was opened for
  std::string edit_name_;
  std::string edit_motto_;
  int edit_avatar_ = 0;                 // in profiles::kAvatars; -1 the photo
  int edit_color_ = 0;
  std::optional<Rgba> edit_photo_;      // a photo chosen while editing, not saved yet
  ImTextureID edit_photo_texture_ = ImTextureID_Invalid;
  bool edit_delete_armed_ = false;      // Delete pressed once
  std::string edit_message_;
  bool edit_message_error_ = false;
  profiles::Store store_;
  bool profiles_loaded_ = false;        // LoadProfiles ran (after the first run, if there was one)
  profiles::LegacyData legacy_;
  ProfileDialog profile_dialog_ = ProfileDialog::kNone;
  bool profile_dialog_opened_ = false;  // its popup was opened (the first frame focuses the name)
  bool profile_first_ = false;          // the first profile: the dialog cannot be closed
  std::string new_name_;
  int new_avatar_ = 0;                  // in profiles::kAvatars; -1 the photo
  int new_color_ = 0;
  std::optional<Rgba> new_photo_;
  ImTextureID new_photo_texture_ = ImTextureID_Invalid;
  std::string profile_error_;           // under the dialog's fields
  std::shared_ptr<DialogResult> photo_dialog_ = std::make_shared<DialogResult>();
  std::string profile_notice_;          // the Play page's notice: the saves copied, or why not
  bool profile_notice_error_ = false;
};

}  // namespace launcher
