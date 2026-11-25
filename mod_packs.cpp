// The mod packs (mod_packs.h).

#include "mod_packs.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>

#include "dlc.h"
#include "game.h"
#include "install.h"
#include "zip.h"

namespace launcher {
namespace {

namespace fs = std::filesystem;

constexpr wchar_t kModsFolder[] = L"Mods";
constexpr wchar_t kEnabledFile[] = L"enabled.txt";
constexpr wchar_t kRecordFile[] = L"prepared.txt";
constexpr char kPatchMagic[8] = {'R', 'C', 'M', 'P', 'D', 'L', 'T', '1'};
constexpr size_t kPatchHeaderBytes = 88;
constexpr size_t kChunk = 1024 * 1024;

// UTF-8 and the paths' UTF-16 both ways through Windows, which never throws (std::filesystem's conversions do, on text
// that is not valid).
std::string Utf8(const std::wstring& wide) {
  if (wide.empty()) return {};
  int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0, nullptr, nullptr);
  std::string text(size_t(std::max(length, 0)), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), text.data(), length, nullptr, nullptr);
  return text;
}

std::string Utf8(const fs::path& path) { return Utf8(path.native()); }

fs::path FromUtf8(const std::string& text) {
  if (text.empty()) return {};
  int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
  std::wstring wide(size_t(std::max(length, 0)), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), wide.data(), length);
  return fs::path(wide);
}

bool ValidUtf8(const std::string& text) {
  for (size_t i = 0; i < text.size();) {
    const auto c = static_cast<unsigned char>(text[i]);
    const size_t length = c < 0x80                ? 1
                          : (c >= 0xC2 && c < 0xE0) ? 2
                          : (c >= 0xE0 && c < 0xF0) ? 3
                          : (c >= 0xF0 && c < 0xF5) ? 4
                                                    : 0;
    if (length == 0 || i + length > text.size()) return false;
    for (size_t k = 1; k < length; ++k) {
      if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) return false;
    }
    i += length;
  }
  return true;
}

std::string Lower(std::string text) {
  for (char& c : text) {
    if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
  }
  return text;
}

std::string Trim(const std::string& text) {
  size_t begin = text.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) return {};
  size_t end = text.find_last_not_of(" \t\r\n");
  return text.substr(begin, end - begin + 1);
}

uint64_t U64(const uint8_t* p) {
  uint64_t value = 0;
  for (int i = 7; i >= 0; --i) value = (value << 8) | p[i];
  return value;
}

// A sum of sizes from the patches' headers, which a damaged one could make wrap.
uint64_t Saturated(uint64_t a, uint64_t b) {
  return a > std::numeric_limits<uint64_t>::max() - b ? std::numeric_limits<uint64_t>::max() : a + b;
}

std::string Hex(const uint8_t* bytes, size_t count) {
  static const char kHex[] = "0123456789abcdef";
  std::string text;
  for (size_t i = 0; i < count; ++i) {
    text += kHex[bytes[i] >> 4];
    text += kHex[bytes[i] & 15];
  }
  return text;
}

int64_t FileTime(const fs::path& path) {
  std::error_code ec;
  return int64_t(fs::last_write_time(path, ec).time_since_epoch().count());
}

// The order of the list and of enabled.txt: by folder name, letters in either case together.
bool FolderBefore(const std::string& a, const std::string& b) {
  std::string la = Lower(a), lb = Lower(b);
  return la != lb ? la < lb : a < b;
}

