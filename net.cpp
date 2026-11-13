// HTTP requests with WinHTTP (net.h).

#include "net.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <cwchar>
#include <memory>
#include <vector>

#include "game.h"

namespace launcher::net {
namespace {

std::wstring Wide(const std::string& text) {
  if (text.empty()) return {};
  int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
  std::wstring out(size_t(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), length);
  return out;
}

std::string Utf8(const std::wstring& text) {
  if (text.empty()) return {};
  int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
  std::string out(size_t(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), length, nullptr, nullptr);
  return out;
}

// The system's text for an error, WinHTTP's own included (they are in winhttp.dll).
std::string ErrorText(DWORD code) {
  wchar_t* buffer = nullptr;
  DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM;
  HMODULE winhttp = GetModuleHandleW(L"winhttp.dll");
  if (winhttp && code >= 12000 && code < 13000) {
    flags |= FORMAT_MESSAGE_FROM_HMODULE;
  }
  DWORD length = FormatMessageW(flags, winhttp, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
  std::wstring text = length ? std::wstring(buffer, length) : L"Error " + std::to_wstring(code);
  LocalFree(buffer);
  while (!text.empty() && (text.back() == L'\n' || text.back() == L'\r' || text.back() == L'.' || text.back() == L' ')) {
    text.pop_back();
  }
  return Utf8(text);
}

struct Handle {
  HINTERNET h = nullptr;
  ~Handle() {
    if (h) WinHttpCloseHandle(h);
  }
};

// One request, up to the response's headers. The caller reads the body.
struct Request {
  Handle session, connection, request;
  int status = 0;
  uint64_t length = 0;  // Content-Length, 0 if absent
  bool rate_limited = false;
  std::string error;

  bool Open(const std::string& url, const char* accept) {
    std::wstring wide_url = Wide(url);
    URL_COMPONENTS parts = {sizeof(parts)};
    wchar_t host[256] = {};
    wchar_t path[2048] = {};
    parts.lpszHostName = host;
    parts.dwHostNameLength = DWORD(std::size(host));
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = DWORD(std::size(path));
    parts.dwExtraInfoLength = 1;  // the query stays on the path
    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &parts)) {
      error = ErrorText(GetLastError());
      return false;
    }
    std::wstring object = std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength) +
                          std::wstring(parts.lpszExtraInfo ? parts.lpszExtraInfo : L"", parts.dwExtraInfoLength);
    // The launcher's name without spaces, and its version (GitHub wants a User-Agent).
    std::wstring agent = Wide(game::kLauncherName) + L"/" + Wide(game::kVersion);
    std::replace(agent.begin(), agent.end(), L' ', L'-');
    session.h = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session.h) {
      // Before Windows 8.1 (and on some Wine versions): the proxy set in the registry.
      session.h = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!session.h) {
      error = ErrorText(GetLastError());
      return false;
    }
    WinHttpSetTimeouts(session.h, 15000, 15000, 30000, 60000);
    connection.h = WinHttpConnect(session.h, host, parts.nPort, 0);
    if (!connection.h) {
      error = ErrorText(GetLastError());
      return false;
    }
    bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
    request.h = WinHttpOpenRequest(connection.h, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER,
                                   WINHTTP_DEFAULT_ACCEPT_TYPES, secure ? WINHTTP_FLAG_SECURE : 0);
    if (!request.h) {
      error = ErrorText(GetLastError());
      return false;
    }
    std::wstring headers = L"X-GitHub-Api-Version: 2022-11-28\r\n";
    if (accept) {
      headers += L"Accept: " + Wide(accept) + L"\r\n";
    }
    if (!WinHttpSendRequest(request.h, headers.c_str(), DWORD(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.h, nullptr)) {
      error = ErrorText(GetLastError());
      return false;
    }
    DWORD code = 0;
    DWORD size = sizeof(code);
    WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                        &code, &size, WINHTTP_NO_HEADER_INDEX);
    status = int(code);
    wchar_t text[32] = {};
    size = sizeof(text);
    if (WinHttpQueryHeaders(request.h, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, text, &size,
                            WINHTTP_NO_HEADER_INDEX)) {
      length = std::wcstoull(text, nullptr, 10);
    }
    size = sizeof(text);
    if ((status == 403 || status == 429) &&
        WinHttpQueryHeaders(request.h, WINHTTP_QUERY_CUSTOM, L"x-ratelimit-remaining", text, &size,
                            WINHTTP_NO_HEADER_INDEX)) {
      rate_limited = std::wcstoull(text, nullptr, 10) == 0;
    }
    return true;
  }

  // The next piece of the body: false at its end or on an error (`error` set then).
  bool Read(std::vector<char>& buffer, DWORD& got) {
    got = 0;
    if (!WinHttpReadData(request.h, buffer.data(), DWORD(buffer.size()), &got)) {
      error = ErrorText(GetLastError());
      return false;
    }
    return got > 0;
  }
};

