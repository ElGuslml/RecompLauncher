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
# Windows only: SDL3 and Dear ImGui linked statically, drawn with Direct3D 11, nanosvg for the icons and art.

set(RECOMP_LAUNCHER_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(recomp_launcher_add target)
  cmake_parse_arguments(ARG "" "OUTPUT_NAME;GAME_DIR;RC;SDK_DIR;DEFAULT_GAME_ROOT" "INCLUDE_DIRS" ${ARGN})
  foreach(required OUTPUT_NAME GAME_DIR RC SDK_DIR)
    if(NOT ARG_${required})
      message(FATAL_ERROR "recomp_launcher_add(${target}): ${required} is missing")
    endif()
  endforeach()
  set(dir "${RECOMP_LAUNCHER_DIR}")

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
  target_link_libraries(${target} PRIVATE shell32 ole32)
endfunction()
