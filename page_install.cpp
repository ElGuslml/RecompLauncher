// The first run (launcher 3.18, Gustavo's 2026-11-11 mockup "Camerino", boards 22 and 23): Welcome, Choose your ISO
// (and its checking, valid and invalid states), Extracting, Ready, each a column over the art (no card) with the steps
// in the header. The launcher opens on them while the game's files are missing (GameInstalled); the ISO is checked and
// extracted by install.cpp, off the UI thread.

#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "app.h"
#include "game.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

std::string Format(const char* format, const std::string& a, const std::string& b = {}) {
  char text[1024];
  std::snprintf(text, sizeof(text), format, a.c_str(), b.c_str());
  return text;
}

std::string Utf8(const std::filesystem::path& path) {
  std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

}  // namespace


std::string App::Gigabytes(uint64_t bytes) const {
  char text[32];
  std::snprintf(text, sizeof(text), "%.1f", double(bytes) / (1024.0 * 1024.0 * 1024.0));
  std::string out = text;
  std::replace(out.begin(), out.end(), '.', t_.decimal_separator[0]);
  return out;
}

void App::ChooseIso(const std::string& path) {
  if (iso_check_.valid()) {
    return;  // one check at a time
  }
  install_error_.clear();
  iso_.reset();
  std::filesystem::path file(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
  iso_pending_ = Utf8(file.filename());
  iso_check_ = std::async(std::launch::async, CheckIso, file);
}

void App::BrowseIso() {
  // SDL keeps the filters until the dialog answers.
  static SDL_DialogFileFilter filters[1];
  filters[0] = {t_.iso_filter, "iso"};
  // The dialog answers on a thread of its own, maybe after this App is gone: the callback holds the result
  // through a shared pointer of its own, and PollInstall takes the path on the UI thread.
  auto* holder = new std::shared_ptr<DialogResult>(dialog_);
  SDL_ShowOpenFileDialog(
      [](void* userdata, const char* const* files, int) {
        auto* result = static_cast<std::shared_ptr<DialogResult>*>(userdata);
        if (files && files[0]) {
          std::lock_guard lock((*result)->mutex);
          (*result)->path = files[0];
        }
        delete result;
      },
      holder, window_, filters, 1, nullptr, false);
}

void App::PollInstall() {
  {
    std::optional<std::string> chosen;
    {
      std::lock_guard lock(dialog_->mutex);
      chosen.swap(dialog_->path);
    }
    if (chosen && install_step_ == InstallStep::kChooseIso) {
      ChooseIso(*chosen);
    }
  }
  if (iso_check_.valid() && iso_check_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    iso_ = iso_check_.get();
    step_changed_ = true;  // the focus moves to Extract, or to Choose another file
  }
  // The extraction's speed, smoothed, for the time left.
  Extraction::Progress progress = extraction_.Get();
  if (progress.state == Extraction::State::kRunning) {
    auto now = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(now - rate_time_).count();
    if (rate_time_ == std::chrono::steady_clock::time_point{}) {
      rate_time_ = now;
      rate_done_ = progress.done;
    } else if (seconds >= 0.5) {
      double rate = double(progress.done - rate_done_) / seconds;
      rate_ = rate_ > 0.0 ? rate_ * 0.7 + rate * 0.3 : rate;
      rate_time_ = now;
      rate_done_ = progress.done;
    }
  }
}

void App::InstallScreen(ImVec2 size) {
  PollInstall();
  ImVec2 origin = ImGui::GetMainViewport()->Pos;
  // Launcher 3.18 (the mockup's boards 22 and 23): the art in full, the step's column at its left with no card.
  Background(size, 0.0f);

  // The column: 56 dp from the left, 500 wide; the title 24 dp under the header; the buttons 28 dp over the footer.
  const float header_height = Snap(Px(72.0f));
  const float footer_height = Snap(Px(52.0f));
  const float x = origin.x + Px(56.0f);
  const float width = std::min(Px(500.0f), size.x - Px(112.0f));
  const float page_top = origin.y + header_height;
  const float page_bottom = origin.y + size.y - footer_height;
  const float buttons_y = page_bottom - Px(32.0f) - Px(64.0f);
  float y = page_top + Px(24.0f);

  bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
  bool back_pressed = !popup && ButtonPressed(PadButton::kB);
  bool x_pressed = !popup && ButtonPressed(PadButton::kX);
  // The step's main button gets the focus when the step appears (the controller starts there).
  // (Through RequestFocus: it waits until A or Enter, still down on the button that changed the step, is up.)
  auto default_focus = [this] {
    ImGui::SetItemDefaultFocus();
    if (step_changed_) {
      RequestFocus(ImGui::GetItemID(), gamepad_used_ || SDL_HasGamepad());
      step_changed_ = false;
    }
  };
  // The title (Oswald 600 60, in capitals for a game that writes them so) and the line under it.
  constexpr Type kTitle{Font::kDisplaySemiBold, 60, 0.015f};
  constexpr Type kLead{Font::kRegular, 17};
  auto heading = [&](const char* title, const std::string& lead, ImU32 title_color) {
    y += DrawParagraph(kTitle, ImVec2(x, y), std::max(width, Px(520.0f)), 1.05f, title_color, Title(title));
    y += Px(16.0f);
    y += DrawParagraph(kLead, ImVec2(x, y), width, 1.55f, col::kTextSoft, lead);
  };
  // A fact's row (the mockup's ".fact"): its icon, a title and a line, a hairline under it.
  constexpr Type kFactTitle{Font::kSemiBold, 17};
  constexpr Type kFactText{Font::kRegular, 15};
  auto fact = [&](Icon icon, std::string_view title, std::string_view text, ImU32 text_color) {
    float text_x = x + Px(24.0f) + Px(16.0f);
    float text_width = x + width - text_x;
    float text_height = DrawParagraph(kFactText, ImVec2(0, 0), text_width, 1.4f, 0, text, false);
    float height = Px(14.0f) * 2.0f + (title.empty() ? 0.0f : LineHeight(kFactTitle)) + text_height;
    DrawIcon(icon, ImVec2(x, y + Px(14.0f) + Px(2.0f)), 24.0f, col::kTextDim, 1.75f);
    float ty = y + Px(14.0f);
    if (!title.empty()) {
      DrawLabel(kFactTitle, ImVec2(text_x, ty), col::kText, Ellipsize(kFactTitle, title, text_width));
      ty += LineHeight(kFactTitle);
    }
    DrawParagraph(kFactText, ImVec2(text_x, ty), text_width, 1.4f, text_color, text);
    y += height;
    FillRect(ImRect(ImVec2(x, y - Snap(Px(1.0f))), ImVec2(x + width, y)), col::kHairline);
  };
  auto hairline = [&]() { FillRect(ImRect(ImVec2(x, y), ImVec2(x + width, y + Snap(Px(1.0f)))), col::kHairline); };
  // The main action (Play's look), and the quieter buttons.
  KeyButtonSpec primary{KeyLook::kPrimary, 'A', Type{Font::kDisplaySemiBold, 28, 0.02f}, 64, 26, 14, 28, 14};
  ButtonSpec ghost{ButtonKind::kGhost, Type{Font::kSemiBold, 16}, 48, 24, 12};
  auto ghost_y = [&]() { return buttons_y + Px(8.0f); };

  switch (install_step_) {
    case InstallStep::kWelcome: {
      // The game's own welcome (game.cpp), or the launcher's.
      const game::GameTexts& own = game::GameTextsFor(language_);
      heading(own.welcome_title ? own.welcome_title : t_.welcome_title,
              own.welcome_text ? own.welcome_text : t_.welcome_text, col::kText);
      y += Px(22.0f);
      // What it needs: the ISO, where it goes, the space.
      bool enough = FreeSpace(exe_dir_) >= game::kInstalledBytes;
      hairline();
      fact(Icon::kDisc, t_.need_iso, Format(t_.need_iso_hint, game::kName), col::kTextMuted);
      fact(Icon::kFolder, t_.install_here, Format(t_.install_here_hint, game::kRecompExecutable), col::kTextMuted);
      fact(Icon::kDrive, t_.need_space,
           Format(t_.need_space_hint, Gigabytes(game::kInstalledBytes), Gigabytes(FreeSpace(exe_dir_))),
           enough ? col::kTextMuted : col::kError);
      constexpr Type kNote{Font::kRegular, 14};
      float note_height = DrawParagraph(kNote, ImVec2(0, 0), width, 1.4f, 0, t_.own_copy, false);
      DrawParagraph(kNote, ImVec2(x, std::max(y + Px(14.0f), buttons_y - Px(16.0f) - note_height)), width, 1.4f,
                    col::kTextDim, t_.own_copy);

      if (KeyButton("##start", ImVec2(x + Px(2.0f), buttons_y), primary, t_.start).pressed) {
        SetStep(InstallStep::kChooseIso);
      }
      default_focus();
      if (Button("##quit", ImVec2(x + Px(2.0f) + KeyButtonWidth(primary, t_.start) + Px(20.0f), ghost_y()), ghost, t_.quit) ||
          back_pressed) {
        AskQuit();
      }
      break;
    }

    case InstallStep::kChooseIso: {
      bool checking = iso_check_.valid();
      bool valid = !checking && iso_ && iso_->result == IsoCheck::Result::kValid;
      bool invalid = !checking && iso_ && !valid;
      heading(t_.choose_title, Format(t_.choose_text, game::kName), col::kText);
      y += Px(26.0f);
      bool browse_pressed = x_pressed;
      // Browse: the main action while there is no valid file; an outline once there is.
      KeyButtonSpec browse{valid ? KeyLook::kGhost : KeyLook::kPrimary, valid ? 'X' : 'A',
                           Type{Font::kDisplaySemiBold, 20, 0.02f}, 48, 22, 12, 24, 12};

      if (invalid) {
        // Why, the file and what was found in it; another try (the main action).
        const char* error_title = t_.wrong_game_title;
        std::string error_text = Format(t_.wrong_game_text, game::kName);
        std::string detected;
        switch (iso_->result) {
          case IsoCheck::Result::kOtherVersion: {
            error_title = t_.wrong_version_title;
            error_text = Format(t_.wrong_version_text, game::kName, game::kRegion);
            char crc[16];
            std::snprintf(crc, sizeof(crc), "%08X", iso_->crc);
            detected = Format("%s (CRC %s)", game::kName, crc);
            break;
          }
          case IsoCheck::Result::kOtherGame: {
            char id[32];
            std::snprintf(id, sizeof(id), "Title ID %08X", iso_->title_id);
            detected = id;
            break;
          }
          case IsoCheck::Result::kNoExecutable:
            detected = t_.detected_no_xex;
            break;
          case IsoCheck::Result::kNotXbox:
            detected = t_.detected_none;
            break;
          default:
            error_title = t_.unreadable_title;
            error_text = t_.unreadable_text;
            detected = t_.detected_none;
            break;
        }
        // The error between hairlines: an alert in the error's color, its title and text, the file and what it is.
        constexpr Type kErrorTitle{Font::kSemiBold, 18};
        constexpr Type kTerm{Font::kRegular, 14};
        constexpr Type kValue{Font::kSemiBold, 14};
        hairline();
        float tx = x + Px(24.0f) + Px(16.0f);
        float text_width = x + width - tx;
        float ty = y + Px(16.0f);
        DrawIcon(Icon::kAlert, ImVec2(x, ty + Px(1.0f)), 24.0f, col::kError, 1.9f);
        ty += DrawParagraph(kErrorTitle, ImVec2(tx, ty), text_width, 1.3f, col::kText, error_title) + Px(4.0f);
        ty += DrawParagraph(kFactText, ImVec2(tx, ty), text_width, 1.45f, col::kTextMuted, error_text) + Px(12.0f);
        float value_x = tx + Px(110.0f);
        DrawLabel(kTerm, ImVec2(tx, ty), col::kTextDim, t_.file_label);
        DrawLabel(kValue, ImVec2(value_x, ty), col::kText, Ellipsize(kValue, Utf8(iso_->path.filename()), x + width - value_x));
        ty += LineHeight(kTerm) + Px(4.0f);
        DrawLabel(kTerm, ImVec2(tx, ty), col::kTextDim, t_.detected_label);
        DrawLabel(kValue, ImVec2(value_x, ty), col::kText, Ellipsize(kValue, detected, x + width - value_x));
        y = ty + LineHeight(kTerm) + Px(16.0f);
        hairline();
        y += Px(20.0f);
        KeyButtonSpec other{KeyLook::kPrimary, 'A', Type{Font::kDisplaySemiBold, 20, 0.02f}, 48, 22, 12, 24, 12};
        if (KeyButton("##other", ImVec2(x + Px(2.0f), y), other, t_.choose_other).pressed) {
          browse_pressed = true;
        }
        default_focus();
        DrawLabel(Type{Font::kRegular, 14}, ImVec2(x + Px(2.0f) + KeyButtonWidth(other, t_.choose_other) + Px(16.0f), y),
                  col::kTextDim, t_.or_drop, Px(48.0f));
      } else {
        // The drop zone (dashed), with Browse in it.
        ImRect zone(ImVec2(x, y), ImVec2(x + width, y + Px(valid || checking ? 170.0f : 230.0f)));
        DashedRect(zone, col::kOutline, 16.0f, 6.0f, 4.0f);
        constexpr Type kDrop{Font::kSemiBold, 17};
        float content = Px(32.0f) + Px(12.0f) + LineHeight(kDrop) + Px(16.0f) + Px(48.0f);
        float zy = zone.GetCenter().y - content * 0.5f;
        DrawIcon(Icon::kUpload, ImVec2(zone.GetCenter().x - Px(16.0f), zy), 32.0f, col::kTextMuted, 1.75f);
        zy += Px(32.0f) + Px(12.0f);
        DrawLabel(kDrop, ImVec2(zone.GetCenter().x - MeasureText(kDrop, t_.drop_here).x * 0.5f, zy), col::kText, t_.drop_here);
        zy += LineHeight(kDrop) + Px(16.0f);
        if (KeyButton("##browse", ImVec2(zone.GetCenter().x - KeyButtonWidth(browse, t_.browse_file) * 0.5f, zy), browse,
                      t_.browse_file, !checking)
                .pressed) {
          browse_pressed = true;
        }
        if (!iso_ && !checking) {
          default_focus();
        }
        y = zone.Max.y + Px(18.0f);
        if (checking || iso_) {
          // The file: being checked (a turning arc), or valid (a check), as a line between hairlines.
          constexpr Type kName{Font::kSemiBold, 16};
          constexpr Type kSub{Font::kRegular, 14};
          hairline();
          float line_height = Px(14.0f) * 2.0f + LineHeight(kName) + Px(2.0f) + LineHeight(kSub);
          ImRect r(ImVec2(x, y), ImVec2(x + width, y + line_height));
          ImVec2 icon(r.Min.x, r.GetCenter().y - Px(11.0f));
          if (checking) {
            ImDrawList* draw = ImGui::GetWindowDrawList();
            ImVec2 c(icon.x + Px(11.0f), icon.y + Px(11.0f));
            float start = float(ImGui::GetTime()) * 5.0f;
            draw->PathArcTo(c, Px(8.0f), start, start + 4.7f, 32);
            draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(2.2f));
          } else {
            DrawIcon(Icon::kCheck, icon, 22.0f, col::kSuccess, 2.2f);
          }
          float tx = r.Min.x + Px(22.0f) + Px(14.0f);
          float text_width = r.Max.x - tx;
          std::string name = checking ? iso_pending_ : Utf8(iso_->path.filename());
          std::string sub = checking ? std::string(t_.checking_iso)
                                     : std::string(t_.iso_valid) + " \xC2\xB7 " + Format(t_.iso_detail, game::kName, game::kRegion) +
                                           " \xC2\xB7 " + Gigabytes(iso_->file_size) + " GB";
          float ty = r.Min.y + Px(14.0f);
          DrawLabel(kName, ImVec2(tx, ty), col::kText, Ellipsize(kName, name, text_width));
          DrawLabel(kSub, ImVec2(tx, ty + LineHeight(kName) + Px(2.0f)), checking ? col::kTextMuted : col::kTextDim,
                    Ellipsize(kSub, sub, text_width));
          y = r.Max.y;
          hairline();
        }
        if (!install_error_.empty()) {
          DrawParagraph(Type{Font::kRegular, 14}, ImVec2(x, y + Px(12.0f)), width, 1.4f, col::kError, install_error_);
        }
      }
      if (browse_pressed && !checking) {
        BrowseIso();
      }

      // Extract and install: the main action once the file is valid; grey before.
      KeyButtonSpec extract_spec = primary;
      extract_spec.type = Type{Font::kDisplaySemiBold, 22, 0.02f};
      extract_spec.height = 56;
      if (KeyButton("##extract", ImVec2(x + Px(2.0f), buttons_y + Px(8.0f)), extract_spec, t_.extract, valid).pressed) {
        uint64_t free = FreeSpace(exe_dir_);
        uint64_t needed = iso_->image.total_bytes + 64ull * 1024 * 1024;
        if (free && free < needed) {
          install_error_ = Format(t_.no_space, Gigabytes(needed), Gigabytes(free));
        } else {
          install_error_.clear();
          rate_ = 0.0;
          rate_time_ = {};
          extraction_.Start(iso_->path, iso_->image, exe_dir_ / "assets");
          SetStep(InstallStep::kExtracting);
        }
      }
      if (valid) {
        default_focus();
      }
      ButtonSpec back = ghost;
      back.height = 44;
      if (Button("##back", ImVec2(x + Px(2.0f) + KeyButtonWidth(extract_spec, t_.extract) + Px(20.0f), buttons_y + Px(14.0f)),
                 back, t_.back) ||
          back_pressed) {
        SetStep(InstallStep::kWelcome);
      }
      break;
    }

    case InstallStep::kExtracting: {
      Extraction::Progress progress = extraction_.Get();
      bool done = progress.state == Extraction::State::kDone;
      bool failed = progress.state == Extraction::State::kFailed;
      // While it runs, the game's own words (game.cpp) if it has them.
      const game::GameTexts& own = game::GameTextsFor(language_);
      const char* extracting_title = own.extracting_title ? own.extracting_title : t_.extracting_title;
      const char* extracting_text = own.extracting_text ? own.extracting_text : t_.extracting_text;
      const char* title = done ? t_.extracted_title : failed ? t_.extract_failed_title : extracting_title;
      std::string text = done     ? std::string(t_.extracted_text)
                         : failed ? Format(t_.extract_failed_text, progress.error_path, progress.error)
                                  : std::string(extracting_text);
      heading(title, text, failed ? col::kError : col::kText);
      y += Px(30.0f);

      // The percentage (live progress: the accent's bar under it), and the time left at its right.
      constexpr Type kPercent{Font::kDisplaySemiBold, 72};
      constexpr Type kTime{Font::kRegular, 15};
      float fraction = progress.total ? float(double(progress.done) / double(progress.total)) : 0.0f;
      int percent = done ? 100 : std::min(99, int(fraction * 100.0f));
      char percent_text[16];
      std::snprintf(percent_text, sizeof(percent_text), "%d %%", percent);
      float box = Px(72.0f);
      DrawLabel(kPercent, ImVec2(x, y), failed ? col::kError : col::kText, percent_text, box);
      std::string left = "\xE2\x80\x94";
      if (rate_ > 1.0 && progress.total > progress.done) {
        int seconds = int(double(progress.total - progress.done) / rate_ + 0.5);
        char mmss[16];
        std::snprintf(mmss, sizeof(mmss), "%02d:%02d", seconds / 60, seconds % 60);
        left = mmss;
      }
      std::string eta = done ? std::string(t_.finished) : Format(t_.time_left, left);
      float baseline = y + (box - Px(72.0f) * 1.482f) * 0.5f + Px(72.0f) * 1.193f;
      float eta_y = baseline - (LineHeight(kTime) - Px(15.0f) * 1.424f) * 0.5f - Px(15.0f) * 1.024f;
      DrawLabel(kTime, ImVec2(x + width - MeasureText(kTime, eta).x, eta_y), col::kTextMuted, eta);
      y += box + Px(12.0f);
      Progress(ImRect(ImVec2(x, y), ImVec2(x + width, y + Px(8.0f))), done ? 1.0f : fraction, col::kChip,
               failed ? col::kError : col::kAccent);
      y += Px(8.0f) + Px(18.0f);
      // The file being written, and that the ISO was the right one.
      hairline();
      fact(Icon::kFolder, t_.current_file, done || progress.current.empty() ? "\xE2\x80\x94" : progress.current,
           col::kTextMuted);
      fact(Icon::kCheck, "", Format(t_.iso_verified, game::kName), col::kTextMuted);

      if (done) {
        if (KeyButton("##continue", ImVec2(x + Px(2.0f), buttons_y), primary, t_.continue_label).pressed) {
          SetStep(InstallStep::kDone);
        }
        default_focus();
        if (!achievements_requested_) {
          LoadAchievements();  // the game can write their list now
        }
      } else if (failed) {
        if (Button("##back", ImVec2(x + Px(2.0f), ghost_y()), ghost, t_.back) || back_pressed) {
          SetStep(InstallStep::kChooseIso);
        }
        default_focus();
      } else {
        ButtonSpec cancel{ButtonKind::kDanger, Type{Font::kSemiBold, 16}, 48, 24, 12};
        if (Button("##cancel", ImVec2(x + Px(2.0f), ghost_y()), cancel, t_.cancel_install) || back_pressed) {
          extraction_.Cancel();
          SetStep(InstallStep::kChooseIso);
        }
      }
      break;
    }

    case InstallStep::kDone: {
      // The game's own (game.cpp), or the launcher's: a check, the title, its line; deleting the ISO as a fact.
      const game::GameTexts& own = game::GameTextsFor(language_);
      ImVec2 check(x + Px(28.0f), y + Px(28.0f));
      ImGui::GetWindowDrawList()->AddCircleFilled(check, Px(28.0f), Fade(col::kSuccess, 0.16f), 48);
      DrawIcon(Icon::kCheck, ImVec2(check.x - Px(15.0f), check.y - Px(15.0f)), 30.0f, col::kSuccess, 2.4f);
      y += Px(56.0f) + Px(20.0f);
      heading(own.done_title ? own.done_title : t_.done_title, own.done_text ? own.done_text : t_.done_text, col::kText);
      y += Px(22.0f);
      hairline();
      fact(Icon::kInfo, "", t_.delete_iso, col::kTextMuted);

      if (KeyButton("##play", ImVec2(x + Px(2.0f), buttons_y), primary, t_.nav_play).pressed) {
        // Without a profile yet, the launcher first asks for one (page_profile.cpp); then Play.
        if (profiles::Load().Current()) {
          result_ = Result::kPlay;
        } else {
          installing_ = false;
        }
      }
      default_focus();
      KeyButtonSpec review{KeyLook::kGhost, 'X', Type{Font::kSemiBold, 16}, 48, 22, 12, 22, 10};
      if (KeyButton("##review", ImVec2(x + Px(2.0f) + KeyButtonWidth(primary, t_.nav_play) + Px(20.0f), ghost_y()), review,
                    t_.review_settings)
              .pressed ||
          x_pressed) {
        installing_ = false;
        Navigate(Page::kVideo, false);
      }
      break;
    }
  }

  Chrome(size);
}

}  // namespace launcher
