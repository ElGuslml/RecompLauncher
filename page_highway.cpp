// The Mods tab's Highway section (launcher 3.30, Gustavo's "mods-galeria" mockup; game::kHighwayPreview): the preview of
// the note highway at the rows' right, drawn from the installed game's own textures (no file of the game is shipped),
// and the gallery of the player's pictures over it: All (Original, Random and the folder's pictures), Favorites (Random
// among them and the favorites) and Recent, a tall preview of the picture in focus, A uses it, X marks a favorite, Y
// gives it its own darkening, LB / RB change the list, B closes it unchanged. highway.h reads the textures and the
// pictures.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL_dialog.h>
#include <imgui_internal.h>

#include "app.h"
#include "game.h"
#include "highway.h"
#include "pages.h"

namespace launcher {

using namespace ui;

namespace {

// A row with several values of the game's (game::kChoices) by its cvar: its value's text, or "".
std::string ChoiceValue(const Options& o, const char* cvar) {
  if (!cvar) return {};
  for (size_t i = 0; i < game::kChoices.size(); ++i) {
    const game::Choice& choice = game::kChoices[i];
    if (std::string_view(choice.cvar) == cvar) {
      return choice.values[size_t(std::clamp(o.choices[i], 0, int(choice.values.size()) - 1))];
    }
  }
  return {};
}

double Percent(const Options& o, const char* cvar) {
  const std::string value = ChoiceValue(o, cvar);
  return value.empty() ? 1.0 : std::atof(value.c_str()) / 100.0;
}

// A favorite's star: five points around a pentagon.
void DrawStar(ImVec2 center, float radius, ImU32 color) {
  ImDrawList* draw = ImGui::GetWindowDrawList();
  ImVec2 outer[5], inner[5];
  for (int i = 0; i < 5; ++i) {
    const float a = -1.5707964f + float(i) * 1.2566371f;
    const float b = a + 0.62831855f;
    outer[i] = ImVec2(center.x + std::cos(a) * radius, center.y + std::sin(a) * radius);
    inner[i] = ImVec2(center.x + std::cos(b) * radius * 0.45f, center.y + std::sin(b) * radius * 0.45f);
  }
  for (int i = 0; i < 5; ++i) draw->AddTriangleFilled(outer[i], inner[i], inner[(i + 4) % 5], color);
  draw->AddConvexPolyFilled(inner, 5, color);
}

constexpr ImU32 kStarColor = IM_COL32(245, 197, 66, 255);

// A darkening's name: Off, Automatic, a percent.
std::string DimName(const Texts& t, std::string_view value) {
  if (value.empty() || value == "off") return t.off;
  if (value == "auto") return t.highway_dim_auto;
  return std::string(value) + " %";
}

// The gallery's tiles: "" Original, kHighwayRandom Random, or a picture's name.
std::vector<std::string> Tiles(const Options& o, const highway::Pictures& pictures, int filter) {
  std::vector<std::string> tiles;
  const auto& list = pictures.List();
  if (filter == 0) {
    tiles.push_back("");
    tiles.push_back(kHighwayRandom);
    for (const highway::Picture& p : list) tiles.push_back(p.name);
  } else if (filter == 1) {
    bool any = false;
    for (const highway::Picture& p : list) {
      if (IsHighwayFavorite(o, p.name)) {
        if (!any) tiles.push_back(kHighwayRandom);
        any = true;
        tiles.push_back(p.name);
      }
    }
  } else {
    for (const std::string& name : o.highway_recent) {
      const int at = pictures.Find(name);
      if (at >= 0) tiles.push_back(list[size_t(at)].name);
    }
  }
  return tiles;
}

int FavoriteCount(const Options& o, const highway::Pictures& pictures) {
  int count = 0;
  for (const highway::Picture& p : pictures.List()) count += IsHighwayFavorite(o, p.name) ? 1 : 0;
  return count;
}

}  // namespace

std::string App::HighwayImageLabel() const {
  const std::string& image = settings_.o.highway_image;
  if (image.empty()) return t_.highway_original;
  if (image == kHighwayRandom) return t_.highway_random;
  return image;
}

highway::Look App::HighwayLook(std::string_view picture) const {
  const Options& o = settings_.o;
  const game::HighwayGallery& g = game::kHighwayGallery;
  highway::Look look;
  look.scale = Percent(o, g.scale_cvar);
  look.length = Percent(o, g.length_cvar);
  look.perspective = Percent(o, g.perspective_cvar);
  if (g.keep_speed_cvar) {
    for (size_t i = 0; i < game::kToggles.size(); ++i) {
      if (std::string_view(game::kToggles[i].cvar) == g.keep_speed_cvar) look.keep_speed = o.toggles[i];
    }
  }
  look.opacity = float(Percent(o, g.opacity_cvar));
  look.dark = g.style_cvar && g.dark_style && ChoiceValue(o, g.style_cvar) == g.dark_style;
  if (!picture.empty() && highway_pictures_) {
    const int at = highway_pictures_->Find(picture);
    const float luminance = at >= 0 ? highway_pictures_->List()[size_t(at)].luminance : 0.0f;
    std::string dim = HighwayImageDim(o, picture);
    if (dim.empty()) dim = o.highway_dim;
    look.brightness = highway::Brightness(dim, luminance);
  }
  return look;
}

void App::OpenHighwayGallery() {
  gallery_ = HighwayGalleryState{.rect = gallery_.rect};
  gallery_.open = true;
  const Options& o = settings_.o;
  // On the picture in use, in the list that has it.
  gallery_.focus = o.highway_image;
  gallery_.focus_valid = true;
  gallery_.filter = o.highway_image == kHighwayRandom && o.highway_random_favorites ? 1 : 0;
  if (highway_pictures_ && gallery_.filter == 1 && Tiles(o, *highway_pictures_, 1).empty()) gallery_.filter = 0;
}

void App::ChooseHighwayFolder() {
  // As the DLC folder's: the dialog answers on a thread of its own, PollHighwayFolder takes the folder.
  auto* holder = new std::shared_ptr<DialogResult>(highway_dialog_);
  std::error_code ec;
  std::filesystem::create_directories(highway::PathFromUtf8(settings_.o.highway_folder), ec);
  SDL_ShowOpenFolderDialog(
      [](void* userdata, const char* const* folders, int) {
        auto* result = static_cast<std::shared_ptr<DialogResult>*>(userdata);
        if (folders && folders[0]) {
          std::lock_guard lock((*result)->mutex);
          (*result)->path = folders[0];
        }
        delete result;
      },
      holder, window_, settings_.o.highway_folder.empty() ? nullptr : settings_.o.highway_folder.c_str(), false);
}

void App::PollHighwayFolder() {
  std::optional<std::string> chosen;
  {
    std::lock_guard lock(highway_dialog_->mutex);
    chosen.swap(highway_dialog_->path);
  }
  if (chosen && !chosen->empty() && *chosen != settings_.o.highway_folder) {
    Options& o = settings_.o;
    o.highway_folder = *chosen;
    // A picture of the other folder is not in this one: the game's own until one is chosen (Random stays).
    if (o.highway_image != kHighwayRandom) o.highway_image.clear();
    Changed();
  }
}

void App::HighwayFolderMenuWindow() {
  // As the selectors' list (ChoiceList): a card under the row's right side, the controller's focus on its first item,
  // up and down move, A or a click picks, B, Escape or a click outside closes it.
  constexpr const char* kPopup = "##highway_folder_menu";
  if (!folder_menu_.open) return;
  if (!folder_menu_.opened) ImGui::OpenPopup(kPopup);
  const float row_height = Px(46.0f);
  const float padding = Px(6.0f);
  const ImVec2 size(Px(300.0f), padding * 2.0f + row_height * 2.0f);
  ImVec2 pos(folder_menu_.anchor.Max.x - size.x, folder_menu_.anchor.Max.y + Px(6.0f));
  ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float bottom = viewport->Pos.y + viewport->Size.y - Snap(Px(52.0f));
  if (pos.y + size.y > bottom) pos.y = folder_menu_.anchor.Min.y - Px(6.0f) - size.y;
  ImGui::SetNextWindowPos(Snap(pos));
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
  if (!ImGui::BeginPopup(kPopup, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground)) {
    ImGui::PopStyleVar(4);
    if (folder_menu_.opened) folder_menu_ = HighwayFolderMenu{};  // closed by a click outside it
    return;
  }
  const bool first = !folder_menu_.opened;
  folder_menu_.opened = true;
  const ImVec2 origin = ImGui::GetWindowPos();
  ImRect card(origin, ImVec2(origin.x + size.x, origin.y + size.y));
  FillRect(card, col::kCardRaised, 12.0f);
  StrokeRect(card, col::kHairline, 12.0f);
  constexpr Type kItem{Font::kSemiBold, 15};
  const char* labels[2] = {t_.highway_folder_open, t_.highway_folder_change};
  const Icon icons[2] = {Icon::kFolder, Icon::kPencil};
  int picked = -1;
  for (int i = 0; i < 2; ++i) {
    ImRect r(ImVec2(card.Min.x + padding, card.Min.y + padding + row_height * float(i)),
             ImVec2(card.Max.x - padding, card.Min.y + padding + row_height * float(i + 1)));
    if (first && i == 0) ImGui::SetKeyboardFocusHere();
    ImGui::PushID(i);
    ItemState state = Item("##folder_item", r);
    ImGui::PopID();
    if (state.focused || state.hovered) {
      FillRect(r, state.focused ? Fade(col::kAccent, 0.16f) : Fade(col::kFocus, 0.06f), 8.0f);
    }
    DrawIcon(icons[i], ImVec2(r.Min.x + Px(14.0f), r.GetCenter().y - Px(9.0f)), 18.0f,
             state.focused ? col::kText : col::kTextMuted, 1.9f);
    DrawLabel(kItem, ImVec2(r.Min.x + Px(44.0f), r.Min.y), state.focused || state.hovered ? col::kText : col::kTextSoft,
              Ellipsize(kItem, labels[i], r.GetWidth() - Px(54.0f)), r.GetHeight());
    if (state.pressed) picked = i;
  }
  const bool cancel = !first && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
                                 ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false));
  if (picked >= 0 || cancel) {
    folder_menu_ = HighwayFolderMenu{};
    ImGui::CloseCurrentPopup();
    if (picked == 0) {
      // Made first: the default folder does not exist until a picture goes in.
      std::error_code ec;
      const std::filesystem::path folder = highway::PathFromUtf8(settings_.o.highway_folder);
      std::filesystem::create_directories(folder, ec);
      OpenFolder(folder);
    } else if (picked == 1) {
      ChooseHighwayFolder();
    }
    if (settings_row_focus_) RequestFocus(settings_row_focus_, gamepad_used_ || SDL_HasGamepad());
  }
  ImGui::EndPopup();
  ImGui::PopStyleVar(4);
}

