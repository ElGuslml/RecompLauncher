// HTTP requests outside Windows (net.h), with libcurl (part of macOS). The same requests as net.cpp's WinHTTP ones:
// the launcher's User-Agent, GitHub's API version header, redirects followed, the same timeouts.

#include "net.h"

#include <curl/curl.h>

#include <algorithm>
#include <cstdio>
#include <mutex>

#include "game.h"

namespace launcher::net {
namespace {

void GlobalInit() {
  static std::once_flag once;
  std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

std::string Agent() {
  std::string agent = std::string(game::kLauncherName) + "/" + game::kVersion;
  std::replace(agent.begin(), agent.end(), ' ', '-');
  return agent;
}

struct Request {
  CURL* curl = nullptr;
  curl_slist* headers = nullptr;
  bool rate_limit_zero = false;

  explicit Request(const std::string& url, const char* accept) {
    GlobalInit();
    curl = curl_easy_init();
    if (!curl) return;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, Agent().c_str());
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 15000L);
    // No progress for 60 s: the transfer has stalled (WinHTTP's receive timeout).
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    headers = curl_slist_append(headers, "X-GitHub-Api-Version: 2022-11-28");
    if (accept) headers = curl_slist_append(headers, (std::string("Accept: ") + accept).c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, &Request::Header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, this);
  }
  ~Request() {
    if (headers) curl_slist_free_all(headers);
    if (curl) curl_easy_cleanup(curl);
  }

  static size_t Header(char* data, size_t size, size_t count, void* user) {
    auto* self = static_cast<Request*>(user);
    std::string line(data, size * count);
    std::string lower = line;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    // Each response of a redirect chain starts with its status line: only the last one's headers count.
    if (lower.rfind("http/", 0) == 0) self->rate_limit_zero = false;
    if (lower.rfind("x-ratelimit-remaining:", 0) == 0) {
      std::string value = lower.substr(22);
      value.erase(0, value.find_first_not_of(" \t"));
      self->rate_limit_zero = value.rfind('0', 0) == 0 && (value.size() == 1 || !std::isdigit(uint8_t(value[1])));
    }
    return size * count;
  }

  int Status() const {
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    return int(status);
  }
};

size_t AppendBody(char* data, size_t size, size_t count, void* user) {
  static_cast<std::string*>(user)->append(data, size * count);
  return size * count;
}

}  // namespace

Response Get(const std::string& url, const char* accept) {
  Response response;
  Request request(url, accept);
  if (!request.curl) {
    response.error = "curl_easy_init failed";
    return response;
  }
  curl_easy_setopt(request.curl, CURLOPT_TIMEOUT_MS, 60000L);
  curl_easy_setopt(request.curl, CURLOPT_WRITEFUNCTION, &AppendBody);
  curl_easy_setopt(request.curl, CURLOPT_WRITEDATA, &response.body);
  CURLcode result = curl_easy_perform(request.curl);
  if (result != CURLE_OK) {
    response.error = curl_easy_strerror(result);
    response.body.clear();
    return response;
  }
  response.status = request.Status();
  response.rate_limited = (response.status == 403 || response.status == 429) && request.rate_limit_zero;
  return response;
}

namespace {

struct DownloadState {
  std::FILE* file = nullptr;
  const std::function<void(uint64_t, uint64_t)>* progress = nullptr;
  const std::atomic<bool>* cancel = nullptr;
  bool write_failed = false;
};

size_t WriteFile(char* data, size_t size, size_t count, void* user) {
  auto* state = static_cast<DownloadState*>(user);
  if (std::fwrite(data, 1, size * count, state->file) != size * count) {
    state->write_failed = true;
    return 0;
  }
  return size * count;
}

int Progress(void* user, curl_off_t total, curl_off_t done, curl_off_t, curl_off_t) {
  auto* state = static_cast<DownloadState*>(user);
  if (state->cancel->load()) return 1;
  if (*state->progress) (*state->progress)(uint64_t(done), uint64_t(total));
  return 0;
}

}  // namespace

bool Download(const std::string& url, const std::filesystem::path& file,
              const std::function<void(uint64_t done, uint64_t total)>& progress, const std::atomic<bool>& cancel,
              std::string* error) {
  Request request(url, "application/octet-stream");
  if (!request.curl) {
    if (error) *error = "curl_easy_init failed";
    return false;
  }
  DownloadState state;
  state.progress = &progress;
  state.cancel = &cancel;
  state.file = std::fopen(file.c_str(), "wb");
  if (!state.file) {
    if (error) *error = "cannot write " + file.string();
    return false;
  }
  curl_easy_setopt(request.curl, CURLOPT_WRITEFUNCTION, &WriteFile);
  curl_easy_setopt(request.curl, CURLOPT_WRITEDATA, &state);
  curl_easy_setopt(request.curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(request.curl, CURLOPT_XFERINFOFUNCTION, &Progress);
  curl_easy_setopt(request.curl, CURLOPT_XFERINFODATA, &state);
  curl_easy_setopt(request.curl, CURLOPT_FAILONERROR, 1L);
  CURLcode result = curl_easy_perform(request.curl);
  bool closed = std::fclose(state.file) == 0;
  if (result != CURLE_OK || !closed || state.write_failed) {
    if (error) {
      *error = cancel ? "cancelled"
               : state.write_failed || !closed ? "cannot write " + file.string()
                                                : std::string(curl_easy_strerror(result));
    }
    std::error_code ec;
    std::filesystem::remove(file, ec);
    return false;
  }
  return true;
}

}  // namespace launcher::net
