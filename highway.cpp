// The Mods tab's Highway section: the game's textures, the player's pictures and the preview (highway.h).
//
// The game's files (launcher 3.30, local_docs/52 of GH Warriors of Rock): a .pak.xen is a list of CHNK chunks, each a
// 0x80-byte header (big-endian: "CHNK", the header's size, the compressed size, the chunk's size on disk, the next
// one's, the inflated size and its place) and raw deflate. The pak inflated is a list of 32-byte entries (the type's
// key, the data's offset from the entry, its size, 0, the full name's key, the name's key, 0, 0). A .tex dictionary is
// FACECAA7, a version and a count (16 bits each), the table's offset; 40-byte entries (the key at +4; the D3D header's
// offset at +28, the data's size at +32 and offset at +36); the Xenos fetch constant 28 bytes into that header (tiled
// and pitch in dword 0, the format and the endianness in dword 1, the size in dword 2, the swizzle in dword 3). Mip 0
// only: untiled as Xenia's texture_util::GetTiledOffset2D does, DXT1/3/5, 8888 and 8_8 decoded, then swizzled.

#include "highway.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <stb_image.h>

#include "picture.h"
#include "ui/gfx.h"
#include "ui/ui.h"

namespace launcher::highway {

using namespace ui;

namespace {

uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint16_t Be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }

// The pak inflated: its CHNK chunks one after the other, each in its place; empty if it is not one.
std::vector<uint8_t> Inflate(const std::vector<uint8_t>& file) {
  std::vector<uint8_t> out;
  size_t at = 0;
  while (at + 0x80 <= file.size() && std::memcmp(file.data() + at, "CHNK", 4) == 0) {
    const uint8_t* h = file.data() + at;
    const uint32_t header = Be32(h + 4), compressed = Be32(h + 8), on_disk = Be32(h + 12);
    const uint32_t size = Be32(h + 20), place = Be32(h + 24);
    if (header + size_t(compressed) > file.size() - at || size > (64u << 20) || place > (256u << 20)) return {};
    if (out.size() < size_t(place) + size) out.resize(size_t(place) + size);
    const int written = stbi_zlib_decode_noheader_buffer(reinterpret_cast<char*>(out.data() + place), int(size),
                                                         reinterpret_cast<const char*>(h + header), int(compressed));
    if (written < 0) return {};
    if (on_disk == 0) break;
    at += on_disk;
  }
  return out;
}

// The pak entry whose full name's key is `key`: its data.
std::pair<const uint8_t*, size_t> PakEntry(const std::vector<uint8_t>& pak, uint32_t key) {
  for (size_t at = 0; at + 32 <= pak.size(); at += 32) {
    const uint8_t* e = pak.data() + at;
    const uint32_t offset = Be32(e + 4), size = Be32(e + 8);
    if (offset == 0 && size == 0) break;
    if (Be32(e + 16) == key && at + uint64_t(offset) + size <= pak.size()) return {e + offset, size};
  }
  return {nullptr, 0};
}

uint32_t TiledOffset2D(uint32_t x, uint32_t y, uint32_t pitch, uint32_t bpb_log2) {
  pitch = (pitch + 31) & ~31u;
  const uint32_t macro = ((x >> 5) + (y >> 5) * (pitch >> 5)) << (bpb_log2 + 7);
  const uint32_t micro = ((x & 7) + ((y & 0xE) << 2)) << bpb_log2;
  const uint32_t offset = macro + ((micro & ~0xFu) << 1) + (micro & 0xF) + ((y & 1) << 4);
  return ((offset & ~0x1FFu) << 3) + ((y & 16) << 7) + ((offset & 0x1C0) << 2) +
         (((((y & 8) >> 2) + (x >> 3)) & 3) << 6) + (offset & 0x3F);
}

void Color565(uint16_t v, uint8_t* rgb) {
  rgb[0] = uint8_t(((v >> 11) & 31) * 255 / 31);
  rgb[1] = uint8_t(((v >> 5) & 63) * 255 / 63);
  rgb[2] = uint8_t((v & 31) * 255 / 31);
}

// A texture of the dictionary as RGBA (mip 0), or empty.
struct Image {
  std::vector<uint8_t> rgba;
  int width = 0;
  int height = 0;
};

Image DecodeTexture(const uint8_t* tex, size_t tex_size, uint32_t key) {
  Image image;
  if (tex_size < 16 || Be32(tex) != 0xFACECAA7) return image;
  const uint32_t count = Be16(tex + 6), table = Be32(tex + 8);
  for (uint32_t n = 0; n < count; ++n) {
    const size_t at = table + size_t(n) * 40;
    if (at + 40 > tex_size || Be32(tex + at + 4) != key) continue;
    const uint32_t header = Be32(tex + at + 28), size = Be32(tex + at + 32), data_at = Be32(tex + at + 36);
    if (header + 28 + 24 > tex_size || data_at + uint64_t(size) > tex_size) return image;
    const uint8_t* fetch = tex + header + 28;
    const uint32_t d0 = Be32(fetch), d1 = Be32(fetch + 4), d2 = Be32(fetch + 8), d3 = Be32(fetch + 12);
    const bool tiled = (d0 >> 31) & 1;
    const uint32_t pitch_texels = ((d0 >> 22) & 0x1FF) * 32;
    const uint32_t format = d1 & 0x3F, endian = (d1 >> 6) & 3;
    const int w = int(d2 & 0x1FFF) + 1, h = int((d2 >> 13) & 0x1FFF) + 1;
    const uint32_t swizzle = (d3 >> 1) & 0xFFF;
    int block = 1, bpb = 0;
    switch (format) {
      case 18: block = 4, bpb = 8; break;   // DXT1
      case 19:                               // DXT2/3
      case 20: block = 4, bpb = 16; break;  // DXT4/5
      case 6: bpb = 4; break;               // 8_8_8_8
      case 10: bpb = 2; break;              // 8_8
      case 2: bpb = 1; break;               // 8
      default: return image;
    }
    const uint32_t log2 = bpb == 1 ? 0 : bpb == 2 ? 1 : bpb == 4 ? 2 : bpb == 8 ? 3 : 4;
    const int bw = (w + block - 1) / block, bh = (h + block - 1) / block;
    const uint32_t pitch = std::max<uint32_t>(1, pitch_texels / uint32_t(block));
    const uint8_t* data = tex + data_at;
    image.width = w;
    image.height = h;
    image.rgba.assign(size_t(w) * h * 4, 0);
    uint8_t b[16];
    for (int by = 0; by < bh; ++by) {
      for (int bx = 0; bx < bw; ++bx) {
        const uint32_t off = tiled ? TiledOffset2D(uint32_t(bx), uint32_t(by), pitch, log2)
                                   : (uint32_t(by) * pitch + uint32_t(bx)) * uint32_t(bpb);
        if (off + uint32_t(bpb) > size) continue;
        std::memcpy(b, data + off, size_t(bpb));
        if (endian == 1) {
          for (int i = 0; i + 1 < bpb; i += 2) std::swap(b[i], b[i + 1]);
        } else if (endian == 2) {
          for (int i = 0; i + 3 < bpb; i += 4) std::swap(b[i], b[i + 3]), std::swap(b[i + 1], b[i + 2]);
        } else if (endian == 3) {
          for (int i = 0; i + 3 < bpb; i += 4) std::swap(b[i], b[i + 2]), std::swap(b[i + 1], b[i + 3]);
        }
        auto put = [&](int x, int y, const uint8_t* c) {
          if (x < w && y < h) std::memcpy(&image.rgba[(size_t(y) * w + x) * 4], c, 4);
        };
        if (block == 1) {
          uint8_t c[4] = {0, 0, 0, 255};
          for (int i = 0; i < bpb; ++i) c[i] = b[i];
          put(bx, by, c);
          continue;
        }
        const uint8_t* color = bpb == 16 ? b + 8 : b;
        const uint16_t c0 = uint16_t(color[0] | color[1] << 8), c1 = uint16_t(color[2] | color[3] << 8);
        const uint32_t bits = uint32_t(color[4]) | uint32_t(color[5]) << 8 | uint32_t(color[6]) << 16 |
                              uint32_t(color[7]) << 24;
        uint8_t palette[4][4];
        Color565(c0, palette[0]);
        Color565(c1, palette[1]);
        const bool four = c0 > c1 || bpb == 16;
        for (int k = 0; k < 3; ++k) {
          palette[2][k] = uint8_t(four ? (2 * palette[0][k] + palette[1][k]) / 3 : (palette[0][k] + palette[1][k]) / 2);
          palette[3][k] = uint8_t(four ? (palette[0][k] + 2 * palette[1][k]) / 3 : 0);
        }
        palette[0][3] = palette[1][3] = palette[2][3] = 255;
        palette[3][3] = four ? 255 : 0;
        uint8_t alphas[8];
        uint64_t alpha_bits = 0;
        if (format == 20) {
          alphas[0] = b[0];
          alphas[1] = b[1];
          for (int i = 0; i < 6; ++i) alpha_bits |= uint64_t(b[2 + i]) << (8 * i);
          if (b[0] > b[1]) {
            for (int i = 0; i < 6; ++i) alphas[2 + i] = uint8_t(((6 - i) * b[0] + (i + 1) * b[1]) / 7);
          } else {
            for (int i = 0; i < 4; ++i) alphas[2 + i] = uint8_t(((4 - i) * b[0] + (i + 1) * b[1]) / 5);
            alphas[6] = 0;
            alphas[7] = 255;
          }
        } else if (format == 19) {
          for (int i = 0; i < 8; ++i) alpha_bits |= uint64_t(b[i]) << (8 * i);
        }
        for (int k = 0; k < 16; ++k) {
          uint8_t c[4];
          std::memcpy(c, palette[(bits >> (2 * k)) & 3], 4);
          if (format == 20) c[3] = alphas[(alpha_bits >> (3 * k)) & 7];
          if (format == 19) c[3] = uint8_t(((alpha_bits >> (4 * k)) & 15) * 17);
          put(bx * 4 + k % 4, by * 4 + k / 4, c);
        }
      }
    }
    // The fetch constant's swizzle: each of RGBA from X, Y, Z, W, 0 or 1.
    if (swizzle != 0x688) {  // 0x688 = XYZW
      for (size_t i = 0; i < image.rgba.size(); i += 4) {
        const uint8_t in[6] = {image.rgba[i], image.rgba[i + 1], image.rgba[i + 2], image.rgba[i + 3], 0, 255};
        for (int c = 0; c < 4; ++c) {
          const uint32_t from = (swizzle >> (3 * c)) & 7;
          image.rgba[i + c] = in[from < 6 ? from : 4];
        }
      }
    }
    return image;
  }
  return image;
}

Texture MakeTexture(const uint8_t* rgba, int width, int height) {
  Texture t;
  t.id = gfx::CreateTexture(rgba, width, height);
  if (t.id) {
    t.width = width;
    t.height = height;
  }
  return t;
}

void Free(Texture& t) {
  if (t.id) gfx::DestroyTexture(t.id);
  t = Texture{};
}

bool SameName(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
         });
}

