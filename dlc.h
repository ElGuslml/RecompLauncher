// Downloadable content (the DLC page), in the game's mode (game::kDlcMode):
// - kFolder (page_dlc.cpp): the player's DLC folder, Xbox 360 content packages the game reads where they are (the SDK's
//   content_packages.txt, its change 80: nothing copied, one folder for every game), each turned on or off for the
//   game; and the packages extracted before, into the user data's content folder.
// - kInstall (page_dlc_install.cpp): the packages of a folder the player chooses, extracted into the content folder
//   by the game itself (its --install_content tool mode), each turned on or off.

#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace launcher {

// An Xbox 360 content package (an STFS file: LIVE, PIRS or CON) found to install (kInstall), from its header.
struct DlcPackage {
  std::filesystem::path path;
  std::string file_name;     // the package's file name: the folder it is installed in
  std::string display_name;  // UTF-8; the file name when the package has none
  std::string title_name;    // the game it was made for, UTF-8 (may be empty)
  uint32_t title_id = 0;
  uint32_t content_type = 0;
  uint64_t content_size = 0;  // the data inside, about the installed size
  bool installed = false;     // a folder of its name is in the DLC folder already
  // Downloadable content (Marketplace content, type 2): the only kind the page installs.
  bool IsDlc() const { return content_type == 2; }
};

// Content installed for the game: a folder in the DLC folder, named after its package, or in the folder of DLC turned
// off beside it (the launchers before 3.16 extracted each package).
struct InstalledDlc {
  std::string folder;        // its name, UTF-8
  std::string display_name;  // from the header the game wrote next to it, else the folder's name
  uint64_t bytes = 0;
  bool on = true;     // in the DLC folder, where the game sees it; else in the folder of DLC turned off
  bool fits = true;   // the game reads it (game::kDlcRule)
  bool turned_off_now = false;  // not one the game reads, and turned off by this listing
};

// A package of the player's DLC folder: an Xbox 360 content package file (STFS: LIVE, PIRS or CON) of downloadable
// content, read by the game where it is while it is on.
struct LibraryDlc {
  std::filesystem::path path;
  std::string relative;      // its path in the folder, UTF-8: the key of the player's choice
  std::string display_name;  // UTF-8; the file's name when the package has none
  uint32_t title_id = 0;     // the game it was made for (the page groups by it)
  std::string title_name;    // that game's name in the header, UTF-8 (may be empty)
  uint64_t bytes = 0;
  bool fits = true;  // the game reads it (game::kDlcRule)
  bool on = false;   // in the game's list: by default when it fits, else by the player's choice
  // An extracted copy of it installed by an earlier launcher (its folder's name, UTF-8; empty: none) and its size. The
  // game reads the copy, so the package stays out of the game's list while the copy is there (no song twice).
  std::string copy_folder;
  uint64_t copy_bytes = 0;
};

// The player's DLC folder and its packages.
struct DlcLibrary {
  std::filesystem::path folder;  // empty: none chosen
  bool available = false;        // the folder is there (its drive connected)
  std::vector<LibraryDlc> packages;
  size_t Fitting() const;      // the packages the game reads
  size_t Copies() const;       // the packages with an extracted copy installed
  uint64_t CopyBytes() const;  // the copies' size
};

// A package's picture of its game: its header's title thumbnail, else its thumbnail; a PNG as the package has it,
// empty when it has none or is not a package. The disc's nxeart package is the dashboard's art for the game.
std::vector<uint8_t> PackageTitleThumbnail(const std::filesystem::path& path);

// The game's user data folder: userdata\ next to the game when it can be written there, else
// Documents\<game::kDocumentsFolder> (src/startup.cpp, ConfigureUserData).
std::filesystem::path GameUserDataFolder(const std::filesystem::path& exe_dir);
// Where the game's DLC is installed: <user data>\0000000000000000\<title ID>\00000002 (the SDK's ContentManager).
std::filesystem::path DlcFolder(const std::filesystem::path& exe_dir);
// Where a package turned off waits: <title ID>\Disabled DLC, beside the DLC folder (the game lists only that one's
// folders). Its "turned on by the player.txt" names the packages the game does not read that the player turned on.
std::filesystem::path DisabledDlcFolder(const std::filesystem::path& exe_dir);

// What is installed, on and off, by name. A package on that the game does not read (game::kDlcRule) is turned off
// first, unless the player turned it on. Sums the folders' sizes and reads the manifests: run it off the UI thread.
std::vector<InstalledDlc> ListInstalledDlc(const std::filesystem::path& exe_dir);
// Turns an installed package on (into the DLC folder) or off (into the folder of DLC turned off); the player's choice
// is kept, so a package the game does not read stays on once turned on.
bool SetDlcOn(const std::filesystem::path& exe_dir, const std::string& folder, bool on);
// Removes an installed package's folder, on or off, and its header.
bool RemoveDlc(const std::filesystem::path& exe_dir, const std::string& folder);

// The player's DLC folder, kept in <title ID>\dlc_library.txt beside the DLC folder with the player's choices (a
// package turned on although the game does not read it, or off although it does; by its path in the folder).
std::filesystem::path DlcLibraryFolder(const std::filesystem::path& exe_dir);
// Chooses the folder; the choices of its packages are kept.
void SetDlcLibraryFolder(const std::filesystem::path& exe_dir, const std::filesystem::path& folder);
// The folder's packages (it and its subfolders, a few levels deep): their headers and file tables, a few KB each,
// checked against game::kDlcRule; then the game's list (<title ID>\content_packages.txt) is written with those on.
// Run it off the UI thread: a folder of 200 packages on a hard disk takes seconds the first time.
DlcLibrary ScanDlcLibrary(const std::filesystem::path& exe_dir);
// Turns a package of the folder on or off for the game: the choice kept, the game's list written again.
void SetLibraryDlcOn(const std::filesystem::path& exe_dir, DlcLibrary& library, size_t index, bool on);
// Deletes the extracted copies of the folder's packages (their folders and headers; the player confirmed it): the
// packages, read from the folder, take their place once the folder is read again. Run it off the UI thread.
struct FreedDlc {
  size_t copies = 0;
  uint64_t bytes = 0;
};
FreedDlc RemoveDlcCopies(const std::filesystem::path& exe_dir, const DlcLibrary& library);

// kInstall: the packages in `path` (a package itself, or a folder and its subfolders, a few levels deep), with
// `installed` set against the DLC folder and the folder of DLC turned off. Reads a few KB of each file: run it off the
// UI thread.
std::vector<DlcPackage> FindDlcPackages(const std::filesystem::path& path, const std::filesystem::path& exe_dir);

// kInstall: packages installed one after another, on a thread of its own: the game in its --install_content tool mode
// for each (no window), its progress the installed folder's size against the package's content size.
class DlcInstall {
 public:
  enum class State { kIdle, kRunning, kDone };
  struct Progress {
    State state = State::kIdle;
    size_t index = 0;  // the package being installed
    size_t count = 0;
    std::string current;  // its name, UTF-8
    uint64_t done = 0;    // bytes of the packages so far
    uint64_t total = 0;
    size_t installed = 0;
    std::vector<std::string> failed;  // the names of those not installed
  };

  ~DlcInstall();
  void Start(const std::filesystem::path& exe_dir, std::vector<DlcPackage> packages);
  Progress Get() const;
  // Back to kIdle once its result has been shown.
  void Reset();

 private:
  void Run(std::filesystem::path exe_dir, std::vector<DlcPackage> packages);

  std::thread worker_;
  mutable std::mutex mutex_;
  Progress progress_;
};

}  // namespace launcher
