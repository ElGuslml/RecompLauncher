// The launcher's look (ui.h).

#include "ui/ui.h"

#include "game.h"

#include "resources.h"
#include "ui/gfx.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <utility>
#include <tuple>
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#endif
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace ui {
namespace {

float g_scale = 1.0f;
bool g_capture_first = false;
ImGuiID g_first_item = 0;
ImGuiID g_focus_request = 0;
bool g_focus_request_show = false;
int g_focus_request_frames = 0;  // frames since asked: a request whose item never shows is dropped
ImFont* g_fonts[int(Font::kCount)] = {};
NSVGrasterizer* g_rasterizer = nullptr;

ImFont* FontOf(const Type& type) { return g_fonts[int(type.font)]; }

// A font's vertical metrics, in em. `file_height` is the embedded file's ascent minus its descent: ImGui sizes a font
// by it, so a CSS font size of 1 em is that many pixels for ImGui. `ascent` and `height` are the mockup's: the line box
// of "line-height: normal" and the baseline in it, as the browser laid the mockup out with Google Fonts' files.
// Source Sans 3: Adobe's static files have 1000 and -326 of 1000 units, Google Fonts' 1024 and -400. Oswald: 1193 and
// -289 in both.
struct Metrics {
  float file_height;
  float ascent;
  float height;
};
constexpr Metrics kSourceSans{1.326f, 1.024f, 1.424f};
constexpr Metrics kOswald{1.482f, 1.193f, 1.482f};
const Metrics& MetricsOf(const Type& type) { return type.font >= Font::kDisplayMedium ? kOswald : kSourceSans; }
float FontPixels(const Type& type) { return Px(type.size) * MetricsOf(type).file_height; }

// The paths of each icon, in a 24-unit box (the mockup's, which follow Lucide's style; drawn here
// from scratch where the mockup had none: the mouse and the keyboard).
constexpr const char* kIconBodies[int(Icon::kCount)] = {
    /* kNone */ "",
    /* kPlay */ "<path d='M7 4l13 8-13 8z'/>",
    /* kTrophy */
    "<path d='M8 21h8'/><path d='M12 17v4'/><path d='M7 4h10v5a5 5 0 0 1-10 0z'/>"
    "<path d='M17 6h3v1a4 4 0 0 1-3.5 4'/><path d='M7 6H4v1a4 4 0 0 0 3.5 4'/>",
    /* kGamepad */
    "<rect x='2' y='6' width='20' height='12' rx='6'/><path d='M6 12h4'/><path d='M8 10v4'/>"
    "<path d='M15 11h.01'/><path d='M18 13h.01'/>",
    /* kSliders */
    "<path d='M4 6h9'/><path d='M19 6h1'/><path d='M4 12h3'/><path d='M13 12h7'/><path d='M4 18h11'/>"
    "<circle cx='16' cy='6' r='2.5'/><circle cx='10' cy='12' r='2.5'/><circle cx='18' cy='18' r='2.5'/>",
    /* kRefresh */ "<path d='M20 12a8 8 0 1 1-2.4-5.7'/><path d='M20 4v5h-5'/>",
    /* kReset */ "<path d='M4 12a8 8 0 1 0 2.4-5.7'/><path d='M4 4v5h5'/>",
    /* kHeart */ "<path d='M12 20.5s-8-4.6-8-10.6A4.4 4.4 0 0 1 12 7.3a4.4 4.4 0 0 1 8 2.6c0 6-8 10.6-8 10.6z'/>",
    /* kBrand */ launcher::game::kBrandIcon,
    /* kCheck */ "<path d='M5 12l5 5 9-10'/>",
    /* kArrowRight */ "<path d='M5 12h14'/><path d='M13 6l6 6-6 6'/>",
    /* kArrowLeft */ "<path d='M19 12H5'/><path d='M11 6l-6 6 6 6'/>",
    /* kArrowUp */ "<path d='M12 19V5'/><path d='M6 11l6-6 6 6'/>",
    /* kArrowDown */ "<path d='M12 5v14'/><path d='M6 13l6 6 6-6'/>",
    /* kChevronDown */ "<path d='M6 9l6 6 6-6'/>",
    /* kLock */ "<rect x='5' y='11' width='14' height='10' rx='2'/><path d='M8 11V8a4 4 0 0 1 8 0v3'/>",
    /* kLogOut */ "<path d='M9 20H6a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h3'/><path d='M16 16l4-4-4-4'/><path d='M20 12H9'/>",
    /* kDisc */ "<circle cx='12' cy='12' r='9'/><circle cx='12' cy='12' r='2.5'/>",
    /* kFolder */ "<path d='M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z'/>",
    /* kDrive */ "<rect x='3' y='13' width='18' height='7' rx='2'/><path d='M5 13l2-8h10l2 8'/><path d='M17 16.5h.01'/>",
    /* kUpload */ "<path d='M12 15V3'/><path d='M7 8l5-5 5 5'/><path d='M4 15v4a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-4'/>",
    /* kAlert */ "<circle cx='12' cy='12' r='9'/><path d='M12 7v6'/><path d='M12 16.5h.01'/>",
    /* kMouse */ "<rect x='6' y='3' width='12' height='18' rx='6'/><path d='M12 7v4'/>",
    /* kKeyboard */
    "<rect x='2' y='6' width='20' height='12' rx='2'/><path d='M6 10h.01'/><path d='M10 10h.01'/>"
    "<path d='M14 10h.01'/><path d='M18 10h.01'/><path d='M7 14h10'/>",
    /* kPsCross */ "<path d='M6.5 6.5l11 11'/><path d='M17.5 6.5l-11 11'/>",
    /* kPsCircle */ "<circle cx='12' cy='12' r='6.5'/>",
    /* kPsSquare */ "<rect x='6' y='6' width='12' height='12' rx='1'/>",
    /* kPsTriangle */ "<path d='M12 5.5l7 12.5H5z'/>",
    /* kPackage */
    "<path d='M12 3l8.5 4.5v9L12 21l-8.5-4.5v-9z'/><path d='M3.5 7.5L12 12l8.5-4.5'/><path d='M12 12v9'/>"
    "<path d='M7.8 5.3l8.5 4.6'/>",
    /* kTrash */
    "<path d='M4 7h16'/><path d='M10 11v6'/><path d='M14 11v6'/><path d='M6 7l1 12a2 2 0 0 0 2 2h6a2 2 0 0 0 2-2l1-12'/>"
    "<path d='M9 7V4h6v3'/>",
    /* kGuitar: the 2026-10-28 mockup's, a guitar tilted 40 degrees */
    "<g transform='rotate(40 12 12)'><path d='M11 13C10.4 11.6 9.6 10.2 8.6 10.4C7.4 10.6 7.6 12.6 7.9 14.2C8.1 15.4 6.6 16.6 "
    "6.7 19C6.8 21.6 9.2 23.4 12 23.4S17.3 21.8 17.3 19.2C17.3 16.8 15.9 15.8 16.1 14.6C16.3 13.4 16.6 11.9 15.6 11.6C14.6 "
    "11.3 13.6 12.4 13 13.2M11 13V6.5M13 13.2V6.5M11 6.5V1.7Q11 1 11.7 1.1L13.3 1.6Q14 1.9 13.8 2.6L13 6.5M11 2.6H9.8M11 "
    "4H9.8M11 5.4H9.8M10.2 16.2h3.6M10.2 18.4h3.6'/></g>",
    /* kDrum */
    "<ellipse cx='12' cy='9' rx='8' ry='3'/><path d='M4 9v7c0 1.7 3.6 3 8 3s8-1.3 8-3V9'/><path d='M3 2l7 6'/>"
    "<path d='M21 2l-7 6'/>",
    /* kCamera */
    "<path d='M4 8h3l2-3h6l2 3h3v11H4z'/><circle cx='12' cy='13' r='3.5'/>",
    /* kClose */
    "<path d='M6 6l12 12'/><path d='M18 6L6 18'/>",
    /* kChevronUp */
    "<path d='M7 15l5-5 5 5'/>",
    /* kPencil */
    "<path d='M4 20h4L19 9l-4-4L4 16z'/><path d='M13.5 6.5l4 4'/>",
    /* kPin */
    "<path d='M9 4h6l-1 6 3 3v2H7v-2l3-3z'/><path d='M12 15v6'/>",
    /* kInfo */
    "<circle cx='12' cy='12' r='9'/><path d='M12 11v6'/><path d='M12 7.5v.5'/>",
    /* kPlus */
    "<path d='M12 5v14'/><path d='M5 12h14'/>",
    /* kClock */
    "<circle cx='12' cy='12' r='9'/><path d='M12 7v5l3 2'/>",
    /* kFlame */
    "<path d='M12 3c1 3 4 5 5 8a5.5 5.5 0 1 1-11 1c0-2 1-3.5 2-4.5 0 2 1 3 2 3-1-3 1-5.5 2-7.5z'/>",
    /* kChevronRight */ "<path d='M9 6l6 6-6 6'/>",
    /* kChevronLeft */ "<path d='M15 6l-6 6 6 6'/>",
};

struct Picture {
  ImTextureID texture = ImTextureID_Invalid;
  int width = 0;
  int height = 0;
};

// Icons by (icon, pixel size, stroke in hundredths, filled).
std::map<std::tuple<int, int, int, bool>, Picture> g_icons;
// SVG pictures (avatars) by (key, pixel size).
std::map<std::pair<std::string, int>, Picture> g_svgs;
// Stroke icons given by their SVG (other games' marks) by (key, pixel size, stroke in hundredths).
std::map<std::tuple<std::string, int, int>, Picture> g_svg_icons;

struct ArtCache {
  Picture picture;
  std::chrono::steady_clock::time_point drawn_at;
};
ArtCache g_art[int(Art::kCount)];

// Rasterizes SVG text into a width x height picture: scaled by `scale`, moved by (tx, ty) pixels.
Picture Rasterize(std::string svg, int width, int height, float scale, float tx, float ty) {
  Picture picture;
  if (width <= 0 || height <= 0) {
    return picture;
  }
  if (!g_rasterizer) {
    g_rasterizer = nsvgCreateRasterizer();
  }
  NSVGimage* image = nsvgParse(svg.data(), "px", 96.0f);
  if (!image) {
    return picture;
  }
  std::vector<uint8_t> pixels(size_t(width) * size_t(height) * 4);
  nsvgRasterize(g_rasterizer, image, tx, ty, scale, pixels.data(), width, height, width * 4);
  nsvgDelete(image);
  picture.texture = gfx::CreateTexture(pixels.data(), width, height);
  picture.width = width;
  picture.height = height;
  return picture;
}

void Release(Picture& picture) {
  gfx::DestroyTexture(picture.texture);
  picture = {};
}

// Decodes the UTF-8 character at `s`; returns its length.
int NextChar(const char* s, const char* end, unsigned int* c) { return ImTextCharFromUtf8(c, s, end); }

ImDrawList* List() { return ImGui::GetWindowDrawList(); }

}  // namespace

ImU32 Fade(ImU32 color, float alpha) {
  uint32_t a = (color >> IM_COL32_A_SHIFT) & 0xFF;
  a = uint32_t(std::clamp(float(a) * alpha, 0.0f, 255.0f) + 0.5f);
  return (color & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
}

ImU32 Brighten(ImU32 color) {
  auto channel = [&](int shift) {
    uint32_t v = (color >> shift) & 0xFF;
    return std::min<uint32_t>(255, uint32_t(float(v) * 1.15f + 0.5f)) << shift;
  };
  return (color & IM_COL32_A_MASK) | channel(IM_COL32_R_SHIFT) | channel(IM_COL32_G_SHIFT) |
         channel(IM_COL32_B_SHIFT);
}

void SetScale(float pixels_per_dp) { g_scale = pixels_per_dp; }
float Scale() { return g_scale; }

float Radius(float dp) { return Px(dp * col::kRoundness); }

std::string_view Resource(int id) {
  HMODULE module = GetModuleHandleW(nullptr);
  HRSRC info = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));  // RT_RCDATA
  if (!info) {
    return {};
  }
  HGLOBAL data = LoadResource(module, info);
  const void* bytes = data ? LockResource(data) : nullptr;
  return bytes ? std::string_view(static_cast<const char*>(bytes), SizeofResource(module, info))
               : std::string_view();
}

