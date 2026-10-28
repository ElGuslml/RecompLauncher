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
  const Release* newer = updates_ ? updates_->Newer(running) : nullptr;
  // A game without releases to look for (game.h): its version, and nothing to check.
  const bool off = update_url_.empty();

  ImRect main(ImVec2(rect.Min.x + Px(40.0f), rect.Min.y + Px(24.0f)), ImVec2(rect.Max.x - Px(40.0f), rect.Max.y - Px(20.0f)));
  float left = main.Min.x;
  float column = Px(520.0f);
  constexpr Type kTitle{Font::kDisplayBold, 40};
  DrawLabel(kTitle, main.Min, col::kText, Upper(t_.updates_title), Px(40.0f));
  float y = main.Min.y + Px(40.0f) + Px(16.0f);

  // The state: its tile, title and line; the installed version and the last check; the button (check, or update to
  // the new version, with its progress).
  {
    constexpr Type kStateTitle{Font::kDisplayBold, 26};
    constexpr Type kStateSub{Font::kRegular, 14};
    constexpr Type kStatLabel{Font::kBold, 12, 0.1f};
    constexpr Type kStatValue{Font::kDisplaySemiBold, 20};
    constexpr Type kSmall{Font::kRegular, 13};
    bool failed_check = updates_ && !updates_->ok && !checking;
    bool failed_update = progress.state == Update::State::kFailed;
    // The state's words.
    std::string title = t_.all_up_to_date;
    std::string sub = t_.up_to_date;
    ImU32 sub_color = col::kTabText;
    Icon icon = Icon::kCheck;
    ImU32 tile = Fade(col::kSuccess, 0.16f);
    ImU32 ink = col::kSuccess;
    if (off) {
      char text[64];
      std::snprintf(text, sizeof(text), t_.version, kVersion);
      title = text;
      sub = t_.updates_off;
    } else if (checking) {
      title = t_.updates_checking_title;
      sub = t_.updates_checking_sub;
      icon = Icon::kNone;
      tile = Fade(col::kAccent, 0.14f);
    } else if (newer) {
      std::string number = newer->tag.size() > 1 && (newer->tag[0] == 'v' || newer->tag[0] == 'V') ? newer->tag.substr(1) : newer->tag;
      char text[96];
      std::snprintf(text, sizeof(text), t_.update_card_available, number.c_str());
      title = text;
      char published[160];
      std::snprintf(published, sizeof(published), t_.update_published, PublishedDate(newer->published, language_).c_str(),
                    Megabytes(newer->asset_size).c_str());
      sub = published;
      icon = Icon::kArrowDown;
      tile = col::kAccent;
      ink = col::kOnAccent;
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
      tile = Fade(col::kError, 0.16f);
      ink = col::kError;
    } else if (!updates_) {
      // Not checked in this session: the version, when it was last checked.
      char text[64];
      std::snprintf(text, sizeof(text), t_.version, kVersion);
      title = text;
      sub = t_.check_on_start_hint;
      icon = Icon::kRefresh;
      tile = Fade(col::kAccent, 0.14f);
      ink = col::kAccent;
    }
    float inner_width = column - Px(48.0f);
    float sub_height = DrawParagraph(kStateSub, ImVec2(0, 0), inner_width - Px(72.0f), 1.35f, 0, sub, false);
    float head = std::max(Px(56.0f), Px(26.0f) * 1.1f + Px(4.0f) + sub_height);
    float stats = Px(12.0f) * 2.0f + LineHeight(kStatLabel) + Px(4.0f) + LineHeight(kStatValue);
    bool show_progress = updating || failed_update;
    float progress_height = show_progress ? Px(14.0f) + LineHeight(kSmall) + Px(8.0f) + Px(6.0f) : 0.0f;
    float height = Px(24.0f) * 2.0f + head + Px(18.0f) + stats + (off ? 0.0f : Px(18.0f) + Px(56.0f)) + progress_height;
    ImRect card(ImVec2(left, y), ImVec2(left + column, y + height));
    FillRect(card, col::kPanel, 14.0f);
    StrokeRect(card, Hex(0xFFFFFF, 26), 14.0f);
    float x = card.Min.x + Px(24.0f);
    float top = card.Min.y + Px(24.0f);
    ImRect icon_tile(ImVec2(x, top + (head - Px(56.0f)) * 0.5f), ImVec2(x + Px(56.0f), top + (head + Px(56.0f)) * 0.5f));
    FillRect(icon_tile, tile, 12.0f);
    if (checking) {
      ImDrawList* draw = ImGui::GetWindowDrawList();
      float start = float(ImGui::GetTime()) * 5.0f;
      draw->PathArcTo(icon_tile.GetCenter(), Px(10.5f), start, start + 4.7f, 32);
      draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(2.4f));
    } else {
      DrawIcon(icon, ImVec2(icon_tile.Min.x + Px(14.0f), icon_tile.Min.y + Px(14.0f)), 28.0f, ink, 2.4f);
    }
    float text_x = icon_tile.Max.x + Px(16.0f);
    float text_y = top + (head - (Px(26.0f) * 1.1f + Px(4.0f) + sub_height)) * 0.5f;
    DrawLabel(kStateTitle, ImVec2(text_x, text_y), col::kText, Ellipsize(kStateTitle, title, card.Max.x - Px(24.0f) - text_x),
              Px(26.0f) * 1.1f);
    DrawParagraph(kStateSub, ImVec2(text_x, text_y + Px(26.0f) * 1.1f + Px(4.0f)), card.Max.x - Px(24.0f) - text_x, 1.35f,
                  sub_color, sub);
    top += head + Px(18.0f);
    // The two values.
    float half = (inner_width - Px(10.0f)) * 0.5f;
    for (int i = 0; i < 2; ++i) {
      ImRect box(ImVec2(x + float(i) * (half + Px(10.0f)), top), ImVec2(x + float(i) * (half + Px(10.0f)) + half, top + stats));
      FillRect(box, Hex(0xFFFFFF, 13), 8.0f);
      const char* label = i == 0 ? t_.installed_version_short : t_.last_review;
      std::string value = i == 0 ? std::string(kVersion) : (settings_.o.last_update_check > 0 ? LastCheckedText() : t_.never);
      DrawLabel(kStatLabel, ImVec2(box.Min.x + Px(14.0f), box.Min.y + Px(12.0f)), col::kLabel,
                Ellipsize(kStatLabel, Upper(label), half - Px(28.0f)));
      DrawLabel(kStatValue, ImVec2(box.Min.x + Px(14.0f), box.Min.y + Px(12.0f) + LineHeight(kStatLabel) + Px(4.0f)), col::kText,
                Ellipsize(kStatValue, value, half - Px(28.0f)));
      if (i == 0) {
        char launcher[64];
        std::snprintf(launcher, sizeof(launcher), t_.launcher_version, kLauncherVersion);
        float value_width = MeasureText(kStatValue, value).x;
        float lx = box.Min.x + Px(14.0f) + value_width + Px(10.0f);
        if (lx + MeasureText(kSmall, launcher).x < box.Max.x - Px(12.0f)) {
          float baseline = box.Min.y + Px(12.0f) + LineHeight(kStatLabel) + Px(4.0f) + Px(20.0f) * 1.193f;
          DrawLabel(kSmall, ImVec2(lx, baseline - Px(13.0f) * 1.024f), col::kTabText, launcher);
        }
      }
    }
    top += stats + Px(18.0f);
    // The button: check (again), or update.
    if (!off) {
      constexpr Type kButton{Font::kDisplayBold, 20, 0.05f};
      const char* label = newer ? t_.update_now : checking ? t_.updates_checking_title : updates_ ? t_.check_again : t_.check_updates;
      std::string text = Upper(label);
      bool enabled = !checking && !updating;
      ImRect button(ImVec2(x, top), ImVec2(x + inner_width, top + Px(56.0f)));
      ItemState state = Item("##update_button", button, enabled);
      ImU32 background = state.hovered ? Brighten(col::kAccent) : col::kAccent;
      FillRect(button, Fade(background, enabled ? 1.0f : 0.6f), 10.0f);
      float glyph = GlyphWidth(PadButton::kA, 26.0f);
      float content = glyph + Px(12.0f) + MeasureText(kButton, text).x;
      float gx = button.GetCenter().x - content * 0.5f;
      DrawGlyph(PadButton::kA, ImVec2(gx, button.GetCenter().y - Px(13.0f)), 26.0f, true);
      DrawLabel(kButton, ImVec2(gx + glyph + Px(12.0f), button.Min.y), col::kOnAccent, text, button.GetHeight());
      if (state.focused) FocusRing(button, 10.0f);
      page_focus_ = ImGui::GetItemID();
      if (state.pressed) {
        if (newer) {
          update_.Start(*newer, exe_dir_);
        } else {
          CheckForUpdatesNow();
        }
      }
      top += Px(56.0f);
      // The update's progress, or why it failed.
      if (show_progress) {
        top += Px(14.0f);
        char status[512];
        ImU32 status_color = col::kTextSoft;
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
        } else if (progress.state == Update::State::kDownloading) {
          std::snprintf(status, sizeof(status), t_.update_downloading, Megabytes(progress.done).c_str(),
                        Megabytes(progress.total).c_str());
        } else {
          std::snprintf(status, sizeof(status), "%s",
                        progress.state == Update::State::kVerifying ? t_.update_verifying : t_.update_installing);
        }
        DrawLabel(kSmall, ImVec2(x, top), status_color, Ellipsize(kSmall, status, inner_width));
        top += LineHeight(kSmall) + Px(8.0f);
        if (!failed_update) {
          float fraction = progress.state == Update::State::kDownloading && progress.total
                               ? float(double(progress.done) / double(progress.total))
                               : 1.0f;
          Progress(ImRect(ImVec2(x, top), ImVec2(x + inner_width, top + Px(6.0f))), fraction, col::kPanelTrack, col::kAccent);
        }
      }
    }
    y = card.Max.y + Px(16.0f);
  }

  // Checking when the launcher opens, and every version on GitHub.
  if (!off) {
    constexpr Type kSwitchTitle{Font::kBold, 15};
    constexpr Type kSwitchHint{Font::kRegular, 13};
    constexpr Type kLinkType{Font::kBold, 14};
    float height = Px(18.0f) * 2.0f + LineHeight(kSwitchTitle) + Px(2.0f) + LineHeight(kSwitchHint) + Px(14.0f) +
                   LineHeight(kLinkType);
    ImRect card(ImVec2(left, y), ImVec2(left + column, y + height));
    FillRect(card, col::kPanel, 14.0f);
    StrokeRect(card, col::kHairline, 14.0f);
    float x = card.Min.x + Px(20.0f);
    float top = card.Min.y + Px(18.0f);
    float row_height = LineHeight(kSwitchTitle) + Px(2.0f) + LineHeight(kSwitchHint);
    ImRect row(ImVec2(x, top), ImVec2(card.Max.x - Px(20.0f), top + row_height));
    ItemState state = Item("##check_on_start", row);
    DrawLabel(kSwitchTitle, ImVec2(x, top), col::kText, t_.check_on_start);
    DrawLabel(kSwitchHint, ImVec2(x, top + LineHeight(kSwitchTitle) + Px(2.0f)), col::kTabText, t_.check_on_start_hint);
    // A 22 dp check box in accent.
    ImRect box(ImVec2(row.Max.x - Px(22.0f), row.GetCenter().y - Px(11.0f)), ImVec2(row.Max.x, row.GetCenter().y + Px(11.0f)));
    if (o.check_updates) {
      FillRect(box, col::kAccent, 4.0f);
      DrawIcon(Icon::kCheck, ImVec2(box.Min.x + Px(3.0f), box.Min.y + Px(3.0f)), 16.0f, col::kOnAccent, 3.0f);
    } else {
      FillRect(box, col::kGhostFill, 4.0f);
      StrokeRect(box, Hex(0xFFFFFF, 90), 4.0f, 1.5f);
    }
    if (state.focused) FocusRing(row, 6.0f);
    if (state.pressed) {
      o.check_updates = !o.check_updates;
      changed = true;
    }
    top += row_height + Px(14.0f);
    if (Link("##history", ImVec2(x, top), t_.full_history, 14)) {
      OpenUrl(game::kReleasesPage);
    }
  }

  // The release notes: what is new since the installed version, or the installed version's.
  {
    ImRect card(ImVec2(left + column + Px(24.0f), main.Min.y), main.Max);
    FillRect(card, col::kPanel, 14.0f);
    StrokeRect(card, col::kHairline, 14.0f);
    constexpr Type kHeading{Font::kDisplayBold, 24};
    constexpr Type kMeta{Font::kRegular, 13};
    float x = card.Min.x + Px(24.0f);
    float width = card.GetWidth() - Px(48.0f);
    float top = card.Min.y + Px(24.0f);
    DrawLabel(kHeading, ImVec2(x, top), col::kText, Upper(t_.release_notes_title));
    std::vector<const Release*> shown;
    if (updates_ && updates_->ok) {
      for (const Release& release : updates_->releases) {
        if (running < release.version) shown.push_back(&release);
      }
      if (shown.empty()) {
        if (const Release* installed = updates_->Of(running)) shown.push_back(installed);
      }
    }
    if (!shown.empty()) {
      std::string meta = shown.front()->tag + " · " + PublishedDate(shown.front()->published, language_);
      DrawLabel(kMeta, ImVec2(card.Max.x - Px(24.0f) - MeasureText(kMeta, meta).x, top + Px(24.0f) * 1.193f - Px(13.0f) * 1.024f),
                col::kTabText, meta);
    }
    top += LineHeight(kHeading) + Px(16.0f);
    ImRect notes(ImVec2(x, top), ImVec2(card.Max.x - Px(16.0f), card.Max.y - Px(24.0f)));
    if (shown.empty()) {
      DrawParagraph(Type{Font::kRegular, 15}, notes.Min, width, 1.5f, col::kTextSoft,
                    off ? t_.updates_off : checking ? t_.checking_updates : t_.release_notes_none);
    } else {
      constexpr Type kSection{Font::kBold, 12, 0.1f};
      BeginScroll("##notes", notes);
      ImVec2 origin = ImGui::GetCursorScreenPos();
      float notes_width = ImGui::GetContentRegionAvail().x - Px(16.0f);
      float notes_y = origin.y;
      for (const Release* release : shown) {
        // Several versions: each one's title before its notes.
        if (shown.size() > 1) {
          char title[96];
          std::snprintf(title, sizeof(title), running < release->version ? t_.whats_new : t_.release_notes,
                        release->tag.c_str());
          if (notes_y > origin.y) notes_y += Px(18.0f);
          DrawLabel(kSection, ImVec2(origin.x, notes_y), col::kLabel, title);
          notes_y += LineHeight(kSection) + Px(12.0f);
        }
        notes_y += ReleaseNotes(release->notes, ImVec2(origin.x, notes_y), notes_width, true);
      }
      ImGui::Dummy(ImVec2(notes_width, notes_y - origin.y));
      EndScroll();
    }
  }
  if (changed) {
    Changed();
  }
}

}  // namespace launcher
