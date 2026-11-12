// Singing (game::kHasMicrophones), in the Controls tab: the microphone among its devices, whose rows choose the PC's
// recording device the game hears as its USB microphone and the channel of it, with a level meter; and each
// controller's part, the game's instrument or the vocals. The SDK's usb_microphone_1 and usb_microphone_1_channel
// (recomp change 55) and gamepad_device_subtypes (change 56). The game takes a second microphone too
// (usb_microphone_2), which only the options file sets: Guitar Hero: Metallica's band has one singer.

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_guid.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "app.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

// The part a controller sings with: XINPUT_DEVSUBTYPE_GAMEPAD, which the game gives the vocals. A controller that is
// not in gamepad_device_subtypes plays the game's instrument (its gamepad_subtype, src/startup.cpp).
constexpr int kSingerSubtype = 1;
// The keyboard's emulated controller in gamepad_device_subtypes.
constexpr const char* kKeyboardKey = "keyboard";

std::string_view Trim(std::string_view s) {
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
  return s;
}

bool EqualsCI(std::string_view a, std::string_view b) {
  return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
           return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
         });
}

// gamepad_device_subtypes' entries ("key=subtype", comma-separated), as the SDK reads them.
template <typename F>
void ForEachEntry(std::string_view list, F f) {
  while (!list.empty()) {
    size_t comma = list.find(',');
    std::string_view entry = list.substr(0, comma);
    list = comma == std::string_view::npos ? std::string_view() : list.substr(comma + 1);
    size_t equals = entry.find('=');
    if (equals != std::string_view::npos) {
      f(Trim(entry.substr(0, equals)), Trim(entry.substr(equals + 1)));
    }
  }
}

int SubtypeOf(std::string_view list, std::string_view key) {
  int subtype = -1;
  ForEachEntry(list, [&](std::string_view k, std::string_view v) {
    if (EqualsCI(k, key)) subtype = std::atoi(std::string(v).c_str());
  });
  return subtype;
}

// The list with `key` given `subtype`, or without it (-1).
std::string WithSubtype(std::string_view list, std::string_view key, int subtype) {
  std::string out;
  ForEachEntry(list, [&](std::string_view k, std::string_view v) {
    if (EqualsCI(k, key)) return;
    if (!out.empty()) out += ',';
    out.append(k).append("=").append(v);
  });
  if (subtype >= 0) {
    if (!out.empty()) out += ',';
    out.append(key).append("=").append(std::to_string(subtype));
  }
  return out;
}

// The recording devices SDL sees, with their channels.
struct Recorder {
  std::string name;
  int channels = 1;
};

int ChannelsOf(SDL_AudioDeviceID id) {
  SDL_AudioSpec spec{};
  int frames = 0;
  return SDL_GetAudioDeviceFormat(id, &spec, &frames) ? std::clamp(spec.channels, 1, 8) : 1;
}

std::vector<Recorder> Recorders() {
  std::vector<Recorder> list;
  int count = 0;
  SDL_AudioDeviceID* ids = SDL_GetAudioRecordingDevices(&count);
  for (int i = 0; ids && i < count; ++i) {
    const char* name = SDL_GetAudioDeviceName(ids[i]);
    if (name && *name) list.push_back({name, ChannelsOf(ids[i])});
  }
  SDL_free(ids);
  return list;
}

// The device a usb_microphone_1 value names: its id (the system's default for kSystemMicrophone), 0 if none is.
SDL_AudioDeviceID FindRecorder(const std::string& device) {
  int count = 0;
  SDL_AudioDeviceID* ids = SDL_GetAudioRecordingDevices(&count);
  SDL_AudioDeviceID found = 0;
  if (device == kSystemMicrophone) {
    found = count > 0 ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING : 0;
  } else {
    for (int i = 0; ids && i < count && !found; ++i) {
      const char* name = SDL_GetAudioDeviceName(ids[i]);
      if (name && device == name) found = ids[i];
    }
  }
  SDL_free(ids);
  return found;
}

bool EnsureAudio() {
  return SDL_WasInit(SDL_INIT_AUDIO) || SDL_InitSubSystem(SDL_INIT_AUDIO);
}

}  // namespace

const std::string& App::PadKey(SDL_JoystickID id) {
  auto it = pad_keys_.find(id);
  if (it != pad_keys_.end()) {
    return it->second;
  }
  // Two controllers of one model share a GUID; a serial number (SDL's HIDAPI controllers have one) tells them apart.
  std::string key;
  if (SDL_Gamepad* gamepad = SDL_OpenGamepad(id)) {
    const char* serial = SDL_GetGamepadSerial(gamepad);
    if (serial && *serial) key = serial;
    SDL_CloseGamepad(gamepad);
  }
  if (key.empty()) {
    char guid[33] = {};
    SDL_GUIDToString(SDL_GetGamepadGUIDForID(id), guid, int(sizeof(guid)));
    key = guid;
  }
  return pad_keys_[id] = key;
}

