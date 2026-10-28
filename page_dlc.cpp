// The DLC tab (dlc.h; Gustavo's 2026-10-28 mockup): a card to install the packages of a folder the player chooses (or
// drops on the window), searched and installed at once, and the content installed, each with a button to remove it.

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>

#include <SDL3/SDL_dialog.h>

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

void App::ScanDlc(const std::string& path) {
  if (dlc_scan_.valid() || dlc_install_.Get().state == DlcInstall::State::kRunning) {
    return;
  }
  dlc_scanned_ = path;
  dlc_found_.reset();
  dlc_install_.Reset();
  dlc_scan_ = std::async(std::launch::async, FindDlcPackages, std::filesystem::path(Wide(path)), exe_dir_);
}

void App::ListDlc() {
  dlc_listed_ = true;
  if (!dlc_listing_.valid()) {
    dlc_listing_ = std::async(std::launch::async, ListInstalledDlc, exe_dir_);
  }
}

void App::PollDlc() {
  {
    std::optional<std::string> chosen;
    {
      std::lock_guard lock(dlc_dialog_->mutex);
      chosen.swap(dlc_dialog_->path);
    }
    if (chosen) {
      ScanDlc(*chosen);
    }
  }
  // The search ended: what it found that is not installed yet is installed straight away.
  if (dlc_scan_.valid() && dlc_scan_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    dlc_found_ = dlc_scan_.get();
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
  }
  // An installation ended: what is installed now, the new ones marked.
  if (dlc_refresh_after_install_ && dlc_install_.Get().state == DlcInstall::State::kDone) {
    dlc_refresh_after_install_ = false;
    ListDlc();
    std::filesystem::path folder = DlcFolder(exe_dir_);
    std::error_code ec;
    for (const std::string& name : dlc_installing_names_) {
      if (std::filesystem::is_directory(folder / std::filesystem::path(Wide(name)), ec)) {
        dlc_new_.insert(name);
      }
    }
    if (dlc_found_) {
      for (DlcPackage& package : *dlc_found_) {
        package.installed = std::filesystem::is_directory(folder / package.path.filename(), ec);
      }
    }
  }
}

