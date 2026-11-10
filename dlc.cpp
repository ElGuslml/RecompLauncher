// Downloadable content: see dlc.h.

#include "dlc.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <set>
#include <utility>

#include "game.h"

namespace launcher {

namespace {

// An STFS header's fields (Free60's layout; the SDK's StfsHeader): all big-endian.
constexpr size_t kContentTypeOffset = 0x344;
constexpr size_t kContentSizeOffset = 0x34C;
constexpr size_t kTitleIdOffset = 0x360;
constexpr size_t kDisplayNameOffset = 0x411;  // 18 languages of 0x80 bytes, UTF-16, English first
constexpr size_t kTitleNameOffset = 0x1691;   // 0x80 bytes, UTF-16
constexpr size_t kNameBytes = 0x80;
constexpr size_t kHeaderBytes = kTitleNameOffset + kNameBytes;
// The thumbnails' sizes, then the thumbnail and the title thumbnail (PNG, at most 0x4000 bytes each).
constexpr size_t kThumbnailSizesOffset = 0x1712;
constexpr size_t kThumbnailOffset = 0x171A;
constexpr size_t kTitleThumbnailOffset = 0x571A;
constexpr size_t kThumbnailBytes = 0x4000;
constexpr size_t kLanguages = 18;
// A folder is searched this deep, and this many entries at most (a drive chosen by mistake).
constexpr int kMaxDepth = 5;
constexpr int kMaxEntries = 20000;

uint32_t ReadU32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }

uint64_t ReadU64(const uint8_t* p) { return uint64_t(ReadU32(p)) << 32 | ReadU32(p + 4); }

std::string Utf8(const std::wstring& wide) {
  if (wide.empty()) return {};
  int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0, nullptr, nullptr);
  std::string text(size_t(std::max(length, 0)), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), text.data(), length, nullptr, nullptr);
  return text;
}

// A big-endian UTF-16 string of at most `bytes`, up to its terminator.
std::string Utf16BeToUtf8(const uint8_t* p, size_t bytes) {
  std::wstring wide;
  for (size_t i = 0; i + 1 < bytes; i += 2) {
    wchar_t c = wchar_t(p[i] << 8 | p[i + 1]);
    if (!c) break;
    wide.push_back(c);
  }
  // Names padded with spaces.
  while (!wide.empty() && wide.back() == L' ') wide.pop_back();
  return Utf8(wide);
}

bool IsPackage(const uint8_t* magic) {
  return !std::memcmp(magic, "LIVE", 4) || !std::memcmp(magic, "PIRS", 4) || !std::memcmp(magic, "CON ", 4);
}

bool ReadPackage(const std::filesystem::path& path, DlcPackage& out) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || std::filesystem::file_size(path, ec) < kHeaderBytes) {
    return false;
  }
  std::array<uint8_t, kHeaderBytes> header{};
  {
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(header.data()), header.size())) {
      return false;
    }
  }
  if (!IsPackage(header.data())) {
    return false;
  }
  out.path = path;
  out.file_name = Utf8(path.filename().wstring());
  out.content_type = ReadU32(&header[kContentTypeOffset]);
  out.content_size = ReadU64(&header[kContentSizeOffset]);
  out.title_id = ReadU32(&header[kTitleIdOffset]);
  for (size_t i = 0; i < kLanguages && out.display_name.empty(); ++i) {
    out.display_name = Utf16BeToUtf8(&header[kDisplayNameOffset + i * kNameBytes], kNameBytes);
  }
  if (out.display_name.empty()) {
    out.display_name = out.file_name;
  }
  out.title_name = Utf16BeToUtf8(&header[kTitleNameOffset], kNameBytes);
  return true;
}

uint64_t FolderBytes(const std::filesystem::path& folder) {
  uint64_t bytes = 0;
  std::error_code ec;
  for (auto it = std::filesystem::recursive_directory_iterator(
           folder, std::filesystem::directory_options::skip_permission_denied, ec);
       !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
    std::error_code size_ec;
    if (it->is_regular_file(size_ec)) {
      bytes += it->file_size(size_ec);
    }
  }
  return bytes;
}

// The header the game writes next to an installed package, last (ContentManager::WriteContentHeaderFile):
// <title folder>\Headers\00000002\<package>.header, an XCONTENT_AGGREGATE_DATA.
std::filesystem::path HeaderPath(const std::filesystem::path& exe_dir, const std::wstring& folder) {
  return DlcFolder(exe_dir).parent_path() / L"Headers" / L"00000002" / (folder + L".header");
}

