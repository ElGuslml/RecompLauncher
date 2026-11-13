// HTTP requests for the launcher's updates, with WinHTTP (part of Windows, and of Wine and Proton). HTTPS for
// GitHub; plain HTTP works too, for tests against a local server (--launcher_update_url). And this PC's addresses.

#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace launcher::net {

struct Response {
  int status = 0;     // the HTTP status; 0 when no answer came
  std::string body;
  std::string error;  // the system's message when no answer came
  bool rate_limited = false;  // GitHub: 403 or 429 with no calls left (x-ratelimit-remaining: 0)
};

// GET into memory, following redirects. `accept`: the Accept header, or null.
Response Get(const std::string& url, const char* accept = nullptr);

// GET into `file`, following redirects (GitHub's assets redirect to their storage). `progress` gets the bytes so far
// and the total (0 if unknown). Stops when `cancel` becomes true. False with `error` set when it did not complete.
bool Download(const std::string& url, const std::filesystem::path& file,
              const std::function<void(uint64_t done, uint64_t total)>& progress, const std::atomic<bool>& cancel,
              std::string* error);

// This PC's IPv4 addresses on its networks (Settings > Online's "Your address"), each with its adapter's name:
// adapters with a default gateway first (the home network's), then the others (a virtual network program's), without
// the loopback and the link-local ones (169.254.x.x, no DHCP answer).
struct LocalAddress {
  std::string address;  // "192.168.1.20"
  std::string adapter;  // "Ethernet", "Wi-Fi", "ZeroTier One [...]"
};
std::vector<LocalAddress> LocalAddresses();

// The router's internet address, as the router reports it by UPnP (Settings > Online's "Internet" row; the game asks
// the router to forward its port when it hosts, the SDK's online_upnp). Read only: an SSDP search on the LAN, the
// router's description and its GetExternalIPAddress, nothing outside the LAN. Takes up to about 5 seconds: call it on a
// thread of its own.
struct RouterAddress {
  enum class Kind {
    kNoRouter,  // no router answered (UPnP off on it, or none): the port is forwarded by hand
    kPublic,    // `address` is what friends type over the internet
    kPrivate,   // the provider's carrier-grade NAT, or another router in front: no friend can reach this PC
  };
  Kind kind = Kind::kNoRouter;
  std::string address;  // "203.0.113.7" (kPublic, kPrivate)
};
RouterAddress AskRouter();

}  // namespace launcher::net
