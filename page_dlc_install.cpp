// The DLC tab in the extracting mode (game::kDlcMode kInstall; dlc.h), as before 3.16, in launcher 3.18's style (the
// folder mode's page, page_dlc.cpp): a column to install the packages of a folder the player chooses (or drops on the
// window), searched and installed at once by the game (its --install_content tool mode), and the content installed as
// rows, each with its switch and a button to remove it. BrowseDlc and ListDlc, shared with the folder mode, are in page_dlc.cpp.

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

#include "app.h"
#include "game.h"
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

}  // namespace

void App::SearchDlc(const std::string& path) {
  if (dlc_search_.valid() || dlc_install_.Get().state == DlcInstall::State::kRunning) {
    return;
  }
  dlc_searched_ = path;
  dlc_found_.reset();
  dlc_install_.Reset();
  dlc_search_ = std::async(std::launch::async, FindDlcPackages, std::filesystem::path(Wide(path)), exe_dir_);
}

void App::PollDlcInstall() {
  // A game that checks its packages (game::kDlcRule) has them checked when the launcher opens, not only when the page
  // is shown: a package it does not read is off before the player presses Play.
  if (!dlc_listed_ && (!game::kDlcRule.files.empty() || game::kDlcRule.manifest_prefix)) {
    ListDlc();
  }
  {
    std::optional<std::string> chosen;
    {
      std::lock_guard lock(dlc_dialog_->mutex);
      chosen.swap(dlc_dialog_->path);
    }
    if (chosen) {
      SearchDlc(*chosen);
    }
  }
  // The search ended: what it found that is not installed yet is installed straight away.
  if (dlc_search_.valid() && dlc_search_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    dlc_found_ = dlc_search_.get();
    std::vector<DlcPackage> to_install;
    for (const DlcPackage& package : *dlc_found_) {
      if (package.IsDlc() && !package.installed) {
        to_install.push_back(package);
      }
    }
    dlc_installing_names_.clear();
    if (!to_install.empty()) {
      for (const DlcPackage& package : to_install) {
        dlc_installing_names_.push_back(package.file_name);
      }
      dlc_install_.Start(exe_dir_, to_install);
      dlc_refresh_after_install_ = true;
    }
  }
  if (dlc_listing_.valid() && dlc_listing_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    dlc_installed_ = dlc_listing_.get();
    // Packages the game does not read, turned off by the listing (game::kDlcRule): the install's note counts the new ones.
    for (const InstalledDlc& dlc : *dlc_installed_) {
      if (dlc.turned_off_now) {
        dlc_turned_off_.insert(dlc.folder);
      }
    }
    if (dlc_relist_) {
      dlc_relist_ = false;
      ListDlc();
    }
  }
  // An installation ended: what is installed now, the new ones marked.
  if (dlc_refresh_after_install_ && dlc_install_.Get().state == DlcInstall::State::kDone) {
    dlc_refresh_after_install_ = false;
    ListDlc();
    std::filesystem::path folder = DlcFolder(exe_dir_);
    std::filesystem::path disabled = DisabledDlcFolder(exe_dir_);
    std::error_code ec;
    for (const std::string& name : dlc_installing_names_) {
      if (std::filesystem::is_directory(folder / std::filesystem::path(Wide(name)), ec)) {
        dlc_new_.insert(name);
      }
    }
    if (dlc_found_) {
      for (DlcPackage& package : *dlc_found_) {
        package.installed = std::filesystem::is_directory(folder / package.path.filename(), ec) ||
                            std::filesystem::is_directory(disabled / package.path.filename(), ec);
      }
    }
  }
}