// --- Type ---

// A font of Windows (C:\Windows\Fonts), mapped into memory for the program's life: only the pages ImGui reads
// are loaded. Empty if it is not there.
std::string_view MapWindowsFont(const wchar_t* name) {
  wchar_t path[MAX_PATH];
  UINT length = GetWindowsDirectoryW(path, MAX_PATH);
  if (length == 0 || length + 8 + wcslen(name) >= MAX_PATH) {
    return {};
  }
  wcscat_s(path, L"\\Fonts\\");
  wcscat_s(path, name);
  HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return {};
  }
  LARGE_INTEGER size = {};
  HANDLE mapping = GetFileSizeEx(file, &size) ? CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr) : nullptr;
  CloseHandle(file);
  const void* view = mapping ? MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0) : nullptr;
  if (mapping) {
    CloseHandle(mapping);  // the view keeps it
  }
  return view ? std::string_view(static_cast<const char*>(view), size_t(size.QuadPart)) : std::string_view();
}

// Windows' fonts for the scripts Source Sans 3 does not have, which the names of the game's languages (game.h, in
// their own words) use: merged after it into each font, in this order.
std::vector<std::string_view> FallbackFonts() {
  bool han = false, kana = false, hangul = false, cyrillic = false;
  bool traditional = false, simplified = false;
  for (const launcher::game::Language& language : launcher::game::kLanguages) {
    traditional = traditional || language.id == 8;
    simplified = simplified || language.id == 10;
    for (const char* s = language.name; *s;) {
      unsigned int c;
      s += NextChar(s, nullptr, &c);
      han = han || (c >= 0x4E00 && c <= 0x9FFF);
      kana = kana || (c >= 0x3040 && c <= 0x30FF);
      hangul = hangul || (c >= 0xAC00 && c <= 0xD7AF) || (c >= 0x1100 && c <= 0x11FF) || (c >= 0x3130 && c <= 0x318F);
      cyrillic = cyrillic || (c >= 0x0400 && c <= 0x04FF);
    }
  }
  std::vector<std::string_view> fonts;
  auto add = [&](std::initializer_list<const wchar_t*> names) {
    for (const wchar_t* name : names) {
      if (std::string_view font = MapWindowsFont(name); !font.empty()) {
        fonts.push_back(font);
        return;
      }
    }
  };
  if (kana || (han && !traditional && !simplified)) add({L"YuGothM.ttc", L"msgothic.ttc"});
  if (han && traditional) add({L"msjh.ttc", L"mingliu.ttc"});
  if (han && simplified) add({L"msyh.ttc", L"simsun.ttc"});
  if (hangul) add({L"malgun.ttf", L"gulim.ttc"});
  if (cyrillic) add({L"segoeui.ttf", L"arial.ttf"});
  return fonts;
}