void App::HighwayPanel(const ImRect& rect) {
  Options& o = settings_.o;
  if (!highway_game_) {
    highway_game_ = std::make_unique<highway::GameTextures>();
    const std::filesystem::path files = GameFilesFolder(exe_dir_);
    if (!files.empty()) {
      const game::HighwayAssets& a = game::kHighwayAssets;
      highway_game_->Load(files / a.pak, a.highway_pak ? files / a.highway_pak : std::filesystem::path{});
    }
  }
  if (!highway_pictures_) highway_pictures_ = std::make_unique<highway::Pictures>();
  highway_game_->Update();
  // The player's pictures where the game reads them (launcher 3.39); otherwise the game's own highway only.
  if (kHighwayPictures) {
    PollHighwayFolder();
    HighwayFolderMenuWindow();
    highway_pictures_->Refresh(o.highway_folder);
    highway_pictures_->Update();
  }

  // The card: the preview's, or the gallery's (HighwayGalleryWindow draws over it).
  const ImRect card(rect.Min, ImVec2(rect.Max.x, std::min(rect.Max.y, rect.Min.y + Px(500.0f))));
  gallery_.rect = card;
  FillRect(card, col::kCard, 18.0f);
  StrokeRect(card, col::kHairline, 18.0f);
  if (gallery_.open) {
    HighwayGalleryWindow();
    return;
  }
  constexpr Type kHead{Font::kSemiBold, 14};
  constexpr Type kBadge{Font::kSemiBold, 12};
  constexpr Type kCaption{Font::kRegular, 14};
  constexpr Type kNote{Font::kRegular, 13};
  const float pad = Px(16.0f);
  float y = card.Min.y + pad;
  DrawLabel(kHead, ImVec2(card.Min.x + pad, y), col::kText, t_.preview_title, Px(24.0f));
  {
    const float x = card.Min.x + pad + MeasureText(kHead, t_.preview_title).x + Px(8.0f);
    const float w = MeasureText(kBadge, t_.preview_badge).x + Px(16.0f);
    ImRect badge(ImVec2(x, y), ImVec2(x + w, y + Px(24.0f)));
    FillRect(badge, col::kChip, 12.0f);
    DrawLabel(kBadge, ImVec2(x + Px(8.0f), y), col::kTextMuted, t_.preview_badge, Px(24.0f));
  }
  const float caption_height = LineHeight(kCaption) + Px(6.0f) + LineHeight(kNote);
  ImRect area(ImVec2(card.Min.x + pad, y + Px(36.0f)), ImVec2(card.Max.x - pad, card.Max.y - pad - caption_height - Px(14.0f)));
  // The picture shown: the one chosen, or with Random each of its pool's for 3 seconds.
  std::string picture = kHighwayPictures ? o.highway_image : std::string();
  if (picture == kHighwayRandom) {
    std::vector<std::string> pool;
    for (const highway::Picture& p : highway_pictures_->List()) {
      if (!p.unreadable && !p.no_preview && (!o.highway_random_favorites || IsHighwayFavorite(o, p.name))) {
        pool.push_back(p.name);
      }
    }
    if (pool.empty() && o.highway_random_favorites) {
      for (const highway::Picture& p : highway_pictures_->List()) {
        if (!p.unreadable && !p.no_preview) pool.push_back(p.name);
      }
    }
    picture = pool.empty() ? std::string() : pool[size_t(ImGui::GetTime() / 3.0) % pool.size()];
  } else if (!picture.empty() && highway_pictures_->Find(picture) < 0) {
    picture.clear();  // not in the folder: the game shows its own
  }
  const highway::Texture* texture = picture.empty() ? nullptr : highway_pictures_->Large(picture);
  if (highway_game_->Failed() || GameFilesFolder(exe_dir_).empty()) {
    DrawParagraph(kCaption, ImVec2(area.Min.x, area.GetCenter().y - Px(10.0f)), area.GetWidth(), 1.4f, col::kTextMuted,
                  t_.preview_no_game);
  } else if (!picture.empty() && !texture) {
    // The picture still being read: the highway's place empty for these frames.
    highway::Draw(area, *highway_game_, nullptr, HighwayLook({}), ImGui::GetTime());
  } else {
    highway::Draw(area, *highway_game_, texture, HighwayLook(picture), ImGui::GetTime());
  }
  // The caption: the picture and the shape's rows, then the note.
  y = area.Max.y + Px(14.0f);
  const game::GameTexts& own = game::GameTextsFor(language_);
  std::string caption = kHighwayPictures ? HighwayImageLabel() : std::string();
  for (size_t i = 0; i < game::kChoices.size(); ++i) {
    if (game::kChoices[i].group != game::ToggleGroup::kModsHighway) continue;
    if (!caption.empty()) caption += " \xC2\xB7 ";
    caption += own.choices[i * 2];
    caption += " ";
    caption += ChoiceValue(o, game::kChoices[i].cvar) + game::kChoices[i].suffix;
  }
  DrawLabel(kCaption, ImVec2(card.Min.x + pad, y), col::kTextSoft, Ellipsize(kCaption, caption, card.GetWidth() - pad * 2));
  DrawLabel(kNote, ImVec2(card.Min.x + pad, y + LineHeight(kCaption) + Px(6.0f)), col::kTextDim,
            Ellipsize(kNote, t_.preview_note, card.GetWidth() - pad * 2));
}

