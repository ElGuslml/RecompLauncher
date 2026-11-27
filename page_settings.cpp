// The Settings tab (launcher 3.18, Gustavo's 2026-11-11 mockup "Camerino"; reordered by intention in 3.28, his
// "propuesta A v2"): the title and its categories on the left (Display, Graphics, Effects where the game has screen
// effects, Audio, Game, Mods, and Online where the game plays online: a dot marks the current one), the category's
// options as rows of 48 dp on the right, in groups under a title, a row that depends on the one above indented under
// it; each its value written as text (a switch for what is on or off). The row in focus grows to show its line of help,
// the arrows and its place among the values ("‹ value ›", the controller's left and right, A the next). Y opens the
// row's whole help, X resets the category. Every change is saved at once.

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <SDL3/SDL.h>

#include "app.h"
#include "net.h"
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
  std::string group;              // a group's title above the row (launcher 3.28: "Screen", "Upscaling", ...)
  bool child = false;             // it depends on the row above: indented under it, a line at its left (3.28)
  std::string notice;             // a line in a dashed box under the row (3.28: what Xenos lacks)
  bool is_switch = false;         // on (index 0) or off (index 1): drawn as a switch
  std::function<void(int)> apply;
  // A row that does something when chosen (A, a click) instead of stepping its value (launcher 3.20: a sheet to type the
  // value in, a copy to the clipboard); its icon before the value.
  std::function<void()> activate;
  Icon icon = Icon::kNone;
  std::vector<std::string> bullets;  // the help panel's list under its text

  std::string Value() const {
    if (locked) return locked_value;
    return values.empty() ? std::string() : values[size_t(std::clamp(index, 0, int(values.size()) - 1))];
  }
  std::string Sub() const { return subs.empty() ? sub : subs[size_t(std::clamp(index, 0, int(subs.size()) - 1))]; }
  std::string Tag() const {
    return locked || tags.empty() ? std::string() : tags[size_t(std::clamp(index, 0, int(tags.size()) - 1))];
  }
};

// The mockup's ".row" (launcher 3.28, "propuesta A v2"): 48 dp, the focused one 86 dp with 4 dp around it; its text
// 20 dp in, an indented row's 46 dp in (its focus 38 dp in, a line at 27 dp beside it).
constexpr float kRowHeight = 48.0f;
constexpr float kFocusedRowHeight = 86.0f;
constexpr float kFocusedRowMargin = 4.0f;
constexpr float kRowRounding = 14.0f;
constexpr float kRowPad = 20.0f;
constexpr float kChildLeft = 38.0f;
constexpr float kChildText = 46.0f;
constexpr float kChildLine = 27.0f;
constexpr Type kRowLabel{Font::kMedium, 18};
constexpr Type kRowLabelFocused{Font::kSemiBold, 19};
constexpr Type kRowValue{Font::kSemiBold, 18};
constexpr Type kRowValueFocused{Font::kSemiBold, 20};
constexpr Type kRowDescription{Font::kRegular, 15};
// A group's title (3.28): Oswald, a line 30 dp under its top; 12 dp above it at the top of the list, 23 dp elsewhere.
constexpr Type kGroupTitle{Font::kDisplayMedium, 18};
constexpr float kGroupFirstGap = 12.0f;
constexpr float kGroupGap = 23.0f;
constexpr float kGroupLine = 30.0f;
constexpr float kGroupBelowLine = 3.0f;
// A notice under a row (3.28): a dashed box 52 dp high, 6 dp under the row.
constexpr float kNoticeHeight = 52.0f;
constexpr float kNoticeAbove = 6.0f;
constexpr float kNoticeBelow = 5.0f;
constexpr Type kNoticeText{Font::kRegular, 15};

// The categories (launcher 3.30): the Mods tab's sections, then the Settings tab's categories, each in its pages' order
// (Page::kModsHighway to Page::kModsPacks, Page::kDisplay to Page::kUpdates).
enum Category {
  kCategoryHighway, kCategoryGameplay, kCategoryStage, kCategorySound, kCategoryTools, kCategoryPacks,
  kCategoryDisplay, kCategoryGraphics, kCategoryEffects, kCategoryAudio, kCategoryGame, kCategoryOnline, kCategoryUpdates,
  kCategoryCount
};
static_assert(int(Page::kModsPacks) - int(Page::kModsHighway) == kCategoryPacks - kCategoryHighway);
static_assert(int(Page::kUpdates) - int(Page::kDisplay) == kCategoryUpdates - kCategoryDisplay);

int CategoryOf(Page page) {
  if (IsModsPage(page)) return kCategoryHighway + (int(page) - int(Page::kModsHighway));
  return kCategoryDisplay + std::clamp(int(page) - int(Page::kDisplay), 0, kCategoryUpdates - kCategoryDisplay);
}

// Whether the game has a switch in that group (launcher 3.37: a group's title only where it has rows).
bool HasToggles(game::ToggleGroup group) {
  for (const game::Toggle& t : game::kToggles) {
    if (t.group == group) return true;
  }
  return false;
}

// A Mods section with rows in this game (launcher 3.30): Highway with the gallery or a switch or row of its groups,
// Gameplay (launcher 3.38),
// Stage and Sound with one of theirs, Tools always (the launcher's Debug keys); launcher next, Packs where the game has
// mod packs.
bool ModsSectionHasRows(int section) {
  auto has = [](std::initializer_list<game::ToggleGroup> groups) {
    for (const game::Toggle& t : game::kToggles) {
      if (std::find(groups.begin(), groups.end(), t.group) != groups.end()) return true;
    }
    for (const game::Choice& c : game::kChoices) {
      if (std::find(groups.begin(), groups.end(), c.group) != groups.end()) return true;
    }
    return false;
  };
  switch (section) {
    case 0:
      return game::kHighwayPreview || has({game::ToggleGroup::kModsHighway, game::ToggleGroup::kModsHighwayLook,
                                           game::ToggleGroup::kModsNoteEffects});
    case 1:
      return has({game::ToggleGroup::kModsGameplay});
    case 2:
      return has({game::ToggleGroup::kModsStage, game::ToggleGroup::kModsVenues});
    case 3:
      return has({game::ToggleGroup::kModsSound});
    case 5:
      return game::kHasModPacks;
    default:
      return true;
  }
}

Page PageOf(int category) {
  return category < kCategoryDisplay ? Page(int(Page::kModsHighway) + category - kCategoryHighway)
                                     : Page(int(Page::kDisplay) + category - kCategoryDisplay);
}
// Launcher 3.22, the rows as an accordion: the mouse's row opens after it rests there this long (crossing the list
// opens nothing), over this long.
constexpr double kHoverDelay = 0.08;
constexpr float kOpenSeconds = 0.2f;

// Cubic, eased in and out: e(1 - t) = 1 - e(t), so a row opening and one closing at once change by the same height.
float EaseInOut(float t) {
  return t < 0.5f ? 4.0f * t * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * 0.5f;
}

// What was drawn since `start` (a vertex index of `draw`) made fainter: alpha times `alpha`.
void FadeVertices(ImDrawList* draw, int start, float alpha) {
  if (alpha >= 1.0f) return;
  for (int i = start; i < draw->VtxBuffer.Size; ++i) {
    ImU32& c = draw->VtxBuffer[i].col;
    uint32_t a = (c >> IM_COL32_A_SHIFT) & 0xFF;
    c = (c & ~IM_COL32_A_MASK) | (uint32_t(float(a) * std::max(alpha, 0.0f) + 0.5f) << IM_COL32_A_SHIFT);
  }
}

// A chevron of the focused row (the mouse's; the controller steps with left and right).
bool ArrowButton(const char* id, ImVec2 pos, bool left, bool active = false) {
  ImRect rect(pos, ImVec2(pos.x + Px(28.0f), pos.y + Px(28.0f)));
  ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
  ItemState state = Item(id, rect);
  ImGui::PopItemFlag();
  if (state.hovered) FillRect(rect, Fade(col::kFocus, 0.08f), 14.0f);
  // While the row is changing (A), the arrows in the accent: left and right step it.
  DrawIcon(left ? Icon::kChevronLeft : Icon::kChevronRight, ImVec2(pos.x + Px(2.0f), pos.y + Px(2.0f)), 24.0f,
           active ? col::kAccent : state.hovered ? col::kText : col::kTextSoft, active ? 2.4f : 1.9f);
  return state.pressed;
}

// "experimental" as a chip's text: its first letter in capitals (the five languages' words start with an ASCII one).
std::string ChipText(const std::string& text) {
  std::string chip = text;
  if (!chip.empty() && chip[0] >= 'a' && chip[0] <= 'z') chip[0] = char(chip[0] - 'a' + 'A');
  return chip;
}