bool App::Sings(const std::string& key) const {
  return SubtypeOf(settings_.o.device_subtypes, key) == kSingerSubtype;
}

std::string App::SingersText() {
  std::string names;
  int count = 0;
  SDL_JoystickID* ids = SDL_GetGamepads(&count);
  for (int i = 0; ids && i < count; ++i) {
    SDL_JoystickType kind = SDL_GetJoystickTypeForID(ids[i]);
    if (kind == SDL_JOYSTICK_TYPE_GUITAR || kind == SDL_JOYSTICK_TYPE_DRUM_KIT || !Sings(PadKey(ids[i]))) continue;
    const char* name = SDL_GetGamepadNameForID(ids[i]);
    if (!names.empty()) names += ", ";
    names += name && *name ? name : t_.controller;
  }
  SDL_free(ids);
  if (settings_.o.mnk && Sings(kKeyboardKey)) {
    if (!names.empty()) names += ", ";
    names += t_.device_keyboard;
  }
  if (names.empty()) {
    return t_.microphone_no_singer;
  }
  char text[512];
  std::snprintf(text, sizeof(text), t_.microphone_singers, names.c_str());
  return text;
}

std::string App::MicStatus(ImU32* dot) {
  const std::string& device = settings_.o.microphones[0];
  if (device.empty() || !EnsureAudio()) {
    *dot = col::kTextDim;
    return t_.microphone_off;
  }
  bool connected = FindRecorder(device) != 0;
  *dot = connected ? col::kSuccess : col::kError;
  std::string name = device == kSystemMicrophone ? std::string(t_.microphone_system) : device;
  if (connected) {
    return name;
  }
  char text[512];
  std::snprintf(text, sizeof(text), t_.microphone_missing, name.c_str());
  return text;
}

float App::PartRow(ImVec2 origin, float width, const std::string& key) {
  // A selector (launcher 3.18): the row is its item; A, a click, left or right switch the part. Its value a chip with a
  // chevron.
  ImGuiContext& g = *GImGui;
  const game::GameTexts& own = game::GameTextsFor(language_);
  ImRect r(origin, ImVec2(origin.x + width, origin.y + Px(kMappingRow)));
  ImRect chip = MappingChipRect(r);
  ImGui::PushID("part");
  ItemState state = Item("##row", r);
  ImGuiID id = ImGui::GetItemID();
  ImGui::PopID();
  if (!mapping_first_) mapping_first_ = id;
  mapping_rows_.push_back({id, id});
  bool vocals = Sings(key);
  bool step = state.pressed;
  if (g.NavId == id && g.NavMoveSubmitted && (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right)) {
    step = true;
    ImGui::NavMoveRequestCancel();
  }
  MappingRow(r, 0, t_.part, chip.Min.x - Px(16.0f), false, state.focused);
  if (state.hovered && !state.focused) FillRect(r, Fade(col::kFocus, 0.03f), 12.0f);
  ChipLook look;
  look.text = vocals ? (own.part_vocals ? own.part_vocals : t_.part_vocals)
                     : (own.part_instrument ? own.part_instrument : t_.part_instrument);
  look.icon = Icon::kChevronDown;
  look.icon_color = col::kTextSoft;
  // Drawn only: the row is the item.
  FillRect(chip, state.hovered ? Brighten(col::kField) : col::kField, 8.0f);
  StrokeRect(chip, col::kFieldBorder, 8.0f);
  constexpr Type kChip{Font::kSemiBold, 15};
  std::string shown = Ellipsize(kChip, look.text, chip.GetWidth() - Px(28.0f) - Px(22.0f));
  float text_width = MeasureText(kChip, shown).x;
  float x = chip.GetCenter().x - (text_width + Px(6.0f) + Px(16.0f)) * 0.5f;
  DrawLabel(kChip, ImVec2(x, chip.Min.y), col::kText, shown, chip.GetHeight());
  DrawIcon(Icon::kChevronDown, ImVec2(x + text_width + Px(6.0f), chip.GetCenter().y - Px(8.0f)), 16.0f, col::kTextSoft,
           2.0f);
  if (step) {
    StopListening();
    settings_.o.device_subtypes = WithSubtype(settings_.o.device_subtypes, key, vocals ? -1 : kSingerSubtype);
    Changed();
  }
  return Px(kMappingRow);
}