bool LoadFonts() {
  ImGuiIO& io = ImGui::GetIO();
  constexpr int kIds[int(Font::kCount)] = {IDR_FONT_REGULAR,         IDR_FONT_MEDIUM,
                                           IDR_FONT_SEMIBOLD,        IDR_FONT_BOLD,
                                           IDR_FONT_DISPLAY_MEDIUM,  IDR_FONT_DISPLAY_SEMIBOLD,
                                           IDR_FONT_DISPLAY_BOLD};
  constexpr const char* kNames[int(Font::kCount)] = {"Source Sans 3 Regular", "Source Sans 3 Medium",
                                                     "Source Sans 3 SemiBold", "Source Sans 3 Bold",
                                                     "Oswald Medium",          "Oswald SemiBold",
                                                     "Oswald Bold"};
  for (int i = 0; i < int(Font::kCount); ++i) {
    std::string_view data = Resource(kIds[i]);
    if (data.empty()) {
      return false;
    }
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;  // the resource stays mapped
    // ImGui sizes a font by its ascent minus its descent (Metrics::file_height, FontPixels). (A negative size
    // would map the em square, but ImGui 1.92 then scales glyph advances by a negative factor.)
    config.SizePixels = 16.0f;
    std::snprintf(config.Name, sizeof(config.Name), "%s", kNames[i]);
    g_fonts[i] = io.Fonts->AddFontFromMemoryTTF(const_cast<char*>(data.data()), int(data.size()), 0.0f, &config);
    if (!g_fonts[i]) {
      return false;
    }
    static const std::vector<std::string_view> fallbacks = FallbackFonts();
    for (std::string_view fallback : fallbacks) {
      ImFontConfig merge;
      merge.MergeMode = true;
      merge.DstFont = g_fonts[i];
      merge.FontDataOwnedByAtlas = false;  // mapped for the program's life
      io.Fonts->AddFontFromMemoryTTF(const_cast<char*>(fallback.data()), int(fallback.size()), 0.0f, &merge);
    }
  }
  return true;
}

float LineHeight(const Type& type) { return Px(type.size) * MetricsOf(type).height; }

ImVec2 MeasureText(const Type& type, std::string_view text) {
  float size = Px(type.size);
  ImVec2 extent = FontOf(type)->CalcTextSizeA(FontPixels(type), FLT_MAX, 0.0f, text.data(), text.data() + text.size());
  if (type.tracking != 0.0f) {
    int count = 0;
    for (const char* s = text.data(); s < text.data() + text.size();) {
      unsigned int c;
      s += NextChar(s, text.data() + text.size(), &c);
      ++count;
    }
    extent.x += float(count) * type.tracking * size;
  }
  return {extent.x, LineHeight(type)};
}

void DrawLabel(const Type& type, ImVec2 pos, ImU32 color, std::string_view text, float line_height) {
  if (text.empty()) {
    return;
  }
  ImFont* font = FontOf(type);
  float size = Px(type.size);
  float pixels = FontPixels(type);
  // CSS puts the baseline at the top of the line box, plus half the leading, plus the ascent; ImGui puts it at the
  // top plus its baked (rounded) ascent.
  const Metrics& metrics = MetricsOf(type);
  float box = line_height > 0.0f ? line_height : LineHeight(type);
  float baseline = pos.y + (box - size * metrics.height) * 0.5f + size * metrics.ascent;
  ImVec2 at = Snap(ImVec2(pos.x, baseline - font->GetFontBaked(pixels)->Ascent));
  const char* begin = text.data();
  const char* end = begin + text.size();
  if (type.tracking == 0.0f) {
    List()->AddText(font, pixels, at, color, begin, end);
    return;
  }
  float spacing = type.tracking * size;
  for (const char* s = begin; s < end;) {
    unsigned int c;
    int length = NextChar(s, end, &c);
    List()->AddText(font, pixels, Snap(at), color, s, s + length);
    at.x += font->CalcTextSizeA(pixels, FLT_MAX, 0.0f, s, s + length).x + spacing;
    s += length;
  }
}

