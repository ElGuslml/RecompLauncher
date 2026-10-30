// The launcher's texts, UTF-8, in the Windows display language: English, German, French,
// Spanish and Italian (the Spanish ones are the mockup's). The game asks for its own language in its
// first menu.

#pragma once

#include <cstdint>

namespace launcher {

struct Texts {
  // The sidebar.
  const char* nav_play;
  const char* nav_achievements;
  const char* nav_controls;
  const char* nav_settings;
  const char* nav_video;
  const char* nav_graphics;
  const char* nav_game;
  const char* nav_updates;
  const char* donate;
  const char* version;  // %s: the version
  const char* launcher_version;  // %s: the launcher's version (the Updates page)
  // The controller's hints, under the sidebar.
  const char* hint_select;
  const char* hint_back;
  const char* hint_play;

  // Shared by the pages.
  const char* autosave;
  const char* reset_defaults;

  // Play.
  const char* tagline;
  const char* quit;
  const char* play;  // upper case
  const char* achievements_card;
  const char* see_achievements;
  const char* achievements_after_play;
  const char* never;

  // Video.
  const char* monitor;
  const char* monitor_hint;
  const char* display;
  const char* display_hint;
  const char* fullscreen;
  const char* borderless;
  const char* window;
  const char* resolution;
  const char* resolution_hint_screen;
  const char* resolution_hint_window;
  const char* format;
  const char* format_hint;
  const char* format_hint_16x9;
  const char* format_hint_xenos;
  const char* format_fills;       // per value of the Format row
  const char* format_bars_sides;
  const char* format_bars_top;
  const char* format_your_screen;  // badge on the screen's own aspect ratio
  const char* internal;
  const char* internal_hint;
  const char* internal_hint_xenos;  // %d × %d: the game's frame
  const char* automatic;
  const char* original;

  // Graphics.
  const char* graphics_api;
  const char* recommended;  // a badge, upper case
  const char* native_text;
  const char* native_missing;
  const char* xenos_text;
  const char* framerate;
  const char* framerate_hint;
  const char* antialiasing;
  const char* antialiasing_hint;
  const char* antialiasing_hint_xenos;
  const char* none;
  const char* experimental;
  const char* anisotropic;
  const char* anisotropic_hint;
  const char* off;
  const char* on;
  const char* shadows;
  const char* shadows_hint;
  const char* shadows_hint_xenos;
  const char* shadows_high;
  const char* shadows_ultra;
  const char* api_hint;
  // Launcher 3.4: V-Sync.
  const char* vsync;
  const char* vsync_hint;
  const char* vsync_help;

  // Keyboard.
  const char* mnk;
  const char* mnk_hint;
  const char* group_movement;
  const char* group_camera;
  const char* group_buttons;
  const char* group_dpad;
  const char* group_mouse;
  const char* binds[24];  // in launcher::Bind order
  const char* unbound;
  const char* press_key;
  const char* modifier_alone;
  const char* mouse_camera;
  const char* mouse_sensitivity;
  const char* key_space;
  const char* key_enter;
  const char* key_backspace;
  const char* key_delete;
  const char* key_insert;
  const char* key_home;
  const char* key_end;
  const char* key_page_up;
  const char* key_page_down;
  const char* key_caps_lock;
  const char* key_num_lock;
  const char* key_scroll_lock;
  const char* key_print_screen;
  const char* key_pause;
  const char* key_numpad;  // the prefix of the numeric keypad's keys
  const char* key_shift;
  const char* key_ctrl;
  const char* key_alt;
  const char* mouse_left;
  const char* mouse_right;
  const char* mouse_middle;

  // Gamepad.
  const char* pad_buttons[14];  // A B X Y LB RB LT RT LS RS Start Back D-pad Guide
  const char* press_button;
  const char* controller;  // a controller of unknown type

  // Achievements.
  const char* achievements_title;
  const char* filter_unlocked;  // %d
  const char* filter_locked;    // %d
  const char* gamerscore;       // upper case
  const char* unlocked;
  const char* locked;
  const char* achievements_empty_title;
  const char* achievements_empty;
  const char* achievement_secret;       // a secret achievement's name while locked
  const char* achievement_secret_hint;  // and its description
  const char* achievements_filter_empty;

