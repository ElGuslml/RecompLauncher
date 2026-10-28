// The Settings tab (Gustavo's 2026-10-28 mockup): its categories on the left (Game, when the game has options of its
// own; Video; Graphics), the category's options in the middle, each a row that steps through its values ("‹ value ›",
// the controller's left and right), and on the right the help of the row in focus with its current value. Every change
// is saved at once.

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

constexpr Type kRowTitle{Font::kSemiBold, 17};
constexpr Type kRowSub{Font::kRegular, 13};
constexpr Type kValue{Font::kDisplaySemiBold, 19, 0.02f};
constexpr Type kTag{Font::kBold, 10, 0.08f};
constexpr Type kGroupTitle{Font::kBold, 12, 0.14f};

// A round arrow button of a row (mouse only: the controller steps with left and right while the row is being
// changed, `active`).
bool ArrowButton(const char* id, ImVec2 pos, bool left, bool active) {
  ImRect rect(pos, ImVec2(pos.x + Px(40.0f), pos.y + Px(40.0f)));
  ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
  ItemState state = Item(id, rect);
  ImGui::PopItemFlag();
  ImU32 fill = active ? col::kAccent : col::kPanelChip;
  FillRect(rect, state.hovered ? Brighten(fill) : fill, 20.0f);
  DrawIcon(left ? Icon::kChevronLeft : Icon::kChevronRight, ImVec2(pos.x + Px(11.0f), pos.y + Px(11.0f)), 18.0f,
           active ? col::kOnAccent : col::kText, 2.2f);
  return state.pressed;
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
    // Internal resolution: NativeXRenderer's (automatic, or a lower height), or Xenos's scale.
    {
      OptionRow row;
      row.id = "internal";
      row.title = t_.internal;
      char xenos_hint[128];
      std::snprintf(xenos_hint, sizeof(xenos_hint), t_.internal_hint_xenos, game::kFrameWidth, game::kFrameHeight);
      row.sub = o.native ? t_.internal_hint : xenos_hint;
      row.help = row.sub;
      if (o.native) {
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
      std::vector<game::FrameRate> rates = FrameRates(settings_.CurrentMonitor());
      OptionRow row;
      row.id = "fps";
      row.title = t_.framerate;
      row.sub = t_.framerate_hint;
      row.help = t_.framerate_hint;
      for (size_t i = 0; i < rates.size(); ++i) {
        row.values.push_back(std::to_string(rates[i].fps) + " FPS");
        row.tags.push_back(rates[i].fps > 60 ? t_.experimental : std::string());
        row.tag_accent.push_back(false);
        if (rates[i].value == o.frame_rate) row.index = int(i);
      }
      row.apply = [&o, rates](int index) { o.frame_rate = rates[size_t(index)].value; };
      rows.push_back(std::move(row));
    }
    // Antialiasing: NativeXRenderer's (an experimental DLSS mode the options file named, last), or the game's MSAA.
    if (o.native) {
      constexpr int kOrder[] = {kSmaa, kFxaa, kDlaa, kAaOff};
      OptionRow row;
      row.id = "aa";
      row.title = t_.antialiasing;
      row.sub = t_.antialiasing_hint;
      row.help = t_.antialiasing_hint;
      row.values = {"SMAA", "FXAA", "DLAA", t_.none};
      row.tags = {"", "", "", ""};
      row.tag_accent = {false, false, false, false};
      if (o.native_aa_experimental >= 0) {
        row.values.push_back(kExperimentalAntialiasing[o.native_aa_experimental].label);
        row.tags.push_back(t_.experimental);
        row.tag_accent.push_back(false);
      }
      row.index = o.native_aa >= kNativeAntialiasingCount
                      ? 4
                      : int(std::find(std::begin(kOrder), std::end(kOrder), o.native_aa) - std::begin(kOrder));
      row.apply = [&o](int index) { o.native_aa = index < 4 ? kOrder[index] : kNativeAntialiasingCount; };
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

  // --- The layout: 24 40 20 dp of padding, the categories 220 dp wide, the help 320, 24 dp between ---
  ImRect main(ImVec2(rect.Min.x + Px(40.0f), rect.Min.y + Px(24.0f)), ImVec2(rect.Max.x - Px(40.0f), rect.Max.y - Px(20.0f)));
  ImRect nav(main.Min, ImVec2(main.Min.x + Px(220.0f), main.Max.y));
  ImRect help(ImVec2(main.Max.x - Px(320.0f), main.Min.y), main.Max);
  ImRect section(ImVec2(nav.Max.x + Px(24.0f), main.Min.y), ImVec2(help.Min.x - Px(24.0f), main.Max.y));

  // The categories: the title, a button each (accent for the current one), the note at the foot.
  {
    constexpr Type kTitle{Font::kDisplayBold, 40};
    DrawLabel(kTitle, nav.Min, col::kText, Upper(t_.nav_settings), Px(40.0f));
    float y = nav.Min.y + Px(40.0f) + Px(12.0f) + Px(8.0f);
    constexpr Type kCategory{Font::kDisplaySemiBold, 19, 0.04f};
    struct Category {
      Page page;
      const char* label;
      const char* icon;
    };
    // The mockup's icons: a globe, a monitor, a cube.
    const Category categories[] = {
        {Page::kGame, t_.nav_game, "<path d='M12 3a9 9 0 1 0 0 18a9 9 0 1 0 0-18M3 12h18M12 3c3 3 3 15 0 18M12 3c-3 3-3 15 0 18'/>"},
        {Page::kVideo, t_.nav_video, "<path d='M3 5h18v12H3zM8 21h8M12 17v4'/>"},
        {Page::kGraphics, t_.nav_graphics, "<path d='M12 3l8 4.5v9L12 21l-8-4.5v-9zM12 12l8-4.5M12 12v9M12 12L4 7.5'/>"},
    };
    std::vector<std::array<ImGuiID, 2>> category_ids;
    for (const Category& c : categories) {
      if (c.page == Page::kGame && !game::kHasGamePage) continue;
      ImRect button(ImVec2(nav.Min.x, y), ImVec2(nav.Max.x, y + Px(56.0f)));
      ImGui::PushID(int(c.page));
      ItemState state = Item("##category", button);
      ImGui::PopID();
      bool on = page_ == c.page;
      ImU32 background = on ? col::kAccent : col::kGhostFill;
      ImU32 border = on ? col::kAccent : col::kBumper;
      ImU32 text = on ? col::kOnAccent : col::kTextSoft;
      if (state.hovered) background = Brighten(background);
      FillRect(button, background, 10.0f);
      StrokeRect(button, border, 10.0f);
      DrawIconSvg(c.icon, c.icon, ImVec2(button.Min.x + Px(16.0f), button.GetCenter().y - Px(10.0f)), 20.0f, text, 1.8f);
      DrawLabel(kCategory, ImVec2(button.Min.x + Px(16.0f) + Px(20.0f) + Px(12.0f), button.Min.y), text, Upper(c.label),
                button.GetHeight());
      if (state.focused) FocusRing(button, 10.0f);
      if (state.pressed && !on) Navigate(c.page, false);
      if (on) category_focus_ = ImGui::GetItemID();
      category_ids.push_back({ImGui::GetItemID(), ImGui::GetItemID()});
      // Right from a category: the row in focus there last (not the nearest one).
      if (GImGui->NavId == ImGui::GetItemID() && GImGui->NavMoveSubmitted && GImGui->NavMoveDir == ImGuiDir_Right &&
          settings_row_focus_) {
        RequestFocus(settings_row_focus_);
        ImGui::NavMoveRequestCancel();
      }
      y += Px(56.0f) + Px(8.0f);
    }
    // The controller starts on the category; up and down go through the categories (not into the rows).
    page_focus_ = category_focus_;
    ColumnNav(category_ids);
    constexpr Type kNote{Font::kRegular, 13};
    float note_height = DrawParagraph(kNote, ImVec2(0, 0), nav.GetWidth(), 1.5f, 0, t_.settings_controls_note, false);
    DrawParagraph(kNote, ImVec2(nav.Min.x, nav.Max.y - note_height), nav.GetWidth(), 1.5f, col::kTabText,
                  t_.settings_controls_note);
  }

  // The options' panel: the intro, the rows (scrolling when they do not fit), "saved automatically" and Reset.
  FillRect(section, col::kPanel, 14.0f);
  StrokeRect(section, col::kHairline, 14.0f);
  ImRect inner(ImVec2(section.Min.x + Px(16.0f), section.Min.y + Px(16.0f)),
               ImVec2(section.Max.x - Px(16.0f), section.Max.y - Px(16.0f)));
  constexpr Type kIntro{Font::kRegular, 15};
  DrawLabel(kIntro, ImVec2(inner.Min.x + Px(8.0f), inner.Min.y + Px(4.0f)), col::kTabText, intro);
  if (category == 2) {
    // The graphics API in use, right of the intro.
    constexpr Type kApi{Font::kRegular, 13};
    constexpr Type kApiValue{Font::kBold, 13};
    std::string label = std::string(t_.graphics_api) + ": ";
    const char* api = o.native || o.vulkan ? "Vulkan" : "DirectX 12";
    float width = MeasureText(kApi, label).x + MeasureText(kApiValue, api).x + Px(20.0f);
    ImRect badge(ImVec2(inner.Max.x - Px(8.0f) - width, inner.Min.y + Px(4.0f) + (LineHeight(kIntro) - LineHeight(kApi) - Px(10.0f)) * 0.5f),
                 ImVec2(inner.Max.x - Px(8.0f), inner.Min.y + Px(4.0f) + (LineHeight(kIntro) + LineHeight(kApi) + Px(10.0f)) * 0.5f));
    FillRect(badge, col::kHairline, 6.0f);
    DrawLabel(kApi, ImVec2(badge.Min.x + Px(10.0f), badge.Min.y), col::kTabText, label, badge.GetHeight());
    DrawLabel(kApiValue, ImVec2(badge.Min.x + Px(10.0f) + MeasureText(kApi, label).x, badge.Min.y), col::kText, api,
              badge.GetHeight());
  }
  float rows_top = inner.Min.y + Px(4.0f) + LineHeight(kIntro) + Px(8.0f) + Px(8.0f);
  float footer_top = inner.Max.y - Px(12.0f) - Px(38.0f);
  ImRect list(ImVec2(inner.Min.x, rows_top), ImVec2(inner.Max.x, footer_top - Px(8.0f)));

  int& selected = settings_row_[category];
  selected = std::clamp(selected, 0, std::max(0, int(rows.size()) - 1));
  ImGuiContext& g = *GImGui;
  BeginScroll("##settings_rows", list);
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float y = origin.y;
  // A focus ring 3 dp wide inside the row's edge needs no room outside it; the rows keep 4 dp from the region's edges.
  float row_left = origin.x + Px(4.0f);
  float row_right = origin.x + list.GetWidth() - Px(4.0f) - (ImGui::GetCurrentWindow()->ScrollbarY ? Px(12.0f) : 0.0f);
  bool editing_any = false;
  // The chosen row is outlined only while the rows have the controller's focus, or the mouse is over them; with the
  // focus on the categories none is (its help still shows).
  bool rows_active = (g.NavWindow == ImGui::GetCurrentWindow() && g.NavCursorVisible) ||
                     (!gamepad_used_ && ImGui::IsWindowHovered());
  std::vector<std::array<ImGuiID, 2>> row_ids;
  for (int i = 0; i < int(rows.size()); ++i) {
    OptionRow& row = rows[size_t(i)];
    if (!row.group.empty()) {
      y += Px(10.0f);
      DrawLabel(kGroupTitle, ImVec2(row_left + Px(16.0f), y), col::kAccent, row.group);
      y += LineHeight(kGroupTitle) + Px(8.0f);
    }
    ImRect r(ImVec2(row_left, y), ImVec2(row_right, y + Px(68.0f)));
    ImGui::PushID(row.id.c_str());
    // The row's arrows are drawn over it: a click on one steps the value at once (the mouse needs no A first).
    ImGui::SetNextItemAllowOverlap();
    ItemState state = Item("##row", r);
    ImGuiID row_id = ImGui::GetItemID();
    bool focused = g.NavId == row_id && g.NavWindow == ImGui::GetCurrentWindow();
    if (selected == i) settings_row_focus_ = row_id;
    row_ids.push_back({row_id, row_id});
    if (focused || (state.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))) {
      selected = i;
    }
    bool on = selected == i && rows_active;
    if (on) {
      FillRect(r, Fade(col::kAccent, 0.14f), 10.0f);
      StrokeRect(r, col::kAccent, 10.0f, 3.0f);
    } else if (state.hovered) {
      FillRect(r, col::kHairline, 10.0f);
    }
    int count = int(row.values.size());
    auto step = [&](int delta) {
      if (row.locked || count < 2) return;
      row.index = (row.index + delta + count) % count;
      row.apply(row.index);
      selected = i;
      changed = true;
    };
    // A on the row in focus starts changing it: left and right step its value then (the focus stays) until A or B
    // (App::Frame). Otherwise left and right move the focus, left to the categories.
    bool changeable = !row.locked && count > 1;
    bool editing = settings_editing_ && g.NavId == row_id;
    if (editing) {
      editing_any = true;
      if (g.NavMoveSubmitted) {
        if (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right) {
          step(g.NavMoveDir == ImGuiDir_Left ? -1 : 1);
        }
        ImGui::NavMoveRequestCancel();
      }
    }
    if (state.pressed && PressedByNav() && changeable) {
      settings_editing_ = !settings_editing_;
      editing = settings_editing_;
      editing_any = editing;
    }

    // The value in a 280 dp block, its dots under it; the arrows around it, or a lock right of it.
    float right = r.Max.x - Px(12.0f);
    float value_right = row.locked ? right - Px(96.0f) - Px(16.0f) : right - Px(40.0f) - Px(16.0f);
    float value_left = value_right - Px(280.0f);
    if (row.locked) {
      DrawIcon(Icon::kLock, ImVec2(right - Px(48.0f) - Px(9.0f), r.GetCenter().y - Px(9.0f)), 18.0f, col::kTextDim, 2.0f);
    } else if (count > 1) {
      if (ArrowButton("##prev", ImVec2(value_left - Px(16.0f) - Px(40.0f), r.GetCenter().y - Px(20.0f)), true, editing)) step(-1);
      if (ArrowButton("##next", ImVec2(value_right + Px(16.0f), r.GetCenter().y - Px(20.0f)), false, editing)) step(1);
    }
    std::string value = row.Value();
    std::string tag = Upper(row.Tag());
    float tag_width = tag.empty() ? 0.0f : MeasureText(kTag, tag).x + Px(12.0f) + Px(8.0f);
    std::string shown = Ellipsize(kValue, value, Px(280.0f) - tag_width);
    float value_width = MeasureText(kValue, shown).x + tag_width;
    float block_height = LineHeight(kValue) + (row.locked ? 0.0f : Px(6.0f) + Px(3.0f));
    float value_y = r.GetCenter().y - block_height * 0.5f;
    float value_x = (value_left + value_right - value_width) * 0.5f;
    ImU32 value_color = row.locked ? col::kTabText : on ? col::kAccentLight : col::kText;
    DrawLabel(kValue, ImVec2(value_x, value_y), value_color, shown);
    if (!tag.empty()) {
      bool accent = !row.tag_accent.empty() && row.tag_accent[size_t(std::clamp(row.index, 0, int(row.tag_accent.size()) - 1))];
      float tag_x = value_x + MeasureText(kValue, shown).x + Px(8.0f);
      ImRect badge(ImVec2(tag_x, value_y + (LineHeight(kValue) - Px(18.0f)) * 0.5f),
                   ImVec2(tag_x + MeasureText(kTag, tag).x + Px(12.0f), value_y + (LineHeight(kValue) + Px(18.0f)) * 0.5f));
      FillRect(badge, accent ? col::kAccent : Fade(col::kGlyphY, 0.18f), 4.0f);
      DrawLabel(kTag, ImVec2(badge.Min.x + Px(6.0f), badge.Min.y), accent ? col::kOnAccent : col::kGlyphY, tag,
                badge.GetHeight());
    }
    if (!row.locked && count > 1) {
      float dots_width = float(count) * Px(18.0f) + float(count - 1) * Px(5.0f);
      float dot_x = (value_left + value_right - dots_width) * 0.5f;
      float dot_y = value_y + LineHeight(kValue) + Px(6.0f);
      for (int d = 0; d < count; ++d) {
        FillRect(ImRect(ImVec2(dot_x, dot_y), ImVec2(dot_x + Px(18.0f), dot_y + Px(3.0f))),
                 d == row.index ? col::kAccent : Hex(0xFFFFFF, 46), 2.0f);
        dot_x += Px(18.0f) + Px(5.0f);
      }
    }

    // The title and its line, left, cut to what is left of the row.
    float text_left = r.Min.x + Px(20.0f);
    float text_width = std::max(Px(40.0f), (row.locked ? value_left : value_left - Px(56.0f)) - Px(16.0f) - text_left);
    float text_height = LineHeight(kRowTitle) + Px(2.0f) + LineHeight(kRowSub);
    float text_y = r.GetCenter().y - text_height * 0.5f;
    DrawLabel(kRowTitle, ImVec2(text_left, text_y), row.locked ? col::kTabText : col::kText,
              Ellipsize(kRowTitle, row.title, text_width));
    DrawLabel(kRowSub, ImVec2(text_left, text_y + LineHeight(kRowTitle) + Px(2.0f)), col::kTabText,
              Ellipsize(kRowSub, row.Sub(), text_width));
    if (state.focused && !on) {
      FocusRing(r, 10.0f);
    }
    ImGui::PopID();
    y += Px(68.0f) + Px(8.0f);
  }
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(list.GetWidth(), y - origin.y - Px(8.0f)));
  // Up and down through the rows (a row being changed keeps the focus: its code took the move).
  ColumnNav(row_ids);
  // The focus left the row being changed (the mouse, another page): no row is.
  if (!editing_any) {
    settings_editing_ = false;
  }
  // While the focus is in the rows, B goes back to the categories (App::Frame).
  if (g.NavWindow == ImGui::GetCurrentWindow()) {
    back_focus_ = category_focus_;
    // Left from a row (not being changed): back to its category.
    if (g.NavMoveSubmitted && g.NavMoveDir == ImGuiDir_Left) {
      RequestFocus(category_focus_);
      ImGui::NavMoveRequestCancel();
    }
  }
  EndScroll();

  // The panel's foot: saved automatically; Reset (Y) for the category.
  {
    float top = footer_top - Px(0.0f);
    FillRect(ImRect(ImVec2(inner.Min.x, top - Px(12.0f)), ImVec2(inner.Max.x, top - Px(12.0f) + Snap(Px(1.0f)))),
             col::kHairline);
    constexpr Type kNote{Font::kRegular, 14};
    char reset[96];
    std::snprintf(reset, sizeof(reset), t_.reset_category, category_label);
    constexpr Type kReset{Font::kSemiBold, 14};
    float width = Px(14.0f) * 2.0f + GlyphWidth(PadButton::kY, 20.0f) + Px(8.0f) + MeasureText(kReset, reset).x;
    // The note wraps beside the button, its lines centered on it.
    float note_width = inner.GetWidth() - Px(8.0f) - Px(24.0f) - Px(16.0f) - width - Px(8.0f);
    float note_height = DrawParagraph(kNote, ImVec2(0, 0), note_width, 1.3f, 0, t_.autosave, false);
    float note_y = top + (Px(38.0f) - note_height) * 0.5f;
    DrawIcon(Icon::kCheck, ImVec2(inner.Min.x + Px(8.0f), note_y + (Px(14.0f) * 1.3f - Px(16.0f)) * 0.5f), 16.0f,
             col::kSuccess, 2.4f);
    DrawParagraph(kNote, ImVec2(inner.Min.x + Px(8.0f) + Px(24.0f), note_y), note_width, 1.3f, col::kTabText, t_.autosave);
    ImRect button(ImVec2(inner.Max.x - Px(8.0f) - width, top), ImVec2(inner.Max.x - Px(8.0f), top + Px(38.0f)));
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    ItemState state = Item("##reset", button);
    ImGui::PopItemFlag();
    if (state.hovered) FillRect(button, col::kHairline, 19.0f);
    StrokeRect(button, Hex(0xFFFFFF, 41), 19.0f);
    DrawGlyph(PadButton::kY, ImVec2(button.Min.x + Px(14.0f), button.GetCenter().y - Px(10.0f)), 20.0f);
    DrawLabel(kReset, ImVec2(button.Min.x + Px(14.0f) + GlyphWidth(PadButton::kY, 20.0f) + Px(8.0f), button.Min.y),
              col::kText, reset, button.GetHeight());
    bool reset_pressed = state.pressed || (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
                                           ButtonPressed(PadButton::kY));
    if (reset_pressed) {
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

  // The help of the row in focus: the category, its title and text, its current value.
  FillRect(help, col::kPanel, 14.0f);
  StrokeRect(help, Hex(0xFFFFFF, 26), 14.0f);
  if (!rows.empty()) {
    const OptionRow& row = rows[size_t(selected)];
    ImVec2 p(help.Min.x + Px(24.0f), help.Min.y + Px(24.0f));
    float width = help.GetWidth() - Px(48.0f);
    constexpr Type kKicker{Font::kBold, 12, 0.14f};
    DrawLabel(kKicker, p, col::kAccent, Upper(category_label));
    p.y += LineHeight(kKicker) + Px(14.0f);
    constexpr Type kHelpTitle{Font::kDisplayBold, 28};
    p.y += DrawParagraph(kHelpTitle, p, width, 1.1f, col::kText, row.title) + Px(14.0f);
    constexpr Type kHelpText{Font::kRegular, 16};
    DrawParagraph(kHelpText, p, width, 1.5f, col::kTextSoft, row.help);
    constexpr Type kCurrent{Font::kRegular, 13};
    constexpr Type kCurrentValue{Font::kDisplaySemiBold, 20};
    std::string value = row.Value();
    if (!row.Tag().empty()) value += " · " + row.Tag();
    float box_height = Px(14.0f) * 2.0f + LineHeight(kCurrent) + Px(4.0f) + LineHeight(kCurrentValue);
    ImRect box(ImVec2(help.Min.x + Px(24.0f), help.Max.y - Px(24.0f) - box_height),
               ImVec2(help.Max.x - Px(24.0f), help.Max.y - Px(24.0f)));
    FillRect(box, Hex(0xFFFFFF, 15), 10.0f);
    DrawLabel(kCurrent, ImVec2(box.Min.x + Px(16.0f), box.Min.y + Px(14.0f)), col::kTabText, t_.current_value);
    DrawLabel(kCurrentValue, ImVec2(box.Min.x + Px(16.0f), box.Min.y + Px(14.0f) + LineHeight(kCurrent) + Px(4.0f)),
              col::kText, Ellipsize(kCurrentValue, value, box.GetWidth() - Px(32.0f)));
  }

  if (changed) {
    Changed();
  }
}

}  // namespace launcher