// An ApplyModPatch temporary: <name>.part<digits>, left by a preparation that could not end (the launcher stopped).
bool IsPartFile(const std::wstring& name) {
  const size_t at = name.rfind(L".part");
  if (at == std::wstring::npos || at + 5 == name.size()) return false;
  return std::all_of(name.begin() + ptrdiff_t(at + 5), name.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

// SHA-256 with Windows' CNG, fed in pieces.
class Sha256 {
 public:
  Sha256() {
    if (BCryptOpenAlgorithmProvider(&algorithm_, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
        BCryptCreateHash(algorithm_, &hash_, nullptr, 0, nullptr, 0, 0) == 0) {
      ok_ = true;
    }
  }
  ~Sha256() {
    if (hash_) BCryptDestroyHash(hash_);
    if (algorithm_) BCryptCloseAlgorithmProvider(algorithm_, 0);
  }
  void Add(const void* data, size_t size) {
    if (ok_ && size && BCryptHashData(hash_, PUCHAR(data), ULONG(size), 0) != 0) ok_ = false;
  }
  // The digest, or false when CNG failed.
  bool Finish(uint8_t digest[32]) { return ok_ && BCryptFinishHash(hash_, digest, 32, 0) == 0; }

 private:
  BCRYPT_ALG_HANDLE algorithm_ = nullptr;
  BCRYPT_HASH_HANDLE hash_ = nullptr;
  bool ok_ = false;
};

// mod.toml's flat `key = value` lines (a string in double quotes, with \" and \\; or a bare value up to a space or a
// #). Tables, arrays and the keys after them are not read.
using ModToml = std::map<std::string, std::string>;

ModToml ParseModToml(const std::string& text) {
  ModToml values;
  std::istringstream lines(text);
  std::string line;
  while (std::getline(lines, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#') continue;
    if (line[0] == '[') break;
    size_t equals = line.find('=');
    if (equals == std::string::npos) continue;
    std::string key = Trim(line.substr(0, equals));
    std::string raw = Trim(line.substr(equals + 1));
    std::string value;
    if (!raw.empty() && raw[0] == '"') {
      for (size_t i = 1; i < raw.size() && raw[i] != '"'; ++i) {
        if (raw[i] == '\\' && i + 1 < raw.size()) {
          ++i;
          value += raw[i] == 'n' ? '\n' : raw[i] == 't' ? '\t' : raw[i];
        } else {
          value += raw[i];
        }
      }
    } else {
      value = raw.substr(0, raw.find_first_of(" \t#"));
    }
    values[key] = value;
  }
  return values;
}

// A pack's mod.toml, or none. `locked`: it is there but could not be opened now (another program has it).
std::optional<ModToml> ReadModToml(const fs::path& path, bool* locked = nullptr) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    std::error_code ec;
    if (locked) *locked = fs::is_regular_file(path, ec);
    return std::nullopt;
  }
  std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
  if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);  // a byte order mark
  return ParseModToml(text);
}

std::string Value(const ModToml& toml, const char* key) {
  auto it = toml.find(key);
  return it != toml.end() ? it->second : std::string();
}

// A name Windows takes for one part of a path: no character it refuses, not "." or "..", not a device's name (CON,
// PRN, AUX, NUL, COM1-9, LPT1-9, with an extension too), not ending in a dot or a space.
bool SafeComponent(const std::string& part) {
  if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ') return false;
  for (char c : part) {
    if (static_cast<unsigned char>(c) < 0x20 || std::strchr("<>:\"/\\|?*", c)) return false;
  }
  std::string device = part.substr(0, part.find('.'));
  while (!device.empty() && device.back() == ' ') device.pop_back();
  device = Lower(device);
  static const char* const kDevices[] = {"con", "prn", "aux", "nul", "conin$", "conout$"};
  for (const char* name : kDevices) {
    if (device == name) return false;
  }
  return !(device.size() == 4 && (device.rfind("com", 0) == 0 || device.rfind("lpt", 0) == 0) && device[3] >= '1' &&
           device[3] <= '9');
}

// An id that can name a folder of Mods\: letters, digits, '-', '_' and '.', at most 64, not starting with a '.' (the
// launcher's own folders there do), a name Windows takes, and not one of the launcher's own files.
bool FolderId(const std::string& id) {
  if (id.empty() || id.size() > 64 || id[0] == '.' || !SafeComponent(id)) return false;
  const std::string lower = Lower(id);
  if (lower == "enabled.txt" || lower == "enabled.txt.tmp") return false;
  return std::all_of(id.begin(), id.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
           c == '.';
  });
}

// A path of a zip that is safe to write under a folder: valid UTF-8, relative, each part a name Windows takes.
bool SafeRelative(const std::string& name) {
  if (name.empty() || name[0] == '/' || !ValidUtf8(name)) return false;
  size_t start = 0;
  while (start <= name.size()) {
    size_t end = name.find('/', start);
    if (!SafeComponent(name.substr(start, end == std::string::npos ? std::string::npos : end - start))) return false;
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return true;
}

// A zip entry's name for a message: what is not valid UTF-8 or printable shown as '?'.
std::string Printable(const std::string& name) {
  if (ValidUtf8(name)) {
    std::string text = name;
    for (char& c : text) {
      if (static_cast<unsigned char>(c) < 0x20) c = '?';
    }
    return text;
  }
  std::string text;
  for (char c : name) text += static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) >= 0x80 ? '?' : c;
  return text;
}

// The files under `folder` (not an ApplyModPatch temporary), as paths relative to it with '/' separators.
std::vector<std::string> ListFiles(const fs::path& folder) {
  std::vector<std::string> files;
  std::error_code ec;
  if (!fs::is_directory(folder, ec)) return files;
  for (fs::recursive_directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec) || IsPartFile(it->path().filename().native())) continue;
    files.push_back(Utf8(it->path().lexically_relative(folder).generic_wstring()));
  }
  return files;
}

// The paths of patches\ (without .delta) a pack has.
std::vector<std::string> ListPatches(const fs::path& pack) {
  std::vector<std::string> patches;
  for (const std::string& file : ListFiles(pack / L"patches")) {
    if (file.size() > 6 && Lower(file.substr(file.size() - 6)) == ".delta") {
      patches.push_back(file.substr(0, file.size() - 6));
    }
  }
  std::sort(patches.begin(), patches.end());
  return patches;
}

fs::path PatchPath(const fs::path& pack, const std::string& relative) {
  return pack / L"patches" / FromUtf8(relative + ".delta");
}

struct PatchHeader {
  uint64_t base_size = 0;
  uint64_t result_size = 0;
};

bool ReadPatchHeader(const fs::path& path, PatchHeader* header) {
  uint8_t bytes[kPatchHeaderBytes];
  std::ifstream file(path, std::ios::binary);
  if (!file.read(reinterpret_cast<char*>(bytes), sizeof(bytes)) || std::memcmp(bytes, kPatchMagic, 8) != 0) {
    return false;
  }
  header->base_size = U64(bytes + 8);
  header->result_size = U64(bytes + 48);
  return true;
}

