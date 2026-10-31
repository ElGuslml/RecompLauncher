// Pictures from files for the profiles on macOS (picture.h), with ImageIO: what picture.cpp does with WIC.

#include "picture.h"

#import <CoreGraphics/CoreGraphics.h>
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include <algorithm>
#include <system_error>

namespace launcher {
namespace {

// `image`'s `source` rectangle drawn into a width x height RGBA picture, straight alpha (CoreGraphics draws
// premultiplied: undone after).
std::optional<Rgba> Draw(CGImageRef image, CGRect source, int width, int height) {
  if (!image || width <= 0 || height <= 0) return std::nullopt;
  Rgba picture;
  picture.width = width;
  picture.height = height;
  picture.pixels.assign(size_t(width) * size_t(height) * 4, 0);
  CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  CGContextRef context = CGBitmapContextCreate(picture.pixels.data(), size_t(width), size_t(height), 8,
                                               size_t(width) * 4, space,
                                               kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
  CGColorSpaceRelease(space);
  if (!context) return std::nullopt;
  CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
  CGImageRef cropped = CGRectEqualToRect(source, CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image)))
                           ? CGImageRetain(image)
                           : CGImageCreateWithImageInRect(image, source);
  if (cropped) {
    CGContextDrawImage(context, CGRectMake(0, 0, width, height), cropped);
    CGImageRelease(cropped);
  }
  CGContextRelease(context);
  if (!cropped) return std::nullopt;
  for (size_t i = 0; i + 3 < picture.pixels.size(); i += 4) {
    uint8_t a = picture.pixels[i + 3];
    if (a != 0 && a != 255) {
      for (int k = 0; k < 3; ++k) {
        picture.pixels[i + k] = uint8_t(std::min(255, (picture.pixels[i + k] * 255 + a / 2) / a));
      }
    }
  }
  return picture;
}

// The source's first image, as it is (at most 1024 x 1024, as picture.cpp's Decode).
std::optional<Rgba> DecodeSource(CGImageSourceRef source) {
  if (!source) return std::nullopt;
  std::optional<Rgba> result;
  if (CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr)) {
    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    if (width > 0 && height > 0 && width <= 1024 && height <= 1024) {
      result = Draw(image, CGRectMake(0, 0, width, height), int(width), int(height));
    }
    CGImageRelease(image);
  }
  CFRelease(source);
  return result;
}

CGImageSourceRef SourceForFile(const std::filesystem::path& path) {
  NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
  return url ? CGImageSourceCreateWithURL((__bridge CFURLRef)url, nullptr) : nullptr;
}

}  // namespace

std::optional<Rgba> DecodePicture(const std::vector<uint8_t>& file) {
  if (file.empty()) return std::nullopt;
  @autoreleasepool {
    NSData* data = [NSData dataWithBytes:file.data() length:file.size()];
    return DecodeSource(CGImageSourceCreateWithData((__bridge CFDataRef)data, nullptr));
  }
}

std::optional<Rgba> LoadPicture(const std::filesystem::path& path) {
  @autoreleasepool {
    return DecodeSource(SourceForFile(path));
  }
}

std::optional<Rgba> LoadSquarePicture(const std::filesystem::path& path, int size) {
  @autoreleasepool {
    CGImageSourceRef source = SourceForFile(path);
    if (!source) return std::nullopt;
    // The photo as it is meant to be seen (a phone's rotation).
    NSDictionary* options = @{
      (__bridge NSString*)kCGImageSourceCreateThumbnailFromImageAlways : @YES,
      (__bridge NSString*)kCGImageSourceCreateThumbnailWithTransform : @YES,
      (__bridge NSString*)kCGImageSourceThumbnailMaxPixelSize : @(std::max(size * 4, 1024)),
    };
    CGImageRef image = CGImageSourceCreateThumbnailAtIndex(source, 0, (__bridge CFDictionaryRef)options);
    CFRelease(source);
    if (!image) return std::nullopt;
    size_t width = CGImageGetWidth(image);
    size_t height = CGImageGetHeight(image);
    size_t side = std::min(width, height);
    std::optional<Rgba> result =
        Draw(image, CGRectMake(CGFloat((width - side) / 2), CGFloat((height - side) / 2), side, side), size, size);
    CGImageRelease(image);
    return result;
  }
}

bool SavePng(const std::filesystem::path& path, const Rgba& picture) {
  if (picture.width <= 0 || picture.height <= 0) return false;
  @autoreleasepool {
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    CGColorSpaceRef space = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CFDataRef data = CFDataCreate(nullptr, picture.pixels.data(), CFIndex(picture.pixels.size()));
    CGDataProviderRef provider = CGDataProviderCreateWithCFData(data);
    CGImageRef image = CGImageCreate(size_t(picture.width), size_t(picture.height), 8, 32, size_t(picture.width) * 4,
                                     space, kCGImageAlphaLast | kCGBitmapByteOrder32Big, provider, nullptr, false,
                                     kCGRenderingIntentDefault);
    bool ok = false;
    NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:temporary.c_str()]];
    if (image && url) {
      CGImageDestinationRef destination = CGImageDestinationCreateWithURL(
          (__bridge CFURLRef)url, (__bridge CFStringRef)UTTypePNG.identifier, 1, nullptr);
      if (destination) {
        CGImageDestinationAddImage(destination, image, nullptr);
        ok = CGImageDestinationFinalize(destination);
        CFRelease(destination);
      }
    }
    if (image) CGImageRelease(image);
    CGDataProviderRelease(provider);
    CFRelease(data);
    CGColorSpaceRelease(space);
    if (!ok) return false;
    std::error_code ec;
    std::filesystem::rename(temporary, path, ec);
    return !ec;
  }
}

}  // namespace launcher