std::string Utf8(const std::filesystem::path& path) {
  const std::u8string s = path.u8string();
  return std::string(reinterpret_cast<const char*>(s.data()), s.size());
}

std::filesystem::path FromUtf8(const std::string& s) {
  return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}

}  // namespace

std::filesystem::path PathFromUtf8(const std::string& utf8) { return FromUtf8(utf8); }
std::string Utf8Of(const std::filesystem::path& path) { return Utf8(path); }

namespace {

// The pictures' sizes: a thumbnail, and the preview's (the game's highway texture is 512 x 1024).
constexpr int kThumbWidth = 96, kThumbHeight = 192;
constexpr int kLargeWidth = 512, kLargeHeight = 1024;
constexpr size_t kLargeKept = 6;

}  // namespace

// --- The game's textures ---

GameTextures::~GameTextures() {
  if (thread_.joinable()) thread_.join();
  for (Texture& t : textures_) Free(t);
}

void GameTextures::Load(const std::filesystem::path& pak, const std::filesystem::path& highway_pak) {
  if (started_) return;
  started_ = true;
  thread_ = std::thread([this, pak, highway_pak] {
    std::vector<Decoded> decoded(static_cast<size_t>(Part::kCount));
    bool ok = false;
    auto read = [](const std::filesystem::path& path) {
      std::ifstream file(path, std::ios::binary);
      if (!file) return std::vector<uint8_t>{};
      std::vector<uint8_t> raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
      return Inflate(raw);
    };
    std::vector<uint8_t> inflated = read(pak);
    if (!inflated.empty()) {
      const game::HighwayAssets& a = game::kHighwayAssets;
      auto [tex, tex_size] = PakEntry(inflated, a.dictionary);
      // The highway's texture where the game keeps it in another pak (launcher 3.31.1).
      std::vector<uint8_t> highway_inflated;
      const uint8_t* highway_tex = tex;
      size_t highway_tex_size = tex_size;
      if (!highway_pak.empty() && a.highway_dictionary) {
        highway_inflated = read(highway_pak);
        auto [h, h_size] = PakEntry(highway_inflated, a.highway_dictionary);
        highway_tex = h;
        highway_tex_size = h_size;
      }
      if (tex && highway_tex) {
        const uint32_t keys[] = {a.highway, a.sidebar,   a.string,  a.fretbar, a.collar,
                                 a.gem_base, a.gem_color, a.gem_cap, a.sustain};
        static_assert(std::size(keys) == size_t(Part::kCount));
        ok = true;
        for (size_t i = 0; i < std::size(keys); ++i) {
          Image image = Part(i) == Part::kHighway ? DecodeTexture(highway_tex, highway_tex_size, keys[i])
                                                  : DecodeTexture(tex, tex_size, keys[i]);
          if (image.rgba.empty()) {
            ok = false;
            continue;
          }
          // The highway's alpha is a mask of its edges the game does not blend with: drawn opaque.
          if (Part(i) == Part::kHighway) {
            for (size_t p = 3; p < image.rgba.size(); p += 4) image.rgba[p] = 255;
          }
          decoded[i] = {std::move(image.rgba), image.width, image.height};
        }
      }
    }
    std::lock_guard lock(mutex_);
    decoded_ = std::move(decoded);
    ok_ = ok;
    done_ = true;
  });
}