// prepared.txt: what the launcher prepared from patches\ (`ok` lines), and the bases found not to be the patches'
// (`differs` lines), each with the patch's size and time it was found with; the path last (it may hold spaces).
struct Record {
  struct Prepared {
    uint64_t patch_size = 0;
    int64_t patch_time = 0;
    uint64_t result_size = 0;
    std::string sha256;
  };
  struct Differs {
    uint64_t patch_size = 0;
    int64_t patch_time = 0;
    uint64_t base_size = 0;
    int64_t base_time = 0;
  };
  std::map<std::string, Prepared> prepared;  // by lower-case path
  std::map<std::string, Differs> differs;
  std::map<std::string, std::string> paths;  // each lower-case path as written
};

Record ReadRecord(const fs::path& pack) {
  Record record;
  std::ifstream file(pack / kRecordFile, std::ios::binary);
  std::string line;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream fields(line);
    std::string kind;
    fields >> kind;
    std::string path;
    if (kind == "ok") {
      Record::Prepared p;
      fields >> p.patch_size >> p.patch_time >> p.result_size >> p.sha256;
      std::getline(fields >> std::ws, path);
      if (fields.fail() && path.empty()) continue;
      record.prepared[Lower(path)] = p;
    } else if (kind == "differs") {
      Record::Differs d;
      fields >> d.patch_size >> d.patch_time >> d.base_size >> d.base_time;
      std::getline(fields >> std::ws, path);
      if (fields.fail() && path.empty()) continue;
      record.differs[Lower(path)] = d;
    } else {
      continue;
    }
    record.paths[Lower(path)] = path;
  }
  return record;
}

// Written to <file>.tmp, closed and checked, then put in place in one step; a failure leaves no temporary.
bool WriteWhole(const fs::path& path, const std::string& content) {
  fs::path temporary = path;
  temporary += L".tmp";
  std::error_code ec;
  {
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (file) {
      file.write(content.data(), std::streamsize(content.size()));
      file.close();
    }
    if (!file) {
      fs::remove(temporary, ec);
      return false;
    }
  }
  if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    fs::remove(temporary, ec);
    return false;
  }
  return true;
}

bool WriteRecord(const fs::path& pack, const Record& record) {
  std::string content =
      "# The files the launcher made from this pack's patches (Mods > Packs). Deleted, they are made again.\n";
  for (const auto& [key, p] : record.prepared) {
    content += "ok " + std::to_string(p.patch_size) + " " + std::to_string(p.patch_time) + " " +
               std::to_string(p.result_size) + " " + p.sha256 + " " + record.paths.at(key) + "\n";
  }
  for (const auto& [key, d] : record.differs) {
    content += "differs " + std::to_string(d.patch_size) + " " + std::to_string(d.patch_time) + " " +
               std::to_string(d.base_size) + " " + std::to_string(d.base_time) + " " + record.paths.at(key) + "\n";
  }
  return WriteWhole(pack / kRecordFile, content);
}