float DrawParagraph(const Type& type, ImVec2 pos, float width, float line_height, ImU32 color,
                      std::string_view text, bool draw, float align) {
  ImFont* font = FontOf(type);
  float size = Px(type.size);
  float step = line_height * size;
  const char* s = text.data();
  const char* end = s + text.size();
  float y = pos.y;
  while (s < end) {
    const char* line_end = font->CalcWordWrapPosition(FontPixels(type), s, end, width);
    if (line_end <= s) {
      line_end = s + 1;
    }
    const char* newline = std::find(s, line_end, '\n');
    if (newline < line_end) {
      line_end = newline;
    }
    if (draw) {
      std::string_view line(s, size_t(line_end - s));
      while (!line.empty() && line.back() == ' ') {
        line.remove_suffix(1);
      }
      float x = pos.x + (align > 0.0f ? (width - MeasureText(type, line).x) * align : 0.0f);
      DrawLabel(type, {x, y}, color, line, step);
    }
    y += step;
    s = line_end;
    if (s < end && *s == '\n') {
      ++s;
    } else {
      while (s < end && *s == ' ') {
        ++s;
      }
    }
  }
  return y - pos.y;
}

std::string Upper(std::string_view text) {
  if (text.empty()) {
    return {};
  }
  int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
  std::wstring wide(size_t(std::max(length, 0)), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), wide.data(), length);
  // Windows' own mapping, which knows the accented letters ("é" -> "É").
  CharUpperBuffW(wide.data(), DWORD(wide.size()));
  int bytes = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0, nullptr, nullptr);
  std::string upper(size_t(std::max(bytes, 0)), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), upper.data(), bytes, nullptr, nullptr);
  return upper;
}

std::string Title(std::string_view text) { return launcher::game::kUppercaseTitles ? Upper(text) : std::string(text); }

std::string Ellipsize(const Type& type, std::string_view text, float width) {
  if (MeasureText(type, text).x <= width) {
    return std::string(text);
  }
  std::string cut(text);
  while (!cut.empty()) {
    // Drop one whole UTF-8 character.
    size_t back = cut.size() - 1;
    while (back > 0 && (static_cast<unsigned char>(cut[back]) & 0xC0) == 0x80) {
      --back;
    }
    cut.resize(back);
    while (!cut.empty() && cut.back() == ' ') {
      cut.pop_back();
    }
    if (MeasureText(type, cut + "\xE2\x80\xA6").x <= width) {
      break;
    }
  }
  return cut + "\xE2\x80\xA6";
}

// --- Pictures ---

