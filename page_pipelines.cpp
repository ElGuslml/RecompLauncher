// RecompLauncher - "Compiling shaders": NativeXRenderer's pipelines compiled before Play (pipelines.h).

#include <algorithm>
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
constexpr Type kCount{Font::kSemiBold, 15};
constexpr Type kHint{Font::kRegular, 14};

}  // namespace

bool App::PreparePipelinesFirst(const std::wstring& arguments) {
  if (prepared_) {
    prepared_ = false;  // just compiled, or skipped: Play goes on
    return false;
  }
  if (!game::kHasNativeXRenderer || !settings_.o.native || !PipelinesNeedPreparing(exe_dir_)) {
    return false;
  }
  preparation_.Start(exe_dir_, arguments);
  preparing_ = true;
  result_ = Result::kNone;
  return true;
}

void App::PollPreparation() {
  if (!preparing_) {
    return;
  }
  if (preparation_.Get().state == PipelinePreparation::State::kRunning) {
    return;
  }
  // Done, failed or skipped: the game starts either way (a failed preparation leaves it compiling as it draws).
  preparing_ = false;
  prepared_ = true;
  preparation_.Reset();
  result_ = Result::kPlay;
}

void App::PreparingScreen(ImVec2 size) {
  PollPreparation();
  if (!preparing_) {
    return;
  }
  PipelinePreparation::Progress progress = preparation_.Get();
  ImVec2 origin = ImGui::GetMainViewport()->Pos;
  Background(size, 0.55f);

  // A card in the middle of the window, 640 dp wide, padded 32 36.
  float card_width = std::min(Px(640.0f), size.x - Px(112.0f));
  float card_height = std::min(Px(330.0f), size.y - Px(64.0f));
  ImRect card(ImVec2(Snap(origin.x + (size.x - card_width) * 0.5f), Snap(origin.y + (size.y - card_height) * 0.5f)),
              ImVec2(Snap(origin.x + (size.x + card_width) * 0.5f), Snap(origin.y + (size.y + card_height) * 0.5f)));
  FillRect(card, col::kPanel, 16.0f);
  StrokeRect(card, Hex(0xFFFFFF, 26), 16.0f);
  float x = card.Min.x + Px(36.0f);
  float width = card.GetWidth() - Px(72.0f);
  float y = card.Min.y + Px(32.0f);

  DrawLabel(kKicker, ImVec2(x, y), col::kAccent, Upper(game::kName));
  y += LineHeight(kKicker) + Px(8.0f);
  y += DrawParagraph(Type{Font::kDisplayBold, 40.0f}, ImVec2(x, y), width, 1.0f, col::kText, Upper(t_.compiling_title));
  y += Px(14.0f);
  y += DrawParagraph(kLead, ImVec2(x, y), width, 1.55f, col::kTextSoft, t_.compiling_text);
  y += Px(22.0f);

  // The bar, and the count under it; a turning arc until the game's tool has counted its work.
  if (progress.total) {
    Progress(ImRect(ImVec2(x, y), ImVec2(x + width, y + Px(8.0f))),
             float(double(progress.done) / double(progress.total)), col::kPanelTrack, col::kAccent);
    y += Px(8.0f) + Px(10.0f);
    char text[96];
    std::snprintf(text, sizeof(text), t_.compiling_count, int(progress.done), int(progress.total));
    DrawLabel(kCount, ImVec2(x, y), col::kTextSoft, text);
  } else {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float radius = Px(10.0f);
    ImVec2 center(x + radius, y + radius);
    float start = float(ImGui::GetTime()) * 5.0f;
    draw->PathArcTo(center, radius, 0.0f, 6.2831853f, 32);
    draw->PathStroke(col::kTrack, ImDrawFlags_None, Px(2.5f));
    draw->PathArcTo(center, radius, start, start + 1.9f, 16);
    draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(2.5f));
  }

  // Skip (B, Escape or a click): the game starts now and compiles as it draws.
  KeyButtonSpec skip{KeyLook::kGhost, 'B', Type{Font::kSemiBold, 16}, 52, 24};
  float buttons_y = card.Max.y - Px(32.0f) - Px(52.0f);
  std::string skip_label = t_.compiling_skip;
  bool pressed = KeyButton("##skip_compiling", ImVec2(x, buttons_y), skip, skip_label).pressed;
  ImGui::SetItemDefaultFocus();
  if (pressed || ButtonPressed(PadButton::kB) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    preparation_.Skip();
  }
  float hint_x = x + KeyButtonWidth(skip, skip_label) + Px(16.0f);
  DrawParagraph(kHint, ImVec2(hint_x, buttons_y + Px(8.0f)), card.Max.x - Px(36.0f) - hint_x, 1.4f, col::kTabText,
                t_.compiling_skip_hint);
}

}  // namespace launcher
