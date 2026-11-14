// The launcher's updates from this project's GitHub releases (game.h: kReleasesApi; none when it is empty).
//
// - The check asks GitHub's API for the releases (no token: 60 calls an hour per address, so once per launcher start
//   and on request) and keeps them all but drafts, pre-releases marked. An update is offered from the published ones,
//   and from the pre-releases too when the player asks for betas (launcher 3.22, the Updates page's switch); an
//   installed pre-release's own notes are shown either way.
// - An update downloads the release's zip into update\ next to the launcher, checks its SHA-256 (the digest GitHub
//   gives for each asset, or a <zip>.sha256 asset), extracts it there, and moves its files into place. Each file it
//   replaces is renamed *.old-update first (Windows lets a running executable be renamed, so the launcher replaces
//   itself), and put back if anything fails. Never touched: assets\ (the game), userdata\ (saves, achievements,
//   caches), logs\ and the .toml files (the settings).
// - Then the launcher starts its new self (main.cpp). The next start deletes what is left (CleanUpAfterUpdate).
// Nothing here is specific to a game besides game.h's names.

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace launcher {

struct Version {
  int major = 0, minor = 0, patch = 0;
  bool valid = false;
  bool prerelease = false;  // a suffix after the numbers ("1.2.3-beta.1"): before 1.2.3, as in Semantic Versioning
  std::string suffix;       // the suffix without its '-' ("beta.1"), up to a '+' (build metadata, ignored)
  // "v1.2.3", "1.2.3" or "1.2", with or without a suffix.
  static Version Parse(const std::string& text);
  // Semantic Versioning's order: the numbers, then a version with a suffix before the same one without; two suffixes
  // by their dot-separated parts, numbers by value and below words, words in ASCII order, the shorter list first when
  // one starts the other ("beta.1" < "beta.2" < "beta.10" < "rc.1").
  bool operator<(const Version& o) const;
  bool operator==(const Version& o) const {
    return major == o.major && minor == o.minor && patch == o.patch && prerelease == o.prerelease &&
           (!prerelease || suffix == o.suffix);
  }
};

struct Release {
  std::string tag;           // "v0.2.0"
  Version version;
  std::string name;
  std::string notes;         // Markdown
  std::string published;     // ISO 8601, UTC
  std::string page;          // its page on GitHub
  std::string asset_name;    // the zip
  std::string asset_url;
  uint64_t asset_size = 0;
  std::string sha256;        // hex, lower case; empty when GitHub gave none
  std::string sha256_url;    // a <zip>.sha256 asset, when there is one
  bool prerelease = false;   // marked Pre-release on GitHub (a beta)
};

struct UpdateCheck {
  bool ok = false;
  bool rate_limited = false;
  std::string error;              // when !ok: the system's message, or "HTTP 404"
  std::vector<Release> releases;  // published and pre-releases (not drafts), newest first
  // The newest release newer than `running`, pre-releases only with `betas`, or null.
  const Release* Newer(const Version& running, bool betas) const;
  // The release of `version`, or null.
  const Release* Of(const Version& version) const;
};

// Blocking: for a thread. `url` is game::kReleasesApi, or --launcher_update_url for a test.
UpdateCheck CheckForUpdates(const std::string& url);

class Update {
 public:
  enum class State { kIdle, kDownloading, kVerifying, kInstalling, kDone, kFailed, kCancelled };
  // The failures the launcher explains in its language; the others show `error` as it is (the system's own
  // messages are already in the user's language).
  enum class Problem { kOther, kNoChecksum, kChecksumMismatch, kInUse };
  struct Progress {
    State state = State::kIdle;
    uint64_t done = 0, total = 0;  // bytes downloaded
    std::string error;
    Problem problem = Problem::kOther;
    std::string detail;  // kInUse: the file
  };

  ~Update();
  void Start(const Release& release, const std::filesystem::path& exe_dir);
  // Stops a download; the installation itself is not interrupted once it began.
  void Cancel();
  Progress Get() const;

 private:
  void Run(Release release, std::filesystem::path exe_dir);
  void Fail(const std::string& error, Problem problem = Problem::kOther, const std::string& detail = {});
  void Set(State state);

  std::thread worker_;
  std::atomic<bool> cancel_{false};
  mutable std::mutex mutex_;
  Progress progress_;
};

// At the launcher's start: deletes the update folder, and the replaced files an update could not delete because they
// were in use (the previous launcher's executable), which it listed in update-leftovers.txt; the previous launcher's
// only once it has exited.
void CleanUpAfterUpdate(const std::filesystem::path& exe_dir);

}  // namespace launcher
