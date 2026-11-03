// The launcher's settings: the game's base settings and the options file (game::kConfigFile, game::kOptionsFile),
// the monitors, and the resolutions the game and NativeXRenderer will use. Ported from the Win32 launcher it
// replaces; the file it writes keeps that launcher's keys, which the game loads at startup (src/options.cpp),
// plus the keyboard's, and the game's own options (game.h).

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "game.h"

namespace launcher {

// The game always renders its frame (game.h; its Direct3D presentation parameters are hard-coded; the console
// video mode only drives the output scaler): resolution is raised with the SDK render scale
// (draw_resolution_scale_x/y, integers, per axis), or by NativeXRenderer at any size.
constexpr int kGuestWidth = game::kFrameWidth;
constexpr int kGuestHeight = game::kFrameHeight;
constexpr int kMaxScale = 3;

using Size = std::pair<int, int>;  // width, height

enum Display : int { kFullscreen, kBorderless, kWindow };

// NativeXRenderer's antialiasing on the finished scene (native_x_renderer_antialiasing, D41): SMAA, FXAA or none. With
// a temporal upscaler (below) the upscaler's mode is written there instead.
enum NativeAntialiasing : int { kSmaa, kFxaa, kAaOff, kNativeAntialiasingCount };
inline constexpr const char* kNativeAntialiasingNames[] = {"smaa", "fxaa", "off"};

// NativeXRenderer's upscaling (Settings > Video > Upscaling, launcher 3.9), the choices other PC games give:
// - off: the image at the internal resolution, stretched bilinearly into the window (native_x_renderer_scaling bilinear);
// - AMD FSR 1, NVIDIA Image Scaling: the finished image enlarged into the window (D115: fsr, nis), from an internal
//   resolution the quality sets (native_x_renderer_resolution_scale);
// - AMD FSR 3.1, NVIDIA DLSS, where the game marks its views (game::kHasDlaa): the 3D view rebuilt from several frames,
//   its antialiasing too (D47-D49, D116), at the screen's resolution; the quality is their mode
//   (native_x_renderer_antialiasing: fsr_native / dlaa, fsr_quality / dlss_quality...). DLSS needs an NVIDIA RTX GPU;
//   elsewhere NativeXRenderer falls back to SMAA.
enum Upscaler : int { kUpscaleOff, kUpscaleFsr1, kUpscaleNis, kUpscaleFsr3, kUpscaleDlss, kUpscalerCount };
inline constexpr bool IsTemporal(int upscaler) { return upscaler == kUpscaleFsr3 || upscaler == kUpscaleDlss; }
inline constexpr bool IsSpatial(int upscaler) { return upscaler == kUpscaleFsr1 || upscaler == kUpscaleNis; }

// The upscaling's quality: how much smaller the image is drawn, per axis (AMD's FSR ratios; NVIDIA's NIS and DLSS
// presets are the same within a few pixels). Native only for FSR 3.1 and DLSS (their antialiasing alone: Native AA,
// DLAA); ultra quality only for FSR 1 and NIS. FSR 3.1's and DLSS's other modes are experimental (D53).
enum UpscaleQuality : int {
  kQualityNative,
  kQualityUltra,
  kQualityQuality,
  kQualityBalanced,
  kQualityPerformance,
  kQualityUltraPerformance,
  kUpscaleQualityCount
};
inline constexpr double kUpscaleRatios[] = {1.0, 1.3, 1.5, 1.7, 2.0, 3.0};
// launcher_upscaling_quality's values, and the temporal modes' suffixes (fsr_quality, dlss_balanced...).
inline constexpr const char* kUpscaleQualityNames[] = {"native",  "ultra_quality", "quality",
                                                       "balanced", "performance",   "ultra_performance"};

// The sharpening (native_x_renderer_sharpness, D115), after the HUD: off, low, medium (what AMD and NVIDIA recommend)
// or high. Each filter's own values: FSR 1's RCAS (2 × (1 - s) stops: 1, 0.2 as AMD's default, 0), NIS's (0.5 its
// default), and AMD CAS on the image otherwise (off, FSR 3.1, DLSS).
enum Sharpness : int { kSharpnessOff, kSharpnessLow, kSharpnessMedium, kSharpnessHigh, kSharpnessCount };
inline constexpr double kSharpnessFsr1[] = {0.0, 0.5, 0.9, 1.0};
inline constexpr double kSharpnessNis[] = {0.0, 0.25, 0.5, 0.8};
inline constexpr double kSharpnessCas[] = {0.0, 0.3, 0.6, 1.0};

// The keyboard's controls: the SDK's keybind_* cvars (thirdparty/rexglue-sdk/src/input/mnk), in the Keyboard
// page's order. Each is a comma-separated list of key names; the page edits the first two (primary and
// secondary) and keeps any others.
enum class Bind : int {
  kLStickUp,
  kLStickDown,
  kLStickLeft,
  kLStickRight,
  kLStickPress,
  kRStickUp,
  kRStickDown,
  kRStickLeft,
  kRStickRight,
  kRStickPress,
  kA,
  kB,
  kX,
  kY,
  kLeftShoulder,
  kRightShoulder,
  kLeftTrigger,
  kRightTrigger,
  kDpadUp,
  kDpadDown,
  kDpadLeft,
  kDpadRight,
  kStart,
  kBack,
  kCount
};

struct BindInfo {
  const char* cvar;
  const char* sdk_default;  // the SDK's default, as REXCVAR_DEFINE_STRING gives it
};
extern const BindInfo kBinds[int(Bind::kCount)];

struct Options {
  int monitor = 1;  // cvar monitor: 1 = primary, in SDL order
  int display = kBorderless;
  Size window{kGuestWidth, kGuestHeight};  // window size in physical pixels
  int scale = 0;                           // 0 = automatic; 1..kMaxScale = chosen
  int frame_rate = game::kFrameRateDefault;  // game::kFrameRateCvar's value (a game::kFrameRates value)
  bool no_msaa = false;                    // game::kNoMsaaCvar (src/msaa.cpp)
  int anisotropic = 5;                     // anisotropic_override (SDK): 5 = 16x, 4 = 8x, 3 = 4x, 2 = 2x, 0 = off
  bool vulkan = false;                     // gpu_backend = "vulkan" (otherwise "d3d12")
  // V-Sync: frames shown once, never torn (NativeXRenderer's present mode "vsync", D107: FIFO; the SDK's presenters
  // without their tearing modes; the game's vertical blank following the monitor's, vblank_follows_display), or each
  // shown as it is ready ("immediate": IMMEDIATE, tearing).
  bool vsync = true;
  bool native = game::kHasNativeXRenderer && game::kNativeXRendererDefault;  // native_x_renderer: NativeXRenderer draws
  // native_x_renderer_antialiasing without a temporal upscaler: a NativeAntialiasing.
  int native_aa = kSmaa;
  // The image's aspect ratio (native_x_renderer_fill_screen and native_x_renderer_aspect, game::kUltrawideCvar): 0 = the
  // screen's, which it fills; kAspect16x9 = the game's own (bars on a wider screen); another (kAspect21x9,
  // kAspect32x9) with NativeXRenderer only, with bars where the screen is narrower or wider.
  double native_aspect = 0.0;
  int native_height = 0;    // native_x_renderer_resolution_scale × the frame's height without upscaling: the internal
                            // resolution's height; 0 = automatic
  int upscaler = kUpscaleOff;              // an Upscaler
  int upscale_quality = kQualityQuality;   // an UpscaleQuality (launcher_upscaling_quality)
  int sharpness = kSharpnessOff;           // a Sharpness
  // native_x_renderer_shadow_scale (NativeXRenderer's D92): 0 = the shadow maps like everything else, 1 = at least
  // twice the game's resolution (2), 2 = at least four times (4).
  int shadow_quality = 0;
  // The game's own options (game.h): its language (an index of game::kLanguages: user_language and its variant
  // cvar) and its switches. game::kUltrawideCvar follows native_aspect (the image format).
  int language = 0;
  std::array<bool, game::kToggles.size()> toggles{};
  // The sound through WASAPI in exclusive mode (the SDK's audio_wasapi_exclusive, recomp change 64): less delay, and
  // other programs silent on that device while the game runs.
  bool audio_exclusive = false;
  // The SDK's developer keys (debug_hotkeys: F3 the debug overlay, F4 its settings window, ` the console); off, those
  // keys do nothing in the game. A key bound to a control plays it either way.
  bool debug_hotkeys = false;

