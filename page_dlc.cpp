// The DLC tab (dlc.h; Gustavo's 2026-10-28 mockup): a card for the player's DLC folder, whose packages the game reads
// where they are (the folder dialog, or a folder dropped on the window), and the list of the game's DLC: the folder's
// packages and the ones extracted by launchers before 3.16, each with its switch (and Remove for the extracted).

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

void App::PollDlc() {
  // Read when the launcher opens, not only when the page is shown: the DLC folder's new packages are in the game's list,
  // and a package the game does not read (game::kDlcRule) is off before the player presses Play.
  if (game::kHasDlcPage && !dlc_listed_) {
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

void App::DlcPage(const ImRect& rect) {
  if (!dlc_listed_) {
    ListDlc();
    ReadDlcLibrary();
  }
  ImRect main(ImVec2(rect.Min.x + Px(40.0f), rect.Min.y + Px(24.0f)), ImVec2(rect.Max.x - Px(40.0f), rect.Max.y - Px(20.0f)));
  constexpr Type kTitle{Font::kDisplayBold, 40};
  constexpr Type kSubtitle{Font::kRegular, 15};
  DrawLabel(kTitle, main.Min, col::kText, Upper(t_.dlc_title), Px(40.0f));
  DrawLabel(kSubtitle, ImVec2(main.Min.x, main.Min.y + Px(40.0f) + Px(8.0f)), col::kTextSoft, t_.dlc_subtitle);
  float top = main.Min.y + Px(40.0f) + Px(8.0f) + LineHeight(kSubtitle) + Px(18.0f);
  ImRect folder_card(ImVec2(main.Min.x, top), ImVec2(main.Min.x + Px(520.0f), main.Max.y));
  ImRect list_card(ImVec2(folder_card.Max.x + Px(20.0f), top), main.Max);
  bool scanning = dlc_scan_.valid();
  bool has_folder = dlc_library_ && !dlc_library_->folder.empty();
  constexpr Type kHeading{Font::kDisplayBold, 24};

  // The DLC folder: chosen (or dropped on the window) and read; what it holds.
  {
    FillRect(folder_card, col::kPanel, 14.0f);
    StrokeRect(folder_card, Hex(0xFFFFFF, 26), 14.0f);
    float x = folder_card.Min.x + Px(24.0f);
    float width = folder_card.GetWidth() - Px(48.0f);
    float y = folder_card.Min.y + Px(24.0f);
    ImRect icon(ImVec2(x, y), ImVec2(x + Px(52.0f), y + Px(52.0f)));
    FillRect(icon, col::kAccentTint, 12.0f);
    DrawIcon(Icon::kPackage, ImVec2(icon.Min.x + Px(13.0f), icon.Min.y + Px(13.0f)), 26.0f, col::kAccent, 1.8f);
    DrawLabel(kHeading, ImVec2(icon.Max.x + Px(14.0f), y), col::kText, Upper(t_.dlc_library_title), Px(52.0f));
    y += Px(52.0f) + Px(16.0f);
    constexpr Type kText{Font::kRegular, 15};
    y += DrawParagraph(kText, ImVec2(x, y), width, 1.55f, col::kTextSoft, t_.dlc_library_text) + Px(16.0f);

    // The buttons at the foot, the folder's box between, and under it the extracted copies of the folder's packages:
    // how many and their size, with Remove copies (its dialog); or, once deleted, what was freed.
    float buttons_y = folder_card.Max.y - Px(24.0f) - Px(52.0f);
    constexpr Type kNotice{Font::kRegular, 14};
    size_t copy_count = dlc_library_ ? dlc_library_->Copies() : 0;
    bool freeing = dlc_freeing_.valid();
    std::string notice;
    if (freeing) {
      notice = t_.dlc_freeing;
    } else if (copy_count > 0 && !scanning) {
      char line[256];
      std::string size_text = SizeText(dlc_library_->CopyBytes(), t_.decimal_separator);
      if (copy_count == 1) {
        std::snprintf(line, sizeof(line), t_.dlc_copies_one, size_text.c_str());
      } else {
        std::snprintf(line, sizeof(line), t_.dlc_copies_many, int(copy_count), size_text.c_str());
      }
      notice = line;
    } else if (!dlc_freed_text_.empty()) {
      notice = dlc_freed_text_;
    }
    bool with_button = copy_count > 0 && !scanning && !freeing;
    KeyButtonSpec remove_spec;
    remove_spec.look = KeyLook::kDanger;
    remove_spec.height = 40;
    remove_spec.padding = 16;
    remove_spec.type = Type{Font::kSemiBold, 14};
    // The notice: its icon, the text beside it, Remove copies at its right; as tall as its text or the button.
    float remove_width = with_button ? KeyButtonWidth(remove_spec, t_.dlc_remove_copies) : 0.0f;
    float notice_text_width =
        width - Px(14.0f) - Px(28.0f) - Px(14.0f) - (with_button ? remove_width + Px(12.0f) : 0.0f);
    float notice_text_height =
        notice.empty() ? 0.0f : DrawParagraph(kNotice, ImVec2(0, 0), notice_text_width, 1.45f, 0, notice, false);
    float notice_height =
        notice.empty() ? 0.0f : std::max(notice_text_height, with_button ? Px(40.0f) : 0.0f) + Px(24.0f);

    // The folder's box. No folder chosen: the drop zone, its icon above two lines, down to the buttons. A folder chosen
    // (or being read): one row, the icon and then the folder and what it holds, as tall as that; the notice under it.
    constexpr Type kZoneTitle{Font::kBold, 15};
    constexpr Type kZoneSub{Font::kRegular, 13};
    bool compact = has_folder || scanning;
    float lines_height = LineHeight(kZoneTitle) + Px(4.0f) + LineHeight(kZoneSub);
    ImRect zone(ImVec2(x, y), ImVec2(x + width, y + Px(18.0f) + std::max(Px(30.0f), lines_height) + Px(18.0f)));
    if (!compact) {
      zone.Max.y = buttons_y - Px(16.0f) - (notice.empty() ? 0.0f : notice_height + Px(16.0f));
    }
    if (!notice.empty()) {
      ImRect box(ImVec2(x, zone.Max.y + Px(16.0f)), ImVec2(x + width, zone.Max.y + Px(16.0f) + notice_height));
      bool done = copy_count == 0 && !freeing;
      ImU32 tone = done ? col::kSuccess : col::kGlyphX;
      FillRect(box, Fade(tone, 0.1f), 10.0f);
      DrawIcon(done ? Icon::kCheck : Icon::kInfo, ImVec2(box.Min.x + Px(14.0f), box.GetCenter().y - Px(9.0f)), 18.0f, tone,
               2.0f);
      DrawParagraph(kNotice, ImVec2(box.Min.x + Px(14.0f) + Px(28.0f), box.GetCenter().y - notice_text_height * 0.5f),
                    notice_text_width, 1.45f, col::kTextSoft, notice);
      if (with_button &&
          KeyButton("##dlc_remove_copies", ImVec2(box.Max.x - Px(12.0f) - remove_width, box.GetCenter().y - Px(20.0f)),
                    remove_spec, t_.dlc_remove_copies)
              .pressed) {
        dlc_free_confirm_ = true;
      }
    }
    ImU32 border = Hex(0xFFFFFF, 51);
    ImU32 fill = Hex(0xFFFFFF, 8);
    Icon zone_icon = Icon::kUpload;
    ImU32 icon_color = col::kTabText;
    std::string title = t_.dlc_drop_here;
    std::string sub = t_.dlc_drop_or;
    ImU32 sub_color = col::kTabText;
    std::string folder_text = has_folder ? Utf8(dlc_library_->folder.wstring()) : std::string();
    if (scanning) {
      border = Fade(col::kAccent, 0.6f);
      fill = Fade(col::kAccent, 0.08f);
      zone_icon = Icon::kNone;
      title = t_.dlc_searching_short;
      sub = has_folder ? folder_text : Utf8(DlcLibraryFolder(exe_dir_).wstring());
    } else if (has_folder && !dlc_library_->available) {
      border = Fade(col::kError, 0.6f);
      fill = Fade(col::kError, 0.08f);
      zone_icon = Icon::kAlert;
      icon_color = col::kError;
      title = folder_text;
      sub = t_.dlc_library_unavailable;
      sub_color = col::kError;
    } else if (has_folder && dlc_library_->packages.empty()) {
      zone_icon = Icon::kAlert;
      title = folder_text;
      sub = t_.dlc_none_found_short;
    } else if (has_folder) {
      border = Fade(col::kSuccess, 0.6f);
      fill = Fade(col::kSuccess, 0.08f);
      zone_icon = Icon::kCheck;
      icon_color = col::kSuccess;
      title = folder_text;
      char found[128];
      std::snprintf(found, sizeof(found), t_.dlc_library_found, int(dlc_library_->packages.size()),
                    int(dlc_library_->Fitting()));
      sub = found;
    }
    FillRect(zone, fill, 12.0f);
    // "2px dashed".
    DashedRect(zone, border, 12.0f, 6.0f, 4.0f);
    DashedRect(ImRect(ImVec2(zone.Min.x + Px(1.0f), zone.Min.y + Px(1.0f)), ImVec2(zone.Max.x - Px(1.0f), zone.Max.y - Px(1.0f))),
               border, 11.0f, 6.0f, 4.0f);
    // The icon (or a spinning arc while the folder is read) in its 30 dp box at `icon_pos`.
    auto draw_icon = [&](ImVec2 icon_pos) {
      if (scanning) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImVec2 c(icon_pos.x + Px(15.0f), icon_pos.y + Px(15.0f));
        float start = float(ImGui::GetTime()) * 5.0f;
        draw->PathArcTo(c, Px(11.0f), start, start + 4.7f, 32);
        draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(2.4f));
      } else {
        DrawIcon(zone_icon, icon_pos, 30.0f, icon_color, zone_icon == Icon::kCheck ? 2.6f : 1.6f);
      }
    };
    if (compact) {
      float icon_x = zone.Min.x + Px(18.0f);
      draw_icon(ImVec2(icon_x, zone.GetCenter().y - Px(15.0f)));
      float text_x = icon_x + Px(30.0f) + Px(16.0f);
      float text_width = zone.Max.x - Px(18.0f) - text_x;
      float ty = zone.GetCenter().y - lines_height * 0.5f;
      DrawLabel(kZoneTitle, ImVec2(text_x, ty), col::kText, Ellipsize(kZoneTitle, title, text_width));
      DrawLabel(kZoneSub, ImVec2(text_x, ty + LineHeight(kZoneTitle) + Px(4.0f)), sub_color,
                Ellipsize(kZoneSub, sub, text_width));
    } else {
      float block = Px(30.0f) + Px(10.0f) + LineHeight(kZoneTitle) + Px(10.0f) + LineHeight(kZoneSub);
      float zy = zone.GetCenter().y - block * 0.5f;
      float cx = zone.GetCenter().x;
      draw_icon(ImVec2(cx - Px(15.0f), zy));
      zy += Px(30.0f) + Px(10.0f);
      std::string shown_title = Ellipsize(kZoneTitle, title, zone.GetWidth() - Px(32.0f));
      DrawLabel(kZoneTitle, ImVec2(cx - MeasureText(kZoneTitle, shown_title).x * 0.5f, zy), col::kText, shown_title);
      zy += LineHeight(kZoneTitle) + Px(10.0f);
      std::string shown_sub = Ellipsize(kZoneSub, sub, zone.GetWidth() - Px(32.0f));
      DrawLabel(kZoneSub, ImVec2(cx - MeasureText(kZoneSub, shown_sub).x * 0.5f, zy), sub_color, shown_sub);
    }

    // Choose a folder… (A), and the folder in Explorer (the DLC folder, else the content folder of the extracted).
    constexpr Type kChoose{Font::kDisplayBold, 18, 0.05f};
    std::string choose_label = Upper(has_folder ? t_.dlc_change_folder : t_.dlc_choose_folder);
    float glyph = GlyphWidth(PadButton::kA, 24.0f);
    float choose_width = Px(22.0f) * 2.0f + glyph + Px(10.0f) + MeasureText(kChoose, choose_label).x;
    ImRect choose(ImVec2(x, buttons_y), ImVec2(x + choose_width, buttons_y + Px(52.0f)));
    ItemState state = Item("##dlc_choose", choose, !scanning);
    FillRect(choose, Fade(state.hovered ? Brighten(col::kAccent) : col::kAccent, scanning ? 0.6f : 1.0f), 10.0f);
    DrawGlyph(PadButton::kA, ImVec2(choose.Min.x + Px(22.0f), choose.GetCenter().y - Px(12.0f)), 24.0f, true);
    DrawLabel(kChoose, ImVec2(choose.Min.x + Px(22.0f) + glyph + Px(10.0f), choose.Min.y), col::kOnAccent, choose_label,
              choose.GetHeight());
    if (state.focused) FocusRing(choose, 10.0f);
    if (state.pressed) BrowseDlc();
    page_focus_ = ImGui::GetItemID();
    constexpr Type kOpen{Font::kBold, 15};
    float open_width = Px(18.0f) * 2.0f + MeasureText(kOpen, t_.dlc_open_folder).x + Px(8.0f) + Px(16.0f);
    ImRect open(ImVec2(choose.Max.x + Px(12.0f), buttons_y), ImVec2(choose.Max.x + Px(12.0f) + open_width, buttons_y + Px(52.0f)));
    ItemState open_state = Item("##dlc_open", open);
    FillRect(open, open_state.hovered ? Brighten(col::kGhostFill) : col::kGhostFill, 10.0f);
    StrokeRect(open, col::kGhostBorder, 10.0f);
    DrawLabel(kOpen, ImVec2(open.Min.x + Px(18.0f), open.Min.y), col::kText, t_.dlc_open_folder, open.GetHeight());
    DrawIcon(Icon::kArrowRight, ImVec2(open.Max.x - Px(18.0f) - Px(16.0f), open.GetCenter().y - Px(8.0f)), 16.0f, col::kText,
             2.2f);
    if (open_state.focused) FocusRing(open, 10.0f);
    if (open_state.pressed) {
      if (has_folder && dlc_library_->available) {
        OpenFolder(dlc_library_->folder);
      } else {
        std::filesystem::path folder = DlcFolder(exe_dir_);
        std::error_code ec;
        std::filesystem::create_directories(folder, ec);
        OpenFolder(folder);
      }
    }
    // A on the page chooses a folder (the footer's hint) wherever the focus is not on a button.
    if (!scanning && !state.focused && !open_state.focused && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
        GImGui->NavId == 0 && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false)) {
      BrowseDlc();
    }
  }

  // The game's DLC, grouped by the game each package was made for (the extracted copies apart): each group folds open
  // or shut, with its count and a switch for all of it; in it, each package with its switch.
  {
    FillRect(list_card, col::kPanel, 14.0f);
    StrokeRect(list_card, col::kHairline, 14.0f);
    float x = list_card.Min.x + Px(24.0f);
    float width = list_card.GetWidth() - Px(48.0f);
    float y = list_card.Min.y + Px(24.0f);
    // A package of the folder with an extracted copy installed shows as its copy: the copy is what the game reads.
    std::set<std::string> copies;
    if (dlc_library_) {
      for (const LibraryDlc& p : dlc_library_->packages) {
        if (!p.copy_folder.empty()) copies.insert(p.copy_folder);
      }
    }
    std::vector<DlcGroup> groups;
    auto group_for = [&groups](uint64_t key) -> DlcGroup& {
      for (DlcGroup& g : groups) {
        if (g.key == key) return g;
      }
      groups.emplace_back();
      groups.back().key = key;
      return groups.back();
    };
    if (dlc_library_) {
      for (size_t i = 0; i < dlc_library_->packages.size(); ++i) {
        const LibraryDlc& p = dlc_library_->packages[i];
        if (!p.copy_folder.empty()) continue;
        DlcGroup& g = group_for(p.title_id);
        g.rows.push_back({true, i, &p.display_name, p.bytes, p.on, p.fits});
        if (!p.title_name.empty()) ++g.names[p.title_name];
      }
    }
    if (dlc_installed_) {
      for (size_t i = 0; i < dlc_installed_->size(); ++i) {
        const InstalledDlc& d = (*dlc_installed_)[i];
        DlcGroup& g = group_for(kInstalledGroup);
        g.installed = true;
        g.rows.push_back({false, i, &d.display_name, d.bytes, d.on, d.fits, copies.count(d.folder) != 0});
      }
    }
    size_t total = 0;
    for (DlcGroup& g : groups) {
      std::stable_sort(g.rows.begin(), g.rows.end(), [](const DlcRow& a, const DlcRow& b) { return *a.name < *b.name; });
      for (const DlcRow& r : g.rows) {
        g.on += r.on;
        g.fits = g.fits || r.fits;
      }
      total += g.rows.size();
      // Its game's name: the one its headers give most (they spell it several ways), else the title ID.
      if (g.installed) {
        g.name = t_.dlc_group_installed;
      } else if (!g.names.empty()) {
        g.name = std::max_element(g.names.begin(), g.names.end(),
                                  [](const auto& a, const auto& b) { return a.second < b.second; })
                     ->first;
      } else {
        char id[16];
        std::snprintf(id, sizeof(id), "%08X", uint32_t(g.key));
        g.name = id;
      }
    }
    // This game's own first, then the copies installed, then the games whose packages it reads, then the rest.
    auto rank = [](const DlcGroup& g) {
      if (!g.installed && uint32_t(g.key) == game::kTitleId) return 0;
      if (g.installed) return 1;
      return g.fits ? 2 : 3;
    };
    std::stable_sort(groups.begin(), groups.end(), [&rank](const DlcGroup& a, const DlcGroup& b) {
      return rank(a) != rank(b) ? rank(a) < rank(b) : a.name < b.name;
    });

    DrawLabel(kHeading, ImVec2(x, y), col::kText, Upper(t_.dlc_list_title));
    char count[64];
    std::snprintf(count, sizeof(count), total == 1 ? t_.dlc_count_one : t_.dlc_count, int(total));
    constexpr Type kCount{Font::kRegular, 14};
    DrawLabel(kCount, ImVec2(x + width - MeasureText(kCount, count).x, y + Px(24.0f) * 1.193f - Px(14.0f) * 1.024f),
              col::kTabText, count);
    y += LineHeight(kHeading) + Px(6.0f);
    // Where the game shows the songs: a line under the title.
    if (const char* where = game::GameTextsFor(language_).dlc_where) {
      constexpr Type kWhere{Font::kRegular, 14};
      y += DrawParagraph(kWhere, ImVec2(x, y), width, 1.4f, col::kTabText, where);
    }
    y += Px(14.0f);
    ImRect area(ImVec2(x, y), ImVec2(x + width, list_card.Max.y - Px(24.0f)));
    if (groups.empty()) {
      constexpr Type kEmptyTitle{Font::kBold, 16};
      constexpr Type kEmptyText{Font::kRegular, 14};
      bool reading = scanning || dlc_listing_.valid();
      const char* title = reading ? "…" : t_.dlc_empty_title;
      float cy = area.GetCenter().y - (LineHeight(kEmptyTitle) + Px(6.0f) + LineHeight(kEmptyText)) * 0.5f;
      DrawLabel(kEmptyTitle, ImVec2(area.GetCenter().x - MeasureText(kEmptyTitle, title).x * 0.5f, cy), col::kTextSoft, title);
      if (!reading) {
        DrawLabel(kEmptyText,
                  ImVec2(area.GetCenter().x - MeasureText(kEmptyText, t_.dlc_empty_text).x * 0.5f,
                         cy + LineHeight(kEmptyTitle) + Px(6.0f)),
                  col::kTabText, t_.dlc_empty_text);
      }
    } else {
      BeginScroll("##dlc_installed", area);
      ImVec2 origin = ImGui::GetCursorScreenPos();
      float row_width = area.GetWidth() - (ImGui::GetCurrentWindow()->ScrollbarY ? Px(12.0f) : 0.0f);
      float row_y = origin.y;
      constexpr Type kGroup{Font::kBold, 17};
      constexpr Type kName{Font::kBold, 16};
      constexpr Type kMeta{Font::kRegular, 13};
      constexpr Type kNote{Font::kRegular, 13};
      constexpr Type kRemove{Font::kBold, 14};
      std::string removed;
      const DlcRow* switched = nullptr;
      bool switched_on = false;
      const DlcGroup* group_switched = nullptr;
      bool group_switched_on = false;
      // A small list opens every group; a big one starts folded. The player's clicks are kept.
      bool open_by_default = total <= 12;
      for (size_t gi = 0; gi < groups.size(); ++gi) {
        const DlcGroup& group = groups[gi];
        ImGui::PushID(int(gi));
        auto known = dlc_group_open_.find(group.key);
        bool open = known != dlc_group_open_.end() ? known->second : open_by_default;
        // The group's header: the chevron, its game, how many are on, and the switch for all of it.
        float header_height = Px(56.0f);
        ImRect header(ImVec2(origin.x, row_y), ImVec2(origin.x + row_width, row_y + header_height));
        bool all_on = group.on == group.rows.size();
        ImVec2 group_toggle(header.Max.x - Px(12.0f) - Px(52.0f), header.GetCenter().y - Px(15.0f));
        ImRect fold(header.Min, ImVec2(group_toggle.x - Px(8.0f), header.Max.y));
        ItemState fold_state = Item("##fold", fold);
        FillRect(header, fold_state.hovered ? Hex(0xFFFFFF, 22) : Hex(0xFFFFFF, 14), 10.0f);
        StrokeRect(header, col::kHairline, 10.0f);
        if (fold_state.focused) FocusRing(header, 10.0f);
        if (fold_state.pressed) {
          dlc_group_open_[group.key] = !open;
        }
        DrawIcon(open ? Icon::kChevronDown : Icon::kChevronRight,
                 ImVec2(header.Min.x + Px(16.0f), header.GetCenter().y - Px(9.0f)), 18.0f, col::kText, 2.2f);
        char on_text[64];
        std::snprintf(on_text, sizeof(on_text), t_.dlc_group_on, int(group.on), int(group.rows.size()));
        float on_width = MeasureText(kMeta, on_text).x;
        float name_x = header.Min.x + Px(16.0f) + Px(18.0f) + Px(12.0f);
        float on_x = group_toggle.x - Px(16.0f) - on_width;
        DrawLabel(kGroup, ImVec2(name_x, header.Min.y), group.fits ? col::kText : col::kTextSoft,
                  Ellipsize(kGroup, group.name, on_x - Px(16.0f) - name_x), header.GetHeight());
        DrawLabel(kMeta, ImVec2(on_x, header.Min.y), col::kTabText, on_text, header.GetHeight());
        bool group_value = all_on;
        if (Toggle("##group_on", group_toggle, &group_value, !(scanning && !group.installed))) {
          group_switched = &group;
          group_switched_on = group_value;
        }
        row_y += header_height + Px(8.0f);
        if (!open) {
          ImGui::PopID();
          continue;
        }
        for (size_t i = 0; i < group.rows.size(); ++i) {
          const DlcRow& dlc = group.rows[i];
          // A package the game does not read: what the game does with it on (game::kDlcRule), below its name.
          const char* note = nullptr;
          if (!dlc.fits) {
            switch (game::kDlcRule.foreign) {
              case game::DlcForeign::kUnused: note = t_.dlc_foreign_unused; break;
              case game::DlcForeign::kDamaged: note = t_.dlc_foreign_damaged; break;
              case game::DlcForeign::kDisablesAll: note = t_.dlc_foreign_all; break;
            }
          }
          // An extracted copy of a package of the DLC folder: says so (Remove copies, left, deletes it).
          const char* copy_note = dlc.copy ? t_.dlc_copy_note : nullptr;
          float indent = Px(24.0f);
          float top_height = Px(12.0f) * 2.0f + Px(44.0f);
          float text_left = Px(14.0f) + Px(44.0f) + Px(14.0f);
          float item_width = row_width - indent;
          float note_width = item_width - text_left - Px(14.0f);
          float note_height = note ? DrawParagraph(kNote, ImVec2(0, 0), note_width, 1.4f, 0, note, false) + Px(12.0f) : 0.0f;
          float copy_height =
              copy_note ? DrawParagraph(kNote, ImVec2(0, 0), note_width, 1.4f, 0, copy_note, false) + Px(12.0f) : 0.0f;
          float height = top_height + note_height + copy_height;
          ImRect row(ImVec2(origin.x + indent, row_y), ImVec2(origin.x + row_width, row_y + height));
          float center_y = row.Min.y + top_height * 0.5f;
          FillRect(row, Hex(0xFFFFFF, 10), 10.0f);
          StrokeRect(row, col::kHairline, 10.0f);
          ImRect tile(ImVec2(row.Min.x + Px(14.0f), row.Min.y + Px(12.0f)),
                      ImVec2(row.Min.x + Px(58.0f), row.Min.y + top_height - Px(12.0f)));
          FillRect(tile, dlc.on ? col::kAccentTint : Hex(0xFFFFFF, 10), 8.0f);
          // A music note.
          DrawIconSvg("note", "<path d='M9 18V5l11-2v13'/><circle cx='6' cy='18' r='3'/><circle cx='17' cy='16' r='3'/>",
                      ImVec2(tile.Min.x + Px(11.0f), tile.Min.y + Px(11.0f)), 22.0f, dlc.on ? col::kAccent : col::kTabText,
                      1.8f);
          ImGui::PushID(int(i));
          float right = row.Max.x - Px(12.0f);
          // Remove: only an extracted package (the folder's are the player's files, never deleted), pressed twice.
          if (!dlc.library) {
            const std::string& folder = (*dlc_installed_)[dlc.index].folder;
            bool confirming = dlc_confirm_ == folder;
            const char* label = confirming ? t_.dlc_remove_confirm : t_.dlc_remove;
            float remove_width = Px(14.0f) * 2.0f + Px(16.0f) + Px(8.0f) + MeasureText(kRemove, label).x;
            ImRect remove(ImVec2(right - remove_width, center_y - Px(20.0f)), ImVec2(right, center_y + Px(20.0f)));
            ItemState state = Item("##remove", remove);
            ImU32 background = confirming ? col::kAccent : col::kGhostFill;
            if (state.hovered) background = Brighten(background);
            FillRect(remove, background, 8.0f);
            StrokeRect(remove, confirming ? col::kAccent : Hex(0xFFFFFF, 41), 8.0f);
            ImU32 ink = confirming ? col::kOnAccent : col::kText;
            DrawIcon(Icon::kTrash, ImVec2(remove.Min.x + Px(14.0f), remove.GetCenter().y - Px(8.0f)), 16.0f, ink, 2.0f);
            DrawLabel(kRemove, ImVec2(remove.Min.x + Px(14.0f) + Px(24.0f), remove.Min.y), ink, label, remove.GetHeight());
            if (state.focused) FocusRing(remove, 8.0f);
            if (state.pressed) {
              if (confirming) {
                removed = folder;
              } else {
                dlc_confirm_ = folder;
              }
            }
            right = remove.Min.x - Px(14.0f);
          }
          // On: the game reads it; off: kept out of the game's sight. The folder's wait while it is read.
          bool on = dlc.on;
          ImVec2 toggle_pos(right - Px(52.0f), center_y - Px(15.0f));
          if (Toggle("##on", toggle_pos, &on, !(dlc.library && scanning))) {
            switched = &dlc;
            switched_on = on;
          }
          ImGui::PopID();
          right = toggle_pos.x - Px(14.0f);
          float text_x = tile.Max.x + Px(14.0f);
          float text_width = right - text_x;
          float text_y = center_y - (LineHeight(kName) + Px(2.0f) + LineHeight(kMeta)) * 0.5f;
          DrawLabel(kName, ImVec2(text_x, text_y), dlc.on ? col::kText : col::kTextSoft,
                    Ellipsize(kName, *dlc.name, text_width));
          std::string meta = SizeText(dlc.bytes, t_.decimal_separator);
          if (dlc.library) meta += std::string(" · ") + t_.dlc_in_folder;
          DrawLabel(kMeta, ImVec2(text_x, text_y + LineHeight(kName) + Px(2.0f)), col::kTabText,
                    Ellipsize(kMeta, meta, text_width));
          if (note) {
            DrawParagraph(kNote, ImVec2(row.Min.x + text_left, row.Min.y + top_height - Px(4.0f)), note_width, 1.4f,
                          col::kError, note);
          }
          if (copy_note) {
            DrawParagraph(kNote, ImVec2(row.Min.x + text_left, row.Min.y + top_height + note_height - Px(4.0f)),
                          note_width, 1.4f, col::kTextSoft, copy_note);
          }
          row_y += height + Px(8.0f);
        }
        row_y += Px(6.0f);
        ImGui::PopID();
      }
      ImGui::SetCursorScreenPos(origin);
      ImGui::Dummy(ImVec2(row_width, row_y - origin.y));
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
    ImGui::GetWindowDrawList()->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                                              Fade(col::kBackground, 0.82f));
    // The card: the question, what is deleted and where, what stays; Cancel (focused first, or B) and Delete.
    constexpr Type kTitle{Font::kDisplayBold, 28};
    constexpr Type kText{Font::kRegular, 15};
    constexpr float kWidth = 600.0f;
    constexpr float kPadding = 28.0f;
    const float inner_width = Px(kWidth - kPadding * 2.0f);
    size_t count = dlc_library_ ? dlc_library_->Copies() : 0;
    std::string size_text = SizeText(dlc_library_ ? dlc_library_->CopyBytes() : 0, t_.decimal_separator);
    std::string where = Utf8(DlcFolder(exe_dir_).parent_path().wstring());
    char text[1024];
    std::snprintf(text, sizeof(text), t_.dlc_free_text, int(count), size_text.c_str(), where.c_str());
    std::string title = Title(t_.dlc_free_title);
    const float title_height = DrawParagraph(kTitle, ImVec2(0, 0), inner_width, 1.15f, 0, title, false);
    const float text_height = DrawParagraph(kText, ImVec2(0, 0), inner_width, 1.5f, 0, text, false);
    KeyButtonSpec cancel_spec;
    cancel_spec.look = KeyLook::kGhost;
    cancel_spec.key = 'B';
    cancel_spec.height = 50;
    KeyButtonSpec delete_spec;
    delete_spec.look = KeyLook::kDanger;
    delete_spec.height = 50;
    float height = Px(kPadding) + title_height + Px(12.0f) + text_height + Px(28.0f) + Px(50.0f) + Px(kPadding);
    float width = Px(kWidth);
    ImRect card(Snap(ImVec2(origin.x + (size.x - width) * 0.5f, origin.y + (size.y - height) * 0.5f)),
                Snap(ImVec2(origin.x + (size.x + width) * 0.5f, origin.y + (size.y + height) * 0.5f)));
    FillRect(card, col::kCard, 16.0f);
    StrokeRect(card, col::kOutline, 16.0f);
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
