// Editing the current profile (the profiles mockup's "Editar perfil"): a preview card left as the launchers show the
// profile, the form right (the name, the motto, the avatar or a photo, the color), and deleting it (to the Recycle
// Bin, with its saves; not the last profile).

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

}  // namespace

void App::OpenProfileEdit() {
  const profiles::Profile* profile = store_.Current();
  if (!profile || store_.read_only) {
    return;
  }
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

void App::ProfileEditPage(const ImRect& rect) {
  profiles::Profile* profile = store_.Find(store_.current);
  if (!profile || store_.read_only) {
    Navigate(profile ? Page::kProfile : Page::kPlay, false);
    return;
  }
  // Another profile became the current one (switched or created from the sidebar's menu): edit that one.
  if (profile->id != edit_id_) {
    OpenProfileEdit();
    return;
  }
  // A photo chosen with the file dialog (page_profile.cpp's BrowsePhoto shares its result); the create dialog's
  // while it is open over this page.
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
  ImTextureID photo = edit_photo_texture_ != ImTextureID_Invalid ? edit_photo_texture_ : AvatarTexture(*profile);
  bool has_photo = photo != ImTextureID_Invalid;
  std::string avatar_key =
      edit_avatar_ < 0 ? std::string(profiles::kPhoto) : std::string(profiles::kAvatars[size_t(edit_avatar_)].key);

  ImRect content = PageContent(rect);
  float x = content.Min.x;
  float y = content.Min.y;
  // Back to the profile, the title.
  {
    constexpr Type kBack{Font::kSemiBold, 15};
    ImRect back(ImVec2(x - Px(6.0f), y), ImVec2(x + Px(24.0f) + MeasureText(kBack, t_.back_my_profile).x + Px(6.0f), y + Px(32.0f)));
    ItemState state = Item("##back", back);
    DrawIcon(Icon::kArrowLeft, ImVec2(x, y + Px(7.0f)), 18.0f, state.hovered ? col::kText : col::kTextMuted, 2.0f);
    DrawLabel(kBack, ImVec2(x + Px(24.0f), y), state.hovered ? col::kText : col::kTextSoft, t_.back_my_profile, Px(32.0f));
    if (state.focused) FocusRing(back, 8.0f);
    if (state.pressed) Navigate(Page::kProfile, false);
    y += Px(32.0f) + Px(6.0f);
    constexpr Type kTitle{Font::kCondensedBold, 40};
    DrawLabel(kTitle, ImVec2(x, y), col::kText, Title(t_.edit_profile));
    y += LineHeight(kTitle) + Px(20.0f);
  }

  // The preview, left.
  const float preview_width = Px(320.0f);
  const float gap = Px(24.0f);
  {
    float height = Px(300.0f);
    ImRect card(ImVec2(x, y), ImVec2(x + preview_width, y + height));
    FillRect(card, col::kCard, 14.0f);
    FillRect(ImRect(card.Min, ImVec2(card.Max.x, card.Min.y + Px(80.0f))), ProfileColor(edit_color_), 14.0f,
             ImDrawFlags_RoundCornersTop);
    const float avatar = 104.0f;
    ImVec2 avatar_pos(card.GetCenter().x - Px(avatar * 0.5f), card.Min.y + Px(80.0f - avatar * 0.5f));
    ImGui::GetWindowDrawList()->AddCircleFilled(
        ImVec2(avatar_pos.x + Px(avatar * 0.5f), avatar_pos.y + Px(avatar * 0.5f)), Px(avatar * 0.5f + 5.0f), col::kCard, 64);
    DrawAvatar(avatar_pos, avatar, avatar_key, photo, edit_color_);
    float cy = avatar_pos.y + Px(avatar) + Px(14.0f);
    constexpr Type kName{Font::kCondensedBold, 26};
    constexpr Type kMotto{Font::kRegular, 14};
    std::string name = profiles::Trim(edit_name_).empty() ? profile->name : profiles::Trim(edit_name_);
    std::string shown = Ellipsize(kName, name, card.GetWidth() - Px(32.0f));
    DrawLabel(kName, ImVec2(card.GetCenter().x - MeasureText(kName, shown).x * 0.5f, cy), col::kText, shown);
    cy += LineHeight(kName) + Px(2.0f);
    if (!edit_motto_.empty()) {
      std::string motto = Ellipsize(kMotto, edit_motto_, card.GetWidth() - Px(32.0f));
      DrawLabel(kMotto, ImVec2(card.GetCenter().x - MeasureText(kMotto, motto).x * 0.5f, cy), col::kTextMuted, motto);
    }
    cy += LineHeight(kMotto) + Px(12.0f);
    if (!profile_games_.empty()) {
      char level[48];
      std::snprintf(level, sizeof(level), t_.level, Totals().Level());
      ImVec2 size = BadgeSize(level, 12);
      Badge(ImVec2(card.GetCenter().x - size.x * 0.5f, cy), level, col::kAccentTint, col::kAccent, 12);
      cy += size.y + Px(14.0f);
    }
    constexpr Type kNote{Font::kRegular, 12};
    DrawParagraph(kNote, ImVec2(card.Min.x + Px(16.0f), card.Max.y - Px(16.0f) - LineHeight(kNote) * 1.4f * 2.0f),
                  card.GetWidth() - Px(32.0f), 1.4f, col::kTextDim, t_.profile_preview_note, true, 0.5f);
  }

  // The form, right.
  float fx = x + preview_width + gap;
  float form_width = content.Max.x - fx;
  ImRect form(ImVec2(fx, y), ImVec2(content.Max.x, content.Max.y));
  FillRect(form, col::kCard, 14.0f);
  float px = fx + Px(28.0f);
  float inner = form_width - Px(56.0f);
  float py = y + Px(28.0f);
  constexpr Type kLabel{Font::kSemiBold, 14};
  constexpr Type kCount{Font::kRegular, 13};
  float label = LineHeight(kLabel);
  {
    // The name and the motto, side by side.
    float field_gap = Px(16.0f);
    float half = (inner - field_gap) * 0.5f;
    auto field = [&](float fx0, const char* title, const char* extra, std::string* value, size_t max, const char* id) {
      DrawLabel(kLabel, ImVec2(fx0, py), col::kText, title);
      if (extra) {
        DrawLabel(kCount, ImVec2(fx0 + MeasureText(kLabel, title).x + Px(6.0f), py), col::kTextDim, extra, label);
      }
      char count[16];
      std::snprintf(count, sizeof(count), "%zu/%zu", profiles::CountChars(*value), max);
      DrawLabel(kCount, ImVec2(fx0 + half - MeasureText(kCount, count).x, py), col::kTextDim, count, label);
      TextField(id, ImRect(ImVec2(fx0, py + label + Px(8.0f)), ImVec2(fx0 + half, py + label + Px(8.0f) + Px(48.0f))),
                value, max, "");
    };
    field(px, t_.profile_name, nullptr, &edit_name_, profiles::kNameMax, "##edit_name");
    field(px + half + field_gap, t_.profile_motto, t_.optional, &edit_motto_, profiles::kMottoMax, "##edit_motto");
    py += label + Px(8.0f) + Px(48.0f) + Px(24.0f);
  }
  {
    // The avatars, and the photo.
    DrawLabel(kLabel, ImVec2(px, py), col::kText, t_.profile_avatar);
    py += label + Px(10.0f);
    const float button = Px(76.0f);
    const float bgap = Px(10.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
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
        draw->AddCircle(center, button * 0.5f - Px(1.5f), col::kOutline, 64, Px(3.0f));
      }
      ImRect picture(ImVec2(r.Min.x + Px(6.0f), r.Min.y + Px(6.0f)), ImVec2(r.Min.x + Px(70.0f), r.Min.y + Px(70.0f)));
      if (!is_photo) {
        DrawSvg(profiles::kAvatars[i].key, profiles::kAvatars[i].body, 100.0f, picture, true);
      } else if (has_photo) {
        DrawTexture(photo, picture, true);
      } else {
        draw->AddCircleFilled(center, button * 0.5f - Px(2.0f), col::kCardRaised, 64);
        DashedRect(ImRect(ImVec2(r.Min.x + Px(1.0f), r.Min.y + Px(1.0f)), ImVec2(r.Max.x - Px(1.0f), r.Max.y - Px(1.0f))),
                   col::kDashed, 76.0f * 0.5f / col::kRoundness);
        DrawIcon(Icon::kCamera, ImVec2(center.x - Px(10.0f), center.y - Px(18.0f)), 20.0f, col::kTextSoft, 2.0f);
        constexpr Type kSmall{Font::kSemiBold, 11};
        std::string caption = Ellipsize(kSmall, t_.profile_photo, button - Px(8.0f));
        DrawLabel(kSmall, ImVec2(center.x - MeasureText(kSmall, caption).x * 0.5f, center.y + Px(5.0f)), col::kTextSoft,
                  caption);
      }
      if (state.focused) FocusRing(r, 76.0f * 0.5f / col::kRoundness);
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
    // The color, and what it is for.
    DrawLabel(kLabel, ImVec2(px, py), col::kText, t_.profile_color);
    py += label + Px(10.0f);
    const float swatch = Px(44.0f);
    const float sgap = Px(10.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    for (size_t i = 0; i < profiles::kColors.size(); ++i) {
      ImRect r(ImVec2(px + float(i) * (swatch + sgap), py), ImVec2(px + float(i) * (swatch + sgap) + swatch, py + swatch));
      char id[32];
      std::snprintf(id, sizeof(id), "##edit_color%zu", i);
      ItemState state = Item(id, r);
      draw->AddCircleFilled(r.GetCenter(), swatch * 0.5f - Px(5.0f), ProfileColor(int(i)), 48);
      if (edit_color_ == int(i)) {
        draw->AddCircle(r.GetCenter(), swatch * 0.5f - Px(1.5f), col::kText, 48, Px(3.0f));
      } else if (state.hovered) {
        draw->AddCircle(r.GetCenter(), swatch * 0.5f - Px(1.5f), col::kOutline, 48, Px(3.0f));
      }
      if (state.focused) FocusRing(r, 44.0f * 0.5f / col::kRoundness);
      if (state.pressed) edit_color_ = int(i);
    }
    py += swatch + Px(10.0f);
    DrawLabel(Type{Font::kRegular, 13}, ImVec2(px, py), col::kTextDim, t_.profile_color_hint);
  }

  // The foot: delete left, cancel and save right, a message above.
  float foot = form.Max.y - Px(28.0f) - Px(46.0f);
  FillRect(ImRect(ImVec2(px, foot - Px(18.0f)), ImVec2(px + inner, foot - Px(17.0f))), col::kDivider);
  bool last = store_.profiles.size() < 2;
  ButtonSpec del{ButtonKind::kDanger, Type{Font::kSemiBold, 14}, 46, 18, 8};
  const char* del_text = edit_delete_armed_ ? t_.delete_profile_confirm : t_.delete_profile;
  ImRect del_rect(ImVec2(px, foot), ImVec2(px + ButtonWidth(del, del_text), foot + Px(46.0f)));
  // Why the last profile cannot be deleted, while the pointer is on its button; otherwise the last message.
  bool last_hint = last && ImGui::IsMouseHoveringRect(del_rect.Min, del_rect.Max);
  if (last_hint || !edit_message_.empty()) {
    constexpr Type kMessage{Font::kMedium, 13};
    DrawLabel(kMessage, ImVec2(px, foot - Px(18.0f) - Px(10.0f) - LineHeight(kMessage)),
              last_hint || edit_message_error_ ? col::kError : col::kSuccess,
              Ellipsize(kMessage, last_hint ? std::string(t_.delete_last_profile) : edit_message_, inner));
  }
  {
    if (Button("##delete_profile", ImVec2(px, foot), del, del_text, !last)) {
      if (!edit_delete_armed_) {
        edit_delete_armed_ = true;
      } else {
        std::string id = profile->id;
        ForgetAvatar(id);
        if (profiles::Delete(store_, id, RECOMP_LAUNCHER_VERSION_STR)) {
          edit_delete_armed_ = false;
          LoadAchievements();
          LoadProfileGames();
          profile_tab_ = 0;
          Navigate(Page::kProfile, false);
          return;
        }
        edit_message_ = t_.delete_failed;
        edit_message_error_ = true;
        edit_delete_armed_ = false;
      }
    }
  }
  {
    std::string name = profiles::Trim(edit_name_);
    ButtonSpec save{name.empty() ? ButtonKind::kDisabled : ButtonKind::kPrimary, Type{Font::kBold, 15}, 46, 22, 8};
    ButtonSpec cancel{ButtonKind::kGhost, Type{Font::kSemiBold, 14}, 46, 18, 8};
    float save_width = ButtonWidth(save, t_.save_changes);
    float cancel_width = ButtonWidth(cancel, t_.cancel);
    float right = px + inner;
    if (Button("##save_profile", ImVec2(right - save_width, foot), save, t_.save_changes, !name.empty())) {
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
        edit_message_ = t_.changes_saved;
        edit_message_error_ = false;
        edit_photo_.reset();
      } else {
        char text[256];
        std::snprintf(text, sizeof(text), t_.profile_save_error, "profile.toml");
        edit_message_ = text;
        edit_message_error_ = true;
      }
    }
    if (Button("##cancel_profile", ImVec2(right - save_width - Px(10.0f) - cancel_width, foot), cancel, t_.cancel)) {
      Navigate(Page::kProfile, false);
    }
  }
}

}  // namespace launcher
