# The launcher's executable for a game: include() this file, then call recomp_launcher_add().
#
#   recomp_launcher_add(<target>
#       OUTPUT_NAME <name>              the executable's name, without .exe ("Kameo Launcher")
#       GAME_DIR <dir>                  the game's own launcher files: game.h, game.cpp, palette.h, art/
#       RC <file>                       the game's resource script: the icon, the file properties, and the fonts and
#                                       art as "fonts/..." and "art/..." (found in this folder and GAME_DIR)
#       SDK_DIR <dir>                   the ReXGlue SDK tree: SDL3, Dear ImGui, toml++ and stb (its targets imgui and
#                                       SDL3::SDL3 must exist)
#       [INCLUDE_DIRS <dir>...]         more include directories (the game's src/ for project.h, res/ for version.h)
#       [DEFAULT_GAME_ROOT <dir>])      a development build's game files: the launcher then skips the first run
#
#   recomp_launcher_add_profiles(<game target> SDK_DIR <dir>)
#       the game executable reads which profile it runs with (profile_store.h's ForGame): the store's code compiled
#       into it, without the launcher's UI
#
# Windows: SDL3 and Dear ImGui linked statically, drawn with Direct3D 11, nanosvg for the icons and art.
# macOS: the same code, drawn with Metal (ui/gfx_metal.mm); the system's parts in platform_posix.cpp and platform_mac.mm,
# pictures through ImageIO (picture_mac.mm), HTTP through libcurl (net_curl.cpp). The RC script's fonts and art are
# embedded from the same script (recomp_launcher_embed_resources).

