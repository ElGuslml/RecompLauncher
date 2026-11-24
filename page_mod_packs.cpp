// The Mods tab's Packs section (launcher next; mod_packs.h, game::kHasModPacks), in the category's place: the Mods
// folder's card (its path, what it holds or what was just done; Add mod…, which takes a pack's zip or its mod.toml, and
// the folder in Explorer; a zip or a folder dropped on the window is added too), then a row for each pack: its name,
// version and author, its description or its state, its switch. A pack with patches is prepared as it is switched on,
// its progress in its row (A again cancels it); a pack made for other files, another game or broken cannot be. The
// note at the foot is page_settings.cpp's.

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>

#include <SDL3/SDL_dialog.h>
#include <imgui_internal.h>

#include "app.h"
#include "game.h"
#include "launcher_log.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

std::wstring Wide(const std::string& utf8) {
  int length = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
  std::wstring wide(size_t(std::max(length, 1)), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, wide.data(), length);
  wide.resize(wcslen(wide.c_str()));
  return wide;
}

std::string Utf8(const std::wstring& wide) {
  if (wide.empty()) return {};
  int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0, nullptr, nullptr);
  std::string text(size_t(std::max(length, 0)), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), text.data(), length, nullptr, nullptr);
  return text;
}

// A text with its %s filled (two at most).
std::string Formatted(const char* format, const std::string& a, const std::string& b = {}) {
  char text[1024];
  std::snprintf(text, sizeof(text), format, a.c_str(), b.c_str());
  return text;
}

// "1,4 GB", or "350 MB" below a gigabyte (the DLC page's).
std::string SizeText(uint64_t bytes, const char* decimal_separator) {
  char text[32];
  if (bytes >= 1024ull * 1024 * 1024) {
    std::snprintf(text, sizeof(text), "%.1f GB", double(bytes) / (1024.0 * 1024.0 * 1024.0));
  } else {
    std::snprintf(text, sizeof(text), "%.0f MB", std::max(1.0, double(bytes) / (1024.0 * 1024.0)));
  }
  std::string out = text;
  std::replace(out.begin(), out.end(), '.', decimal_separator[0]);
  return out;
}

// The text cut in its middle to fit `width` pixels: a path keeps its drive and its last folders (the DLC page's).
std::string EllipsizeMiddle(const Type& type, const std::string& text, float width) {
  if (MeasureText(type, text).x <= width) return text;
  std::wstring wide = Wide(text);
  size_t keep_front = wide.size() / 2;
  size_t keep_back = wide.size() - keep_front;
  while (keep_front + keep_back > 2) {
    if (keep_front > keep_back) --keep_front; else --keep_back;
    std::string candidate = Utf8(wide.substr(0, keep_front)) + "\xE2\x80\xA6" + Utf8(wide.substr(wide.size() - keep_back));
    if (MeasureText(type, candidate).x <= width) return candidate;
  }
  return "\xE2\x80\xA6";
}

// A pack's path ('/' separators) as Windows writes it.
std::string Shown(std::string path) {
  std::replace(path.begin(), path.end(), '/', '\\');
  return path;
}

// mod.toml's text in the launcher's language (its name_<key> or description_<key>), else its own.
const std::string& Localized(const std::map<std::string, std::string>& texts, const std::string& fallback,
                             uint32_t language) {
  const char* key = language == 3 ? "de" : language == 4 ? "fr" : language == 5 ? "es" : language == 6 ? "it" : "en";
  auto it = texts.find(key);
  return it != texts.end() ? it->second : fallback;
}

// The last part of a path with '/' or '\' separators.
std::string LastPart(const std::string& path) {
  size_t slash = path.find_last_of("/\\");
  return slash == std::string::npos ? path : path.substr(slash + 1);
}

}  // namespace

void App::ReadModPacks() {
  mod_packs_read_ = true;
  // Nothing to read without the Mods folder: no thread, no line in the log.
  std::error_code ec;
  if (!mod_packs_scan_.valid() && !std::filesystem::exists(ModsFolder(exe_dir_), ec)) {
    mod_packs_ = ModPacks{ModsFolder(exe_dir_), {}, {}};
    return;
  }
  if (mod_packs_scan_.valid()) {
    mod_packs_rescan_ = true;  // read again after this one
    return;
  }
  mod_packs_scan_ = std::async(std::launch::async, ScanModPacks, exe_dir_);
}

