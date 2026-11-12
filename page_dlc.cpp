// The DLC tab (dlc.h; Gustavo's 2026-10-28 mockup), in the game's mode (game::kDlcMode). kFolder, here: a card for the
// player's DLC folder, whose packages the game reads where they are (the folder dialog, or a folder dropped on the
// window), and the list of the game's DLC: the folder's packages and the ones extracted by launchers before 3.16, each
// with its switch (and Remove for the extracted). kInstall: page_dlc_install.cpp.

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <SDL3/SDL_dialog.h>

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

// "1,4 GB", or "350 MB" below a gigabyte.
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

// A row of the list: a package of the DLC folder, or one extracted before.
struct DlcRow {
  bool library = false;
  size_t index = 0;  // in the folder's packages, or in the extracted list
  const std::string* name = nullptr;
  uint64_t bytes = 0;
  bool on = false;
  bool fits = true;
  bool copy = false;  // an extracted copy of a package of the DLC folder
};

// A group of the list: the packages made for one game (by the header's title ID), or the extracted copies.
constexpr uint64_t kInstalledGroup = uint64_t(1) << 32;
struct DlcGroup {
  uint64_t key = 0;  // the title ID, or kInstalledGroup
  bool installed = false;
  std::string name;
  std::map<std::string, int> names;  // its game's names in the headers, and how often
  std::vector<DlcRow> rows;
  size_t on = 0;
  bool fits = false;  // the game reads one of them at least
};

}  // namespace

void App::BrowseDlc() {
  // As BrowseIso: the dialog answers on a thread of its own, PollDlc takes the folder on the UI thread.
  auto* holder = new std::shared_ptr<DialogResult>(dlc_dialog_);
  SDL_ShowOpenFolderDialog(
      [](void* userdata, const char* const* folders, int) {
        auto* result = static_cast<std::shared_ptr<DialogResult>*>(userdata);
        if (folders && folders[0]) {
          std::lock_guard lock((*result)->mutex);
          (*result)->path = folders[0];
        }
        delete result;
      },
      holder, window_, nullptr, false);
}

void App::ChooseDlcLibrary(const std::string& path) {
  std::filesystem::path folder(Wide(path));
  std::error_code ec;
  if (std::filesystem::is_regular_file(folder, ec)) {
    folder = folder.parent_path();  // a package dropped: its folder
  }
  SetDlcLibraryFolder(exe_dir_, folder);
  ReadDlcLibrary();
}

void App::ReadDlcLibrary() {
  if (dlc_scan_.valid()) {
    dlc_rescan_ = true;  // read again after this one: the folder or a choice changed
    return;
  }
  dlc_scan_ = std::async(std::launch::async, ScanDlcLibrary, exe_dir_);
}

void App::ListDlc() {
  dlc_listed_ = true;
  if (!dlc_listing_.valid()) {
    dlc_listing_ = std::async(std::launch::async, ListInstalledDlc, exe_dir_);
  } else {
    dlc_relist_ = true;  // a change while one is read: read again after it
  }
}

void App::DlcPage(const ImRect& rect) {
  if constexpr (game::kDlcMode == game::DlcMode::kFolder) {
    DlcFolderPage(rect);
  } else {
    DlcInstallPage(rect);
  }
}

void App::PollDlc() {
  if (!game::kHasDlcPage) {
    return;
  }
  if constexpr (game::kDlcMode == game::DlcMode::kFolder) {
    PollDlcFolder();
  } else {
    PollDlcInstall();
  }
}

void App::PollDlcFolder() {
  // Read when the launcher opens, not only when the page is shown: the DLC folder's new packages are in the game's list,
  // and a package the game does not read (game::kDlcRule) is off before the player presses Play.
  if (!dlc_listed_) {
    ListDlc();
    ReadDlcLibrary();
  }
  {
    std::optional<std::string> chosen;
    {
      std::lock_guard lock(dlc_dialog_->mutex);
      chosen.swap(dlc_dialog_->path);
    }
    if (chosen) {
      ChooseDlcLibrary(*chosen);
    }
  }
  if (dlc_scan_.valid() && dlc_scan_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    dlc_library_ = dlc_scan_.get();
    Log("DLC folder: %s, %d packages, %d for this game", Utf8(dlc_library_->folder.wstring()).c_str(),
        int(dlc_library_->packages.size()), int(dlc_library_->Fitting()));
    if (dlc_rescan_) {
      dlc_rescan_ = false;
      ReadDlcLibrary();
    }
  }
  if (dlc_listing_.valid() && dlc_listing_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    dlc_installed_ = dlc_listing_.get();
    if (dlc_relist_) {
      dlc_relist_ = false;
      ListDlc();
    }
  }
  // The copies deleted: what was freed, and both lists read again (the folder's packages take the copies' place).
  if (dlc_freeing_.valid() && dlc_freeing_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    FreedDlc freed = dlc_freeing_.get();
    char text[256];
    std::snprintf(text, sizeof(text), t_.dlc_freed, int(freed.copies), SizeText(freed.bytes, t_.decimal_separator).c_str());
    dlc_freed_text_ = text;
    Log("DLC: %d extracted copies deleted, %llu bytes", int(freed.copies), (unsigned long long)freed.bytes);
    ListDlc();
    ReadDlcLibrary();
  }
}

