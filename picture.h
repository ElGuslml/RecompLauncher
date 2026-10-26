// Pictures from files for the profiles (Windows Imaging Component): a photo cropped to a square, and PNG files.

#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <vector>

namespace launcher {

// RGBA pixels, straight alpha.
struct Rgba {
  std::vector<uint8_t> pixels;
  int width = 0;
  int height = 0;
};

// Any image Windows reads (JPEG, PNG, BMP, GIF, TIFF, HEIC where installed), cropped to its centered square and
// scaled to `size` x `size`; nullopt if it cannot be read.
std::optional<Rgba> LoadSquarePicture(const std::filesystem::path& path, int size);
// The picture as a PNG file, written whole.
bool SavePng(const std::filesystem::path& path, const Rgba& picture);

}  // namespace launcher