void App::UpdateMicMeter() {
  mic_meter_frame_ = ImGui::GetFrameCount();
  const std::string& device = settings_.o.microphones[0];
  auto now = std::chrono::steady_clock::now();
  if (mic_meter_.stream && mic_meter_.device != device) {
    CloseMicMeter();
  }
  if (!mic_meter_.stream && !device.empty() && now >= mic_meter_.retry_at && EnsureAudio()) {
    if (SDL_AudioDeviceID id = FindRecorder(device)) {
      mic_meter_.channels = ChannelsOf(id);
      SDL_AudioSpec spec{};
      spec.format = SDL_AUDIO_S16;
      spec.channels = mic_meter_.channels;
      spec.freq = 48000;
      mic_meter_.stream = SDL_OpenAudioDeviceStream(id, &spec, nullptr, nullptr);
      if (mic_meter_.stream) {
        SDL_ResumeAudioStreamDevice(mic_meter_.stream);
        mic_meter_.device = device;
        mic_meter_.read_at = now;
      }
    }
    if (!mic_meter_.stream) mic_meter_.retry_at = now + std::chrono::seconds(1);
  }
  // The level falls by half every quarter of a second, and the loudest sample read since takes it up.
  float seconds = std::chrono::duration<float>(now - mic_meter_.read_at).count();
  mic_meter_.read_at = now;
  mic_meter_.peak *= std::pow(0.5f, seconds / 0.25f);
  if (!mic_meter_.stream) {
    return;
  }
  // What the game hears: the chosen channel, or the channels' mean, at the gain.
  float gain = std::pow(10.0f, float(settings_.o.microphone_gains[0]) / 20.0f);
  int channels = mic_meter_.channels;
  int only = std::min(settings_.o.microphone_channels[0], channels);
  int16_t samples[4096];
  int frame_bytes = 2 * channels;
  for (;;) {
    int got = SDL_GetAudioStreamData(mic_meter_.stream, samples, int(sizeof(samples)) / frame_bytes * frame_bytes);
    if (got <= 0) break;
    for (int f = 0; f < got / frame_bytes; ++f) {
      const int16_t* frame = samples + size_t(f) * size_t(channels);
      int sample = 0;
      if (only > 0) {
        sample = frame[only - 1];
      } else {
        for (int c = 0; c < channels; ++c) sample += frame[c];
        sample /= channels;
      }
      mic_meter_.peak = std::max(mic_meter_.peak, std::min(std::abs(sample) * gain / 32768.0f, 1.0f));
    }
  }
}

void App::CloseMicMeter() {
  if (mic_meter_.stream) {
    SDL_DestroyAudioStream(mic_meter_.stream);
  }
  mic_meter_ = {};
}

