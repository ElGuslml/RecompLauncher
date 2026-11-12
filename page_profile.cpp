// The profiles in the launcher (app.h, profile_store.h): the first profile's dialog when the store has none, and this
// installation's saves from before the profiles copied into the current profile, with a dialog when the profile
// already has saves of the game. The profiles mockup's "Crear perfil" (a card over the page).

#include "app.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define SECURITY_WIN32
#include <security.h>

#include <SDL3/SDL_dialog.h>

#include <algorithm>
#include <chrono>
#include <cstdio>

#include "game.h"
#include "launcher_version.h"
#include "pages.h"
#include "profile_art.h"
#include "ui/gfx.h"

namespace launcher {

using namespace ui;

namespace {

// A photo's side in pixels, in the profile's folder (avatar.png).
constexpr int kPhotoSize = 256;

ImU32 ProfileColor(int index) {
  uint32_t rgb = profiles::kColors[size_t(std::clamp(index, 0, int(profiles::kColors.size()) - 1))];
  return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0xFF);
}

std::string ToUtf8(std::wstring_view text) {
  if (text.empty()) {
    return {};
  }
  int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
  std::string out(size_t(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), size, nullptr, nullptr);
  return out;
}

std::string PathUtf8(const std::filesystem::path& path) {
  std::u8string text = path.u8string();
  return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

// The Windows user's name for the first profile: the display name ("Gustavo Garcia"), else the account's.
std::string WindowsUserName() {
  wchar_t name[256];
  ULONG size = ULONG(std::size(name));
  if (GetUserNameExW(NameDisplay, name, &size) && size > 0 && name[0]) {
    return ToUtf8(name);
  }
  DWORD account = DWORD(std::size(name));
  if (GetUserNameW(name, &account) && name[0]) {
    return ToUtf8(name);
  }
  return {};
}

// A centered card over the page (launcher 3.18: the card's color, a hairline, 18 dp corners); returns its rectangle.
// Centred in the space over the footer, which the dialog keeps.
ImRect DialogCard(ImVec2 size, float width_dp, float height) {
  ImVec2 origin = ImGui::GetMainViewport()->Pos;
  float width = Px(width_dp);
  float space = size.y - Snap(Px(52.0f));
  ImRect card(Snap(ImVec2(origin.x + (size.x - width) * 0.5f, origin.y + (space - height) * 0.5f)),
              Snap(ImVec2(origin.x + (size.x + width) * 0.5f, origin.y + (space + height) * 0.5f)));
  FillRect(card, col::kCard, 18.0f);
  StrokeRect(card, col::kHairline, 18.0f);
  return card;
}

}  // namespace

void FieldLabel(ImVec2 pos, float width, std::string_view title, std::string_view count) {
  constexpr Type kTitle{Font::kSemiBold, 15};
  constexpr Type kCount{Font::kRegular, 13};
  DrawLabel(kTitle, pos, col::kTextSoft, title);
  if (!count.empty()) {
    DrawLabel(kCount, ImVec2(pos.x + width - MeasureText(kCount, count).x, pos.y), col::kTextDim, count, LineHeight(kTitle));
  }
}

int AvatarPicker(const char* id, ImVec2 pos, float size, float gap, int selected, ImTextureID photo,
                 std::string_view photo_label) {
  int pressed = -1;
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const float side = Px(size);
  ImGui::PushID(id);
  for (size_t i = 0; i <= profiles::kAvatars.size(); ++i) {
    bool is_photo = i == profiles::kAvatars.size();
    float x = pos.x + float(i) * (side + Px(gap));
    ImRect r(ImVec2(x, pos.y), ImVec2(x + side, pos.y + side));
    ImGui::PushID(int(i));
    ItemState state = Item("##avatar", r);
    ImGui::PopID();
    bool on = is_photo ? selected < 0 : selected == int(i);
    ImVec2 center = r.GetCenter();
    if (!is_photo) {
      DrawSvg(profiles::kAvatars[i].key, profiles::kAvatars[i].body, 100.0f, r, true);
    } else if (photo != ImTextureID_Invalid) {
      DrawTexture(photo, r, true);
    } else {
      // The photo's place: a dashed circle, a camera and its word.
      DashedRect(r, state.hovered ? col::kTextDim : col::kOutline, size * 0.5f / col::kRoundness);
      constexpr Type kSmall{Font::kSemiBold, 12};
      std::string caption = Ellipsize(kSmall, photo_label, side - Px(10.0f));
      float block = Px(20.0f) + Px(2.0f) + LineHeight(kSmall);
      float top = center.y - block * 0.5f;
      DrawIcon(Icon::kCamera, ImVec2(center.x - Px(10.0f), top), 20.0f, col::kTextMuted, 1.8f);
      DrawLabel(kSmall, ImVec2(center.x - MeasureText(kSmall, caption).x * 0.5f, top + Px(22.0f)), col::kTextMuted,
                caption);
    }
    if (state.hovered && !on && !state.focused) {
      draw->AddCircle(center, side * 0.5f + Px(3.0f), Fade(col::kText, 0.25f), 48, Px(2.0f));
    }
    if (on) {
      // The chosen one: a check on an accent badge at its lower right, ringed in the window's color.
      float badge = Px(22.0f);
      ImVec2 c(r.Max.x + Px(4.0f) - badge * 0.5f, r.Max.y + Px(2.0f) - badge * 0.5f);
      draw->AddCircleFilled(c, badge * 0.5f + Px(2.0f), col::kBackground, 24);
      draw->AddCircleFilled(c, badge * 0.5f, col::kAccent, 24);
      DrawIcon(Icon::kCheck, ImVec2(c.x - Px(7.0f), c.y - Px(7.0f)), 14.0f, col::kOnAccent, 2.6f);
    }
    if (state.focused) FocusRing(r, size * 0.5f / col::kRoundness);
    if (state.pressed) pressed = int(i);
  }
  ImGui::PopID();
  return pressed;
}

int ColorPicker(const char* id, ImVec2 pos, float size, float gap, int selected) {
  int pressed = -1;
  ImDrawList* draw = ImGui::GetWindowDrawList();
  const float side = Px(size);
  ImGui::PushID(id);
  for (size_t i = 0; i < profiles::kColors.size(); ++i) {
    float x = pos.x + float(i) * (side + Px(gap));
    ImRect r(ImVec2(x, pos.y), ImVec2(x + side, pos.y + side));
    ImGui::PushID(int(i));
    ItemState state = Item("##color", r);
    ImGui::PopID();
    ImU32 color = ProfileColor(int(i));
    draw->AddCircleFilled(r.GetCenter(), side * 0.5f, state.hovered ? Brighten(color) : color, 40);
    if (selected == int(i)) {
      DrawIcon(Icon::kCheck, ImVec2(r.GetCenter().x - Px(9.0f), r.GetCenter().y - Px(9.0f)), 18.0f, col::kBackground, 2.6f);
    }
    if (state.focused) FocusRing(r, size * 0.5f / col::kRoundness);
    if (state.pressed) pressed = int(i);
  }
  ImGui::PopID();
  return pressed;
}

void DrawStrip(ImVec2 pos, std::span<const StripPart> parts) {
  // As Play's: 44 dp high, the panel over the art with a hairline, each part 16 dp in, hairlines between.
  constexpr Type kPart{Font::kMedium, 15};
  float width = Px(8.0f);
  for (const StripPart& part : parts) width += Px(16.0f) * 2.0f + Px(18.0f) + Px(8.0f) + MeasureText(kPart, part.text).x;
  ImRect strip(pos, ImVec2(pos.x + width, pos.y + Px(44.0f)));
  FillRect(strip, col::kPanel, 12.0f);
  StrokeRect(strip, col::kHairline, 12.0f);
  float x = strip.Min.x + Px(4.0f);
  for (size_t i = 0; i < parts.size(); ++i) {
    float part_width = Px(16.0f) * 2.0f + Px(18.0f) + Px(8.0f) + MeasureText(kPart, parts[i].text).x;
    if (i > 0) {
      FillRect(ImRect(ImVec2(x, strip.Min.y + Px(13.0f)), ImVec2(x + Snap(Px(1.0f)), strip.Max.y - Px(13.0f))), col::kHairline);
    }
    DrawIcon(parts[i].icon, ImVec2(x + Px(16.0f), strip.GetCenter().y - Px(9.0f)), 18.0f, parts[i].icon_color, 1.75f);
    DrawLabel(kPart, ImVec2(x + Px(16.0f) + Px(18.0f) + Px(8.0f), strip.Min.y), col::kTextSoft, parts[i].text,
              strip.GetHeight());
    x += part_width;
  }
}

void App::LoadProfiles() {
  store_ = profiles::Load();
  if (store_.read_only) {
    // A newer launcher's store: this one only reads it (the game still runs with its current profile).
    profile_notice_ = t_.profiles_newer;
    profile_notice_error_ = true;
    return;
  }
  if (!store_.Current()) {
    OpenCreateProfile(true);
    return;
  }
  RegisterThisGame();
  CheckMigration();
  LoadProfileGames();
}

void App::CheckMigration() {
  const profiles::Profile* profile = store_.Current();
  std::filesystem::path user_data = GameUserDataFolder(exe_dir_);
  if (!profile || store_.read_only || profiles::IsMigrated(user_data, store_)) {
    return;
  }
  legacy_ = profiles::FindLegacy(user_data);
  if (legacy_.empty()) {
    return;
  }
  if (profiles::ProfileHasDataOf(*profile, legacy_)) {
    profile_dialog_ = ProfileDialog::kConflict;
    profile_dialog_opened_ = false;
    return;
  }
  RunMigration(profiles::Keep::kInstallation);
}

void App::RunMigration(profiles::Keep keep) {
  const profiles::Profile* profile = store_.Current();
  if (!profile) {
    return;
  }
  char note[1024];
  std::snprintf(note, sizeof(note), t_.saves_moved_note, PathUtf8(profile->folder).c_str());
  profiles::MigrationResult result =
      profiles::Migrate(GameUserDataFolder(exe_dir_), legacy_, *profile, keep, store_, note);
  char text[512];
  if (result.ok) {
    std::snprintf(text, sizeof(text), t_.migrated, game::kName);
    profile_notice_error_ = false;
  } else {
    std::snprintf(text, sizeof(text), t_.migrate_error, result.error.c_str());
    profile_notice_error_ = true;
  }
  profile_notice_ = text;
  // The unlocks are the profile's now.
  LoadAchievements();
  LoadProfileGames();
}

void App::OpenCreateProfile(bool first) {
  profile_dialog_ = ProfileDialog::kCreate;
  profile_dialog_opened_ = false;
  profile_first_ = first;
  new_name_ = first ? profiles::Clip(profiles::Trim(WindowsUserName()), profiles::kNameMax) : std::string();
  new_avatar_ = 0;
  new_color_ = 0;
  SetPhoto(std::nullopt);
  profile_error_.clear();
}

void App::BrowsePhoto() {
  static SDL_DialogFileFilter filters[1];
  filters[0] = {"JPG, PNG", "jpg;jpeg;png;bmp;gif;webp;heic"};
  auto* holder = new std::shared_ptr<DialogResult>(photo_dialog_);
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

void App::PollPhoto() {
  // The edit page takes the dialog's answer when it is open (page_profile_edit.cpp).
  if (profile_dialog_ != ProfileDialog::kCreate) {
    return;
  }
  std::optional<std::string> chosen;
  {
    std::lock_guard lock(photo_dialog_->mutex);
    chosen.swap(photo_dialog_->path);
  }
  if (!chosen) {
    return;
  }
  std::u8string path8(chosen->begin(), chosen->end());
  std::optional<Rgba> photo = LoadSquarePicture(std::filesystem::path(path8), kPhotoSize);
  if (!photo) {
    profile_error_ = t_.profile_photo_error;
    return;
  }
  profile_error_.clear();
  SetPhoto(std::move(photo));
  new_avatar_ = -1;
}

void App::SetPhoto(std::optional<Rgba> photo) {
  if (new_photo_texture_ != ImTextureID_Invalid) {
    gfx::DestroyTexture(new_photo_texture_);
    new_photo_texture_ = ImTextureID_Invalid;
  }
  new_photo_ = std::move(photo);
  if (new_photo_) {
    new_photo_texture_ = gfx::CreateTexture(new_photo_->pixels.data(), new_photo_->width, new_photo_->height);
  }
}

void App::DrawAvatar(ImVec2 pos, float size, std::string_view avatar, ImTextureID photo, int color) {
  float ring = size >= 96.0f ? 4.0f : size >= 48.0f ? 3.0f : 2.0f;
  float side = Px(size);
  ImVec2 center(pos.x + side * 0.5f, pos.y + side * 0.5f);
  ImGui::GetWindowDrawList()->AddCircleFilled(Snap(center), side * 0.5f, ProfileColor(color), 64);
  float inset = Px(ring);
  ImRect inner(ImVec2(pos.x + inset, pos.y + inset), ImVec2(pos.x + side - inset, pos.y + side - inset));
  if (avatar == profiles::kPhoto) {
    if (photo != ImTextureID_Invalid) {
      DrawTexture(photo, inner, true);
    } else {
      DrawSvg("photo", profiles::kPhotoPlaceholder, 100.0f, inner, true);
    }
  } else if (const profiles::Avatar* known = profiles::FindAvatar(avatar)) {
    DrawSvg(known->key, known->body, 100.0f, inner, true);
  } else {
    DrawSvg(profiles::kAvatars[0].key, profiles::kAvatars[0].body, 100.0f, inner, true);
  }
}

void App::ProfileDialogs(ImVec2 size) {
  PollPhoto();
  if (profile_dialog_ == ProfileDialog::kNone) {
    return;
  }
  constexpr const char* kPopup = "##profiles";
  if (!profile_dialog_opened_) {
    ImGui::OpenPopup(kPopup);
  }
  ImVec2 origin = ImGui::GetMainViewport()->Pos;
  ImGui::SetNextWindowPos(origin);
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 0));
  if (ImGui::BeginPopupModal(kPopup, nullptr,
                             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                 ImGuiWindowFlags_NoBackground)) {
    // Launcher 3.18: a flat veil over the header and the page; the footer stays, with the dialog's own hints.
    float footer_height = Snap(Px(52.0f));
    ImGui::GetWindowDrawList()->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y - footer_height),
                                              Hex(0x000000, 158));
    if (profile_dialog_ == ProfileDialog::kCreate) {
      CreateProfileCard(size);
    } else if (profile_dialog_ == ProfileDialog::kConflict) {
      ConflictCard(size);
    }
    std::vector<Hint> hints = PageHints();
    Footer(ImRect(ImVec2(origin.x, origin.y + size.y - footer_height), ImVec2(origin.x + size.x, origin.y + size.y)), hints);
    profile_dialog_opened_ = true;
    if (profile_dialog_ == ProfileDialog::kNone) {
      profile_dialog_opened_ = false;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(3);
}