// The playback devices' names (SDL's, the names Windows shows and the SDK's audio_output_device matches), asked again
// every two seconds while the page is shown: a headset plugged in appears.
const std::vector<std::string>& AudioOutputs() {
  static std::vector<std::string> names;
  static uint64_t asked = 0;
  const uint64_t now = SDL_GetTicks();
  if (asked && now - asked < 2000) return names;
  asked = now;
  names.clear();
  if (!SDL_WasInit(SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return names;
  int count = 0;
  SDL_AudioDeviceID* ids = SDL_GetAudioPlaybackDevices(&count);
  for (int i = 0; ids && i < count; ++i) {
    const char* name = SDL_GetAudioDeviceName(ids[i]);
    if (name && *name) names.push_back(name);
  }
  SDL_free(ids);
  return names;
}

// The router's internet address (net::AskRouter, up to ~5 s), asked on a thread of its own when the row is first shown
// and again a minute after each answer while it is shown; null until the first answer. The state outlives the thread
// (never freed: a question may still be out when the launcher closes).
const net::RouterAddress* RouterAnswer() {
  struct State {
    std::mutex mutex;
    bool running = false;
    bool answered = false;
    uint64_t answered_at = 0;
    net::RouterAddress answer;
  };
  static State* state = new State;
  static net::RouterAddress shown;
  std::lock_guard lock(state->mutex);
  const uint64_t now = SDL_GetTicks();
  if (!state->running && (!state->answered || now - state->answered_at > 60000)) {
    state->running = true;
    std::thread([] {
      net::RouterAddress answer = net::AskRouter();
      std::lock_guard lock(state->mutex);
      state->answer = std::move(answer);
      state->answered = true;
      state->answered_at = SDL_GetTicks();
      state->running = false;
    }).detach();
  }
  if (!state->answered) return nullptr;
  shown = state->answer;
  return &shown;
}

// This PC's addresses, asked again every two seconds while the page is shown (a network connected).
const std::vector<net::LocalAddress>& LocalAddresses() {
  static std::vector<net::LocalAddress> addresses;
  static uint64_t asked = 0;
  const uint64_t now = SDL_GetTicks();
  if (asked && now - asked < 2000) return addresses;
  asked = now;
  addresses = net::LocalAddresses();
  return addresses;
}

}  // namespace

void App::SettingsPage(const ImRect& rect) {
  Options& o = settings_.o;
  const game::GameTexts& own = game::GameTextsFor(language_);
  bool changed = false;
  // The category (launcher 3.30): a section of the Mods tab, or a category of the Settings tab.
  if (IsModsPage(page_) && !ModsSectionHasRows(CategoryOf(page_) - kCategoryHighway)) {
    for (int c = kCategoryHighway; c <= kCategoryTools; ++c) {
      if (ModsSectionHasRows(c - kCategoryHighway)) {
        page_ = PageOf(c);
        break;
      }
    }
  }
  const bool mods_tab = IsModsPage(page_);
  const int category = CategoryOf(page_);

  // --- The rows of the category ---
  std::vector<OptionRow> rows;
  const char* online_intro = game::kOnlineByAddress ? t_.settings_intro_online : t_.settings_intro_online_steam;
  const char* const category_labels[] = {t_.group_highway, t_.group_gameplay, t_.group_stage,  t_.group_sound,
                                         t_.group_tools,   t_.group_packs,    t_.nav_display,  t_.nav_graphics,
                                         t_.nav_effects,   t_.nav_audio,      t_.nav_game,     t_.nav_online,
                                         t_.nav_updates};
  const char* const intros[] = {t_.mods_intro_highway,     t_.mods_intro_gameplay,    t_.mods_intro_stage,
                                t_.mods_intro_sound,       t_.mods_intro_tools,       t_.mods_intro_packs,
                                t_.settings_intro_display, t_.settings_intro_graphics, t_.settings_intro_effects,
                                t_.settings_intro_audio,   t_.settings_intro_game,    online_intro,
                                t_.settings_intro_updates};
  static_assert(std::size(category_labels) == kCategoryCount && std::size(intros) == kCategoryCount);
  const char* category_label = category_labels[category];
  const char* intro = intros[category];
  Size output = settings_.OutputSize();
  double aspect = ImageAspect(output, o.native_aspect);

  // Rows go in groups (launcher 3.28): group() names the next one, push() adds a row, the first after group() under
  // its title (a group with no rows shows nothing).
  std::string next_group;
  auto group = [&](const char* title) { next_group = title ? title : ""; };
  auto push = [&](OptionRow&& row) {
    if (!next_group.empty()) {
      row.group = std::move(next_group);
      next_group.clear();
    }
    rows.push_back(std::move(row));
  };
  // Whether a switch of the game's own is shown (its ToggleShown; one that is not keeps its value).
  auto toggle_shown = [&](const game::Toggle& toggle) {
    switch (toggle.shown) {
      case game::ToggleShown::kSceneUpscaled:
        return o.native && SceneUpscaled(o, output);
      case game::ToggleShown::kDlssUpscales:
        return o.native && game::kHasDlaa && o.upscaler == kUpscaleDlss &&
               FitUpscaleQuality(o.upscaler, o.upscale_quality, output, aspect) != kQualityNative;
      default:
        return true;
    }
  };
  // The row of the game's own with several values named `cvar` (game.h's kChoices), or -1.
  auto choice_index = [](const char* cvar) {
    for (size_t j = 0; cvar && j < game::kChoices.size(); ++j) {
      if (std::string_view(game::kChoices[j].cvar) == cvar) return int(j);
    }
    return -1;
  };
  // The game's switches of a group, in game.h's order. `children`: each under the row above, indented (Upscaling's);
  // a switch that needs another is under that one anyway, and one that needs a row with several values (launcher
  // 3.32) right under that row.
  auto toggle_row = [&](size_t i, bool children) {
    const game::Toggle& toggle = game::kToggles[i];
    OptionRow row;
    row.id = toggle.cvar;
    row.title = own.toggles[i * 2];
    row.sub = own.toggles[i * 2 + 1];
    row.help = own.toggle_help[i] ? own.toggle_help[i] : own.toggles[i * 2 + 1];
    row.values = {t_.on_value, t_.off};
    row.is_switch = true;
    row.index = o.toggles[i] ? 0 : 1;
    row.child = children || toggle.needs;
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
    // Launcher 3.32: a switch that needs a row with several values, locked while that row is at its default (where the
    // switch changes nothing; GH Warriors of Rock's notes' speed at a perspective of 100 %). Drawn off as every locked
    // switch; its value is kept, and shows again when the row changes.
    if (const int c = choice_index(toggle.needs); c >= 0) {
      const game::Choice& choice = game::kChoices[size_t(c)];
      const int at = std::clamp(o.choices[size_t(c)], 0, int(choice.values.size()) - 1);
      if (std::string_view(choice.values[size_t(at)]) == choice.default_value) {
        size_t default_index = 0;
        while (default_index + 1 < choice.values.size() &&
               std::string_view(choice.values[default_index]) != choice.default_value) {
          ++default_index;
        }
        const char* label =
            default_index < own.choice_values[size_t(c)].size() ? own.choice_values[size_t(c)][default_index] : nullptr;
        const std::string value = label ? std::string(label) : std::string(choice.default_value) + choice.suffix;
        row.locked = true;
        row.locked_value = t_.off;
        char needs[200];
        std::snprintf(needs, sizeof(needs), t_.needs_other_value, own.choices[size_t(c) * 2], value.c_str());
        row.sub = needs;
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
    push(std::move(row));
  };
  auto toggle_rows = [&](game::ToggleGroup toggle_group, bool children = false) {
    for (size_t i = 0; i < game::kToggles.size(); ++i) {
      const game::Toggle& toggle = game::kToggles[i];
      if (toggle.group != toggle_group || !toggle_shown(toggle)) continue;
      // Under the row with several values it needs (below).
      if (choice_index(toggle.needs) >= 0) continue;
      toggle_row(i, children);
    }
    // Then the group's rows with several values (launcher 3.29), in game.h's order.
    for (size_t i = 0; i < game::kChoices.size(); ++i) {
      const game::Choice& choice = game::kChoices[i];
      if (choice.group != toggle_group) continue;
      OptionRow row;
      row.id = choice.cvar;
      row.title = own.choices[i * 2];
      row.sub = own.choices[i * 2 + 1];
      row.help = own.choice_help[i] ? own.choice_help[i] : own.choices[i * 2 + 1];
      for (size_t j = 0; j < choice.values.size(); ++j) {
        const char* label = j < own.choice_values[i].size() ? own.choice_values[i][j] : nullptr;
        row.values.push_back(label ? std::string(label) : std::string(choice.values[j]) + choice.suffix);
      }
      row.index = std::clamp(o.choices[i], 0, int(choice.values.size()) - 1);
      row.child = children || choice.needs;
      // Launcher 3.30: a row that needs a switch, locked with its value while that one is off.
      if (choice.needs) {
        for (size_t j = 0; j < game::kToggles.size(); ++j) {
          if (std::string_view(game::kToggles[j].cvar) == choice.needs && !o.toggles[j]) {
            row.locked = true;
            row.locked_value = row.values[size_t(row.index)];
            char needs[160];
            std::snprintf(needs, sizeof(needs), t_.screen_effects_needs, own.toggles[j * 2]);
            row.sub = needs;
          }
        }
      }
      row.apply = [&o, i](int index) { o.choices[i] = index; };
      push(std::move(row));
      // The switches that need this row, right under it (launcher 3.32).
      for (size_t k = 0; k < game::kToggles.size(); ++k) {
        const game::Toggle& toggle = game::kToggles[k];
        if (toggle.needs && std::string_view(toggle.needs) == choice.cvar && toggle_shown(toggle)) toggle_row(k, true);
      }
    }
  };

  if (category == kCategoryDisplay) {
    // Where the game opens and how it fills the screen; then how smooth it is.
    group(t_.group_screen);
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
      push(std::move(row));
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
      push(std::move(row));
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
      row.child = true;  // under Display mode, which decides it
      push(std::move(row));
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
      push(std::move(row));
    }
    group(t_.group_smoothness);
    // The frame rate: the rates the monitor reaches; those above 60 are experimental.
    if (*game::kFrameRateCvar) {
      const Monitor& monitor = settings_.CurrentMonitor();
      std::vector<game::FrameRate> rates = FrameRates(monitor, o.any_frame_rate);
      OptionRow row;
      row.id = "fps";
      row.title = t_.framerate;
      row.sub = settings_.FrameGenerationOn() ? t_.framerate_with_generation : t_.framerate_hint;
      row.help = row.sub;
      for (size_t i = 0; i < rates.size(); ++i) {
        row.values.push_back(rates[i].fps == 0 ? std::string(t_.framerate_unlimited)
                                               : std::to_string(rates[i].fps) + " FPS");
        row.tags.push_back(rates[i].fps > 60 ? t_.experimental : std::string());
        row.tag_accent.push_back(false);
        if (rates[i].value == o.frame_rate) row.index = int(i);
      }
      // 3.31: every rate is offered. Above the monitor's with V-Sync on the game runs at the monitor's (Gustavo:
      // "V-Sync manda"); above 240 its timing is still being checked.
      const bool above_monitor = o.frame_rate > monitor.refresh + 1;
      if (!settings_.FrameGenerationOn() && o.vsync && above_monitor) {
        static char capped[256];
        std::snprintf(capped, sizeof(capped), t_.framerate_vsync_caps, monitor.refresh);
        row.sub = capped;
      } else if (!settings_.FrameGenerationOn() && o.frame_rate > 240) {
        row.sub = t_.framerate_above_240;
      }
      row.help = row.sub;
      row.apply = [&o, rates](int index) { o.frame_rate = rates[size_t(index)].value; };
      push(std::move(row));
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
      push(std::move(row));
    }
    toggle_rows(game::ToggleGroup::kSmoothness);
  } else if (category == kCategoryGraphics) {
    // What draws the game (it decides what can be chosen below), how its 3D scene is enlarged, and the image's detail.
    // With Xenos the internal resolution is the scale's, among the image's rows.
    group(t_.renderer_title);
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
      push(std::move(row));
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
      row.child = game::kHasNativeXRenderer;  // under Renderer, Xenos's
      push(std::move(row));
    }
    // Launcher 3.19, the SDK's change 86: the graphics card, automatic or one by its PCI IDs (a laptop's integrated
    // and dedicated ones); a chosen one not found now stays chosen, marked.
    {
      OptionRow row;
      row.id = "gpu";
      row.title = t_.gpu;
      row.sub = t_.gpu_hint;
      row.help = t_.gpu_help;
      row.values.push_back(t_.gpu_automatic);
      std::vector<std::string> ids = {""};
      for (const GpuAdapter& gpu : settings_.gpus) {
        if (gpu.id == o.gpu_adapter) row.index = int(row.values.size());
        row.values.push_back(gpu.name);
        ids.push_back(gpu.id);
      }
      if (!o.gpu_adapter.empty() && row.index == 0) {
        char missing[160];
        std::snprintf(missing, sizeof(missing), t_.audio_output_missing, o.gpu_adapter.c_str());
        row.index = int(row.values.size());
        row.values.push_back(missing);
        ids.push_back(o.gpu_adapter);
      }
      row.apply = [&o, ids](int index) { o.gpu_adapter = ids[size_t(index)]; };
      // Xenos chosen where NativeXRenderer could draw: what only NativeXRenderer has, said under it.
      if (!o.native && game::kHasNativeXRenderer) {
        row.notice = *game::kNoMsaaCvar ? t_.xenos_notice_msaa
                     : game::kHasDlaa && game::kOffersTemporalUpscalers ? t_.xenos_notice_dlss
                                                                         : t_.xenos_notice;
      }
      push(std::move(row));
    }
    group(o.native ? t_.scaling : t_.group_image);
    // NativeXRenderer's upscaling (launcher 3.9, settings.h): off, AMD FSR 1, NVIDIA NIS, and AMD FSR 3.1 and NVIDIA DLSS
    // where the game marks its views (game::kHasDlaa); then its quality, which sets the internal resolution.
    if (o.native) {
      if (!game::kHasDlaa && IsTemporal(o.upscaler)) o.upscaler = kUpscaleOff;
      std::vector<int> upscalers = {kUpscaleOff, kUpscaleFsr1, kUpscaleNis};
      OptionRow row;
      row.id = "scaling";
      row.title = t_.scaling;
      row.sub = t_.scaling_hint;
      row.help = game::kLowResolutions ? t_.scaling_help_low : t_.scaling_help;
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
      push(std::move(row));
    }
    if (o.native && o.upscaler != kUpscaleOff) {
      o.upscale_quality = FitUpscaleQuality(o.upscaler, o.upscale_quality, output, aspect);
      std::vector<int> qualities = UpscaleQualities(o.upscaler, output, aspect);
      const char* names[] = {t_.quality_native, t_.quality_ultra,       t_.quality_quality,
                             t_.quality_balanced, t_.quality_performance, t_.quality_ultra_performance};
      OptionRow row;
      row.id = "quality";
      row.title = t_.quality;
      row.help = game::kLowResolutions ? t_.quality_help_low : t_.quality_help;
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
          // On the 3D scene only (3.27): the scene's size; otherwise the whole image's internal resolution.
          Size size = LowersScene(o) ? TemporalInputSize(o.upscaler, output, aspect, quality)
                                     : NativeRenderSize(output, aspect, SpatialHeight(output, aspect, quality));
          std::snprintf(from, sizeof(from), t_.quality_from, SizeText(size).c_str());
          row.subs.push_back(from);
          row.tags.push_back("");
        }
        row.tag_accent.push_back(false);
        if (quality == o.upscale_quality) row.index = int(row.values.size()) - 1;
      }
      row.apply = [&o, qualities](int index) { o.upscale_quality = qualities[size_t(index)]; };
      row.child = true;  // under Upscaling
      push(std::move(row));
    }
    // DLSS's model (launcher 3.24, game::kHasDlssModel; NativeXRenderer's native_x_renderer_dlss_model): Transformer,
    // NVIDIA's own for each mode (recommended), Transformer K, the first Transformer model in every mode (launcher
    // 3.26), or CNN, the earlier one.
    if (game::kHasDlssModel && o.native && o.upscaler == kUpscaleDlss) {
      OptionRow row;
      row.id = "dlss_model";
      row.title = t_.dlss_model;
      row.help = t_.dlss_model_help;
      row.values = {"Transformer", "Transformer K", "CNN"};
      row.subs = {t_.dlss_model_transformer_sub, t_.dlss_model_transformer_k_sub, t_.dlss_model_cnn_sub};
      row.tags = {t_.recommended, "", ""};
      row.tag_accent = {true, false, false};
      row.index = std::clamp(o.dlss_model, 0, kDlssModelCount - 1);
      row.apply = [&o](int index) { o.dlss_model = index; };
      row.child = true;  // under Upscaling
      push(std::move(row));
    }
    // One more texture level with DLSS or FSR 3.1 (3.31, game::kTextureDetail; NativeXRenderer's
    // native_x_renderer_upscaler_lod_bias = -1): the vendors' advice, which their accumulation over frames smooths.
    if (game::kTextureDetail && o.native && IsTemporal(o.upscaler)) {
      OptionRow row;
      row.id = "upscaler_texture_detail";
      row.title = t_.upscaler_texture_detail;
      row.sub = t_.upscaler_texture_detail_hint;
      row.help = t_.upscaler_texture_detail_help;
      row.values = {t_.off, t_.on};
      row.index = o.upscaler_texture_detail ? 1 : 0;
      row.apply = [&o](int index) { o.upscaler_texture_detail = index == 1; };
      row.child = true;  // under Upscaling
      push(std::move(row));
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
      row.child = true;  // under Upscaling
      push(std::move(row));
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
      if (o.native && IsSpatial(o.upscaler) && !LowersScene(o)) {
        row.locked = true;
        row.locked_value = SizeText(NativeRenderSize(output, aspect, SpatialHeight(output, aspect, o.upscale_quality)));
        row.sub = t_.internal_by_quality;
      } else if (o.native && (IsTemporal(o.upscaler) || IsSpatial(o.upscaler))) {
        // FSR 3.1 and DLSS draw only the 3D scene at the quality's size; the HUD and the game's effects are drawn at
        // the screen's, which the upscaler rebuilds the scene at (launcher 3.9.1: 3.9 showed the screen's size here).
        // FSR 1 and NIS too where they take the 3D scene only (3.27, LowersScene).
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
        // A lower one on the 3D scene only (3.27): said under it.
        if (LowersScene(o) && o.native_height > 0) row.sub = t_.internal_scene_only;
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
      row.child = o.native;  // NativeXRenderer's under Upscaling, which sets it or leaves it to choose
      push(std::move(row));
    }
    // What a lower resolution lowers (3.27, game::kLowResolutions with the game's views): the 3D scene only
    // (recommended: the HUD, the menus and the game's effects stay sharp) or the whole image (the lightest). Not with
    // FSR 3.1 and DLSS, which draw the 3D scene only.
    if (o.native && SceneLowerable() && !IsTemporal(o.upscaler)) {
      OptionRow row;
      row.id = "lower";
      row.title = t_.lower_for;
      row.sub = t_.lower_for_hint;
      row.help = t_.lower_for_help;
      row.values = {t_.lower_scene, t_.lower_image};
      row.subs = {t_.lower_scene_sub, t_.lower_image_sub};
      row.tags = {t_.recommended, ""};
      row.tag_accent = {true, false};
      row.index = o.lower_scene ? 0 : 1;
      row.apply = [&o](int index) { o.lower_scene = index == 0; };
      row.child = true;  // under Upscaling
      push(std::move(row));
    }
    if (o.native) {
      toggle_rows(game::ToggleGroup::kUpscaling, true);
      group(t_.group_image);
    }
    // Antialiasing: NativeXRenderer's SMAA or FXAA (FSR 3.1 and DLSS, under Upscaling, do their own), or the game's
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
      push(std::move(row));
    } else if (*game::kNoMsaaCvar) {
      OptionRow row;
      row.id = "msaa";
      row.title = t_.antialiasing;
      row.sub = t_.antialiasing_hint_xenos;
      row.help = t_.antialiasing_hint_xenos;
      row.values = {game::kMsaaLabel, t_.none};
      row.index = o.no_msaa ? 1 : 0;
      row.apply = [&o](int index) { o.no_msaa = index == 1; };
      push(std::move(row));
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
      push(std::move(row));
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
      push(std::move(row));
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
      push(std::move(row));
    }
    // Ambient occlusion of NativeXRenderer's own (3.31, native_x_renderer_ao), locked with Xenos.
    if (game::kAmbientOcclusion) {
      OptionRow row;
      row.id = "ambient_occlusion";
      row.title = t_.ambient_occlusion;
      row.sub = o.native ? t_.ambient_occlusion_hint : t_.shadows_hint_xenos;
      row.help = o.native ? t_.ambient_occlusion_help : t_.shadows_hint_xenos;
      row.values = {"Original", t_.sharpness_low, t_.sharpness_medium, t_.sharpness_high, "Ultra"};
      row.index = o.native ? std::clamp(o.ambient_occlusion, 0, 4) : 0;
      if (!o.native) {
        row.locked = true;
        row.locked_value = "Original";
      }
      row.apply = [&o](int index) { o.ambient_occlusion = index; };
      push(std::move(row));
    }
    // The main 3D view's textures sharper (3.31, native_x_renderer_texture_lod_bias), locked with Xenos.
    if (game::kTextureDetail) {
      OptionRow row;
      row.id = "texture_detail";
      row.title = t_.texture_detail;
      row.sub = o.native ? t_.texture_detail_hint : t_.shadows_hint_xenos;
      row.help = o.native ? t_.texture_detail_help : t_.shadows_hint_xenos;
      row.values = {t_.texture_detail_normal, t_.texture_detail_sharper, t_.texture_detail_sharpest};
      row.index = o.native ? std::clamp(o.texture_detail, 0, 2) : 0;
      if (!o.native) {
        row.locked = true;
        row.locked_value = t_.texture_detail_normal;
      }
      row.apply = [&o](int index) { o.texture_detail = index; };
      push(std::move(row));
    }
  } else if (category == kCategoryEffects) {
    toggle_rows(game::ToggleGroup::kScreenEffects);
  } else if (category == kCategoryAudio) {
    // Launcher 3.19, the SDK's change 85. The output device: Windows' default (followed when it changes), or one by
    // its name; a chosen one not connected now stays chosen, marked.
    {
      OptionRow row;
      row.id = "audio_output";
      row.title = t_.audio_output;
      row.sub = t_.audio_output_hint;
      row.help = t_.audio_output_help;
      row.values.push_back(t_.audio_output_default);
      std::vector<std::string> devices = {""};
      for (const std::string& name : AudioOutputs()) {
        if (name == o.audio_device) row.index = int(row.values.size());
        row.values.push_back(name);
        devices.push_back(name);
      }
      if (!o.audio_device.empty() && row.index == 0) {
        char missing[320];
        std::snprintf(missing, sizeof(missing), t_.audio_output_missing, o.audio_device.c_str());
        row.index = int(row.values.size());
        row.values.push_back(missing);
        devices.push_back(o.audio_device);
      }
      row.apply = [&o, devices](int index) { o.audio_device = devices[size_t(index)]; };
      push(std::move(row));
    }
    // The channels: automatic (5.1 where the device has it), stereo, mono.
    {
      OptionRow row;
      row.id = "audio_channels";
      row.title = t_.audio_channels;
      row.help = t_.audio_channels_help;
      if (game::kSpeakerChoice) {
        // 3.31: 5.1 too, the game told the speakers it plays on; in this order.
        static constexpr int kOrder[] = {kAudioAuto, kAudioSurround, kAudioStereo, kAudioMono};
        row.help = t_.audio_channels_speakers_help;
        row.values = {t_.audio_channels_auto, "5.1", t_.audio_channels_stereo, t_.audio_channels_mono};
        row.subs = {t_.audio_channels_auto_speakers_sub, t_.audio_channels_surround_sub,
                    t_.audio_channels_stereo_speakers_sub, t_.audio_channels_mono_sub};
        row.index = int(std::find(std::begin(kOrder), std::end(kOrder), o.audio_channels) - std::begin(kOrder));
        if (row.index >= int(std::size(kOrder))) row.index = 0;
        row.apply = [&o](int index) { o.audio_channels = kOrder[index]; };
      } else {
        row.values = {t_.audio_channels_auto, t_.audio_channels_stereo, t_.audio_channels_mono};
        row.subs = {t_.audio_channels_auto_sub, t_.audio_channels_stereo_sub, t_.audio_channels_mono_sub};
        row.index = std::clamp(o.audio_channels, 0, int(kAudioMono));
        row.apply = [&o](int index) { o.audio_channels = index; };
      }
      push(std::move(row));
    }
    // The volume, 0-200 % in tens (above 100 a gain above the game's own: its loudest moments can clip).
    {
      OptionRow row;
      row.id = "audio_volume";
      row.title = t_.audio_volume;
      row.sub = t_.audio_volume_hint;
      row.help = t_.audio_volume_help;
      for (int v = 0; v <= 200; v += 10) row.values.push_back(std::to_string(v) + " %");
      row.index = std::clamp((o.audio_volume + 5) / 10, 0, 20);
      row.apply = [&o](int index) { o.audio_volume = index * 10; };
      push(std::move(row));
    }
    // Silence while another window is in front.
    {
      OptionRow row;
      row.id = "audio_mute_unfocused";
      row.title = t_.audio_mute_unfocused;
      row.sub = t_.audio_mute_unfocused_hint;
      row.help = t_.audio_mute_unfocused_help;
      row.values = {t_.on, t_.off};
      row.is_switch = true;
      row.index = o.audio_mute_unfocused ? 0 : 1;
      row.apply = [&o](int index) { o.audio_mute_unfocused = index == 0; };
      push(std::move(row));
    }
#ifdef _WIN32
    // Low latency audio (moved from Game in 3.19).
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
      push(std::move(row));
    }