void DrawIcon(Icon icon, ImVec2 pos, float size, ImU32 color, float stroke, bool filled) {
  if (icon == Icon::kNone || icon >= Icon::kCount) {
    return;
  }
  int pixels = std::max(1, int(Snap(Px(size))));
  auto key = std::make_tuple(int(icon), pixels, int(stroke * 100.0f + 0.5f), filled);
  auto it = g_icons.find(key);
  if (it == g_icons.end()) {
    char header[256];
    if (filled) {
      std::snprintf(header, sizeof(header),
                    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='#FFFFFF' stroke='none'>");
    } else {
      std::snprintf(header, sizeof(header),
                    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='#FFFFFF' "
                    "stroke-width='%g' stroke-linecap='round' stroke-linejoin='round'>",
                    double(stroke));
    }
    std::string svg = std::string(header) + kIconBodies[int(icon)] + "</svg>";
    it = g_icons.emplace(key, Rasterize(std::move(svg), pixels, pixels, float(pixels) / 24.0f, 0, 0)).first;
  }
  if (it->second.texture == ImTextureID_Invalid) {
    return;
  }
  ImVec2 at = Snap(pos);
  List()->AddImage(it->second.texture, at, ImVec2(at.x + float(pixels), at.y + float(pixels)), ImVec2(0, 0),
                   ImVec2(1, 1), color);
}

// The art's size in its own units: its SVG's viewBox, read once.
static ImVec2 ArtSize(Art art, int id) {
  static ImVec2 sizes[int(Art::kCount)] = {};
  ImVec2& size = sizes[int(art)];
  if (size.x <= 0.0f || size.y <= 0.0f) {
    std::string svg(Resource(id));
    if (NSVGimage* image = nsvgParse(svg.data(), "px", 96.0f)) {
      size = ImVec2(image->width, image->height);
      nsvgDelete(image);
    }
    if (size.x <= 0.0f || size.y <= 0.0f) {
      size = ImVec2(1000.0f, 480.0f);
    }
  }
  return size;
}

void DrawArt(Art art, const ImRect& rect, ImVec2 anchor, float rounding, ImDrawFlags corners) {
  constexpr int kIds[int(Art::kCount)] = {IDR_ART_BACKGROUND};
  int width = int(Snap(rect.GetWidth()));
  int height = int(Snap(rect.GetHeight()));
  if (width <= 0 || height <= 0) {
    return;
  }
  ArtCache& cache = g_art[int(art)];
  auto now = std::chrono::steady_clock::now();
  // While the window is being resized, the last picture stretches; it is drawn again at most 10 times a
  // second.
  if ((cache.picture.width != width || cache.picture.height != height) &&
      (cache.picture.texture == ImTextureID_Invalid || now - cache.drawn_at > std::chrono::milliseconds(100))) {
    Release(cache.picture);
    ImVec2 view = ArtSize(art, kIds[int(art)]);
    float vw = view.x;
    float vh = view.y;
    float scale = std::max(float(width) / vw, float(height) / vh);
    float tx = (float(width) - vw * scale) * anchor.x;
    float ty = (float(height) - vh * scale) * anchor.y;
    cache.picture = Rasterize(std::string(Resource(kIds[int(art)])), width, height, scale, tx, ty);
    cache.drawn_at = now;
  }
  if (cache.picture.texture == ImTextureID_Invalid) {
    return;
  }
  ImVec2 a = Snap(rect.Min);
  ImVec2 b = Snap(rect.Max);
  if (rounding > 0.0f) {
    List()->AddImageRounded(cache.picture.texture, a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE,
                            Radius(rounding), corners);
  } else {
    List()->AddImage(cache.picture.texture, a, b);
  }
}

void DrawSvg(std::string_view key, std::string_view body, float view, const ImRect& rect, bool circle) {
  int pixels = std::max(1, int(Snap(std::min(rect.GetWidth(), rect.GetHeight()))));
  auto cache_key = std::make_pair(std::string(key), pixels);
  auto it = g_svgs.find(cache_key);
  if (it == g_svgs.end()) {
    char header[160];
    std::snprintf(header, sizeof(header), "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 %g %g'>",
                  double(view), double(view));
    std::string svg = std::string(header) + std::string(body) + "</svg>";
    it = g_svgs.emplace(cache_key, Rasterize(std::move(svg), pixels, pixels, float(pixels) / view, 0, 0)).first;
  }
  DrawTexture(it->second.texture, ImRect(rect.Min, ImVec2(rect.Min.x + float(pixels), rect.Min.y + float(pixels))),
              circle);
}

void DrawIconSvg(std::string_view key, std::string_view body, ImVec2 pos, float size, ImU32 color, float stroke) {
  int pixels = std::max(1, int(Snap(Px(size))));
  auto cache_key = std::make_tuple(std::string(key), pixels, int(stroke * 100.0f + 0.5f));
  auto it = g_svg_icons.find(cache_key);
  if (it == g_svg_icons.end()) {
    char header[256];
    std::snprintf(header, sizeof(header),
                  "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24' fill='none' stroke='#FFFFFF' "
                  "stroke-width='%g' stroke-linecap='round' stroke-linejoin='round'>",
                  double(stroke));
    std::string svg = std::string(header) + std::string(body) + "</svg>";
    it = g_svg_icons.emplace(cache_key, Rasterize(std::move(svg), pixels, pixels, float(pixels) / 24.0f, 0, 0)).first;
  }
  if (it->second.texture == ImTextureID_Invalid) {
    return;
  }
  ImVec2 at = Snap(pos);
  List()->AddImage(it->second.texture, at, ImVec2(at.x + float(pixels), at.y + float(pixels)), ImVec2(0, 0),
                   ImVec2(1, 1), color);
}

void Ring(ImVec2 center, float radius_dp, float thickness_dp, float fraction, ImU32 track, ImU32 fill) {
  constexpr float kPi = 3.14159265f;
  float radius = Px(radius_dp);
  float thickness = Px(thickness_dp);
  ImDrawList* draw = List();
  draw->PathArcTo(center, radius, 0.0f, 2.0f * kPi, 64);
  draw->PathStroke(track, ImDrawFlags_Closed, thickness);
  fraction = std::clamp(fraction, 0.0f, 1.0f);
  if (fraction > 0.0f) {
    float start = -0.5f * kPi;
    draw->PathArcTo(center, radius, start, start + 2.0f * kPi * fraction, std::max(8, int(64.0f * fraction)));
    draw->PathStroke(fill, ImDrawFlags_None, thickness);
  }
}

void DrawTexture(ImTextureID texture, const ImRect& rect, bool circle) {
  if (texture == ImTextureID_Invalid) {
    return;
  }
  ImVec2 a = Snap(rect.Min);
  ImVec2 b = Snap(rect.Max);
  if (circle) {
    List()->AddImageRounded(texture, a, b, ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, (b.x - a.x) * 0.5f);
  } else {
    List()->AddImage(texture, a, b);
  }
}

void DrawRoundedTexture(ImTextureID texture, const ImRect& rect, float rounding_dp) {
  if (texture == ImTextureID_Invalid) {
    return;
  }
  List()->AddImageRounded(texture, Snap(rect.Min), Snap(rect.Max), ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE,
                          Radius(rounding_dp));
}

namespace {

struct FieldLimit {
  size_t max_chars;
};

// Keeps a field's text within its characters as it is typed or pasted.
int LimitField(ImGuiInputTextCallbackData* data) {
  const size_t max_chars = static_cast<FieldLimit*>(data->UserData)->max_chars;
  size_t chars = 0;
  for (int i = 0; i < data->BufTextLen; ++i) {
    if ((static_cast<unsigned char>(data->Buf[i]) & 0xC0) != 0x80) {
      if (chars == max_chars) {
        data->DeleteChars(i, data->BufTextLen - i);
        break;
      }
      ++chars;
    }
  }
  return 0;
}

}  // namespace

bool TextField(const char* id, const ImRect& rect, std::string* value, size_t max_chars, std::string_view hint) {
  constexpr Type kType{Font::kMedium, 16};
  ImGuiID item = ImGui::GetID(id);
  bool active = ImGui::GetActiveID() == item;
  bool hovered = ImGui::IsMouseHoveringRect(rect.Min, rect.Max);
  FillRect(rect, col::kField, 8.0f);
  StrokeRect(rect, active ? col::kAccent : hovered ? col::kTextDim : col::kFieldBorder, 8.0f, active ? 1.5f : 1.0f);

  // Dear ImGui's own field, without its frame, over the text's line.
  ImFont* font = FontOf(kType);
  float pixels = FontPixels(kType);
  float padding_x = Px(14.0f);
  ImGui::PushFont(font, pixels);
  float frame_height = ImGui::GetFrameHeight();
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, ImGui::GetStyle().FramePadding.y));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_Text, col::kText);
  ImGui::PushStyleColor(ImGuiCol_TextDisabled, col::kTextDim);
  ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, Fade(col::kAccent, 0.35f));
  ImGui::PushStyleColor(ImGuiCol_NavCursor, IM_COL32(0, 0, 0, 0));
  ImGui::SetCursorScreenPos(Snap(ImVec2(rect.Min.x + padding_x, rect.Min.y + (rect.GetHeight() - frame_height) * 0.5f)));
  ImGui::SetNextItemWidth(rect.GetWidth() - padding_x * 2.0f);
  char buffer[256];
  std::snprintf(buffer, sizeof(buffer), "%s", value->c_str());
  FieldLimit limit{max_chars};
  std::string hint_text(hint);
  bool changed = ImGui::InputTextWithHint(id, hint_text.c_str(), buffer, sizeof(buffer),
                                          ImGuiInputTextFlags_CallbackEdit, LimitField, &limit);
  bool focused = ImGui::IsItemFocused() && GImGui->NavCursorVisible;
  ImGui::PopStyleColor(7);
  ImGui::PopStyleVar();
  ImGui::PopFont();
  if (focused && !active) {
    FocusRing(rect, 8.0f);
  }
  if (changed) {
    *value = buffer;
  }
  return changed;
}

