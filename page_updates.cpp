// The Updates tab (Gustavo's 2026-10-28 mockup): the state with the installed version and the last check, the button
// that checks or installs the new version (with its progress), checking when the launcher opens, every version on
// GitHub, and the release notes (Markdown from GitHub, its headings as colored tags). The updater is updater.h.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>
#include <vector>

#include "app.h"
#include "game.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

// Markdown's inline marks taken out: **bold**, *italic*, __bold__, `code`, [text](url) -> text, ![image](url) ->
// nothing, HTML tags (<img ...>, which GitHub's editor inserts for an uploaded picture; <br>, <p>) -> nothing. A single
// "_" stays (file names have them).
std::string PlainInline(std::string_view text) {
  std::string out;
  for (size_t i = 0; i < text.size();) {
    char c = text[i];
    if (c == '<' && i + 1 < text.size() &&
        (std::isalpha(static_cast<unsigned char>(text[i + 1])) || text[i + 1] == '/' || text[i + 1] == '!')) {
      size_t close = text.find('>', i);
      if (close != std::string_view::npos) {
        i = close + 1;
        continue;
      }
    }
    if (c == '_' && i + 1 < text.size() && text[i + 1] == '_') {
      i += 2;
      continue;
    }
    if (c == '*') {
      ++i;
      continue;
    }
    if (c == '`') {
      ++i;
      continue;
    }
    if (c == '!' && i + 1 < text.size() && text[i + 1] == '[') {
      size_t close = text.find("](", i);
      size_t end = close == std::string_view::npos ? close : text.find(')', close);
      if (end != std::string_view::npos) {
        i = end + 1;
        continue;
      }
    }
    if (c == '[') {
      size_t close = text.find("](", i);
      size_t end = close == std::string_view::npos ? close : text.find(')', close);
      if (end != std::string_view::npos) {
        out.append(text.substr(i + 1, close - i - 1));
        i = end + 1;
        continue;
      }
    }
    out += c;
    ++i;
  }
  return out;
}

std::string_view Trim(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) text.remove_suffix(1);
  return text;
}

// "- item", "* item", "1. item": the item's text, or npos.
size_t ListMarker(std::string_view line) {
  if (line.size() >= 2 && (line[0] == '-' || line[0] == '*' || line[0] == '+') && line[1] == ' ') return 2;
  size_t digits = 0;
  while (digits < line.size() && digits < 3 && line[digits] >= '0' && line[digits] <= '9') ++digits;
  if (digits && digits + 1 < line.size() && (line[digits] == '.' || line[digits] == ')') && line[digits + 1] == ' ') {
    return digits + 2;
  }
  return std::string_view::npos;
}

// "2026-10-25T18:03:00Z" as the language's date.
std::string PublishedDate(const std::string& iso, uint32_t language) {
  std::tm utc = {};
  if (sscanf_s(iso.c_str(), "%d-%d-%dT%d:%d:%d", &utc.tm_year, &utc.tm_mon, &utc.tm_mday, &utc.tm_hour,
                  &utc.tm_min, &utc.tm_sec) != 6) {
    return iso.substr(0, 10);
  }
  utc.tm_year -= 1900;
  utc.tm_mon -= 1;
  std::time_t seconds = _mkgmtime(&utc);
  constexpr uint64_t kUnixEpoch = 116'444'736'000'000'000ULL;
  return FormatDate(uint64_t(seconds) * 10'000'000ULL + kUnixEpoch, language);
}

std::string Megabytes(uint64_t bytes) {
  char text[32];
  std::snprintf(text, sizeof(text), "%.1f", double(bytes) / (1024.0 * 1024.0));
  return text;
}

}  // namespace

std::string App::LastCheckedText() const {
  int64_t when = settings_.o.last_update_check;
  if (when <= 0) return t_.never;
  constexpr uint64_t kUnixEpoch = 116'444'736'000'000'000ULL;
  std::string date = FormatDate(uint64_t(when) * 10'000'000ULL + kUnixEpoch, language_);
  std::time_t seconds = std::time_t(when);
  std::tm local = {};
  if (localtime_s(&local, &seconds) == 0) {
    char time_text[16];
    std::snprintf(time_text, sizeof(time_text), " %02d:%02d", local.tm_hour, local.tm_min);
    date += time_text;
  }
  return date;
}