// One request to a LAN address (a router): no proxy, short timeouts. The status (0 when it did not answer) and the
// body (WinHTTP joins a chunked one).
int LanRequest(const std::string& url, const wchar_t* method, const std::wstring& headers, const std::string& body,
               std::string* reply) {
  std::wstring wide_url = Wide(url);
  URL_COMPONENTS parts = {sizeof(parts)};
  wchar_t host[256] = {};
  wchar_t path[1024] = {};
  parts.lpszHostName = host;
  parts.dwHostNameLength = DWORD(std::size(host));
  parts.lpszUrlPath = path;
  parts.dwUrlPathLength = DWORD(std::size(path));
  if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTP) {
    return 0;
  }
  Handle session, connection, request;
  session.h = WinHttpOpen(L"RecompLauncher", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session.h) return 0;
  WinHttpSetTimeouts(session.h, 2500, 2500, 2500, 2500);
  connection.h = WinHttpConnect(session.h, host, parts.nPort, 0);
  if (!connection.h) return 0;
  request.h = WinHttpOpenRequest(connection.h, method, std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength).c_str(),
                                 nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
  if (!request.h) return 0;
  if (!WinHttpSendRequest(request.h, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                          headers.empty() ? 0 : DWORD(-1L), body.empty() ? nullptr : const_cast<char*>(body.data()),
                          DWORD(body.size()), DWORD(body.size()), 0) ||
      !WinHttpReceiveResponse(request.h, nullptr)) {
    return 0;
  }
  DWORD code = 0;
  DWORD size = sizeof(code);
  WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                      &code, &size, WINHTTP_NO_HEADER_INDEX);
  char buffer[4096];
  DWORD got = 0;
  while (reply->size() < (1u << 20) && WinHttpReadData(request.h, buffer, sizeof(buffer), &got) && got) {
    reply->append(buffer, got);
  }
  return int(code);
}

// The text of the first <tag>...</tag> (or <prefix:tag>) at or after `from`, trimmed; empty if none.
std::string Element(const std::string& xml, const std::string& tag, size_t from = 0) {
  size_t at = from;
  while ((at = xml.find(tag, at)) != std::string::npos) {
    const size_t after = at + tag.size();
    size_t open = at;
    if (open > 0 && xml[open - 1] == ':') {
      --open;
      while (open > 0 && (std::isalnum(uint8_t(xml[open - 1])) || xml[open - 1] == '_' || xml[open - 1] == '-')) {
        --open;
      }
    }
    if (open > 0 && xml[open - 1] == '<' && after < xml.size() &&
        (xml[after] == '>' || xml[after] == ' ' || xml[after] == '\t')) {
      const size_t text = xml.find('>', after);
      const size_t close = text == std::string::npos ? text : xml.find("</", text);
      if (close == std::string::npos) return {};
      std::string value = xml.substr(text + 1, close - text - 1);
      while (!value.empty() && std::isspace(uint8_t(value.back()))) value.pop_back();
      size_t start = 0;
      while (start < value.size() && std::isspace(uint8_t(value[start]))) ++start;
      return value.substr(start);
    }
    at = after;
  }
  return {};
}

// 10/8, 172.16/12, 192.168/16, 100.64/10 (carrier-grade NAT), 169.254/16, 127/8.
bool PrivateAddress(const uint8_t* b) {
  return b[0] == 10 || b[0] == 127 || (b[0] == 172 && b[1] >= 16 && b[1] <= 31) || (b[0] == 192 && b[1] == 168) ||
         (b[0] == 100 && b[1] >= 64 && b[1] <= 127) || (b[0] == 169 && b[1] == 254);
}