void ReleasePictures() {
  for (auto& [key, picture] : g_icons) {
    Release(picture);
  }
  g_icons.clear();
  for (auto& [key, picture] : g_svgs) {
    Release(picture);
  }
  g_svgs.clear();
  for (auto& [key, picture] : g_svg_icons) {
    Release(picture);
  }
  g_svg_icons.clear();
  for (ArtCache& art : g_art) {
    Release(art.picture);
  }
}

// --- Items ---

ItemState Item(ImGuiID id, const ImRect& rect, bool enabled) {
  ItemState state;
  if (!enabled) {
    ImGui::ItemAdd(rect, 0);
    return state;
  }
  ImGuiContext& g = *GImGui;
  // A mouse-only item (NoNav) is no page's first control.
  if (g_capture_first && !g_first_item && !(g.CurrentItemFlags & ImGuiItemFlags_NoNav)) {
    g_first_item = id;
  }
  bool visible = ImGui::ItemAdd(rect, id);
  // The focus asked for this item: given now, with its window and its rectangle (ImGui moves the controller's focus
  // from that rectangle), and scrolled into view. Not while another item is held active (A still down on the tab or
  // the device that asked): ImGui would keep the focus there.
  if (g_focus_request == id && (g.ActiveId == 0 || g.ActiveId == id)) {
    g_focus_request = 0;
    ImGui::SetFocusID(id, ImGui::GetCurrentWindow());
    if (g_focus_request_show) {
      ImGui::SetNavCursorVisible(true);
    }
    ImGui::ScrollToBringRectIntoView(ImGui::GetCurrentWindow(), rect);
  }
  if (!visible) {
    return state;
  }
  state.pressed = ImGui::ButtonBehavior(rect, id, &state.hovered, &state.held);
  // Only A (or Enter, Space, the mouse) presses an item: ImGui's navigation also activates the item in focus with Y
  // (Triangle, its "input") and X (Square), which are the pages' own shortcuts (Y on Play opens Donate, not PLAY).
  if (state.pressed && (ImGui::IsKeyDown(ImGuiKey_GamepadFaceUp) || ImGui::IsKeyDown(ImGuiKey_GamepadFaceLeft)) &&
      !ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown)) {
    state.pressed = false;
  }
  state.focused = g.NavId == id && g.NavCursorVisible && g.NavWindow &&
                  (g.NavWindow == ImGui::GetCurrentWindow() || g.NavWindow->RootWindow == ImGui::GetCurrentWindow()->RootWindow);
  if (state.hovered) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  return state;
}

ItemState Item(const char* id, const ImRect& rect, bool enabled) { return Item(ImGui::GetID(id), rect, enabled); }

bool AnyGamepadButtonDown() {
  for (int key = ImGuiKey_GamepadStart; key <= ImGuiKey_GamepadR3; ++key) {
    if (ImGui::IsKeyDown(ImGuiKey(key))) {
      return true;
    }
  }
  return false;
}

void RequestFocus(ImGuiID id, bool show) {
  g_focus_request = id;
  g_focus_request_show = show;
  g_focus_request_frames = 0;
}

bool FocusRequested() { return g_focus_request != 0; }

bool PressedByNav() {
  return !ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsMouseReleased(ImGuiMouseButton_Left);
}

void CaptureFirstItem() {
  // Dropped when its item has not shown for a few frames (not counting the frames A is held).
  if (g_focus_request && GImGui->ActiveId == 0 && ++g_focus_request_frames > 3) {
    g_focus_request = 0;
  }
  g_capture_first = true;
  g_first_item = 0;
}

ImGuiID CapturedFirstItem() {
  g_capture_first = false;
  return g_first_item;
}

void FocusRing(const ImRect& rect, float rounding) {
  // The mockup's ":focus-visible { outline: 3px solid; outline-offset: 4px }": the line's middle 5.5 dp out.
  float offset = Px(5.5f);
  List()->AddRect(Snap(ImVec2(rect.Min.x - offset, rect.Min.y - offset)),
                  Snap(ImVec2(rect.Max.x + offset, rect.Max.y + offset)), col::kFocus, Radius(rounding) + offset, 0,
                  Px(3.0f));
}

// --- Shapes ---

void FillRect(const ImRect& rect, ImU32 color, float rounding_dp, ImDrawFlags corners) {
  List()->AddRectFilled(Snap(rect.Min), Snap(rect.Max), color, Radius(rounding_dp), corners);
}

void StrokeRect(const ImRect& rect, ImU32 color, float rounding_dp, float thickness_dp) {
  // ImGui strokes on the rectangle's edge: inset by half the thickness, as a CSS border is drawn inside.
  float t = std::max(1.0f, Snap(Px(thickness_dp)));
  ImVec2 a = Snap(rect.Min);
  ImVec2 b = Snap(rect.Max);
  float h = t * 0.5f - 0.5f;
  List()->AddRect(ImVec2(a.x + h, a.y + h), ImVec2(b.x - h - 1.0f, b.y - h - 1.0f), color,
                  std::max(0.0f, Radius(rounding_dp) - h), 0, t);
}

void DashedRect(const ImRect& rect, ImU32 color, float rounding_dp, float dash_dp, float gap_dp) {
  ImDrawList* list = List();
  ImVec2 a = Snap(rect.Min);
  ImVec2 b = Snap(rect.Max);
  a.x += 0.5f;
  a.y += 0.5f;
  b.x -= 0.5f;
  b.y -= 0.5f;
  list->PathRect(a, b, Radius(rounding_dp));
  std::vector<ImVec2> points(list->_Path.begin(), list->_Path.end());
  list->PathClear();
  if (points.size() < 2) {
    return;
  }
  points.push_back(points.front());
  float dash = Px(dash_dp);
  float gap = Px(gap_dp);
  float period = dash + gap;
  float travelled = 0.0f;
  float thickness = std::max(1.0f, Snap(Px(1.0f)));
  for (size_t i = 0; i + 1 < points.size(); ++i) {
    ImVec2 p = points[i];
    ImVec2 q = points[i + 1];
    float length = std::sqrt((q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y));
    float t = 0.0f;
    while (t < length) {
      float phase = std::fmod(travelled + t, period);
      float run = phase < dash ? std::min(dash - phase, length - t) : std::min(period - phase, length - t);
      if (phase < dash) {
        ImVec2 from(p.x + (q.x - p.x) * t / length, p.y + (q.y - p.y) * t / length);
        ImVec2 to(p.x + (q.x - p.x) * (t + run) / length, p.y + (q.y - p.y) * (t + run) / length);
        list->AddLine(from, to, color, thickness);
      }
      t += std::max(run, 0.01f);
    }
    travelled += length;
  }
}