  // Updates.
  const char* updates_title;
  const char* check_updates;
  const char* up_to_date;
  // The Game page (the game's own options, game.h).
  const char* game_language;
  const char* game_language_hint;
  const char* updates_off;  // the game has no releases to look for (game.h)
  const char* check_on_start;
  const char* check_on_start_hint;
  const char* release_notes;  // upper case; %s: the version
  const char* release_notes_none;
  const char* whats_new;  // %s: the version
  const char* checking_updates;
  const char* check_failed;  // %s: the error
  const char* check_rate_limited;
  const char* update_published;  // %s: date, %s: size in MB
  const char* update_now;
  const char* update_downloading;  // %s of %s MB
  const char* update_verifying;
  const char* update_installing;
  const char* update_failed;  // %s: the error
  const char* updated_to;  // %s: the version
  const char* update_card_available;  // %s: the version
  const char* see_whats_new;
  const char* cancel;
  const char* update_no_checksum;
  const char* update_checksum_mismatch;
  const char* update_in_use;  // %s: the file
  const char* full_history;

  // The first run (page_install.cpp).
  const char* step_welcome;
  const char* step_iso;
  const char* step_install;
  const char* step_done;
  const char* welcome_title;
  const char* welcome_text;
  const char* need_iso;
  const char* need_iso_hint;  // %s: the game
  const char* install_here;
  const char* install_here_hint;  // %s: the recompiled executable
  const char* need_space;
  const char* need_space_hint;  // %s: GB needed, GB free
  const char* own_copy;
  const char* start;
  const char* choose_title;
  const char* choose_text;  // %s: the game
  const char* drop_here;
  const char* checking_iso;
  const char* iso_valid;
  const char* iso_detail;  // %s: the game, the region
  const char* back;
  const char* extract;
  const char* wrong_game_title;
  const char* wrong_game_text;  // %s: the game
  const char* wrong_version_title;
  const char* wrong_version_text;  // %s: the game, the region
  const char* unreadable_title;
  const char* unreadable_text;
  const char* file_label;
  const char* detected_label;
  const char* detected_none;
  const char* detected_no_xex;
  const char* choose_other;
  const char* or_drop;
  const char* no_space;  // %s: GB needed, GB free
  const char* extracting_title;
  const char* extracting_text;
  const char* extracted_title;
  const char* extracted_text;
  const char* time_left;  // %s: mm:ss
  const char* current_file;  // upper case
  const char* iso_verified;  // %s: the game
  const char* cancel_install;
  const char* continue_label;
  const char* extract_failed_title;
  const char* extract_failed_text;  // %s: the file, the system's message
  const char* done_title;
  const char* done_text;
  const char* delete_iso;
  const char* review_settings;
  const char* iso_filter;  // the file dialog's filter
  const char* decimal_separator;  // in sizes (7,3 GB)

  // Errors.
  const char* game_missing;  // %s: the game's executable, %s: the launcher
  const char* starting_game;      // over the launcher until the game's window shows
  const char* game_closed_error;  // %s: the exit code
  const char* open_logs;
  const char* save_failed;  // %s: the options file
  const char* no_direct3d;  // %s: the launcher

  // The DLC page.
  const char* nav_dlc;
  const char* dlc_title;
  const char* dlc_subtitle;
  const char* dlc_install_title;
  const char* dlc_install_text;
  const char* dlc_choose_folder;
  const char* dlc_open_folder;
  const char* dlc_failed;      // %s: their names
  const char* dlc_installed_title;
  const char* dlc_remove;
  const char* dlc_remove_confirm;

  // The Keyboard and Gamepad pages with the game's own functions (game.h, GameControlsFor).
  const char* group_other;  // the controls no function is on

  // The profiles (profile_store.h): the first profile's dialog, the migration of an installation's saves.
  const char* profile_create_title;  // the first profile's dialog
  const char* profile_new_title;  // another profile's dialog
  const char* profile_create_text;
  const char* profile_name;
  const char* profile_name_hint;
  const char* profile_avatar;
  const char* profile_photo;  // a button under its camera icon: short
  const char* profile_photo_error;
  const char* profile_color;
  const char* profile_create;  // the dialog's button
  const char* profile_save_error;  // %s: why
  const char* migrate_conflict_title;  // %s: the game's name
  const char* migrate_conflict_text;
  const char* migrate_keep_installation;  // a button
  const char* migrate_keep_profile;  // a button
  const char* migrated;  // %s: the game's name
  const char* migrate_error;  // %s: why
  const char* profiles_newer;
  const char* saves_moved_note;  // a text file left in the game's user data folder; %s: the profile's folder