void App::DlcPage(const ImRect& rect) {
  if (!dlc_listed_) {
    ListDlc();
  }
  ImRect main(ImVec2(rect.Min.x + Px(40.0f), rect.Min.y + Px(24.0f)), ImVec2(rect.Max.x - Px(40.0f), rect.Max.y - Px(20.0f)));
  constexpr Type kTitle{Font::kDisplayBold, 40};
  constexpr Type kSubtitle{Font::kRegular, 15};
  DrawLabel(kTitle, main.Min, col::kText, Upper(t_.dlc_title), Px(40.0f));
  DrawLabel(kSubtitle, ImVec2(main.Min.x, main.Min.y + Px(40.0f) + Px(8.0f)), col::kTextSoft, t_.dlc_subtitle);
  float top = main.Min.y + Px(40.0f) + Px(8.0f) + LineHeight(kSubtitle) + Px(18.0f);
  ImRect install(ImVec2(main.Min.x, top), ImVec2(main.Min.x + Px(520.0f), main.Max.y));
  ImRect installed(ImVec2(install.Max.x + Px(20.0f), top), main.Max);
  DlcInstall::Progress progress = dlc_install_.Get();
  bool running = progress.state == DlcInstall::State::kRunning;
  bool busy = running || dlc_scan_.valid();
  constexpr Type kHeading{Font::kDisplayBold, 24};

  // Installing: the folder chosen (or dropped on the window) is searched, and what it holds installed.
  {
    FillRect(install, col::kPanel, 14.0f);
    StrokeRect(install, Hex(0xFFFFFF, 26), 14.0f);
    float x = install.Min.x + Px(24.0f);
    float width = install.GetWidth() - Px(48.0f);
    float y = install.Min.y + Px(24.0f);
    ImRect icon(ImVec2(x, y), ImVec2(x + Px(52.0f), y + Px(52.0f)));
    FillRect(icon, col::kAccentTint, 12.0f);
    DrawIcon(Icon::kPackage, ImVec2(icon.Min.x + Px(13.0f), icon.Min.y + Px(13.0f)), 26.0f, col::kAccent, 1.8f);
    DrawLabel(kHeading, ImVec2(icon.Max.x + Px(14.0f), y), col::kText, Upper(t_.dlc_install_title), Px(52.0f));
    y += Px(52.0f) + Px(16.0f);
    constexpr Type kText{Font::kRegular, 15};
    y += DrawParagraph(kText, ImVec2(x, y), width, 1.55f, col::kTextSoft, t_.dlc_install_text) + Px(16.0f);

    // The buttons at the foot, the drop zone between.
    float buttons_y = install.Max.y - Px(24.0f) - Px(52.0f);
    ImRect zone(ImVec2(x, y), ImVec2(x + width, buttons_y - Px(16.0f)));
    bool done = progress.state == DlcInstall::State::kDone || (dlc_found_ && !busy);
    ImU32 border = Hex(0xFFFFFF, 51);
    ImU32 fill = Hex(0xFFFFFF, 8);
    Icon zone_icon = Icon::kUpload;
    ImU32 icon_color = col::kTabText;
    std::string title = t_.dlc_drop_here;
    std::string sub = t_.dlc_drop_or;
    ImU32 sub_color = col::kTabText;
    if (busy) {
      border = Fade(col::kAccent, 0.6f);
      fill = Fade(col::kAccent, 0.08f);
      zone_icon = Icon::kNone;
      if (running) {
        char text[512];
        std::snprintf(text, sizeof(text), t_.dlc_installing_short, progress.current.c_str(), int(progress.index + 1),
                      int(progress.count));
        title = text;
      } else {
        title = t_.dlc_searching_short;
      }
      sub = dlc_scanned_;
    } else if (done) {
      bool failed = progress.state == DlcInstall::State::kDone && !progress.failed.empty();
      border = failed ? Fade(col::kError, 0.6f) : Fade(col::kSuccess, 0.6f);
      fill = failed ? Fade(col::kError, 0.08f) : Fade(col::kSuccess, 0.08f);
      zone_icon = failed ? Icon::kAlert : Icon::kCheck;
      icon_color = failed ? col::kError : col::kSuccess;
      if (progress.state == DlcInstall::State::kDone && progress.installed > 0) {
        char text[96];
        std::snprintf(text, sizeof(text), progress.installed == 1 ? t_.dlc_installed_one : t_.dlc_installed_many,
                      int(progress.installed));
        title = text;
        sub = t_.dlc_listed_hint;
      } else if (dlc_found_ && dlc_found_->empty()) {
        title = t_.dlc_none_found_short;
        sub = dlc_scanned_;
        zone_icon = Icon::kAlert;
        icon_color = col::kTabText;
        border = Hex(0xFFFFFF, 51);
        fill = Hex(0xFFFFFF, 8);
      } else if (progress.state != DlcInstall::State::kDone) {
        title = t_.dlc_all_installed;
        sub = dlc_scanned_;
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
    FillRect(zone, fill, 12.0f);
    // "2px dashed".
    DashedRect(zone, border, 12.0f, 6.0f, 4.0f);
    DashedRect(ImRect(ImVec2(zone.Min.x + Px(1.0f), zone.Min.y + Px(1.0f)), ImVec2(zone.Max.x - Px(1.0f), zone.Max.y - Px(1.0f))),
               border, 11.0f, 6.0f, 4.0f);
    constexpr Type kZoneTitle{Font::kBold, 15};
    constexpr Type kZoneSub{Font::kRegular, 13};
    float block = Px(30.0f) + Px(10.0f) + LineHeight(kZoneTitle) + Px(10.0f) + LineHeight(kZoneSub);
    float zy = zone.GetCenter().y - block * 0.5f;
    float cx = zone.GetCenter().x;
    if (busy) {
      // A spinning arc.
      ImDrawList* draw = ImGui::GetWindowDrawList();
      ImVec2 c(cx, zy + Px(15.0f));
      float start = float(ImGui::GetTime()) * 5.0f;
      draw->PathArcTo(c, Px(11.0f), start, start + 4.7f, 32);
      draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(2.4f));
    } else {
      DrawIcon(zone_icon, ImVec2(cx - Px(15.0f), zy), 30.0f, icon_color, zone_icon == Icon::kCheck ? 2.6f : 1.6f);
    }
    zy += Px(30.0f) + Px(10.0f);
    std::string shown_title = Ellipsize(kZoneTitle, title, zone.GetWidth() - Px(32.0f));
    DrawLabel(kZoneTitle, ImVec2(cx - MeasureText(kZoneTitle, shown_title).x * 0.5f, zy), col::kText, shown_title);
    zy += LineHeight(kZoneTitle) + Px(10.0f);
    std::string shown_sub = Ellipsize(kZoneSub, sub, zone.GetWidth() - Px(32.0f));
    DrawLabel(kZoneSub, ImVec2(cx - MeasureText(kZoneSub, shown_sub).x * 0.5f, zy), sub_color, shown_sub);
    if (running && progress.total) {
      float bar_y = zone.Max.y - Px(20.0f);
      Progress(ImRect(ImVec2(zone.Min.x + Px(24.0f), bar_y), ImVec2(zone.Max.x - Px(24.0f), bar_y + Px(6.0f))),
               float(double(progress.done) / double(progress.total)), col::kPanelTrack, col::kAccent);
    }

    // Choose a folder… (A), and the DLC folder for copying by hand.
    constexpr Type kChoose{Font::kDisplayBold, 18, 0.05f};
    std::string choose_label = Upper(t_.dlc_choose_folder);
    float glyph = GlyphWidth(PadButton::kA, 24.0f);
    float choose_width = Px(22.0f) * 2.0f + glyph + Px(10.0f) + MeasureText(kChoose, choose_label).x;
    ImRect choose(ImVec2(x, buttons_y), ImVec2(x + choose_width, buttons_y + Px(52.0f)));
    ItemState state = Item("##dlc_choose", choose, !busy);
    FillRect(choose, Fade(state.hovered ? Brighten(col::kAccent) : col::kAccent, busy ? 0.6f : 1.0f), 10.0f);
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
      std::filesystem::path folder = DlcFolder(exe_dir_);
      std::error_code ec;
      std::filesystem::create_directories(folder, ec);
      OpenFolder(folder);
    }
    // A on the page chooses a folder (the footer's hint) wherever the focus is not on a button.
    if (!busy && !state.focused && !open_state.focused && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId) &&
        GImGui->NavId == 0 && ImGui::IsKeyPressed(ImGuiKey_GamepadFaceDown, false)) {
      BrowseDlc();
    }
  }

  // What is installed: the count, where the game shows it, and each package with Remove (pressed twice).
  {
    FillRect(installed, col::kPanel, 14.0f);
    StrokeRect(installed, col::kHairline, 14.0f);
    float x = installed.Min.x + Px(24.0f);
    float width = installed.GetWidth() - Px(48.0f);
    float y = installed.Min.y + Px(24.0f);
    const std::vector<InstalledDlc> none;
    const std::vector<InstalledDlc>& list = dlc_installed_ ? *dlc_installed_ : none;
    DrawLabel(kHeading, ImVec2(x, y), col::kText, Upper(t_.dlc_installed_title));
    char count[64];
    std::snprintf(count, sizeof(count), list.size() == 1 ? t_.dlc_count_one : t_.dlc_count, int(list.size()));
    constexpr Type kCount{Font::kRegular, 14};
    DrawLabel(kCount, ImVec2(x + width - MeasureText(kCount, count).x, y + Px(24.0f) * 1.193f - Px(14.0f) * 1.024f),
              col::kTabText, count);
    y += LineHeight(kHeading) + Px(14.0f);
    if (const char* where = game::GameTextsFor(language_).dlc_where) {
      constexpr Type kWhere{Font::kRegular, 14};
      float text_height = DrawParagraph(kWhere, ImVec2(0, 0), width - Px(28.0f) - Px(28.0f), 1.45f, 0, where, false);
      ImRect note(ImVec2(x, y), ImVec2(x + width, y + text_height + Px(24.0f)));
      FillRect(note, Fade(col::kGlyphX, 0.1f), 10.0f);
      DrawIcon(Icon::kInfo, ImVec2(note.Min.x + Px(14.0f), note.Min.y + Px(13.0f)), 18.0f, col::kGlyphX, 2.0f);
      DrawParagraph(kWhere, ImVec2(note.Min.x + Px(14.0f) + Px(28.0f), note.Min.y + Px(12.0f)), width - Px(28.0f) - Px(28.0f),
                    1.45f, col::kTextSoft, where);
      y = note.Max.y + Px(14.0f);
    }
    ImRect area(ImVec2(x, y), ImVec2(x + width, installed.Max.y - Px(24.0f)));
    if (list.empty()) {
      constexpr Type kEmptyTitle{Font::kBold, 16};
      constexpr Type kEmptyText{Font::kRegular, 14};
      const char* title = dlc_listing_.valid() ? "…" : t_.dlc_empty_title;
      float cy = area.GetCenter().y - (LineHeight(kEmptyTitle) + Px(6.0f) + LineHeight(kEmptyText)) * 0.5f;
      DrawLabel(kEmptyTitle, ImVec2(area.GetCenter().x - MeasureText(kEmptyTitle, title).x * 0.5f, cy), col::kTextSoft, title);
      if (!dlc_listing_.valid()) {
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
      constexpr Type kName{Font::kBold, 16};
      constexpr Type kMeta{Font::kRegular, 13};
      std::string removed;
      for (size_t i = 0; i < list.size(); ++i) {
        const InstalledDlc& dlc = list[i];
        bool is_new = dlc_new_.count(dlc.folder) != 0;
        float height = Px(12.0f) * 2.0f + Px(44.0f);
        ImRect row(ImVec2(origin.x, row_y), ImVec2(origin.x + row_width, row_y + height));
        FillRect(row, is_new ? Fade(col::kAccent, 0.08f) : Hex(0xFFFFFF, 10), 10.0f);
        StrokeRect(row, col::kHairline, 10.0f);
        ImRect tile(ImVec2(row.Min.x + Px(14.0f), row.Min.y + Px(12.0f)), ImVec2(row.Min.x + Px(58.0f), row.Max.y - Px(12.0f)));
        FillRect(tile, col::kAccentTint, 8.0f);
        // A music note.
        DrawIconSvg("note", "<path d='M9 18V5l11-2v13'/><circle cx='6' cy='18' r='3'/><circle cx='17' cy='16' r='3'/>",
                    ImVec2(tile.Min.x + Px(11.0f), tile.Min.y + Px(11.0f)), 22.0f, col::kAccent, 1.8f);
        bool confirming = dlc_confirm_ == dlc.folder;
        constexpr Type kRemove{Font::kBold, 14};
        const char* label = confirming ? t_.dlc_remove_confirm : t_.dlc_remove;
        float remove_width = Px(14.0f) * 2.0f + Px(16.0f) + Px(8.0f) + MeasureText(kRemove, label).x;
        ImRect remove(ImVec2(row.Max.x - Px(12.0f) - remove_width, row.GetCenter().y - Px(20.0f)),
                      ImVec2(row.Max.x - Px(12.0f), row.GetCenter().y + Px(20.0f)));
        ImGui::PushID(int(i));
        ItemState state = Item("##remove", remove, !running);
        ImGui::PopID();
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
            removed = dlc.folder;
          } else {
            dlc_confirm_ = dlc.folder;
          }
        }
        float right = remove.Min.x - Px(14.0f);
        if (is_new) {
          constexpr Type kNew{Font::kBold, 11, 0.06f};
          std::string tag = Upper(t_.dlc_new_tag);
          float tag_width = MeasureText(kNew, tag).x + Px(16.0f);
          ImRect badge(ImVec2(right - tag_width, row.GetCenter().y - Px(10.0f)), ImVec2(right, row.GetCenter().y + Px(10.0f)));
          FillRect(badge, col::kAccent, 4.0f);
          DrawLabel(kNew, ImVec2(badge.Min.x + Px(8.0f), badge.Min.y), col::kOnAccent, tag, badge.GetHeight());
          right = badge.Min.x - Px(14.0f);
        }
        float text_x = tile.Max.x + Px(14.0f);
        float text_width = right - text_x;
        float text_y = row.GetCenter().y - (LineHeight(kName) + Px(2.0f) + LineHeight(kMeta)) * 0.5f;
        DrawLabel(kName, ImVec2(text_x, text_y), col::kText, Ellipsize(kName, dlc.display_name, text_width));
        DrawLabel(kMeta, ImVec2(text_x, text_y + LineHeight(kName) + Px(2.0f)), col::kTabText,
                  SizeText(dlc.bytes, t_.decimal_separator));
        row_y += height + Px(8.0f);
      }
      ImGui::SetCursorScreenPos(origin);
      ImGui::Dummy(ImVec2(row_width, row_y - origin.y));
      EndScroll();
      if (!removed.empty()) {
        RemoveDlc(exe_dir_, removed);
        dlc_confirm_.clear();
        dlc_new_.erase(removed);
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