// Whether a file or folder is a link (a symbolic link, a junction: any reparse point), not followed.
bool IsLink(const fs::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

// Whether writing `out` stays in the pack's folder: neither it nor a folder between it and the pack's is a link, and
// its canonical path is under the pack's.
bool WritesInside(const fs::path& pack, const fs::path& out) {
  for (fs::path p = out; p != pack && p.has_relative_path(); p = p.parent_path()) {
    if (IsLink(p)) return false;
  }
  std::error_code ec;
  const fs::path canonical_pack = fs::weakly_canonical(pack, ec);
  if (ec) return false;
  const fs::path canonical_out = fs::weakly_canonical(out, ec);
  if (ec) return false;
  const fs::path relative = canonical_out.lexically_relative(canonical_pack);
  return !relative.empty() && *relative.begin() != L"..";
}

// What a pack's patch needs: nothing (its result in files\ is the record's), preparing, or nothing it can do (its base
// is not the player's file, by its size, or by the record of a preparation that found it so).
enum class PatchNeed { kPrepared, kPrepare, kOtherFiles };

PatchNeed NeedOf(const fs::path& pack, const fs::path& root, const std::string& relative, const PatchHeader& header,
                 const Record& record) {
  std::error_code ec;
  const fs::path patch = PatchPath(pack, relative);
  const uint64_t patch_size = fs::file_size(patch, ec);
  const int64_t patch_time = FileTime(patch);
  const std::string key = Lower(relative);
  if (auto it = record.prepared.find(key); it != record.prepared.end()) {
    const Record::Prepared& p = it->second;
    const uint64_t size = fs::file_size(pack / L"files" / FromUtf8(relative), ec);
    if (!ec && p.patch_size == patch_size && p.patch_time == patch_time && p.result_size == header.result_size &&
        size == p.result_size) {
      return PatchNeed::kPrepared;
    }
  }
  if (root.empty()) return PatchNeed::kPrepare;  // no game files: the preparation says so
  const fs::path base = root / FromUtf8(relative);
  const uint64_t base_size = fs::file_size(base, ec);
  if (ec || base_size != header.base_size) return PatchNeed::kOtherFiles;
  if (auto it = record.differs.find(key); it != record.differs.end()) {
    const Record::Differs& d = it->second;
    if (d.patch_size == patch_size && d.patch_time == patch_time && d.base_size == base_size &&
        d.base_time == FileTime(base)) {
      return PatchNeed::kOtherFiles;
    }
  }
  return PatchNeed::kPrepare;
}

ModPack ReadPack(const fs::path& dir, const fs::path& root) {
  ModPack pack;
  pack.folder = Utf8(dir.filename());
  pack.name = pack.folder;
  // enabled.txt skips its '#' lines and trims its lines: a folder it could not name is never on.
  if (pack.folder.empty() || pack.folder[0] == '#' || pack.folder.front() == ' ' || pack.folder.back() == ' ' ||
      pack.folder.back() == '.') {
    pack.broken = ModPack::Broken::kFolderName;
    return pack;
  }
  bool locked = false;
  std::optional<ModToml> toml = ReadModToml(dir / L"mod.toml", &locked);
  if (!toml) {
    if (locked) {
      pack.state = ModPack::State::kUnavailable;
    } else {
      pack.broken = ModPack::Broken::kModToml;
    }
    return pack;
  }
  pack.id = Value(*toml, "id");
  if (std::string name = Value(*toml, "name"); !name.empty()) pack.name = name;
  pack.version = Value(*toml, "version");
  pack.author = Value(*toml, "author");
  pack.description = Value(*toml, "description");
  for (const auto& [key, value] : *toml) {
    if (key.rfind("name_", 0) == 0 && !value.empty()) pack.names[key.substr(5)] = value;
    if (key.rfind("description_", 0) == 0 && !value.empty()) pack.descriptions[key.substr(12)] = value;
  }
  const std::string format = Value(*toml, "format");
  const std::string title = Value(*toml, "title_id");
  if (format.empty() || pack.id.empty() || title.empty()) {
    pack.broken = ModPack::Broken::kModToml;
    return pack;
  }
  if (format != "1") {
    pack.broken = ModPack::Broken::kFormat;
    pack.detail = format;
    return pack;
  }
  char* end = nullptr;
  const unsigned long title_id = std::strtoul(title.c_str(), &end, 16);
  if (!end || *end || title_id != game::kTitleId) {
    pack.state = ModPack::State::kOtherGame;
    pack.detail = title;
    return pack;
  }
  const std::vector<std::string> files = ListFiles(dir / L"files");
  const std::vector<std::string> patches = ListPatches(dir);
  if (files.empty() && patches.empty()) {
    pack.broken = ModPack::Broken::kEmpty;
    return pack;
  }
  for (const std::string& file : files) pack.files.push_back(Lower(file));
  for (const std::string& patch : patches) pack.files.push_back(Lower(patch));
  std::sort(pack.files.begin(), pack.files.end());
  pack.files.erase(std::unique(pack.files.begin(), pack.files.end()), pack.files.end());

  // Its patches: a damaged one makes it broken; the first whose base is not the player's file, made for other files.
  const Record record = ReadRecord(dir);
  pack.state = ModPack::State::kReady;
  for (const std::string& relative : patches) {
    PatchHeader header;
    const bool read = ReadPatchHeader(PatchPath(dir, relative), &header);
    if (!read || header.result_size > kMaxModPatchResult) {
      pack.state = ModPack::State::kBroken;
      pack.broken = read ? ModPack::Broken::kTooLarge : ModPack::Broken::kPatch;
      pack.detail = relative;
      return pack;
    }
    switch (NeedOf(dir, root, relative, header, record)) {
      case PatchNeed::kPrepared:
        break;
      case PatchNeed::kPrepare:
        pack.prepare_bytes = Saturated(pack.prepare_bytes, header.result_size);
        if (pack.state == ModPack::State::kReady) pack.state = ModPack::State::kNeedsPreparing;
        break;
      case PatchNeed::kOtherFiles:
        if (pack.state != ModPack::State::kOtherFiles) {
          pack.state = ModPack::State::kOtherFiles;
          pack.detail = relative;
        }
        break;
    }
  }
  return pack;
}

std::vector<std::string> ReadEnabled(const fs::path& mods) {
  std::vector<std::string> folders;
  std::ifstream file(mods / kEnabledFile, std::ios::binary);
  std::string line;
  while (std::getline(file, line)) {
    line = Trim(line);
    if (line.rfind("\xEF\xBB\xBF", 0) == 0) line.erase(0, 3);
    if (!line.empty() && line[0] != '#') folders.push_back(line);
  }
  return folders;
}

// enabled.txt written whole: the packs on, in the list's order (by folder name).
bool WriteEnabled(const ModPacks& packs) {
  std::string content =
      "# The mod packs on (the launcher's Mods > Packs): a folder of this one a line, in load order, a later one's "
      "file winning. Read by the game when it starts.\n";
  for (const ModPack& pack : packs.packs) {
    if (pack.on) content += pack.folder + "\n";
  }
  std::error_code ec;
  fs::create_directories(packs.folder, ec);
  return WriteWhole(packs.folder / kEnabledFile, content);
}

// Where a pack's mod.toml is: `root` itself, or its one subfolder that has one (a zip's top folder).
std::optional<fs::path> PackRoot(const fs::path& root) {
  std::error_code ec;
  if (fs::is_regular_file(root / L"mod.toml", ec)) return root;
  std::optional<fs::path> found;
  for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->is_directory(ec) && fs::is_regular_file(it->path() / L"mod.toml", ec)) {
      if (found) return std::nullopt;  // two: which one is meant is not known
      found = it->path();
    }
  }
  return found;
}