#endif
  } else if (category == kCategoryGame) {
    group(t_.group_language);
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
      push(std::move(row));
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
      push(std::move(row));
    }
    group(t_.group_start);
    toggle_rows(game::ToggleGroup::kStart);
    group(t_.group_game_other);
    toggle_rows(game::ToggleGroup::kGame);
  } else if (category == kCategoryHighway) {
    // The Mods tab (launcher 3.30): the note highway's picture (the gallery, game::kHighwayGallery), its shape and its
    // look (launcher 3.39: the pictures only where the game reads them, kHighwayPictures).
    if (kHighwayPictures) {
      if (!highway_pictures_) highway_pictures_ = std::make_unique<highway::Pictures>();
      highway_pictures_->Refresh(o.highway_folder);
      group(t_.group_highway_background);
      // The folder: A chooses another (its last name shown).
      {
        OptionRow row;
        row.id = "highway_folder";
        row.title = t_.highway_folder;
        row.sub = t_.highway_folder_sub;
        row.help = t_.highway_folder_help;
        std::string shown = o.highway_folder;
        while (!shown.empty() && (shown.back() == '\\' || shown.back() == '/')) shown.pop_back();
        const size_t last = shown.find_last_of("\\/");
        if (last != std::string::npos && last > 0) shown = "\xE2\x80\xA6" + shown.substr(last);
        row.values = {shown};
        row.icon = Icon::kFolder;
        // A list under it (launcher 3.30.1): open it in Explorer, or change it. Called right after the row's item.
        row.activate = [this] {
          folder_menu_ = HighwayFolderMenu{};
          folder_menu_.open = true;
          folder_menu_.anchor = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        };
        push(std::move(row));
      }
      // The picture: A opens the gallery; its line counts the folder's pictures and favorites.
      {
        OptionRow row;
        row.id = "highway_image";
        row.title = t_.highway_image;
        const auto& list = highway_pictures_->List();
        int favorites = 0;
        for (const highway::Picture& picture : list) favorites += IsHighwayFavorite(o, picture.name) ? 1 : 0;
        char sub[192];
        std::snprintf(sub, sizeof(sub), t_.highway_image_sub, int(list.size()), favorites);
        row.sub = list.empty() ? std::string(t_.highway_image_sub_empty) : std::string(sub);
        row.help = t_.highway_image_help;
        row.values = {HighwayImageLabel()};
        row.icon = Icon::kChevronRight;
        row.activate = [this] { OpenHighwayGallery(); };
        push(std::move(row));
      }
      // Where Random's picture comes from.
      {
        OptionRow row;
        row.id = "highway_random_from";
        row.title = t_.highway_random_from;
        row.sub = t_.highway_random_from_sub;
        row.help = t_.highway_random_from_help;
        row.values = {t_.highway_all, t_.highway_favorites};
        row.index = o.highway_random_favorites ? 1 : 0;
        row.apply = [&o](int index) { o.highway_random_favorites = index == 1; };
        push(std::move(row));
      }
    }
    group(t_.group_highway_shape);
    toggle_rows(game::ToggleGroup::kModsHighway);
    group(t_.group_highway_look);
    toggle_rows(game::ToggleGroup::kModsHighwayLook);
    // Every picture darkened (each one's own in the gallery, Y).
    if (kHighwayPictures) {
      OptionRow row;
      row.id = "highway_dim";
      row.title = t_.highway_dim;
      row.sub = t_.highway_dim_sub;
      row.help = t_.highway_dim_help;
      for (const char* value : kHighwayDims) {
        const std::string_view v(value);
        row.values.push_back(v == "off" ? std::string(t_.off) : v == "auto" ? std::string(t_.highway_dim_auto)
                                                                            : std::string(v) + " %");
      }
      row.index = 0;
      for (size_t i = 0; i < std::size(kHighwayDims); ++i) {
        if (o.highway_dim == kHighwayDims[i]) row.index = int(i);
      }
      row.apply = [&o](int index) { o.highway_dim = kHighwayDims[size_t(index)]; };
      push(std::move(row));
    }
    // Launcher 3.37: the flames and sparks on the strike line, where the game has switches for them.
    if (HasToggles(game::ToggleGroup::kModsNoteEffects)) {
      group(t_.group_note_effects);
      toggle_rows(game::ToggleGroup::kModsNoteEffects);
    }
  } else if (category == kCategoryGameplay) {
    toggle_rows(game::ToggleGroup::kModsGameplay);
  } else if (category == kCategoryStage) {
    toggle_rows(game::ToggleGroup::kModsStage);
    // Launcher 3.37: the venues the game may pick at random, a switch each.
    if (HasToggles(game::ToggleGroup::kModsVenues)) {
      group(t_.group_venues);
      toggle_rows(game::ToggleGroup::kModsVenues);
    }
  } else if (category == kCategorySound) {
    toggle_rows(game::ToggleGroup::kModsSound);
  } else if (category == kCategoryTools) {
    toggle_rows(game::ToggleGroup::kModsTools);
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
      push(std::move(row));
    }
  } else if (category == kCategoryOnline) {
    // Launcher 3.20: online play between friends (the SDK's online_play, experimental). While it is off, the other rows
    // are locked with their values.
    auto lock_while_off = [&](OptionRow& row) {
      if (o.online) return;
      std::string value = row.Value();
      row.locked = true;
      row.locked_value = value;
      row.sub = t_.online_needs_on;
      row.activate = nullptr;
      row.icon = Icon::kNone;
    };
    {
      OptionRow row;
      row.id = "online_play";
      row.title = t_.online_play;
      row.sub = game::kOnlineByAddress ? t_.online_play_hint : t_.online_play_hint_steam;
      row.help = game::kOnlineByAddress ? t_.online_play_help : t_.online_play_help_steam;
      row.values = {t_.on, t_.off};
      row.tags = {t_.experimental, t_.experimental};
      row.tag_accent = {false, false};
      row.is_switch = true;
      row.index = o.online ? 0 : 1;
      row.apply = [&o](int index) { o.online = index == 0; };
      push(std::move(row));
    }
    // Playing by address (game::kOnlineByAddress): hosting, or joining a friend's game at its address.
    if (game::kOnlineByAddress) {
      {
        OptionRow row;
        row.id = "online_mode";
        row.title = t_.online_mode;
        row.help = t_.online_mode_help;
        row.values = {t_.online_mode_host, t_.online_mode_join};
        row.subs = {t_.online_mode_host_sub, t_.online_mode_join_sub};
        row.index = o.online_join ? 1 : 0;
        row.apply = [&o](int index) { o.online_join = index == 1; };
        lock_while_off(row);
        push(std::move(row));
      }
      if (o.online_join) {
        // The friend's address, typed in a sheet: "a.b.c.d" or "a.b.c.d:port" (3074 when it names none).
        OptionRow row;
        row.id = "online_address";
        row.title = t_.online_address;
        row.sub = t_.online_address_hint;
        row.help = t_.online_address_help;
        row.values = {o.online_address.empty() ? std::string(t_.online_address_none) : o.online_address};
        row.icon = Icon::kPencil;
        row.activate = [this] {
          OpenTextSheet(t_.nav_online, t_.online_address, t_.online_address_sheet, settings_.o.online_address,
                        "203.0.113.7:3074", 21, [this](const std::string& text) -> std::string {
                          std::string endpoint = OnlineEndpoint(text, kOnlinePort);
                          if (endpoint.empty() && text.find_first_not_of(' ') != std::string::npos) {
                            return t_.online_address_invalid;
                          }
                          settings_.o.online_address = endpoint;
                          Changed();
                          return {};
                        });
        };
        lock_while_off(row);
        push(std::move(row));
      } else {
        // This PC's address on its network, with the port: chosen, it goes to the clipboard. The help lists them all.
        const std::vector<net::LocalAddress>& addresses = LocalAddresses();
        const std::string port = std::to_string(o.online_port);
        OptionRow row;
        row.id = "online_your_address";
        row.title = t_.online_your_address;
        const bool copied = online_copied_at_ && SDL_GetTicks() - online_copied_at_ < 2000;
        row.sub = copied ? t_.online_your_address_copied : t_.online_your_address_hint;
        char help[1024];
        std::snprintf(help, sizeof(help), t_.online_your_address_help, o.online_port);
        row.help = help;
        for (const net::LocalAddress& a : addresses) {
          row.bullets.push_back(a.address + ":" + port + (a.adapter.empty() ? std::string() : " · " + a.adapter));
        }
        if (addresses.empty()) {
          row.values = {t_.online_your_address_none};
        } else {
          std::string address = addresses.front().address + ":" + port;
          row.values = {address};
          row.icon = copied ? Icon::kCheck : Icon::kNone;
          row.activate = [this, address] {
            SDL_SetClipboardText(address.c_str());
            online_copied_at_ = SDL_GetTicks();
          };
        }
        lock_while_off(row);
        push(std::move(row));

        // The router's internet address (UPnP): what friends type over the internet, or why there is none.
        OptionRow internet;
        internet.id = "online_internet";
        internet.title = t_.online_internet;
        std::snprintf(help, sizeof(help), t_.online_internet_help, o.online_port);
        internet.help = help;
        const net::RouterAddress* router = o.online ? RouterAnswer() : nullptr;
        if (!router) {
          internet.values = {t_.online_internet_asking};
          internet.sub = t_.online_internet_asking_hint;
        } else if (router->kind == net::RouterAddress::Kind::kPublic) {
          const bool internet_copied = online_internet_copied_at_ && SDL_GetTicks() - online_internet_copied_at_ < 2000;
          std::string address = router->address + ":" + port;
          internet.values = {address};
          internet.sub = internet_copied ? t_.online_your_address_copied : t_.online_internet_public_hint;
          internet.icon = internet_copied ? Icon::kCheck : Icon::kNone;
          internet.activate = [this, address] {
            SDL_SetClipboardText(address.c_str());
            online_internet_copied_at_ = SDL_GetTicks();
          };
        } else if (router->kind == net::RouterAddress::Kind::kPrivate) {
          internet.values = {t_.online_internet_cgnat};
          internet.tags = {"CGNAT"};
          internet.tag_accent = {false};
          internet.sub = t_.online_internet_cgnat_hint;
        } else {
          internet.values = {t_.online_internet_unknown};
          internet.sub = t_.online_internet_unknown_hint;
        }
        lock_while_off(internet);
        push(std::move(internet));
      }
      // The UDP port, typed in a sheet.
      {
        OptionRow row;
        row.id = "online_port";
        row.title = t_.online_port;
        row.sub = t_.online_port_hint;
        row.help = t_.online_port_help;
        row.values = {std::to_string(o.online_port)};
        row.icon = Icon::kPencil;
        row.activate = [this] {
          OpenTextSheet(t_.nav_online, t_.online_port, t_.online_port_sheet, std::to_string(settings_.o.online_port),
                        std::to_string(kOnlinePort), 5, [this](const std::string& text) -> std::string {
                          int port = OnlinePortOf(text);
                          if (!port) return t_.online_port_invalid;
                          settings_.o.online_port = port;
                          Changed();
                          return {};
                        });
        };
        lock_while_off(row);
        push(std::move(row));
      }
    }
    // Steam (the SDK's rexsteam.exe, with Valve's steam_api64.dll next to it; experimental): locked off in a build
    // without them.
    {
      OptionRow row;
      row.id = "online_steam";
      row.title = t_.online_steam;
      row.sub = t_.online_steam_hint;
      row.help = t_.online_steam_help;
      row.values = {t_.on, t_.off};
      row.tags = {t_.experimental, t_.experimental};
      row.tag_accent = {false, false};
      row.is_switch = true;
      row.index = o.online_steam ? 0 : 1;
      row.apply = [&o](int index) { o.online_steam = index == 0; };
      std::error_code error;
      const bool has_steam = std::filesystem::exists(exe_dir_ / L"rexsteam.exe", error) &&
                             std::filesystem::exists(exe_dir_ / L"steam_api64.dll", error);
      if (o.online && !has_steam) {
        row.locked = true;
        row.locked_value = t_.off;
        row.sub = t_.online_steam_missing;
      }
      if (!game::kOnlineByAddress) {
        // Through Steam only: on with Online play, as it is the only way. Its value written, with the lock (a locked
        // switch is drawn off whatever its value).
        row.help = t_.online_steam_help_auto;
        row.is_switch = false;
        row.index = o.online && has_steam ? 0 : 1;
        row.locked_value = row.values[size_t(row.index)];
        row.locked = true;
        row.sub = !o.online ? t_.online_needs_on : has_steam ? t_.online_steam_auto : t_.online_steam_missing;
        row.activate = nullptr;
        row.icon = Icon::kNone;
      } else {
        lock_while_off(row);
      }
      push(std::move(row));
    }
  }

  // --- The layout (dp from the page's top left; the mockup's 1280 x 760 board, "propuesta A v2"): the title at 56,
  // 12; the categories at 40, 112, 46 apart; the intro at 300, 28; the rows from 300, 68 to 56 from the right edge; the
  // foot 52 high at 576 ---
  ImGuiContext& g = *GImGui;
  const float page_left = rect.Min.x;
  const float page_top = rect.Min.y;
  const float rows_left = page_left + Px(300.0f);
  // The Highway section (launcher 3.30): its rows 430 dp wide, its preview or gallery at their right to 56 dp from the
  // edge.
  const bool highway_panel = category == kCategoryHighway && game::kHighwayPreview;
  const float rows_right = highway_panel ? rows_left + Px(430.0f) : rect.Max.x - Px(56.0f);
  const float foot_top = rect.Max.y - Px(60.0f);

  // The title and the categories: a dot marks the current one; up and down change it at once, right (or A) goes to
  // its rows.
  {
    Type title{Font::kDisplaySemiBold, 44, 0.015f};
    std::string label = Title(mods_tab ? t_.nav_mods : t_.nav_settings);
    // No wider than the column, clear of the intro at 300 (German's EINSTELLUNGEN).
    float width = MeasureText(title, label).x;
    if (width > Px(216.0f)) title.size *= Px(216.0f) / width;
    DrawLabel(title, ImVec2(page_left + Px(56.0f), page_top + Px(12.0f)), col::kText, label, Px(44.0f * 1.1f));
    constexpr Type kCategory{Font::kDisplayMedium, 26};
    constexpr float kCategoryStep = 46.0f;
    // Effects where the game has screen effects to turn off, Online where it plays online.
    const bool has_effects = std::any_of(game::kToggles.begin(), game::kToggles.end(), [](const game::Toggle& toggle) {
      return toggle.group == game::ToggleGroup::kScreenEffects;
    });
    // The Mods tab's sections that have rows (the Tools' Debug keys in every game), Settings' categories.
    std::vector<std::pair<Page, const char*>> categories;
    const int first = mods_tab ? kCategoryHighway : kCategoryDisplay;
    const int last = mods_tab ? kCategoryPacks : kCategoryUpdates;
    for (int c = first; c <= last; ++c) {
      if ((c == kCategoryEffects && !has_effects) || (c == kCategoryOnline && !game::kHasOnline)) continue;
      if (mods_tab && c != kCategoryTools && !ModsSectionHasRows(c - kCategoryHighway)) continue;
      categories.push_back({PageOf(c), category_labels[c]});
    }
    std::vector<std::array<ImGuiID, 2>> category_ids;
    float y = page_top + Px(112.0f);
    for (const auto& [category_page, category_name] : categories) {
      ImRect item(ImVec2(page_left + Px(40.0f), y), ImVec2(page_left + Px(240.0f), y + Px(kCategoryStep)));
      ImGui::PushID(int(category_page));
      ItemState state = Item("##category", item);
      ImGui::PopID();
      ImGuiID id = ImGui::GetItemID();
      bool on = page_ == category_page;
      if (on) category_focus_ = id;
      ImU32 color = on ? col::kText : state.hovered ? col::kTextSoft : col::kLabel;
      DrawLabel(kCategory, ImVec2(item.Min.x + Px(18.0f), item.Min.y), color, category_name, item.GetHeight());
      if (on) {
        // Left of the item, clear of the focus's outline (drawn 4 dp outside the ring's box, which starts 10 dp in).
        float dot = Px(8.0f);
        ImVec2 c(item.Min.x - Px(4.0f), item.GetCenter().y);
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
      y += Px(kCategoryStep);
    }
    ColumnNav(category_ids);
    constexpr Type kNote{Font::kRegular, 15};
    float note_width = Px(236.0f);
    float note_height =
        DrawParagraph(kNote, ImVec2(0, 0), note_width, 1.5f, 0, mods_tab ? t_.mods_note : t_.settings_controls_note, false);
    DrawParagraph(kNote, ImVec2(page_left + Px(56.0f), rect.Max.y - Px(65.0f) - note_height), note_width, 1.5f,
                  col::kTextMuted, mods_tab ? t_.mods_note : t_.settings_controls_note);
  }

  // The intro over the rows (launcher 3.28: the graphics API is a row of Xenos's, no longer written at its right).
  constexpr Type kIntro{Font::kRegular, 17};
  float intro_y = page_top + Px(28.0f);
  DrawLabel(kIntro, ImVec2(rows_left, intro_y), col::kTextMuted, intro);

  // Settings > Updates (launcher 3.30): the state, its card and its rows, page_updates.cpp's.
  if (category == kCategoryUpdates) {
    UpdatesCategory(ImRect(ImVec2(rows_left, page_top + Px(68.0f)), ImVec2(rows_right, foot_top - Px(8.0f))));
    ImRect foot(ImVec2(rows_left, foot_top), ImVec2(rows_right, foot_top + Px(52.0f)));
    FillRect(ImRect(foot.Min, ImVec2(foot.Max.x, foot.Min.y + Snap(Px(1.0f)))), col::kHairline);
    constexpr Type kNote{Font::kRegular, 15};
    DrawIcon(Icon::kCheck, ImVec2(foot.Min.x, foot.GetCenter().y - Px(9.0f)), 18.0f, col::kTextDim, 1.9f);
    DrawLabel(kNote, ImVec2(foot.Min.x + Px(28.0f), foot.GetCenter().y - LineHeight(kNote) * 0.5f), col::kTextMuted,
              t_.autosave);
    return;
  }
  // Mods > Packs (launcher next): the Mods folder and its packs, page_mod_packs.cpp's; at the foot what the packs do
  // and do not do.
  if (category == kCategoryPacks) {
    ModPacksCategory(ImRect(ImVec2(rows_left, page_top + Px(68.0f)), ImVec2(rows_right, foot_top - Px(8.0f))));
    ImRect foot(ImVec2(rows_left, foot_top), ImVec2(rows_right, foot_top + Px(52.0f)));
    FillRect(ImRect(foot.Min, ImVec2(foot.Max.x, foot.Min.y + Snap(Px(1.0f)))), col::kHairline);
    constexpr Type kNote{Font::kRegular, 14};
    const float note_x = foot.Min.x + Px(28.0f);
    const float note_width = foot.Max.x - note_x;
    const float note_y =
        foot.GetCenter().y - DrawParagraph(kNote, ImVec2(0, 0), note_width, 1.4f, 0, t_.mod_packs_note, false) * 0.5f;
    // The icon beside its first line.
    DrawIcon(Icon::kInfo, ImVec2(foot.Min.x, note_y + (Px(14.0f) * 1.4f - Px(18.0f)) * 0.5f), 18.0f, col::kTextDim,
             1.9f);
    DrawParagraph(kNote, ImVec2(note_x, note_y), note_width, 1.4f, col::kTextMuted, t_.mod_packs_note);
    return;
  }
  // The Highway section's preview or gallery (launcher 3.30), at the rows' right.
  if (highway_panel) {
    HighwayPanel(ImRect(ImVec2(rows_right + Px(40.0f), page_top + Px(68.0f)), ImVec2(rect.Max.x - Px(56.0f), foot_top - Px(8.0f))));
  }

  // The rows, scrolling when they do not fit; the scroll bar 16 dp right of them.
  int& selected = settings_row_[category];
  selected = std::clamp(selected, 0, std::max(0, int(rows.size()) - 1));
  settings_row_id_ = rows.empty() ? std::string() : rows[size_t(selected)].id;
  ImRect list(ImVec2(rows_left, page_top + Px(68.0f)), ImVec2(rows_right + Px(16.0f), foot_top - Px(8.0f)));
  BeginScroll("##settings_rows", list);
  ImVec2 origin = ImGui::GetCursorScreenPos();
  float y = origin.y;
  const float row_left = origin.x;
  const float row_right = origin.x + (rows_right - rows_left);
  // The row the controller's focus is on, or is going to (ColumnNav's RequestFocus, given when that row is drawn), is
  // the big one from the start of this frame: chosen while drawing, a row before it was already drawn small and the
  // one after it too, a frame with no row outlined (a jump, moving up).
  // Launcher 3.22: with the mouse, the row under it once it rests there kHoverDelay (found in the rows' rectangles of
  // the last frame: this frame's layout depends on the choice); it stays the big one when the mouse leaves the rows.
  // A move or an activation of the controller or the keyboard, or a focus asked for, gives the choice back to the focus.
  const int frame = ImGui::GetFrameCount();
  const bool fresh = settings_drawn_frame_ != frame - 1 || settings_rects_category_ != category;
  settings_drawn_frame_ = frame;
  if (fresh) {
    settings_row_rects_.clear();
    settings_hover_candidate_ = -1;
  }
  {
    const ImGuiIO& io = ImGui::GetIO();
    if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) settings_mouse_ = true;
    if (g.NavMoveSubmitted || g.NavActivateId != 0 || PendingFocus()) settings_mouse_ = false;
  }
  if (settings_mouse_) {
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    int under = -1;
    if (ImGui::IsWindowHovered() && list.Contains(mouse)) {
      for (int i = 0; i < int(settings_row_rects_.size()) && i < int(rows.size()); ++i) {
        if (settings_row_rects_[size_t(i)].Contains(mouse)) {
          under = i;
          break;
        }
      }
    }
    if (under < 0 || under == selected) {
      settings_hover_candidate_ = -1;
    } else if (under != settings_hover_candidate_) {
      settings_hover_candidate_ = under;
      settings_hover_since_ = ImGui::GetTime();
    } else if (ImGui::GetTime() - settings_hover_since_ >= kHoverDelay) {
      selected = under;
      settings_hover_candidate_ = -1;
    }
  } else {
    settings_hover_candidate_ = -1;
    const ImGuiID target = PendingFocus() ? PendingFocus()
                           : g.NavWindow == ImGui::GetCurrentWindow() ? g.NavId
                                                                      : 0;
    for (int i = 0; target && i < int(rows.size()); ++i) {
      ImGui::PushID(rows[size_t(i)].id.c_str());
      const ImGuiID id = ImGui::GetID("##row");
      ImGui::PopID();
      if (id == target) {
        selected = i;
        break;
      }
    }
  }
  const bool motion = MotionEnabled();
  const float dt = ImGui::GetIO().DeltaTime;
  bool moving = settings_hover_candidate_ >= 0;
  settings_row_rects_.assign(rows.size(), ImRect());
  settings_rects_category_ = category;
  std::vector<std::array<ImGuiID, 2>> row_ids;
  for (int i = 0; i < int(rows.size()); ++i) {
    OptionRow& row = rows[size_t(i)];
    // A group's title (launcher 3.28): Oswald, a line under it from the text to the scroll bar's side.
    if (!row.group.empty()) {
      y += Px(i == 0 ? kGroupFirstGap : kGroupGap);
      DrawLabel(kGroupTitle, ImVec2(row_left + Px(kRowPad), y), col::kTextMuted, row.group);
      const float line_y = Snap(y + Px(kGroupLine));
      FillRect(ImRect(ImVec2(row_left + Px(kRowPad), line_y), ImVec2(row_right - Px(kRowPad), line_y + Snap(Px(1.0f)))),
               Fade(col::kFocus, 0.14f));
      y = line_y + Snap(Px(1.0f)) + Px(kGroupBelowLine) - Px(1.0f);
    }
    const float slot_top = y;
    // The current row grows to show its line of help and its arrows: the one in focus, the mouse's, or the last one
    // chosen. Launcher 3.22: it opens (and the one before closes) over kOpenSeconds, eased in and out: the two rows
    // change by the same height at every moment, so the rows under the mouse stay where they are.
    const bool big = selected == i;
    ImGui::PushID(row.id.c_str());
    float& open = settings_row_open_.try_emplace(ImGui::GetID("##row"), big ? 1.0f : 0.0f).first->second;
    if (fresh || !motion) {
      open = big ? 1.0f : 0.0f;
    } else if (big) {
      open = std::min(1.0f, open + dt / kOpenSeconds);
    } else {
      open = std::max(0.0f, open - dt / kOpenSeconds);
    }
    if (open > 0.0f && open < 1.0f) moving = true;
    const float e = EaseInOut(open);
    y += Px(kFocusedRowMargin) * e;
    // An indented row (3.28): its focus from kChildLeft, its text at kChildText; the others' text kRowPad in.
    const float r_left = row.child ? row_left + Px(kChildLeft) : row_left;
    ImRect r(ImVec2(r_left, y), ImVec2(row_right, y + Px(kRowHeight + (kFocusedRowHeight - kRowHeight) * e)));
    const float text_left = row.child ? row_left + Px(kChildText) : r.Min.x + Px(kRowPad);
    settings_row_rects_[size_t(i)] = r;
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
    // (The big row follows the focus from the start of the frame, above: changed here, it fought a focus given in the
    // middle of the frame.)
    const bool nav_here = g.NavId == row_id && g.NavWindow == ImGui::GetCurrentWindow();
    int count = int(row.values.size());
    const bool changeable = !row.locked && count > 1;
    auto step = [&](int delta) {
      if (!changeable) return;
      row.index = (row.index + delta + count) % count;
      row.apply(row.index);
      selected = i;
      changed = true;
    };
    // The controller (launcher 3.19.1, as before 3.18): A on a row of values starts changing it, left and right then
    // step it until A or B; a switch flips with A at once. Otherwise left goes back to the categories. A click steps
    // the value, as its arrows do.
    const bool editing = RowEditing(row_id);
    if (int delta = EditingStep(row_id)) {
      step(delta);
    } else if (nav_here && !editing && g.NavMoveSubmitted &&
               (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right)) {
      if (g.NavMoveDir == ImGuiDir_Left) RequestFocus(category_focus_);
      ImGui::NavMoveRequestCancel();
    }
    // The mouse (launcher 3.22): a switch flips on a click on it, a value steps with its arrows (their own items, over
    // the row); a click elsewhere on the row only opens it. A row that opens something (a list, a sheet) opens it
    // from anywhere, having nothing else to click.
    const float pad = Px(20.0f);
    const ImVec2 toggle_pos(r.Max.x - pad - Px(52.0f), r.GetCenter().y - Px(15.0f));
    const ImRect toggle_hit(ImVec2(toggle_pos.x - Px(8.0f), toggle_pos.y - Px(8.0f)),
                            ImVec2(toggle_pos.x + Px(60.0f), toggle_pos.y + Px(38.0f)));
    if (state.pressed) {
      if (row.activate) {
        row.activate();
      } else if (PressedByNav()) {
        if (!row.is_switch) {
          if (changeable) SetRowEditing(editing ? 0 : row_id);
        } else {
          step(1);
        }
      } else if (row.is_switch && toggle_hit.Contains(ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left])) {
        step(1);
      }
      selected = i;
    }

    // The row: a hairline under it from its text, or the focus's fill and outline when it is the big one (both through
    // its opening).
    if (big && state.focused) {
      FocusRow(r, kRowRounding);
    } else if (e > 0.0f) {
      FillRect(r, Fade(col::kFocus, 0.06f * e), kRowRounding);
    }
    if (e < 1.0f) {
      if (state.hovered && !big) FillRect(r, Fade(col::kFocus, 0.03f * (1.0f - e)), 8.0f);
      FillRect(ImRect(ImVec2(text_left, r.Max.y - Snap(Px(1.0f))), r.Max), Fade(col::kHairline, 1.0f - e));
    }

    // The value at the right: a switch, or its text (with a lock when it cannot change) and its chip; in the big row
    // between the arrows, its place among the values under it.
    float value_left = r.Max.x - pad;
    std::string tag = ChipText(row.Tag());
    bool tag_warning = !tag.empty() && !(row.tag_accent.empty() ||
                                         row.tag_accent[size_t(std::clamp(row.index, 0, int(row.tag_accent.size()) - 1))]);
    if (row.is_switch) {
      bool on = row.locked ? row.locked_value == std::string(t_.on) : row.index == 0;
      value_left = r.Max.x - pad - Px(52.0f);
      DrawToggle(toggle_pos, on, !row.locked, false, state.hovered && toggle_hit.Contains(ImGui::GetIO().MousePos));
      if (row.locked) {
        DrawIcon(Icon::kLock, ImVec2(value_left - Px(10.0f) - Px(18.0f), r.GetCenter().y - Px(9.0f)), 18.0f,
                 col::kTextDim, 1.8f);
        value_left -= Px(28.0f);
      }
      if (!tag.empty()) {
        value_left -= Px(12.0f) + ChipWidth(tag);
        DrawChip(ImVec2(value_left, r.GetCenter().y), tag, tag_warning);
      }
    } else {
      // Launcher 3.22: a changeable value's two looks (the big row's between its arrows, the small row's at the
      // right) crossfaded as the row opens or closes; a value that cannot change keeps one look.
      auto arrows_value = [&]() -> float {
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
        if (ArrowButton("##prev", ImVec2(left_arrow, r.GetCenter().y - Px(14.0f)), true, editing)) step(-1);
        if (ArrowButton("##next", ImVec2(right_arrow, r.GetCenter().y - Px(14.0f)), false, editing)) step(1);
        return left_arrow;
      };
      auto plain_value = [&](bool large) -> float {
        const Type& type = large ? kRowValueFocused : kRowValue;
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
        } else if (row.icon != Icon::kNone) {
          x -= Px(10.0f) + Px(18.0f);
          DrawIcon(row.icon, ImVec2(x, r.GetCenter().y - Px(9.0f)), 18.0f,
                   row.icon == Icon::kCheck ? col::kAccent : state.hovered || large ? col::kTextSoft : col::kTextMuted,
                   1.8f);
        }
        return x;
      };
      ImDrawList* draw = ImGui::GetWindowDrawList();
      if (changeable && e > 0.0f) {
        float small_left = r.Max.x;
        if (e < 1.0f) {
          const int start = draw->VtxBuffer.Size;
          small_left = plain_value(false);
          FadeVertices(draw, start, 1.0f - e);
        }
        const int start = draw->VtxBuffer.Size;
        const float big_left = arrows_value();
        FadeVertices(draw, start, e);
        value_left = std::min(small_left, big_left);
      } else {
        value_left = plain_value(e >= 0.5f);
      }
    }

    // The label at the left; in the big row its line of help under it. Both cut to what the value leaves. Launcher
    // 3.22: the label moves up as the row opens, and its line of help fades in under it, inside the row.
    float text_width = std::max(Px(60.0f), value_left - Px(24.0f) - text_left);
    const float small_y = r.GetCenter().y - LineHeight(kRowLabel) * 0.5f;
    const float big_y =
        r.GetCenter().y - (LineHeight(kRowLabelFocused) + Px(3.0f) + LineHeight(kRowDescription)) * 0.5f;
    const float label_y = small_y + (big_y - small_y) * e;
    const bool strong = e >= 0.5f;
    const Type& label_type = strong ? kRowLabelFocused : kRowLabel;
    DrawLabel(label_type, ImVec2(text_left, label_y),
              row.locked ? (strong ? col::kTextMuted : col::kTextDim) : (strong ? col::kText : col::kTextSoft),
              Ellipsize(label_type, row.title, text_width));
    if (e > 0.0f) {
      ImGui::PushClipRect(r.Min, r.Max, true);
      DrawLabel(kRowDescription, ImVec2(text_left, label_y + LineHeight(kRowLabelFocused) + Px(3.0f)),
                Fade(col::kTextMuted, e), Ellipsize(kRowDescription, row.Sub(), text_width));
      ImGui::PopClipRect();
    }
    ImGui::PopID();
    y = r.Max.y + Px(kFocusedRowMargin) * e;
    // An indented row's line at its left, through its whole height: the rows under one row draw one line.
    if (row.child) {
      const float line_x = Snap(row_left + Px(kChildLine));
      FillRect(ImRect(ImVec2(line_x, slot_top), ImVec2(line_x + Snap(Px(1.0f)), y)), Fade(col::kFocus, 0.16f));
    }
    // A notice under the row (3.28: what Xenos lacks), in a dashed box; not a row of its own (no focus, no help).
    if (!row.notice.empty()) {
      y += Px(kNoticeAbove);
      const ImRect box(ImVec2(row_left, y), ImVec2(row_right, y + Px(kNoticeHeight)));
      DashedRect(box, Fade(col::kFocus, 0.16f), 12.0f);
      const float icon_x = box.Min.x + Px(kRowPad);
      DrawIcon(Icon::kInfo, ImVec2(icon_x, box.GetCenter().y - Px(9.0f)), 18.0f, col::kTextMuted, 1.75f);
      const float notice_x = icon_x + Px(18.0f) + Px(10.0f);
      DrawLabel(kNoticeText, ImVec2(notice_x, box.GetCenter().y - LineHeight(kNoticeText) * 0.5f), col::kTextSoft,
                Ellipsize(kNoticeText, row.notice, box.Max.x - Px(kRowPad) - notice_x));
      y = box.Max.y + Px(kNoticeBelow);
    }
  }
  if (moving) {
    settings_moving_until_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
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
    // "Reset display": the category's name in lower case inside the sentence, but in German, whose nouns keep their
    // capital (the five languages' names start with an ASCII letter).
    std::string category_word = category_label;
    if (language_ != 3 && !category_word.empty() && category_word[0] >= 'A' && category_word[0] <= 'Z') {
      category_word[0] = char(category_word[0] - 'A' + 'a');
    }
    char reset[96];
    std::snprintf(reset, sizeof(reset), t_.reset_category, category_word.c_str());
    ButtonSpec spec{ButtonKind::kGhost, Type{Font::kSemiBold, 14}, 34, 16, 10};
    float button_width = ButtonWidth(spec, reset);
    constexpr Type kNote{Font::kRegular, 15};
    DrawIcon(Icon::kCheck, ImVec2(foot.Min.x, foot.GetCenter().y - Px(9.0f)), 18.0f, col::kTextDim, 1.9f);
    float note_x = foot.Min.x + Px(18.0f) + Px(10.0f);
    DrawLabel(kNote, ImVec2(note_x, foot.GetCenter().y - LineHeight(kNote) * 0.5f), col::kTextMuted,
              Ellipsize(kNote, t_.autosave, foot.Max.x - button_width - Px(24.0f) - note_x));
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    bool reset_pressed = Button("##reset", ImVec2(foot.Max.x - button_width, foot.GetCenter().y - Px(17.0f)), spec, reset);
    ImGui::PopItemFlag();
    if (reset_pressed || (!popup && ButtonPressed(PadButton::kX))) {
      switch (category) {
        case kCategoryDisplay:
          ResetDisplay(o);
          settings_.MonitorChanged();
          break;
        case kCategoryGraphics:
          ResetGraphics(o);
          o.native = o.native && settings_.has_vulkan && game::kHasNativeXRenderer;
          break;
        case kCategoryEffects:
          ResetEffects(o);
          break;
        case kCategoryAudio:
          ResetAudio(o);
          break;
        case kCategoryGame:
          ResetGame(o);
          break;
        case kCategoryHighway:
          ResetModsHighway(o);
          break;
        case kCategoryGameplay:
          ResetModsGameplay(o);
          break;
        case kCategoryStage:
          ResetModsStage(o);
          break;
        case kCategorySound:
          ResetModsSound(o);
          break;
        case kCategoryTools:
          ResetModsTools(o);
          break;
        default:
          ResetOnline(o);
          break;
      }
      changed = true;
    }
  }

  // Y: the current row's whole help.
  if (!popup && !rows.empty() && ButtonPressed(PadButton::kY)) {
    const OptionRow& row = rows[size_t(selected)];
    OpenInfo(t_.more_info, row.title, row.help.empty() ? row.Sub() : row.help, row.bullets);
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
