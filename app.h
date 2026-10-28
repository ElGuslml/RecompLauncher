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
#include <array>
#include <optional>
#include <set>
#include <span>
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
// kProfiles .. kProfileEdit are not tabs: the header's avatar opens kProfiles, the others follow from it.
enum class Page : int {
  kPlay, kDlc, kAchievements, kControls, kVideo, kGraphics, kGame, kUpdates,
  kProfiles, kProfile, kProfileGame, kProfileEdit, kCount
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

  // The shell (shell.cpp): the art under every page, darkened by `dim` (0 to 1); the header with the tabs, Donate and
  // the profile's avatar; the footer with the controller, the version and the controller's hints for the page.
  // kA .. kY and kView are ui::Button's (the same order).
  enum class HintKey { kA, kB, kX, kY, kLB, kRB, kView, kBumpers, kArrows };
  struct Hint {
    HintKey key;
    const char* label;
  };
  void Background(ImVec2 size, float dim);
  void Header(const ImRect& rect);
  void Footer(const ImRect& rect, std::span<const Hint> hints);
  std::vector<Hint> PageHints() const;
  // The header and the footer over the page, each a child window of its own; the header's controls join the page's
  // navigation (up from its top), the footer's hints take none.
  void Chrome(ImVec2 size);
  // The first run's steps in the header, between `left` and `right` (pixels).
  void InstallSteps(const ImRect& rect, float left, float right);
  // The tabs: whether `tab` (its first page) holds the current page; the next one that the game has, LB/RB's.
  bool OnTab(Page tab) const;
  void NextTab(int step);
  // Where B goes from the current page (a profile page's parent; Play from the tabs).
  Page BackPage() const;
  // The controller's up and down in a column of controls (`rows`, top first; each row one control, or two side by
  // side): the row above or below, in the same column; up from the first reaches the header, down from the last
  // stays. ImGui's own choice would take the nearest control, which can be the next column's (a list in a child
  // window). True when the focus was in the column and a move was taken.
  bool ColumnNav(std::span<const std::array<ImGuiID, 2>> rows);
  void Donate();

  // Pages, each in its own file (page_*.cpp). `rect` is the area right of the sidebar.
  void PlayPage(const ImRect& rect);
  void DlcPage(const ImRect& rect);
  void AchievementsPage(const ImRect& rect);
  // The Controls tab (page_controls.cpp): the devices, the chosen one's mapping, its help. Its rows, in the mapping's
  // scrolling region at `origin` (pixels), `width` wide: a controller's (page_controls.cpp) or the keyboard's
  // (page_keyboard.cpp). Each returns their height.
  void ControlsPage(const ImRect& rect);
  float PadRows(ImVec2 origin, float width, SDL_GamepadType type, SDL_JoystickType kind);
  float KeyRows(ImVec2 origin, float width);
  // The Settings tab's categories: Game (page kGame), Video (kVideo), Graphics (kGraphics).
  void SettingsPage(const ImRect& rect);
  void UpdatesPage(const ImRect& rect);
  // The release notes (Markdown) at `pos`, `width` wide. Returns the height; draws only when `draw`.
  float ReleaseNotes(const std::string& markdown, ImVec2 pos, float width, bool draw);
  // A notice across the top of the Play page's art (an error exit code, an update installed).
  // `link_page` kCount: the link opens the logs folder; a null `link`: none.
  void Notice(const ImRect& rect, bool error, const char* text, const char* link, Page link_page);
  // When updates were last checked ("22/10/2026 18:04"), or "never".
  std::string LastCheckedText() const;

  // Opens a page; `focus` puts the controller's focus on its first control (a tab changed with LB/RB).
  void Navigate(Page page, bool focus);
  void StartingOverlay(ImVec2 size);

  // The keyboard's key being assigned (page_keyboard.cpp).
  bool OnKeyCapture(const SDL_Event& event);
  // A controller's control being assigned a button (page_controls.cpp).
  bool OnPadCapture(const SDL_Event& event);
  void AssignPad(int input);
  void StartListening(int bind, int slot);
  void StopListening();
  void Assign(std::string token);

