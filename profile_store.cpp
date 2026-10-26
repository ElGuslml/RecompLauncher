// The players' profiles (profile_store.h).

#include "profile_store.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>

#include <toml.hpp>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <random>
#include <system_error>

namespace launcher::profiles {
namespace {

namespace fs = std::filesystem;

constexpr wchar_t kStoreName[] = L"ElGuslml Recomps";
// The one XUID of every profile (SDK change 47): the folder of the saves, in the installation and in a profile.
constexpr wchar_t kUserContent[] = L"B13EBABEBABEBABE";
constexpr wchar_t kMarker[] = L"profile_migration.toml";
constexpr wchar_t kNoteName[] = L"SAVES MOVED.txt";

fs::path FromUtf8(std::string_view text) {
  return fs::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

std::string ToUtf8(const fs::path& path) {
  std::u8string text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

int64_t Now() {
  return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::optional<toml::table> ReadToml(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return std::nullopt;
  }
  std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  try {
    return toml::parse(content);
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

// Written to <file>.tmp, then put in place in one step.
bool WriteWhole(const fs::path& path, std::string_view content) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  fs::path temporary = path;
  temporary += L".tmp";
  {
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file) {
      return false;
    }
    file.write(content.data(), std::streamsize(content.size()));
    if (!file) {
      return false;
    }
  }
  return MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

// A TOML basic string.
std::string Quote(std::string_view text) {
  std::string out = "\"";
  for (char c : text) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          char escaped[8];
          std::snprintf(escaped, sizeof(escaped), "\\u%04X", unsigned(c));
          out += escaped;
        } else {
          out += c;
        }
        break;
    }
  }
  return out + "\"";
}

bool IsTitle(const std::wstring& name) {
  return name.size() == 8 && std::all_of(name.begin(), name.end(), [](wchar_t c) { return iswxdigit(c) != 0; });
}

bool HasFiles(const fs::path& folder) {
  std::error_code ec;
  if (!fs::is_directory(folder, ec)) {
    return false;
  }
  for (fs::recursive_directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->is_regular_file(ec)) {
      return true;
    }
  }
  return false;
}

std::string NewId(const Store& store) {
  std::random_device device;
  std::mt19937_64 random((uint64_t(device()) << 32) ^ device() ^ uint64_t(Now()));
  for (;;) {
    char id[17];
    std::snprintf(id, sizeof(id), "%016llX", static_cast<unsigned long long>(random()));
    std::error_code ec;
    if (!fs::exists(store.root / L"profiles" / FromUtf8(id), ec)) {
      return id;
    }
  }
}

std::string Stamp() {
  std::time_t now = std::time(nullptr);
  std::tm local = {};
  localtime_s(&local, &now);
  char text[32];
  std::strftime(text, sizeof(text), "%Y%m%d-%H%M%S", &local);
  return text;
}

// Every file of `from`, copied into `to` (made as needed); `files` counts them. False with `error` on a failure.
bool CopyTree(const fs::path& from, const fs::path& to, int* files, std::string* error) {
  std::error_code ec;
  fs::create_directories(to, ec);
  for (fs::recursive_directory_iterator it(from, ec), end; !ec && it != end; it.increment(ec)) {
    fs::path relative = it->path().lexically_relative(from);
    fs::path target = to / relative;
    if (it->is_directory(ec)) {
      fs::create_directories(target, ec);
    } else if (it->is_regular_file(ec)) {
      fs::create_directories(target.parent_path(), ec);
      if (!fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, ec)) {
        *error = "copy " + ToUtf8(it->path()) + ": " + ec.message();
        return false;
      }
      ++*files;
    }
    if (ec) {
      break;
    }
  }
  if (ec) {
    *error = ToUtf8(from) + ": " + ec.message();
    return false;
  }
  return true;
}