// The text cut in its middle to fit `width` pixels ("G:\dev\…\launcher_shots\DLC"): a path keeps its drive and its last
// folders.
static std::string EllipsizeMiddle(const Type& type, const std::string& text, float width) {
  if (MeasureText(type, text).x <= width) {
    return text;
  }
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

void App::DlcFolderPage(const ImRect& rect) {
  if (!dlc_listed_) {
    ListDlc();
    ReadDlcLibrary();
  }
  ImGuiContext& g = *GImGui;
  dlc_focus_kind_ = 0;
  // --- The layout (launcher 3.18, the mockup's boards 2-4; dp from the page's top left): the title at 56, 20 and its
  // line at 74; the folder's column from 56, 124, 340 wide; a hairline at 432; the list's column from 472 to 56 from
  // the right edge. No cards: Y opens the folder's whole text. ---
  const float page_left = rect.Min.x;
  const float page_top = rect.Min.y;
  constexpr Type kTitle{Font::kDisplaySemiBold, 44, 0.015f};
  constexpr Type kIntro{Font::kRegular, 16};
  constexpr Type kHeading{Font::kDisplayMedium, 26, 0.015f};
  constexpr Type kSmall{Font::kRegular, 14};
  DrawLabel(kTitle, ImVec2(page_left + Px(56.0f), page_top + Px(20.0f)), col::kText, Title(t_.nav_dlc), Px(44.0f * 1.1f));
  DrawLabel(kIntro, ImVec2(page_left + Px(56.0f), page_top + Px(74.0f)), col::kTextMuted, t_.dlc_subtitle);
  const float columns_top = page_top + Px(124.0f);
  const float foot = rect.Max.y - Px(28.0f);
  FillRect(ImRect(ImVec2(page_left + Px(432.0f), columns_top + Px(4.0f)),
                  ImVec2(page_left + Px(432.0f) + Snap(Px(1.0f)), foot - Px(10.0f))),
           col::kHairline);
  bool scanning = dlc_scan_.valid();
  bool has_folder = dlc_library_ && !dlc_library_->folder.empty();
  ImGuiID choose_id = 0;
  std::vector<ImGuiID> rightmost;  // the folder column's controls right from which is the list

  // The DLC folder: its heading, two sentences, the folder (or the place to drop one) and what it holds, the extracted
  // copies of its packages; at the foot Choose (or Change) and Open the DLC folder.
  {
    const float x = page_left + Px(56.0f);
    const float width = Px(340.0f);
    float y = columns_top;
    DrawLabel(kHeading, ImVec2(x, y), col::kText, t_.dlc_library_title);
    y += LineHeight(kHeading) + Px(12.0f);
    constexpr Type kText{Font::kRegular, 15};
    y += DrawParagraph(kText, ImVec2(x, y), width, 1.5f, col::kTextMuted, t_.dlc_folder_short) + Px(22.0f);

    // The folder: a line between two hairlines, its icon (a check, an alert, a spinning arc while it is read), the
    // folder and what it holds. No folder chosen: a dashed place to drop one.
    constexpr Type kPath{Font::kSemiBold, 16};
    std::string folder_text = has_folder ? Utf8(dlc_library_->folder.wstring()) : std::string();
    if (!has_folder && !scanning) {
      ImRect zone(ImVec2(x, y), ImVec2(x + width, y + Px(112.0f)));
      DashedRect(zone, col::kOutline, 14.0f, 6.0f, 4.0f);
      DrawIcon(Icon::kUpload, ImVec2(zone.GetCenter().x - Px(12.0f), zone.Min.y + Px(22.0f)), 24.0f, col::kTextMuted, 1.75f);
      constexpr Type kDrop{Font::kSemiBold, 16};
      std::string drop = Ellipsize(kDrop, t_.dlc_drop_here, width - Px(32.0f));
      DrawLabel(kDrop, ImVec2(zone.GetCenter().x - MeasureText(kDrop, drop).x * 0.5f, zone.Min.y + Px(56.0f)), col::kText,
                drop);
      y = zone.Max.y + Px(14.0f);
    } else {
      ImRect line(ImVec2(x, y), ImVec2(x + width, y + Px(14.0f) * 2.0f + LineHeight(kPath) + Px(2.0f) + LineHeight(kSmall)));
      FillRect(ImRect(line.Min, ImVec2(line.Max.x, line.Min.y + Snap(Px(1.0f)))), col::kHairline);
      FillRect(ImRect(ImVec2(line.Min.x, line.Max.y - Snap(Px(1.0f))), line.Max), col::kHairline);
      std::string sub;
      ImU32 sub_color = col::kTextDim;
      ImVec2 icon(line.Min.x, line.GetCenter().y - Px(11.0f));
      if (scanning) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 c(icon.x + Px(11.0f), icon.y + Px(11.0f));
        float start = float(ImGui::GetTime()) * 5.0f;
        draw->PathArcTo(c, Px(8.0f), start, start + 4.7f, 32);
        draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(2.2f));
        if (folder_text.empty()) folder_text = Utf8(DlcLibraryFolder(exe_dir_).wstring());
        sub = t_.dlc_searching_short;
      } else if (!dlc_library_->available) {
        DrawIcon(Icon::kAlert, icon, 22.0f, col::kError, 1.9f);
        sub = t_.dlc_library_unavailable;
        sub_color = col::kError;
      } else if (dlc_library_->packages.empty()) {
        DrawIcon(Icon::kAlert, icon, 22.0f, col::kTextMuted, 1.9f);
        sub = t_.dlc_none_found_short;
      } else {
        DrawIcon(Icon::kCheck, icon, 22.0f, col::kSuccess, 2.2f);
        char found[128];
        std::snprintf(found, sizeof(found), t_.dlc_library_found, int(dlc_library_->packages.size()),
                      int(dlc_library_->Fitting()));
        sub = found;
      }
      float text_x = line.Min.x + Px(22.0f) + Px(14.0f);
      float text_width = line.Max.x - text_x;
      float ty = line.Min.y + Px(14.0f);
      DrawLabel(kPath, ImVec2(text_x, ty), col::kText, EllipsizeMiddle(kPath, folder_text, text_width));
      DrawLabel(kSmall, ImVec2(text_x, ty + LineHeight(kPath) + Px(2.0f)), sub_color, Ellipsize(kSmall, sub, text_width));
      y = line.Max.y + Px(14.0f);
    }
    DrawIcon(Icon::kUpload, ImVec2(x, y + (LineHeight(kSmall) - Px(16.0f)) * 0.5f), 16.0f, col::kTextDim, 1.8f);
    DrawLabel(kSmall, ImVec2(x + Px(16.0f) + Px(8.0f), y), col::kTextDim,
              Ellipsize(kSmall, t_.dlc_drop_hint, width - Px(24.0f)));
    y += LineHeight(kSmall) + Px(16.0f);

    // The extracted copies of the folder's packages: how many and their size, with Remove copies (its dialog); or,
    // once deleted, what was freed.
    size_t copy_count = dlc_library_ ? dlc_library_->Copies() : 0;
    bool freeing = dlc_freeing_.valid();
    std::string notice;
    if (freeing) {
      notice = t_.dlc_freeing;
    } else if (copy_count > 0 && !scanning) {
      char text[256];
      std::string size_text = SizeText(dlc_library_->CopyBytes(), t_.decimal_separator);
      if (copy_count == 1) {
        std::snprintf(text, sizeof(text), t_.dlc_copies_one, size_text.c_str());
      } else {
        std::snprintf(text, sizeof(text), t_.dlc_copies_many, int(copy_count), size_text.c_str());
      }
      notice = text;
    } else if (!dlc_freed_text_.empty()) {
      notice = dlc_freed_text_;
    }
    if (!notice.empty()) {
      bool done = copy_count == 0 && !freeing;
      DrawIcon(done ? Icon::kCheck : Icon::kInfo, ImVec2(x, y + Px(1.0f)), 18.0f, done ? col::kSuccess : col::kTextMuted, 1.9f);
      y += DrawParagraph(kSmall, ImVec2(x + Px(26.0f), y), width - Px(26.0f), 1.4f, col::kTextMuted, notice) + Px(10.0f);
      if (copy_count > 0 && !scanning && !freeing) {
        ButtonSpec spec{ButtonKind::kDanger, Type{Font::kSemiBold, 14}, 34, 16, 10};
        if (Button("##dlc_remove_copies", ImVec2(x + Px(26.0f), y), spec, t_.dlc_remove_copies)) {
          dlc_free_confirm_ = true;
        }
        rightmost.push_back(ImGui::GetItemID());
      }
    }

    // Choose a folder… (the main action while there is none; Change folder… an outline once there is) and the folder in
    // Explorer (the DLC folder, else the content folder of the extracted).
    float buttons_y = foot - Px(40.0f) - Px(48.0f);
    ButtonSpec choose_spec{has_folder ? ButtonKind::kGhost : ButtonKind::kPrimary, Type{Font::kSemiBold, 16}, 48, 20, 12};
    const char* choose_label = has_folder ? t_.dlc_change_folder : t_.dlc_choose_folder;
    if (Button("##dlc_choose", ImVec2(x, buttons_y), choose_spec, choose_label, !scanning)) {
      BrowseDlc();
    }
    choose_id = ImGui::GetItemID();
    page_focus_ = choose_id;
    float link_x = x + ButtonWidth(choose_spec, choose_label) + Px(22.0f);
    if (Link("##dlc_open", ImVec2(link_x, buttons_y + (Px(48.0f) - LineHeight(Type{Font::kSemiBold, 15})) * 0.5f),
             t_.dlc_open_folder, 15.0f)) {
      if (has_folder && dlc_library_->available) {
        OpenFolder(dlc_library_->folder);
      } else {
        std::filesystem::path folder = DlcFolder(exe_dir_);
        std::error_code ec;
        std::filesystem::create_directories(folder, ec);
        OpenFolder(folder);
      }
    }
    rightmost.push_back(ImGui::GetItemID());
    // Right from the column's rightmost controls enters the list (ImGui would not reach its rows, far above them).
    if (dlc_list_entry_ && g.NavMoveSubmitted && g.NavMoveDir == ImGuiDir_Right &&
        std::find(rightmost.begin(), rightmost.end(), g.NavId) != rightmost.end()) {
      RequestFocus(dlc_list_entry_);
      ImGui::NavMoveRequestCancel();
    }
    rightmost.clear();
  }

  // The game's DLC, grouped by the game each package was made for (the extracted copies apart): each group a row that
  // folds open or shut (A), with how many are on and a switch for all of it (X); under an open one, each package a row
  // with its switch (A).
  {
    const float x = page_left + Px(472.0f);
    const float right = rect.Max.x - Px(56.0f);
    const float width = right - x;
    float y = columns_top;
    // A package of the folder with an extracted copy installed shows as its copy: the copy is what the game reads.
    std::set<std::string> copies;
    if (dlc_library_) {
      for (const LibraryDlc& p : dlc_library_->packages) {
        if (!p.copy_folder.empty()) copies.insert(p.copy_folder);
      }
    }
    std::vector<DlcGroup> groups;
    auto group_for = [&groups](uint64_t key) -> DlcGroup& {
      for (DlcGroup& group : groups) {
        if (group.key == key) return group;
      }
      groups.emplace_back();
      groups.back().key = key;
      return groups.back();
    };
    if (dlc_library_) {
      for (size_t i = 0; i < dlc_library_->packages.size(); ++i) {
        const LibraryDlc& p = dlc_library_->packages[i];
        if (!p.copy_folder.empty()) continue;
        DlcGroup& group = group_for(p.title_id);
        group.rows.push_back({true, i, &p.display_name, p.bytes, p.on, p.fits});
        if (!p.title_name.empty()) ++group.names[p.title_name];
      }
    }
    if (dlc_installed_) {
      for (size_t i = 0; i < dlc_installed_->size(); ++i) {
        const InstalledDlc& d = (*dlc_installed_)[i];
        DlcGroup& group = group_for(kInstalledGroup);
        group.installed = true;
        group.rows.push_back({false, i, &d.display_name, d.bytes, d.on, d.fits, copies.count(d.folder) != 0});
      }
    }
    size_t total = 0;
    for (DlcGroup& group : groups) {
      std::stable_sort(group.rows.begin(), group.rows.end(),
                       [](const DlcRow& a, const DlcRow& b) { return *a.name < *b.name; });
      for (const DlcRow& r : group.rows) {
        group.on += r.on;
        group.fits = group.fits || r.fits;
      }
      total += group.rows.size();
      // Its game's name: the one its headers give most (they spell it several ways), else the title ID.
      if (group.installed) {
        group.name = t_.dlc_group_installed;
      } else if (!group.names.empty()) {
        group.name = std::max_element(group.names.begin(), group.names.end(),
                                      [](const auto& a, const auto& b) { return a.second < b.second; })
                         ->first;
      } else {
        char id[16];
        std::snprintf(id, sizeof(id), "%08X", uint32_t(group.key));
        group.name = id;
      }
    }
    // This game's own first, then the copies installed, then the games whose packages it reads, then the rest.
    auto rank = [](const DlcGroup& group) {
      if (!group.installed && uint32_t(group.key) == game::kTitleId) return 0;
      if (group.installed) return 1;
      return group.fits ? 2 : 3;
    };
    std::stable_sort(groups.begin(), groups.end(), [&rank](const DlcGroup& a, const DlcGroup& b) {
      return rank(a) != rank(b) ? rank(a) < rank(b) : a.name < b.name;
    });

    DrawLabel(kHeading, ImVec2(x, y), col::kText, t_.dlc_list_title);
    char count[64];
    std::snprintf(count, sizeof(count), total == 1 ? t_.dlc_count_one : t_.dlc_count, int(total));
    // Baselines together (Oswald's ascent 1.193 em, Source Sans 3's 1.024 em, ui.cpp).
    DrawLabel(kSmall, ImVec2(right - MeasureText(kSmall, count).x, y + Px(26.0f) * 1.193f - Px(14.0f) * 1.024f),
              col::kTextDim, count);
    y += LineHeight(kHeading) + Px(2.0f);
    // Where the game shows the songs: a line under the title.
    if (const char* where = game::GameTextsFor(language_).dlc_where) {
      DrawLabel(kSmall, ImVec2(x, y), col::kTextDim, Ellipsize(kSmall, where, width));
      y += LineHeight(kSmall);
    }
    y += Px(18.0f);
    FillRect(ImRect(ImVec2(x, y), ImVec2(right, y + Snap(Px(1.0f)))), col::kHairline);
    ImRect area(ImVec2(x, y + Snap(Px(1.0f))), ImVec2(right + Px(16.0f), foot));
    if (groups.empty()) {
      constexpr Type kEmptyTitle{Font::kSemiBold, 17};
      bool reading = scanning || dlc_listing_.valid();
      const char* title = reading ? "\xE2\x80\xA6" : t_.dlc_empty_title;
      ImRect space(area.Min, ImVec2(right, area.Max.y));
      float cy = space.GetCenter().y - (LineHeight(kEmptyTitle) + Px(4.0f) + LineHeight(kSmall)) * 0.5f;
      DrawLabel(kEmptyTitle, ImVec2(space.GetCenter().x - MeasureText(kEmptyTitle, title).x * 0.5f, cy), col::kTextSoft,
                title);
      if (!reading) {
        DrawLabel(kSmall, ImVec2(space.GetCenter().x - MeasureText(kSmall, t_.dlc_empty_text).x * 0.5f,
                                 cy + LineHeight(kEmptyTitle) + Px(4.0f)),
                  col::kTextDim, t_.dlc_empty_text);
      }
    } else {
      BeginScroll("##dlc_installed", area);
      ImVec2 origin = ImGui::GetCursorScreenPos();
      const float row_width = width;
      float row_y = origin.y;
      constexpr Type kGroup{Font::kSemiBold, 18};
      constexpr Type kName{Font::kSemiBold, 16};
      std::string removed;
      const DlcRow* switched = nullptr;
      bool switched_on = false;
      const DlcGroup* group_switched = nullptr;
      bool group_switched_on = false;
      std::vector<std::array<ImGuiID, 2>> row_ids;
      // A small list opens every group; a big one starts folded. The player's clicks are kept.
      bool open_by_default = total <= 12;
      for (size_t gi = 0; gi < groups.size(); ++gi) {
        const DlcGroup& group = groups[gi];
        ImGui::PushID(int(gi));
        auto known = dlc_group_open_.find(group.key);
        bool open = known != dlc_group_open_.end() ? known->second : open_by_default;
        // The group's row (the mockup's ".grow", 56 dp): the chevron, its game, how many are on, the switch for all of
        // it (mixed while some are on). A folds it; X, or a click on the switch, sets all of it.
        ImRect header(ImVec2(origin.x, row_y), ImVec2(origin.x + row_width, row_y + Px(56.0f)));
        bool all_on = group.on == group.rows.size();
        bool mixed = group.on > 0 && !all_on;
        bool switch_enabled = !(scanning && !group.installed);
        ImGui::SetNextItemAllowOverlap();
        ItemState fold = Item("##fold", header);
        row_ids.push_back({ImGui::GetItemID(), ImGui::GetItemID()});
        if (fold.focused) {
          FocusRow(header, 12.0f);
          dlc_focus_kind_ = 1;
          dlc_focus_all_on_ = all_on;
        } else {
          if (fold.hovered) FillRect(header, Fade(col::kFocus, 0.03f), 12.0f);
          FillRect(ImRect(ImVec2(header.Min.x, header.Max.y - Snap(Px(1.0f))), header.Max), col::kHairline);
        }
        if (fold.pressed) {
          dlc_group_open_[group.key] = !open;
        }
        ImVec2 toggle(header.Max.x - Px(16.0f) - Px(52.0f), header.GetCenter().y - Px(15.0f));
        bool group_value = all_on;
        bool by_switch = Toggle("##group_on", toggle, &group_value, switch_enabled, mixed, false);
        bool by_x = fold.focused && switch_enabled && ButtonPressed(PadButton::kX);
        if (by_switch || by_x) {
          group_switched = &group;
          group_switched_on = by_switch ? group_value : !all_on;
        }
        DrawIcon(open ? Icon::kChevronDown : Icon::kChevronRight,
                 ImVec2(header.Min.x + Px(16.0f), header.GetCenter().y - Px(10.0f)), 20.0f, col::kTextMuted, 2.0f);
        char on_text[64];
        std::snprintf(on_text, sizeof(on_text), t_.dlc_group_on, int(group.on), int(group.rows.size()));
        float on_width = MeasureText(kSmall, on_text).x;
        float on_x = toggle.x - Px(10.0f) - on_width;
        float name_x = header.Min.x + Px(16.0f) + Px(20.0f) + Px(12.0f);
        DrawLabel(kGroup, ImVec2(name_x, header.Min.y), group.fits ? col::kText : col::kTextSoft,
                  Ellipsize(kGroup, group.name, on_x - Px(16.0f) - name_x), header.GetHeight());
        DrawLabel(kSmall, ImVec2(on_x, header.Min.y), col::kTextMuted, on_text, header.GetHeight());
        row_y = header.Max.y;
        if (!open) {
          ImGui::PopID();
          continue;
        }
        for (size_t i = 0; i < group.rows.size(); ++i) {
          const DlcRow& dlc = group.rows[i];
          // A package's row (".srow", 64 dp, 48 in): a music note, its name and size, "Not for this game" when the game
          // does not read it, Remove for an extracted one (X), and its switch (A, or a click on the row).
          const char* copy_note = dlc.copy ? t_.dlc_copy_note : nullptr;
          float height = Px(64.0f) + (copy_note ? LineHeight(kSmall) : 0.0f);
          ImRect row(ImVec2(origin.x, row_y), ImVec2(origin.x + row_width, row_y + height));
          ImGui::PushID(int(i));
          bool enabled = !(dlc.library && scanning);
          ImGui::SetNextItemAllowOverlap();
          ItemState state = Item("##package", row, enabled);
          if (enabled) row_ids.push_back({ImGui::GetItemID(), ImGui::GetItemID()});
          if (state.focused) {
            FocusRow(row, 12.0f);
            dlc_focus_kind_ = dlc.library ? 2 : 3;
          } else {
            if (state.hovered) FillRect(row, Fade(col::kFocus, 0.03f), 12.0f);
            FillRect(ImRect(ImVec2(row.Min.x, row.Max.y - Snap(Px(1.0f))), row.Max), col::kHairline);
          }
          float center_y = row.Min.y + Px(32.0f);
          // On: the game reads it; off: kept out of the game's sight. The folder's wait while it is read.
          float toggle_x = row.Max.x - Px(16.0f) - Px(52.0f);
          DrawToggle(ImVec2(toggle_x, center_y - Px(15.0f)), dlc.on, enabled, false, state.hovered);
          if (state.pressed) {
            switched = &dlc;
            switched_on = !dlc.on;
          }
          float text_right = toggle_x - Px(14.0f);
          // Remove: only an extracted package (the folder's are the player's files, never deleted), pressed twice.
          if (!dlc.library) {
            const std::string& folder = (*dlc_installed_)[dlc.index].folder;
            bool confirming = dlc_confirm_ == folder;
            const char* label = confirming ? t_.dlc_remove_confirm : t_.dlc_remove;
            ButtonSpec spec{ButtonKind::kDanger, Type{Font::kSemiBold, 14}, 34, 14, 10, Icon::kTrash, 16, 2.0f};
            float remove_width = ButtonWidth(spec, label);
            ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
            bool pressed = Button("##remove", ImVec2(text_right - remove_width, center_y - Px(17.0f)), spec, label);
            ImGui::PopItemFlag();
            if (pressed || (state.focused && ButtonPressed(PadButton::kX))) {
              if (confirming) {
                removed = folder;
              } else {
                dlc_confirm_ = folder;
              }
            }
            text_right -= remove_width + Px(14.0f);
          }
          // Not for this game (game::kDlcRule): a quiet note with an info icon, not an error.
          if (!dlc.fits) {
            float note_width = MeasureText(kSmall, t_.dlc_not_for_game).x;
            float note_x = text_right - note_width;
            DrawLabel(kSmall, ImVec2(note_x, center_y - LineHeight(kSmall) * 0.5f), col::kTextDim, t_.dlc_not_for_game);
            DrawIcon(Icon::kInfo, ImVec2(note_x - Px(6.0f) - Px(16.0f), center_y - Px(8.0f)), 16.0f, col::kTextDim, 1.8f);
            text_right = note_x - Px(6.0f) - Px(16.0f) - Px(14.0f);
          }
          float icon_x = row.Min.x + Px(48.0f);
          DrawIconSvg("note", "<path d='M9 18V5l11-2v13'/><circle cx='6' cy='18' r='3'/><circle cx='17' cy='16' r='3'/>",
                      ImVec2(icon_x, center_y - Px(11.0f)), 22.0f, dlc.on ? col::kTextMuted : col::kTextDim, 1.75f);
          float text_x = icon_x + Px(22.0f) + Px(14.0f);
          float text_width = std::max(Px(40.0f), text_right - text_x);
          float text_y = center_y - (LineHeight(kName) + LineHeight(kSmall)) * 0.5f;
          DrawLabel(kName, ImVec2(text_x, text_y), dlc.on ? col::kText : col::kTextSoft,
                    Ellipsize(kName, *dlc.name, text_width));
          std::string meta = SizeText(dlc.bytes, t_.decimal_separator);
          if (dlc.library) meta += std::string(" \xC2\xB7 ") + t_.dlc_in_folder;
          DrawLabel(kSmall, ImVec2(text_x, text_y + LineHeight(kName)), col::kTextDim, Ellipsize(kSmall, meta, text_width));
          if (copy_note) {
            DrawLabel(kSmall, ImVec2(text_x, text_y + LineHeight(kName) + LineHeight(kSmall)), col::kTextMuted,
                      Ellipsize(kSmall, copy_note, row.Max.x - Px(16.0f) - text_x));
          }
          ImGui::PopID();
          row_y = row.Max.y;
        }
        ImGui::PopID();
      }
      ImGui::SetCursorScreenPos(origin);
      ImGui::Dummy(ImVec2(row_width, row_y - origin.y));
      // Up and down through the rows; up from the first reaches the header; left (or B) back to Choose.
      ColumnNav(row_ids);
      // Where right from the folder's column comes back in: the row in focus, else the first.
      bool entry_shown = std::any_of(row_ids.begin(), row_ids.end(),
                                     [&](const std::array<ImGuiID, 2>& row) { return row[0] == dlc_list_entry_; });
      if (!entry_shown) dlc_list_entry_ = row_ids.empty() ? 0 : row_ids.front()[0];
      if (g.NavWindow == ImGui::GetCurrentWindow()) {
        if (std::any_of(row_ids.begin(), row_ids.end(), [&](const std::array<ImGuiID, 2>& row) { return row[0] == g.NavId; })) {
          dlc_list_entry_ = g.NavId;
        }
        back_focus_ = choose_id;
        if (g.NavMoveSubmitted && g.NavMoveDir == ImGuiDir_Left) {
          RequestFocus(choose_id);
          ImGui::NavMoveRequestCancel();
        }
      }
      EndScroll();
      // A package's switch, or a group's (each of its packages set the same way).
      std::vector<std::pair<const DlcRow*, bool>> changes;
      if (switched) changes.push_back({switched, switched_on});
      if (group_switched) {
        for (const DlcRow& r : group_switched->rows) {
          if (r.on != group_switched_on) changes.push_back({&r, group_switched_on});
        }
      }
      bool relist = false;
      for (const auto& [row, value] : changes) {
        if (row->library) {
          // The choice kept and the game's list written at once: a few lines.
          SetLibraryDlcOn(exe_dir_, *dlc_library_, row->index, value);
        } else {
          // A rename, at once; the list read again shows where it is (a failed move, the game running, leaves it).
          InstalledDlc& d = (*dlc_installed_)[row->index];
          if (SetDlcOn(exe_dir_, d.folder, value)) {
            d.on = value;
          }
          relist = true;
        }
      }
      if (relist) {
        ListDlc();
      }
      if (!removed.empty()) {
        RemoveDlc(exe_dir_, removed);
        dlc_confirm_.clear();
        ListDlc();
        ReadDlcLibrary();  // a copy removed: its package of the folder takes its place
      }
    }
  }

  // Y: the folder's whole text, and what the game does with a package that is not for it.
  if (!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) && ButtonPressed(PadButton::kY)) {
    const char* foreign = game::kDlcRule.foreign == game::DlcForeign::kDamaged       ? t_.dlc_foreign_damaged
                          : game::kDlcRule.foreign == game::DlcForeign::kDisablesAll ? t_.dlc_foreign_all
                                                                                    : t_.dlc_foreign_unused;
    std::vector<std::string> bullets;
    if (const char* where = game::GameTextsFor(language_).dlc_where) bullets.push_back(where);
    bullets.push_back(foreign);
    bullets.push_back(t_.dlc_drop_hint);
    OpenInfo(t_.more_info, t_.dlc_library_title, t_.dlc_library_text, std::move(bullets));
  }
  if (dlc_free_confirm_) {
    DlcFreeDialog();
  }
}

