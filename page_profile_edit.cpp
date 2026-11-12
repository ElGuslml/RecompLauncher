// Editing the current profile, or creating another (launcher 3.18, Gustavo's 2026-11-11 mockup "Camerino", boards 19
// and 20): a preview at the left as the launchers show the profile, the form at the right (the name, the motto, the
// avatar or a photo, the ring's color); editing also deletes it (to the Recycle Bin, with its saves; not the last
// profile, which says why beside its button).

#include <algorithm>
#include <cstdio>
#include <string>

#include "app.h"
#include "launcher_version.h"
#include "pages.h"
#include "profile_art.h"
#include "ui/gfx.h"

namespace launcher {

using namespace ui;

namespace {

ImU32 ProfileColor(int index) {
  uint32_t rgb = profiles::kColors[size_t(std::clamp(index, 0, int(profiles::kColors.size()) - 1))];
  return IM_COL32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, 0xFF);
}

std::string Utf8(const std::filesystem::path& path) {
  std::u8string text = path.u8string();
  return std::string(text.begin(), text.end());
}

}  // namespace

void App::OpenProfileEdit() {
  const profiles::Profile* profile = store_.Current();
  if (!profile || store_.read_only) {
    return;
  }
  edit_back_ = page_ == Page::kProfile ? Page::kProfile : Page::kProfiles;
  edit_id_ = profile->id;
  edit_name_ = profile->name;
  edit_motto_ = profile->motto;
  edit_avatar_ = profile->avatar == profiles::kPhoto ? -1 : 0;
  for (size_t i = 0; i < profiles::kAvatars.size(); ++i) {
    if (profile->avatar == profiles::kAvatars[i].key) edit_avatar_ = int(i);
  }
  edit_color_ = profile->color;
  edit_photo_.reset();
  if (edit_photo_texture_ != ImTextureID_Invalid) {
    gfx::DestroyTexture(edit_photo_texture_);
    edit_photo_texture_ = ImTextureID_Invalid;
  }
  edit_delete_armed_ = false;
  edit_message_.clear();
  Navigate(Page::kProfileEdit, true);  // the controller on its first control (the avatars: the fields type)
}

void App::OpenProfileCreate() {
  if (store_.read_only) {
    return;
  }
  edit_back_ = page_ == Page::kProfile ? Page::kProfile : Page::kProfiles;
  edit_id_.clear();
  edit_name_.clear();
  edit_motto_.clear();
  edit_avatar_ = int(profiles::kAvatars.size()) - 1;
  edit_color_ = int(store_.profiles.size() % profiles::kColors.size());
  edit_photo_.reset();
  if (edit_photo_texture_ != ImTextureID_Invalid) {
    gfx::DestroyTexture(edit_photo_texture_);
    edit_photo_texture_ = ImTextureID_Invalid;
  }
  edit_delete_armed_ = false;
  edit_message_.clear();
  Navigate(Page::kProfileEdit, true);
}