bool SameFile(const fs::path& a, const fs::path& b) {
  std::error_code ec;
  if (fs::file_size(a, ec) != fs::file_size(b, ec) || ec) {
    return false;
  }
  std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
  if (!fa || !fb) {
    return false;
  }
  std::vector<char> ba(1 << 16), bb(1 << 16);
  while (fa && fb) {
    fa.read(ba.data(), std::streamsize(ba.size()));
    fb.read(bb.data(), std::streamsize(bb.size()));
    if (fa.gcount() != fb.gcount() || std::memcmp(ba.data(), bb.data(), size_t(fa.gcount())) != 0) {
      return false;
    }
  }
  return true;
}

// Each file of `from` has its copy in `to`, byte for byte.
bool Verify(const fs::path& from, const fs::path& to, std::string* error) {
  std::error_code ec;
  for (fs::recursive_directory_iterator it(from, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->is_regular_file(ec) && !SameFile(it->path(), to / it->path().lexically_relative(from))) {
      *error = "check " + ToUtf8(it->path()) + ": the copy differs";
      return false;
    }
  }
  return true;
}

// An unlock file's achievements and their dates (FILETIME), as the SDK writes it.
std::map<uint32_t, int64_t> ReadUnlocks(const fs::path& path) {
  std::map<uint32_t, int64_t> unlocks;
  std::optional<toml::table> table = ReadToml(path);
  if (!table) {
    return unlocks;
  }
  if (const toml::table* unlocked = (*table)["unlocked"].as_table()) {
    for (const auto& [key, value] : *unlocked) {
      uint32_t id = uint32_t(std::strtoul(std::string(key.str()).c_str(), nullptr, 10));
      int64_t filetime = 0;
      if (const toml::table* entry = value.as_table()) {
        filetime = (*entry)["filetime"].value_or<int64_t>(0);
      }
      if (id) {
        unlocks[id] = filetime;
      }
    }
  }
  return unlocks;
}

bool WriteUnlocks(const fs::path& path, const std::map<uint32_t, int64_t>& unlocks) {
  std::string content = "# Achievement unlock state - managed by ReXGlue runtime\n\n";
  for (const auto& [id, filetime] : unlocks) {
    content += "[unlocked." + std::to_string(id) + "]\nfiletime = " + std::to_string(filetime) + "\n\n";
  }
  return WriteWhole(path, content);
}

}  // namespace

const Profile* Store::Current() const {
  for (const Profile& profile : profiles) {
    if (profile.id == current) {
      return &profile;
    }
  }
  return nullptr;
}

Profile* Store::Find(std::string_view id) {
  for (Profile& profile : profiles) {
    if (profile.id == id) {
      return &profile;
    }
  }
  return nullptr;
}

fs::path StoreFolder() {
  if (const wchar_t* folder = _wgetenv(L"ELGUSLML_RECOMPS_DIR"); folder && *folder) {
    return fs::path(folder);
  }
  fs::path result;
  PWSTR path = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, nullptr, &path))) {
    result = fs::path(path) / kStoreName;
  }
  CoTaskMemFree(path);
  return result;
}

Store Load() {
  Store store;
  store.root = StoreFolder();
  if (store.root.empty()) {
    return store;
  }
  if (std::optional<toml::table> table = ReadToml(store.root / L"store.toml")) {
    store.found = true;
    store.read_only = (*table)["schema"].value_or<int64_t>(kSchema) > kSchema;
    store.current = (*table)["current"].value_or<std::string>("");
    store.writer = (*table)["writer"].value_or<std::string>("");
  }
  std::error_code ec;
  for (fs::directory_iterator it(store.root / L"profiles", ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_directory(ec)) {
      continue;
    }
    std::optional<toml::table> table = ReadToml(it->path() / L"profile.toml");
    if (!table) {
      continue;
    }
    Profile profile;
    profile.id = ToUtf8(it->path().filename());
    profile.name = (*table)["name"].value_or<std::string>("");
    profile.motto = (*table)["motto"].value_or<std::string>("");
    profile.avatar = (*table)["avatar"].value_or<std::string>("fox");
    profile.color = int((*table)["color"].value_or<int64_t>(0));
    profile.created = (*table)["created"].value_or<int64_t>(0);
    profile.last_used = (*table)["last_used"].value_or<int64_t>(0);
    profile.folder = it->path();
    store.profiles.push_back(std::move(profile));
  }
  std::sort(store.profiles.begin(), store.profiles.end(),
            [](const Profile& a, const Profile& b) { return a.created < b.created; });
  // A current profile that is gone (deleted by hand): the first.
  if (!store.Current() && !store.profiles.empty()) {
    store.current = store.profiles.front().id;
  }
  return store;
}