void App::HighwayGalleryWindow() {
  constexpr const char* kPopup = "##highway_gallery";
  Options& o = settings_.o;
  if (!gallery_.opened) ImGui::OpenPopup(kPopup);
  const ImRect card = gallery_.rect;
  ImGui::SetNextWindowPos(card.Min);
  ImGui::SetNextWindowSize(card.GetSize());
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 0.0f);
  if (!ImGui::BeginPopup(kPopup, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground)) {
    ImGui::PopStyleVar(4);
    if (gallery_.opened) gallery_ = HighwayGalleryState{.rect = gallery_.rect};  // closed by a click outside it
    return;
  }
  const bool first = !gallery_.opened;
  gallery_.opened = true;
  bool close = false;
  constexpr Type kTitle{Font::kDisplayMedium, 26, 0.015f};
  constexpr Type kSmall{Font::kRegular, 13};
  constexpr Type kChipText{Font::kSemiBold, 13};
  constexpr Type kInfo{Font::kRegular, 14};
  constexpr Type kInfoBold{Font::kSemiBold, 14};
  const float pad = Px(20.0f);
  highway::Pictures& pictures = *highway_pictures_;
  const auto& list = pictures.List();
  const int favorites = FavoriteCount(o, pictures);

  // LB / RB: the list; the focus on its first tile (or the one in use).
  if (!first) {
    int filter = gallery_.filter;
    if (ButtonPressed(PadButton::kLB)) filter = (filter + 2) % 3;
    if (ButtonPressed(PadButton::kRB)) filter = (filter + 1) % 3;
    if (filter != gallery_.filter) {
      gallery_.filter = filter;
      gallery_.refocus = true;
    }
  }
  std::vector<std::string> tiles = Tiles(o, pictures, gallery_.filter);

  // The title, and the focus's place (Favorites: how many).
  float y = card.Min.y + Px(18.0f);
  DrawLabel(kTitle, ImVec2(card.Min.x + pad, y), col::kText, Title(t_.gallery_title));
  {
    char text[64];
    if (gallery_.filter == 1) {
      std::snprintf(text, sizeof(text), t_.gallery_count_favorites, favorites);
    } else {
      std::snprintf(text, sizeof(text), t_.gallery_position, gallery_.position, int(tiles.size()));
    }
    std::string shown = gallery_.filter == 1 || !tiles.empty() ? std::string(text) : std::string();
    DrawLabel(kSmall, ImVec2(card.Max.x - pad - MeasureText(kSmall, shown).x, y + Px(8.0f)), col::kTextMuted, shown);
  }
  y += LineHeight(kTitle) + Px(8.0f);
  // The lists as chips (the mouse's; LB / RB the controller's), the order at the right.
  {
    char all[48], fav[48];
    std::snprintf(all, sizeof(all), t_.gallery_count_all, int(list.size()));
    std::snprintf(fav, sizeof(fav), t_.gallery_count_favorites, favorites);
    const char* labels[3] = {all, fav, t_.gallery_recent};
    float x = card.Min.x + pad;
    const float h = Px(30.0f);
    ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    for (int i = 0; i < 3; ++i) {
      const float w = MeasureText(kChipText, labels[i]).x + Px(32.0f);
      ImRect chip(ImVec2(x, y), ImVec2(x + w, y + h));
      ImGui::PushID(i);
      ItemState state = Item("##filter", chip);
      ImGui::PopID();
      const bool on = gallery_.filter == i;
      FillRect(chip, on ? col::kChip : (state.hovered ? Fade(col::kFocus, 0.06f) : 0), 15.0f);
      StrokeRect(chip, on ? col::kAccent : col::kHairline, 15.0f, on ? 1.5f : 1.0f);
      DrawLabel(kChipText, ImVec2(chip.Min.x + Px(16.0f), chip.Min.y), on ? col::kText : col::kTextMuted, labels[i], h);
      if (state.pressed && !on) {
        gallery_.filter = i;
        gallery_.refocus = true;
        tiles = Tiles(o, pictures, i);
      }
      x = chip.Max.x + Px(8.0f);
    }
    ImGui::PopItemFlag();
    DrawLabel(kSmall, ImVec2(card.Max.x - pad - MeasureText(kSmall, t_.gallery_by_name).x, y), col::kTextMuted,
              t_.gallery_by_name, h);
    y += h + Px(16.0f);
  }

  // The body: the tall preview at the left, the tiles at its right, the line about the focus under them.
  const float info_height = LineHeight(kInfo) + Px(16.0f);
  ImRect body(ImVec2(card.Min.x + pad, y), ImVec2(card.Max.x - pad, card.Max.y - info_height - Px(8.0f)));
  const float preview_width = std::min(Px(150.0f), body.GetWidth() * 0.32f);
  ImRect preview(body.Min, ImVec2(body.Min.x + preview_width, body.Max.y));
  ImRect grid(ImVec2(preview.Max.x + Px(20.0f), body.Min.y), body.Max);

  // The tile the preview and the line show: the mouse's while it moves over one, else the focus.
  const std::string* shown = nullptr;
  std::string focus_now;
  bool focus_found = false;
  if (tiles.empty()) {
    // An empty list: what to do.
    const char* title = gallery_.filter == 1 ? t_.gallery_no_favorites_title
                        : gallery_.filter == 2 ? t_.gallery_no_recent_title
                                               : t_.gallery_empty_title;
    const char* text = gallery_.filter == 1 ? t_.gallery_no_favorites
                       : gallery_.filter == 2 ? t_.gallery_no_recent
                                              : t_.gallery_empty;
    constexpr Type kEmptyTitle{Font::kDisplayMedium, 22};
    constexpr Type kEmptyText{Font::kRegular, 15};
    float ey = body.Min.y + Px(40.0f);
    const float ex = body.Min.x + Px(8.0f), ew = body.GetWidth() - Px(16.0f);
    ey += DrawParagraph(kEmptyTitle, ImVec2(ex, ey), ew, 1.2f, col::kText, title) + Px(8.0f);
    ey += DrawParagraph(kEmptyText, ImVec2(ex, ey), ew, 1.5f, col::kTextMuted, text) + Px(20.0f);
    ButtonSpec spec{ButtonKind::kGhost, Type{Font::kSemiBold, 14}, 40, 18, 10};
    const char* label = gallery_.filter == 0 ? t_.gallery_open_folder : t_.see_all;
    if (Button("##gallery_empty", ImVec2(ex, ey), spec, label)) {
      if (gallery_.filter == 0) {
        std::error_code ec;
        std::filesystem::create_directories(highway::PathFromUtf8(o.highway_folder), ec);
        OpenFolder(highway::PathFromUtf8(o.highway_folder));
      } else {
        gallery_.filter = 0;
        gallery_.refocus = true;
      }
    }
    if (first || gallery_.refocus) {
      RequestFocus(ImGui::GetItemID(), gamepad_used_ || SDL_HasGamepad());
      gallery_.refocus = false;
    }
    gallery_.position = 0;
  } else {
    BeginScroll("##gallery_tiles", grid);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float gap = Px(12.0f);
    const float avail = grid.GetWidth() - Px(14.0f);
    const int columns = std::max(1, int((avail + gap) / (Px(52.0f) + gap)));
    const float tile_w = std::floor((avail - gap * float(columns - 1)) / float(columns));
    const float tile_h = tile_w * 1.9f;
    // The focus: kept on its tile, else the one in use, else the first.
    std::string want = gallery_.focus;
    if (gallery_.refocus || !gallery_.focus_valid ||
        std::find(tiles.begin(), tiles.end(), want) == tiles.end()) {
      want = std::find(tiles.begin(), tiles.end(), o.highway_image) != tiles.end() ? o.highway_image : tiles.front();
    }
    const bool give_focus = first || gallery_.refocus;
    gallery_.refocus = false;
    const highway::Texture& original = highway_game_->Get(highway::Part::kHighway);
    std::string hovered;
    bool any_hovered = false;
    for (size_t i = 0; i < tiles.size(); ++i) {
      const std::string& tile = tiles[i];
      const int column = int(i) % columns, row = int(i) / columns;
      ImRect r(ImVec2(origin.x + float(column) * (tile_w + gap), origin.y + float(row) * (tile_h + gap + Px(6.0f))),
               ImVec2(origin.x + float(column) * (tile_w + gap) + tile_w,
                      origin.y + float(row) * (tile_h + gap + Px(6.0f)) + tile_h));
      ImGui::PushID(int(i));
      if (give_focus && tile == want) ImGui::SetKeyboardFocusHere();
      ItemState state = Item("##tile", r);
      ImGui::PopID();
      const int at = tile.empty() || tile == kHighwayRandom ? -1 : pictures.Find(tile);
      const highway::Picture* picture = at >= 0 ? &list[size_t(at)] : nullptr;
      // Its face.
      if (tile.empty()) {
        if (original.id) DrawRoundedTexture(original.id, r, 8.0f);
        else FillRect(r, col::kChip, 8.0f);
      } else if (tile == kHighwayRandom) {
        FillRect(r, Fade(col::kFocus, 0.03f), 8.0f);
        DashedRect(r, col::kDashed, 8.0f);
        DrawIcon(Icon::kRefresh, ImVec2(r.GetCenter().x - Px(9.0f), r.GetCenter().y - Px(9.0f)), 18.0f, col::kTextMuted);
      } else if (picture && picture->thumbnail.id) {
        DrawRoundedTexture(picture->thumbnail.id, r, 8.0f);
      } else {
        FillRect(r, col::kChip, 8.0f);
        if (picture && picture->no_preview) {
          constexpr Type kTga{Font::kSemiBold, 12};
          DrawLabel(kTga, ImVec2(r.GetCenter().x - MeasureText(kTga, "TGA").x * 0.5f, r.GetCenter().y - Px(8.0f)),
                    col::kTextMuted, "TGA");
        }
      }
      if (picture && picture->unreadable) {
        StrokeRect(r, col::kError, 8.0f, 2.0f);
        DrawIcon(Icon::kAlert, ImVec2(r.GetCenter().x - Px(9.0f), r.GetCenter().y - Px(9.0f)), 18.0f, col::kError);
      }
      if (picture && IsHighwayFavorite(o, tile)) {
        DrawStar(ImVec2(r.Max.x - Px(11.0f), r.Min.y + Px(11.0f)), Px(7.0f), kStarColor);
      }
      // Its own darkening: a half-dark disc at the top left.
      if (picture && !HighwayImageDim(o, tile).empty()) {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 c(r.Min.x + Px(11.0f), r.Min.y + Px(11.0f));
        draw->AddCircleFilled(c, Px(6.5f), IM_COL32(0, 0, 0, 200), 16);
        draw->PathArcTo(c, Px(5.0f), -1.5707964f, 1.5707964f, 12);
        draw->PathFillConvex(IM_COL32(255, 255, 255, 230));
        draw->AddCircle(c, Px(5.0f), IM_COL32(255, 255, 255, 230), 16, Px(1.2f));
      }
      // In use: a bar under it.
      const bool in_use = tile == o.highway_image &&
                          (tile != kHighwayRandom || o.highway_random_favorites == (gallery_.filter == 1));
      if (in_use) {
        const float bw = std::min(tile_w * 0.45f, Px(24.0f));
        FillRect(ImRect(ImVec2(r.GetCenter().x - bw * 0.5f, r.Max.y + Px(5.0f)),
                        ImVec2(r.GetCenter().x + bw * 0.5f, r.Max.y + Px(8.0f))),
                 col::kAccent, 1.5f);
      }
      if (state.focused) {
        StrokeRect(ImRect(ImVec2(r.Min.x - Px(3.0f), r.Min.y - Px(3.0f)), ImVec2(r.Max.x + Px(3.0f), r.Max.y + Px(3.0f))),
                   col::kFocus, 10.0f, 2.0f);
        focus_now = tile;
        focus_found = true;
      } else if (state.hovered) {
        StrokeRect(r, Fade(col::kFocus, 0.5f), 8.0f, 1.0f);
      }
      if (state.hovered) {
        hovered = tile;
        any_hovered = true;
      }
      // A or a click: use it (not a file that cannot be read).
      if (state.pressed && !(picture && picture->unreadable)) {
        if (tile == kHighwayRandom) {
          o.highway_random_favorites = gallery_.filter == 1;
        } else if (!tile.empty()) {
          AddHighwayRecent(o, tile);
        }
        o.highway_image = tile;
        close = true;
        Changed();
      }
    }
    ImGui::Dummy(ImVec2(grid.GetWidth() - Px(14.0f),
                        float((int(tiles.size()) + columns - 1) / columns) * (tile_h + gap + Px(6.0f))));
    EndScroll();
    if (focus_found) {
      gallery_.focus = focus_now;
      gallery_.focus_valid = true;
    }
    gallery_.hovered = hovered;
    // The mouse's tile while the controller's focus does not show.
    gallery_.hovered_valid = any_hovered && !GImGui->NavCursorVisible;
    shown = gallery_.hovered_valid ? &gallery_.hovered : gallery_.focus_valid ? &gallery_.focus : nullptr;
    // The place shown in the title: that tile's, else the one in use.
    const std::string& place = shown ? *shown : o.highway_image;
    const auto it = std::find(tiles.begin(), tiles.end(), place);
    gallery_.position = it == tiles.end() ? 0 : int(it - tiles.begin()) + 1;
  }

  // The tall preview of that tile.
  {
    FillRect(preview, IM_COL32(12, 10, 11, 255), 10.0f);
    std::string picture;
    if (shown && *shown == kHighwayRandom) {
      // Random: each picture of its pool for 3 seconds.
      std::vector<std::string> pool;
      for (const highway::Picture& p : list) {
        if (!p.unreadable && !p.no_preview && (gallery_.filter != 1 || IsHighwayFavorite(o, p.name))) {
          pool.push_back(p.name);
        }
      }
      if (!pool.empty()) picture = pool[size_t(ImGui::GetTime() / 3.0) % pool.size()];
    } else if (shown) {
      picture = *shown;
    }
    const highway::Texture* texture = picture.empty() ? nullptr : pictures.Large(picture);
    if (highway_game_->Ready()) {
      ImGui::GetWindowDrawList()->PushClipRect(preview.Min, preview.Max, true);
      highway::Draw(preview, *highway_game_, texture, HighwayLook(texture ? std::string_view(picture) : std::string_view()),
                    ImGui::GetTime());
      ImGui::GetWindowDrawList()->PopClipRect();
    }
    StrokeRect(preview, col::kHairline, 10.0f);
  }

  // The line about it: its name, size, favorite and darkening; or what Original and Random are.
  if (shown) {
    const float iy = card.Max.y - info_height - Px(4.0f);
    std::string name, rest;
    ImU32 rest_color = col::kTextMuted;
    if (shown->empty()) {
      name = t_.highway_original;
      rest = std::string(" \xC2\xB7 ") + t_.gallery_original;
    } else if (*shown == kHighwayRandom) {
      name = t_.highway_random;
      rest = std::string(" \xC2\xB7 ") + (gallery_.filter == 1 ? t_.gallery_random_favorites : t_.gallery_random_all);
    } else {
      name = *shown;
      const int at = pictures.Find(*shown);
      const highway::Picture* p = at >= 0 ? &list[size_t(at)] : nullptr;
      if (p && p->unreadable) {
        rest = std::string(" \xC2\xB7 ") + t_.gallery_unreadable;
        rest_color = col::kError;
      } else if (p && p->no_preview) {
        rest = std::string(" \xC2\xB7 ") + t_.gallery_no_preview;
      } else {
        if (p && p->width) rest += " \xC2\xB7 " + std::to_string(p->width) + " \xC3\x97 " + std::to_string(p->height);
        if (IsHighwayFavorite(o, *shown)) rest += std::string(" \xC2\xB7 ") + t_.gallery_favorite;
        const std::string own_dim = HighwayImageDim(o, *shown);
        char dim[96];
        std::snprintf(dim, sizeof(dim), t_.gallery_dim,
                      own_dim.empty() ? (std::string(t_.gallery_dim_as_all) + " (" + DimName(t_, o.highway_dim) + ")").c_str()
                                      : DimName(t_, own_dim).c_str());
        rest += std::string(" \xC2\xB7 ") + dim;
      }
    }
    const float x = card.Min.x + pad;
    const float max = card.GetWidth() - pad * 2;
    const std::string name_shown = Ellipsize(kInfoBold, name, max * 0.5f);
    DrawLabel(kInfoBold, ImVec2(x, iy), col::kText, name_shown, info_height);
    const float nx = x + MeasureText(kInfoBold, name_shown).x;
    DrawLabel(kInfo, ImVec2(nx, iy), rest_color, Ellipsize(kInfo, rest, x + max - nx), info_height);
  }

  // X: a favorite; Y: its own darkening (as every picture's, off, automatic, 90 % to 40 %); B: closed unchanged.
  if (!first && shown) {
    const std::string tile = *shown;
    const bool picture = !tile.empty() && tile != kHighwayRandom && pictures.Find(tile) >= 0;
    if (picture && ButtonPressed(PadButton::kX)) {
      ToggleHighwayFavorite(o, tile);
      Changed();
    }
    if (picture && ButtonPressed(PadButton::kY)) {
      const std::string current = HighwayImageDim(o, tile);
      std::vector<std::string> cycle = {""};
      for (const char* value : kHighwayDims) cycle.push_back(value);
      auto it = std::find(cycle.begin(), cycle.end(), current);
      const size_t next = it == cycle.end() ? 0 : (size_t(it - cycle.begin()) + 1) % cycle.size();
      SetHighwayImageDim(o, tile, cycle[next]);
      Changed();
    }
  }
  if (!first && (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false))) {
    close = true;
  }
  if (close) {
    gallery_ = HighwayGalleryState{.rect = gallery_.rect};
    ImGui::CloseCurrentPopup();
    // Back on the Image row.
    if (settings_row_focus_) RequestFocus(settings_row_focus_, gamepad_used_ || SDL_HasGamepad());
  }
  ImGui::EndPopup();
  ImGui::PopStyleVar(4);
}

}  // namespace launcher