float App::ReleaseNotes(const std::string& markdown, ImVec2 pos, float width, bool draw) {
  constexpr Type kText{Font::kRegular, 15};
  constexpr Type kTag{Font::kBold, 12, 0.08f};
  constexpr float kLine = 1.45f;
  // Each heading is a tag in a color of its own (the mockup's "New", "Improvements", "Fixes"), its items' bullets in it.
  const ImU32 kColors[] = {col::kAccent, col::kGlyphX, col::kError, col::kGlyphY};
  int section = -1;
  float y = pos.y;
  std::string paragraph;
  int item_level = -1;  // the list item being gathered, or -1 for a paragraph
  bool quote = false;
  bool any = false;
  auto color = [&] { return section < 0 ? col::kAccent : kColors[section % 4]; };
  auto flush = [&] {
    if (paragraph.empty()) return;
    std::string text = PlainInline(paragraph);
    float indent = item_level >= 0 ? Px(18.0f) * float(item_level + 1) : quote ? Px(14.0f) : 0.0f;
    float height = DrawParagraph(kText, ImVec2(pos.x + indent, y), width - indent, kLine, col::kTextSoft, text, draw);
    if (draw && item_level >= 0) {
      float dot = Px(5.0f);
      ImVec2 center(pos.x + indent - Px(10.0f), y + Px(kText.size) * kLine * 0.5f);
      ImGui::GetWindowDrawList()->AddCircleFilled(center, dot * 0.5f, item_level ? col::kTextDim : color());
    }
    if (draw && quote) {
      FillRect(ImRect(ImVec2(pos.x, y + Px(2.0f)), ImVec2(pos.x + Px(3.0f), y + height - Px(2.0f))), col::kOutline, 1.5f);
    }
    y += height + Px(8.0f);
    paragraph.clear();
    any = true;
  };
  size_t start = 0;
  while (start <= markdown.size()) {
    size_t end = markdown.find('\n', start);
    std::string_view raw(markdown.data() + start, (end == std::string::npos ? markdown.size() : end) - start);
    start = end == std::string::npos ? markdown.size() + 1 : end + 1;
    size_t indent = 0;
    while (indent < raw.size() && (raw[indent] == ' ' || raw[indent] == '\t')) ++indent;
    std::string_view line = Trim(raw);
    // A line of HTML tags only (a picture, <p align="center">): nothing to read, a break between paragraphs.
    if (!line.empty() && line[0] == '<' && Trim(PlainInline(line)).empty()) line = {};
    if (line.empty()) {
      flush();
      item_level = -1;
      quote = false;
      continue;
    }
    if (line[0] == '#') {
      flush();
      item_level = -1;
      quote = false;
      while (!line.empty() && line[0] == '#') line.remove_prefix(1);
      std::string heading = Upper(PlainInline(Trim(line)));
      ++section;
      if (any) y += Px(10.0f);
      float tag_width = std::min(MeasureText(kTag, heading).x + Px(18.0f), width);
      ImRect tag(ImVec2(pos.x, y), ImVec2(pos.x + tag_width, y + LineHeight(kTag) + Px(6.0f)));
      if (draw) {
        FillRect(tag, Fade(color(), 0.16f), 4.0f);
        DrawLabel(kTag, ImVec2(tag.Min.x + Px(9.0f), tag.Min.y), color(), Ellipsize(kTag, heading, tag_width - Px(18.0f)),
                  tag.GetHeight());
      }
      y = tag.Max.y + Px(10.0f);
      any = true;
      continue;
    }
    if (line[0] == '>') {
      line.remove_prefix(1);
      line = Trim(line);
      if (line.rfind("[!", 0) == 0) continue;  // GitHub's "> [!IMPORTANT]" marker
      if (!quote) {
        flush();
        quote = true;
        item_level = -1;
      }
      if (!paragraph.empty()) paragraph += ' ';
      paragraph.append(line);
      continue;
    }
    if (size_t marker = ListMarker(line); marker != std::string_view::npos) {
      flush();
      quote = false;
      item_level = std::min<int>(int(indent / 2), 3);
      paragraph.append(line.substr(marker));
      continue;
    }
    if (quote) {
      flush();
      quote = false;
    }
    if (!paragraph.empty()) paragraph += ' ';
    paragraph.append(line);
  }
  flush();
  return y - pos.y;
}