std::wstring WideFromUtf8(const std::string& utf8) {
  int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
  std::wstring wide(size_t(std::max(length, 1)), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), length);
  wide.resize(wcslen(wide.c_str()));
  return wide;
}

// A folder name the page passes back: only a folder directly in the DLC folders.
bool IsPlainName(const std::wstring& name) {
  return !name.empty() && name != L"." && name != L".." && name.find_first_of(L"\\/:") == std::wstring::npos;
}

// The packages the player turned on although the game does not read them (DisabledDlcFolder's list), UTF-8.
std::filesystem::path TurnedOnListPath(const std::filesystem::path& exe_dir) {
  return DisabledDlcFolder(exe_dir) / L"turned on by the player.txt";
}

std::set<std::string> ReadTurnedOn(const std::filesystem::path& exe_dir) {
  std::set<std::string> names;
  std::ifstream file(TurnedOnListPath(exe_dir), std::ios::binary);
  std::string line;
  while (std::getline(file, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
    if (!line.empty()) names.insert(line);
  }
  return names;
}

void WriteTurnedOn(const std::filesystem::path& exe_dir, const std::set<std::string>& names) {
  std::error_code ec;
  if (names.empty()) {
    std::filesystem::remove(TurnedOnListPath(exe_dir), ec);
    return;
  }
  std::filesystem::create_directories(DisabledDlcFolder(exe_dir), ec);
  std::ofstream file(TurnedOnListPath(exe_dir), std::ios::binary | std::ios::trunc);
  for (const std::string& name : names) {
    file << name << "\r\n";
  }
}

// Neversoft's QBKey: CRC-32 (0xEDB88320) from 0xFFFFFFFF without the final inversion, of the name in lower case with '/'
// as '\'.
uint32_t QbKey(const std::string& text) {
  static const std::array<uint32_t, 256> table = [] {
    std::array<uint32_t, 256> t{};
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = (c & 1) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
      t[i] = c;
    }
    return t;
  }();
  uint32_t crc = 0xFFFFFFFFu;
  for (char ch : text) {
    uint8_t c = uint8_t(ch);
    if (c >= 'A' && c <= 'Z') c = uint8_t(c + 32);
    if (c == '/') c = '\\';
    crc = table[(crc ^ c) & 0xFF] ^ (crc >> 8);
  }
  return crc;
}