void App::BrowseModPack() {
  // As BrowseDlc: the dialog answers on a thread of its own, PollModPacks takes the file on the UI thread. A folder's
  // pack is chosen by its mod.toml.
  static const SDL_DialogFileFilter kFilters[] = {{"Mod (.zip, mod.toml)", "zip;toml"}};
  auto* holder = new std::shared_ptr<DialogResult>(mod_pack_dialog_);
  SDL_ShowOpenFileDialog(
      [](void* userdata, const char* const* files, int) {
        auto* result = static_cast<std::shared_ptr<DialogResult>*>(userdata);
        if (files && files[0]) {
          std::lock_guard lock((*result)->mutex);
          (*result)->path = files[0];
        }
        delete result;
      },
      holder, window_, kFilters, 1, nullptr, false);
}

void App::AddModPackFrom(const std::string& path, bool replace) {
  if (mod_pack_adding_.valid()) return;  // one at a time
  if (mod_pack_preparation_.Get().state == ModPackPreparation::State::kRunning) {
    mod_packs_notice_ = t_.mod_packs_busy;
    mod_packs_notice_error_ = true;
    return;
  }
  mod_packs_notice_.clear();
  mod_pack_adding_path_ = path;
  Log("mod packs: adding %s%s", path.c_str(), replace ? ", replacing the one installed" : "");
  mod_pack_adding_ =
      std::async(std::launch::async, AddModPack, exe_dir_, std::filesystem::path(Wide(path)), replace);
}

void App::SwitchModPack(size_t index, bool on) {
  // Not while the folder is read again or a pack is added (the rows wait too).
  if (!mod_packs_ || index >= mod_packs_->packs.size() || mod_packs_scan_.valid() || mod_pack_adding_.valid()) return;
  ModPack& pack = mod_packs_->packs[index];
  mod_packs_notice_.clear();
  if (!on || pack.state == ModPack::State::kReady) {
    const bool written = SetModPackOn(exe_dir_, *mod_packs_, index, on);
    Log("mod pack %s: %s%s", pack.folder.c_str(), on ? "on" : "off",
        written ? "" : ": enabled.txt not written, the switch put back");
    if (!written) {
      mod_packs_notice_ = t_.mod_packs_not_saved;
      mod_packs_notice_error_ = true;
    }
    return;
  }
  if (pack.state != ModPack::State::kNeedsPreparing) return;
  // Its patches applied first, on a thread; switched on when they all are (PollModPacks).
  if (mod_pack_preparation_.Get().state == ModPackPreparation::State::kRunning) {
    mod_packs_notice_ = t_.mod_packs_busy;
    mod_packs_notice_error_ = true;
    return;
  }
  const uint64_t free = FreeSpace(mod_packs_->folder / std::filesystem::path(Wide(pack.folder)));
  if (free && free < pack.prepare_bytes) {
    mod_packs_notice_ = Formatted(t_.mod_packs_no_space, SizeText(pack.prepare_bytes, t_.decimal_separator),
                                  SizeText(free, t_.decimal_separator));
    mod_packs_notice_error_ = true;
    Log("mod pack %s: not prepared, %llu bytes to write and %llu free", pack.folder.c_str(),
        (unsigned long long)pack.prepare_bytes, (unsigned long long)free);
    return;
  }
  Log("mod pack %s: preparing (%llu bytes to write)", pack.folder.c_str(), (unsigned long long)pack.prepare_bytes);
  mod_pack_preparation_.Start(exe_dir_, pack.folder);
}