void GameTextures::Update() {
  if (!started_ || ready_ || failed_) return;
  std::lock_guard lock(mutex_);
  if (!done_) return;
  if (!ok_) {
    failed_ = true;
    return;
  }
  // The visible part of each sprite: the pixels with some alpha. The strike line's rings and the notes are placed by
  // it (the rings' centre on the line), the gem's layers by the union of theirs.
  auto content = [](const Decoded& d, ImVec2& lo, ImVec2& hi) {
    int x0 = d.width, y0 = d.height, x1 = -1, y1 = -1;
    for (int y = 0; y < d.height; ++y) {
      for (int x = 0; x < d.width; ++x) {
        if (d.rgba[(size_t(y) * d.width + x) * 4 + 3] > 24) {
          x0 = std::min(x0, x), y0 = std::min(y0, y), x1 = std::max(x1, x), y1 = std::max(y1, y);
        }
      }
    }
    if (x1 < 0) return;
    lo = ImVec2(std::min(lo.x, float(x0) / d.width), std::min(lo.y, float(y0) / d.height));
    hi = ImVec2(std::max(hi.x, float(x1 + 1) / d.width), std::max(hi.y, float(y1 + 1) / d.height));
  };
  for (size_t i = 0; i < decoded_.size(); ++i) {
    textures_[i] = MakeTexture(decoded_[i].rgba.data(), decoded_[i].width, decoded_[i].height);
  }
  for (Part part : {Part::kCollar}) {
    ImVec2 lo(1.0f, 1.0f), hi(0.0f, 0.0f);
    content(decoded_[size_t(part)], lo, hi);
    if (hi.x > lo.x) textures_[size_t(part)].content_min = lo, textures_[size_t(part)].content_max = hi;
  }
  {
    ImVec2 lo(1.0f, 1.0f), hi(0.0f, 0.0f);
    for (Part part : {Part::kGemBase, Part::kGemColor, Part::kGemCap}) content(decoded_[size_t(part)], lo, hi);
    if (hi.x > lo.x) {
      for (Part part : {Part::kGemBase, Part::kGemColor, Part::kGemCap}) {
        textures_[size_t(part)].content_min = lo;
        textures_[size_t(part)].content_max = hi;
      }
    }
  }
  decoded_.clear();
  ready_ = true;
}