namespace {

// A row whose value steps (launcher 3.18, the mockup's board 10, as Settings' rows): 56 dp, the row its item; left and
// right (or the arrows of the focused row) step the value, A or a click the next one. Returns -1 or 1 for a step, 2
// for X (clear), 0 for none.
int ValueRow(const char* id, const ImRect& r, std::string_view label, std::string_view value, bool enabled) {
  ImGuiContext& g = *GImGui;
  ImGui::PushID(id);
  ImGui::SetNextItemAllowOverlap();
  ItemState state = Item("##row", r, enabled);
  ImGuiID item = ImGui::GetItemID();
  int step = 0;
  if (enabled && g.NavId == item && g.NavMoveSubmitted &&
      (g.NavMoveDir == ImGuiDir_Left || g.NavMoveDir == ImGuiDir_Right)) {
    step = g.NavMoveDir == ImGuiDir_Left ? -1 : 1;
    ImGui::NavMoveRequestCancel();
  }
  if (state.pressed) step = 1;
  if (enabled && ClearPressed(state)) step = 2;
  if (state.focused) {
    FocusRow(r, 12.0f);
  } else {
    if (state.hovered) FillRect(r, Fade(col::kFocus, 0.03f), 12.0f);
    FillRect(ImRect(ImVec2(r.Min.x, r.Max.y - Snap(Px(1.0f))), r.Max), col::kHairline);
  }
  const float pad = Px(20.0f);
  const Type value_type{Font::kSemiBold, state.focused ? 20.0f : 18.0f};
  float value_left;
  if (state.focused && enabled) {
    // The value centred between its arrows, in a block 150 dp wide at least.
    float max_text = (r.GetWidth() - pad * 2.0f) * 0.5f;
    std::string shown = Ellipsize(value_type, value, max_text);
    float text_width = MeasureText(value_type, shown).x;
    float block = std::max(Px(150.0f), text_width);
    float right_arrow = r.Max.x - pad - Px(28.0f);
    float block_right = right_arrow - Px(4.0f);
    float block_left = block_right - block;
    float left_arrow = block_left - Px(4.0f) - Px(28.0f);
    DrawLabel(value_type, ImVec2((block_left + block_right - text_width) * 0.5f, r.GetCenter().y - LineHeight(value_type) * 0.5f),
              col::kText, shown);
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    for (int side = 0; side < 2; ++side) {
      ImRect arrow(ImVec2(side == 0 ? left_arrow : right_arrow, r.GetCenter().y - Px(14.0f)),
                   ImVec2((side == 0 ? left_arrow : right_arrow) + Px(28.0f), r.GetCenter().y + Px(14.0f)));
      ItemState a = Item(side == 0 ? "##prev" : "##next", arrow);
      if (a.hovered) FillRect(arrow, Fade(col::kFocus, 0.08f), 14.0f);
      DrawIcon(side == 0 ? Icon::kChevronLeft : Icon::kChevronRight, ImVec2(arrow.Min.x + Px(2.0f), arrow.Min.y + Px(2.0f)),
               24.0f, a.hovered ? col::kText : col::kTextSoft, 1.9f);
      if (a.pressed) step = side == 0 ? -1 : 1;
    }
    ImGui::PopItemFlag();
    value_left = left_arrow;
  } else {
    std::string shown = Ellipsize(value_type, value, (r.GetWidth() - pad * 2.0f) * 0.55f);
    float text_width = MeasureText(value_type, shown).x;
    value_left = r.Max.x - pad - text_width;
    DrawLabel(value_type, ImVec2(value_left, r.GetCenter().y - LineHeight(value_type) * 0.5f),
              enabled ? col::kText : col::kTextDim, shown);
  }
  const Type label_type{state.focused ? Font::kSemiBold : Font::kMedium, 18};
  DrawLabel(label_type, ImVec2(r.Min.x + pad, r.GetCenter().y - LineHeight(label_type) * 0.5f),
            !enabled ? col::kTextDim : state.focused ? col::kText : col::kTextSoft,
            Ellipsize(label_type, label, value_left - Px(24.0f) - r.Min.x - pad));
  ImGui::PopID();
  return step;
}

}  // namespace

