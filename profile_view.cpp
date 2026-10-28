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
    ProfileGamesRead read;
    std::vector<ProfileGame>& games = read.games;
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
    // Every profile's gamerscore over the same lists, from its unlocks.
    for (const profiles::Profile& other : store.profiles) {
      uint32_t& score = read.gamerscores[other.id];
      for (const ProfileGame& game : games) {
        if (!game.achievements) continue;
        std::map<uint32_t, int64_t> unlocks = profiles::Unlocks(profiles::UnlocksFile(other, game.info.title_id));
        for (const Achievement& a : game.achievements->items) {
          if (unlocks.count(a.id)) score += a.gamerscore;
        }
      }
    }
    return read;
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
  ProfileGamesRead read = profile_games_loading_.get();
  profile_games_ = std::move(read.games);
  profile_gamerscores_ = std::move(read.gamerscores);
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

}  // namespace launcher