void App::PollModPacks() {
  if (!game::kHasModPacks) {
    return;
  }
  // Read when the launcher opens, not only when the section is shown: a pack updated since it was prepared is off
  // before the player presses Play.
  if (!mod_packs_read_) {
    ReadModPacks();
  }
  {
    std::optional<std::string> chosen;
    {
      std::lock_guard lock(mod_pack_dialog_->mutex);
      chosen.swap(mod_pack_dialog_->path);
    }
    if (chosen) {
      AddModPackFrom(*chosen, false);
    }
  }
  if (mod_packs_scan_.valid() && mod_packs_scan_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    mod_packs_ = mod_packs_scan_.get();
    Log("Mods folder: %s, %d packs, %d on", Utf8(mod_packs_->folder.wstring()).c_str(), int(mod_packs_->packs.size()),
        int(mod_packs_->On()));
    if (!mod_packs_->turned_off.empty()) {
      std::string names;
      for (const std::string& folder : mod_packs_->turned_off) {
        names += (names.empty() ? "" : ", ") + folder;
        Log("mod pack %s: in enabled.txt but not ready, switched off", folder.c_str());
      }
      mod_packs_notice_ = Formatted(t_.mod_packs_turned_off, names);
      mod_packs_notice_error_ = false;
    }
    if (mod_packs_rescan_) {
      mod_packs_rescan_ = false;
      ReadModPacks();
    }
  }
  // A pack added: what it did, and the folder read again (or the question before a pack is replaced).
  if (mod_pack_adding_.valid() && mod_pack_adding_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    ModPackAdd add = mod_pack_adding_.get();
    const std::string source = LastPart(mod_pack_adding_path_);
    mod_packs_notice_error_ = add.result != ModPackAdd::Result::kAdded && add.result != ModPackAdd::Result::kReplaced;
    switch (add.result) {
      case ModPackAdd::Result::kAdded:
        mod_packs_notice_ = Formatted(t_.mod_packs_added, Localized(add.names, add.name, language_));
        break;
      case ModPackAdd::Result::kReplaced:
        mod_packs_notice_ = Formatted(t_.mod_packs_replaced, Localized(add.names, add.name, language_), add.version);
        break;
      case ModPackAdd::Result::kAsk: {
        mod_packs_notice_.clear();
        const std::string path = mod_pack_adding_path_;
        AskConfirm(Formatted(t_.mod_packs_replace_title, Localized(add.names, add.name, language_)),
                   Formatted(t_.mod_packs_replace_text, add.existing_version, add.version), t_.mod_packs_replace,
                   [this, path] { AddModPackFrom(path, true); });
        break;
      }
      case ModPackAdd::Result::kOtherPack:
        mod_packs_notice_ = Formatted(t_.mod_packs_other_pack, add.id);
        break;
      case ModPackAdd::Result::kNotAPack:
        mod_packs_notice_ = Formatted(t_.mod_packs_not_a_pack, source);
        break;
      case ModPackAdd::Result::kNotAZip:
        mod_packs_notice_ = Formatted(t_.mod_packs_not_zip, source);
        break;
      case ModPackAdd::Result::kFailed:
        mod_packs_notice_ = Formatted(t_.mod_packs_add_failed, add.error);
        break;
    }
    static const char* const kResults[] = {"added",      "replaced",  "already installed, asking",
                                           "not added: another pack's folder", "not a pack",
                                           "not added: not a zip",              "not added"};
    Log("mod packs: %s: %s%s%s", source.c_str(), kResults[int(add.result)], add.error.empty() ? "" : ": ",
        add.error.c_str());
    ReadModPacks();
  }
  // A preparation ended: switched on when done; why not otherwise.
  const ModPackPreparation::Progress prepared = mod_pack_preparation_.Get();
  if (prepared.state == ModPackPreparation::State::kDone || prepared.state == ModPackPreparation::State::kFailed ||
      prepared.state == ModPackPreparation::State::kCancelled) {
    std::string name = prepared.folder;
    bool switched_on = false;
    if (mod_packs_) {
      for (size_t i = 0; i < mod_packs_->packs.size(); ++i) {
        ModPack& pack = mod_packs_->packs[i];
        if (pack.folder != prepared.folder) continue;
        name = Localized(pack.names, pack.name, language_);
        if (prepared.state == ModPackPreparation::State::kDone) {
          pack.state = ModPack::State::kReady;
          switched_on = SetModPackOn(exe_dir_, *mod_packs_, i, true);
        }
      }
    }
    char seconds[32];
    std::snprintf(seconds, sizeof(seconds), "%.1f", prepared.seconds);
    std::replace(seconds, seconds + std::strlen(seconds), '.', t_.decimal_separator[0]);
    switch (prepared.state) {
      case ModPackPreparation::State::kDone:
        Log("mod pack %s: %d files prepared in %.1f s, on", prepared.folder.c_str(), int(prepared.count),
            prepared.seconds);
        if (switched_on) {
          mod_packs_notice_ = Formatted(t_.mod_packs_prepared, name, seconds);
          mod_packs_notice_error_ = false;
        } else {
          Log("mod pack %s: enabled.txt not written, left off", prepared.folder.c_str());
          mod_packs_notice_ = t_.mod_packs_not_saved;
          mod_packs_notice_error_ = true;
        }
        break;
      case ModPackPreparation::State::kFailed: {
        Log("mod pack %s: not prepared after %.1f s: %s: %s", prepared.folder.c_str(), prepared.seconds,
            prepared.detail.c_str(), prepared.error.c_str());
        // A file that is not the player's, or a damaged patch: the pack's row says why; else the system's message.
        const bool by_file = prepared.failed_as == ModPack::State::kOtherFiles ||
                             prepared.failed_as == ModPack::State::kBroken;
        mod_packs_notice_ =
            Formatted(t_.mod_packs_prepare_failed, name, by_file ? Shown(prepared.detail) : prepared.error);
        mod_packs_notice_error_ = true;
        break;
      }
      default:
        Log("mod pack %s: preparation cancelled after %.1f s", prepared.folder.c_str(), prepared.seconds);
        break;
    }
    mod_pack_preparation_.Reset();
    ReadModPacks();
  }
}