  Settings settings_;
  const Texts& t_;
  Page page_ = Page::kPlay;
  Result result_ = Result::kNone;
  bool gamepad_used_ = false;  // the last input came from a controller: the focus shows
  // The buttons' glyphs: the controller or the keyboard used last (ui::SetGlyphStyle); until then, a connected
  // controller's, or the keyboard's.
  void UseGlyphsOf(SDL_GamepadType type);
  ui::GlyphStyle glyph_style_ = ui::GlyphStyle::kKeyboard;
  const char* view_label_ = "View";
  bool glyphs_chosen_ = false;
  ImGuiID page_focus_ = 0;     // the control the controller enters the page on (App::Frame)
  ImGuiID back_focus_ = 0;     // B's target while the focus is in a page's second column (its list); 0: back to Play
  ImGuiID header_focus_ = 0;   // the header's control up from the page reaches: the current tab, or the avatar
  std::vector<ImGuiID> header_items_;  // the header's controls (tabs, Donate, avatar), this frame
  ImGuiID category_focus_ = 0; // the Settings tab's current category button
  int settings_row_[3] = {};   // the row in focus in each Settings category (its help shows)
  bool settings_editing_ = false;  // A on a Settings row: left and right change its value until A or B
  ImGuiID settings_row_focus_ = 0;  // the Settings row in focus last (right from a category returns to it)
  bool controls_focus_mapping_ = false;  // a device chosen with A: the focus goes to its mapping
  ImGuiID mapping_first_ = 0;  // the Controls tab's first row control (the mapping's)
  std::vector<std::array<ImGuiID, 2>> mapping_rows_;  // its rows' controls, this frame (ColumnNav)
  bool focus_page_ = false;    // focus page_focus_ on the next frame (back from the game)
  bool game_running_ = false;
  uint32_t game_exit_code_ = 0;  // the last game's, when not 0: a notice on the Play page

  // The Controls tab's keyboard.
  int listen_bind_ = -1;  // the Bind whose key is being assigned, or -1
  int listen_slot_ = 0;   // 0 = primary, 1 = secondary
  uint8_t held_modifiers_ = 0;
  bool modifier_pressed_ = false;
  std::string notice_;  // a message in place of the keyboard mapping's hint, for a moment
  std::chrono::steady_clock::time_point notice_until_{};

  // The Controls tab's controllers.
  SDL_JoystickID selected_device_ = 0;  // the controller picked; while it is not connected, the first one
  bool keyboard_selected_ = false;      // the keyboard was picked instead
  SDL_GamepadType pad_type_ = SDL_GAMEPAD_TYPE_UNKNOWN;  // the chosen controller's type: its layout is edited
  int pad_listen_ = -1;  // the control waiting for a button, or -1
  bool pad_release_wait_ = false;

  // Achievements page.
  int achievement_filter_ = 0;
  size_t achievement_selected_ = 0;          // the detail's achievement (an index of achievements_->items)
  std::set<uint32_t> revealed_achievements_;  // secret ones revealed this session (their ids)

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
  std::vector<std::string> dlc_installing_names_;  // the folders of the packages being installed
  std::set<std::string> dlc_new_;                  // installed this session: marked "New" in the list

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
  // "Who's playing?" (page_profiles.cpp): every profile, switching, creating.
  void ProfilesPage(const ImRect& rect);
  void ProfilePage(const ImRect& rect);
  void ProfileSummary(const ImRect& body);
  void ProfileGames(const ImRect& body);
  void ProfileGamePage(const ImRect& rect);
  void ProfileEditPage(const ImRect& rect);
  void OpenProfileEdit();
  // The edit page empty, creating another profile.
  void OpenProfileCreate();
  // One achievement's card (the Achievements page and a game's achievements): `pin` -1 none, 0 not pinned, 1 pinned;
  // true when the pin was pressed.
  bool AchievementCard(const ImRect& card, const Achievement& a, ImTextureID icon, int pin, bool pin_enabled);
  std::vector<ProfileGame> profile_games_;
  ProfileActivity profile_activity_;
  // The games, and every profile's gamerscore over them (the profiles page's levels), read on a thread.
  struct ProfileGamesRead {
    std::vector<ProfileGame> games;
    std::map<std::string, uint32_t> gamerscores;
  };
  std::future<ProfileGamesRead> profile_games_loading_;
  std::map<std::string, uint32_t> profile_gamerscores_;
  std::string profiles_selected_;       // the profiles page's chosen card (a profile's id)
  std::filesystem::path session_profile_;  // the running session's profile folder; empty: none is counted
  int64_t session_start_ = 0;              // Unix time
  uint64_t session_mark_ = 0;              // QueryUnbiasedInterruptTime of the last count (100 ns, sleep excluded)
  bool session_counted_ = false;           // its first stretch was written
  std::map<std::string, ImTextureID> avatar_textures_;
  int profile_tab_ = 0;                 // 0 the summary, 1 the games
  int profile_sort_ = 0;                // the games: last played, most played, most complete, name
  size_t profile_game_ = 0;             // the game of the achievements page, in profile_games_
  int profile_game_filter_ = 0;         // its filter: unlocked, locked, all
  std::string edit_id_;                 // the profile the edit page was opened for; empty: creating one
  Page edit_back_ = Page::kProfiles;    // where Cancel and B go from it
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