void App::ProfileEditPage(const ImRect& rect) {
  bool creating = edit_id_.empty();
  profiles::Profile* profile = creating ? nullptr : store_.Find(store_.current);
  if ((!creating && !profile) || store_.read_only) {
    Navigate(store_.Current() ? Page::kProfiles : Page::kPlay, false);
    return;
  }
  // Another profile became the current one while editing: edit that one.
  if (!creating && profile->id != edit_id_) {
    OpenProfileEdit();
    return;
  }
  // A photo chosen with the file dialog (page_profile.cpp's BrowsePhoto shares its result); the create dialog's while
  // it is open over this page.
  if (profile_dialog_ != ProfileDialog::kCreate) {
    std::optional<std::string> chosen;
    {
      std::lock_guard lock(photo_dialog_->mutex);
      chosen.swap(photo_dialog_->path);
    }
    if (chosen) {
      std::u8string path8(chosen->begin(), chosen->end());
      if (std::optional<Rgba> photo = LoadSquarePicture(std::filesystem::path(path8), 256)) {
        if (edit_photo_texture_ != ImTextureID_Invalid) gfx::DestroyTexture(edit_photo_texture_);
        edit_photo_ = std::move(photo);
        edit_photo_texture_ = gfx::CreateTexture(edit_photo_->pixels.data(), edit_photo_->width, edit_photo_->height);
        edit_avatar_ = -1;
        edit_message_.clear();
      } else {
        edit_message_ = t_.profile_photo_error;
        edit_message_error_ = true;
      }
    }
  }
  ImTextureID photo = edit_photo_texture_ != ImTextureID_Invalid ? edit_photo_texture_
                      : profile                                  ? AvatarTexture(*profile)
                                                                 : ImTextureID_Invalid;
  bool has_photo = photo != ImTextureID_Invalid;
  std::string avatar_key =
      edit_avatar_ < 0 ? std::string(profiles::kPhoto) : std::string(profiles::kAvatars[size_t(edit_avatar_)].key);
  std::string name = profiles::Trim(edit_name_);

  // --- The layout (launcher 3.18, the mockup's boards 19 and 20; dp from the page's top left): the preview centred in
  // a column from 56, 344 wide; a hairline at 432; the form from 472 to 56 from the right edge. No cards. ---
  const float page_left = rect.Min.x;
  const float top = rect.Min.y;
  FillRect(ImRect(ImVec2(page_left + Px(432.0f), top + Px(28.0f)), ImVec2(page_left + Px(432.0f) + Snap(Px(1.0f)), rect.Max.y - Px(20.0f))),
           col::kHairline);

  // The preview: the avatar (176 dp, its ring), the name and motto (placeholders while creating), the level.
  {
    const float column_left = page_left + Px(56.0f);
    const float column_width = Px(344.0f);
    const float cx = column_left + column_width * 0.5f;
    const float avatar = 176.0f;
    float cy = top + Px(48.0f);
    DrawAvatar(ImVec2(cx - Px(avatar) * 0.5f, cy), avatar, avatar_key, photo, edit_color_);
    cy += Px(avatar) + Px(26.0f);
    constexpr Type kName{Font::kDisplayMedium, 38, 0.015f};
    constexpr Type kMotto{Font::kRegular, 15};
    constexpr Type kLevel{Font::kSemiBold, 14};
    std::string shown = Ellipsize(kName, name.empty() ? std::string(t_.your_name) : name, column_width);
    DrawLabel(kName, ImVec2(cx - MeasureText(kName, shown).x * 0.5f, cy), name.empty() ? col::kTextMuted : col::kText, shown);
    cy += LineHeight(kName) + Px(2.0f);
    std::string motto =
        Ellipsize(kMotto, creating && edit_motto_.empty() ? std::string(t_.new_profile_sub) : edit_motto_, column_width);
    DrawLabel(kMotto, ImVec2(cx - MeasureText(kMotto, motto).x * 0.5f, cy), col::kTextMuted, motto);
    cy += LineHeight(kMotto) + Px(18.0f);
    char level[48];
    std::snprintf(level, sizeof(level), t_.level, creating ? 1 : Totals().Level());
    DrawLabel(kLevel, ImVec2(cx - MeasureText(kLevel, level).x * 0.5f, cy), col::kTextSoft, level);
    constexpr Type kNote{Font::kRegular, 14};
    DrawLabel(kNote, ImVec2(cx - MeasureText(kNote, t_.profile_preview_note).x * 0.5f, rect.Max.y - Px(48.0f) - LineHeight(kNote)),
              col::kTextDim, t_.profile_preview_note);
  }

  // The form: the title, the name and the motto side by side, the avatars, the ring's color, the buttons.
  const float px = page_left + Px(472.0f);
  const float inner = rect.Max.x - Px(56.0f) - px;
  constexpr Type kTitle{Font::kDisplaySemiBold, 44, 0.015f};
  DrawLabel(kTitle, ImVec2(px, top + Px(20.0f)), col::kText, Title(creating ? t_.create_profile : t_.edit_profile),
            Px(44.0f * 1.1f));
  constexpr Type kLabel{Font::kSemiBold, 15};
  const float label = LineHeight(kLabel);
  float py = top + Px(104.0f);
  {
    float field_gap = Px(20.0f);
    float half = (inner - field_gap) * 0.5f;
    auto field = [&](float x, const char* title, std::string* value, size_t max, const char* hint, const char* id) {
      char count[16];
      std::snprintf(count, sizeof(count), "%zu / %zu", profiles::CountChars(*value), max);
      FieldLabel(ImVec2(x, py), half, title, count);
      TextField(id, ImRect(ImVec2(x, py + label + Px(8.0f)), ImVec2(x + half, py + label + Px(8.0f) + Px(48.0f))), value,
                max, hint);
    };
    field(px, t_.profile_name, &edit_name_, profiles::kNameMax, t_.profile_name_hint, "##edit_name");
    field(px + half + field_gap, t_.motto_label, &edit_motto_, profiles::kMottoMax, t_.motto_hint, "##edit_motto");
  }
  py = top + Px(202.0f);
  {
    DrawLabel(kLabel, ImVec2(px, py), col::kTextSoft, t_.profile_avatar);
    py += label + Px(14.0f);
    int pressed = AvatarPicker("##edit_avatars", ImVec2(px, py), 72.0f, 28.0f, edit_avatar_, has_photo ? photo : ImTextureID_Invalid,
                               t_.profile_photo);
    if (pressed >= 0) {
      if (pressed < int(profiles::kAvatars.size())) {
        edit_avatar_ = pressed;
      } else if (has_photo && edit_avatar_ >= 0) {
        edit_avatar_ = -1;
      } else {
        BrowsePhoto();
      }
    }
  }
  py = top + Px(348.0f);
  {
    DrawLabel(kLabel, ImVec2(px, py), col::kTextSoft, t_.ring_color);
    py += label + Px(14.0f);
    if (int color = ColorPicker("##edit_colors", ImVec2(px, py), 36.0f, 20.0f, edit_color_); color >= 0) {
      edit_color_ = color;
    }
  }

  // The foot: Save (or Create, the main action), Cancel; Delete at the right while editing; a message above.
  float foot = rect.Max.y - Px(40.0f) - Px(56.0f);
  KeyButtonSpec save{KeyLook::kPrimary, 'A', Type{Font::kDisplaySemiBold, 24, 0.02f}, 56, 28, 14, 26, 12};
  ButtonSpec cancel{ButtonKind::kGhost, Type{Font::kSemiBold, 16}, 44, 20, 12};
  const char* save_label = creating ? t_.create_profile : t_.save;
  float bx = px;
  if (KeyButton("##save_profile", ImVec2(bx, foot), save, save_label, !name.empty()).pressed) {
    if (creating) {
      profiles::Profile* created = profiles::Create(store_, name, avatar_key == profiles::kPhoto ? std::string(profiles::kAvatars[0].key) : avatar_key,
                                                    edit_color_, RECOMP_LAUNCHER_VERSION_STR);
      if (!created) {
        char text[256];
        std::snprintf(text, sizeof(text), t_.profile_save_error, Utf8(store_.root).c_str());
        edit_message_ = text;
        edit_message_error_ = true;
      } else {
        created->motto = profiles::Clip(profiles::Trim(edit_motto_), profiles::kMottoMax);
        if (edit_avatar_ < 0 && edit_photo_ && SavePng(created->folder / L"avatar.png", *edit_photo_)) {
          created->avatar = profiles::kPhoto;
        }
        profiles::SaveProfile(store_, *created);
        profiles_selected_ = created->id;
        RegisterThisGame();
        CheckMigration();
        // The new profile is the current one: the pages are its.
        LoadAchievements();
        LoadProfileGames();
        Navigate(Page::kProfiles, false);
        return;
      }
    } else {
      profile->name = profiles::Clip(name, profiles::kNameMax);
      profile->motto = profiles::Clip(profiles::Trim(edit_motto_), profiles::kMottoMax);
      profile->color = edit_color_;
      bool photo_saved = true;
      if (edit_avatar_ < 0 && edit_photo_) {
        photo_saved = SavePng(profile->folder / L"avatar.png", *edit_photo_);
        ForgetAvatar(profile->id);
      }
      profile->avatar = edit_avatar_ < 0 && photo_saved && (edit_photo_ || has_photo)
                            ? std::string(profiles::kPhoto)
                            : (edit_avatar_ < 0 ? std::string(profiles::kAvatars[0].key) : avatar_key);
      if (profiles::SaveProfile(store_, *profile)) {
        edit_photo_.reset();
        Navigate(edit_back_, false);
        return;
      }
      char text[256];
      std::snprintf(text, sizeof(text), t_.profile_save_error, "profile.toml");
      edit_message_ = text;
      edit_message_error_ = true;
    }
  }
  bx += KeyButtonWidth(save, save_label) + Px(18.0f);
  if (name.empty()) {
    constexpr Type kNeeded{Font::kRegular, 14};
    DrawLabel(kNeeded, ImVec2(bx, foot), col::kTextMuted, t_.name_needed, Px(56.0f));
    bx += MeasureText(kNeeded, t_.name_needed).x + Px(18.0f);
  }
  float right = px + inner;
  if (creating) {
    // Creating: Cancel at the right (the mockup's board 20).
    if (Button("##cancel_profile", ImVec2(right - ButtonWidth(cancel, t_.cancel), foot + Px(6.0f)), cancel, t_.cancel)) {
      Navigate(edit_back_, false);
      return;
    }
  } else if (Button("##cancel_profile", ImVec2(bx, foot + Px(6.0f)), cancel, t_.cancel)) {
    Navigate(edit_back_, false);
    return;
  }
  bool last = store_.profiles.size() < 2;
  if (!creating) {
    // Delete: pressed twice; never the last profile, which says why beside it.
    ButtonSpec del{last ? ButtonKind::kDisabled : ButtonKind::kDanger, Type{Font::kSemiBold, 16}, 44, 20, 12};
    const char* del_text = edit_delete_armed_ ? t_.delete_profile_confirm : t_.delete_profile;
    float del_width = ButtonWidth(del, del_text);
    if (Button("##delete_profile", ImVec2(right - del_width, foot + Px(6.0f)), del, del_text, !last)) {
      if (!edit_delete_armed_) {
        edit_delete_armed_ = true;
      } else {
        std::string id = profile->id;
        ForgetAvatar(id);
        if (profiles::Delete(store_, id, RECOMP_LAUNCHER_VERSION_STR)) {
          edit_delete_armed_ = false;
          LoadAchievements();
          LoadProfileGames();
          Navigate(Page::kProfiles, false);
          return;
        }
        edit_message_ = t_.delete_failed;
        edit_message_error_ = true;
        edit_delete_armed_ = false;
      }
    }
    if (last) {
      constexpr Type kWhy{Font::kRegular, 13};
      std::string why = Ellipsize(kWhy, t_.delete_last_profile, inner * 0.5f);
      DrawLabel(kWhy, ImVec2(right - MeasureText(kWhy, why).x, foot + Px(6.0f) + Px(44.0f) + Px(8.0f)), col::kTextDim, why);
    }
  }
  if (!edit_message_.empty()) {
    constexpr Type kMessage{Font::kMedium, 13};
    DrawLabel(kMessage, ImVec2(px, foot - Px(12.0f) - LineHeight(kMessage)), edit_message_error_ ? col::kError : col::kSuccess,
              Ellipsize(kMessage, edit_message_, inner));
  }
}

}  // namespace launcher