void App::UpdatesPage(const ImRect& rect) {
  Options& o = settings_.o;
  bool changed = false;
  const Version running = Version::Parse(kVersion);
  const bool checking = update_check_.valid();
  const Update::Progress progress = update_.Get();
  const bool updating = progress.state == Update::State::kDownloading || progress.state == Update::State::kVerifying ||
                        progress.state == Update::State::kInstalling || progress.state == Update::State::kDone;
  const Release* newer = NewerRelease();
  // A game without releases to look for (game.h): its version, and nothing to check.
  const bool off = update_url_.empty();

  // --- The layout (launcher 3.18, the mockup's board 14; dp from the page's top left): the title at 56, 20; the state's
  // column from 56, 104, 424 wide; a hairline at 544; the release notes from 584 to 56 from the right edge. ---
  const float page_left = rect.Min.x;
  const float page_top = rect.Min.y;
  const float foot = rect.Max.y - Px(28.0f);
  constexpr Type kTitle{Font::kDisplaySemiBold, 44, 0.015f};
  constexpr Type kHeading{Font::kDisplayMedium, 26, 0.015f};
  constexpr Type kSmall{Font::kRegular, 14};
  constexpr Type kText{Font::kRegular, 15};
  DrawLabel(kTitle, ImVec2(page_left + Px(56.0f), page_top + Px(20.0f)), col::kText, Title(t_.updates_title),
            Px(44.0f * 1.1f));
  const float top = page_top + Px(104.0f);
  FillRect(ImRect(ImVec2(page_left + Px(544.0f), top), ImVec2(page_left + Px(544.0f) + Snap(Px(1.0f)), foot - Px(10.0f))),
           col::kHairline);

  // The state: its icon and title, its line; the installed version and the last check; the main button (check, or
  // update to the new version, with its progress); checking when the launcher opens; every version on GitHub.
  {
    const float x = page_left + Px(56.0f);
    const float width = Px(424.0f);
    float y = top;
    bool failed_check = updates_ && !updates_->ok && !checking;
    bool failed_update = progress.state == Update::State::kFailed;
    char version[64];
    std::snprintf(version, sizeof(version), t_.version, kVersion);
    std::string title = version;
    std::string sub = t_.check_on_start_hint;
    ImU32 sub_color = col::kTextMuted;
    Icon icon = Icon::kRefresh;
    ImU32 ink = col::kTextMuted;
    if (off) {
      sub = t_.updates_off;
    } else if (checking) {
      title = t_.updates_checking_title;
      sub = t_.updates_checking_sub;
      icon = Icon::kNone;
    } else if (newer) {
      std::string number = newer->tag.size() > 1 && (newer->tag[0] == 'v' || newer->tag[0] == 'V') ? newer->tag.substr(1)
                                                                                                    : newer->tag;
      char text[96];
      std::snprintf(text, sizeof(text), t_.update_card_available, number.c_str());
      title = text;
      char published[160];
      std::snprintf(published, sizeof(published), t_.update_published, PublishedDate(newer->published, language_).c_str(),
                    Megabytes(newer->asset_size).c_str());
      sub = published;
      icon = Icon::kArrowDown;
      ink = col::kAccent;
    } else if (failed_check) {
      title = t_.updates_failed_title;
      char text[512];
      if (updates_->rate_limited) {
        std::snprintf(text, sizeof(text), "%s", t_.check_rate_limited);
      } else {
        std::snprintf(text, sizeof(text), t_.check_failed, updates_->error.c_str());
      }
      sub = text;
      sub_color = col::kError;
      icon = Icon::kAlert;
      ink = col::kError;
    } else if (updates_) {
      title = t_.all_up_to_date;
      sub = t_.up_to_date;
      icon = Icon::kCheck;
      ink = col::kSuccess;
    }
    // The icon (a spinning arc while it checks), 14 dp before the title.
    float title_height = LineHeight(kHeading);
    ImVec2 icon_pos(x, y + (title_height - Px(30.0f)) * 0.5f);
    if (checking) {
      ImDrawList* draw = ImGui::GetWindowDrawList();
      ImVec2 c(icon_pos.x + Px(15.0f), icon_pos.y + Px(15.0f));
      float start = float(ImGui::GetTime()) * 5.0f;
      draw->PathArcTo(c, Px(11.0f), start, start + 4.7f, 32);
      draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(2.4f));
    } else {
      DrawIcon(icon, icon_pos, 30.0f, ink, icon == Icon::kCheck ? 2.2f : 1.75f);
    }
    float title_x = x + Px(30.0f) + Px(14.0f);
    DrawLabel(kHeading, ImVec2(title_x, y), col::kText, Ellipsize(kHeading, title, x + width - title_x));
    y += title_height + Px(8.0f);
    y += DrawParagraph(kText, ImVec2(x, y), width, 1.5f, sub_color, sub) + Px(26.0f);

    // The two values: the installed version (with the launcher's) and the last check, 56 dp apart.
    constexpr Type kValue{Font::kDisplayMedium, 22};
    {
      DrawLabel(kSmall, ImVec2(x, y), col::kTextDim, t_.installed_version_short);
      DrawLabel(kValue, ImVec2(x, y + LineHeight(kSmall) + Px(2.0f)), col::kText, kVersion);
      char launcher[64];
      std::snprintf(launcher, sizeof(launcher), t_.launcher_version, kLauncherVersion);
      float value_width = MeasureText(kValue, kVersion).x;
      float baseline = y + LineHeight(kSmall) + Px(2.0f) + Px(22.0f) * 1.193f;
      DrawLabel(kSmall, ImVec2(x + value_width + Px(8.0f), baseline - Px(14.0f) * 1.024f), col::kTextDim, launcher);
      float second_x = x + std::max(value_width + Px(8.0f) + MeasureText(kSmall, launcher).x,
                                    MeasureText(kSmall, t_.installed_version_short).x) + Px(56.0f);
      DrawLabel(kSmall, ImVec2(second_x, y), col::kTextDim, t_.last_review);
      DrawLabel(kValue, ImVec2(second_x, y + LineHeight(kSmall) + Px(2.0f)), col::kText,
                Ellipsize(kValue, o.last_update_check > 0 ? LastCheckedText() : std::string(t_.never), x + width - second_x));
      y += LineHeight(kSmall) + Px(2.0f) + LineHeight(kValue) + Px(28.0f);
    }

    if (!off) {
      // The main button: Check for updates (again), or Update now; disabled while it checks or updates.
      constexpr Type kButton{Font::kDisplaySemiBold, 28, 0.02f};
      const char* label = newer ? t_.update_now : checking ? t_.updates_checking_title : updates_ ? t_.check_again
                                                                                                  : t_.check_updates;
      bool enabled = !checking && !updating;
      constexpr float kGlyph = 28.0f;
      float glyph = GlyphWidth(PadButton::kA, kGlyph);
      float button_width = std::min(width, Px(20.0f) + glyph + Px(14.0f) + MeasureText(kButton, label).x + Px(30.0f));
      ImRect button(ImVec2(x + Px(2.0f), y), ImVec2(x + Px(2.0f) + button_width, y + Px(64.0f)));
      ItemState state = Item("##update_button", button, enabled);
      if (enabled) {
        FillRect(button, state.hovered ? Brighten(col::kAccent) : col::kAccent, 14.0f);
        DrawGlyph(PadButton::kA, ImVec2(button.Min.x + Px(20.0f), button.GetCenter().y - Px(kGlyph) * 0.5f), kGlyph, true);
        DrawLabel(kButton, ImVec2(button.Min.x + Px(20.0f) + glyph + Px(14.0f), button.Min.y), col::kOnAccent,
                  Ellipsize(kButton, label, button.Max.x - Px(29.0f) - (button.Min.x + Px(20.0f) + glyph + Px(14.0f))),
                  button.GetHeight());
      } else {
        FillRect(button, col::kChip, 14.0f);
        DrawLabel(kButton, ImVec2(button.GetCenter().x - MeasureText(kButton, label).x * 0.5f, button.Min.y), col::kTextDim,
                  label, button.GetHeight());
      }
      if (state.focused) FocusRing(button, 14.0f);
      page_focus_ = ImGui::GetItemID();
      if (state.pressed) {
        if (newer) {
          update_.Start(*newer, exe_dir_);
        } else {
          CheckForUpdatesNow();
        }
      }
      y = button.Max.y;
      // The update's progress, or why it failed.
      if (updating || failed_update) {
        y += Px(14.0f);
        char status[512];
        ImU32 status_color = col::kTextMuted;
        if (failed_update) {
          char reason[384];
          switch (progress.problem) {
            case Update::Problem::kNoChecksum:
              std::snprintf(reason, sizeof(reason), "%s", t_.update_no_checksum);
              break;
            case Update::Problem::kChecksumMismatch:
              std::snprintf(reason, sizeof(reason), "%s", t_.update_checksum_mismatch);
              break;
            case Update::Problem::kInUse:
              std::snprintf(reason, sizeof(reason), t_.update_in_use, progress.detail.c_str());
              break;
            default:
              std::snprintf(reason, sizeof(reason), "%s", progress.error.c_str());
              break;
          }
          std::snprintf(status, sizeof(status), t_.update_failed, reason);
          status_color = col::kError;
          y += DrawParagraph(kSmall, ImVec2(x, y), width, 1.4f, status_color, status);
        } else {
          if (progress.state == Update::State::kDownloading) {
            std::snprintf(status, sizeof(status), t_.update_downloading, Megabytes(progress.done).c_str(),
                          Megabytes(progress.total).c_str());
          } else {
            std::snprintf(status, sizeof(status), "%s",
                          progress.state == Update::State::kVerifying ? t_.update_verifying : t_.update_installing);
          }
          DrawLabel(kSmall, ImVec2(x, y), status_color, Ellipsize(kSmall, status, width));
          y += LineHeight(kSmall) + Px(8.0f);
          float fraction = progress.state == Update::State::kDownloading && progress.total
                               ? float(double(progress.done) / double(progress.total))
                               : 1.0f;
          Progress(ImRect(ImVec2(x, y), ImVec2(x + width, y + Px(6.0f))), fraction, col::kChip, col::kAccent);
          y += Px(6.0f);
        }
      }
      y += Px(36.0f);

      // Checking when the launcher opens, and betas: rows between hairlines, each row its item (the controller's A flips
      // it), its switch at the right the mouse's (launcher 3.22: a click elsewhere on the row changes nothing).
      constexpr Type kLabel{Font::kMedium, 18};
      auto switch_row = [&](const char* id, const char* label, const char* hint, bool& value) {
        ImRect row(ImVec2(x, y), ImVec2(x + width, y + Px(76.0f)));
        ItemState row_state = Item(id, row);
        FillRect(ImRect(row.Min, ImVec2(row.Max.x, row.Min.y + Snap(Px(1.0f)))), col::kHairline);
        if (row_state.focused) {
          FocusRow(ImRect(ImVec2(row.Min.x - Px(12.0f), row.Min.y + Px(4.0f)),
                          ImVec2(row.Max.x + Px(12.0f), row.Max.y - Px(4.0f))),
                   12.0f);
        } else {
          FillRect(ImRect(ImVec2(row.Min.x, row.Max.y - Snap(Px(1.0f))), row.Max), col::kHairline);
        }
        const ImVec2 toggle(row.Max.x - Px(52.0f), row.GetCenter().y - Px(15.0f));
        const ImRect toggle_hit(ImVec2(toggle.x - Px(8.0f), toggle.y - Px(8.0f)),
                                ImVec2(toggle.x + Px(60.0f), toggle.y + Px(38.0f)));
        const bool toggle_hovered = row_state.hovered && toggle_hit.Contains(ImGui::GetIO().MousePos);
        if (row_state.pressed && (PressedByNav() || toggle_hit.Contains(ImGui::GetIO().MouseClickedPos[0]))) {
          value = !value;
          changed = true;
        }
        float block = LineHeight(kLabel) + Px(2.0f) + LineHeight(kSmall);
        float ty = row.GetCenter().y - block * 0.5f;
        float text_width = width - Px(52.0f) - Px(16.0f);
        DrawLabel(kLabel, ImVec2(x, ty), row_state.focused ? col::kText : col::kTextSoft, Ellipsize(kLabel, label, text_width));
        DrawLabel(kSmall, ImVec2(x, ty + LineHeight(kLabel) + Px(2.0f)), col::kTextDim, Ellipsize(kSmall, hint, text_width));
        DrawToggle(toggle, value, true, false, toggle_hovered);
        y = row.Max.y;
      };
      switch_row("##check_on_start", t_.check_on_start, t_.check_on_start_hint, o.check_updates);
      switch_row("##beta_updates", t_.beta_updates, t_.beta_updates_hint, o.beta_updates);
      y += Px(20.0f);
      if (Link("##history", ImVec2(x, y), t_.full_history, 15)) {
        AskOpenUrl(game::kReleasesPage);
      }
    }
  }

  // The release notes: what is new since the installed version, or the installed version's.
  {
    const float x = page_left + Px(584.0f);
    const float right = rect.Max.x - Px(56.0f);
    const float width = right - x;
    float y = top;
    DrawLabel(kHeading, ImVec2(x, y), col::kText, t_.release_notes_title);
    std::vector<const Release*> shown;
    if (updates_ && updates_->ok) {
      // A newer beta's notes only for a player who asks for betas; the installed version's own (a beta's too) below.
      for (const Release& release : updates_->releases) {
        if (running < release.version && (o.beta_updates || !release.prerelease)) shown.push_back(&release);
      }
      if (shown.empty()) {
        if (const Release* installed = updates_->Of(running)) shown.push_back(installed);
      }
    }
    if (!shown.empty()) {
      std::string meta = shown.front()->tag + " \xC2\xB7 " + PublishedDate(shown.front()->published, language_);
      DrawLabel(kSmall, ImVec2(right - MeasureText(kSmall, meta).x, y + Px(26.0f) * 1.193f - Px(14.0f) * 1.024f),
                col::kTextDim, meta);
    }
    y += LineHeight(kHeading) + Px(12.0f);
    ImRect notes(ImVec2(x, y), ImVec2(right + Px(16.0f), foot));
    if (shown.empty()) {
      DrawParagraph(kText, notes.Min, width, 1.5f, col::kTextMuted,
                    off ? t_.updates_off : checking ? t_.checking_updates : t_.release_notes_none);
    } else {
      constexpr Type kSection{Font::kSemiBold, 14};
      BeginScroll("##notes", notes);
      ImVec2 origin = ImGui::GetCursorScreenPos();
      float notes_y = origin.y;
      for (const Release* release : shown) {
        // Several versions: each one's title before its notes.
        if (shown.size() > 1) {
          char title[96];
          std::snprintf(title, sizeof(title), running < release->version ? t_.whats_new : t_.release_notes,
                        release->tag.c_str());
          if (notes_y > origin.y) notes_y += Px(18.0f);
          DrawLabel(kSection, ImVec2(origin.x, notes_y), col::kTextMuted, title);
          notes_y += LineHeight(kSection) + Px(12.0f);
        }
        notes_y += ReleaseNotes(release->notes, ImVec2(origin.x, notes_y), width, true);
      }
      ImGui::Dummy(ImVec2(width, notes_y - origin.y));
      EndScroll();
    }
  }
  if (changed) {
    Changed();
  }
}

}  // namespace launcher