set(RECOMP_LAUNCHER_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(recomp_launcher_add target)
  cmake_parse_arguments(ARG "" "OUTPUT_NAME;GAME_DIR;RC;SDK_DIR;DEFAULT_GAME_ROOT" "INCLUDE_DIRS" ${ARGN})
  foreach(required OUTPUT_NAME GAME_DIR RC SDK_DIR)
    if(NOT ARG_${required})
      message(FATAL_ERROR "recomp_launcher_add(${target}): ${required} is missing")
    endif()
  endforeach()
  set(dir "${RECOMP_LAUNCHER_DIR}")
  if(APPLE)
    _recomp_launcher_add_macos(${target})
    return()
  endif()

  add_executable(${target} WIN32
      "${dir}/main.cpp"
      "${dir}/app.cpp"
      "${dir}/shell.cpp"
      "${dir}/settings.cpp"
      "${dir}/texts.cpp"
      "${dir}/page_play.cpp"
      "${dir}/page_settings.cpp"
      "${dir}/page_controls.cpp"
      "${dir}/page_keyboard.cpp"
      "${dir}/page_microphone.cpp"
      "${dir}/page_updates.cpp"
      "${dir}/page_achievements.cpp"
      "${dir}/page_install.cpp"
      "${dir}/page_dlc.cpp"
      "${dir}/dlc.cpp"
      "${dir}/iso.cpp"
      "${dir}/install.cpp"
      "${dir}/achievements.cpp"
      "${dir}/net.cpp"
      "${dir}/updater.cpp"
      "${dir}/zip.cpp"
      "${dir}/profile_store.cpp"
      "${dir}/page_profile.cpp"
      "${dir}/page_profiles.cpp"
      "${dir}/page_profile_summary.cpp"
      "${dir}/page_profile_game.cpp"
      "${dir}/page_profile_edit.cpp"
      "${dir}/profile_view.cpp"
      "${dir}/picture.cpp"
      "${dir}/ui/gfx.cpp"
      "${dir}/ui/ui.cpp"
      "${ARG_GAME_DIR}/game.cpp"
      "${ARG_SDK_DIR}/thirdparty/imgui/backends/imgui_impl_sdl3.cpp"
      "${ARG_SDK_DIR}/thirdparty/imgui/backends/imgui_impl_dx11.cpp"
      "${ARG_RC}")
  set_target_properties(${target} PROPERTIES OUTPUT_NAME "${ARG_OUTPUT_NAME}")
  target_compile_definitions(${target} PRIVATE UNICODE _UNICODE NOMINMAX IMGUI_DEFINE_MATH_OPERATORS)
  if(ARG_DEFAULT_GAME_ROOT)
    target_compile_definitions(${target} PRIVATE LAUNCHER_DEFAULT_GAME_ROOT="${ARG_DEFAULT_GAME_ROOT}")
  endif()
  # The game's folder first: its game.h and palette.h are what the common code includes.
  target_include_directories(${target} PRIVATE "${ARG_GAME_DIR}" "${dir}" ${ARG_INCLUDE_DIRS} "${dir}/thirdparty/nanosvg"
      "${ARG_SDK_DIR}/thirdparty/imgui/backends" "${ARG_SDK_DIR}/thirdparty/tomlplusplus"
      "${ARG_SDK_DIR}/thirdparty/inja/third_party/include" "${ARG_SDK_DIR}/thirdparty/stb")
  target_link_libraries(${target} PRIVATE imgui SDL3::SDL3 d3d11 dxgi d3dcompiler shcore windowscodecs ole32 winhttp
      bcrypt shell32 secur32)

  # The resource compiler gets the same include directories, where it also looks for the files a resource names: the
  # script finds "fonts/..." here and "art/..." in the game's folder. A change to any of them rebuilds the resources.
  file(GLOB resources CONFIGURE_DEPENDS "${dir}/fonts/*.ttf" "${ARG_GAME_DIR}/art/*.svg" "${dir}/launcher_version.h")
  set_source_files_properties("${ARG_RC}" PROPERTIES OBJECT_DEPENDS "${resources}")
endfunction()

function(recomp_launcher_add_profiles target)
  cmake_parse_arguments(ARG "" "SDK_DIR" "" ${ARGN})
  if(NOT ARG_SDK_DIR)
    message(FATAL_ERROR "recomp_launcher_add_profiles(${target}): SDK_DIR is missing")
  endif()
  target_sources(${target} PRIVATE "${RECOMP_LAUNCHER_DIR}/profile_store.cpp")
  target_include_directories(${target} PRIVATE "${RECOMP_LAUNCHER_DIR}" "${ARG_SDK_DIR}/thirdparty/tomlplusplus")
  if(WIN32)
    target_link_libraries(${target} PRIVATE shell32 ole32)
  else()
    target_sources(${target} PRIVATE "${RECOMP_LAUNCHER_DIR}/platform_posix.cpp")
    if(APPLE)
      target_sources(${target} PRIVATE "${RECOMP_LAUNCHER_DIR}/platform_mac.mm")
      set_source_files_properties("${RECOMP_LAUNCHER_DIR}/platform_mac.mm" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
      target_link_libraries(${target} PRIVATE "-framework AppKit" "-framework CoreGraphics" "-framework Foundation")
    endif()
  endif()
endfunction()

# The RC script's RCDATA resources (fonts, art) as a source file for the launcher outside Windows: each
# `<NAME> RCDATA "<file>"`, NAME's number from resources.h, the file found in `search_dirs`, embedded with .incbin and
# returned by ui::Resource(id).
function(_recomp_launcher_embed_resources target rc output search_dirs)
  file(STRINGS "${RECOMP_LAUNCHER_DIR}/resources.h" defines REGEX "^#define IDR_")
  file(STRINGS "${rc}" lines REGEX "^[A-Z_0-9]+[ \t]+RCDATA[ \t]+\"")
  set(asm "")
  set(table "")
  set(entries "")
  set(depends "")
  foreach(line IN LISTS lines)
    string(REGEX MATCH "^([A-Z_0-9]+)[ \t]+RCDATA[ \t]+\"([^\"]+)\"" _ "${line}")
    set(name "${CMAKE_MATCH_1}")
    set(relative "${CMAKE_MATCH_2}")
    set(id "")
    foreach(define IN LISTS defines)
      if(define MATCHES "^#define ${name}[ \t]+([0-9]+)")
        set(id "${CMAKE_MATCH_1}")
      endif()
    endforeach()
    set(found "")
    foreach(search IN LISTS search_dirs)
      if(NOT found AND EXISTS "${search}/${relative}")
        set(found "${search}/${relative}")
      endif()
    endforeach()
    if(NOT id OR NOT found)
      message(FATAL_ERROR "recomp_launcher: resource ${name} (${relative}) not found")
    endif()
    list(APPEND depends "${found}")
    string(APPEND asm "    \".globl _recomp_res_${id}\\n_recomp_res_${id}:\\n.incbin \\\"${found}\\\"\\n"
                      ".globl _recomp_res_${id}_end\\n_recomp_res_${id}_end:\\n\"\n")
    string(APPEND table "extern \"C\" const char recomp_res_${id}[], recomp_res_${id}_end[];\n")
    string(APPEND entries "    {${id}, recomp_res_${id}, recomp_res_${id}_end},\n")
  endforeach()
  file(CONFIGURE OUTPUT "${output}" CONTENT "// Generated by RecompLauncher.cmake from ${rc}.
#include <string_view>

__asm__(\".section __DATA,__const\\n\"
${asm}    \".text\\n\");

${table}
namespace ui {
std::string_view Resource(int id) {
  struct Entry { int id; const char* begin; const char* end; };
  static const Entry kEntries[] = {
${entries}  };
  for (const Entry& e : kEntries) {
    if (e.id == id) return std::string_view(e.begin, size_t(e.end - e.begin));
  }
  return {};
}
}  // namespace ui
" @ONLY)
  set_source_files_properties("${output}" PROPERTIES OBJECT_DEPENDS "${depends}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${rc}")
endfunction()

# recomp_launcher_add's macOS executable (its arguments are the caller's ARG_*).
macro(_recomp_launcher_add_macos target)
  set(_sources)
  foreach(source main_mac.cpp app.cpp shell.cpp settings.cpp texts.cpp page_play.cpp page_settings.cpp page_controls.cpp
                 page_keyboard.cpp page_microphone.cpp page_updates.cpp page_achievements.cpp page_install.cpp
                 page_dlc.cpp dlc.cpp iso.cpp install.cpp achievements.cpp updater.cpp zip.cpp profile_store.cpp
                 page_profile.cpp page_profiles.cpp page_profile_summary.cpp page_profile_game.cpp
                 page_profile_edit.cpp profile_view.cpp ui/ui.cpp
                 net_curl.cpp platform_posix.cpp platform_mac.mm picture_mac.mm ui/gfx_metal.mm)
    list(APPEND _sources "${dir}/${source}")
  endforeach()
  set(_resources "${CMAKE_CURRENT_BINARY_DIR}/${target}_resources.cpp")
  _recomp_launcher_embed_resources(${target} "${ARG_RC}" "${_resources}" "${dir};${ARG_GAME_DIR};${ARG_INCLUDE_DIRS}")
  add_executable(${target} ${_sources} "${ARG_GAME_DIR}/game.cpp" "${_resources}"
      "${ARG_SDK_DIR}/thirdparty/imgui/backends/imgui_impl_sdl3.cpp"
      "${ARG_SDK_DIR}/thirdparty/imgui/backends/imgui_impl_metal.mm")
  set_source_files_properties("${dir}/platform_mac.mm" "${dir}/picture_mac.mm" "${dir}/ui/gfx_metal.mm"
      "${ARG_SDK_DIR}/thirdparty/imgui/backends/imgui_impl_metal.mm" PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
  set_target_properties(${target} PROPERTIES OUTPUT_NAME "${ARG_OUTPUT_NAME}")
  target_compile_definitions(${target} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
  if(ARG_DEFAULT_GAME_ROOT)
    target_compile_definitions(${target} PRIVATE LAUNCHER_DEFAULT_GAME_ROOT="${ARG_DEFAULT_GAME_ROOT}")
  endif()
  target_include_directories(${target} PRIVATE "${ARG_GAME_DIR}" "${dir}" ${ARG_INCLUDE_DIRS} "${dir}/thirdparty/nanosvg"
      "${ARG_SDK_DIR}/thirdparty/imgui/backends" "${ARG_SDK_DIR}/thirdparty/tomlplusplus"
      "${ARG_SDK_DIR}/thirdparty/inja/third_party/include" "${ARG_SDK_DIR}/thirdparty/stb")
  find_package(CURL REQUIRED)
  target_link_libraries(${target} PRIVATE imgui SDL3::SDL3 CURL::libcurl "-framework Metal" "-framework QuartzCore"
      "-framework AppKit" "-framework CoreGraphics" "-framework Foundation" "-framework ImageIO"
      "-framework UniformTypeIdentifiers")
endmacro()