// The key the game knows a package by (game::DlcRule): its display name's first `length` characters, a letter or digit
// in lower case, anything else an underscore (Guitar Hero 5's sub_823C0608, GH Warriors of Rock's sub_823B0A48).
uint32_t PackageKey(const std::wstring& display_name, size_t length) {
  std::string name;
  for (size_t i = 0; i < display_name.size() && i < length; ++i) {
    wchar_t c = display_name[i];
    bool alnum = (c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
    name.push_back(alnum ? char(c >= L'A' && c <= L'Z' ? c + 32 : c) : '_');
  }
  return QbKey(name);
}

// The display name the game gets for an installed package (the SDK's ContentManager::ListContent): the header's, up to
// its terminator, else the folder's name.
std::wstring GameDisplayName(const std::filesystem::path& exe_dir, const std::wstring& folder) {
  std::array<uint8_t, 8 + 256> header{};
  std::ifstream file(HeaderPath(exe_dir, folder), std::ios::binary);
  if (!file.read(reinterpret_cast<char*>(header.data()), header.size())) {
    return folder;
  }
  std::wstring name;
  for (size_t i = 8; i + 1 < header.size(); i += 2) {
    wchar_t c = wchar_t(header[i] << 8 | header[i + 1]);
    if (!c) break;
    name.push_back(c);
  }
  return name.empty() ? folder : name;
}

bool ContainsKey(const std::vector<uint8_t>& data, uint32_t key) {
  const uint8_t bytes[4] = {uint8_t(key >> 24), uint8_t(key >> 16), uint8_t(key >> 8), uint8_t(key)};
  return std::search(data.begin(), data.end(), std::begin(bytes), std::end(bytes)) != data.end();
}

// One installed package, as the check sees it.
struct Checked {
  std::filesystem::path path;
  uint32_t key = 0;
  bool fits = false;
  std::vector<std::vector<uint8_t>> manifests;  // its files of the rule's manifest prefix
};

// Which of the packages the game reads (game::kDlcRule; see game_types.h): one holding one of the rule's files, or whose
// key is in the game's own list, its own manifest, or a manifest of a package on that the game reads (the game sees no
// package off).
std::vector<bool> CheckPackages(const std::vector<std::filesystem::path>& packages,
                                const std::vector<std::wstring>& names, const std::vector<bool>& on) {
  const game::DlcRule& rule = game::kDlcRule;
  std::vector<bool> result(packages.size(), true);
  if (rule.files.empty() && !rule.manifest_prefix) {
    return result;
  }
  std::vector<Checked> checked(packages.size());
  std::wstring prefix;
  if (rule.manifest_prefix) {
    prefix = WideFromUtf8(rule.manifest_prefix);
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::towlower);
  }
  for (size_t i = 0; i < packages.size(); ++i) {
    Checked& c = checked[i];
    c.path = packages[i];
    std::error_code ec;
    for (const char* file : rule.files) {
      if (std::filesystem::is_regular_file(c.path / WideFromUtf8(file), ec)) {
        c.fits = true;
      }
    }
    if (!rule.manifest_prefix) {
      continue;
    }
    c.key = PackageKey(names[i], rule.name_length);
    if (std::find(rule.known_names.begin(), rule.known_names.end(), c.key) != rule.known_names.end()) {
      c.fits = true;
    }
    wchar_t own[64];
    std::swprintf(own, std::size(own), L"%ls%x.pak.xen", prefix.c_str(), c.key);
    for (const auto& entry : std::filesystem::directory_iterator(c.path, ec)) {
      std::error_code file_ec;
      if (!entry.is_regular_file(file_ec)) continue;
      std::wstring file = entry.path().filename().wstring();
      std::transform(file.begin(), file.end(), file.begin(), ::towlower);
      if (file.compare(0, prefix.size(), prefix) != 0 || file.size() < 8 ||
          file.compare(file.size() - 8, 8, L".pak.xen") != 0) {
        continue;
      }
      if (file == own) {
        c.fits = true;
      }
      std::ifstream in(entry.path(), std::ios::binary);
      c.manifests.emplace_back(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
  }
  // A package the game reads loads its manifests, which may name others (a later pack's manifest lists the earlier ones).
  for (bool changed = true; changed;) {
    changed = false;
    for (Checked& c : checked) {
      if (c.fits || !rule.manifest_prefix) continue;
      for (size_t j = 0; j < checked.size(); ++j) {
        const Checked& other = checked[j];
        if (!other.fits || !on[j]) continue;
        for (const auto& manifest : other.manifests) {
          if (ContainsKey(manifest, c.key)) {
            c.fits = true;
            changed = true;
            break;
          }
        }
        if (c.fits) break;
      }
    }
  }
  for (size_t i = 0; i < checked.size(); ++i) {
    result[i] = checked[i].fits;
  }
  return result;
}

}  // namespace

std::vector<uint8_t> PackageTitleThumbnail(const std::filesystem::path& path) {
  std::vector<uint8_t> header(kTitleThumbnailOffset + kThumbnailBytes);
  {
    std::ifstream file(path, std::ios::binary);
    if (!file.read(reinterpret_cast<char*>(header.data()), std::streamsize(header.size())) ||
        !IsPackage(header.data())) {
      return {};
    }
  }
  static constexpr uint8_t kPng[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  uint32_t thumbnail = ReadU32(&header[kThumbnailSizesOffset]);
  uint32_t title_thumbnail = ReadU32(&header[kThumbnailSizesOffset + 4]);
  for (auto [offset, size] : {std::pair<size_t, uint32_t>{kTitleThumbnailOffset, title_thumbnail},
                              std::pair<size_t, uint32_t>{kThumbnailOffset, thumbnail}}) {
    if (size > sizeof(kPng) && size <= kThumbnailBytes && !std::memcmp(&header[offset], kPng, sizeof(kPng))) {
      return std::vector<uint8_t>(header.begin() + ptrdiff_t(offset), header.begin() + ptrdiff_t(offset + size));
    }
  }
  return {};
}

std::filesystem::path GameUserDataFolder(const std::filesystem::path& exe_dir) {
  std::filesystem::path portable = exe_dir / L"userdata";
  std::error_code ec;
  std::filesystem::create_directories(portable, ec);
  std::filesystem::path probe = portable / L".write_test";
  bool writable = false;
  {
    std::ofstream file(probe, std::ios::binary);
    writable = bool(file);
  }
  std::filesystem::remove(probe, ec);
  if (writable) {
    return portable;
  }
  PWSTR documents = nullptr;
  std::filesystem::path result = portable;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, nullptr, &documents))) {
    result = std::filesystem::path(documents) / game::kDocumentsFolder;
  }
  CoTaskMemFree(documents);
  return result;
}

std::filesystem::path DlcFolder(const std::filesystem::path& exe_dir) {
  wchar_t title[16];
  std::swprintf(title, std::size(title), L"%08X", game::kTitleId);
  return GameUserDataFolder(exe_dir) / L"0000000000000000" / title / L"00000002";
}