  // Keyboard and mouse: the SDK's controller emulation (mnk_mode, mnk_mouse, mnk_sensitivity, keybind_*).
  bool mnk = false;
  bool mnk_mouse = false;
  // The keyboard's frets strum as they are pressed (game::kKeyboardTapTarget: the SDK's mnk_tap_target).
  bool keyboard_tap = false;
  // The same for every controller playing guitar (game::kKeyboardTapTarget: the SDK's gamepad_tap_target).
  bool gamepad_tap = false;
  float mnk_sensitivity = 1.0f;
  std::array<std::string, int(Bind::kCount)> binds;

  // The controllers' buttons moved to other Xbox 360 controls: the SDK's gamepad_remap (recomp change 36: one layout
  // per type of controller, as launchers before 3.4 wrote them) and gamepad_device_remap (change 59: one per
  // controller, by its serial number or GUID, applied after the type's), edited by the Controls tab
  // (page_controls.cpp).
  std::string gamepad_remap;
  std::string device_remap;
  // The whammy bars calibrated on the Controls tab (game::kHasWhammy): the SDK's gamepad_whammy_calibration (change
  // 60), "controller=axis:rest:full" comma-separated, a controller by its serial number or GUID.
  std::string whammy_calibration;

  // Singing (game::kHasMicrophones, the Controls tab): the recording device of each of the game's USB microphones
  // (the SDK's usb_microphone_1 and _2, recomp change 55: a device's name, kSystemMicrophone for the system's default,
  // "" for none) and the channel of it the game hears (usb_microphone_1_channel and _2: 0 = all of them mixed, n =
  // channel n); each controller's part (the SDK's gamepad_device_subtypes, change 56: "serial or GUID=subtype",
  // comma-separated; a controller not in it plays the game's instrument).
  std::array<std::string, 2> microphones;
  std::array<int, 2> microphone_channels{};
  // usb_microphone_1_gain and _2: dB added to the recording (game::kMicrophoneGainDefault until changed).
  std::array<int, 2> microphone_gains{game::kMicrophoneGainDefault, game::kMicrophoneGainDefault};
  std::string device_subtypes;