void App::DlcFreeDialog() {
  constexpr const char* kPopup = "##dlc_free";
  if (!dlc_free_opened_) {
    ImGui::OpenPopup(kPopup);
  }
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  ImVec2 origin = viewport->Pos;
  ImVec2 size = viewport->Size;
  ImGui::SetNextWindowPos(origin);
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 0));
  if (ImGui::BeginPopupModal(kPopup, nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoBackground)) {
    // Launcher 3.18: a flat veil over the header and the page; the footer stays, with the dialog's hints.
    const float footer_height = Snap(Px(52.0f));
    ImGui::GetWindowDrawList()->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y - footer_height),
                                              Hex(0x000000, 158));
    // The card: the question, what is deleted and where, what stays; Cancel (focused first, or B) and Delete.
    constexpr Type kTitle{Font::kDisplayMedium, 28, 0.015f};
    constexpr Type kText{Font::kRegular, 15};
    constexpr float kWidth = 600.0f;
    constexpr float kPadding = 28.0f;
    const float inner_width = Px(kWidth - kPadding * 2.0f);
    size_t count = dlc_library_ ? dlc_library_->Copies() : 0;
    std::string size_text = SizeText(dlc_library_ ? dlc_library_->CopyBytes() : 0, t_.decimal_separator);
    std::string where = Utf8(DlcFolder(exe_dir_).parent_path().wstring());
    char text[1024];
    std::snprintf(text, sizeof(text), t_.dlc_free_text, int(count), size_text.c_str(), where.c_str());
    std::string title = t_.dlc_free_title;
    const float title_height = DrawParagraph(kTitle, ImVec2(0, 0), inner_width, 1.15f, 0, title, false);
    const float text_height = DrawParagraph(kText, ImVec2(0, 0), inner_width, 1.5f, 0, text, false);
    KeyButtonSpec cancel_spec{KeyLook::kGhost, 'B', Type{Font::kSemiBold, 16}, 48, 22, 12, 22, 10};
    KeyButtonSpec delete_spec{KeyLook::kDanger, 0, Type{Font::kSemiBold, 16}, 48, 22, 12, 22, 10};
    float height = Px(kPadding) + title_height + Px(12.0f) + text_height + Px(28.0f) + Px(48.0f) + Px(kPadding);
    float width = Px(kWidth);
    float space = size.y - footer_height;
    ImRect card(Snap(ImVec2(origin.x + (size.x - width) * 0.5f, origin.y + (space - height) * 0.5f)),
                Snap(ImVec2(origin.x + (size.x + width) * 0.5f, origin.y + (space + height) * 0.5f)));
    FillRect(card, col::kCard, 18.0f);
    StrokeRect(card, col::kHairline, 18.0f);
    float x = card.Min.x + Px(kPadding);
    float y = card.Min.y + Px(kPadding);
    DrawParagraph(kTitle, ImVec2(x, y), inner_width, 1.15f, col::kText, title);
    y += title_height + Px(12.0f);
    DrawParagraph(kText, ImVec2(x, y), inner_width, 1.5f, col::kTextMuted, text);
    y += text_height + Px(28.0f);
    float right = card.Max.x - Px(kPadding);
    float delete_width = KeyButtonWidth(delete_spec, t_.dlc_free_delete);
    float cancel_width = KeyButtonWidth(cancel_spec, t_.cancel);
    if (!dlc_free_opened_) {
      ImGui::SetKeyboardFocusHere();
    }
    bool cancel = KeyButton("##dlc_free_cancel", ImVec2(right - delete_width - Px(12.0f) - cancel_width, y), cancel_spec,
                            t_.cancel)
                      .pressed;
    bool confirm = KeyButton("##dlc_free_delete", ImVec2(right - delete_width, y), delete_spec, t_.dlc_free_delete).pressed;
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
      cancel = true;
    }
    dlc_free_opened_ = true;
    {
      std::vector<Hint> hints = {{HintKey::kB, t_.cancel}};
      Footer(ImRect(ImVec2(origin.x, origin.y + size.y - footer_height), ImVec2(origin.x + size.x, origin.y + size.y)), hints);
    }
    if (confirm && dlc_library_ && !dlc_freeing_.valid()) {
      Log("DLC: deleting %d extracted copies (%s), confirmed", int(count), size_text.c_str());
      dlc_freed_text_.clear();
      dlc_freeing_ = std::async(std::launch::async, RemoveDlcCopies, exe_dir_, *dlc_library_);
    }
    if (confirm || cancel) {
      dlc_free_confirm_ = false;
      dlc_free_opened_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(3);
}

}  // namespace launcher