std::filesystem::path DisabledDlcFolder(const std::filesystem::path& exe_dir) {
  return DlcFolder(exe_dir).parent_path() / L"Disabled DLC";
}

std::vector<DlcPackage> FindDlcPackages(const std::filesystem::path& path, const std::filesystem::path& exe_dir) {
  std::vector<DlcPackage> result;
  std::error_code ec;
  DlcPackage package;
  if (std::filesystem::is_regular_file(path, ec)) {
    if (ReadPackage(path, package)) {
      result.push_back(package);
    }
  } else {
    int entries = 0;
    for (auto it = std::filesystem::recursive_directory_iterator(
             path, std::filesystem::directory_options::skip_permission_denied, ec);
         !ec && it != std::filesystem::recursive_directory_iterator() && entries < kMaxEntries; it.increment(ec)) {
      ++entries;
      if (it.depth() >= kMaxDepth) {
        it.disable_recursion_pending();
      }
      if (ReadPackage(it->path(), package)) {
        result.push_back(package);
        package = {};
      }
    }
  }
  std::filesystem::path installed = DlcFolder(exe_dir);
  std::filesystem::path disabled = DisabledDlcFolder(exe_dir);
  for (DlcPackage& p : result) {
    p.installed = std::filesystem::is_directory(installed / p.path.filename(), ec) ||
                  std::filesystem::is_directory(disabled / p.path.filename(), ec);
  }
  std::sort(result.begin(), result.end(),
            [](const DlcPackage& a, const DlcPackage& b) { return a.display_name < b.display_name; });
  return result;
}

std::vector<InstalledDlc> ListInstalledDlc(const std::filesystem::path& exe_dir) {
  std::vector<InstalledDlc> result;
  std::vector<std::filesystem::path> paths;
  std::vector<std::wstring> names;
  std::vector<bool> on;
  for (bool folder_on : {true, false}) {
    std::error_code ec;
    for (const auto& entry :
         std::filesystem::directory_iterator(folder_on ? DlcFolder(exe_dir) : DisabledDlcFolder(exe_dir), ec)) {
      std::error_code dir_ec;
      if (!entry.is_directory(dir_ec)) {
        continue;
      }
      InstalledDlc dlc;
      std::wstring folder = entry.path().filename().wstring();
      dlc.folder = Utf8(folder);
      dlc.display_name = dlc.folder;
      dlc.on = folder_on;
      // The header: an XCONTENT_DATA (device ID, content type, then the display name, 128 UTF-16 characters).
      std::array<uint8_t, 8 + 256> header{};
      std::ifstream file(HeaderPath(exe_dir, folder), std::ios::binary);
      if (file.read(reinterpret_cast<char*>(header.data()), header.size())) {
        std::string name = Utf16BeToUtf8(&header[8], 256);
        if (!name.empty()) {
          dlc.display_name = name;
        }
      }
      dlc.bytes = FolderBytes(entry.path());
      result.push_back(std::move(dlc));
      paths.push_back(entry.path());
      names.push_back(GameDisplayName(exe_dir, folder));
      on.push_back(folder_on);
    }
  }
  // A package on that the game does not read goes off, unless the player turned it on. Only the manifests of packages
  // the game reads count, so turning one it does not read off changes no other's answer.
  std::vector<bool> fits = CheckPackages(paths, names, on);
  std::set<std::string> turned_on = ReadTurnedOn(exe_dir);
  for (size_t i = 0; i < result.size(); ++i) {
    InstalledDlc& dlc = result[i];
    dlc.fits = fits[i];
    if (dlc.on && !dlc.fits && !turned_on.count(dlc.folder) && SetDlcOn(exe_dir, dlc.folder, false)) {
      dlc.on = false;
      dlc.turned_off_now = true;
    }
  }
  std::sort(result.begin(), result.end(),
            [](const InstalledDlc& a, const InstalledDlc& b) { return a.display_name < b.display_name; });
  return result;
}

bool SetDlcOn(const std::filesystem::path& exe_dir, const std::string& folder, bool on) {
  std::wstring name = WideFromUtf8(folder);
  if (!IsPlainName(name)) {
    return false;
  }
  std::filesystem::path from = (on ? DisabledDlcFolder(exe_dir) : DlcFolder(exe_dir)) / name;
  std::filesystem::path to = (on ? DlcFolder(exe_dir) : DisabledDlcFolder(exe_dir)) / name;
  std::error_code ec;
  std::filesystem::create_directories(to.parent_path(), ec);
  // On the same volume: a rename, at once. It fails while a file of it is open (the game running).
  std::filesystem::rename(from, to, ec);
  if (ec) {
    return false;
  }
  std::set<std::string> turned_on = ReadTurnedOn(exe_dir);
  if (on ? turned_on.insert(folder).second : turned_on.erase(folder) != 0) {
    WriteTurnedOn(exe_dir, turned_on);
  }
  return true;
}

