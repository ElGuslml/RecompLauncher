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
  const game::GameTexts& own = game::GameTextsFor(language_);
  ImRect r(origin, ImVec2(origin.x + width, origin.y + Px(44.0f)));
  ImRect chip(ImVec2(r.Max.x - Px(6.0f) - Px(190.0f), r.Min.y + Px(4.0f)), ImVec2(r.Max.x - Px(6.0f), r.Max.y - Px(4.0f)));
  MappingRow(r, 0, t_.part, chip.Min.x - Px(14.0f), false);
  bool vocals = Sings(key);
  ChipLook look;
  look.text = vocals ? (own.part_vocals ? own.part_vocals : t_.part_vocals)
                     : (own.part_instrument ? own.part_instrument : t_.part_instrument);
  if (vocals) {
    look.icon = Icon::kMicrophone;
    look.icon_color = col::kAccent;
  }
  ImGui::PushID("part");
  ItemState state = MappingChip("##chip", chip, look);
  ImGui::PopID();
  if (!mapping_first_) mapping_first_ = ImGui::GetItemID();
  mapping_rows_.push_back({ImGui::GetItemID(), ImGui::GetItemID()});
  if (state.pressed) {
    StopListening();
    settings_.o.device_subtypes = WithSubtype(settings_.o.device_subtypes, key, vocals ? -1 : kSingerSubtype);
    Changed();
  }
  return Px(44.0f);
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