// The SSDP search on the LAN: the description URLs of the Internet gateways inside it that answered in 3 seconds.
std::vector<std::string> FindGateways() {
  std::vector<std::string> locations;
  WSADATA wsa;
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return locations;
  SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s != INVALID_SOCKET) {
    DWORD ttl = 2;
    setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char*>(&ttl), sizeof(ttl));
    sockaddr_in to{};
    to.sin_family = AF_INET;
    to.sin_port = htons(1900);
    inet_pton(AF_INET, "239.255.255.250", &to.sin_addr);
    for (const char* target : {"urn:schemas-upnp-org:device:InternetGatewayDevice:1",
                               "urn:schemas-upnp-org:device:InternetGatewayDevice:2"}) {
      std::string search = std::string("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\n") +
                           "MAN: \"ssdp:discover\"\r\nMX: 2\r\nST: " + target + "\r\n\r\n";
      sendto(s, search.data(), int(search.size()), 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
    }
    const ULONGLONG deadline = GetTickCount64() + 3000;
    char buffer[2048];
    for (ULONGLONG now = GetTickCount64(); now < deadline; now = GetTickCount64()) {
      fd_set set;
      FD_ZERO(&set);
      FD_SET(s, &set);
      const ULONGLONG left = deadline - now;
      timeval timeout{long(left / 1000), long((left % 1000) * 1000)};
      if (select(0, &set, nullptr, nullptr, &timeout) <= 0) break;
      int n = recv(s, buffer, sizeof(buffer), 0);
      if (n <= 0) continue;
      std::string reply(buffer, size_t(n));
      std::string lower = reply;
      for (char& c : lower) c = char(std::tolower(uint8_t(c)));
      size_t at = lower.find("\r\nlocation:");
      if (at == std::string::npos) continue;
      size_t value = at + 11;
      size_t end = reply.find("\r\n", value);
      std::string location = reply.substr(value, end == std::string::npos ? std::string::npos : end - value);
      location.erase(0, location.find_first_not_of(' '));
      while (!location.empty() && location.back() == ' ') location.pop_back();
      // Only routers inside the LAN: "http://a.b.c.d[:port]/..." with a private address.
      if (location.compare(0, 7, "http://") != 0) continue;
      std::string host = location.substr(7, location.find_first_of(":/", 7) - 7);
      in_addr address{};
      if (inet_pton(AF_INET, host.c_str(), &address) != 1 ||
          !PrivateAddress(reinterpret_cast<const uint8_t*>(&address))) {
        continue;
      }
      if (std::find(locations.begin(), locations.end(), location) == locations.end()) locations.push_back(location);
    }
    closesocket(s);
  }
  WSACleanup();
  return locations;
}

}  // namespace

Response Get(const std::string& url, const char* accept) {
  Response response;
  Request request;
  if (!request.Open(url, accept)) {
    response.error = request.error;
    return response;
  }
  response.status = request.status;
  response.rate_limited = request.rate_limited;
  std::vector<char> buffer(64 * 1024);
  DWORD got = 0;
  while (request.Read(buffer, got)) {
    response.body.append(buffer.data(), got);
  }
  if (!request.error.empty()) {
    response.status = 0;
    response.error = request.error;
  }
  return response;
}

bool Download(const std::string& url, const std::filesystem::path& file,
              const std::function<void(uint64_t, uint64_t)>& progress, const std::atomic<bool>& cancel,
              std::string* error) {
  Request request;
  if (!request.Open(url, "application/octet-stream")) {
    *error = request.error;
    return false;
  }
  if (request.status != 200) {
    *error = "HTTP " + std::to_string(request.status);
    return false;
  }
  HANDLE out = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (out == INVALID_HANDLE_VALUE) {
    *error = ErrorText(GetLastError());
    return false;
  }
  std::vector<char> buffer(1024 * 1024);
  uint64_t done = 0;
  DWORD got = 0;
  bool ok = true;
  while (!cancel && request.Read(buffer, got)) {
    DWORD wrote = 0;
    if (!WriteFile(out, buffer.data(), got, &wrote, nullptr) || wrote != got) {
      *error = ErrorText(GetLastError());
      ok = false;
      break;
    }
    done += got;
    progress(done, request.length);
  }
  CloseHandle(out);
  if (ok && !request.error.empty()) {
    *error = request.error;
    ok = false;
  }
  if (ok && cancel) {
    ok = false;
  }
  if (ok && request.length && done != request.length) {
    *error = "the download ended early (" + std::to_string(done) + " of " + std::to_string(request.length) + " bytes)";
    ok = false;
  }
  if (!ok) {
    std::error_code ec;
    std::filesystem::remove(file, ec);
  }
  return ok;
}