bool RemoveDlc(const std::filesystem::path& exe_dir, const std::string& folder) {
  std::wstring name = WideFromUtf8(folder);
  // Only a folder directly in the DLC folders.
  if (!IsPlainName(name)) {
    return false;
  }
  std::error_code ec;
  bool removed = false;
  for (const std::filesystem::path& parent : {DlcFolder(exe_dir), DisabledDlcFolder(exe_dir)}) {
    if (std::filesystem::is_directory(parent / name, ec)) {
      std::filesystem::remove_all(parent / name, ec);
      removed = !ec;
    }
  }
  std::filesystem::remove(HeaderPath(exe_dir, name), ec);
  std::set<std::string> turned_on = ReadTurnedOn(exe_dir);
  if (turned_on.erase(folder)) {
    WriteTurnedOn(exe_dir, turned_on);
  }
  return removed;
}

DlcInstall::~DlcInstall() {
  if (worker_.joinable()) {
    worker_.join();
  }
}

void DlcInstall::Start(const std::filesystem::path& exe_dir, std::vector<DlcPackage> packages) {
  if (worker_.joinable()) {
    worker_.join();
  }
  {
    std::lock_guard lock(mutex_);
    progress_ = {};
    progress_.state = State::kRunning;
    progress_.count = packages.size();
    for (const DlcPackage& p : packages) {
      progress_.total += p.content_size;
    }
  }
  worker_ = std::thread(&DlcInstall::Run, this, exe_dir, std::move(packages));
}

DlcInstall::Progress DlcInstall::Get() const {
  std::lock_guard lock(mutex_);
  return progress_;
}

void DlcInstall::Reset() {
  std::lock_guard lock(mutex_);
  if (progress_.state == State::kDone) {
    progress_ = {};
  }
}

void DlcInstall::Run(std::filesystem::path exe_dir, std::vector<DlcPackage> packages) {
  std::filesystem::path game = exe_dir / game::kRecompExecutable;
  std::filesystem::path dlc_folder = DlcFolder(exe_dir);
  uint64_t base = 0;
  for (size_t i = 0; i < packages.size(); ++i) {
    const DlcPackage& package = packages[i];
    {
      std::lock_guard lock(mutex_);
      progress_.index = i;
      progress_.current = package.display_name;
      progress_.done = base;
    }
    std::wstring folder = package.path.filename().wstring();
    std::filesystem::path header = HeaderPath(exe_dir, folder);
    std::error_code ec;
    std::filesystem::remove(header, ec);  // written last: its presence afterwards is the success
    std::wstring command_line =
        L"\"" + game.wstring() + L"\" \"--install_content=" + package.path.wstring() + L"\"";
    STARTUPINFOW startup = {sizeof(startup)};
    PROCESS_INFORMATION process = {};
    bool ok = false;
    if (CreateProcessW(game.c_str(), command_line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                       exe_dir.c_str(), &startup, &process)) {
      CloseHandle(process.hThread);
      // The game extracts the package: its folder grows towards the package's content size. A package of a few GB
      // takes a minute or two on a hard disk; one that has not finished in 30 minutes has failed.
      auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(30);
      while (WaitForSingleObject(process.hProcess, 250) == WAIT_TIMEOUT) {
        if (std::chrono::steady_clock::now() > deadline) {
          TerminateProcess(process.hProcess, 1);
          WaitForSingleObject(process.hProcess, 5000);
          break;
        }
        uint64_t bytes = std::min(FolderBytes(dlc_folder / folder), package.content_size);
        std::lock_guard lock(mutex_);
        progress_.done = base + bytes;
      }
      DWORD exit_code = 1;
      GetExitCodeProcess(process.hProcess, &exit_code);
      CloseHandle(process.hProcess);
      ok = exit_code == 0 && std::filesystem::exists(header, ec);
    }
    base += package.content_size;
    std::lock_guard lock(mutex_);
    progress_.done = base;
    if (ok) {
      ++progress_.installed;
    } else {
      progress_.failed.push_back(package.display_name);
    }
  }
  std::lock_guard lock(mutex_);
  progress_.state = State::kDone;
}

}  // namespace launcher
