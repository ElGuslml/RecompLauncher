// The Settings tab (launcher 3.18, Gustavo's 2026-11-11 mockup "Camerino"): the title and its categories on the left
// (Game, Video, Graphics: a dot marks the current one), the category's options as flat rows on the right, each its
// value written as text (a switch for what is on or off); the row in focus grows to show its line of help, the arrows
// and its place among the values ("‹ value ›", the controller's left and right, A the next). Y opens the row's whole
// help, X resets the category. Every change is saved at once.

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

std::string SizeText(Size size) { return std::to_string(size.first) + " × " + std::to_string(size.second); }

// One option of a category: its values, the one chosen, and what choosing another does.
struct OptionRow {
  std::string id;
  std::string title;
  std::string sub;   // the short line under the title (one per value when `subs` is given)
  std::vector<std::string> subs;
  std::string help;  // the help panel's text
  std::vector<std::string> values;
  std::vector<std::string> tags;  // per value: "" or a badge ("RECOMMENDED", "EXPERIMENTAL")
  std::vector<bool> tag_accent;   // the badge on accent (recommended), else on the warning yellow
  int index = 0;
  bool locked = false;            // shown with a lock: `locked_value`, no arrows
  std::string locked_value;
  std::string group;              // a group's title above the row (the screen effects)
  bool is_switch = false;         // on (index 0) or off (index 1): drawn as a switch
  std::function<void(int)> apply;

  std::string Value() const {
    if (locked) return locked_value;
    return values.empty() ? std::string() : values[size_t(std::clamp(index, 0, int(values.size()) - 1))];
  }
  std::string Sub() const { return subs.empty() ? sub : subs[size_t(std::clamp(index, 0, int(subs.size()) - 1))]; }
  std::string Tag() const {
    return locked || tags.empty() ? std::string() : tags[size_t(std::clamp(index, 0, int(tags.size()) - 1))];
  }
};

// The mockup's ".row": 60 dp, the focused one 88 dp with 4 dp around it.
constexpr float kRowHeight = 60.0f;
constexpr float kFocusedRowHeight = 88.0f;
constexpr float kFocusedRowMargin = 4.0f;
constexpr Type kRowLabel{Font::kMedium, 18};
constexpr Type kRowLabelFocused{Font::kSemiBold, 18};
constexpr Type kRowValue{Font::kSemiBold, 18};
constexpr Type kRowValueFocused{Font::kSemiBold, 20};
constexpr Type kRowDescription{Font::kRegular, 14};
constexpr Type kGroupTitle{Font::kSemiBold, 14};

// A chevron of the focused row (the mouse's; the controller steps with left and right).
bool ArrowButton(const char* id, ImVec2 pos, bool left) {
  ImRect rect(pos, ImVec2(pos.x + Px(28.0f), pos.y + Px(28.0f)));
  ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
  ItemState state = Item(id, rect);
  ImGui::PopItemFlag();
  if (state.hovered) FillRect(rect, Fade(col::kFocus, 0.08f), 14.0f);
  DrawIcon(left ? Icon::kChevronLeft : Icon::kChevronRight, ImVec2(pos.x + Px(2.0f), pos.y + Px(2.0f)), 24.0f,
           state.hovered ? col::kText : col::kTextSoft, 1.9f);
  return state.pressed;
}

// "experimental" as a chip's text: its first letter in capitals (the five languages' words start with an ASCII one).
std::string ChipText(const std::string& text) {
  std::string chip = text;
  if (!chip.empty() && chip[0] >= 'a' && chip[0] <= 'z') chip[0] = char(chip[0] - 'a' + 'A');
  return chip;
}

}  // namespace

