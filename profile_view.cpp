// What the profile pages read and share (app.h): this launcher's game registered in the profiles' store with its
// achievements list, every registered game with the current profile's achievements and playtime of it, the session
// being counted while the game runs, another game's launcher opened from its card, the profiles' avatars as textures,
// and the sidebar's profile card with its menu (the profiles mockup's "Barra lateral con avatar" and "Menú del
// avatar").

#include "app.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <SDL3/SDL_messagebox.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <initializer_list>
#include <map>

#include "game.h"
#include "launcher_version.h"
#include "pages.h"
#include "profile_art.h"
#include "ui/gfx.h"

namespace launcher {

using namespace ui;

namespace {

int64_t Now() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

// The local day (days since the epoch) of a Unix time.
int64_t LocalDay(int64_t unix_time) {
  std::time_t t = std::time_t(unix_time);
  std::tm local = {};
  localtime_s(&local, &t);
  local.tm_hour = 12;
  local.tm_min = 0;
  local.tm_sec = 0;
  return int64_t(std::mktime(&local)) / 86400;
}

ImU32 Rgb(uint32_t rgb) { return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0xFF); }

uint32_t ToRgb(ImU32 color) {
  return ((color >> IM_COL32_R_SHIFT) & 0xFF) << 16 | ((color >> IM_COL32_G_SHIFT) & 0xFF) << 8 |
         ((color >> IM_COL32_B_SHIFT) & 0xFF);
}

// Days since 1970-01-01 of a civil date (Howard Hinnant's days_from_civil): days in a row, without time zones.
int64_t DayNumber(int year, int month, int day) {
  year -= month <= 2;
  int64_t era = (year >= 0 ? year : year - 399) / 400;
  int64_t yoe = year - era * 400;
  int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

// A playtime day ("2026-10-26"), or -1.
int64_t DayNumber(const std::string& date) {
  int year = 0, month = 0, day = 0;
  if (std::sscanf(date.c_str(), "%d-%d-%d", &year, &month, &day) != 3) {
    return -1;
  }
  return DayNumber(year, month, day);
}

int64_t Today() {
  std::time_t now = std::time(nullptr);
  std::tm local = {};
  localtime_s(&local, &now);
  return DayNumber(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

App::ProfileActivity ActivityOf(const std::vector<App::ProfileGame>& games) {
  App::ProfileActivity activity;
  std::map<int64_t, int64_t> days;
  for (const App::ProfileGame& game : games) {
    activity.seconds += game.playtime.seconds;
    activity.played += game.playtime.seconds > 0;
    for (const auto& [date, seconds] : game.playtime.days) {
      if (int64_t day = DayNumber(date); day >= 0 && seconds > 0) days[day] += seconds;
    }
  }
  // The longest run of days, and the one that reaches today or yesterday.
  int64_t today = Today();
  int run = 0;
  int64_t previous = -2;
  for (const auto& [day, seconds] : days) {
    run = day == previous + 1 ? run + 1 : 1;
    previous = day;
    activity.best_streak = std::max(activity.best_streak, run);
    if (day == today || day == today - 1) activity.streak = run;
  }
  // 1970-01-01 was a Thursday: Monday is 0.
  activity.today = int((today + 3) % 7);
  int64_t monday = today - activity.today;
  for (int i = 0; i < 7; ++i) {
    auto it = days.find(monday + i);
    activity.week[i] = it != days.end() ? it->second : 0;
    activity.week_seconds += activity.week[i];
  }
  return activity;
}

// `a` over `b` at `t` (0 = b, 1 = a).
ImU32 Mix(ImU32 a, ImU32 b, float t) {
  auto channel = [&](int shift) {
    float x = float((a >> shift) & 0xFF), y = float((b >> shift) & 0xFF);
    return uint32_t(y + (x - y) * t + 0.5f) << shift;
  };
  return channel(IM_COL32_R_SHIFT) | channel(IM_COL32_G_SHIFT) | channel(IM_COL32_B_SHIFT) | (0xFFu << IM_COL32_A_SHIFT);
}

}  // namespace

void App::RegisterThisGame() {
  if (store_.read_only || !store_.found) {
    return;
  }
  profiles::GameInfo info;
  info.title_id = game::kTitleId;
  info.name = game::kName;
  info.accent = ToRgb(col::kAccent);
  info.icon = game::kBrandIcon;
  wchar_t exe[MAX_PATH];
  if (GetModuleFileNameW(nullptr, exe, MAX_PATH)) {
    info.launcher = exe;
  }
  info.version = kVersion;
  info.seen = Now();
  profiles::RegisterGame(store_, info);
  profiles::SaveGameIcon(store_, game::kTitleId, game_icon_png_);
  profiles::CopyAchievementList(store_, game::kTitleId, AchievementsFolder(exe_dir_));
}

void App::LoadProfileGames() {
  if (!store_.Current()) {
    return;
  }
  profiles::Store store = store_;
  uint32_t language = language_;
  profile_games_loading_ = std::async(std::launch::async, [store, language] {
    std::vector<ProfileGame> games;
    const profiles::Profile* profile = store.Current();
    for (profiles::GameInfo& info : profiles::Games(store)) {
      ProfileGame game;
      game.current = info.title_id == game::kTitleId;
      game.achievements = LoadAchievementsFrom(profiles::GameAchievementsFolder(store, info.title_id), info.title_id,
                                               profiles::UnlocksFile(*profile, info.title_id), language);
      game.playtime = profiles::ReadPlaytime(profiles::PlaytimeFile(profile->folder, info.title_id));
      if (!info.picture.empty()) {
        game.picture_pixels = LoadPicture(info.picture);
      }
      std::error_code ec;
      game.installed = !info.launcher.empty() && std::filesystem::is_regular_file(info.launcher, ec);
      game.info = std::move(info);
      games.push_back(std::move(game));
    }
    // This launcher's game first, then the others as their launchers last ran.
    std::stable_sort(games.begin(), games.end(), [](const ProfileGame& a, const ProfileGame& b) {
      return a.current != b.current ? a.current : a.info.seen > b.info.seen;
    });
    return games;
  });
}

void App::PollProfileGames() {
  if (!profile_games_loading_.valid() ||
      profile_games_loading_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    return;
  }
  for (ProfileGame& game : profile_games_) {
    for (ImTextureID& icon : game.icons) {
      gfx::DestroyTexture(icon);
    }
    gfx::DestroyTexture(game.picture);
  }
  profile_games_ = profile_games_loading_.get();
  for (ProfileGame& game : profile_games_) {
    game.icons.assign(game.achievements ? game.achievements->items.size() : 0, ImTextureID_Invalid);
    if (game.picture_pixels) {
      game.picture = gfx::CreateTexture(game.picture_pixels->pixels.data(), game.picture_pixels->width,
                                        game.picture_pixels->height);
      game.picture_pixels.reset();
    }
  }
  if (profile_game_ >= profile_games_.size()) {
    profile_game_ = 0;
  }
  profile_activity_ = ActivityOf(profile_games_);
}

void App::BeginSession() {
  session_profile_.clear();
  if (store_.read_only) {
    return;
  }
  // The profile the game runs with (none while the installation's old saves are not copied yet).
  if (std::optional<profiles::GameProfile> profile = profiles::ForGame(GameUserDataFolder(exe_dir_))) {
    session_profile_ = profile->folder;
    session_start_ = Now();
    ULONGLONG mark = 0;
    QueryUnbiasedInterruptTime(&mark);
    session_mark_ = mark;
    session_counted_ = false;
  }
}

void App::CountSession(bool end) {
  if (session_profile_.empty()) {
    return;
  }
  // The time since the last count without the computer's sleep (the unbiased interrupt time stops meanwhile).
  ULONGLONG mark = 0;
  QueryUnbiasedInterruptTime(&mark);
  int64_t seconds = int64_t((mark - session_mark_) / 10'000'000ULL);
  if (!end && seconds < 60) {
    return;
  }
  // A game that closed within seconds (it did not start) is no session.
  if (end && !session_counted_ && seconds < 10) {
    session_profile_.clear();
    return;
  }
  if (seconds > 0) {
    int64_t now = Now();
    if (profiles::AddPlaytime(session_profile_, game::kTitleId, now - seconds, now, !session_counted_)) {
      session_counted_ = true;
      session_mark_ += uint64_t(seconds) * 10'000'000ULL;
    }
  }
  if (end) {
    session_profile_.clear();
  }
}

void App::GameTick() { CountSession(false); }

std::string App::HoursText(int64_t seconds) const {
  char text[32];
  if (seconds <= 0) {
    return "0 h";
  }
  if (seconds < 3600) {
    std::snprintf(text, sizeof(text), "%d min", int(std::max<int64_t>(1, seconds / 60)));
    return text;
  }
  double hours = double(seconds) / 3600.0;
  if (hours >= 9.95) {
    std::snprintf(text, sizeof(text), "%d h", int(std::lround(hours)));
    return text;
  }
  // One decimal below ten hours, without a trailing zero ("2 h", "2,5 h").
  int tenths = int(std::lround(hours * 10.0));
  if (tenths % 10 == 0) {
    std::snprintf(text, sizeof(text), "%d h", tenths / 10);
  } else {
    std::snprintf(text, sizeof(text), "%d%s%d h", tenths / 10, t_.decimal_separator, tenths % 10);
  }
  return text;
}

std::string App::DaysText(int days) const {
  if (days == 1) {
    return t_.day_one;
  }
  char text[48];
  std::snprintf(text, sizeof(text), t_.days_count, days);
  return text;
}

size_t App::LastPlayedGame() const {
  size_t last = profile_games_.size();
  for (size_t i = 0; i < profile_games_.size(); ++i) {
    if (profile_games_[i].playtime.last &&
        (last == profile_games_.size() || profile_games_[i].playtime.last > profile_games_[last].playtime.last)) {
      last = i;
    }
  }
  return last;
}

void App::PlayProfileGame(const ProfileGame& game) {
  if (game.current) {
    result_ = Result::kPlay;
    return;
  }
  // The other game's launcher, on its own (out of a job that would close it with this one, when the job allows),
  // then this one closes: the other handles its updates, first run and settings.
  std::wstring command_line = L"\"" + game.info.launcher.wstring() + L"\"";
  std::filesystem::path folder = game.info.launcher.parent_path();
  STARTUPINFOW startup = {sizeof(startup)};
  PROCESS_INFORMATION process = {};
  AllowSetForegroundWindow(ASFW_ANY);
  bool started = false;
  for (DWORD flags : {DWORD(CREATE_BREAKAWAY_FROM_JOB), DWORD(0)}) {
    if (CreateProcessW(game.info.launcher.c_str(), command_line.data(), nullptr, nullptr, FALSE, flags, nullptr,
                       folder.c_str(), &startup, &process)) {
      started = true;
      break;
    }
  }
  if (!started) {
    char text[512];
    std::snprintf(text, sizeof(text), t_.open_launcher_failed, game.info.name.c_str());
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, game::kLauncherName, text, window_);
    return;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  result_ = Result::kQuit;
}

App::ProfileTotals App::Totals() const {
  ProfileTotals totals;
  for (const ProfileGame& game : profile_games_) {
    ++totals.games;
    if (game.achievements) {
      totals.unlocked += game.achievements->unlocked;
      totals.total += int(game.achievements->items.size());
      totals.gamerscore += game.achievements->gamerscore;
      totals.total_gamerscore += game.achievements->total_gamerscore;
    }
  }
  return totals;
}

ImTextureID App::ProfileGameIcon(ProfileGame& game, size_t index) {
  if (!game.achievements || index >= game.icons.size()) {
    return ImTextureID_Invalid;
  }
  Achievement& a = game.achievements->items[index];
  if (game.icons[index] == ImTextureID_Invalid && !a.icon.empty()) {
    game.icons[index] = gfx::CreateTexture(a.icon.data(), a.icon_width, a.icon_height);
    a.icon.clear();
    a.icon.shrink_to_fit();
  }
  return game.icons[index];
}

ImTextureID App::AvatarTexture(const profiles::Profile& profile) {
  if (profile.avatar != profiles::kPhoto) {
    return ImTextureID_Invalid;
  }
  auto it = avatar_textures_.find(profile.id);
  if (it == avatar_textures_.end()) {
    ImTextureID texture = ImTextureID_Invalid;
    if (std::optional<Rgba> photo = LoadSquarePicture(profile.folder / L"avatar.png", 256)) {
      texture = gfx::CreateTexture(photo->pixels.data(), photo->width, photo->height);
    }
    it = avatar_textures_.emplace(profile.id, texture).first;
  }
  return it->second;
}

void App::ForgetAvatar(const std::string& id) {
  auto it = avatar_textures_.find(id);
  if (it != avatar_textures_.end()) {
    gfx::DestroyTexture(it->second);
    avatar_textures_.erase(it);
  }
}

void App::SwitchProfile(const std::string& id) {
  if (store_.read_only || id == store_.current) {
    return;
  }
  store_.current = id;
  if (profiles::Profile* profile = store_.Find(id)) {
    profile->last_used = Now();
    profiles::SaveProfile(store_, *profile);
  }
  profiles::Save(store_, RECOMP_LAUNCHER_VERSION_STR);
  profile_notice_.clear();
  // The Achievements page and the profile pages are the new profile's.
  LoadAchievements();
  LoadProfileGames();
  CheckMigration();
}

std::string App::WhenText(int64_t unix_time) const {
  int64_t days = LocalDay(Now()) - LocalDay(unix_time);
  if (days <= 0) {
    return t_.today;
  }
  if (days == 1) {
    return t_.yesterday;
  }
  constexpr uint64_t kUnixEpoch = 116'444'736'000'000'000ULL;
  return FormatDate(uint64_t(unix_time) * 10'000'000ULL + kUnixEpoch, language_);
}

std::string App::UsedText(int64_t unix_time) const {
  int64_t days = LocalDay(Now()) - LocalDay(unix_time);
  char text[128];
  if (days <= 0) {
    return t_.used_today;
  }
  if (days == 1) {
    return t_.used_yesterday;
  }
  if (days < 30) {
    std::snprintf(text, sizeof(text), t_.used_days_ago, int(days));
    return text;
  }
  std::snprintf(text, sizeof(text), t_.used_on, WhenText(unix_time).c_str());
  return text;
}

void App::GameTile(ImVec2 pos, float size, const ProfileGame& game) {
  const profiles::GameInfo& info = game.info;
  ImRect tile(pos, ImVec2(pos.x + Px(size), pos.y + Px(size)));
  // This launcher's own icon (found in this session), else the copy its launcher left in the store.
  ImTextureID picture = game.current && game_icon_ != ImTextureID_Invalid ? game_icon_ : game.picture;
  if (picture != ImTextureID_Invalid) {
    DrawRoundedTexture(picture, tile, size * 0.22f);
    StrokeRect(tile, Fade(col::kText, 0.08f), size * 0.22f);
    return;
  }
  ImU32 accent = info.accent ? Rgb(info.accent) : col::kAccent;
  FillRect(tile, Mix(accent, col::kCard, 0.14f), size * 0.22f);
  float icon = size * 0.52f;
  ImVec2 at(pos.x + Px((size - icon) * 0.5f), pos.y + Px((size - icon) * 0.5f));
  if (!info.icon.empty()) {
    char key[32];
    std::snprintf(key, sizeof(key), "game%08X", info.title_id);
    DrawIconSvg(key, info.icon, at, icon, accent, 2.0f);
  }
}

void App::SidebarProfileCard(const ImRect& rect) {
  const profiles::Profile* profile = store_.Current();
  if (!profile) {
    return;
  }
  bool active = page_ == Page::kProfile || page_ == Page::kProfileGame || page_ == Page::kProfileEdit ||
                profile_menu_open_;
  ItemState state = Item("##profile_card", rect);
  FillRect(rect, active || state.hovered ? col::kCardRaised : col::kSidebar, 10.0f);
  StrokeRect(rect, active ? col::kAccent : col::kSidebarBorder, 10.0f);
  float avatar = 40.0f;
  ImVec2 avatar_pos(rect.Min.x + Px(10.0f), rect.Min.y + (rect.GetHeight() - Px(avatar)) * 0.5f);
  DrawAvatar(avatar_pos, avatar, profile->avatar, AvatarTexture(*profile), profile->color);
  constexpr Type kName{Font::kBold, 14};
  constexpr Type kLevel{Font::kSemiBold, 12};
  float text_x = avatar_pos.x + Px(avatar + 12.0f);
  float text_width = rect.Max.x - Px(34.0f) - text_x;
  float block = LineHeight(kName) + Px(2.0f) + LineHeight(kLevel);
  float text_y = rect.Min.y + (rect.GetHeight() - block) * 0.5f;
  DrawLabel(kName, ImVec2(text_x, text_y), col::kText, Ellipsize(kName, profile->name, text_width));
  if (!profile_games_.empty()) {
    char level[48];
    std::snprintf(level, sizeof(level), t_.level, Totals().Level());
    DrawLabel(kLevel, ImVec2(text_x, text_y + LineHeight(kName) + Px(2.0f)), col::kAccent, level);
  }
  DrawIcon(Icon::kChevronUp, ImVec2(rect.Max.x - Px(26.0f), rect.Min.y + (rect.GetHeight() - Px(16.0f)) * 0.5f), 16.0f,
           col::kTextMuted, 2.0f);
  if (state.focused) {
    FocusRing(rect, 10.0f);
  }
  if (state.pressed || profile_menu_request_) {
    ImGui::OpenPopup("##profile_menu");
    profile_menu_request_ = false;
  }
  // The menu, right of the sidebar, its foot level with the card's.
  ImGui::SetNextWindowPos(ImVec2(rect.Max.x + Px(34.0f), rect.Max.y + Px(16.0f)), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
  ProfileMenu();
}

void App::ProfileMenu() {
  constexpr float kWidth = 372.0f;
  constexpr float kPadding = 20.0f;
  ImGui::SetNextWindowSize(ImVec2(Px(kWidth), 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(0, 0, 0, 0));
  profile_menu_open_ = ImGui::BeginPopup("##profile_menu", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(3);
  if (!profile_menu_open_) {
    return;
  }
  const profiles::Profile* profile = store_.Current();
  if (!profile) {
    ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return;
  }
  ImDrawList* draw = ImGui::GetWindowDrawList();
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float width = Px(kWidth);
  float x = origin.x + Px(kPadding);
  float inner = width - Px(kPadding * 2.0f);
  // The card's background goes under its rows (two channels: its height is known at the end).
  draw->ChannelsSplit(2);
  draw->ChannelsSetCurrent(1);
  float y = origin.y + Px(kPadding);
  bool close = false;

  // The profile: its avatar, name, motto, level and its progress.
  constexpr Type kName{Font::kCondensedBold, 24};
  constexpr Type kMotto{Font::kRegular, 13};
  constexpr Type kLevel{Font::kSemiBold, 12};
  DrawAvatar(ImVec2(x, y), 64.0f, profile->avatar, AvatarTexture(*profile), profile->color);
  float text_x = x + Px(64.0f + 14.0f);
  float text_width = x + inner - text_x;
  ProfileTotals totals = Totals();
  DrawLabel(kName, ImVec2(text_x, y + Px(2.0f)), col::kText, Ellipsize(kName, profile->name, text_width));
  float line_y = y + Px(2.0f) + LineHeight(kName);
  if (!profile->motto.empty()) {
    DrawLabel(kMotto, ImVec2(text_x, line_y), col::kTextMuted, Ellipsize(kMotto, profile->motto, text_width));
    line_y += LineHeight(kMotto) + Px(2.0f);
  }
  char level[48];
  std::snprintf(level, sizeof(level), t_.level, totals.Level());
  DrawLabel(kLevel, ImVec2(text_x, line_y), col::kAccent, level);
  float bar_x = text_x + MeasureText(kLevel, level).x + Px(10.0f);
  float bar_y = line_y + (LineHeight(kLevel) - Px(6.0f)) * 0.5f;
  Progress(ImRect(ImVec2(bar_x, bar_y), ImVec2(x + inner, bar_y + Px(6.0f))), float(totals.gamerscore % 100) / 100.0f);
  y += std::max(Px(64.0f), line_y + LineHeight(kLevel) - y) + Px(16.0f);

  // See my profile, edit.
  ButtonSpec see{ButtonKind::kPrimary, Type{Font::kBold, 15}, 44, 16, 8};
  ButtonSpec edit{ButtonKind::kGhost, Type{Font::kSemiBold, 15}, 44, 16, 8, Icon::kPencil, 16, 2};
  float edit_width = ButtonWidth(edit, t_.edit);
  if (Button("##see_profile", ImVec2(x, y), see, t_.see_my_profile, true, inner - edit_width - Px(8.0f))) {
    profile_tab_ = 0;
    Navigate(Page::kProfile, false);
    close = true;
  }
  if (Button("##edit_profile", ImVec2(x + inner - edit_width, y), edit, t_.edit, !store_.read_only)) {
    OpenProfileEdit();
    close = true;
  }
  y += Px(44.0f) + Px(16.0f);
  draw->AddLine(ImVec2(x, y), ImVec2(x + inner, y), col::kDivider, Px(1.0f));
  y += Px(16.0f);

  // The other profiles, and a new one.
  constexpr Type kSection{Font::kBold, 11, 0.1f};
  constexpr Type kRowName{Font::kSemiBold, 15};
  constexpr Type kRowSub{Font::kRegular, 12};
  DrawLabel(kSection, ImVec2(x, y), col::kLabel, t_.switch_profile);
  y += LineHeight(kSection) + Px(8.0f);
  for (const profiles::Profile& other : store_.profiles) {
    if (other.id == profile->id) {
      continue;
    }
    ImRect row(ImVec2(x - Px(8.0f), y), ImVec2(x + inner + Px(8.0f), y + Px(56.0f)));
    ImGui::PushID(other.id.c_str());
    ItemState state = Item("##row", row, !store_.read_only);
    ImGui::PopID();
    if (state.hovered || state.focused) {
      FillRect(row, col::kCardSelected, 8.0f);
    }
    DrawAvatar(ImVec2(x, y + Px(8.0f)), 40.0f, other.avatar, AvatarTexture(other), other.color);
    float row_x = x + Px(40.0f + 12.0f);
    float row_width = x + inner - row_x;
    float block = LineHeight(kRowName) + Px(2.0f) + LineHeight(kRowSub);
    float row_y = y + (Px(56.0f) - block) * 0.5f;
    DrawLabel(kRowName, ImVec2(row_x, row_y), col::kText, Ellipsize(kRowName, other.name, row_width));
    DrawLabel(kRowSub, ImVec2(row_x, row_y + LineHeight(kRowName) + Px(2.0f)), col::kTextMuted,
              Ellipsize(kRowSub, UsedText(other.last_used), row_width));
    if (state.focused) {
      FocusRing(row, 8.0f);
    }
    if (state.pressed) {
      SwitchProfile(other.id);
      close = true;
    }
    y += Px(56.0f) + Px(2.0f);
  }
  {
    ImRect row(ImVec2(x - Px(8.0f), y), ImVec2(x + inner + Px(8.0f), y + Px(56.0f)));
    ItemState state = Item("##new_profile", row, !store_.read_only);
    if (state.hovered || state.focused) {
      FillRect(row, col::kCardSelected, 8.0f);
    }
    ImVec2 center(x + Px(20.0f), y + Px(28.0f));
    float radius = Px(19.0f);
    for (int i = 0; i < 24; i += 2) {
      float a0 = float(i) * 3.14159265f / 12.0f, a1 = float(i + 1) * 3.14159265f / 12.0f;
      draw->PathArcTo(center, radius, a0, a1, 4);
      draw->PathStroke(col::kDashed, ImDrawFlags_None, Px(1.5f));
    }
    DrawIcon(Icon::kPlus, ImVec2(center.x - Px(9.0f), center.y - Px(9.0f)), 18.0f, col::kTextSoft, 2.0f);
    DrawLabel(kRowName, ImVec2(x + Px(52.0f), y), col::kText, t_.new_profile, Px(56.0f));
    if (state.focused) {
      FocusRing(row, 8.0f);
    }
    if (state.pressed) {
      OpenCreateProfile(false);
      close = true;
    }
    y += Px(56.0f) + Px(12.0f);
  }
  // The note: the profiles are every launcher's.
  constexpr Type kNote{Font::kRegular, 12};
  DrawIcon(Icon::kInfo, ImVec2(x, y + (LineHeight(kNote) - Px(14.0f)) * 0.5f), 14.0f, col::kTextDim, 2.0f);
  float note_height = DrawParagraph(kNote, ImVec2(x + Px(22.0f), y), inner - Px(22.0f), 1.4f, col::kTextDim,
                                    t_.profiles_shared);
  y += note_height + Px(kPadding);

  draw->ChannelsSetCurrent(0);
  ImRect card(origin, ImVec2(origin.x + width, y));
  FillRect(card, col::kCard, 14.0f);
  StrokeRect(card, col::kOutline, 14.0f);
  draw->ChannelsMerge();
  ImGui::Dummy(ImVec2(width, y - origin.y));
  if (close || ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
}

}  // namespace launcher