bool Save(const Store& store, std::string_view writer) {
  if (store.read_only || store.root.empty()) {
    return false;
  }
  std::string content =
      "# The players' profiles of the recompilations' launchers (RecompLauncher's profile_store.h).\n"
      "schema = " + std::to_string(kSchema) + "\n"
      "current = " + Quote(store.current) + "\n"
      "writer = " + Quote(writer) + "\n";
  return WriteWhole(store.root / L"store.toml", content);
}

bool SaveProfile(const Store& store, const Profile& profile) {
  if (store.read_only || profile.folder.empty()) {
    return false;
  }
  std::string content = "name = " + Quote(profile.name) + "\n" +
                        "motto = " + Quote(profile.motto) + "\n" +
                        "avatar = " + Quote(profile.avatar) + "\n" +
                        "color = " + std::to_string(profile.color) + "\n" +
                        "created = " + std::to_string(profile.created) + "\n" +
                        "last_used = " + std::to_string(profile.last_used) + "\n";
  return WriteWhole(profile.folder / L"profile.toml", content);
}

Profile* Create(Store& store, std::string_view name, std::string_view avatar, int color, std::string_view writer) {
  if (store.read_only || store.root.empty()) {
    return nullptr;
  }
  Profile profile;
  profile.id = NewId(store);
  profile.name = Clip(Trim(name), kNameMax);
  profile.avatar = std::string(avatar);
  profile.color = color;
  profile.created = Now();
  profile.last_used = profile.created;
  profile.folder = store.root / L"profiles" / FromUtf8(profile.id);
  std::error_code ec;
  fs::create_directories(profile.folder, ec);
  if (ec || !SaveProfile(store, profile)) {
    return nullptr;
  }
  store.profiles.push_back(std::move(profile));
  store.current = store.profiles.back().id;
  store.found = true;
  if (!Save(store, writer)) {
    return nullptr;
  }
  return &store.profiles.back();
}