void App::ModPacksCategory(const ImRect& rect) {
  ImGuiContext& g = *GImGui;
  mod_packs_focus_kind_ = 0;
  if (!mod_packs_read_) ReadModPacks();
  const float x = rect.Min.x;
  const float right = rect.Max.x;
  const float width = rect.GetWidth();
  float y = rect.Min.y + Px(12.0f);
  constexpr Type kPath{Font::kSemiBold, 16};
  constexpr Type kSmall{Font::kRegular, 14};
  constexpr Type kName{Font::kSemiBold, 16};
  const bool scanning = mod_packs_scan_.valid();
  const bool adding = mod_pack_adding_.valid();
  const ModPackPreparation::Progress preparing = mod_pack_preparation_.Get();
  const bool running = preparing.state == ModPackPreparation::State::kRunning;
  const size_t count = mod_packs_ ? mod_packs_->packs.size() : 0;
  std::vector<std::array<ImGuiID, 2>> ids;

  // The Mods folder's card: its icon, the folder and what it holds (or what is being done, or what was); at its right
  // Add mod… (the main action while there is no pack) and the folder in Explorer.
  ImGuiID add_id = 0;
  {
    const float link_size = 15.0f;
    const Type link_type{Font::kSemiBold, link_size};
    const float link_width = MeasureText(link_type, t_.mod_packs_open_folder).x + Px(4.0f) + Px(link_size * 0.95f);
    ButtonSpec add_spec{count ? ButtonKind::kGhost : ButtonKind::kPrimary, Type{Font::kSemiBold, 15}, 40, 16, 10,
                        Icon::kPlus, 16, 2.2f};
    const float add_width = ButtonWidth(add_spec, t_.mod_packs_add);
    ImRect card(ImVec2(x, y), ImVec2(right, y + Px(20.0f) * 2.0f + LineHeight(kPath) + Px(2.0f) + LineHeight(kSmall)));
    FillRect(card, col::kCard, 14.0f);
    StrokeRect(card, col::kHairline, 14.0f);
    const float link_x = card.Max.x - Px(20.0f) - link_width;
    const float add_x = link_x - Px(22.0f) - add_width;
    const float text_x = card.Min.x + Px(20.0f) + Px(22.0f) + Px(14.0f);
    const float text_width = add_x - Px(20.0f) - text_x;
    std::string sub;
    ImU32 sub_color = col::kTextDim;
    if (adding) {
      sub = Formatted(t_.mod_packs_adding, LastPart(mod_pack_adding_path_));
    } else if (!mod_packs_notice_.empty()) {
      sub = mod_packs_notice_;
      sub_color = mod_packs_notice_error_ ? col::kError : col::kSuccess;
    } else if (!mod_packs_) {
      sub = t_.mod_packs_reading;
    } else if (count == 0) {
      sub = t_.mod_packs_none;
    } else {
      char text[128];
      if (count == 1) {
        std::snprintf(text, sizeof(text), t_.mod_packs_count_one, int(mod_packs_->On()));
      } else {
        std::snprintf(text, sizeof(text), t_.mod_packs_count, int(count), int(mod_packs_->On()));
      }
      sub = text;
    }
    const float ty = card.Min.y + Px(20.0f);
    if (adding || !mod_packs_) {
      // A spinning arc while the folder is read the first time or a pack is added.
      ImDrawList* draw = ImGui::GetWindowDrawList();
      ImVec2 c(card.Min.x + Px(20.0f) + Px(11.0f), card.GetCenter().y);
      float start = float(ImGui::GetTime()) * 5.0f;
      draw->PathArcTo(c, Px(8.0f), start, start + 4.7f, 32);
      draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(2.2f));
    } else {
      DrawIcon(Icon::kFolder, ImVec2(card.Min.x + Px(20.0f), card.GetCenter().y - Px(11.0f)), 22.0f, col::kTextMuted,
               1.9f);
    }
    DrawLabel(kPath, ImVec2(text_x, ty), col::kText,
              EllipsizeMiddle(kPath, Utf8(ModsFolder(exe_dir_).wstring()), text_width));
    DrawLabel(kSmall, ImVec2(text_x, ty + LineHeight(kPath) + Px(2.0f)), sub_color, Ellipsize(kSmall, sub, text_width));
    if (Button("##mod_packs_add", ImVec2(add_x, card.GetCenter().y - Px(20.0f)), add_spec, t_.mod_packs_add,
               !adding && !running)) {
      BrowseModPack();
    }
    add_id = ImGui::GetItemID();
    if (Link("##mod_packs_open", ImVec2(link_x, card.GetCenter().y - LineHeight(link_type) * 0.5f),
             t_.mod_packs_open_folder, link_size)) {
      std::filesystem::path folder = ModsFolder(exe_dir_);
      std::error_code ec;
      std::filesystem::create_directories(folder, ec);
      OpenFolder(folder);
    }
    ids.push_back({add_id, ImGui::GetItemID()});
    y = card.Max.y + Px(16.0f);
  }

  // The packs: a row each (the DLC page's, 64 dp; with the progress bar while it is prepared).
  ImRect area(ImVec2(x, y), ImVec2(right + Px(16.0f), rect.Max.y));
  size_t switched = SIZE_MAX;
  bool switched_on = false;
  bool cancel = false;
  if (count == 0) {
    constexpr Type kEmptyTitle{Font::kSemiBold, 17};
    const char* title = mod_packs_ ? t_.mod_packs_empty_title : "\xE2\x80\xA6";
    ImRect space(area.Min, ImVec2(right, area.Max.y));
    float cy = space.GetCenter().y - (LineHeight(kEmptyTitle) + Px(4.0f) + LineHeight(kSmall)) * 0.5f;
    DrawLabel(kEmptyTitle, ImVec2(space.GetCenter().x - MeasureText(kEmptyTitle, title).x * 0.5f, cy), col::kTextSoft,
              title);
    if (mod_packs_) {
      DrawLabel(kSmall, ImVec2(space.GetCenter().x - MeasureText(kSmall, t_.mod_packs_empty_text).x * 0.5f,
                               cy + LineHeight(kEmptyTitle) + Px(4.0f)),
                col::kTextDim, t_.mod_packs_empty_text);
    }
  } else {
    // Two packs on that replace the same file: each row names the other (the later one's file is used).
    std::map<std::string, size_t> owner;
    std::vector<size_t> clash(count, SIZE_MAX);
    for (size_t i = 0; i < count; ++i) {
      const ModPack& pack = mod_packs_->packs[i];
      if (!pack.on) continue;
      for (const std::string& file : pack.files) {
        auto [it, added] = owner.emplace(file, i);
        if (!added && it->second != i) {
          if (clash[i] == SIZE_MAX) clash[i] = it->second;
          if (clash[it->second] == SIZE_MAX) clash[it->second] = i;
        }
      }
    }
    BeginScroll("##mod_packs", area);
    ImVec2 origin = ImGui::GetCursorScreenPos();
    float row_y = origin.y;
    for (size_t i = 0; i < count; ++i) {
      const ModPack& pack = mod_packs_->packs[i];
      const bool this_preparing = running && preparing.folder == pack.folder;
      const bool can_switch =
          pack.on || pack.state == ModPack::State::kReady || pack.state == ModPack::State::kNeedsPreparing;
      const std::string& pack_name = Localized(pack.names, pack.name, language_);
      const float height = Px(64.0f) + (this_preparing ? Px(10.0f) : 0.0f);
      ImRect row(ImVec2(origin.x, row_y), ImVec2(origin.x + width, row_y + height));
      ImGui::PushID(pack.folder.c_str());
      // The rows wait while the folder is read again or a pack is added (a switch then would be lost).
      const bool enabled = !scanning && !adding;
      ItemState state = Item("##pack", row, enabled);
      if (enabled) ids.push_back({ImGui::GetItemID(), ImGui::GetItemID()});
      if (state.focused) {
        FocusRow(row, 12.0f);
        mod_packs_focus_kind_ = this_preparing ? 2 : 1;
      } else {
        if (state.hovered) FillRect(row, Fade(col::kFocus, 0.03f), 12.0f);
        FillRect(ImRect(ImVec2(row.Min.x, row.Max.y - Snap(Px(1.0f))), row.Max), col::kHairline);
      }
      if (state.pressed) {
        if (this_preparing) {
          cancel = true;
        } else if (can_switch) {
          switched = i;
          switched_on = !pack.on;
        }
      }
      const float center_y = row.Min.y + Px(32.0f);
      const float toggle_x = row.Max.x - Px(16.0f) - Px(52.0f);
      DrawToggle(ImVec2(toggle_x, center_y - Px(15.0f)), pack.on || this_preparing, enabled && can_switch,
                 this_preparing, state.hovered);
      const float icon_x = row.Min.x + Px(16.0f);
      DrawIcon(Icon::kPackage, ImVec2(icon_x, center_y - Px(11.0f)), 22.0f, pack.on ? col::kTextMuted : col::kTextDim,
               1.75f);
      const float text_x = icon_x + Px(22.0f) + Px(14.0f);
      const float text_width = std::max(Px(40.0f), toggle_x - Px(14.0f) - text_x);
      const float text_y = center_y - (LineHeight(kName) + LineHeight(kSmall)) * 0.5f;
      // Its name, then its version and author.
      std::string name = Ellipsize(kName, pack_name, text_width);
      DrawLabel(kName, ImVec2(text_x, text_y), pack.on ? col::kText : col::kTextSoft, name);
      std::string meta;
      if (!pack.version.empty()) meta = Formatted(t_.mod_packs_version, pack.version);
      if (!pack.author.empty()) meta += (meta.empty() ? "" : " \xC2\xB7 ") + Formatted(t_.mod_packs_by, pack.author);
      const float name_width = MeasureText(kName, name).x;
      if (!meta.empty() && name_width + Px(10.0f) < text_width) {
        const float meta_y = text_y + (LineHeight(kName) - LineHeight(kSmall)) * 0.5f;
        DrawLabel(kSmall, ImVec2(text_x + name_width + Px(10.0f), meta_y), col::kTextDim,
                  Ellipsize(kSmall, meta, text_width - name_width - Px(10.0f)));
      }
      // Its second line: its state when there is something to say, else its description.
      std::string line = Localized(pack.descriptions, pack.description, language_);
      ImU32 color = col::kTextDim;
      if (this_preparing) {
        const int percent = preparing.total ? int(preparing.done * 100 / preparing.total) : 0;
        char text[512];
        std::snprintf(text, sizeof(text), t_.mod_packs_preparing, percent, LastPart(preparing.current).c_str());
        line = text;
        color = col::kTextMuted;
      } else {
        switch (pack.state) {
          case ModPack::State::kNeedsPreparing:
            line = Formatted(t_.mod_packs_needs_preparing, SizeText(pack.prepare_bytes, t_.decimal_separator));
            color = col::kTextMuted;
            break;
          case ModPack::State::kOtherFiles:
            line = Formatted(t_.mod_packs_other_files, Shown(pack.detail));
            color = col::kGlyphY;
            break;
          case ModPack::State::kOtherGame:
            line = Formatted(t_.mod_packs_other_game, pack.detail);
            color = col::kError;
            break;
          case ModPack::State::kBroken:
            switch (pack.broken) {
              case ModPack::Broken::kFormat:
                line = Formatted(t_.mod_packs_broken_format, pack.detail);
                break;
              case ModPack::Broken::kEmpty:
                line = t_.mod_packs_broken_empty;
                break;
              case ModPack::Broken::kPatch:
                line = Formatted(t_.mod_packs_broken_patch, Shown(pack.detail));
                break;
              case ModPack::Broken::kTooLarge:
                line = Formatted(t_.mod_packs_broken_large, Shown(pack.detail));
                break;
              case ModPack::Broken::kFolderName:
                line = t_.mod_packs_broken_folder;
                break;
              default:
                line = t_.mod_packs_broken_toml;
                break;
            }
            color = col::kError;
            break;
          case ModPack::State::kUnavailable:
            line = t_.mod_packs_unavailable;
            color = col::kGlyphY;
            break;
          case ModPack::State::kReady:
            if (clash[i] != SIZE_MAX) {
              const ModPack& other = mod_packs_->packs[clash[i]];
              line = Formatted(t_.mod_packs_clash, Localized(other.names, other.name, language_));
              color = col::kGlyphY;
            }
            break;
        }
      }
      DrawLabel(kSmall, ImVec2(text_x, text_y + LineHeight(kName)), color, Ellipsize(kSmall, line, text_width));
      if (this_preparing) {
        const float bar_y = text_y + LineHeight(kName) + LineHeight(kSmall) + Px(6.0f);
        Progress(ImRect(ImVec2(text_x, bar_y), ImVec2(text_x + text_width, bar_y + Px(4.0f))),
                 preparing.total ? float(double(preparing.done) / double(preparing.total)) : 0.0f, col::kChip,
                 col::kAccent);
      }
      ImGui::PopID();
      row_y = row.Max.y;
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(ImVec2(width, row_y - origin.y));
    EndScroll();
  }

  // The controller: up and down through the card and the rows, left back to the categories (and B), right from a
  // category onto Add mod….
  ColumnNav(ids);
  for (size_t r = 0; r < ids.size(); ++r) {
    if (g.NavId == ids[r][0] && g.NavMoveSubmitted && g.NavMoveDir == ImGuiDir_Left) {
      RequestFocus(category_focus_);
      ImGui::NavMoveRequestCancel();
    }
  }
  page_focus_ = add_id;
  settings_row_focus_ = add_id;
  if (std::any_of(ids.begin(), ids.end(), [&](const auto& id) { return id[0] == g.NavId || id[1] == g.NavId; })) {
    back_focus_ = category_focus_;
  }

  if (cancel) {
    Log("mod pack %s: cancelling its preparation", preparing.folder.c_str());
    mod_pack_preparation_.Cancel();
  } else if (switched != SIZE_MAX) {
    SwitchModPack(switched, switched_on);
  }

  // Y: what a pack is, how one is added and prepared, the load order.
  if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ButtonPressed(PadButton::kY)) {
    OpenInfo(t_.more_info, t_.group_packs, t_.mod_packs_help,
             {t_.mod_packs_help_add, t_.mod_packs_help_patches, t_.mod_packs_help_order, t_.mod_packs_note});
  }
}

}  // namespace launcher
