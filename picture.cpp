// Pictures from files for the profiles (picture.h).

#include "picture.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>

#include <algorithm>

namespace launcher {
namespace {

// COM for the calling thread, released with the scope when this call started it.
struct Com {
  HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  ~Com() {
    if (SUCCEEDED(result)) {
      CoUninitialize();
    }
  }
};

template <typename T>
void Free(T*& object) {
  if (object) {
    object->Release();
    object = nullptr;
  }
}

// The decoder's first frame as RGBA pixels.
std::optional<Rgba> Decode(IWICBitmapDecoder* decoder) {
  IWICBitmapFrameDecode* frame = nullptr;
  IWICBitmapSource* converted = nullptr;
  std::optional<Rgba> result;
  UINT width = 0, height = 0;
  if (SUCCEEDED(decoder->GetFrame(0, &frame)) &&
      SUCCEEDED(WICConvertBitmapSource(GUID_WICPixelFormat32bppRGBA, frame, &converted)) &&
      SUCCEEDED(converted->GetSize(&width, &height)) && width > 0 && height > 0 && width <= 1024 && height <= 1024) {
    Rgba picture;
    picture.width = int(width);
    picture.height = int(height);
    picture.pixels.resize(size_t(width) * height * 4);
    if (SUCCEEDED(converted->CopyPixels(nullptr, width * 4, UINT(picture.pixels.size()), picture.pixels.data()))) {
      result = std::move(picture);
    }
  }
  Free(converted);
  Free(frame);
  return result;
}

}  // namespace

std::optional<Rgba> DecodePicture(const std::vector<uint8_t>& file) {
  if (file.empty()) {
    return std::nullopt;
  }
  Com com;
  IWICImagingFactory* factory = nullptr;
  IWICStream* stream = nullptr;
  IWICBitmapDecoder* decoder = nullptr;
  std::optional<Rgba> result;
  if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
      SUCCEEDED(factory->CreateStream(&stream)) &&
      SUCCEEDED(stream->InitializeFromMemory(const_cast<BYTE*>(file.data()), DWORD(file.size()))) &&
      SUCCEEDED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder))) {
    result = Decode(decoder);
  }
  Free(decoder);
  Free(stream);
  Free(factory);
  return result;
}

std::optional<Rgba> LoadPicture(const std::filesystem::path& path) {
  Com com;
  IWICImagingFactory* factory = nullptr;
  IWICBitmapDecoder* decoder = nullptr;
  std::optional<Rgba> result;
  if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
      SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                                   &decoder))) {
    result = Decode(decoder);
  }
  Free(decoder);
  Free(factory);
  return result;
}

std::optional<Rgba> LoadSquarePicture(const std::filesystem::path& path, int size) {
  Com com;
  IWICImagingFactory* factory = nullptr;
  IWICBitmapDecoder* decoder = nullptr;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICBitmapClipper* clipper = nullptr;
  IWICBitmapScaler* scaler = nullptr;
  IWICFormatConverter* converter = nullptr;
  std::optional<Rgba> result;
  UINT width = 0, height = 0;
  if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
      SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                   WICDecodeMetadataCacheOnDemand, &decoder)) &&
      SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(frame->GetSize(&width, &height)) && width && height) {
    UINT side = std::min(width, height);
    WICRect square{INT((width - side) / 2), INT((height - side) / 2), INT(side), INT(side)};
    if (SUCCEEDED(factory->CreateBitmapClipper(&clipper)) && SUCCEEDED(clipper->Initialize(frame, &square)) &&
        SUCCEEDED(factory->CreateBitmapScaler(&scaler)) &&
        SUCCEEDED(scaler->Initialize(clipper, UINT(size), UINT(size), WICBitmapInterpolationModeHighQualityCubic)) &&
        SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
        SUCCEEDED(converter->Initialize(scaler, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                        WICBitmapPaletteTypeCustom))) {
      Rgba picture;
      picture.width = size;
      picture.height = size;
      picture.pixels.resize(size_t(size) * size_t(size) * 4);
      if (SUCCEEDED(converter->CopyPixels(nullptr, UINT(size) * 4, UINT(picture.pixels.size()), picture.pixels.data()))) {
        result = std::move(picture);
      }
    }
  }
  Free(converter);
  Free(scaler);
  Free(clipper);
  Free(frame);
  Free(decoder);
  Free(factory);
  return result;
}

bool SavePng(const std::filesystem::path& path, const Rgba& picture) {
  Com com;
  // PNG's encoder takes BGRA.
  std::vector<uint8_t> bgra = picture.pixels;
  for (size_t i = 0; i + 3 < bgra.size(); i += 4) {
    std::swap(bgra[i], bgra[i + 2]);
  }
  std::filesystem::path temporary = path;
  temporary += L".tmp";
  IWICImagingFactory* factory = nullptr;
  IWICStream* stream = nullptr;
  IWICBitmapEncoder* encoder = nullptr;
  IWICBitmapFrameEncode* frame = nullptr;
  bool ok = false;
  WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
  if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
      SUCCEEDED(factory->CreateStream(&stream)) &&
      SUCCEEDED(stream->InitializeFromFilename(temporary.c_str(), GENERIC_WRITE)) &&
      SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
      SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
      SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) &&
      SUCCEEDED(frame->SetSize(UINT(picture.width), UINT(picture.height))) &&
      SUCCEEDED(frame->SetPixelFormat(&format)) && format == GUID_WICPixelFormat32bppBGRA &&
      SUCCEEDED(frame->WritePixels(UINT(picture.height), UINT(picture.width) * 4, UINT(bgra.size()), bgra.data())) &&
      SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit())) {
    ok = true;
  }
  Free(frame);
  Free(encoder);
  Free(stream);
  Free(factory);
  return ok && MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

}  // namespace launcher