void App::CreateProfileCard(ImVec2 size) {
  // Launcher 3.18 (the mockup's board 21): the chosen avatar beside the title and what a profile keeps; the name; the
  // avatars and the photo; the colors; Create (the main action) at the lower right, Cancel beside it after the first.
  constexpr Type kTitle{Font::kDisplayMedium, 30, 0.015f};
  constexpr Type kSubtitle{Font::kRegular, 15};
  constexpr Type kLabel{Font::kSemiBold, 15};
  constexpr Type kError{Font::kMedium, 13};
  constexpr float kWidth = 600.0f;
  constexpr float kPadding = 32.0f;
  constexpr float kAvatar = 64.0f;
  constexpr float kSwatch = 36.0f;
  const char* title = profile_first_ ? t_.profile_create_title : t_.profile_new_title;
  const float inner_width = Px(kWidth - kPadding * 2.0f);
  const float text_width = inner_width - Px(72.0f + 20.0f) - (profile_first_ ? 0.0f : Px(40.0f));
  const float subtitle_height = DrawParagraph(kSubtitle, ImVec2(0, 0), text_width, 1.5f, 0, t_.profile_create_text, false);
  const float header = std::max(Px(72.0f), LineHeight(kTitle) + Px(4.0f) + subtitle_height);
  const float label = LineHeight(kLabel);
  float height = Px(kPadding) + header + Px(28.0f) + label + Px(8.0f) + Px(48.0f) + Px(26.0f) + label + Px(12.0f) +
                 Px(kAvatar) + Px(26.0f) + label + Px(12.0f) + Px(kSwatch) + Px(30.0f) + Px(52.0f) + Px(kPadding);
  if (!profile_error_.empty()) {
    height += LineHeight(kError) + Px(12.0f);
  }
  ImRect card = DialogCard(size, kWidth, height);
  const float x = card.Min.x + Px(kPadding);
  float y = card.Min.y + Px(kPadding);

  // The header: the chosen avatar, the title and what a profile keeps.
  std::string_view avatar = new_avatar_ < 0 ? profiles::kPhoto : std::string_view(profiles::kAvatars[size_t(new_avatar_)].key);
  DrawAvatar(ImVec2(x, y + (header - Px(72.0f)) * 0.5f), 72.0f, avatar, new_photo_texture_, new_color_);
  float text_x = x + Px(72.0f + 20.0f);
  float text_y = y + (header - (LineHeight(kTitle) + Px(4.0f) + subtitle_height)) * 0.5f;
  DrawLabel(kTitle, ImVec2(text_x, text_y), col::kText, title);
  DrawParagraph(kSubtitle, ImVec2(text_x, text_y + LineHeight(kTitle) + Px(4.0f)), text_width, 1.5f, col::kTextMuted,
                t_.profile_create_text);
  if (!profile_first_) {
    ImRect close(ImVec2(card.Max.x - Px(kPadding + 32.0f), y), ImVec2(card.Max.x - Px(kPadding), y + Px(32.0f)));
    ItemState state = Item("##close", close);
    if (state.hovered) FillRect(close, col::kCardRaised, 8.0f);
    DrawIcon(Icon::kClose, ImVec2(close.Min.x + Px(6.0f), close.Min.y + Px(6.0f)), 20.0f, col::kTextMuted, 2.0f);
    if (state.focused) FocusRing(close, 8.0f);
    if (state.pressed || ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
        ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) {
      profile_dialog_ = ProfileDialog::kNone;
    }
  }
  y += header + Px(28.0f);

  // The name, with its count of characters.
  char count[16];
  std::snprintf(count, sizeof(count), "%zu / %zu", profiles::CountChars(new_name_), profiles::kNameMax);
  FieldLabel(ImVec2(x, y), inner_width, t_.profile_name, count);
  y += label + Px(8.0f);
  if (!profile_dialog_opened_) {
    ImGui::SetKeyboardFocusHere();
  }
  TextField("##profile_name", ImRect(ImVec2(x, y), ImVec2(x + inner_width, y + Px(48.0f))), &new_name_,
            profiles::kNameMax, t_.profile_name_hint);
  y += Px(48.0f) + Px(26.0f);

  // The avatars, and the photo.
  DrawLabel(kLabel, ImVec2(x, y), col::kTextSoft, t_.profile_avatar);
  y += label + Px(12.0f);
  int avatar_pressed = AvatarPicker("##avatars", ImVec2(x, y), kAvatar, 20.0f, new_avatar_, new_photo_texture_,
                                    t_.profile_photo);
  if (avatar_pressed >= 0) {
    if (avatar_pressed < int(profiles::kAvatars.size())) {
      new_avatar_ = avatar_pressed;
    } else if (new_photo_ && new_avatar_ >= 0) {
      new_avatar_ = -1;
    } else {
      BrowsePhoto();
    }
  }
  y += Px(kAvatar) + Px(26.0f);

  // The profile's color.
  DrawLabel(kLabel, ImVec2(x, y), col::kTextSoft, t_.profile_color);
  y += label + Px(12.0f);
  if (int color = ColorPicker("##colors", ImVec2(x, y), kSwatch, 20.0f, new_color_); color >= 0) {
    new_color_ = color;
  }
  y += Px(kSwatch) + Px(30.0f);

  if (!profile_error_.empty()) {
    DrawLabel(kError, ImVec2(x, y - Px(14.0f)), col::kError, Ellipsize(kError, profile_error_, inner_width));
    y += LineHeight(kError) + Px(12.0f);
  }

  // The buttons: Create (A), Cancel after the first profile.
  const std::string name = profiles::Trim(new_name_);
  KeyButtonSpec create{KeyLook::kPrimary, 'A', Type{Font::kDisplaySemiBold, 22, 0.02f}, 52, 24, 14, 26, 12};
  float create_width = KeyButtonWidth(create, t_.profile_create);
  float right = card.Max.x - Px(kPadding);
  if (KeyButton("##create_profile", ImVec2(right - create_width, y), create, t_.profile_create, !name.empty()).pressed) {
    std::string avatar_key = new_avatar_ < 0 ? std::string(profiles::kPhoto) : profiles::kAvatars[size_t(new_avatar_)].key;
    profiles::Profile* profile = profiles::Create(store_, name, avatar_key, new_color_, RECOMP_LAUNCHER_VERSION_STR);
    if (!profile) {
      char text[256];
      std::snprintf(text, sizeof(text), t_.profile_save_error, PathUtf8(store_.root).c_str());
      profile_error_ = text;
    } else {
      if (new_avatar_ < 0 && new_photo_ && !SavePng(profile->folder / L"avatar.png", *new_photo_)) {
        profile->avatar = profiles::kAvatars[0].key;
        profiles::SaveProfile(store_, *profile);
      }
      profile_dialog_ = ProfileDialog::kNone;
      SetPhoto(std::nullopt);
      RegisterThisGame();
      CheckMigration();
      // The new profile is the current one: the pages are its.
      LoadAchievements();
      LoadProfileGames();
    }
  }
  if (name.empty()) {
    constexpr Type kNeeded{Font::kRegular, 14};
    float needed_x = right - create_width - Px(14.0f) - MeasureText(kNeeded, t_.name_needed).x;
    if (profile_first_ && needed_x > x) {
      DrawLabel(kNeeded, ImVec2(needed_x, y), col::kTextMuted, t_.name_needed, Px(52.0f));
    }
  }
  if (!profile_first_) {
    ButtonSpec cancel{ButtonKind::kGhost, Type{Font::kSemiBold, 16}, 44, 20, 12};
    float cancel_width = ButtonWidth(cancel, t_.cancel);
    if (Button("##cancel_profile", ImVec2(right - create_width - Px(14.0f) - cancel_width, y + Px(4.0f)), cancel, t_.cancel)) {
      profile_dialog_ = ProfileDialog::kNone;
      SetPhoto(std::nullopt);
    }
  }
}