float App::MicRows(ImVec2 origin, float width) {
  Options& o = settings_.o;
  if (!EnsureAudio()) {
    return 0.0f;
  }
  UpdateMicMeter();
  std::vector<Recorder> recorders = Recorders();
  // Rows 56 dp high: there are few (the mockup's board 10).
  constexpr float kRow = 56.0f;
  float y = origin.y;
  auto row_rect = [&]() { return ImRect(ImVec2(origin.x, y), ImVec2(origin.x + width, y + Px(kRow))); };
  auto navigable = [&]() {
    if (!mapping_first_) mapping_first_ = ImGui::GetItemID();
    mapping_rows_.push_back({ImGui::GetItemID(), ImGui::GetItemID()});
  };

  // The device: none, the system's default, each device the system has; X back to none. A device chosen before and
  // not connected now stays chosen, marked.
  std::string& device = o.microphones[0];
  std::vector<std::string> values = {"", kSystemMicrophone};
  for (const Recorder& recorder : recorders) values.push_back(recorder.name);
  auto current = std::find(values.begin(), values.end(), device);
  bool missing = current == values.end();
  int channels = 0;  // the chosen device's, 0 when there is none
  {
    std::string name;
    if (device.empty()) {
      name = t_.microphone_none;
    } else {
      name = device == kSystemMicrophone ? std::string(t_.microphone_system) : device;
      if (missing) {
        char text[512];
        std::snprintf(text, sizeof(text), t_.microphone_missing, name.c_str());
        name = text;
      }
    }
    int step = ValueRow("mic_device", row_rect(), t_.microphone_device, name, true);
    navigable();
    if (step == 2) {
      if (!device.empty()) {
        device.clear();
        o.microphone_channels[0] = 0;
        Changed();
      }
    } else if (step != 0) {
      int count = int(values.size());
      int index = missing ? 0 : int(current - values.begin());
      device = values[size_t((index + step + count) % count)];
      o.microphone_channels[0] = 0;
      Changed();
    }
    y += Px(kRow);
    current = std::find(values.begin(), values.end(), device);
    missing = current == values.end();
    if (!device.empty() && !missing) {
      channels = device == kSystemMicrophone ? ChannelsOf(SDL_AUDIO_DEVICE_DEFAULT_RECORDING)
                                             : recorders[size_t(current - values.begin()) - 2].channels;
    }
  }

  // The channel: all of them mixed, or one (an interface's input); a mono device has only its own.
  {
    int& channel = o.microphone_channels[0];
    char text[64];
    const char* value = text;
    if (channels <= 1) {
      value = channels == 1 ? t_.microphone_mono : "\xE2\x80\x94";
    } else if (channel == 0 || channel > channels) {
      value = t_.microphone_all_channels;
    } else {
      std::snprintf(text, sizeof(text), t_.microphone_channel_n, channel);
    }
    bool enabled = channels > 1;
    int step = ValueRow("mic_channel", row_rect(), t_.microphone_channel, value, enabled);
    if (enabled) {
      navigable();
      if (step == 2) {
        if (channel != 0) {
          channel = 0;
          Changed();
        }
      } else if (step != 0) {
        channel = (std::min(channel, channels) + step + channels + 1) % (channels + 1);
        Changed();
      }
    }
    y += Px(kRow);
  }

  // The gain: steps of 6 dB; X back to the game's.
  {
    constexpr int kGains[] = {-24, -18, -12, -6, 0, 6};
    int& gain = o.microphone_gains[0];
    char text[32];
    std::snprintf(text, sizeof(text), gain > 0 ? "+%d dB" : gain < 0 ? "\xE2\x88\x92%d dB" : "%d dB", std::abs(gain));
    bool enabled = !device.empty();
    int step = ValueRow("mic_gain", row_rect(), t_.microphone_gain, text, enabled);
    if (enabled) {
      navigable();
      if (step == 2) {
        if (gain != game::kMicrophoneGainDefault) {
          gain = game::kMicrophoneGainDefault;
          Changed();
        }
      } else if (step != 0) {
        int count = int(std::size(kGains));
        int index = int(std::find(std::begin(kGains), std::end(kGains), gain) - std::begin(kGains));
        if (index >= count) index = 2;
        gain = kGains[(index + step + count) % count];
        Changed();
      }
    }
    y += Px(kRow);
  }

  // The level: what the game hears, from -60 dB to full scale, over its three zones (green, yellow from -18 dB, red
  // from -6 dB) drawn faint, so that it shows where the bar should reach.
  {
    ImRect r = row_rect();
    FillRect(ImRect(ImVec2(r.Min.x, r.Max.y - Snap(Px(1.0f))), r.Max), col::kHairline);
    constexpr Type kLabel{Font::kMedium, 18};
    DrawLabel(kLabel, ImVec2(r.Min.x + Px(20.0f), r.GetCenter().y - LineHeight(kLabel) * 0.5f), col::kTextSoft,
              t_.microphone_level);
    float bar_width = std::min(Px(320.0f), r.GetWidth() * 0.45f);
    ImRect track(ImVec2(r.Max.x - Px(20.0f) - bar_width, r.GetCenter().y - Px(4.0f)),
                 ImVec2(r.Max.x - Px(20.0f), r.GetCenter().y + Px(4.0f)));
    auto at = [&](float db) { return track.Min.x + track.GetWidth() * std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f); };
    float db = mic_meter_.peak > 0.0f ? 20.0f * std::log10(mic_meter_.peak) : -100.0f;
    float end = at(db);
    struct Zone {
      float from, to;
      ImU32 color;
    };
    const Zone zones[] = {{-60.0f, -18.0f, col::kSuccess}, {-18.0f, -6.0f, col::kGlyphY}, {-6.0f, 0.0f, col::kError}};
    for (int z = 0; z < 3; ++z) {
      const Zone& zone = zones[z];
      float left = at(zone.from);
      float right = at(zone.to);
      ImDrawFlags corners = z == 0 ? ImDrawFlags_RoundCornersLeft : z == 2 ? ImDrawFlags_RoundCornersRight : ImDrawFlags_RoundCornersNone;
      FillRect(ImRect(ImVec2(left, track.Min.y), ImVec2(right, track.Max.y)), Fade(zone.color, 0.25f), 4.0f, corners);
      float live = std::min(right, end);
      if (live > left) {
        FillRect(ImRect(ImVec2(left, track.Min.y), ImVec2(live, track.Max.y)), zone.color, 4.0f, corners);
      }
    }
    y += Px(kRow);
  }

  // How to set it, under the rows.
  constexpr Type kNote{Font::kRegular, 14};
  y += Px(16.0f);
  y += DrawParagraph(kNote, ImVec2(origin.x, y), width, 1.4f, col::kTextDim, t_.help_mic_tip_level);
  return y - origin.y;
}

}  // namespace launcher