// --- The player's pictures ---

Pictures::Pictures() { thread_ = std::thread([this] { Work(); }); }

Pictures::~Pictures() {
  {
    std::lock_guard lock(mutex_);
    quit_ = true;
  }
  wake_.notify_all();
  if (thread_.joinable()) thread_.join();
  FreeTextures();
}

void Pictures::FreeTextures() {
  for (Picture& p : list_) Free(p.thumbnail);
  for (auto& [name, large] : large_) Free(large.texture);
  large_.clear();
  large_order_.clear();
}

int Pictures::Find(std::string_view name) const {
  for (size_t i = 0; i < list_.size(); ++i) {
    if (SameName(list_[i].name, name)) return int(i);
  }
  return -1;
}

void Pictures::Refresh(const std::string& folder) {
  const double now = ImGui::GetTime();
  if (folder == folder_ && now - listed_at_ < 2.0) return;
  listed_at_ = now;
  if (folder != folder_) {
    // Another folder: everything from the start.
    folder_ = folder;
    path_ = FromUtf8(folder);
    FreeTextures();
    list_.clear();
    std::lock_guard lock(mutex_);
    ++generation_;
    work_generation_ = generation_;
    work_folder_ = path_;
    jobs_.clear();
    results_.clear();
  }
  List(path_);
}

