# RecompLauncher

The launcher of Gustavo's Xbox 360 recompilations (KameoRecomp, PiratesAtWorldsEndRecomp, GRAWRecomp,
GHMetalRecomp): one program per game, `<Game> Launcher.exe`, next to the recompiled executable. It holds the
game's options, installs the game from the player's ISO the first time, shows its achievements, installs its
downloadable content and updates it from the game's GitHub releases. "Play" starts the game, and the launcher
comes back when it closes.

The players' profiles are shared by every launcher (`profile_store.h`, `%APPDATA%\ElGuslml Recomps\`): each game
keeps the current profile's saves, settings and achievements in the profile's folder. Each launcher registers its
game there with its achievement list, so the profile's pages show every recompilation the player has run: level,
Gamerscore, each game's achievements and a showcase of pinned ones, the time played (counted by the launcher from Play
to the game's exit) with its streaks and the week's activity, and Play on another game, which opens that game's
launcher.

Each game takes this repository as a submodule, `recomp_launcher/`. The code is the same for every game; what
belongs to one game stays in its own repository.

## What a game provides

| File | What it is |
|---|---|
| `launcher/game.h` | The game's profile in `namespace launcher::game`: names, files, disc, look, options, languages, keyboard defaults, which pages it has (the types are `game_types.h`'s). |
| `launcher/game.cpp` | Its texts in the launcher's five languages (`GameTextsFor`) and its controls in its own words (`GameControlsFor`). |
| `launcher/palette.h` | Its colors (`namespace col`) and corner scale. |
| `launcher/art/background.svg` | The art under every page, 1280 x 760 (nanosvg: no `<use>`, no CSS). |
| `res/launcher.rc` | The icon, the file properties, and the embedded fonts (`fonts/...`, from here) and art (`art/...`). |
| `res/version.h` | The game's version: the releases', the one the updater compares. |
| `src/project.h` | The names and options the game executable and the launcher share. |

KameoRecomp's, PiratesAtWorldsEndRecomp's or GHMetalRecomp's files are examples; recomp-tools' `New-RecompProject.ps1`
writes a new game's.

## Building

From the game's `CMakeLists.txt`:

```cmake
set(RECOMP_LAUNCHER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/recomp_launcher" CACHE PATH "The RecompLauncher tree")
include("${RECOMP_LAUNCHER_DIR}/RecompLauncher.cmake")
recomp_launcher_add(mygame_launcher
    OUTPUT_NAME "My Game Launcher"
    GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/launcher"
    RC "${CMAKE_CURRENT_SOURCE_DIR}/res/launcher.rc"
    SDK_DIR "${REXSDK_DIR}"
    INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/src" "${CMAKE_CURRENT_SOURCE_DIR}/res"
    DEFAULT_GAME_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/assets")
```

The game executable reads which profile it runs with (`profile_store.h`): `recomp_launcher_add_profiles(<game target>
SDK_DIR "${REXSDK_DIR}")`, and its `src/startup.cpp` sets the SDK's `profile_root` and `profile_name` from
`launcher::profiles::ForGame` (KameoRecomp's `ConfigureProfile`).

Windows and macOS. It needs the ReXGlue SDK tree (SDL3, Dear ImGui, toml++ and stb, and the SDK's `imgui` and
`SDL3::SDL3` targets) and builds with the game's toolchain (LLVM/Clang, Ninja; the Windows SDK on Windows, the Xcode
Command Line Tools on macOS). To try a working copy of this repository in a game, configure the game with
`-DRECOMP_LAUNCHER_DIR=<the working copy>`.

On macOS the same pages and logic are built with these in place of the Windows parts (`RecompLauncher.cmake`'s
`_recomp_launcher_add_macos`); the Windows code is unchanged, and the shared files only have `#ifdef _WIN32` blocks:

| Windows | macOS |
|---|---|
| `main.cpp` (WinMain, job object, `CreateProcess`) | `main_mac.cpp` (`posix_spawn`, the window hidden while the game runs) |
| `ui/gfx.cpp` (Direct3D 11) | `ui/gfx_metal.mm` (Metal, Dear ImGui's Metal backend) |
| `picture.cpp` (WIC) | `picture_mac.mm` (ImageIO) |
| `net.cpp` (WinHTTP) | `net_curl.cpp` (libcurl, part of macOS) |
| Win32 calls in the shared files | `platform.h`: `platform_posix.cpp`, `platform_mac.mm` |
| resources from the `.rc` script | the same script's `RCDATA` embedded at build time (`.incbin`) |

The settings, profiles and game files are in `~/Library/Application Support/<the game's folder>` (`platform::DataFolder`;
the app bundle is signed and may be read-only); the profiles' store in `~/Library/Application Support/ElGuslml
Recomps`. An update downloads the release's `...-macos-<arch>.dmg`, checks its SHA-256 and opens it.

## Versions

`launcher_version.h` holds the launcher's version, the same in every game, shown in the footer and on the Updates
page beside the game's own. `CHANGELOG.md` lists what each version brought. A change here is committed and pushed
first, then each game moves its submodule to it and is checked (recomp-tools' `Test-Games.ps1`).

## License

MIT (`LICENSE`). The fonts and nanosvg have their own licenses: `THIRD_PARTY_NOTICES.md`.