  // The profile pages (page_profile_*.cpp, profile_view.cpp).
  const char* level;  // %d: the level
  const char* see_my_profile;
  const char* edit;
  const char* profiles_shared;
  const char* used_today;
  const char* used_yesterday;
  const char* used_days_ago;  // %d: days
  const char* used_on;  // %s: a date
  const char* profile_tab_summary;
  const char* profile_tab_games;
  const char* edit_profile;
  const char* stat_achievements;  // upper case
  const char* stat_gamerscore;  // upper case
  const char* my_games;
  const char* last_seen;  // %s: today, yesterday or a date
  const char* today;
  const char* yesterday;
  const char* showcase;
  const char* showcase_empty;
  const char* recent_achievements;
  const char* recent_empty;
  const char* games_hint;
  const char* unlocked_on;  // %s: a date
  const char* profile_preview_note;
  const char* delete_profile;
  const char* delete_profile_confirm;  // the button's second press
  const char* delete_last_profile;
  const char* delete_failed;

  // The playtime (page_profile_summary.cpp, profile_view.cpp).
  const char* stat_hours;  // upper case
  const char* stat_hours_in_one;  // in one game
  const char* stat_hours_in;  // %d: games
  const char* day_one;
  const char* days_count;  // %d: days
  const char* weekly_activity;
  const char* week_total;  // %s: hours
  const char* weekdays;  // seven short names, Monday first, separated by commas
  const char* play_game;  // a game card's, not the Play page's
  const char* hours_played;  // %s: hours
  const char* open_launcher_failed;  // %s: the game's name
  // Launcher 3.0: the shell's footer and hints, the Play page.
  const char* hint_tabs;
  const char* hint_change;
  const char* hint_reset;
  const char* device_keyboard;  // the footer's controller: none connected
  const char* device_guitar;
  const char* device_drums;
  const char* device_gamepad;
  const char* settings_card;  // the Play page's card, upper case
  const char* all_up_to_date;
  const char* version_checked;  // %s: the version, %s: when updates were last checked
  const char* last_session;  // %s: when, %s: the hours played in all
  const char* no_session;
  // Launcher 3.0: the Settings tab (page_settings.cpp).
  const char* settings_intro_game;
  const char* settings_intro_video;
  const char* settings_intro_graphics;
  const char* settings_controls_note;
  const char* current_value;
  const char* reset_category;  // %s: the category
  const char* on_value;  // a switch's value
  const char* from_monitor;  // %s: the monitor's resolution
  const char* from_monitor_hint;
  const char* screen_effects;  // the Graphics category's group, upper case
  const char* screen_effects_needs;  // %s: the switch it needs
  const char* renderer_title;
  const char* renderer_native_sub;
  const char* renderer_xenos_sub;
  // Launcher 3.0: the Achievements tab (page_achievements.cpp).
  const char* secret_tag;  // a badge, upper case when drawn
  const char* secret_revealed_tag;
  const char* reveal_achievement;
  const char* hide_again;
  const char* reveal_note;
  const char* hide_note;
  const char* secret_detail;
  const char* still_locked;
  const char* hint_view_detail;
  const char* hint_reveal;
  const char* hint_filter;
  const char* filter_all_short;
  // Launcher 3.0: the DLC tab (page_dlc.cpp).
  const char* dlc_drop_here;
  const char* dlc_drop_or;
  const char* dlc_searching_short;
  const char* dlc_installing_short;  // %s: the package, %d of %d
  const char* dlc_installed_one;
  const char* dlc_installed_many;  // %d: packages
  const char* dlc_listed_hint;
  const char* dlc_none_found_short;
  const char* dlc_all_installed;
  const char* dlc_new_tag;  // a badge, upper case when drawn
  const char* dlc_count_one;
  const char* dlc_count;  // %d: packages
  const char* dlc_empty_title;
  const char* dlc_empty_text;
  const char* hint_choose_folder;
  // Launcher 3.0: the Updates tab (page_updates.cpp).
  const char* updates_checking_title;
  const char* updates_checking_sub;
  const char* updates_failed_title;
  const char* last_review;  // upper case when drawn
  const char* installed_version_short;  // upper case when drawn
  const char* release_notes_title;  // upper case when drawn
  const char* check_again;
  // Launcher 3.0: the Controls tab (page_gamepad.cpp, page_keyboard.cpp).
  const char* devices_detected;  // upper case when drawn
  const char* keyboard_status_on;
  const char* keyboard_status_off;
  const char* connect_more;
  const char* mapping_of;  // %s: the device, upper case when drawn
  const char* mapping_select_hint;
  const char* autosaved;
  const char* hint_reassign;
  const char* hint_clear;
  const char* help_guitar_kicker;
  const char* help_guitar_title;
  const char* help_guitar_text;
  const char* help_drums_kicker;
  const char* help_drums_title;
  const char* help_drums_text;
  const char* help_pad_kicker;
  const char* help_pad_title;
  const char* help_pad_text;
  const char* help_keyboard_kicker;
  const char* help_keyboard_title;
  const char* help_keyboard_text;
  const char* help_tip_this_controller;
  // Launcher 3.4: the whammy bar's row and its calibration (game::kHasWhammy).
  const char* whammy_bar;
  const char* whammy_calibrate;
  const char* whammy_calibrate_again;
  const char* whammy_automatic;   // after the bar's name: "Whammy bar · automatic"
  const char* whammy_calibrated;  // likewise
  const char* whammy_step_hold;
  const char* whammy_step_release;
  const char* whammy_failed;
  const char* help_tip_whammy;
  const char* help_tip_fixed;
  const char* help_tip_escape;
  const char* help_tip_clear;
  const char* no_controller_title;
  // Launcher 3.0: the Controls tab's keyboard hint.
  const char* mapping_select_key_hint;
  // Launcher 3.0: the first run (page_install.cpp).
  const char* first_run_kicker;  // upper case when drawn
  const char* step_of;  // %d: the step, %d: the steps; upper case when drawn
  const char* install_complete;  // upper case when drawn
  const char* browse_file;
  const char* finished;
  // Launcher 3.0: the profile screens (page_profiles.cpp, page_profile_*.cpp).
  const char* profiles_title;  // upper case when drawn
  const char* profiles_subtitle;
  const char* profile_current_tag;  // a badge, upper case when drawn
  const char* profile_switch_tag;  // a badge, upper case when drawn
  const char* in_use;
  const char* create_profile;
  const char* new_player;
  const char* use_profile_of;  // %s: the profile's name; upper case when drawn
  const char* hint_use_profile;
  const char* hint_sections;
  const char* hint_pin;
  const char* hint_choose;
  const char* stat_completed;  // upper case when drawn
  const char* stat_average;
  const char* stat_streak_short;  // upper case when drawn
  const char* stat_streak_best;  // %s: days
  const char* this_launcher;  // a badge, upper case when drawn
  const char* open_its_launcher;
  const char* not_installed;
  const char* game_stats_line;  // %d / %d achievements, %d / %d G; then the showcase
  const char* showcase_count;  // %d of 3
  const char* preview_kicker;  // upper case when drawn
  const char* your_name;  // the preview without a name; upper case when drawn
  const char* new_profile_sub;
  const char* motto_label;
  const char* motto_hint;
  const char* ring_color;
  const char* save;
  const char* name_needed;
  // Launcher 3.0: a Settings row changed with A (page_settings.cpp).
  const char* hint_edit;
  const char* hint_done;
  // Launcher 3.0: the View button's hint on Play (the profiles).
  const char* hint_profile;
  // Launcher 3.1: singing (game::kHasMicrophones, page_controls.cpp): the microphone among the Controls tab's
  // devices, its rows and help, and a controller's part.
  const char* device_microphone;
  const char* microphone_off;  // the device's status: no microphone chosen
  const char* microphone_device;
  const char* microphone_channel;
  const char* microphone_level;
  const char* microphone_none;
  const char* microphone_system;  // the system's default recording device
  const char* microphone_missing;  // %s: a chosen device that is not connected
  const char* microphone_all_channels;
  const char* microphone_channel_n;  // %d
  const char* microphone_mono;
  const char* microphone_singers;  // %s: the controllers whose part is the vocals
  const char* microphone_no_singer;
  const char* part;  // a controller's row: what it plays
  const char* part_instrument;  // game.cpp's word first
  const char* part_vocals;  // game.cpp's word first
  const char* help_mic_kicker;
  const char* help_mic_title;
  const char* help_mic_text;
  const char* help_mic_tip_level;
  const char* help_mic_tip_channel;
  const char* help_mic_tip_privacy;
  const char* help_tip_part;
  // Launcher 3.1: the microphone's gain, and the help of a controller that sings.
  const char* microphone_gain;
  const char* help_vocals_title;
  const char* help_vocals_text;
  // Launcher 3.3: a controller's switch for its frets' strum (game::kKeyboardTapTarget).
  const char* help_tip_gamepad_tap;
};

// The Windows display language as an XLanguage id, like the SDK's user_language: 1 English, 3 German,
// 4 French, 5 Spanish, 6 Italian.
uint32_t SystemLanguage();
const Texts& TextsFor(uint32_t language);

}  // namespace launcher