// --- The controller's buttons ---

namespace {

GlyphStyle g_glyph_style = GlyphStyle::kXbox;
const char* g_view_label = "View";

const char* GlyphText(PadButton button) {
  int i = int(button);
  switch (g_glyph_style) {
    case GlyphStyle::kKeyboard: {
      static constexpr const char* kKeys[] = {"Enter", "Esc", "F", "R", "Q", "E", "P"};
      return kKeys[i];
    }
    case GlyphStyle::kPlayStation: {
      static constexpr const char* kNames[] = {"", "", "", "", "L1", "R1", ""};
      return button == PadButton::kView ? g_view_label : kNames[i];
    }
    case GlyphStyle::kNintendo: {
      // By position: the bottom button is B on a Switch controller, the right one A.
      static constexpr const char* kNames[] = {"B", "A", "Y", "X", "L", "R", "â"};
      return kNames[i];
    }
    case GlyphStyle::kXbox:
      break;
  }
  static constexpr const char* kNames[] = {"A", "B", "X", "Y", "LB", "RB", ""};
  return button == PadButton::kView ? g_view_label : kNames[i];
}

bool IsFaceButton(PadButton button) { return button <= PadButton::kY; }

Type GlyphType(float size) { return Type{Font::kBold, std::round(size * 0.46f)}; }

}  // namespace

void SetGlyphStyle(GlyphStyle style, const char* view) {
  g_glyph_style = style;
  g_view_label = view;
}

GlyphStyle CurrentGlyphStyle() { return g_glyph_style; }

float GlyphWidth(PadButton button, float size) {
  if (IsFaceButton(button) && g_glyph_style != GlyphStyle::kKeyboard) {
    return Px(size);
  }
  return std::max(Px(size), MeasureText(GlyphType(size), GlyphText(button)).x + Px(size * 0.6f));
}

void DrawGlyph(PadButton button, ImVec2 pos, float size, bool on_accent) {
  ImRect r(pos, ImVec2(pos.x + GlyphWidth(button, size), pos.y + Px(size)));
  Type type = GlyphType(size);
  const char* text = GlyphText(button);
  auto label = [&](ImU32 color) {
    DrawLabel(type, ImVec2(r.GetCenter().x - MeasureText(type, text).x * 0.5f, r.Min.y), color, text, r.GetHeight());
  };
  if (g_glyph_style == GlyphStyle::kKeyboard) {
    // A key cap.
    FillRect(r, on_accent ? col::kOnAccent : Hex(0xFFFFFF, 31), size * 0.25f);
    StrokeRect(r, on_accent ? col::kOnAccent : Hex(0xFFFFFF, 77), size * 0.25f);
    label(on_accent ? col::kAccent : col::kText);
    return;
  }
  if (!IsFaceButton(button)) {
    // A bumper or View: a pill.
    FillRect(r, on_accent ? col::kOnAccent : col::kHintBumper, size * 0.5f);
    label(on_accent ? col::kAccent : col::kOnHintBumper);
    return;
  }
  int i = int(button);
  ImVec2 center = r.GetCenter();
  float radius = r.GetHeight() * 0.5f;
  ImDrawList* draw = ImGui::GetWindowDrawList();
  if (g_glyph_style == GlyphStyle::kPlayStation) {
    // The symbol in its color on a dark disc.
    static constexpr Icon kIcons[] = {Icon::kPsCross, Icon::kPsCircle, Icon::kPsSquare, Icon::kPsTriangle};
    static constexpr ImU32 kColors[] = {Hex(0x7CB2E8), Hex(0xF07B6B), Hex(0xE6A4D7), Hex(0x5BC489)};
    draw->AddCircleFilled(center, radius, on_accent ? col::kOnAccent : Hex(0x2E2E36), 32);
    float icon = size * 0.52f;
    DrawIcon(kIcons[i], ImVec2(center.x - Px(icon) * 0.5f, center.y - Px(icon) * 0.5f), icon, kColors[i],
             size >= 24.0f ? 2.4f : 2.8f);
    return;
  }
  if (g_glyph_style == GlyphStyle::kNintendo) {
    draw->AddCircleFilled(center, radius, on_accent ? col::kOnAccent : Hex(0xE6E6EA), 32);
    label(on_accent ? col::kAccent : Hex(0x1C1C21));
    return;
  }
  // An Xbox controller's: the letter on its color; inside an accent button, the color on dark.
  const ImU32 discs[] = {col::kGlyphA, col::kGlyphB, col::kGlyphX, col::kGlyphY};
  const ImU32 letters[] = {col::kOnGlyphA, col::kOnGlyphB, col::kOnGlyphX, col::kOnGlyphY};
  draw->AddCircleFilled(center, radius, on_accent ? col::kOnAccent : discs[i], 32);
  label(on_accent ? discs[i] : letters[i]);
}

bool ButtonPressed(PadButton button) {
  static constexpr ImGuiKey kPad[] = {ImGuiKey_GamepadFaceDown, ImGuiKey_GamepadFaceRight, ImGuiKey_GamepadFaceLeft,
                                      ImGuiKey_GamepadFaceUp,   ImGuiKey_GamepadL1,        ImGuiKey_GamepadR1,
                                      ImGuiKey_GamepadBack};
  static constexpr ImGuiKey kKeys[] = {ImGuiKey_Enter, ImGuiKey_Escape, ImGuiKey_F, ImGuiKey_R,
                                       ImGuiKey_Q,     ImGuiKey_E,      ImGuiKey_P};
  int i = int(button);
  if (ImGui::IsKeyPressed(kPad[i], false)) {
    return true;
  }
  if (ImGui::GetIO().WantTextInput && button != PadButton::kB) {
    return false;
  }
  return ImGui::IsKeyPressed(kKeys[i], false);
}

// --- Widgets ---