// AddModPack's work; `staging` is set once its temporary folder exists (removed by the caller when this throws).
ModPackAdd AddPack(const fs::path& exe_dir, const fs::path& source, bool replace, fs::path& staging) {
  ModPackAdd add;
  const fs::path mods = ModsFolder(exe_dir);
  std::error_code ec;
  fs::path from = source;
  if (Lower(Utf8(from.filename())) == "mod.toml") from = from.parent_path();  // the file dialog's mod.toml: its folder

  // Its mod.toml: in a zip, at its top or in its one top folder; in a folder, the same.
  zip::Archive archive;
  std::string prefix;  // the zip's folder of the pack, with its '/'
  fs::path folder;     // a folder's pack
  ModToml toml;
  const std::string extension = Lower(Utf8(from.extension()));
  const bool is_file = fs::is_regular_file(from, ec);
  const bool is_zip = is_file && extension == ".zip";
  if (is_file && (extension == ".7z" || extension == ".rar")) {
    add.result = ModPackAdd::Result::kNotAZip;
    return add;
  }
  if (is_zip) {
    if (!archive.Open(from, &add.error)) return add;
    const zip::Entry* found = nullptr;
    int nested = 0;
    for (const zip::Entry& entry : archive.entries()) {
      const std::string name = Lower(entry.name);
      if (name == "mod.toml") {
        found = &entry;
        prefix.clear();
        nested = 0;
        break;
      }
      const size_t slash = name.find('/');
      if (slash != std::string::npos && name.substr(slash + 1) == "mod.toml") {
        found = &entry;
        prefix = entry.name.substr(0, slash + 1);
        ++nested;
      }
    }
    if (!found || nested > 1) {
      add.result = ModPackAdd::Result::kNotAPack;
      return add;
    }
    std::vector<uint8_t> data;
    if (!archive.Extract(*found, data, &add.error)) return add;
    std::string text(data.begin(), data.end());
    if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);
    toml = ParseModToml(text);
  } else if (fs::is_directory(from, ec)) {
    std::optional<fs::path> pack_root = PackRoot(from);
    std::optional<ModToml> read = pack_root ? ReadModToml(*pack_root / L"mod.toml") : std::nullopt;
    if (!read) {
      add.result = ModPackAdd::Result::kNotAPack;
      return add;
    }
    folder = *pack_root;
    toml = std::move(*read);
  } else {
    add.result = ModPackAdd::Result::kNotAPack;
    return add;
  }
  add.id = Value(toml, "id");
  add.name = Value(toml, "name").empty() ? add.id : Value(toml, "name");
  add.version = Value(toml, "version");
  for (const auto& [key, value] : toml) {
    if (key.rfind("name_", 0) == 0 && !value.empty()) add.names[key.substr(5)] = value;
  }
  if (!FolderId(add.id)) {
    add.result = ModPackAdd::Result::kNotAPack;
    return add;
  }
  // Every name of the zip checked before anything is written.
  if (is_zip) {
    for (const zip::Entry& entry : archive.entries()) {
      if (entry.directory() || entry.name.rfind(prefix, 0) != 0 || entry.name.size() == prefix.size()) continue;
      if (!SafeRelative(entry.name.substr(prefix.size()))) {
        add.error = "a name in the archive Windows cannot take: " + Printable(entry.name);
        return add;
      }
    }
  }

  // Mods\<id>: the same pack is replaced once the player says so; another pack (or a folder that is none) stays.
  const fs::path target = mods / FromUtf8(add.id);
  const bool exists = fs::exists(target, ec);
  if (exists) {
    std::optional<ModToml> existing = ReadModToml(target / L"mod.toml");
    if (!existing || Value(*existing, "id") != add.id) {
      add.result = ModPackAdd::Result::kOtherPack;
      return add;
    }
    if (!folder.empty() && fs::equivalent(folder, target, ec)) {
      add.result = ModPackAdd::Result::kAdded;  // that folder itself: nothing to copy
      return add;
    }
    if (!replace) {
      add.existing_version = Value(*existing, "version");
      add.result = ModPackAdd::Result::kAsk;
      return add;
    }
  }

  // Into Mods\.adding <id> first, then in place in one step: a failure leaves no half pack.
  staging = mods / FromUtf8(".adding " + add.id);
  fs::create_directories(mods, ec);
  fs::remove_all(staging, ec);
  fs::create_directories(staging, ec);
  auto fail = [&](const std::string& error) {
    fs::remove_all(staging, ec);
    add.result = ModPackAdd::Result::kFailed;
    add.error = error;
    return add;
  };
  if (is_zip) {
    std::vector<uint8_t> data;
    std::string error;
    for (const zip::Entry& entry : archive.entries()) {
      if (entry.directory() || entry.name.rfind(prefix, 0) != 0 || entry.name.size() == prefix.size()) continue;
      if (!archive.Extract(entry, data, &error)) return fail(error);
      const fs::path file = staging / FromUtf8(entry.name.substr(prefix.size()));
      fs::create_directories(file.parent_path(), ec);
      std::ofstream out(file, std::ios::binary | std::ios::trunc);
      if (out) {
        out.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
        out.close();
      }
      if (!out) return fail("cannot write " + Utf8(file));
    }
  } else {
    fs::copy(folder, staging, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec) return fail(ec.message());
  }
  if (exists) {
    const fs::path replaced = mods / FromUtf8(".replaced " + add.id);
    fs::remove_all(replaced, ec);
    if (!MoveFileExW(target.c_str(), replaced.c_str(), 0)) {
      return fail("cannot replace " + Utf8(target) + " (" + std::to_string(GetLastError()) + ")");
    }
    if (!MoveFileExW(staging.c_str(), target.c_str(), 0)) {
      const DWORD code = GetLastError();
      MoveFileExW(replaced.c_str(), target.c_str(), 0);
      return fail("cannot write " + Utf8(target) + " (" + std::to_string(code) + ")");
    }
    fs::remove_all(replaced, ec);
    add.result = ModPackAdd::Result::kReplaced;
  } else {
    if (!MoveFileExW(staging.c_str(), target.c_str(), 0)) {
      return fail("cannot write " + Utf8(target) + " (" + std::to_string(GetLastError()) + ")");
    }
    add.result = ModPackAdd::Result::kAdded;
  }
  return add;
}

}  // namespace