void App::DlcInstallPage(const ImRect& rect) {
  if (!dlc_listed_) {
    ListDlc();
  }
  ImGuiContext& g = *GImGui;
  dlc_focus_kind_ = 0;
  // --- Launcher 3.18: as the folder mode's page (page_dlc.cpp): the title and its line, the installing column from 56,
  // 340 wide, a hairline at 432, the installed list from 472 to 56 from the right edge. No cards. ---
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
  DlcInstall::Progress progress = dlc_install_.Get();
  bool running = progress.state == DlcInstall::State::kRunning;
  bool busy = running || dlc_search_.valid();
  ImGuiID choose_id = 0;

  // Installing: the folder chosen (or dropped on the window) is searched, and what it holds installed.
  {
    const float x = page_left + Px(56.0f);
    const float width = Px(340.0f);
    float y = columns_top;
    DrawLabel(kHeading, ImVec2(x, y), col::kText, t_.dlc_install_title);
    y += LineHeight(kHeading) + Px(12.0f);
    constexpr Type kText{Font::kRegular, 15};
    y += DrawParagraph(kText, ImVec2(x, y), width, 1.5f, col::kTextMuted, t_.dlc_install_text) + Px(22.0f);

    // The state, in a dashed place: drop a folder; searching or installing (a turning arc, the bar); what was done.
    bool done = progress.state == DlcInstall::State::kDone || (dlc_found_ && !busy);
    Icon zone_icon = Icon::kUpload;
    ImU32 icon_color = col::kTextMuted;
    std::string title = t_.dlc_drop_here;
    std::string sub = t_.dlc_drop_or;
    ImU32 sub_color = col::kTextDim;
    if (busy) {
      zone_icon = Icon::kNone;
      if (running) {
        char text[512];
        std::snprintf(text, sizeof(text), t_.dlc_installing_short, progress.current.c_str(), int(progress.index + 1),
                      int(progress.count));
        title = text;
      } else {
        title = t_.dlc_searching_short;
      }
      sub = dlc_searched_;
    } else if (done) {
      bool failed = progress.state == DlcInstall::State::kDone && !progress.failed.empty();
      zone_icon = failed ? Icon::kAlert : Icon::kCheck;
      icon_color = failed ? col::kError : col::kSuccess;
      if (progress.state == DlcInstall::State::kDone && progress.installed > 0) {
        char text[96];
        std::snprintf(text, sizeof(text), progress.installed == 1 ? t_.dlc_installed_one : t_.dlc_installed_many,
                      int(progress.installed));
        title = text;
        sub = t_.dlc_listed_hint;
        // Those the game does not read were turned off (the listing after the install).
        int off = 0;
        for (const std::string& name : dlc_installing_names_) {
          off += dlc_turned_off_.count(name) ? 1 : 0;
        }
        if (off > 0) {
          char turned[256];
          std::snprintf(turned, sizeof(turned), off == 1 ? t_.dlc_turned_off_one : t_.dlc_turned_off_many, off);
          sub = turned;
        }
      } else if (dlc_found_ && dlc_found_->empty()) {
        title = t_.dlc_none_found_short;
        sub = dlc_searched_;
        zone_icon = Icon::kAlert;
        icon_color = col::kTextMuted;
      } else if (progress.state != DlcInstall::State::kDone) {
        title = t_.dlc_all_installed;
        sub = dlc_searched_;
      }
      if (failed) {
        std::string names;
        for (const std::string& name : progress.failed) {
          names += (names.empty() ? "" : ", ") + name;
        }
        char text[1024];
        std::snprintf(text, sizeof(text), t_.dlc_failed, names.c_str());
        sub = text;
        sub_color = col::kError;
      }
    }
    float buttons_y = foot - Px(40.0f) - Px(48.0f);
    ImRect zone(ImVec2(x, y), ImVec2(x + width, std::max(y + Px(120.0f), buttons_y - Px(24.0f))));
    DashedRect(zone, col::kOutline, 14.0f, 6.0f, 4.0f);
    constexpr Type kZoneTitle{Font::kSemiBold, 16};
    float block = Px(26.0f) + Px(10.0f) + LineHeight(kZoneTitle) + Px(4.0f) + LineHeight(kSmall);
    float zy = zone.GetCenter().y - block * 0.5f;
    float cx = zone.GetCenter().x;
    if (busy) {
      ImDrawList* draw = ImGui::GetWindowDrawList();
      ImVec2 c(cx, zy + Px(13.0f));
      float start = float(ImGui::GetTime()) * 5.0f;
      draw->PathArcTo(c, Px(10.0f), start, start + 4.7f, 32);
      draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(2.4f));
    } else {
      DrawIcon(zone_icon, ImVec2(cx - Px(13.0f), zy), 26.0f, icon_color, zone_icon == Icon::kCheck ? 2.2f : 1.75f);
    }
    zy += Px(26.0f) + Px(10.0f);
    std::string shown_title = Ellipsize(kZoneTitle, title, zone.GetWidth() - Px(32.0f));
    DrawLabel(kZoneTitle, ImVec2(cx - MeasureText(kZoneTitle, shown_title).x * 0.5f, zy), col::kText, shown_title);
    zy += LineHeight(kZoneTitle) + Px(4.0f);
    std::string shown_sub = Ellipsize(kSmall, sub, zone.GetWidth() - Px(32.0f));
    DrawLabel(kSmall, ImVec2(cx - MeasureText(kSmall, shown_sub).x * 0.5f, zy), sub_color, shown_sub);
    if (running && progress.total) {
      float bar_y = zone.Max.y - Px(22.0f);
      Progress(ImRect(ImVec2(zone.Min.x + Px(24.0f), bar_y), ImVec2(zone.Max.x - Px(24.0f), bar_y + Px(6.0f))),
               float(double(progress.done) / double(progress.total)), col::kChip, col::kAccent);
    }

    // Choose a folder… (the main action), and the DLC folder for copying by hand.
    ButtonSpec choose_spec{ButtonKind::kPrimary, Type{Font::kSemiBold, 16}, 48, 20, 12};
    if (Button("##dlc_choose", ImVec2(x, buttons_y), choose_spec, t_.dlc_choose_folder, !busy)) {
      BrowseDlc();
    }
    choose_id = ImGui::GetItemID();
    page_focus_ = choose_id;
    float link_x = x + ButtonWidth(choose_spec, t_.dlc_choose_folder) + Px(22.0f);
    if (Link("##dlc_open", ImVec2(link_x, buttons_y + (Px(48.0f) - LineHeight(Type{Font::kSemiBold, 15})) * 0.5f),
             t_.dlc_open_folder, 15.0f)) {
      std::filesystem::path folder = DlcFolder(exe_dir_);
      std::error_code ec;
      std::filesystem::create_directories(folder, ec);
      OpenFolder(folder);
    }
    // Right from the link enters the list (ImGui would not reach its rows, far above).
    if (dlc_list_entry_ && g.NavMoveSubmitted && g.NavMoveDir == ImGuiDir_Right && g.NavId == ImGui::GetItemID()) {
      RequestFocus(dlc_list_entry_);
      ImGui::NavMoveRequestCancel();
    }
  }

  // What is installed: the count, where the game shows it, and each package as a row: A (or a click) turns it on or
  // off, X removes it (pressed twice).
  {
    const float x = page_left + Px(472.0f);
    const float right = rect.Max.x - Px(56.0f);
    const float width = right - x;
    float y = columns_top;
    const std::vector<InstalledDlc> none;
    const std::vector<InstalledDlc>& list = dlc_installed_ ? *dlc_installed_ : none;
    DrawLabel(kHeading, ImVec2(x, y), col::kText, t_.dlc_installed_title);
    char count[64];
    std::snprintf(count, sizeof(count), list.size() == 1 ? t_.dlc_count_one : t_.dlc_count, int(list.size()));
    DrawLabel(kSmall, ImVec2(right - MeasureText(kSmall, count).x, y + Px(26.0f) * 1.193f - Px(14.0f) * 1.024f), col::kTextDim,
              count);
    y += LineHeight(kHeading) + Px(2.0f);
    if (const char* where = game::GameTextsFor(language_).dlc_where) {
      DrawLabel(kSmall, ImVec2(x, y), col::kTextDim, Ellipsize(kSmall, where, width));
      y += LineHeight(kSmall);
    }
    y += Px(18.0f);
    FillRect(ImRect(ImVec2(x, y), ImVec2(right, y + Snap(Px(1.0f)))), col::kHairline);
    ImRect area(ImVec2(x, y + Snap(Px(1.0f))), ImVec2(right + Px(16.0f), foot));
    if (list.empty()) {
      constexpr Type kEmptyTitle{Font::kSemiBold, 17};
      const char* title = dlc_listing_.valid() ? "\xE2\x80\xA6" : t_.dlc_empty_title;
      ImRect space(area.Min, ImVec2(right, area.Max.y));
      float cy = space.GetCenter().y - (LineHeight(kEmptyTitle) + Px(4.0f) + LineHeight(kSmall)) * 0.5f;
      DrawLabel(kEmptyTitle, ImVec2(space.GetCenter().x - MeasureText(kEmptyTitle, title).x * 0.5f, cy), col::kTextSoft,
                title);
      if (!dlc_listing_.valid()) {
        DrawLabel(kSmall, ImVec2(space.GetCenter().x - MeasureText(kSmall, t_.dlc_empty_text).x * 0.5f,
                                 cy + LineHeight(kEmptyTitle) + Px(4.0f)),
                  col::kTextDim, t_.dlc_empty_text);
      }
      dlc_list_entry_ = 0;
    } else {
      BeginScroll("##dlc_installed", area);
      ImVec2 origin = ImGui::GetCursorScreenPos();
      float row_y = origin.y;
      constexpr Type kName{Font::kSemiBold, 16};
      std::string removed;
      std::string switched;  // a package turned on or off this frame
      bool switched_on = false;
      std::vector<std::array<ImGuiID, 2>> row_ids;
      for (size_t i = 0; i < list.size(); ++i) {
        const InstalledDlc& dlc = list[i];
        bool is_new = dlc_new_.count(dlc.folder) != 0;
        ImRect row(ImVec2(origin.x, row_y), ImVec2(origin.x + width, row_y + Px(64.0f)));
        ImGui::PushID(int(i));
        ImGui::SetNextItemAllowOverlap();
        ItemState state = Item("##package", row, !running);
        if (!running) row_ids.push_back({ImGui::GetItemID(), ImGui::GetItemID()});
        if (state.focused) {
          FocusRow(row, 12.0f);
          dlc_focus_kind_ = 3;
        } else {
          if (state.hovered) FillRect(row, Fade(col::kFocus, 0.03f), 12.0f);
          FillRect(ImRect(ImVec2(row.Min.x, row.Max.y - Snap(Px(1.0f))), row.Max), col::kHairline);
        }
        float center_y = row.GetCenter().y;
        // On: in the game's content folder; off: moved beside it, out of the game's sight.
        float toggle_x = row.Max.x - Px(16.0f) - Px(52.0f);
        DrawToggle(ImVec2(toggle_x, center_y - Px(15.0f)), dlc.on, !running, false, state.hovered);
        if (state.pressed) {
          switched = dlc.folder;
          switched_on = !dlc.on;
        }
        float text_right = toggle_x - Px(14.0f);
        // Remove, pressed twice (X on the row, or a click).
        bool confirming = dlc_confirm_ == dlc.folder;
        const char* label = confirming ? t_.dlc_remove_confirm : t_.dlc_remove;
        ButtonSpec spec{ButtonKind::kDanger, Type{Font::kSemiBold, 14}, 34, 14, 10, Icon::kTrash, 16, 2.0f};
        float remove_width = ButtonWidth(spec, label);
        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
        bool pressed = Button("##remove", ImVec2(text_right - remove_width, center_y - Px(17.0f)), spec, label, !running);
        ImGui::PopItemFlag();
        if (pressed || (state.focused && ButtonPressed(PadButton::kX))) {
          if (confirming) {
            removed = dlc.folder;
          } else {
            dlc_confirm_ = dlc.folder;
          }
        }
        text_right -= remove_width + Px(14.0f);
        if (!dlc.fits) {
          float note_width = MeasureText(kSmall, t_.dlc_not_for_game).x;
          float note_x = text_right - note_width;
          DrawLabel(kSmall, ImVec2(note_x, center_y - LineHeight(kSmall) * 0.5f), col::kTextDim, t_.dlc_not_for_game);
          DrawIcon(Icon::kInfo, ImVec2(note_x - Px(6.0f) - Px(16.0f), center_y - Px(8.0f)), 16.0f, col::kTextDim, 1.8f);
          text_right = note_x - Px(6.0f) - Px(16.0f) - Px(14.0f);
        }
        if (is_new) {
          float chip_width = ChipWidth(t_.dlc_new_tag);
          DrawChip(ImVec2(text_right - chip_width, center_y), t_.dlc_new_tag);
          text_right -= chip_width + Px(14.0f);
        }
        float icon_x = row.Min.x + Px(16.0f);
        DrawIconSvg("note", "<path d='M9 18V5l11-2v13'/><circle cx='6' cy='18' r='3'/><circle cx='17' cy='16' r='3'/>",
                    ImVec2(icon_x, center_y - Px(11.0f)), 22.0f, dlc.on ? col::kTextMuted : col::kTextDim, 1.75f);
        float text_x = icon_x + Px(22.0f) + Px(14.0f);
        float text_width = std::max(Px(40.0f), text_right - text_x);
        float text_y = center_y - (LineHeight(kName) + LineHeight(kSmall)) * 0.5f;
        DrawLabel(kName, ImVec2(text_x, text_y), dlc.on ? col::kText : col::kTextSoft,
                  Ellipsize(kName, dlc.display_name, text_width));
        DrawLabel(kSmall, ImVec2(text_x, text_y + LineHeight(kName)), col::kTextDim, SizeText(dlc.bytes, t_.decimal_separator));
        ImGui::PopID();
        row_y = row.Max.y;
      }
      ImGui::SetCursorScreenPos(origin);
      ImGui::Dummy(ImVec2(width, row_y - origin.y));
      ColumnNav(row_ids);
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
      if (!switched.empty()) {
        // A rename, at once; the list read again shows where it is (a failed move, the game running, leaves it).
        if (SetDlcOn(exe_dir_, switched, switched_on)) {
          for (InstalledDlc& dlc : *dlc_installed_) {
            if (dlc.folder == switched) dlc.on = switched_on;
          }
        }
        ListDlc();
      }
      if (!removed.empty()) {
        RemoveDlc(exe_dir_, removed);
        dlc_confirm_.clear();
        dlc_new_.erase(removed);
        dlc_turned_off_.erase(removed);
        ListDlc();
        if (dlc_found_) {
          for (DlcPackage& package : *dlc_found_) {
            if (package.file_name == removed) {
              package.installed = false;
            }
          }
        }
      }
    }
  }
}

}  // namespace launcher
