// Editing the current profile, or creating another (Gustavo's 2026-10-28 mockup, "Editar perfil" and "Crear perfil"):
// a preview card left as the launchers show the profile, the form right (the name, the motto, the avatar or a photo,
// the ring's color); editing also deletes it (to the Recycle Bin, with its saves; not the last profile).

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
  Navigate(Page::kProfileEdit, false);
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
  Navigate(Page::kProfileEdit, false);
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

  // The layout: 24 40 20 dp of padding, the preview 380 dp wide, 24 dp to the form.
  ImRect main(ImVec2(rect.Min.x + Px(40.0f), rect.Min.y + Px(24.0f)), ImVec2(rect.Max.x - Px(40.0f), rect.Max.y - Px(20.0f)));
  ImRect preview(main.Min, ImVec2(main.Min.x + Px(380.0f), main.Max.y));
  ImRect form(ImVec2(preview.Max.x + Px(24.0f), main.Min.y), main.Max);

  // The preview: the avatar, the name and motto (placeholders while creating), the level.
  {
    FillRect(preview, col::kPanel, 14.0f);
    StrokeRect(preview, col::kHairline, 14.0f);
    constexpr Type kKicker{Font::kBold, 12, 0.14f};
    DrawLabel(kKicker, ImVec2(preview.Min.x + Px(28.0f), preview.Min.y + Px(28.0f)), col::kAccent, Upper(t_.preview_kicker));
    const float avatar = 180.0f;
    float cy = preview.Min.y + Px(74.0f);
    DrawAvatar(ImVec2(preview.GetCenter().x - Px(avatar) * 0.5f, cy), avatar, avatar_key, photo, edit_color_);
    cy += Px(avatar) + Px(22.0f);
    constexpr Type kName{Font::kDisplayBold, 34};
    constexpr Type kMotto{Font::kRegular, 15};
    float width = preview.GetWidth() - Px(40.0f);
    std::string shown = Ellipsize(kName, Upper(name.empty() ? std::string(t_.your_name) : name), width);
    DrawLabel(kName, ImVec2(preview.GetCenter().x - MeasureText(kName, shown).x * 0.5f, cy),
              name.empty() ? col::kTabText : col::kText, shown);
    cy += LineHeight(kName) - Px(4.0f);
    std::string motto = Ellipsize(kMotto, creating && edit_motto_.empty() ? std::string(t_.new_profile_sub) : edit_motto_, width);
    DrawLabel(kMotto, ImVec2(preview.GetCenter().x - MeasureText(kMotto, motto).x * 0.5f, cy), col::kTextSoft, motto);
    cy += LineHeight(kMotto) + Px(14.0f);
    char level[48];
    std::snprintf(level, sizeof(level), t_.level, creating ? 1 : Totals().Level());
    constexpr Type kLevel{Font::kBold, 13, 0.06f};
    std::string badge = Upper(level);
    float badge_width = MeasureText(kLevel, badge).x + Px(20.0f);
    ImRect chip(ImVec2(preview.GetCenter().x - badge_width * 0.5f, cy),
                ImVec2(preview.GetCenter().x + badge_width * 0.5f, cy + LineHeight(kLevel) + Px(8.0f)));
    FillRect(chip, col::kAccent, 4.0f);
    DrawLabel(kLevel, ImVec2(chip.Min.x + Px(10.0f), chip.Min.y), col::kOnAccent, badge, chip.GetHeight());
    constexpr Type kNote{Font::kRegular, 13};
    DrawLabel(kNote, ImVec2(preview.GetCenter().x - MeasureText(kNote, t_.profile_preview_note).x * 0.5f,
                            preview.Max.y - Px(28.0f) - LineHeight(kNote)),
              col::kTabText, t_.profile_preview_note);
  }

  // The form.
  FillRect(form, col::kPanel, 14.0f);
  StrokeRect(form, col::kHairline, 14.0f);
  float px = form.Min.x + Px(32.0f);
  float inner = form.GetWidth() - Px(64.0f);
  float py = form.Min.y + Px(28.0f);
  constexpr Type kTitle{Font::kDisplayBold, 44};
  DrawLabel(kTitle, ImVec2(px, py), col::kText, Upper(creating ? t_.create_profile : t_.edit_profile), Px(44.0f));
  py += Px(44.0f) + Px(20.0f);
  constexpr Type kLabel{Font::kBold, 14};
  constexpr Type kCount{Font::kRegular, 13};
  float label = LineHeight(kLabel);
  {
    // The name and the motto, side by side.
    float field_gap = Px(20.0f);
    float half = (inner - field_gap) * 0.5f;
    auto field = [&](float x, const char* title, std::string* value, size_t max, const char* hint, const char* id) {
      DrawLabel(kLabel, ImVec2(x, py), col::kText, title);
      char count[16];
      std::snprintf(count, sizeof(count), "%zu / %zu", profiles::CountChars(*value), max);
      DrawLabel(kCount, ImVec2(x + half - MeasureText(kCount, count).x, py), col::kTabText, count, label);
      TextField(id, ImRect(ImVec2(x, py + label + Px(8.0f)), ImVec2(x + half, py + label + Px(8.0f) + Px(48.0f))),
                value, max, hint);
    };
    field(px, t_.profile_name, &edit_name_, profiles::kNameMax, t_.profile_name_hint, "##edit_name");
    field(px + half + field_gap, t_.motto_label, &edit_motto_, profiles::kMottoMax, t_.motto_hint, "##edit_motto");
    py += label + Px(8.0f) + Px(48.0f) + Px(24.0f);
  }
  ImDrawList* draw = ImGui::GetWindowDrawList();
  {
    // The avatars, and the photo.
    DrawLabel(kLabel, ImVec2(px, py), col::kText, t_.profile_avatar);
    py += label + Px(12.0f);
    const float button = Px(88.0f);
    const float bgap = Px(12.0f);
    for (size_t i = 0; i <= profiles::kAvatars.size(); ++i) {
      bool is_photo = i == profiles::kAvatars.size();
      ImRect r(ImVec2(px + float(i) * (button + bgap), py), ImVec2(px + float(i) * (button + bgap) + button, py + button));
      char id[32];
      std::snprintf(id, sizeof(id), "##edit_avatar%zu", i);
      ItemState state = Item(id, r);
      bool selected = is_photo ? edit_avatar_ < 0 : edit_avatar_ == int(i);
      ImVec2 center = r.GetCenter();
      if (selected) {
        draw->AddCircle(center, button * 0.5f - Px(1.5f), col::kAccent, 64, Px(3.0f));
      } else if (state.hovered) {
        draw->AddCircle(center, button * 0.5f - Px(1.5f), Hex(0xFFFFFF, 46), 64, Px(3.0f));
      }
      ImRect picture(ImVec2(r.Min.x + Px(8.0f), r.Min.y + Px(8.0f)), ImVec2(r.Max.x - Px(8.0f), r.Max.y - Px(8.0f)));
      if (!is_photo) {
        DrawSvg(profiles::kAvatars[i].key, profiles::kAvatars[i].body, 100.0f, picture, true);
      } else if (has_photo) {
        DrawTexture(photo, picture, true);
      } else {
        draw->AddCircleFilled(center, button * 0.5f - Px(1.0f), Hex(0xFFFFFF, 10), 64);
        DashedRect(ImRect(ImVec2(r.Min.x + Px(1.0f), r.Min.y + Px(1.0f)), ImVec2(r.Max.x - Px(1.0f), r.Max.y - Px(1.0f))),
                   Hex(0xFFFFFF, 64), 88.0f * 0.5f / col::kRoundness);
        DrawIcon(Icon::kCamera, ImVec2(center.x - Px(9.0f), center.y - Px(18.0f)), 18.0f, col::kTextSoft, 2.0f);
        constexpr Type kSmall{Font::kBold, 11};
        std::string caption = Ellipsize(kSmall, t_.profile_photo, button - Px(10.0f));
        DrawLabel(kSmall, ImVec2(center.x - MeasureText(kSmall, caption).x * 0.5f, center.y + Px(4.0f)), col::kTextSoft,
                  caption);
      }
      if (state.focused) FocusRing(r, 88.0f * 0.5f / col::kRoundness);
      if (state.pressed) {
        if (!is_photo) {
          edit_avatar_ = int(i);
        } else if (has_photo && edit_avatar_ >= 0) {
          edit_avatar_ = -1;
        } else {
          BrowsePhoto();
        }
      }
    }
    py += button + Px(24.0f);
  }
  {
    // The ring's color.
    DrawLabel(kLabel, ImVec2(px, py), col::kText, t_.ring_color);
    py += label + Px(12.0f);
    const float swatch = Px(48.0f);
    const float sgap = Px(8.0f);
    for (size_t i = 0; i < profiles::kColors.size(); ++i) {
      ImRect r(ImVec2(px + float(i) * (swatch + sgap), py), ImVec2(px + float(i) * (swatch + sgap) + swatch, py + swatch));
      char id[32];
      std::snprintf(id, sizeof(id), "##edit_color%zu", i);
      ItemState state = Item(id, r);
      draw->AddCircleFilled(r.GetCenter(), swatch * 0.5f - Px(4.0f), ProfileColor(int(i)), 48);
      if (edit_color_ == int(i)) {
        draw->AddCircle(r.GetCenter(), swatch * 0.5f - Px(1.5f), col::kText, 48, Px(3.0f));
      } else if (state.hovered) {
        draw->AddCircle(r.GetCenter(), swatch * 0.5f - Px(1.5f), Hex(0xFFFFFF, 46), 48, Px(3.0f));
      }
      if (state.focused) FocusRing(r, 48.0f * 0.5f / col::kRoundness);
      if (state.pressed) edit_color_ = int(i);
    }
  }

  // The foot: Save (or Create) and Cancel; Delete right while editing; a message above.
  float foot = form.Max.y - Px(28.0f) - Px(56.0f);
  KeyButtonSpec save{KeyLook::kPrimary, 'A', Type{Font::kDisplayBold, 20, 0.05f}, 56, 28, 10, 26, 12};
  KeyButtonSpec cancel{KeyLook::kGhost, 0, Type{Font::kSemiBold, 16}, 56, 22};
  std::string save_label = Upper(creating ? t_.create_profile : t_.save);
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
  bx += KeyButtonWidth(save, save_label) + Px(14.0f);
  if (name.empty()) {
    constexpr Type kNeeded{Font::kRegular, 14};
    DrawLabel(kNeeded, ImVec2(bx, foot), col::kTabText, t_.name_needed, Px(56.0f));
    bx += MeasureText(kNeeded, t_.name_needed).x + Px(14.0f);
  }
  if (KeyButton("##cancel_profile", ImVec2(bx, foot), cancel, t_.cancel).pressed) {
    Navigate(edit_back_, false);
    return;
  }
  bool last = store_.profiles.size() < 2;
  ImRect delete_rect;
  if (!creating) {
    KeyButtonSpec del{KeyLook::kDanger, 0, Type{Font::kSemiBold, 15}, 44, 18, 8};
    const char* del_text = edit_delete_armed_ ? t_.delete_profile_confirm : t_.delete_profile;
    ImVec2 pos(px + inner - KeyButtonWidth(del, del_text), foot + Px(6.0f));
    delete_rect = ImRect(pos, ImVec2(pos.x + KeyButtonWidth(del, del_text), pos.y + Px(44.0f)));
    if (KeyButton("##delete_profile", pos, del, del_text, !last).pressed) {
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
  }
  // Why the last profile cannot be deleted, while the pointer is on its button; otherwise the last message.
  bool last_hint = !creating && last && ImGui::IsMouseHoveringRect(delete_rect.Min, delete_rect.Max);
  if (last_hint || !edit_message_.empty()) {
    constexpr Type kMessage{Font::kMedium, 13};
    DrawLabel(kMessage, ImVec2(px, foot - Px(12.0f) - LineHeight(kMessage)),
              last_hint || edit_message_error_ ? col::kError : col::kSuccess,
              Ellipsize(kMessage, last_hint ? std::string(t_.delete_last_profile) : edit_message_, inner));
  }
}

}  // namespace launcher