void Pictures::List(const std::filesystem::path& folder) {
  std::error_code error;
  exists_ = std::filesystem::is_directory(folder, error);
  std::vector<Picture> found;
  if (exists_) {
    for (const auto& entry : std::filesystem::directory_iterator(folder, error)) {
      std::string ext = Utf8(entry.path().extension());
      std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
      if (!entry.is_regular_file(error) ||
          !(ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga")) {
        continue;
      }
      Picture p;
      p.name = Utf8(entry.path().filename());
      p.modified = int64_t(entry.last_write_time(error).time_since_epoch().count());
      found.push_back(std::move(p));
    }
  }
  // By name, as Windows' Explorer sorts letters (any case).
  std::sort(found.begin(), found.end(), [](const Picture& a, const Picture& b) {
    return std::lexicographical_compare(a.name.begin(), a.name.end(), b.name.begin(), b.name.end(), [](char x, char y) {
      return std::tolower(static_cast<unsigned char>(x)) < std::tolower(static_cast<unsigned char>(y));
    });
  });
  // The pictures already there keep their thumbnails (unless the file changed); the new ones are decoded.
  std::vector<Job> jobs;
  for (Picture& p : found) {
    const int old = Find(p.name);
    if (old >= 0 && list_[size_t(old)].modified == p.modified) {
      p = std::move(list_[size_t(old)]);
      list_[size_t(old)].thumbnail = Texture{};
    } else {
      if (old >= 0) {
        if (auto it = large_.find(list_[size_t(old)].name); it != large_.end()) {
          Free(it->second.texture);
          large_.erase(it);
          large_order_.remove(list_[size_t(old)].name);
        }
      }
      jobs.push_back({generation_, p.name, Kind::kThumbnail});
    }
  }
  for (Picture& p : list_) Free(p.thumbnail);  // the ones gone
  list_ = std::move(found);
  if (!jobs.empty()) {
    std::lock_guard lock(mutex_);
    for (Job& job : jobs) jobs_.push_back(std::move(job));
  }
  wake_.notify_all();
}

const Texture* Pictures::Large(std::string_view name) {
  if (name.empty()) return nullptr;
  std::string key(name);
  large_order_.remove(key);
  large_order_.push_front(key);
  LargeEntry& large = large_[key];
  if (!large.asked) {
    large.asked = true;
    {
      std::lock_guard lock(mutex_);
      jobs_.push_front({generation_, key, Kind::kLarge});
    }
    wake_.notify_all();
  }
  // Only the last ones drawn are kept.
  while (large_order_.size() > kLargeKept) {
    auto it = large_.find(large_order_.back());
    if (it != large_.end()) {
      Free(it->second.texture);
      large_.erase(it);
    }
    large_order_.pop_back();
  }
  auto it = large_.find(key);
  return it != large_.end() && it->second.texture.id ? &it->second.texture : nullptr;
}

void Pictures::Update() {
  std::vector<Result> results;
  {
    std::lock_guard lock(mutex_);
    results.swap(results_);
  }
  for (Result& r : results) {
    if (r.generation != generation_) continue;
    if (r.kind == Kind::kThumbnail) {
      const int at = Find(r.name);
      if (at < 0) continue;
      Picture& p = list_[size_t(at)];
      Free(p.thumbnail);
      p.tried = true;
      p.unreadable = !r.ok;
      // A TGA: the game reads it (stb_image), Windows' codecs do not.
      if (!r.ok && p.name.size() > 4 && SameName(std::string_view(p.name).substr(p.name.size() - 4), ".tga")) {
        p.unreadable = false;
        p.no_preview = true;
      }
      if (r.ok) {
        p.width = r.file_width;
        p.height = r.file_height;
        p.luminance = r.luminance;
        p.thumbnail = MakeTexture(r.rgba.data(), r.width, r.height);
      }
    } else if (auto it = large_.find(r.name); it != large_.end()) {
      Free(it->second.texture);
      it->second.failed = !r.ok;
      if (r.ok) it->second.texture = MakeTexture(r.rgba.data(), r.width, r.height);
    }
  }
}

void Pictures::Work() {
  for (;;) {
    Job job;
    std::filesystem::path folder;
    {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, [&] { return quit_ || !jobs_.empty(); });
      if (quit_) return;
      job = std::move(jobs_.front());
      jobs_.pop_front();
      if (job.generation != work_generation_) continue;
      folder = work_folder_;
    }
    Result r{job.generation, job.name, job.kind};
    const bool thumb = job.kind == Kind::kThumbnail;
    const int width = thumb ? kThumbWidth : kLargeWidth, height = thumb ? kThumbHeight : kLargeHeight;
    if (std::optional<Rgba> picture =
            LoadStretchedPicture(folder / FromUtf8(job.name), width, height, &r.file_width, &r.file_height)) {
      r.ok = true;
      r.width = picture->width;
      r.height = picture->height;
      r.rgba = std::move(picture->pixels);
      // NativeXRenderer's D177 measures the luminance (Rec. 709) of the whole picture; the thumbnail is close enough.
      double sum = 0.0;
      for (size_t i = 0; i + 3 < r.rgba.size(); i += 4) {
        sum += 0.2126 * r.rgba[i] + 0.7152 * r.rgba[i + 1] + 0.0722 * r.rgba[i + 2];
        r.rgba[i + 3] = 255;  // drawn opaque, as the game draws it
      }
      r.luminance = float(sum / (double(r.width) * r.height * 255.0));
    }
    std::lock_guard lock(mutex_);
    results_.push_back(std::move(r));
  }
}

// --- The preview ---

float Brightness(std::string_view value, float luminance) {
  if (value == "auto") {
    // NativeXRenderer's kAutoLuminance and kAutoLeast.
    constexpr float kAutoLuminance = 0.20f, kAutoLeast = 0.35f;
    return luminance > kAutoLuminance ? std::max(kAutoLeast, kAutoLuminance / luminance) : 1.0f;
  }
  if (value.empty() || value == "off") return 1.0f;
  return std::clamp(float(std::atoi(std::string(value).c_str())) / 100.0f, 0.1f, 1.0f);
}