void App::ConflictCard(ImVec2 size) {
  constexpr Type kTitle{Font::kDisplayBold, 28};
  constexpr Type kText{Font::kRegular, 15};
  constexpr float kWidth = 560.0f;
  constexpr float kPadding = 28.0f;
  const float inner_width = Px(kWidth - kPadding * 2.0f);
  char title[256];
  std::snprintf(title, sizeof(title), t_.migrate_conflict_title, game::kName);
  std::string shown = Title(title);
  const float title_height = DrawParagraph(kTitle, ImVec2(0, 0), inner_width, 1.15f, 0, shown, false);
  const float text_height = DrawParagraph(kText, ImVec2(0, 0), inner_width, 1.5f, 0, t_.migrate_conflict_text, false);
  float height = Px(kPadding) + title_height + Px(12.0f) + text_height + Px(28.0f) + Px(46.0f) + Px(kPadding);
  ImRect card = DialogCard(size, kWidth, height);
  float x = card.Min.x + Px(kPadding);
  float y = card.Min.y + Px(kPadding);
  DrawParagraph(kTitle, ImVec2(x, y), inner_width, 1.15f, col::kText, shown);
  y += title_height + Px(12.0f);
  DrawParagraph(kText, ImVec2(x, y), inner_width, 1.5f, col::kTextMuted, t_.migrate_conflict_text);
  y += text_height + Px(28.0f);

  ButtonSpec keep_profile;
  keep_profile.kind = ButtonKind::kPrimary;
  keep_profile.type = Type{Font::kBold, 15};
  keep_profile.height = 46;
  ButtonSpec keep_installation;
  keep_installation.kind = ButtonKind::kGhost;
  keep_installation.height = 46;
  float right = card.Max.x - Px(kPadding);
  float profile_width = ButtonWidth(keep_profile, t_.migrate_keep_profile);
  float installation_width = ButtonWidth(keep_installation, t_.migrate_keep_installation);
  if (!profile_dialog_opened_) {
    ImGui::SetKeyboardFocusHere();
  }
  if (Button("##keep_profile", ImVec2(right - profile_width, y), keep_profile, t_.migrate_keep_profile)) {
    profile_dialog_ = ProfileDialog::kNone;
    RunMigration(profiles::Keep::kProfile);
  }
  if (Button("##keep_installation", ImVec2(right - profile_width - Px(10.0f) - installation_width, y), keep_installation,
             t_.migrate_keep_installation)) {
    profile_dialog_ = ProfileDialog::kNone;
    RunMigration(profiles::Keep::kInstallation);
  }
}

}  // namespace launcher