size_t ModPacks::On() const {
  return size_t(std::count_if(packs.begin(), packs.end(), [](const ModPack& pack) { return pack.on; }));
}

fs::path ModsFolder(const fs::path& exe_dir) { return GameUserDataFolder(exe_dir) / kModsFolder; }

ModPacks ScanModPacks(const fs::path& exe_dir) {
  ModPacks result;
  result.folder = ModsFolder(exe_dir);
  const fs::path root = GameDataRoot(exe_dir);
  std::error_code ec;
  for (fs::directory_iterator it(result.folder, ec), end; !ec && it != end; it.increment(ec)) {
    // The launcher's own folders (a pack being added or replaced) start with a '.'.
    if (!it->is_directory(ec) || it->path().filename().native().starts_with(L".")) continue;
    try {
      result.packs.push_back(ReadPack(it->path(), root));
    } catch (...) {
      // Not read now (out of memory, a folder that went away meanwhile): as a mod.toml another program holds.
      ModPack pack;
      pack.folder = pack.name = Utf8(it->path().filename());
      pack.state = ModPack::State::kUnavailable;
      result.packs.push_back(std::move(pack));
    }
  }
  std::sort(result.packs.begin(), result.packs.end(),
            [](const ModPack& a, const ModPack& b) { return FolderBefore(a.folder, b.folder); });
  // The player's switches. A pack listed that cannot be mounted as it is is taken out of the list (the game would mount
  // an earlier version's files, or a pack it cannot use); one not read now stays as it was.
  const std::vector<std::string> enabled = ReadEnabled(result.folder);
  for (ModPack& pack : result.packs) {
    if (std::find(enabled.begin(), enabled.end(), pack.folder) == enabled.end()) continue;
    if (pack.state == ModPack::State::kReady || pack.state == ModPack::State::kUnavailable) {
      pack.on = true;
    } else {
      result.turned_off.push_back(pack.folder);
    }
  }
  // A folder listed that is gone is left out as well.
  bool rewrite = !result.turned_off.empty();
  for (const std::string& folder : enabled) {
    rewrite = rewrite || std::none_of(result.packs.begin(), result.packs.end(),
                                      [&](const ModPack& pack) { return pack.folder == folder; });
  }
  if (rewrite) WriteEnabled(result);
  return result;
}

bool SetModPackOn(const fs::path& exe_dir, ModPacks& packs, size_t index, bool on) {
  if (index >= packs.packs.size() || (on && packs.packs[index].state != ModPack::State::kReady)) return false;
  packs.folder = ModsFolder(exe_dir);
  const bool was = packs.packs[index].on;
  packs.packs[index].on = on;
  if (!WriteEnabled(packs)) {
    packs.packs[index].on = was;
    return false;
  }
  return true;
}

ModPackAdd AddModPack(const fs::path& exe_dir, const fs::path& source, bool replace) {
  fs::path staging;
  ModPackAdd add;
  try {
    add = AddPack(exe_dir, source, replace, staging);
  } catch (const std::exception& e) {
    add = {};
    add.error = e.what();
  } catch (...) {
    add = {};
    add.error = "unexpected error";
  }
  if (add.result == ModPackAdd::Result::kFailed && !staging.empty()) {
    std::error_code ec;
    fs::remove_all(staging, ec);
  }
  return add;
}