namespace {

// The game's projection (its src/mods.cpp): rows linear in time whose heights grow as h[i] = (h[i-1] f)^e, normalised
// over the rows; the perspective times the compression, the length k the same projection continued (e^k).
struct Shape {
  std::vector<double> place;  // place[r]: from the far end (0) to the strike line (1) at row r, rows + 1 of them
  double height = 0.0;
  double top_width = 0.0;
  double length = 1.0;  // the length that fits
};

double Exponent(double f, double e) { return e * std::log(f) / (e - 1.0); }

std::vector<double> Places(double f, double e, int rows) {
  std::vector<double> h(size_t(rows) + 1), place(size_t(rows) + 1);
  h[0] = 1.0;
  for (int i = 1; i <= rows; ++i) h[size_t(i)] = std::pow(h[size_t(i) - 1] * f, e);
  double sum = 0.0;
  for (int i = 0; i < rows; ++i) {
    place[size_t(i)] = sum;
    sum += h[size_t(i)];
  }
  place[size_t(rows)] = sum;
  for (double& p : place) p /= sum;
  return place;
}

double PlaceAt(const std::vector<double>& place, double row) {
  const int rows = int(place.size()) - 1;
  row = std::clamp(row, 0.0, double(rows));
  const int whole = std::min(int(row), rows - 1);
  return place[size_t(whole)] + (place[size_t(whole) + 1] - place[size_t(whole)]) * (row - whole);
}

Shape MakeShape(double p, double k) {
  const game::HighwayAssets& a = game::kHighwayAssets;
  Shape s;
  s.length = k;
  const double a0 = Exponent(a.perspective_fact, a.perspective_exp);
  const double ak = a0 * p * std::pow(a.perspective_exp, a.rows * (1.0 - k));
  const double e = std::pow(a.perspective_exp, k);
  const double f = std::exp(ak * (e - 1.0) / e);
  s.place = Places(f, e, a.rows);
  s.height = k == 1.0 ? a.height : a.height / (1.0 - PlaceAt(s.place, a.rows * (1.0 - 1.0 / k)));
  const double sqrt_r = std::exp(a0 * (std::pow(a.perspective_exp, a.rows) - 1.0) / 2.0);
  const double top_at_far = std::min<double>(a.bottom_width, a.top_width * std::pow(sqrt_r, 1.0 - p));
  s.top_width = top_at_far + (a.bottom_width - top_at_far) * (a.height - s.height) / a.height;
  return s;
}

// The shape for a look, cached (1025 powers a change).
const Shape& ShapeFor(const Look& look) {
  static Shape shape;
  static double p = -1.0, k = -1.0, scale = -1.0;
  if (look.perspective != p || look.length != k || look.scale != scale) {
    p = look.perspective;
    k = look.length;
    scale = look.scale;
    // The length that fits, as the game finds it: the far end, with the scale, no higher than highest_top.
    const game::HighwayAssets& a = game::kHighwayAssets;
    const double room = (a.playline - a.highest_top) / std::max(1.0, scale);
    double length = k;
    shape = MakeShape(p, length);
    while (length > 1.0 + 1e-9 && shape.height > room) {
      length = std::max(1.0, length - 0.05);
      shape = MakeShape(p, length);
    }
  }
  return shape;
}

// The lanes' colors: green, red, yellow, blue, orange.
constexpr ImU32 kLanes[5] = {IM_COL32(40, 200, 70, 255), IM_COL32(230, 50, 50, 255), IM_COL32(245, 205, 40, 255),
                             IM_COL32(50, 120, 240, 255), IM_COL32(245, 130, 30, 255)};

struct Note {
  float time;     // in the loop, seconds
  int lane;
  float sustain;  // seconds, 0 for none
};
// Eight beats at 120 BPM, then again.
constexpr float kLoop = 4.0f;
constexpr Note kNotes[] = {{0.0f, 0, 0.0f},  {0.5f, 1, 0.0f}, {1.0f, 2, 0.0f},  {1.25f, 3, 0.0f}, {1.5f, 4, 0.0f},
                           {2.0f, 2, 0.75f}, {3.0f, 1, 0.0f}, {3.0f, 3, 0.0f}, {3.5f, 0, 0.0f}};

ImU32 Tint(ImU32 color, float factor, float alpha = 1.0f) {
  const int r = int(float((color >> IM_COL32_R_SHIFT) & 0xFF) * factor);
  const int g = int(float((color >> IM_COL32_G_SHIFT) & 0xFF) * factor);
  const int b = int(float((color >> IM_COL32_B_SHIFT) & 0xFF) * factor);
  const int a = int(float((color >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
  return IM_COL32(std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255), std::clamp(a, 0, 255));
}

}  // namespace

void Draw(const ImRect& area, const GameTextures& game, const Texture* picture, const Look& look, double time) {
  const game::HighwayAssets& a = game::kHighwayAssets;
  ImDrawList* draw = ImGui::GetWindowDrawList();
  draw->PushClipRect(area.Min, area.Max, true);
  // The venue behind: a warm light from above fading to the stage's dark.
  draw->AddRectFilledMultiColor(area.Min, area.Max, IM_COL32(52, 30, 18, 255), IM_COL32(52, 30, 18, 255),
                                IM_COL32(12, 10, 11, 255), IM_COL32(12, 10, 11, 255));
  const Shape& shape = ShapeFor(look);
  const double k = shape.length;
  const double s = look.scale;
  // The game's 1280 x 720 region shown: 560 high from y = 140, as wide as the area, centered on the highway.
  const float region_top = 140.0f, region_height = 560.0f;
  const float unit = area.GetHeight() / region_height;
  const ImVec2 center(area.GetCenter().x, area.Min.y);
  auto at = [&](double x, double y) {  // game coordinates (x from the highway's middle) to the area's
    const double sy = a.playline + (y - a.playline) * s;
    return ImVec2(center.x + float(x * s) * unit, center.y + float(sy - region_top) * unit);
  };
  const double top_y = a.playline - shape.height;
  auto width_at = [&](double place) { return shape.top_width + (a.bottom_width - shape.top_width) * place; };
  auto y_at = [&](double place) { return top_y + shape.height * place; };
  // A place below the strike line continues its lines (the highway goes on to the screen's bottom).
  const double below = 45.0 / shape.height;
  const double crossing = a.crossing_time * k;
  const float t = float(std::fmod(time, double(kLoop) * 64.0));

  if (!game.Ready()) {
    draw->PopClipRect();
    return;
  }
  // The highway: strips from the far end to below the strike line, the texture along time (a.tiling repeats over the
  // console's length, k times that over a longer one), moving with the notes.
  {
    const Texture& texture = picture ? *picture : game.Get(Part::kHighway);
    const float tint = look.dark ? 0.0f : (picture ? look.brightness : 1.0f);
    const ImU32 color = Tint(IM_COL32_WHITE, tint, look.opacity);
    constexpr int kStrips = 64;
    const double tiles = double(a.tiling) * k;
    const double scroll = std::fmod(double(t) * tiles / crossing, 1.0);
    // In 16 columns: a trapezoid drawn as two triangles bends the picture along their diagonal (a zigzag strip by
    // strip); narrow pieces keep it straight.
    auto quad = [&](double pa, double pb, double va, double vb) {
      constexpr int kColumns = 16;
      const double wa = width_at(pa), wb = width_at(pb);
      for (int c = 0; c < kColumns; ++c) {
        const double f0 = double(c) / kColumns, f1 = double(c + 1) / kColumns;
        draw->AddImageQuad(texture.id, at(wa * (f0 - 0.5), y_at(pa)), at(wa * (f1 - 0.5), y_at(pa)),
                           at(wb * (f1 - 0.5), y_at(pb)), at(wb * (f0 - 0.5), y_at(pb)), ImVec2(float(f0), float(va)),
                           ImVec2(float(f1), float(va)), ImVec2(float(f1), float(vb)), ImVec2(float(f0), float(vb)),
                           color);
      }
    };
    for (int i = 0; i < kStrips; ++i) {
      const double r0 = double(a.rows) * i / kStrips, r1 = double(a.rows) * (i + 1) / kStrips;
      const double p0 = PlaceAt(shape.place, r0), p1 = PlaceAt(shape.place, r1);
      // v from the far end: the time from the strike line, in textures.
      double v0 = tiles * (r0 / a.rows) - scroll, v1 = tiles * (r1 / a.rows) - scroll;
      // Split where the texture repeats (the sampler clamps).
      const double floor0 = std::floor(v0);
      v0 -= floor0;
      v1 -= floor0;
      if (v1 <= 1.0) {
        quad(p0, p1, v0, v1);
      } else {
        const double cut = p0 + (p1 - p0) * (1.0 - v0) / (v1 - v0);
        quad(p0, cut, v0, 1.0);
        quad(cut, p1, 0.0, v1 - 1.0);
      }
    }
    // Below the strike line, the texture's last rows stretched.
    quad(1.0, 1.0 + below, 0.97, 1.0);
  }
  const double lane_step = 0.18;  // of the width, between lanes' middles
  auto lane_x = [&](int lane, double place) { return (lane - 2) * lane_step * width_at(place); };
  auto factor = [&](double place) { return width_at(place) / a.bottom_width; };
  // A note's place: rows are linear in time, the strike line at the last.
  auto place_of = [&](double ahead) { return PlaceAt(shape.place, a.rows * (1.0 - ahead / crossing)); };

  // The strings: one along each lane.
  {
    const Texture& string = game.Get(Part::kString);
    for (int lane = 0; lane < 5; ++lane) {
      const double w0 = 2.0 * factor(0.0), w1 = 2.0 * factor(1.0 + below);
      const double x0 = lane_x(lane, 0.0), x1 = lane_x(lane, 1.0 + below);
      draw->AddImageQuad(string.id, at(x0 - w0, y_at(0.0)), at(x0 + w0, y_at(0.0)), at(x1 + w1, y_at(1.0 + below)),
                         at(x1 - w1, y_at(1.0 + below)), ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
                         IM_COL32(255, 255, 255, 110));
    }
  }
  // The beat lines, every half second (120 BPM), moving with the notes.
  {
    const Texture& bar = game.Get(Part::kFretbar);
    const float beat = 0.5f;
    for (float when = std::ceil(t / beat) * beat; when - t <= crossing; when += beat) {
      const double place = place_of(when - t);
      const double half = width_at(place) * 0.5, thick = 7.0 * factor(place);
      draw->AddImage(bar.id, at(-half, y_at(place) - thick), at(half, y_at(place) + thick), ImVec2(0, 0), ImVec2(1, 1),
                     IM_COL32(255, 255, 255, 150));
    }
  }
  // The sidebars along the side lines.
  {
    const Texture& side = game.Get(Part::kSidebar);
    for (int sign = -1; sign <= 1; sign += 2) {
      const double x0 = sign * width_at(0.0) * 0.5, x1 = sign * width_at(1.0 + below) * 0.5;
      const double w0 = 10.0 * factor(0.0), w1 = 10.0 * factor(1.0 + below);
      draw->AddImageQuad(side.id, at(x0 - w0, y_at(0.0)), at(x0 + w0, y_at(0.0)), at(x1 + w1, y_at(1.0 + below)),
                         at(x1 - w1, y_at(1.0 + below)), ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1),
                         IM_COL32(255, 255, 255, 230));
    }
  }
  // A sprite's visible part centred on (x, y), `width` wide (game units), its height by its own shape.
  auto sprite = [&](const Texture& t, double x, double y, double width, ImU32 color) {
    const ImVec2 lo = t.content_min, hi = t.content_max;
    const double aspect = (double(hi.y - lo.y) * t.height) / std::max(1.0, double(hi.x - lo.x) * t.width);
    const double height = width * aspect;
    draw->AddImage(t.id, at(x - width * 0.5, y - height * 0.5), at(x + width * 0.5, y + height * 0.5), lo, hi, color);
  };
  // The strike line: a bar and each lane's ring on it.
  {
    const double place = 1.0;
    const double half = width_at(place) * 0.5;
    draw->AddImage(game.Get(Part::kFretbar).id, at(-half, y_at(place) - 6.0), at(half, y_at(place) + 6.0),
                   ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 220));
    const Texture& collar = game.Get(Part::kCollar);
    for (int lane = 0; lane < 5; ++lane) {
      sprite(collar, lane_x(lane, place), y_at(place), lane_step * width_at(place) * 0.9, kLanes[lane]);
    }
  }
  // The notes coming down (the far ones first), their sustains under them.
  {
    struct Shown {
      double ahead;
      const Note* note;
    };
    std::vector<Shown> shown;
    for (int loop = -1; loop <= int(crossing / kLoop) + 1; ++loop) {
      const float base = std::floor(t / kLoop) * kLoop + float(loop) * kLoop;
      for (const Note& n : kNotes) {
        const double ahead = double(base + n.time) - t;
        if (ahead + n.sustain >= 0.0 && ahead <= crossing) shown.push_back({ahead, &n});
      }
    }
    std::sort(shown.begin(), shown.end(), [](const Shown& x, const Shown& y) { return x.ahead > y.ahead; });
    const Texture& sustain = game.Get(Part::kSustain);
    for (const Shown& item : shown) {
      if (item.note->sustain <= 0.0f) continue;
      const double start = std::max(0.0, item.ahead), end = std::min(crossing, item.ahead + item.note->sustain);
      if (end <= start) continue;
      // The bar's cross-section (a row of its texture's lower half) along the lane, in parts that follow the projection.
      constexpr int kParts = 12;
      for (int i = 0; i < kParts; ++i) {
        const double pa = place_of(end - (end - start) * i / kParts), pb = place_of(end - (end - start) * (i + 1) / kParts);
        const double xa = lane_x(item.note->lane, pa), xb = lane_x(item.note->lane, pb);
        const double wa = 0.06 * width_at(pa), wb = 0.06 * width_at(pb);
        draw->AddImageQuad(sustain.id, at(xa - wa, y_at(pa)), at(xa + wa, y_at(pa)), at(xb + wb, y_at(pb)),
                           at(xb - wb, y_at(pb)), ImVec2(0, 0.8f), ImVec2(1, 0.8f), ImVec2(1, 0.8f), ImVec2(0, 0.8f),
                           kLanes[item.note->lane]);
      }
    }
    const Texture& body = game.Get(Part::kGemColor);
    const Texture& rim = game.Get(Part::kGemBase);
    const Texture& cap = game.Get(Part::kGemCap);
    for (const Shown& item : shown) {
      if (item.ahead < 0.0) continue;
      const double place = place_of(item.ahead);
      const double width = lane_step * width_at(place) * 0.85;
      const double x = lane_x(item.note->lane, place), y = y_at(place);
      sprite(body, x, y, width, kLanes[item.note->lane]);
      sprite(rim, x, y, width, IM_COL32_WHITE);
      sprite(cap, x, y, width, IM_COL32_WHITE);
    }
  }
  draw->PopClipRect();
}

}  // namespace launcher::highway
