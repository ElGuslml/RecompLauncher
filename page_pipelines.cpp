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
  // Launcher 3.18: as the first run's steps, a column over the art at its left (no card), centred in the window's
  // height (the screen has no header nor footer).
  Background(size, 0.0f);
  constexpr Type kTitle{Font::kDisplaySemiBold, 60, 0.015f};
  constexpr Type kLead{Font::kRegular, 17};
  const float x = origin.x + Px(56.0f);
  const float width = std::min(Px(500.0f), size.x - Px(112.0f));
  const std::string title = Title(t_.compiling_title);
  const float title_height = DrawParagraph(kTitle, ImVec2(0, 0), std::max(width, Px(520.0f)), 1.05f, 0, title, false);
  const float lead_height = DrawParagraph(kLead, ImVec2(0, 0), width, 1.55f, 0, t_.compiling_text, false);
  const float block = title_height + Px(16.0f) + lead_height + Px(30.0f) + Px(8.0f) + Px(10.0f) + LineHeight(kCount) +
                      Px(40.0f) + Px(48.0f);
  float y = Snap(origin.y + std::max(Px(40.0f), (size.y - block) * 0.5f));

  y += DrawParagraph(kTitle, ImVec2(x, y), std::max(width, Px(520.0f)), 1.05f, col::kText, title) + Px(16.0f);
  y += DrawParagraph(kLead, ImVec2(x, y), width, 1.55f, col::kTextSoft, t_.compiling_text) + Px(30.0f);

  // The bar (live progress: the accent) and the count under it; a turning arc until the game's tool has counted its
  // work.
  if (progress.total) {
    Progress(ImRect(ImVec2(x, y), ImVec2(x + width, y + Px(8.0f))),
             float(double(progress.done) / double(progress.total)), col::kChip, col::kAccent);
    char text[96];
    std::snprintf(text, sizeof(text), t_.compiling_count, int(progress.done), int(progress.total));
    DrawLabel(kCount, ImVec2(x, y + Px(8.0f) + Px(10.0f)), col::kTextSoft, text);
  } else {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    float radius = Px(10.0f);
    ImVec2 center(x + radius, y + radius);
    float start = float(ImGui::GetTime()) * 5.0f;
    draw->PathArcTo(center, radius, 0.0f, 6.2831853f, 32);
    draw->PathStroke(col::kChip, ImDrawFlags_None, Px(2.5f));
    draw->PathArcTo(center, radius, start, start + 1.9f, 16);
    draw->PathStroke(col::kAccent, ImDrawFlags_None, Px(2.5f));
  }
  y += Px(8.0f) + Px(10.0f) + LineHeight(kCount) + Px(40.0f);

  // Skip (B, Escape or a click): the game starts now and compiles as it draws.
  KeyButtonSpec skip{KeyLook::kGhost, 'B', Type{Font::kSemiBold, 16}, 48, 22, 12, 22, 10};
  std::string skip_label = t_.compiling_skip;
  bool pressed = KeyButton("##skip_compiling", ImVec2(x + Px(2.0f), y), skip, skip_label).pressed;
  ImGui::SetItemDefaultFocus();
  if (pressed || ButtonPressed(PadButton::kB) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    preparation_.Skip();
  }
  float hint_x = x + Px(2.0f) + KeyButtonWidth(skip, skip_label) + Px(16.0f);
  DrawParagraph(kHint, ImVec2(hint_x, y + Px(4.0f)), std::max(Px(200.0f), x + width + Px(120.0f) - hint_x), 1.4f,
                col::kTextDim, t_.compiling_skip_hint);
}

}  // namespace launcher