namespace {

struct ButtonColors {
  ImU32 background;
  ImU32 border;  // 0: none
  ImU32 text;
  ImU32 icon;
};

ButtonColors ColorsOf(ButtonKind kind) {
  switch (kind) {
    case ButtonKind::kPrimary:
      return {col::kAccent, 0, col::kOnAccent, col::kOnAccent};
    case ButtonKind::kGhost:
      return {0, col::kOutline, col::kText, col::kTextMuted};
    case ButtonKind::kDanger:
      return {0, Fade(col::kError, 0.55f), col::kError, col::kError};
    case ButtonKind::kDonate:
      return {col::kDonate, 0, col::kOnDonate, col::kOnDonate};
    case ButtonKind::kDisabled:
      return {col::kTrack, 0, col::kLabel, col::kLabel};
  }
  return {};
}

}  // namespace

float ButtonWidth(const ButtonSpec& spec, std::string_view label) {
  float width = Px(spec.padding) * 2.0f + MeasureText(spec.type, label).x;
  if (spec.icon != Icon::kNone) {
    width += Px(spec.icon_size) + (label.empty() ? 0.0f : Px(spec.gap));
  }
  return width;
}

bool Button(const char* id, ImVec2 pos, const ButtonSpec& spec, std::string_view label, bool enabled, float width) {
  if (width <= 0.0f) {
    width = ButtonWidth(spec, label);
  }
  ImRect rect(pos, ImVec2(pos.x + width, pos.y + Px(spec.height)));
  ItemState state = Item(id, rect, enabled && spec.kind != ButtonKind::kDisabled);
  ButtonColors c = ColorsOf(enabled ? spec.kind : ButtonKind::kDisabled);
  if (state.hovered) {
    c = {Brighten(c.background), c.border ? Brighten(c.border) : 0, Brighten(c.text), Brighten(c.icon)};
  }
  if (c.background) {
    FillRect(rect, c.background, spec.radius);
  }
  if (c.border) {
    StrokeRect(rect, c.border, spec.radius);
  }
  float content = MeasureText(spec.type, label).x;
  if (spec.icon != Icon::kNone) {
    content += Px(spec.icon_size) + (label.empty() ? 0.0f : Px(spec.gap));
  }
  float x = rect.Min.x + (width - content) * 0.5f;
  float icon_y = rect.Min.y + (rect.GetHeight() - Px(spec.icon_size)) * 0.5f;
  if (spec.icon != Icon::kNone && !spec.icon_after) {
    DrawIcon(spec.icon, ImVec2(x, icon_y), spec.icon_size, c.icon, spec.icon_stroke, spec.icon_filled);
    x += Px(spec.icon_size) + Px(spec.gap);
  }
  DrawLabel(spec.type, ImVec2(x, rect.Min.y), c.text, label, rect.GetHeight());
  if (spec.icon != Icon::kNone && spec.icon_after) {
    x += MeasureText(spec.type, label).x + Px(spec.gap);
    DrawIcon(spec.icon, ImVec2(x, icon_y), spec.icon_size, c.icon, spec.icon_stroke, spec.icon_filled);
  }
  if (state.focused) {
    FocusRing(rect, spec.radius);
  }
  return state.pressed;
}

bool Toggle(const char* id, ImVec2 pos, bool* value, bool enabled) {
  ImRect rect(pos, ImVec2(pos.x + Px(52.0f), pos.y + Px(30.0f)));
  ItemState state = Item(id, rect, enabled);
  if (state.pressed) {
    *value = !*value;
  }
  ImU32 track = *value ? col::kAccent : col::kOutline;
  ImU32 knob = *value ? col::kOnAccent : col::kTextMuted;
  if (state.hovered) {
    track = Brighten(track);
  }
  if (!enabled) {
    track = Fade(track, 0.45f);
    knob = Fade(knob, 0.6f);
  }
  FillRect(rect, track, 15.0f);
  float knob_x = *value ? rect.Max.x - Px(27.0f) : rect.Min.x + Px(3.0f);
  FillRect(ImRect(ImVec2(knob_x, rect.Min.y + Px(3.0f)), ImVec2(knob_x + Px(24.0f), rect.Min.y + Px(27.0f))), knob,
           12.0f);
  if (state.focused) {
    FocusRing(rect, 15.0f);
  }
  return state.pressed;
}

bool Link(const char* id, ImVec2 pos, std::string_view label, float size) {
  Type type{Font::kSemiBold, size};
  ImVec2 text = MeasureText(type, label);
  float arrow = size * 0.95f;
  ImRect rect(pos, ImVec2(pos.x + text.x + Px(4.0f) + Px(arrow), pos.y + text.y));
  ItemState state = Item(id, rect, true);
  ImU32 color = state.hovered ? col::kAccentLight : col::kAccent;
  DrawLabel(type, pos, color, label);
  DrawIcon(Icon::kArrowRight, ImVec2(pos.x + text.x + Px(4.0f), pos.y + (text.y - Px(arrow)) * 0.5f + Px(0.5f)),
           arrow, color, 2.2f);
  if (state.focused) {
    FocusRing(rect, 4.0f);
  }
  return state.pressed;
}

void Progress(const ImRect& rect, float fraction, ImU32 track, ImU32 fill) {
  float radius = rect.GetHeight() * 0.5f / Scale();
  FillRect(rect, track, radius);
  fraction = std::clamp(fraction, 0.0f, 1.0f);
  if (fraction > 0.0f) {
    // overflow: hidden with a rounded track: the fill's left corners are rounded, its right edge straight.
    ImRect bar(rect.Min, ImVec2(rect.Min.x + rect.GetWidth() * fraction, rect.Max.y));
    FillRect(bar, fill, radius, fraction >= 0.999f ? 0 : ImDrawFlags_RoundCornersLeft);
  }
}

void BeginScroll(const char* id, const ImRect& rect) {
  ImGui::SetCursorScreenPos(rect.Min);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, Px(10.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, Px(5.0f));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImGui::ColorConvertU32ToFloat4(col::kOutline));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImGui::ColorConvertU32ToFloat4(col::kDashed));
  ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ImGui::ColorConvertU32ToFloat4(col::kScrollbarActive));
  ImGui::BeginChild(id, rect.GetSize(), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
}

void EndScroll() {
  ImGui::EndChild();
  ImGui::PopStyleColor(4);
  ImGui::PopStyleVar(4);
}

}  // namespace ui