std::vector<LocalAddress> LocalAddresses() {
  std::vector<uint8_t> buffer;
  ULONG size = 16 * 1024;
  ULONG result = ERROR_BUFFER_OVERFLOW;
  constexpr ULONG kFlags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                           GAA_FLAG_SKIP_DNS_SERVER;
  for (int attempt = 0; attempt < 3 && result == ERROR_BUFFER_OVERFLOW; ++attempt) {
    buffer.resize(size);
    result = GetAdaptersAddresses(AF_INET, kFlags, nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()),
                                  &size);
  }
  if (result != NO_ERROR) {
    return {};
  }
  std::vector<LocalAddress> routed;
  std::vector<LocalAddress> others;
  for (auto* adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data()); adapter; adapter = adapter->Next) {
    if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
      continue;
    }
    std::string name;
    if (adapter->FriendlyName) {
      int length = WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, nullptr, 0, nullptr, nullptr);
      if (length > 1) {
        name.resize(size_t(length - 1));
        WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, name.data(), length, nullptr, nullptr);
      }
    }
    for (auto* unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
      const sockaddr* address = unicast->Address.lpSockaddr;
      if (!address || address->sa_family != AF_INET) {
        continue;
      }
      const auto* bytes = reinterpret_cast<const uint8_t*>(&reinterpret_cast<const sockaddr_in*>(address)->sin_addr);
      if (bytes[0] == 127 || (bytes[0] == 169 && bytes[1] == 254)) {
        continue;
      }
      LocalAddress local;
      local.address = std::to_string(bytes[0]) + "." + std::to_string(bytes[1]) + "." + std::to_string(bytes[2]) +
                      "." + std::to_string(bytes[3]);
      local.adapter = name;
      (adapter->FirstGatewayAddress ? routed : others).push_back(std::move(local));
    }
  }
  routed.insert(routed.end(), others.begin(), others.end());
  return routed;
}

RouterAddress AskRouter() {
  RouterAddress result;
  for (const std::string& location : FindGateways()) {
    std::string description;
    if (LanRequest(location, L"GET", {}, {}, &description) != 200) continue;
    // The WAN connection service (IP, else PPP) and its control URL, relative to the description's host.
    std::string root = location.substr(0, location.find('/', 7));
    std::string base = Element(description, "URLBase");
    if (base.compare(0, 7, "http://") == 0) root = base.substr(0, base.find('/', 7));
    std::string service_type, control;
    for (const char* kind : {"WANIPConnection", "WANPPPConnection"}) {
      for (size_t at = 0; service_type.empty() && (at = description.find("<service>", at)) != std::string::npos;) {
        size_t end = description.find("</service>", at);
        if (end == std::string::npos) break;
        std::string service = description.substr(at, end - at);
        at = end;
        std::string type = Element(service, "serviceType");
        std::string url = Element(service, "controlURL");
        if (type.find(kind) == std::string::npos || url.empty()) continue;
        service_type = type;
        control = url.compare(0, 7, "http://") == 0 ? url
                  : url[0] == '/'                   ? root + url
                                                    : location.substr(0, location.rfind('/') + 1) + url;
      }
      if (!service_type.empty()) break;
    }
    if (service_type.empty()) continue;
    std::string body =
        "<?xml version=\"1.0\"?>\r\n<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:GetExternalIPAddress xmlns:u=\"" +
        service_type + "\"></u:GetExternalIPAddress></s:Body></s:Envelope>\r\n";
    std::wstring headers = L"Content-Type: text/xml; charset=\"utf-8\"\r\nSOAPAction: \"" + Wide(service_type) +
                           L"#GetExternalIPAddress\"\r\n";
    std::string reply;
    if (LanRequest(control, L"POST", headers, body, &reply) != 200) continue;
    std::string text = Element(reply, "NewExternalIPAddress");
    in_addr address{};
    if (text.empty() || inet_pton(AF_INET, text.c_str(), &address) != 1 || address.s_addr == 0) continue;
    result.address = text;
    result.kind = PrivateAddress(reinterpret_cast<const uint8_t*>(&address)) ? RouterAddress::Kind::kPrivate
                                                                              : RouterAddress::Kind::kPublic;
    return result;
  }
  return result;
}

}  // namespace launcher::net