float App::MicRows(ImVec2 origin, float width) {
  Options& o = settings_.o;
  if (!EnsureAudio()) {
    return 0.0f;
  }
  UpdateMicMeter();
  std::vector<Recorder> recorders = Recorders();
  constexpr float kRow = 44.0f;
  constexpr float kGap = 6.0f;
  float y = origin.y;
  auto row_rects = [&](float chip_width, ImRect* row, ImRect* chip) {
    *row = ImRect(ImVec2(origin.x, y), ImVec2(origin.x + width, y + Px(kRow)));
    *chip = ImRect(ImVec2(row->Max.x - Px(6.0f) - Px(chip_width), row->Min.y + Px(4.0f)),
                   ImVec2(row->Max.x - Px(6.0f), row->Max.y - Px(4.0f)));
  };
  auto navigable = [&]() {
    if (!mapping_first_) mapping_first_ = ImGui::GetItemID();
    mapping_rows_.push_back({ImGui::GetItemID(), ImGui::GetItemID()});
  };
  float chip_width = std::min(Px(320.0f), width * 0.6f) / Scale();

  // The device: A goes to the next one (none, the system's default, each device the system has), X back to none. A
  // device chosen before and not connected now stays chosen, marked.
  std::string& device = o.microphones[0];
  std::vector<std::string> values = {"", kSystemMicrophone};
  for (const Recorder& recorder : recorders) values.push_back(recorder.name);
  auto current = std::find(values.begin(), values.end(), device);
  bool missing = current == values.end();
  int channels = 0;  // the chosen device's, 0 when there is none
  {
    ImRect r, chip;
    row_rects(chip_width, &r, &chip);
    MappingRow(r, 0, t_.microphone_device, chip.Min.x - Px(14.0f), false);
    ChipLook look;
    if (device.empty()) {
      look.text = t_.microphone_none;
      look.empty = true;
    } else {
      std::string name = device == kSystemMicrophone ? std::string(t_.microphone_system) : device;
      if (missing) {
        char text[512];
        std::snprintf(text, sizeof(text), t_.microphone_missing, name.c_str());
        name = text;
      }
      look.text = name;
      look.icon = Icon::kMicrophone;
      look.icon_color = missing ? col::kError : col::kAccent;
    }
    ImGui::PushID("mic_device");
    ItemState state = MappingChip("##chip", chip, look);
    ImGui::PopID();
    navigable();
    if (state.pressed) {
      size_t index = missing ? 0 : size_t(current - values.begin());
      device = values[(index + 1) % values.size()];
      o.microphone_channels[0] = 0;
      Changed();
    } else if (!device.empty() && ClearPressed(state)) {
      device.clear();
      o.microphone_channels[0] = 0;
      Changed();
    }
    y += Px(kRow) + Px(kGap);
    if (!device.empty() && !missing) {
      channels = device == kSystemMicrophone ? ChannelsOf(SDL_AUDIO_DEVICE_DEFAULT_RECORDING)
                                             : recorders[size_t(current - values.begin()) - 2].channels;
    }
  }

  // The channel: all of them mixed, or one (an interface's input); a mono device has only its own.
  {
    ImRect r, chip;
    row_rects(chip_width, &r, &chip);
    MappingRow(r, 0, t_.microphone_channel, chip.Min.x - Px(14.0f), false);
    int& channel = o.microphone_channels[0];
    ChipLook look;
    char text[64];
    if (channels <= 1) {
      look.text = channels == 1 ? t_.microphone_mono : "\xE2\x80\x94";
      look.enabled = false;
    } else if (channel == 0 || channel > channels) {
      look.text = t_.microphone_all_channels;
    } else {
      std::snprintf(text, sizeof(text), t_.microphone_channel_n, channel);
      look.text = text;
    }
    ImGui::PushID("mic_channel");
    ItemState state = MappingChip("##chip", chip, look);
    ImGui::PopID();
    if (look.enabled) {
      navigable();
      if (state.pressed) {
        channel = (std::min(channel, channels) + 1) % (channels + 1);
        Changed();
      } else if (channel != 0 && ClearPressed(state)) {
        channel = 0;
        Changed();
      }
    }
    y += Px(kRow) + Px(kGap);
  }

  // The gain: steps of 6 dB, A to the next (louder, then back to the quietest), X back to the game's.
  {
    ImRect r, chip;
    row_rects(chip_width, &r, &chip);
    MappingRow(r, 0, t_.microphone_gain, chip.Min.x - Px(14.0f), false);
    constexpr int kGains[] = {-24, -18, -12, -6, 0, 6};
    int& gain = o.microphone_gains[0];
    ChipLook look;
    char text[32];
    std::snprintf(text, sizeof(text), gain > 0 ? "+%d dB" : gain < 0 ? "\xE2\x88\x92%d dB" : "%d dB", std::abs(gain));
    look.text = text;
    look.enabled = !device.empty();
    ImGui::PushID("mic_gain");
    ItemState state = MappingChip("##chip", chip, look);
    ImGui::PopID();
    if (look.enabled) {
      navigable();
      if (state.pressed) {
        const int* next = std::find_if(std::begin(kGains), std::end(kGains), [&](int g) { return g > gain; });
        gain = next == std::end(kGains) ? kGains[0] : *next;
        Changed();
      } else if (gain != game::kMicrophoneGainDefault && ClearPressed(state)) {
        gain = game::kMicrophoneGainDefault;
        Changed();
      }
    }
    y += Px(kRow) + Px(kGap);
  }

  // The level: what the game hears, from -60 dB to full scale; green, then yellow from -18 dB, red from -6 dB.
  {
    ImRect r, bar;
    row_rects(chip_width, &r, &bar);
    MappingRow(r, 0, t_.microphone_level, bar.Min.x - Px(14.0f), false);
    ImRect track(ImVec2(bar.Min.x, bar.GetCenter().y - Px(5.0f)), ImVec2(bar.Max.x, bar.GetCenter().y + Px(5.0f)));
    FillRect(track, col::kPanelTrack, 5.0f);
    auto at = [&](float db) { return track.Min.x + track.GetWidth() * std::clamp((db + 60.0f) / 60.0f, 0.0f, 1.0f); };
    float db = mic_meter_.peak > 0.0f ? 20.0f * std::log10(mic_meter_.peak) : -100.0f;
    float end = at(db);
    struct Zone {
      float from, to;
      ImU32 color;
    };
    const Zone zones[] = {{-60.0f, -18.0f, col::kSuccess}, {-18.0f, -6.0f, col::kGlyphY}, {-6.0f, 0.0f, col::kError}};
    for (const Zone& zone : zones) {
      float left = at(zone.from);
      float right = std::min(at(zone.to), end);
      if (right > left) {
        FillRect(ImRect(ImVec2(left, track.Min.y), ImVec2(right, track.Max.y)), zone.color, 5.0f);
      }
    }
    y += Px(kRow) + Px(kGap);
  }
  return y - Px(kGap) - origin.y;
}

}  // namespace launcher