  // The launcher's own.
  // launcher_language: the language of the launcher's texts (an id of kLauncherLanguages, texts.h); 0 = the Windows
  // display language's (Settings > Game).
  uint32_t launcher_language = 0;
  bool check_updates = true;    // launcher_check_updates
  int64_t last_update_check = 0;  // launcher_last_update_check: when a check last answered (Unix time); 0 = never
  bool ui_fullscreen = true;    // launcher_ui_fullscreen: the launcher's window (fullscreen by default)
  bool ui_maximized = false;    // launcher_ui_maximized
  Size ui_size{0, 0};           // launcher_ui_width/height, in pixels at 100 % scaling; 0 = the default
};

// usb_microphone_1's value for the system's default recording device.
inline constexpr const char* kSystemMicrophone = "default";

// The defaults of each page's "Reset" (the game's own, src/startup.cpp; the SDK's for the keyboard).
void ResetVideo(Options& o);
void ResetGraphics(Options& o);
void ResetGame(Options& o);
void ResetKeyboard(Options& o);

// --- The game's base settings and the options file ---

// Minimal TOML reader ("key = value"; [x] tables are flattened as x_key, like the SDK does).
using Config = std::map<std::string, std::string>;
void ReadToml(const std::filesystem::path& path, Config& config);

// `primary_dpi`: the SDK converts window_width/height with the primary monitor's DPI.
Options OptionsFromConfig(const Config& c, unsigned primary_dpi);

// --- Monitors ---

struct Monitor {
  std::string name;  // monitor name (EDID), UTF-8; empty if Windows does not provide it
  bool primary = false;
  Size size{0, 0};  // current mode, in physical pixels
  int work_left = 0, work_top = 0, work_right = 0, work_bottom = 0;  // work area (without the taskbar)
  unsigned dpi = 96;
  std::vector<Size> modes;  // 32-bit modes, unique, largest first
  int refresh = 60;          // the current mode's refresh rate, Hz
};

// Monitors in SDL order (the one the monitor cvar uses): the primary first, then the rest in
// EnumDisplayMonitors order (SDL_windowsmodes.c, WIN_AddDisplays).
std::vector<Monitor> EnumerateMonitors();

// The frame rates the monitor reaches (game::kFrameRates, those above 60 only up to its refresh rate).
std::vector<game::FrameRate> FrameRates(const Monitor& monitor);
// The Windows display language's entry of game::kLanguages, or 0.
int SystemGameLanguage();

// Window sizes that fit (frame included) in the monitor work area: its modes from 1280x720 up,
// 1280x720 always, plus keep (the saved size) if it is not in the list.
std::vector<Size> WindowSizes(const Monitor& m, Size keep);

// 16:9 aspect ratio within 1 % (1366x768 and 1360x768 count).
bool Is16x9(Size size);

// Render scale per axis: the smallest one that covers (with 10 % tolerance) the 16:9 image inside
// the output (the presenter letterboxes other aspect ratios).
Size AutoScale(Size output);

// The image formats the Format row offers: the game's own, the usual ultrawide screen's (3440x1440), the super
// ultrawide one's (5120x1440). The wide ones as the options file writes them (4 decimals), so that the render size
// computed here is the renderer's, before and after the file is read again.
inline constexpr double kAspect16x9 = 16.0 / 9.0;
inline constexpr double kAspect21x9 = 2.3889;
inline constexpr double kAspect32x9 = 3.5556;

// Two aspect ratios within 2 % (2560x1080 is 21:9 as 3440x1440 is).
bool SameAspect(double a, double b);
// The image's aspect ratio on `output` for Options::native_aspect: the output's for 0.
double ImageAspect(Size output, double chosen);
// An aspect ratio's usual name: "16:9", "21:9", "16:10"... (the nearest within 4 %), or "2.10:1".
std::string AspectName(double aspect);

// NativeXRenderer's render size (D37, D43, D54, D104), as the renderer computes it from its swap chain, for an image
// of `image_aspect` (ImageAspect).
Size NativeRenderSize(Size output, double image_aspect, int height);

// The internal resolutions NativeXRenderer offers, as heights (D54): 0 (automatic), then lower ones.
std::vector<int> NativeHeights(Size output, double image_aspect, int keep);

// The qualities an upscaler offers (UpscaleQuality values): FSR 3.1 and DLSS native and their four modes; FSR 1 and NIS
// those whose internal resolution differs, down to the lowest NativeXRenderer draws (the game's 720 lines).
std::vector<int> UpscaleQualities(int upscaler, Size output, double image_aspect);
// A quality the upscaler offers: the given one, or the nearest (native <-> ultra quality, the lowest offered).
int FitUpscaleQuality(int upscaler, int quality, Size output, double image_aspect);
// The internal resolution's height FSR 1 and NIS draw at for a quality (native_x_renderer_resolution_scale × 720).
int SpatialHeight(Size output, double image_aspect, int quality);
// The size FSR 3.1 or DLSS draw the 3D view at for a quality, from the screen's internal resolution.
Size TemporalInputSize(Size output, double image_aspect, int quality);
// The sharpening's native_x_renderer_sharpness for an upscaler, and back (the nearest step; a negative value, the
// filter's own default, is medium for FSR 1 and NIS and off otherwise).
double SharpnessValue(int upscaler, int sharpness);
int SharpnessFromValue(int upscaler, double value);

// --- The settings being edited ---

struct Settings {
  Options o;
  std::vector<Monitor> monitors;
  std::vector<Size> window_sizes;  // of the chosen monitor (physical pixels)
  unsigned primary_dpi = 96;
  bool has_vulkan = false;  // vulkan-1.dll present (ships with the video driver)
  std::filesystem::path options_path;

  const Monitor& CurrentMonitor() const;
  Size OutputSize() const;
  Size EffectiveScale() const;
  // After the monitor changed: its window sizes, and a size it does not offer becomes 1280x720.
  void MonitorChanged();
  // A frame rate the monitor does not reach goes back to the game's default.
  void FitFrameRate();
};

// Reads the game's base settings and the options file next to the executable, and what the machine offers.
Settings LoadSettings(const std::filesystem::path& exe_dir);

// Writes the options file. False if it could not be written.
bool SaveOptions(const Settings& s);

}  // namespace launcher
