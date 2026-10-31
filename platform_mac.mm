// The launcher on macOS (platform.h): what needs AppKit, Foundation or CommonCrypto.

#include "platform.h"

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#include <CommonCrypto/CommonDigest.h>
#include <mach-o/dyld.h>

#include <cstdio>
#include <fstream>
#include <vector>

namespace launcher::platform {

std::filesystem::path ExecutableFolder() {
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> buffer(size + 1, '\0');
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
  std::error_code ec;
  std::filesystem::path path = std::filesystem::canonical(buffer.data(), ec);
  if (ec) path = buffer.data();
  return path.parent_path();
}

namespace {

std::filesystem::path SearchPath(NSSearchPathDirectory directory) {
  @autoreleasepool {
    NSArray<NSString*>* paths = NSSearchPathForDirectoriesInDomains(directory, NSUserDomainMask, YES);
    if (paths.count == 0) return {};
    return std::filesystem::path(paths.firstObject.UTF8String);
  }
}

}  // namespace

std::filesystem::path DocumentsFolder() {
  std::filesystem::path path = SearchPath(NSDocumentDirectory);
  return path.empty() ? HomeFolder() / "Documents" : path;
}

std::filesystem::path ApplicationSupportFolder() {
  std::filesystem::path path = SearchPath(NSApplicationSupportDirectory);
  return path.empty() ? HomeFolder() / "Library" / "Application Support" : path;
}

std::string PreferredLanguage() {
  @autoreleasepool {
    NSString* language = NSLocale.preferredLanguages.firstObject;
    if (!language) return "en-US";
    std::string tag = language.UTF8String;
    // The region, when the language has none ("es" with the region set to Mexico).
    NSString* region = [NSLocale.currentLocale objectForKey:NSLocaleCountryCode];
    if (region && tag.find('-') == std::string::npos) tag += "-" + std::string(region.UTF8String);
    return tag;
  }
}

std::string UserName() {
  @autoreleasepool {
    NSString* name = NSFullUserName();
    if (name.length == 0) name = NSUserName();
    return name ? std::string(name.UTF8String) : std::string();
  }
}

std::string Upper(const std::string& text) {
  @autoreleasepool {
    NSString* s = [NSString stringWithUTF8String:text.c_str()];
    if (!s) return text;
    // Each letter on its own (no "ß" -> "SS"): the length stays, as Windows' CharUpperBuff keeps it.
    NSMutableString* out = [NSMutableString stringWithCapacity:s.length];
    [s enumerateSubstringsInRange:NSMakeRange(0, s.length)
                          options:NSStringEnumerationByComposedCharacterSequences
                       usingBlock:^(NSString* c, NSRange, NSRange, BOOL*) {
                         NSString* u = c.uppercaseString;
                         [out appendString:u.length == c.length ? u : c];
                       }];
    return std::string(out.UTF8String);
  }
}

bool OpenInSystem(const std::string& path_or_url) {
  @autoreleasepool {
    NSString* text = [NSString stringWithUTF8String:path_or_url.c_str()];
    if (!text) return false;
    NSURL* url = [text hasPrefix:@"/"] ? [NSURL fileURLWithPath:text] : [NSURL URLWithString:text];
    return url && [NSWorkspace.sharedWorkspace openURL:url];
  }
}

bool MoveToTrash(const std::filesystem::path& path) {
  @autoreleasepool {
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
    return url && [NSFileManager.defaultManager trashItemAtURL:url resultingItemURL:nil error:nil];
  }
}

std::string Sha256File(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return {};
  CC_SHA256_CTX context;
  CC_SHA256_Init(&context);
  std::vector<char> buffer(1 << 20);
  while (file) {
    file.read(buffer.data(), std::streamsize(buffer.size()));
    std::streamsize got = file.gcount();
    if (got > 0) CC_SHA256_Update(&context, buffer.data(), CC_LONG(got));
  }
  if (file.bad()) return {};
  unsigned char digest[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256_Final(digest, &context);
  std::string hex;
  char byte[3];
  for (unsigned char d : digest) {
    std::snprintf(byte, sizeof(byte), "%02x", d);
    hex += byte;
  }
  return hex;
}

bool Process::WindowShown() const {
  if (pid_ <= 0) return false;
  bool found = false;
  CFArrayRef windows = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
                                                  kCGNullWindowID);
  if (!windows) return false;
  for (CFIndex i = 0, n = CFArrayGetCount(windows); i < n && !found; ++i) {
    auto info = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(windows, i));
    int owner = 0;
    int layer = -1;
    auto owner_number = static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowOwnerPID));
    auto layer_number = static_cast<CFNumberRef>(CFDictionaryGetValue(info, kCGWindowLayer));
    if (owner_number) CFNumberGetValue(owner_number, kCFNumberIntType, &owner);
    if (layer_number) CFNumberGetValue(layer_number, kCFNumberIntType, &layer);
    // A normal window (layer 0) or an alert of the game (a different version of default.xex).
    found = owner == pid_ && layer >= 0;
  }
  CFRelease(windows);
  return found;
}

void Activate() {
  @autoreleasepool {
    [NSApp activateIgnoringOtherApps:YES];
  }
}

}  // namespace launcher::platform