size_t CountChars(std::string_view utf8) {
  return size_t(std::count_if(utf8.begin(), utf8.end(), [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

std::string Clip(std::string_view utf8, size_t chars) {
  size_t count = 0;
  for (size_t i = 0; i < utf8.size(); ++i) {
    if ((static_cast<unsigned char>(utf8[i]) & 0xC0) != 0x80) {
      if (count == chars) {
        return std::string(utf8.substr(0, i));
      }
      ++count;
    }
  }
  return std::string(utf8);
}

std::string Trim(std::string_view text) {
  size_t begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string_view::npos) {
    return {};
  }
  size_t end = text.find_last_not_of(" \t\r\n");
  return std::string(text.substr(begin, end - begin + 1));
}

LegacyData FindLegacy(const fs::path& user_data) {
  LegacyData legacy;
  if (user_data.empty()) {
    return legacy;
  }
  std::error_code ec;
  for (fs::directory_iterator it(user_data / kUserContent, ec), end; !ec && it != end; it.increment(ec)) {
    if (IsTitle(it->path().filename().wstring()) && HasFiles(it->path())) {
      legacy.saves.push_back(it->path());
    }
  }
  ec.clear();
  for (fs::directory_iterator it(user_data, ec), end; !ec && it != end; it.increment(ec)) {
    fs::path settings = it->path() / L"profile" / L"User";
    if (IsTitle(it->path().filename().wstring()) && HasFiles(settings)) {
      legacy.settings.push_back(settings);
    }
  }
  ec.clear();
  for (fs::directory_iterator it(user_data / L"achievements", ec), end; !ec && it != end; it.increment(ec)) {
    const fs::path& path = it->path();
    if (it->is_regular_file(ec) && path.extension() == L".toml" && IsTitle(path.stem().wstring())) {
      legacy.unlocks.push_back(path);
    }
  }
  return legacy;
}

bool IsMigrated(const fs::path& user_data, const Store& store) {
  std::optional<toml::table> marker = ReadToml(user_data / kMarker);
  if (!marker) {
    return false;
  }
  std::string id = (*marker)["profile"].value_or<std::string>("");
  return std::any_of(store.profiles.begin(), store.profiles.end(), [&](const Profile& p) { return p.id == id; });
}

std::optional<GameProfile> ForGame(const fs::path& user_data) {
  Store store = Load();
  const Profile* profile = store.Current();
  if (!profile) {
    return std::nullopt;
  }
  if (!IsMigrated(user_data, store) && !FindLegacy(user_data).empty()) {
    return std::nullopt;
  }
  return GameProfile{profile->folder, profile->name};
}

bool ProfileHasDataOf(const Profile& profile, const LegacyData& legacy) {
  for (const fs::path& saves : legacy.saves) {
    if (HasFiles(profile.folder / kUserContent / saves.filename())) {
      return true;
    }
  }
  for (const fs::path& settings : legacy.settings) {
    if (HasFiles(profile.folder / settings.parent_path().parent_path().filename() / L"profile")) {
      return true;
    }
  }
  return false;
}

MigrationResult Migrate(const fs::path& user_data, const LegacyData& legacy, const Profile& profile, Keep keep,
                        const Store& store, std::string_view note) {
  MigrationResult result;
  if (store.read_only) {
    result.error = "the profiles were saved by a newer launcher";
    return result;
  }
  const fs::path backup = store.root / L"backups" / FromUtf8(Stamp() + " " + profile.id);
  // The installation's folder `from` into the profile's `to`; what the profile had there goes to backup\`relative`
  // first, or stays (and the copy is skipped) with Keep::kProfile.
  auto move_in = [&](const fs::path& from, const fs::path& relative) {
    fs::path to = profile.folder / relative;
    std::error_code ec;
    if (HasFiles(to)) {
      if (keep == Keep::kProfile) {
        return true;
      }
      int backed_up = 0;
      if (!CopyTree(to, backup / relative, &backed_up, &result.error) || !Verify(to, backup / relative, &result.error)) {
        return false;
      }
      fs::remove_all(to, ec);
    }
    return CopyTree(from, to, &result.files, &result.error) && Verify(from, to, &result.error);
  };
  for (const fs::path& saves : legacy.saves) {
    if (!move_in(saves, fs::path(kUserContent) / saves.filename())) {
      return result;
    }
  }
  for (const fs::path& settings : legacy.settings) {
    if (!move_in(settings, settings.parent_path().parent_path().filename() / L"profile")) {
      return result;
    }
  }
  // The unlocks: each achievement once, at its earliest date.
  for (const fs::path& unlocks : legacy.unlocks) {
    fs::path to = profile.folder / L"achievements" / unlocks.filename();
    std::map<uint32_t, int64_t> merged = ReadUnlocks(to);
    for (const auto& [id, filetime] : ReadUnlocks(unlocks)) {
      auto it = merged.find(id);
      if (it == merged.end() || (filetime && (!it->second || filetime < it->second))) {
        merged[id] = filetime;
      }
    }
    if (!WriteUnlocks(to, merged)) {
      result.error = "write " + ToUtf8(to);
      return result;
    }
    ++result.files;
  }
  std::string marker =
      "# This installation's saves, settings and achievements were copied into a profile of the launchers\n"
      "# (RecompLauncher's profile_store.h); the game uses that profile's now. They stay here as they were.\n"
      "profile = " + Quote(profile.id) + "\n" +
      "folder = " + Quote(ToUtf8(profile.folder)) + "\n" +
      "date = " + std::to_string(Now()) + "\n" +
      "files = " + std::to_string(result.files) + "\n";
  if (!WriteWhole(user_data / kMarker, marker)) {
    result.error = "write " + ToUtf8(user_data / kMarker);
    return result;
  }
  if (!note.empty()) {
    // UTF-8 with its byte order mark: every text viewer reads the accents.
    WriteWhole(user_data / kNoteName, "ï»¿" + std::string(note));
  }
  result.ok = true;
  return result;
}

}  // namespace launcher::profiles
