// The first run (Gustavo's 2026-10-28 mockup): Welcome, Choose your ISO (and its invalid variant), Extracting, Ready,
// each in a card over the art with the steps in the header. The launcher opens on them while the game's files are
// missing (GameInstalled); the ISO is checked and extracted by install.cpp, off the UI thread.

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

constexpr Type kKicker{Font::kBold, 13, 0.22f};
constexpr Type kLead{Font::kRegular, 16};

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
  // The art, half darkened (the mockup's "oscurecer: medio"); clear on the last step.
  Background(size, install_step_ == InstallStep::kDone ? 0.0f : 0.55f);

  // The card: 600 dp wide, 56 dp from the left, 32 dp under the header and over the footer; padded 32 36.
  float header_height = Snap(Px(72.0f));
  float footer_height = Snap(Px(52.0f));
  ImRect card(ImVec2(origin.x + Px(56.0f), origin.y + header_height + Px(32.0f)),
              ImVec2(origin.x + Px(56.0f) + std::min(Px(600.0f), size.x - Px(112.0f)),
                     origin.y + size.y - footer_height - Px(32.0f)));
  FillRect(card, col::kPanel, 16.0f);
  StrokeRect(card, Hex(0xFFFFFF, 26), 16.0f);
  float x = card.Min.x + Px(36.0f);
  float width = card.GetWidth() - Px(72.0f);
  float y = card.Min.y + Px(32.0f);
  float buttons_y = card.Max.y - Px(32.0f) - Px(60.0f);

  bool popup = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
  bool back_pressed = !popup && ButtonPressed(PadButton::kB);
  bool x_pressed = !popup && ButtonPressed(PadButton::kX);
  if (!popup && ButtonPressed(PadButton::kY)) {
    Donate();
  }
  // The step's main button gets the focus when the step appears (the controller starts there).
  auto default_focus = [this] {
    ImGui::SetItemDefaultFocus();
    if (step_changed_) {
      ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
      step_changed_ = false;
    }
  };
  // The kicker, the title in capitals and the lead under it.
  auto heading = [&](const std::string& kicker, const char* title, const std::string& lead, float title_size,
                     ImU32 title_color, float lead_gap) {
    DrawLabel(kKicker, ImVec2(x, y), col::kAccent, Upper(kicker));
    y += LineHeight(kKicker) + Px(8.0f);
    y += DrawParagraph(Type{Font::kDisplayBold, title_size}, ImVec2(x, y), width, 1.0f, title_color, Upper(title));
    y += Px(lead_gap);
    y += DrawParagraph(kLead, ImVec2(x, y), width, 1.55f, col::kTextSoft, lead);
  };
  auto step_of = [&](int step) {
    char text[64];
    std::snprintf(text, sizeof(text), t_.step_of, step, 4);
    return std::string(text);
  };
  KeyButtonSpec primary{KeyLook::kPrimary, 'A', Type{Font::kDisplayBold, 22, 0.06f}, 60, 34, 10, 28, 12};
  KeyButtonSpec ghost{KeyLook::kGhost, 0, Type{Font::kSemiBold, 16}, 60, 24};

  switch (install_step_) {
    case InstallStep::kWelcome: {
      // The game's own welcome (game.cpp), or the launcher's.
      const game::GameTexts& own = game::GameTextsFor(language_);
      heading(t_.first_run_kicker, own.welcome_title ? own.welcome_title : t_.welcome_title,
              own.welcome_text ? own.welcome_text : t_.welcome_text, 52.0f, col::kText, 16.0f);
      // What it needs: the ISO, where it goes, the space. In a short window (the mockup's 760 dp already is one) the rows
      // close up so that the note keeps clear of the buttons.
      std::string lines[3][2] = {
          {t_.need_iso, Format(t_.need_iso_hint, game::kName)},
          {t_.install_here, Format(t_.install_here_hint, game::kRecompExecutable)},
          {t_.need_space, Format(t_.need_space_hint, Gigabytes(game::kInstalledBytes), Gigabytes(FreeSpace(exe_dir_)))}};
      const Icon icons[] = {Icon::kDisc, Icon::kFolder, Icon::kDrive};
      bool enough = FreeSpace(exe_dir_) >= game::kInstalledBytes;
      constexpr Type kTitle{Font::kBold, 15};
      constexpr Type kText{Font::kRegular, 14};
      constexpr Type kNote{Font::kRegular, 13};
      float text_block = std::max(Px(40.0f), LineHeight(kTitle) + Px(2.0f) + LineHeight(kText));
      float note_height = DrawParagraph(kNote, ImVec2(0, 0), width, 1.4f, 0, t_.own_copy, false);
      auto list_height = [&](float top, float padding, float gap) {
        return top + (padding * 2.0f + text_block) * 3.0f + gap * 3.0f + Px(6.0f) + note_height + Px(16.0f);
      };
      bool compact = y + list_height(Px(24.0f), Px(12.0f), Px(10.0f)) > buttons_y;
      float padding = compact ? Px(6.0f) : Px(12.0f);
      float gap = compact ? Px(6.0f) : Px(10.0f);
      y += compact ? Px(16.0f) : Px(24.0f);
      float height = padding * 2.0f + text_block;
      for (int i = 0; i < 3; ++i) {
        ImRect r(ImVec2(x, y), ImVec2(x + width, y + height));
        FillRect(r, Hex(0xFFFFFF, 13), 10.0f);
        ImRect tile(ImVec2(r.Min.x + Px(14.0f), r.GetCenter().y - Px(20.0f)),
                    ImVec2(r.Min.x + Px(54.0f), r.GetCenter().y + Px(20.0f)));
        FillRect(tile, col::kAccentTint, 8.0f);
        DrawIcon(icons[i], ImVec2(tile.Min.x + Px(9.0f), tile.Min.y + Px(9.0f)), 22.0f, col::kAccent, 1.8f);
        float tx = tile.Max.x + Px(14.0f);
        float ty = r.GetCenter().y - (LineHeight(kTitle) + Px(2.0f) + LineHeight(kText)) * 0.5f;
        DrawLabel(kTitle, ImVec2(tx, ty), col::kText, lines[i][0]);
        DrawLabel(kText, ImVec2(tx, ty + LineHeight(kTitle) + Px(2.0f)), i == 2 && !enough ? col::kError : col::kTabText,
                  Ellipsize(kText, lines[i][1], r.Max.x - Px(14.0f) - tx));
        y += height + gap;
      }
      DrawParagraph(kNote, ImVec2(x, y + Px(6.0f)), width, 1.4f, col::kLabel, t_.own_copy);

      std::string start = Upper(t_.start);
      if (KeyButton("##start", ImVec2(x, buttons_y), primary, start).pressed) {
        SetStep(InstallStep::kChooseIso);
      }
      default_focus();
      if (KeyButton("##quit", ImVec2(x + KeyButtonWidth(primary, start) + Px(14.0f), buttons_y), ghost, t_.quit).pressed ||
          back_pressed) {
        result_ = Result::kQuit;
      }
      break;
    }

    case InstallStep::kChooseIso: {
      bool checking = iso_check_.valid();
      bool valid = !checking && iso_ && iso_->result == IsoCheck::Result::kValid;
      bool invalid = !checking && iso_ && !valid;
      heading(step_of(2), t_.choose_title, Format(t_.choose_text, game::kName), 52.0f, col::kText, 14.0f);
      y += Px(22.0f);
      KeyButtonSpec browse{KeyLook::kGhost, 'X', Type{Font::kBold, 14}, 40, 16, 8, 22, 8};
      bool browse_pressed = x_pressed;

      if (invalid) {
        // Why, the file and what was found in it; another try.
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
        constexpr Type kErrorTitle{Font::kDisplayBold, 22};
        constexpr Type kErrorText{Font::kRegular, 15};
        constexpr Type kTerm{Font::kRegular, 14};
        constexpr Type kValue{Font::kBold, 14};
        float tx = x + Px(20.0f) + Px(40.0f) + Px(14.0f);
        float text_width = x + width - Px(20.0f) - tx;
        float title_height = DrawParagraph(kErrorTitle, ImVec2(0, 0), text_width, 1.2f, 0, Upper(error_title), false);
        float text_height = DrawParagraph(kErrorText, ImVec2(0, 0), text_width, 1.45f, 0, error_text, false);
        float height = Px(18.0f) * 2.0f + title_height + Px(4.0f) + text_height + Px(14.0f) + LineHeight(kTerm) * 2.0f +
                       Px(6.0f);
        ImRect alert(ImVec2(x, y), ImVec2(x + width, y + height));
        FillRect(alert, Fade(Hex(0xB8332B), 0.16f), 12.0f);
        StrokeRect(alert, Fade(col::kError, 0.55f), 12.0f);
        ImVec2 badge(x + Px(20.0f) + Px(20.0f), alert.Min.y + Px(18.0f) + Px(20.0f));
        ImGui::GetWindowDrawList()->AddCircleFilled(badge, Px(20.0f), Hex(0xB8332B), 32);
        DrawIcon(Icon::kClose, ImVec2(badge.x - Px(10.0f), badge.y - Px(10.0f)), 20.0f, Hex(0xFFFFFF), 2.6f);
        float ty = alert.Min.y + Px(18.0f);
        ty += DrawParagraph(kErrorTitle, ImVec2(tx, ty), text_width, 1.2f, col::kText, Upper(error_title)) + Px(4.0f);
        ty += DrawParagraph(kErrorText, ImVec2(tx, ty), text_width, 1.45f, Hex(0xE3C9C4), error_text) + Px(14.0f);
        float value_x = tx + Px(130.0f) + Px(12.0f);
        DrawLabel(kTerm, ImVec2(tx, ty), col::kTabText, t_.file_label);
        DrawLabel(kValue, ImVec2(value_x, ty), col::kText,
                  Ellipsize(kValue, Utf8(iso_->path.filename()), alert.Max.x - Px(20.0f) - value_x));
        ty += LineHeight(kTerm) + Px(6.0f);
        DrawLabel(kTerm, ImVec2(tx, ty), col::kTabText, t_.detected_label);
        DrawLabel(kValue, ImVec2(value_x, ty), col::kText, Ellipsize(kValue, detected, alert.Max.x - Px(20.0f) - value_x));
        y = alert.Max.y + Px(14.0f);
        KeyButtonSpec other{KeyLook::kGhost, 'X', Type{Font::kBold, 15}, 44, 18, 8, 22, 8};
        if (KeyButton("##other", ImVec2(x, y), other, t_.choose_other).pressed) {
          browse_pressed = true;
        }
        default_focus();
        DrawLabel(Type{Font::kRegular, 14}, ImVec2(x + KeyButtonWidth(other, t_.choose_other) + Px(12.0f), y),
                  col::kTabText, t_.or_drop, Px(44.0f));
      } else {
        // The drop zone, with its button for the file dialog.
        ImRect zone(ImVec2(x, y), ImVec2(x + width, y + Px(150.0f)));
        FillRect(zone, Hex(0xFFFFFF, 8), 12.0f);
        DashedRect(zone, Hex(0xFFFFFF, 51), 12.0f, 6.0f, 4.0f);
        constexpr Type kDrop{Font::kBold, 15};
        float content = Px(30.0f) + Px(10.0f) + LineHeight(kDrop) + Px(10.0f) + Px(40.0f);
        float zy = zone.GetCenter().y - content * 0.5f;
        DrawIcon(Icon::kUpload, ImVec2(zone.GetCenter().x - Px(15.0f), zy), 30.0f, col::kTabText, 1.6f);
        zy += Px(30.0f) + Px(10.0f);
        DrawLabel(kDrop, ImVec2(zone.GetCenter().x - MeasureText(kDrop, t_.drop_here).x * 0.5f, zy), col::kText,
                  t_.drop_here);
        zy += LineHeight(kDrop) + Px(10.0f);
        if (KeyButton("##browse", ImVec2(zone.GetCenter().x - KeyButtonWidth(browse, t_.browse_file) * 0.5f, zy), browse,
                      t_.browse_file)
                .pressed) {
          browse_pressed = true;
        }
        if (!iso_) {
          default_focus();
        }
        y = zone.Max.y;
        if (checking || iso_) {
          // The file: being checked, or valid.
          y += Px(12.0f);
          constexpr Type kName{Font::kBold, 15};
          constexpr Type kSub{Font::kRegular, 13};
          constexpr Type kStatus{Font::kBold, 14};
          float height = Px(14.0f) * 2.0f + std::max(Px(42.0f), LineHeight(kName) + Px(2.0f) + LineHeight(kSub));
          ImRect r(ImVec2(x, y), ImVec2(x + width, y + height));
          FillRect(r, Hex(0xFFFFFF, 13), 10.0f);
          StrokeRect(r, valid ? Fade(col::kSuccess, 0.45f) : col::kHairline, 10.0f);
          ImRect tile(ImVec2(r.Min.x + Px(16.0f), r.GetCenter().y - Px(21.0f)),
                      ImVec2(r.Min.x + Px(58.0f), r.GetCenter().y + Px(21.0f)));
          FillRect(tile, Hex(0xFFFFFF, 20), 8.0f);
          DrawIcon(Icon::kDisc, ImVec2(tile.Min.x + Px(10.0f), tile.Min.y + Px(10.0f)), 22.0f, col::kTextSoft, 1.8f);
          const char* status = checking ? t_.checking_iso : t_.iso_valid;
          float status_width = MeasureText(kStatus, status).x + (valid ? Px(22.0f) : 0.0f);
          float tx = tile.Max.x + Px(14.0f);
          float text_width = r.Max.x - Px(16.0f) - status_width - Px(14.0f) - tx;
          std::string name = checking ? iso_pending_ : Utf8(iso_->path.filename());
          if (checking) {
            DrawLabel(kName, ImVec2(tx, r.Min.y), col::kText, Ellipsize(kName, name, text_width), r.GetHeight());
          } else {
            float ty = r.GetCenter().y - (LineHeight(kName) + Px(2.0f) + LineHeight(kSub)) * 0.5f;
            DrawLabel(kName, ImVec2(tx, ty), col::kText, Ellipsize(kName, name, text_width));
            std::string sub = Format(t_.iso_detail, game::kName, game::kRegion) + " · " + Gigabytes(iso_->file_size) + " GB";
            DrawLabel(kSub, ImVec2(tx, ty + LineHeight(kName) + Px(2.0f)), col::kTabText, Ellipsize(kSub, sub, text_width));
          }
          float sx = r.Max.x - Px(16.0f) - status_width;
          if (valid) {
            DrawIcon(Icon::kCheck, ImVec2(sx, r.GetCenter().y - Px(8.0f)), 16.0f, col::kSuccess, 2.6f);
            sx += Px(22.0f);
          }
          DrawLabel(kStatus, ImVec2(sx, r.Min.y), checking ? col::kAccentLight : col::kSuccess, status, r.GetHeight());
          y = r.Max.y;
        }
        if (!install_error_.empty()) {
          DrawParagraph(Type{Font::kRegular, 14}, ImVec2(x, y + Px(12.0f)), width, 1.4f, col::kError, install_error_);
        }
      }
      if (browse_pressed) {
        BrowseIso();
      }

      std::string extract = Upper(t_.extract);
      KeyButtonSpec extract_spec = primary;
      extract_spec.padding = 30;
      if (KeyButton("##extract", ImVec2(x, buttons_y), extract_spec, extract, valid).pressed) {
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
      if (KeyButton("##back", ImVec2(x + KeyButtonWidth(extract_spec, extract) + Px(14.0f), buttons_y), ghost, t_.back)
              .pressed ||
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
      heading(step_of(3), title, text, 52.0f, failed ? col::kError : col::kText, 14.0f);
      y += Px(28.0f);

      // The percentage, and the time left on its baseline.
      constexpr Type kPercent{Font::kDisplayBold, 88};
      constexpr Type kTime{Font::kRegular, 14};
      float fraction = progress.total ? float(double(progress.done) / double(progress.total)) : 0.0f;
      int percent = done ? 100 : std::min(99, int(fraction * 100.0f));
      char percent_text[16];
      std::snprintf(percent_text, sizeof(percent_text), "%d%%", percent);
      float box = Px(88.0f) * 0.9f;
      DrawLabel(kPercent, ImVec2(x, y), failed ? col::kError : col::kAccent, percent_text, box);
      std::string left = "—";
      if (rate_ > 1.0 && progress.total > progress.done) {
        int seconds = int(double(progress.total - progress.done) / rate_ + 0.5);
        char mmss[16];
        std::snprintf(mmss, sizeof(mmss), "%02d:%02d", seconds / 60, seconds % 60);
        left = mmss;
      }
      std::string eta = done ? std::string(t_.finished) : Format(t_.time_left, left);
      // DrawLabel centers a line in its box: the percentage's baseline is the box's middle less half the font's
      // height plus its ascent (Oswald 1.482 and 1.193 em, Source Sans 3 1.424 and 1.024 em).
      float baseline = y + (box - Px(88.0f) * 1.482f) * 0.5f + Px(88.0f) * 1.193f;
      float eta_y = baseline - (LineHeight(kTime) - Px(14.0f) * 1.424f) * 0.5f - Px(14.0f) * 1.024f;
      DrawLabel(kTime, ImVec2(x + width - MeasureText(kTime, eta).x, eta_y), col::kTabText, eta);
      y += box + Px(14.0f);
      Progress(ImRect(ImVec2(x, y), ImVec2(x + width, y + Px(14.0f))), done ? 1.0f : fraction, Hex(0xFFFFFF, 26),
               failed ? col::kError : col::kAccent);
      y += Px(14.0f) + Px(18.0f);

      // The file being written.
      constexpr Type kLabelType{Font::kBold, 12, 0.1f};
      constexpr Type kFile{Font::kRegular, 14};
      ImRect file(ImVec2(x, y), ImVec2(x + width, y + Px(12.0f) * 2.0f + LineHeight(kLabelType) + Px(3.0f) +
                                                      LineHeight(kFile)));
      FillRect(file, Hex(0xFFFFFF, 13), 10.0f);
      DrawLabel(kLabelType, ImVec2(file.Min.x + Px(14.0f), file.Min.y + Px(12.0f)), col::kLabel, Upper(t_.current_file));
      DrawLabel(kFile, ImVec2(file.Min.x + Px(14.0f), file.Min.y + Px(12.0f) + LineHeight(kLabelType) + Px(3.0f)),
                col::kTextSoft,
                Ellipsize(kFile, done || progress.current.empty() ? "—" : progress.current, width - Px(28.0f)));
      y = file.Max.y + Px(14.0f);
      constexpr Type kVerified{Font::kRegular, 14};
      DrawIcon(Icon::kCheck, ImVec2(x, y + (LineHeight(kVerified) - Px(16.0f)) * 0.5f), 16.0f, col::kSuccess, 2.6f);
      DrawLabel(kVerified, ImVec2(x + Px(24.0f), y), col::kTabText, Format(t_.iso_verified, game::kName));

      if (done) {
        if (KeyButton("##continue", ImVec2(x, buttons_y), primary, Upper(t_.continue_label)).pressed) {
          SetStep(InstallStep::kDone);
        }
        default_focus();
        if (!achievements_requested_) {
          LoadAchievements();  // the game can write their list now
        }
      } else if (failed) {
        KeyButtonSpec back = ghost;
        back.key = 'B';
        if (KeyButton("##back", ImVec2(x, buttons_y), back, t_.back).pressed || back_pressed) {
          SetStep(InstallStep::kChooseIso);
        }
        default_focus();
      } else {
        KeyButtonSpec cancel{KeyLook::kDanger, 'B', Type{Font::kBold, 16}, 60, 24};
        if (KeyButton("##cancel", ImVec2(x, buttons_y), cancel, t_.cancel_install).pressed || back_pressed) {
          extraction_.Cancel();
          SetStep(InstallStep::kChooseIso);
        }
      }
      break;
    }

    case InstallStep::kDone: {
      // The game's own (game.cpp), or the launcher's.
      const game::GameTexts& own = game::GameTextsFor(language_);
      ImVec2 check(x + Px(32.0f), y + Px(32.0f));
      ImGui::GetWindowDrawList()->AddCircleFilled(check, Px(32.0f), col::kAccent, 48);
      DrawIcon(Icon::kCheck, ImVec2(check.x - Px(16.0f), check.y - Px(16.0f)), 32.0f, col::kOnAccent, 2.8f);
      y += Px(64.0f) + Px(22.0f);
      heading(t_.install_complete, own.done_title ? own.done_title : t_.done_title,
              own.done_text ? own.done_text : t_.done_text, 56.0f, col::kText, 16.0f);
      y += Px(16.0f);
      constexpr Type kNote{Font::kRegular, 14};
      float note = Px(12.0f) * 2.0f + DrawParagraph(kNote, ImVec2(0, 0), width - Px(28.0f), 1.45f, 0, t_.delete_iso, false);
      FillRect(ImRect(ImVec2(x, y), ImVec2(x + width, y + note)), Hex(0xFFFFFF, 13), 10.0f);
      DrawParagraph(kNote, ImVec2(x + Px(14.0f), y + Px(12.0f)), width - Px(28.0f), 1.45f, col::kTabText, t_.delete_iso);

      KeyButtonSpec play = primary;
      play.type = Type{Font::kDisplayBold, 24, 0.06f};
      play.padding = 40;
      std::string play_label = Upper(t_.play);
      if (KeyButton("##play", ImVec2(x, buttons_y), play, play_label).pressed) {
        // Without a profile yet, the launcher first asks for one (page_profile.cpp); then PLAY.
        if (profiles::Load().Current()) {
          result_ = Result::kPlay;
        } else {
          installing_ = false;
        }
      }
      default_focus();
      KeyButtonSpec review = ghost;
      review.key = 'X';
      review.padding = 22;
      if (KeyButton("##review", ImVec2(x + KeyButtonWidth(play, play_label) + Px(14.0f), buttons_y), review,
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