void App::SettingsPage(const ImRect& rect) {
  Options& o = settings_.o;
  const game::GameTexts& own = game::GameTextsFor(language_);
  bool changed = false;
  int category = page_ == Page::kGame ? 0 : page_ == Page::kVideo ? 1 : 2;

  // --- The rows of the category ---
  std::vector<OptionRow> rows;
  const char* category_label = category == 0 ? t_.nav_game : category == 1 ? t_.nav_video : t_.nav_graphics;
  const char* intro = category == 0 ? t_.settings_intro_game : category == 1 ? t_.settings_intro_video
                                                                             : t_.settings_intro_graphics;
  auto toggle_rows = [&](game::ToggleGroup group, const char* group_title) {
    bool first = true;
    for (size_t i = 0; i < game::kToggles.size(); ++i) {
      const game::Toggle& toggle = game::kToggles[i];
      if (toggle.group != group) continue;
      OptionRow row;
      row.id = toggle.cvar;
      row.title = own.toggles[i * 2];
      row.sub = own.toggles[i * 2 + 1];
      row.help = own.toggle_help[i] ? own.toggle_help[i] : own.toggles[i * 2 + 1];
      row.values = {t_.on_value, t_.off};
      row.is_switch = true;
      row.index = o.toggles[i] ? 0 : 1;
      if (first && group_title) row.group = group_title;
      first = false;
      // A switch that needs another: off and locked while that one is off.
      if (toggle.needs) {
        for (size_t j = 0; j < game::kToggles.size(); ++j) {
          if (std::string_view(game::kToggles[j].cvar) == toggle.needs && !o.toggles[j]) {
            row.locked = true;
            row.locked_value = t_.off;
            char needs[160];
            std::snprintf(needs, sizeof(needs), t_.screen_effects_needs, own.toggles[j * 2]);
            row.sub = needs;
          }
        }
      }
      row.apply = [&o, i](int index) {
        o.toggles[i] = index == 0;
        // Turned off, the switches that need it go off too (GH Metallica: the color correction takes the grain).
        if (!o.toggles[i]) {
          for (size_t k = 0; k < game::kToggles.size(); ++k) {
            if (game::kToggles[k].needs && std::string_view(game::kToggles[k].needs) == game::kToggles[i].cvar) {
              o.toggles[k] = false;
            }
          }
        }
      };
      rows.push_back(std::move(row));
    }
  };

  Size output = settings_.OutputSize();
  if (category == 0) {
    // The launcher's language: automatic (the Windows display language's, named) or one of its own.
    {
      OptionRow row;
      row.id = "launcher_language";
      row.title = t_.launcher_language;
      row.sub = t_.launcher_language_hint;
      row.help = t_.launcher_language_help;
      char automatic[96];
      std::snprintf(automatic, sizeof(automatic), t_.launcher_language_automatic,
                    LauncherLanguageName(SystemLanguage()));
      row.values.push_back(automatic);
      for (const LauncherLanguage& l : kLauncherLanguages) {
        if (l.id == o.launcher_language) row.index = int(row.values.size());
        row.values.push_back(l.name);
      }
      row.apply = [&o](int index) { o.launcher_language = index == 0 ? 0 : kLauncherLanguages[index - 1].id; };
      rows.push_back(std::move(row));
    }
    if (!game::kLanguages.empty()) {
      OptionRow row;
      row.id = "language";
      row.title = t_.game_language;
      row.sub = t_.game_language_hint;
      row.help = t_.game_language_hint;
      for (const game::Language& l : game::kLanguages) row.values.push_back(l.name);
      row.index = std::clamp(o.language, 0, int(row.values.size()) - 1);
      row.apply = [&o](int index) { o.language = index; };
      rows.push_back(std::move(row));
    }
#ifdef _WIN32
    if (game::kHasLowLatencyAudio) {
      OptionRow row;
      row.id = "audio_exclusive";
      row.title = t_.audio_exclusive;
      row.sub = t_.audio_exclusive_hint;
      row.help = t_.audio_exclusive_help;
      row.values = {t_.on, t_.off};
      row.is_switch = true;
      row.index = o.audio_exclusive ? 0 : 1;
      row.apply = [&o](int index) { o.audio_exclusive = index == 0; };
      rows.push_back(std::move(row));
    }
#endif
    // The SDK's developer keys (debug_hotkeys); on, the ones on the keyboard's controls are named: they play there.
    {
      OptionRow row;
      row.id = "debug_hotkeys";
      row.title = t_.debug_keys;
      row.sub = t_.debug_keys_hint;
      row.help = t_.debug_keys_help;
      row.values = {t_.on, t_.off};
      row.is_switch = true;
      row.index = o.debug_hotkeys ? 0 : 1;
      if (o.debug_hotkeys) {
        std::string on_controls = DebugKeysOnControls();
        if (!on_controls.empty()) {
          char sub[320];
          std::snprintf(sub, sizeof(sub), t_.debug_keys_conflict, on_controls.c_str());
          row.sub = sub;
        }
      }
      row.apply = [&o](int index) { o.debug_hotkeys = index == 0; };
      rows.push_back(std::move(row));
    }
    toggle_rows(game::ToggleGroup::kGame, nullptr);
  } else if (category == 1) {
    // Monitor.
    {
      OptionRow row;
      row.id = "monitor";
      row.title = t_.monitor;
      row.sub = t_.monitor_hint;
      row.help = t_.monitor_hint;
      for (size_t i = 0; i < settings_.monitors.size(); ++i) {
        const Monitor& m = settings_.monitors[i];
        row.values.push_back(std::to_string(i + 1) + ": " + (m.name.empty() ? std::string(t_.monitor) : m.name) + " — " +
                             SizeText(m.size));
      }
      row.index = std::clamp(o.monitor, 1, int(settings_.monitors.size())) - 1;
      row.apply = [this, &o](int index) {
        o.monitor = index + 1;
        settings_.MonitorChanged();
      };
      rows.push_back(std::move(row));
    }
    // Display mode.
    {
      OptionRow row;
      row.id = "display";
      row.title = t_.display;
      row.sub = t_.display_hint;
      row.help = t_.display_hint;
      row.values = {t_.fullscreen, t_.borderless, t_.window};
      row.index = o.display;
      row.apply = [&o](int index) { o.display = index; };
      rows.push_back(std::move(row));
    }
    // Resolution: the window's size; the monitor's in fullscreen and borderless.
    {
      OptionRow row;
      row.id = "resolution";
      row.title = t_.resolution;
      row.sub = t_.resolution_hint_window;
      row.help = t_.resolution_hint_window;
      if (o.display != kWindow) {
        row.locked = true;
        char value[96];
        std::snprintf(value, sizeof(value), t_.from_monitor, SizeText(settings_.CurrentMonitor().size).c_str());
        row.locked_value = value;
        row.sub = t_.from_monitor_hint;
        row.help = t_.resolution_hint_screen;
      }
      for (size_t i = 0; i < settings_.window_sizes.size(); ++i) {
        row.values.push_back(SizeText(settings_.window_sizes[i]));
        if (settings_.window_sizes[i] == o.window) row.index = int(i);
      }
      row.apply = [this, &o](int index) {
        if (index < int(settings_.window_sizes.size())) o.window = settings_.window_sizes[size_t(index)];
      };
      rows.push_back(std::move(row));
    }
    // Image format, the image's aspect ratio: NativeXRenderer renders the screen's (filling it), 16:9, 21:9 or 32:9, with
    // bars where it differs from the screen's (D104); Xenos the screen's, if the game widens its cameras
    // (game::kUltrawideCvar), or 16:9.
    {
      double screen = ImageAspect(output, 0.0);
      bool applies = o.native || *game::kUltrawideCvar;
      OptionRow row;
      row.id = "format";
      row.title = t_.format;
      row.help = t_.format_hint;
      std::vector<double> aspects;
      auto offer = [&](double aspect) {
        if (std::none_of(aspects.begin(), aspects.end(), [&](double a) { return SameAspect(a, aspect); })) {
          aspects.push_back(aspect);
        }
      };
      if (o.native) {
        for (double aspect : {screen, kAspect16x9, kAspect21x9, kAspect32x9, o.native_aspect}) {
          if (aspect > 0.0) offer(aspect);
        }
      } else if (applies) {
        offer(screen);
        offer(kAspect16x9);
      }
      std::sort(aspects.begin(), aspects.end());
      if (aspects.size() < 2) {
        row.locked = true;
        row.locked_value = "16:9";
        row.sub = !applies ? t_.format_hint_xenos : t_.format_hint_16x9;
      } else {
        // Each stored as Options::native_aspect: 0 for the screen's, which follows another monitor or window size.
        std::vector<double> stored;
        bool bars = o.native_aspect > 0.0 && SameAspect(o.native_aspect, kAspect16x9);
        double current = o.native ? (o.native_aspect > 0.0 ? o.native_aspect : screen) : bars ? kAspect16x9 : screen;
        for (size_t i = 0; i < aspects.size(); ++i) {
          bool own = SameAspect(aspects[i], screen);
          row.values.push_back(AspectName(aspects[i]));
          row.subs.push_back(own ? t_.format_fills : aspects[i] > screen ? t_.format_bars_top : t_.format_bars_sides);
          row.tags.push_back(own ? t_.format_your_screen : "");
          row.tag_accent.push_back(true);
          stored.push_back(own ? 0.0 : aspects[i]);
          if (SameAspect(aspects[i], current)) row.index = int(i);
        }
        row.apply = [&o, stored](int index) { o.native_aspect = stored[size_t(index)]; };
      }
      rows.push_back(std::move(row));
    }
    // NativeXRenderer's upscaling (launcher 3.9, settings.h): off, AMD FSR 1, NVIDIA NIS, and AMD FSR 3.1 and NVIDIA DLSS
    // where the game marks its views (game::kHasDlaa); then its quality, which sets the internal resolution.
    double aspect = ImageAspect(output, o.native_aspect);
    if (o.native) {
      if (!game::kHasDlaa && IsTemporal(o.upscaler)) o.upscaler = kUpscaleOff;
      std::vector<int> upscalers = {kUpscaleOff, kUpscaleFsr1, kUpscaleNis};
      OptionRow row;
      row.id = "scaling";
      row.title = t_.scaling;
      row.sub = t_.scaling_hint;
      row.help = t_.scaling_help;
      row.values = {t_.scaling_off, "AMD FSR 1", "NVIDIA NIS"};
      row.subs = {t_.scaling_off_sub, t_.scaling_fsr_sub, t_.scaling_nis_sub};
      // A game that does not offer them (game::kOffersTemporalUpscalers) shows the one its options file chose, while it
      // is in use.
      if (game::kHasDlaa && (game::kOffersTemporalUpscalers || o.upscaler == kUpscaleFsr3)) {
        upscalers.push_back(kUpscaleFsr3);
        row.values.push_back("AMD FSR 3.1");
        row.subs.push_back(t_.scaling_fsr3_sub);
      }
      if (game::kHasDlaa && (game::kOffersTemporalUpscalers || o.upscaler == kUpscaleDlss)) {
        upscalers.push_back(kUpscaleDlss);
        row.values.push_back("NVIDIA DLSS");
        row.subs.push_back(t_.scaling_dlss_sub);
      }
      row.index = int(std::find(upscalers.begin(), upscalers.end(), o.upscaler) - upscalers.begin());
      row.apply = [&o, upscalers, output, aspect](int index) {
        int previous = o.upscaler;
        o.upscaler = upscalers[size_t(index)];
        // FSR 1 and NIS come with their own sharpening (medium: what AMD and NVIDIA recommend), and leave without it,
        // unless another strength was chosen.
        bool entering = IsSpatial(o.upscaler) && !IsSpatial(previous);
        bool leaving = !IsSpatial(o.upscaler) && IsSpatial(previous);
        if (entering && o.sharpness == kSharpnessOff) o.sharpness = kSharpnessMedium;
        if (leaving && o.sharpness == kSharpnessMedium) o.sharpness = kSharpnessOff;
        // FSR 3.1 and DLSS start at their antialiasing alone (not experimental).
        int quality = IsTemporal(o.upscaler) && !IsTemporal(previous) ? kQualityNative : o.upscale_quality;
        o.upscale_quality = FitUpscaleQuality(o.upscaler, quality, output, aspect);
      };
      rows.push_back(std::move(row));
    }
    if (o.native && o.upscaler != kUpscaleOff) {
      o.upscale_quality = FitUpscaleQuality(o.upscaler, o.upscale_quality, output, aspect);
      std::vector<int> qualities = UpscaleQualities(o.upscaler, output, aspect);
      const char* names[] = {t_.quality_native, t_.quality_ultra,       t_.quality_quality,
                             t_.quality_balanced, t_.quality_performance, t_.quality_ultra_performance};
      OptionRow row;
      row.id = "quality";
      row.title = t_.quality;
      row.help = t_.quality_help;
      for (int quality : qualities) {
        char from[128];
        if (IsTemporal(o.upscaler)) {
          std::string name = names[quality];
          if (quality == kQualityNative) name += o.upscaler == kUpscaleDlss ? " · DLAA" : " · Native AA";
          row.values.push_back(name);
          Size size = TemporalInputSize(o.upscaler, output, aspect, quality);
          std::snprintf(from, sizeof(from), t_.quality_from, SizeText(size).c_str());
          row.subs.push_back(quality == kQualityNative ? t_.quality_native_sub : from);
          row.tags.push_back(quality == kQualityNative ? std::string() : std::string(t_.experimental));
        } else {
          row.values.push_back(names[quality]);
          Size size = NativeRenderSize(output, aspect, SpatialHeight(output, aspect, quality));
          std::snprintf(from, sizeof(from), t_.quality_from, SizeText(size).c_str());
          row.subs.push_back(from);
          row.tags.push_back("");
        }
        row.tag_accent.push_back(false);
        if (quality == o.upscale_quality) row.index = int(row.values.size()) - 1;
      }
      row.apply = [&o, qualities](int index) { o.upscale_quality = qualities[size_t(index)]; };
      rows.push_back(std::move(row));
    }
    // Frame generation (NativeXRenderer D123): AMD FSR 3.1's, with FSR 3.1 or DLSS (their 3D view and motion vectors),
    // where the game marks its views. With a frame rate row the rate chosen there is the one shown and the game runs at
    // half (the game's src/options.cpp): 120 FPS or more.
    if (kFrameGenerationRow && o.native && game::kHasDlaa) {
      const bool rate_row = *game::kFrameRateCvar && !game::kFrameRateIsBool;
      OptionRow row;
      row.id = "framegen";
      row.title = t_.frame_generation;
      row.sub = t_.frame_generation_hint;
      row.help = rate_row ? t_.frame_generation_help : t_.frame_generation_help_fixed;
      if (!settings_.FrameGenerationPossible()) {
        row.locked = true;
        row.locked_value = t_.off;
        row.sub = !IsTemporal(o.upscaler) ? t_.frame_generation_needs_upscaler
                  : rate_row             ? t_.frame_generation_needs_rate
                                         : t_.frame_generation_needs_monitor;
      } else {
        char on_sub[160];
        if (rate_row) {
          std::snprintf(on_sub, sizeof(on_sub), t_.frame_generation_on_sub, o.frame_rate / 2, o.frame_rate);
        } else {
          std::snprintf(on_sub, sizeof(on_sub), "%s", t_.frame_generation_on_sub_fixed);
        }
        row.values = {t_.off, "AMD FSR 3.1"};
        row.subs = {t_.frame_generation_hint, on_sub};
        row.tags = {"", t_.experimental};
        row.tag_accent = {false, false};
        row.index = o.frame_generation ? 1 : 0;
        row.apply = [&o](int index) { o.frame_generation = index == 1; };
      }
      rows.push_back(std::move(row));
    }
    // Internal resolution: NativeXRenderer's (automatic, or a lower height; set by the upscaling's quality), or Xenos's
    // scale.
    {
      OptionRow row;
      row.id = "internal";
      row.title = t_.internal;
      char xenos_hint[128];
      std::snprintf(xenos_hint, sizeof(xenos_hint), t_.internal_hint_xenos, game::kFrameWidth, game::kFrameHeight);
      row.sub = o.native ? t_.internal_hint : xenos_hint;
      row.help = row.sub;
      if (o.native && IsSpatial(o.upscaler)) {
        row.locked = true;
        row.locked_value = SizeText(NativeRenderSize(output, aspect, SpatialHeight(output, aspect, o.upscale_quality)));
        row.sub = t_.internal_by_quality;
      } else if (o.native && IsTemporal(o.upscaler)) {
        // FSR 3.1 and DLSS draw only the 3D scene at the quality's size; the HUD and the game's effects are drawn at
        // the screen's, which the upscaler rebuilds the scene at (launcher 3.9.1: 3.9 showed the screen's size here).
        row.locked = true;
        row.locked_value = SizeText(TemporalInputSize(o.upscaler, output, aspect, o.upscale_quality));
        row.sub = t_.internal_scene_only;
        row.help = t_.internal_scene_help;
      } else if (o.native) {
        std::vector<int> heights = NativeHeights(output, ImageAspect(output, o.native_aspect), o.native_height);
        if (std::find(heights.begin(), heights.end(), o.native_height) == heights.end()) {
          o.native_height = 0;  // another monitor or window no longer offers it
        }
        for (size_t i = 0; i < heights.size(); ++i) {
          Size size = NativeRenderSize(output, ImageAspect(output, o.native_aspect), heights[i]);
          std::string text = SizeText(size);
          if (!heights[i]) {
            text = std::string(t_.automatic) + " (" + text + ")";
          } else if (size == Size{kGuestWidth, kGuestHeight}) {
            text += std::string(" ") + t_.original;
          }
          row.values.push_back(std::move(text));
          if (heights[i] == o.native_height) row.index = int(i);
        }
        row.apply = [&o, heights](int index) { o.native_height = heights[size_t(index)]; };
      } else {
        auto render = [&](Size scale) {
          std::string text = SizeText({kGuestWidth * scale.first, kGuestHeight * scale.second});
          if (scale.first == 1 && scale.second == 1) return text + " " + t_.original;
          return text + " (" + std::to_string(scale.first) + "×)";
        };
        row.values.push_back(std::string(t_.automatic) + " (" + render(AutoScale(output)) + ")");
        for (int scale = 1; scale <= kMaxScale; ++scale) {
          row.values.push_back(render({scale, scale}));
        }
        row.index = o.scale;
        row.apply = [&o](int index) { o.scale = std::clamp(index, 0, kMaxScale); };
      }
      rows.push_back(std::move(row));
    }
    // The sharpening after the HUD (D115): the upscaler's own with FSR 1 and NIS, AMD CAS otherwise.
    if (o.native) {
      OptionRow row;
      row.id = "sharpness";
      row.title = t_.sharpness;
      row.sub = t_.sharpness_hint;
      row.help = t_.sharpness_help;
      row.values = {t_.sharpness_off, t_.sharpness_low, t_.sharpness_medium, t_.sharpness_high};
      row.index = std::clamp(o.sharpness, 0, kSharpnessCount - 1);
      row.apply = [&o](int index) { o.sharpness = index; };
      rows.push_back(std::move(row));
    }
  } else {
    // The renderer: NativeXRenderer (recommended) or Xenos.
    if (game::kHasNativeXRenderer) {
      OptionRow row;
      row.id = "renderer";
      row.title = t_.renderer_title;
      row.values = {"NativeXRenderer", "Xenos"};
      row.subs = {t_.renderer_native_sub, t_.renderer_xenos_sub};
      row.tags = {t_.recommended, ""};
      row.tag_accent = {true, false};
      row.index = o.native ? 0 : 1;
      row.help = std::string(settings_.has_vulkan ? t_.native_text : t_.native_missing) + " " + t_.xenos_text;
      if (!settings_.has_vulkan) {
        row.locked = true;
        row.locked_value = "Xenos";
      }
      row.apply = [&o](int index) { o.native = index == 0; };
      rows.push_back(std::move(row));
    }
    // The frame rate: the rates the monitor reaches; those above 60 are experimental.
    if (*game::kFrameRateCvar) {
      std::vector<game::FrameRate> rates = FrameRates(settings_.CurrentMonitor(), o.any_frame_rate);
      OptionRow row;
      row.id = "fps";
      row.title = t_.framerate;
      row.sub = settings_.FrameGenerationOn() ? t_.framerate_with_generation : t_.framerate_hint;
      row.help = row.sub;
      for (size_t i = 0; i < rates.size(); ++i) {
        row.values.push_back(std::to_string(rates[i].fps) + " FPS");
        row.tags.push_back(rates[i].fps > 60 ? t_.experimental : std::string());
        row.tag_accent.push_back(false);
        if (rates[i].value == o.frame_rate) row.index = int(i);
      }
      row.apply = [&o, rates](int index) { o.frame_rate = rates[size_t(index)].value; };
      rows.push_back(std::move(row));
    }
    // V-Sync: frames never torn, or each shown as it is ready (settings.h).
    {
      OptionRow row;
      row.id = "vsync";
      row.title = t_.vsync;
      row.sub = t_.vsync_hint;
      row.help = t_.vsync_help;
      row.values = {t_.on, t_.off};
      row.is_switch = true;
      row.index = o.vsync ? 0 : 1;
      row.apply = [&o](int index) { o.vsync = index == 0; };
      if (settings_.FrameGenerationOn()) {
        // Its frames are shown a refresh apart (NativeXRenderer forces FIFO).
        row.locked = true;
        row.locked_value = t_.on;
        row.sub = t_.vsync_by_frame_generation;
      }
      rows.push_back(std::move(row));
    }
    // Antialiasing: NativeXRenderer's SMAA or FXAA (FSR 3.1 and DLSS, Video > Upscaling, do their own), or the game's
    // MSAA.
    if (o.native) {
      OptionRow row;
      row.id = "aa";
      row.title = t_.antialiasing;
      row.sub = game::kHasDlaa && game::kOffersTemporalUpscalers ? t_.antialiasing_hint : t_.antialiasing_hint_no_dlaa;
      row.help = row.sub;
      row.values = {"SMAA", "FXAA", t_.none};
      row.index = std::clamp(o.native_aa, 0, kNativeAntialiasingCount - 1);
      if (game::kHasDlaa && IsTemporal(o.upscaler)) {
        row.locked = true;
        row.locked_value = o.upscaler == kUpscaleDlss ? "NVIDIA DLSS" : "AMD FSR 3.1";
        row.sub = t_.antialiasing_by_upscaler;
      }
      row.apply = [&o](int index) { o.native_aa = index; };
      rows.push_back(std::move(row));
    } else if (*game::kNoMsaaCvar) {
      OptionRow row;
      row.id = "msaa";
      row.title = t_.antialiasing;
      row.sub = t_.antialiasing_hint_xenos;
      row.help = t_.antialiasing_hint_xenos;
      row.values = {game::kMsaaLabel, t_.none};
      row.index = o.no_msaa ? 1 : 0;
      row.apply = [&o](int index) { o.no_msaa = index == 1; };
      rows.push_back(std::move(row));
    }
    // Anisotropic filtering.
    {
      constexpr int kValues[] = {0, 2, 3, 4, 5};
      OptionRow row;
      row.id = "anisotropic";
      row.title = t_.anisotropic;
      row.sub = t_.anisotropic_hint;
      row.help = t_.anisotropic_hint;
      row.values = {t_.off, "2×", "4×", "8×", "16×"};
      row.index = int(std::find(std::begin(kValues), std::end(kValues), o.anisotropic) - std::begin(kValues));
      row.apply = [&o](int index) { o.anisotropic = kValues[index]; };
      rows.push_back(std::move(row));
    }
    // The shadow maps' resolution (NativeXRenderer's D92), locked with Xenos.
    if (game::kShadowQuality) {
      OptionRow row;
      row.id = "shadows";
      row.title = t_.shadows;
      row.sub = o.native ? t_.shadows_hint : t_.shadows_hint_xenos;
      row.help = row.sub;
      row.values = {"Original", t_.shadows_high, t_.shadows_ultra};
      row.index = o.native ? o.shadow_quality : 0;
      if (!o.native) {
        row.locked = true;
        row.locked_value = "Original";
      }
      row.apply = [&o](int index) { o.shadow_quality = index; };
      rows.push_back(std::move(row));
    }
    // Xenos draws through the SDK's Direct3D 12 or Vulkan backend.
    if (!o.native) {
      OptionRow row;
      row.id = "api";
      row.title = t_.graphics_api;
      row.sub = t_.api_hint;
      row.help = t_.api_hint;
      row.values = {"DirectX 12"};
      if (settings_.has_vulkan) row.values.push_back("Vulkan");
      row.index = o.vulkan ? 1 : 0;
      row.apply = [this, &o](int index) { o.vulkan = settings_.has_vulkan && index == 1; };
      rows.push_back(std::move(row));
    }
    toggle_rows(game::ToggleGroup::kScreenEffects, t_.screen_effects);
  }

  // --- The layout (dp from the page's top left; the mockup's 1280 x 760 board): the title at 56, 20; the categories
  // at 40, 110, 200 wide; the intro at 300, 26; the rows from 300, 68 to 56 from the right edge; the foot 52 high at
  // 576 ---
  ImGuiContext& g = *GImGui;
  const float page_left = rect.Min.x;
  const float page_top = rect.Min.y;
  const float rows_left = page_left + Px(300.0f);
  const float rows_right = rect.Max.x - Px(56.0f);
  const float foot_top = rect.Max.y - Px(60.0f);

  // The title and the categories: a dot marks the current one; up and down change it at once, right (or A) goes to
  // its rows.
  {
    Type title{Font::kDisplaySemiBold, 44, 0.015f};
    std::string label = Title(t_.nav_settings);
    // No wider than the column (German's EINSTELLUNGEN).
    float width = MeasureText(title, label).x;
    if (width > Px(232.0f)) title.size *= Px(232.0f) / width;
    DrawLabel(title, ImVec2(page_left + Px(56.0f), page_top + Px(20.0f)), col::kText, label, Px(44.0f * 1.1f));
    constexpr Type kCategory{Font::kDisplayMedium, 22, 0.015f};
    const std::pair<Page, const char*> categories[] = {
        {Page::kGame, t_.nav_game}, {Page::kVideo, t_.nav_video}, {Page::kGraphics, t_.nav_graphics}};
    std::vector<std::array<ImGuiID, 2>> category_ids;
    float y = page_top + Px(110.0f);
    for (const auto& [category_page, category_name] : categories) {
      ImRect item(ImVec2(page_left + Px(40.0f), y), ImVec2(page_left + Px(240.0f), y + Px(48.0f)));
      ImGui::PushID(int(category_page));
      ItemState state = Item("##category", item);
      ImGui::PopID();
      ImGuiID id = ImGui::GetItemID();
      bool on = page_ == category_page;
      if (on) category_focus_ = id;
      ImU32 color = on ? col::kText : state.hovered ? col::kTextSoft : col::kTextMuted;
      DrawLabel(kCategory, ImVec2(item.Min.x + Px(18.0f), item.Min.y), color, category_name, item.GetHeight());
      if (on) {
        float dot = Px(8.0f);
        ImVec2 c(item.Min.x + dot * 0.5f, item.GetCenter().y);
        ImGui::GetWindowDrawList()->AddCircleFilled(c, dot * 0.5f, col::kAccent, 16);
      }
      if (state.focused) {
        float text_width = MeasureText(kCategory, category_name).x;
        FocusRing(ImRect(ImVec2(item.Min.x + Px(10.0f), item.Min.y + Px(6.0f)),
                         ImVec2(item.Min.x + Px(26.0f) + text_width, item.Max.y - Px(6.0f))),
                  8.0f);
      }
      // The controller's focus on a category shows it (a click or A too).
      bool focus_here = g.NavId == id && g.NavCursorVisible && !on;
      if ((state.pressed || focus_here) && !on) {
        Navigate(category_page, false);
      }
      // Right, or A, from a category: its row in focus last.
      if (g.NavId == id && settings_row_focus_ &&
          ((g.NavMoveSubmitted && g.NavMoveDir == ImGuiDir_Right) || (state.pressed && PressedByNav()))) {
        RequestFocus(settings_row_focus_);
        if (g.NavMoveSubmitted) ImGui::NavMoveRequestCancel();
      }
      category_ids.push_back({id, id});
      y += Px(48.0f);
    }
    ColumnNav(category_ids);
    constexpr Type kNote{Font::kRegular, 14};
    float note_width = Px(200.0f);
    float note_height = DrawParagraph(kNote, ImVec2(0, 0), note_width, 1.4f, 0, t_.settings_controls_note, false);
    DrawParagraph(kNote, ImVec2(page_left + Px(56.0f), rect.Max.y - Px(48.0f) - note_height), note_width, 1.4f,
                  col::kTextDim, t_.settings_controls_note);
  }

  // The intro over the rows; on Graphics, the graphics API in use at its right.
  constexpr Type kIntro{Font::kRegular, 16};
  float intro_y = page_top + Px(26.0f);
  DrawLabel(kIntro, ImVec2(rows_left, intro_y), col::kTextMuted, intro);
  if (category == 2) {
    constexpr Type kApi{Font::kRegular, 13};
    constexpr Type kApiValue{Font::kBold, 13};
    std::string label = std::string(t_.graphics_api) + ": ";
    const char* api = o.native || o.vulkan ? "Vulkan" : "DirectX 12";
    float api_x = rows_right - MeasureText(kApiValue, api).x;
    float api_y = intro_y + (LineHeight(kIntro) - LineHeight(kApi)) * 0.5f;
    DrawLabel(kApiValue, ImVec2(api_x, api_y), col::kTextSoft, api);
    DrawLabel(kApi, ImVec2(api_x - MeasureText(kApi, label).x, api_y), col::kTextMuted, label);
  }

  // The rows, scrolling when they do not fit; the scroll bar 16 dp right of them.
  int& selected = settings_row_[category];
  selected = std::clamp(selected, 0, std::max(0, int(rows.size()) - 1));
  ImRect list(ImVec2(rows_left, page_top + Px(68.0f)), ImVec2(rows_right + Px(16.0f), foot_top - Px(8.0f)));
  BeginScroll("##settings_rows", list);
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float y = origin.y;
  const float row_left = origin.x;
  const float row_right = origin.x + (rows_right - rows_left);
  std::vector<std::array<ImGuiID, 2>> row_ids;
  for (int i = 0; i < int(rows.size()); ++i) {
    OptionRow& row = rows[size_t(i)];
    if (!row.group.empty()) {
      y += Px(18.0f);
      DrawLabel(kGroupTitle, ImVec2(row_left + Px(20.0f), y), col::kTextMuted, row.group);
      y += LineHeight(kGroupTitle) + Px(8.0f);
    }
    // The current row grows to show its line of help and its arrows: the one in focus, or the last one chosen.
    const bool big = selected == i;
    if (big) y += Px(kFocusedRowMargin);
    ImRect r(ImVec2(row_left, y), ImVec2(row_right, y + Px(big ? kFocusedRowHeight : kRowHeight)));
    ImGui::PushID(row.id.c_str());
    // The arrows are drawn over the row: a click on one steps the value at once.
    ImGui::SetNextItemAllowOverlap();
    ItemState state = Item("##row", r);
    ImGuiID row_id = ImGui::GetItemID();
    row_ids.push_back({row_id, row_id});
    if (big) {
      settings_row_focus_ = row_id;
      // LB/RB onto this tab, or a category's right: the controller lands on the current row.
      page_focus_ = row_id;
    }
    const bool nav_here = g.NavId == row_id && g.NavWindow == ImGui::GetCurrentWindow();
    if (nav_here && selected != i) {
      selected = i;  // grows next frame
    }
    int count = int(row.values.size());
    const bool changeable = !row.locked && count > 1;
    auto step = [&](int delta) {
      if (!changeable) return;
      row.index = (row.index + delta + count) % count;
      row.apply(row.index);
      selected = i;
      changed = true;
    };
    // The controller: left and right step the value (left from a row that cannot change goes back to the categories);
    // A steps it forward, as a click does.
    if (nav_here && g.NavMoveSubmitted && (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right)) {
      if (changeable) {
        step(g.NavMoveDir == ImGuiDir_Left ? -1 : 1);
      } else if (g.NavMoveDir == ImGuiDir_Left) {
        RequestFocus(category_focus_);
      }
      ImGui::NavMoveRequestCancel();
    }
    if (state.pressed) {
      step(1);
      selected = i;
    }

    // The row: a hairline under it, or the focus's fill and outline when it is the big one.
    if (big) {
      if (state.focused) {
        FocusRow(r, 12.0f);
      } else {
        FillRect(r, Fade(col::kFocus, 0.06f), 12.0f);
      }
    } else {
      if (state.hovered) FillRect(r, Fade(col::kFocus, 0.03f), 8.0f);
      FillRect(ImRect(ImVec2(r.Min.x, r.Max.y - Snap(Px(1.0f))), r.Max), col::kHairline);
    }

    // The value at the right: a switch, or its text (with a lock when it cannot change) and its chip; in the big row
    // between the arrows, its place among the values under it.
    const float pad = Px(20.0f);
    float value_left = r.Max.x - pad;
    std::string tag = ChipText(row.Tag());
    bool tag_warning = !tag.empty() && !(row.tag_accent.empty() ||
                                         row.tag_accent[size_t(std::clamp(row.index, 0, int(row.tag_accent.size()) - 1))]);
    if (row.is_switch) {
      bool on = row.locked ? row.locked_value == std::string(t_.on) : row.index == 0;
      value_left = r.Max.x - pad - Px(52.0f);
      DrawToggle(ImVec2(value_left, r.GetCenter().y - Px(15.0f)), on, !row.locked, false, state.hovered);
      if (row.locked) {
        DrawIcon(Icon::kLock, ImVec2(value_left - Px(10.0f) - Px(18.0f), r.GetCenter().y - Px(9.0f)), 18.0f,
                 col::kTextDim, 1.8f);
        value_left -= Px(28.0f);
      }
    } else if (big && changeable) {
      const Type& type = kRowValueFocused;
      float max_text = std::max(Px(150.0f), (r.GetWidth() - pad * 2.0f) * 0.45f);
      float tag_width = tag.empty() ? 0.0f : Px(10.0f) + ChipWidth(tag);
      std::string shown = Ellipsize(type, row.Value(), max_text - tag_width);
      float content = MeasureText(type, shown).x + tag_width;
      float block = std::max(Px(150.0f), content);
      float right_arrow = r.Max.x - pad - Px(28.0f);
      float block_right = right_arrow - Px(4.0f);
      float block_left = block_right - block;
      float left_arrow = block_left - Px(4.0f) - Px(28.0f);
      bool dots = count <= 12;
      float block_height = LineHeight(type) + (dots ? Px(5.0f) + Px(6.0f) : 0.0f);
      float text_y = r.GetCenter().y - block_height * 0.5f;
      float text_x = (block_left + block_right - content) * 0.5f;
      DrawLabel(type, ImVec2(text_x, text_y), col::kText, shown);
      if (!tag.empty()) {
        DrawChip(ImVec2(text_x + MeasureText(type, shown).x + Px(10.0f), text_y + LineHeight(type) * 0.5f), tag,
                 tag_warning);
      }
      if (dots) {
        float dot = Px(6.0f);
        float gap = Px(5.0f);
        float dots_width = float(count) * dot + float(count - 1) * gap;
        float dx = (block_left + block_right - dots_width) * 0.5f;
        float dy = text_y + LineHeight(type) + Px(5.0f);
        for (int d = 0; d < count; ++d) {
          ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(dx + dot * 0.5f, dy + dot * 0.5f), dot * 0.5f,
                                                      d == row.index ? col::kText : Fade(col::kTextDim, 0.6f), 12);
          dx += dot + gap;
        }
      }
      if (ArrowButton("##prev", ImVec2(left_arrow, r.GetCenter().y - Px(14.0f)), true)) step(-1);
      if (ArrowButton("##next", ImVec2(right_arrow, r.GetCenter().y - Px(14.0f)), false)) step(1);
      value_left = left_arrow;
    } else {
      const Type& type = big ? kRowValueFocused : kRowValue;
      float max_text = std::max(Px(150.0f), (r.GetWidth() - pad * 2.0f) * 0.5f);
      float tag_width = tag.empty() ? 0.0f : Px(10.0f) + ChipWidth(tag);
      std::string shown = Ellipsize(type, row.Value(), max_text - tag_width);
      float text_width = MeasureText(type, shown).x;
      float x = r.Max.x - pad - tag_width - text_width;
      float text_y = r.GetCenter().y - LineHeight(type) * 0.5f;
      DrawLabel(type, ImVec2(x, text_y), row.locked ? col::kTextDim : col::kText, shown);
      if (!tag.empty()) {
        DrawChip(ImVec2(x + text_width + Px(10.0f), r.GetCenter().y), tag, tag_warning);
      }
      if (row.locked) {
        x -= Px(8.0f) + Px(18.0f);
        DrawIcon(Icon::kLock, ImVec2(x, r.GetCenter().y - Px(9.0f)), 18.0f, col::kTextDim, 1.8f);
      }
      value_left = x;
    }

    // The label at the left; in the big row its line of help under it. Both cut to what the value leaves.
    float text_left = r.Min.x + pad;
    float text_width = std::max(Px(60.0f), value_left - Px(24.0f) - text_left);
    if (big) {
      float height = LineHeight(kRowLabelFocused) + Px(3.0f) + LineHeight(kRowDescription);
      float ty = r.GetCenter().y - height * 0.5f;
      DrawLabel(kRowLabelFocused, ImVec2(text_left, ty), row.locked ? col::kTextMuted : col::kText,
                Ellipsize(kRowLabelFocused, row.title, text_width));
      DrawLabel(kRowDescription, ImVec2(text_left, ty + LineHeight(kRowLabelFocused) + Px(3.0f)), col::kTextMuted,
                Ellipsize(kRowDescription, row.Sub(), text_width));
    } else {
      DrawLabel(kRowLabel, ImVec2(text_left, r.GetCenter().y - LineHeight(kRowLabel) * 0.5f),
                row.locked ? col::kTextDim : col::kTextSoft, Ellipsize(kRowLabel, row.title, text_width));
    }
    ImGui::PopID();
    y = r.Max.y + (big ? Px(kFocusedRowMargin) : 0.0f);
  }
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(row_right - row_left, y - origin.y));
  // Up and down through the rows; up from the first reaches the header.
  ColumnNav(row_ids);
  // While the focus is in the rows, B goes back to the categories (App::Frame).
  if (g.NavWindow == ImGui::GetCurrentWindow()) {
    back_focus_ = category_focus_;
  }
  EndScroll();

  // The foot: saved automatically; Reset (X) for the category, its button the mouse's.
  bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
  {
    ImRect foot(ImVec2(rows_left, foot_top), ImVec2(rows_right, foot_top + Px(52.0f)));
    FillRect(ImRect(foot.Min, ImVec2(foot.Max.x, foot.Min.y + Snap(Px(1.0f)))), col::kHairline);
    // "Reset video": the category's name in lower case inside the sentence, but in German, whose nouns keep their
    // capital (the five languages' names start with an ASCII letter).
    std::string category_word = category_label;
    if (language_ != 3 && !category_word.empty() && category_word[0] >= 'A' && category_word[0] <= 'Z') {
      category_word[0] = char(category_word[0] - 'A' + 'a');
    }
    char reset[96];
    std::snprintf(reset, sizeof(reset), t_.reset_category, category_word.c_str());
    ButtonSpec spec{ButtonKind::kGhost, Type{Font::kSemiBold, 14}, 34, 16, 10};
    float button_width = ButtonWidth(spec, reset);
    constexpr Type kNote{Font::kRegular, 14};
    DrawIcon(Icon::kCheck, ImVec2(foot.Min.x, foot.GetCenter().y - Px(9.0f)), 18.0f, col::kTextDim, 1.9f);
    float note_x = foot.Min.x + Px(18.0f) + Px(10.0f);
    DrawLabel(kNote, ImVec2(note_x, foot.GetCenter().y - LineHeight(kNote) * 0.5f), col::kTextMuted,
              Ellipsize(kNote, t_.autosave, foot.Max.x - button_width - Px(24.0f) - note_x));
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    bool reset_pressed = Button("##reset", ImVec2(foot.Max.x - button_width, foot.GetCenter().y - Px(17.0f)), spec, reset);
    ImGui::PopItemFlag();
    if (reset_pressed || (!popup && ButtonPressed(PadButton::kX))) {
      if (category == 0) {
        ResetGame(o);
      } else if (category == 1) {
        ResetVideo(o);
        settings_.MonitorChanged();
      } else {
        ResetGraphics(o);
        o.native = o.native && settings_.has_vulkan && game::kHasNativeXRenderer;
      }
      changed = true;
    }
  }

  // Y: the current row's whole help.
  if (!popup && !rows.empty() && ButtonPressed(PadButton::kY)) {
    const OptionRow& row = rows[size_t(selected)];
    OpenInfo(t_.more_info, row.title, row.help.empty() ? row.Sub() : row.help);
  }

  if (changed) {
    // The launcher's language (its row, or Reset): the texts change from the next frame.
    if (uint32_t language = TextsLanguage(o.launcher_language); language != language_) {
      SetLanguage(language);
    }
    Changed();
  }
}

}  // namespace launcher