ModPatchResult ApplyModPatch(const fs::path& base, const fs::path& patch, const fs::path& out,
                             const std::function<bool(uint64_t)>& progress) {
  ModPatchResult result;
  auto refuse = [&result](std::string error, bool damaged) {
    result.error = std::move(error);
    result.damaged = damaged;
    return result;
  };
  std::ifstream patch_file(patch, std::ios::binary);
  if (!patch_file) return refuse("cannot read the patch", false);
  uint8_t header[kPatchHeaderBytes];
  if (!patch_file.read(reinterpret_cast<char*>(header), sizeof(header))) return refuse("truncated patch: header", true);
  if (std::memcmp(header, kPatchMagic, 8) != 0) return refuse("not a patch (magic)", true);
  const uint64_t base_size = U64(header + 8);
  const uint64_t result_size = U64(header + 48);
  if (result_size > kMaxModPatchResult) return refuse("the result would be over 16 GiB", true);

  // The base: its size, then its SHA-256, before anything is written.
  std::error_code ec;
  if (fs::file_size(base, ec) != base_size || ec) {
    result.base_differs = true;
    return refuse(ec ? "the game file is missing" : "the game file's size differs", false);
  }
  std::ifstream base_file(base, std::ios::binary);
  if (!base_file) {
    result.base_differs = true;
    return refuse("cannot read the game file", false);
  }
  std::vector<char> buffer(kChunk);
  uint64_t done = 0;
  {
    Sha256 hash;
    for (uint64_t left = base_size; left;) {
      const size_t want = size_t(std::min<uint64_t>(left, buffer.size()));
      if (!base_file.read(buffer.data(), std::streamsize(want))) return refuse("cannot read the game file", false);
      hash.Add(buffer.data(), want);
      left -= want;
      done += want;
      if (!progress(done)) return refuse("cancelled", false);
    }
    uint8_t digest[32];
    if (!hash.Finish(digest)) return refuse("SHA-256 failed", false);
    if (std::memcmp(digest, header + 16, 32) != 0) {
      result.base_differs = true;
      return refuse("the game file's SHA-256 differs", false);
    }
  }

  // The result, at a temporary name in its folder: removed unless it is complete and right.
  fs::create_directories(out.parent_path(), ec);
  fs::path temporary = out;
  temporary += L".part" + std::to_wstring(GetCurrentProcessId());
  std::ofstream out_file(temporary, std::ios::binary | std::ios::trunc);
  if (!out_file) return refuse("cannot write " + Utf8(temporary), false);
  struct Remove {
    std::ofstream& file;
    const fs::path& path;
    bool keep = false;
    ~Remove() {
      if (keep) return;
      file.close();
      std::error_code ignored;
      fs::remove(path, ignored);
    }
  } remove{out_file, temporary};
  Sha256 hash;
  uint64_t written = 0;
  auto append = [&](std::istream& from, uint64_t length, const char* what) -> std::string {
    while (length) {
      const size_t want = size_t(std::min<uint64_t>(length, buffer.size()));
      if (!from.read(buffer.data(), std::streamsize(want))) return std::string("truncated patch: ") + what;
      hash.Add(buffer.data(), want);
      if (!out_file.write(buffer.data(), std::streamsize(want))) return "cannot write " + Utf8(temporary);
      length -= want;
      written += want;
      done += want;
      if (!progress(done)) return "cancelled";
    }
    return {};
  };
  while (written < result_size) {
    const int kind = patch_file.get();
    if (kind == std::char_traits<char>::eof()) return refuse("truncated patch: the operations end early", true);
    uint8_t fields[16];
    if (kind == 0x00) {
      if (!patch_file.read(reinterpret_cast<char*>(fields), 16)) return refuse("truncated patch: COPY", true);
      const uint64_t offset = U64(fields);
      const uint64_t length = U64(fields + 8);
      if (length == 0) return refuse("COPY of length 0", true);
      if (offset > base_size || length > base_size - offset) return refuse("COPY outside the base", true);
      if (length > result_size - written) return refuse("COPY passes the result's size", true);
      base_file.clear();
      base_file.seekg(std::streamoff(offset));
      if (std::string error = append(base_file, length, "COPY"); !error.empty()) {
        return refuse(error == "truncated patch: COPY" ? "cannot read the game file" : error, false);
      }
    } else if (kind == 0x01) {
      if (!patch_file.read(reinterpret_cast<char*>(fields), 8)) return refuse("truncated patch: DATA", true);
      const uint64_t length = U64(fields);
      if (length == 0) return refuse("DATA of length 0", true);
      if (length > result_size - written) return refuse("DATA passes the result's size", true);
      if (std::string error = append(patch_file, length, "DATA"); !error.empty()) {
        return refuse(error, error.rfind("truncated", 0) == 0);
      }
    } else {
      return refuse("unknown operation " + std::to_string(kind), true);
    }
  }
  if (patch_file.peek() != std::char_traits<char>::eof()) return refuse("bytes after the last operation", true);
  uint8_t digest[32];
  if (!hash.Finish(digest)) return refuse("SHA-256 failed", false);
  if (std::memcmp(digest, header + 56, 32) != 0) return refuse("the result's SHA-256 differs", true);
  out_file.close();
  if (!out_file) return refuse("cannot write " + Utf8(temporary), false);
  if (!MoveFileExW(temporary.c_str(), out.c_str(), MOVEFILE_REPLACE_EXISTING)) {
    return refuse("cannot write " + Utf8(out) + " (" + std::to_string(GetLastError()) + ")", false);
  }
  remove.keep = true;
  result.ok = true;
  result.sha256 = Hex(digest, 32);
  return result;
}

ModPackPreparation::~ModPackPreparation() { Cancel(); }

void ModPackPreparation::Start(const fs::path& exe_dir, const std::string& folder) {
  Cancel();
  cancel_ = false;
  {
    std::lock_guard lock(mutex_);
    progress_ = {};
    progress_.state = State::kRunning;
    progress_.folder = folder;
  }
  worker_ = std::thread(&ModPackPreparation::Run, this, exe_dir, folder);
}

void ModPackPreparation::Cancel() {
  cancel_ = true;
  if (worker_.joinable()) worker_.join();
}

ModPackPreparation::Progress ModPackPreparation::Get() const {
  std::lock_guard lock(mutex_);
  return progress_;
}

void ModPackPreparation::Reset() {
  if (worker_.joinable()) worker_.join();
  std::lock_guard lock(mutex_);
  progress_ = {};
}

void ModPackPreparation::Run(fs::path exe_dir, std::string folder) {
  const auto start = std::chrono::steady_clock::now();
  auto finish = [&](State state, ModPack::State failed_as = ModPack::State::kReady, std::string detail = {},
                    std::string error = {}) {
    std::lock_guard lock(mutex_);
    progress_.state = state;
    progress_.failed_as = failed_as;
    progress_.detail = std::move(detail);
    progress_.error = std::move(error);
    progress_.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
  };
  try {
    const fs::path pack = ModsFolder(exe_dir) / FromUtf8(folder);
    const fs::path files = pack / L"files";
    const fs::path root = GameDataRoot(exe_dir);
    if (root.empty()) {
      finish(State::kFailed, ModPack::State::kReady, {}, "the game files were not found");
      return;
    }
    // Never through a link: files\ itself, then each result's folders (below).
    if (IsLink(files)) {
      finish(State::kFailed, ModPack::State::kReady, {}, "the pack's files folder is a link");
      return;
    }
    // What an earlier preparation could not end left (the launcher stopped): its temporaries, and only those.
    std::error_code ec;
    for (fs::recursive_directory_iterator it(files, fs::directory_options::none, ec), end; !ec && it != end;
         it.increment(ec)) {
      if (it->is_regular_file(ec) && IsPartFile(it->path().filename().native())) fs::remove(it->path(), ec);
    }
    ec.clear();
    // What is left to prepare: each base and each result counted in the progress.
    Record record = ReadRecord(pack);
    struct Work {
      std::string relative;
      PatchHeader header;
    };
    std::vector<Work> work;
    uint64_t total = 0;
    for (const std::string& relative : ListPatches(pack)) {
      PatchHeader header;
      if (!ReadPatchHeader(PatchPath(pack, relative), &header)) {
        finish(State::kFailed, ModPack::State::kBroken, relative, "not a patch");
        return;
      }
      if (header.result_size > kMaxModPatchResult) {
        finish(State::kFailed, ModPack::State::kBroken, relative, "the result would be over 16 GiB");
        return;
      }
      if (NeedOf(pack, root, relative, header, record) == PatchNeed::kPrepared) continue;
      work.push_back({relative, header});
      total = Saturated(total, Saturated(header.base_size, header.result_size));
    }
    {
      std::lock_guard lock(mutex_);
      progress_.count = work.size();
      progress_.total = total;
    }
    uint64_t before = 0;  // the files done
    for (size_t i = 0; i < work.size(); ++i) {
      if (cancel_) {
        finish(State::kCancelled);
        return;
      }
      const Work& w = work[i];
      {
        std::lock_guard lock(mutex_);
        progress_.index = i;
        progress_.current = w.relative;
      }
      const fs::path patch = PatchPath(pack, w.relative);
      const fs::path base = root / FromUtf8(w.relative);
      const fs::path out = files / FromUtf8(w.relative);
      const std::string key = Lower(w.relative);
      fs::create_directories(out.parent_path(), ec);
      if (!WritesInside(pack, out)) {
        finish(State::kFailed, ModPack::State::kReady, w.relative, "a link in the way of " + Utf8(out));
        return;
      }
      ModPatchResult applied = ApplyModPatch(base, patch, out, [&](uint64_t done) {
        std::lock_guard lock(mutex_);
        progress_.done = Saturated(before, done);
        return !cancel_.load();
      });
      if (cancel_) {
        finish(State::kCancelled);
        return;
      }
      record.paths[key] = w.relative;
      if (!applied.ok) {
        if (applied.base_differs) {
          // Kept, so that the next reading knows without reading the base again (until it or the patch changes).
          record.prepared.erase(key);
          record.differs[key] = {fs::file_size(patch, ec), FileTime(patch), fs::file_size(base, ec), FileTime(base)};
          WriteRecord(pack, record);
          finish(State::kFailed, ModPack::State::kOtherFiles, w.relative, applied.error);
        } else {
          finish(State::kFailed, applied.damaged ? ModPack::State::kBroken : ModPack::State::kReady, w.relative,
                 applied.error);
        }
        return;
      }
      record.differs.erase(key);
      record.prepared[key] = {fs::file_size(patch, ec), FileTime(patch), w.header.result_size, applied.sha256};
      if (!WriteRecord(pack, record)) {
        finish(State::kFailed, ModPack::State::kReady, w.relative, "cannot write " + Utf8(pack / kRecordFile));
        return;
      }
      before = Saturated(before, Saturated(w.header.base_size, w.header.result_size));
    }
    {
      std::lock_guard lock(mutex_);
      progress_.index = work.size();
      progress_.done = total;
    }
    finish(State::kDone);
  } catch (const std::exception& e) {
    finish(State::kFailed, ModPack::State::kReady, {}, e.what());
  } catch (...) {
    finish(State::kFailed, ModPack::State::kReady, {}, "unexpected error");
  }
}

}  // namespace launcher
