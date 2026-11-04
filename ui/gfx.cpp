// The launcher's Direct3D 11 device (gfx.h).

#include "ui/gfx.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <algorithm>
#include <cstdio>
#include <iterator>
#include <string>

#include "launcher_log.h"

namespace ui::gfx {
namespace {

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain1* g_swap_chain = nullptr;
ID3D11RenderTargetView* g_target = nullptr;
HWND g_hwnd = nullptr;

void CreateTarget() {
  ID3D11Texture2D* back_buffer = nullptr;
  if (SUCCEEDED(g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
    g_device->CreateRenderTargetView(back_buffer, nullptr, &g_target);
    back_buffer->Release();
  }
}

void ReleaseTarget() {
  if (g_target) {
    g_target->Release();
    g_target = nullptr;
  }
}

// The window's swap chain (flip model, two buffers); false when it could not be made.
bool CreateSwapChain() {
  IDXGIDevice* dxgi_device = nullptr;
  IDXGIAdapter* adapter = nullptr;
  IDXGIFactory2* factory = nullptr;
  HRESULT result = E_FAIL;
  if (SUCCEEDED(g_device->QueryInterface(IID_PPV_ARGS(&dxgi_device))) &&
      SUCCEEDED(dxgi_device->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory)))) {
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    result = factory->CreateSwapChainForHwnd(g_device, g_hwnd, &desc, nullptr, nullptr, &g_swap_chain);
    if (FAILED(result)) {
      // Before Windows 10 (no FLIP_DISCARD): the sequential flip model, stretched.
      desc.Scaling = DXGI_SCALING_STRETCH;
      desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
      result = factory->CreateSwapChainForHwnd(g_device, g_hwnd, &desc, nullptr, nullptr, &g_swap_chain);
    }
    // Alt+Enter is the launcher's own (a borderless fullscreen through SDL), not DXGI's.
    factory->MakeWindowAssociation(g_hwnd, DXGI_MWA_NO_ALT_ENTER);
  }
  if (factory) factory->Release();
  if (adapter) adapter->Release();
  if (dxgi_device) dxgi_device->Release();
  if (!g_swap_chain) {
    launcher::Log("Direct3D 11: no swap chain (0x%08lX)", static_cast<unsigned long>(result));
    return false;
  }
  CreateTarget();
  return true;
}

}  // namespace

bool Init(void* hwnd) {
  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
  // The hardware device, or WARP (software) where there is none: the launcher draws little.
  HRESULT result = E_FAIL;
  for (D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
    result = D3D11CreateDevice(nullptr, type, nullptr, 0, levels, UINT(std::size(levels)), D3D11_SDK_VERSION,
                               &g_device, nullptr, &g_context);
    if (SUCCEEDED(result)) {
      break;
    }
  }
  if (FAILED(result)) {
    launcher::Log("Direct3D 11: no device (0x%08lX)", static_cast<unsigned long>(result));
    return false;
  }

  {
    IDXGIDevice* dxgi_device = nullptr;
    IDXGIAdapter* adapter = nullptr;
    if (SUCCEEDED(g_device->QueryInterface(IID_PPV_ARGS(&dxgi_device))) &&
        SUCCEEDED(dxgi_device->GetAdapter(&adapter))) {
      DXGI_ADAPTER_DESC adapter_desc = {};
      adapter->GetDesc(&adapter_desc);
      launcher::Log("Direct3D 11: %s, feature level %X", launcher::LogText(adapter_desc.Description).c_str(),
                    unsigned(g_device->GetFeatureLevel()));
    }
    if (adapter) adapter->Release();
    if (dxgi_device) dxgi_device->Release();
  }
  g_hwnd = HWND(hwnd);
  if (!CreateSwapChain()) {
    Shutdown();
    return false;
  }
  return true;
}

void RecreateSwapChain(const char* why) {
  if (!g_device || !g_hwnd) {
    return;
  }
  ReleaseTarget();
  if (g_swap_chain) {
    g_swap_chain->Release();
    g_swap_chain = nullptr;
  }
  g_context->ClearState();
  g_context->Flush();
  launcher::Log(CreateSwapChain() ? "Direct3D 11: the swap chain made again (%s)"
                                  : "Direct3D 11: the swap chain could not be made again (%s)",
                why);
}

void Shutdown() {
  ReleaseTarget();
  if (g_swap_chain) {
    g_swap_chain->Release();
    g_swap_chain = nullptr;
  }
  if (g_context) {
    g_context->Release();
    g_context = nullptr;
  }
  if (g_device) {
    g_device->Release();
    g_device = nullptr;
  }
}

ID3D11Device* Device() { return g_device; }
ID3D11DeviceContext* Context() { return g_context; }

void Resize(int width, int height) {
  if (!g_swap_chain || width <= 0 || height <= 0) {
    return;
  }
  ReleaseTarget();
  HRESULT result = g_swap_chain->ResizeBuffers(0, UINT(width), UINT(height), DXGI_FORMAT_UNKNOWN, 0);
  if (FAILED(result)) {
    launcher::Log("Direct3D 11: resizing to %dx%d failed (0x%08lX)", width, height, static_cast<unsigned long>(result));
  }
  CreateTarget();
}

void BeginFrame(ImU32 clear_color) {
  ImVec4 c = ImGui::ColorConvertU32ToFloat4(clear_color);
  const float rgba[4] = {c.x, c.y, c.z, c.w};
  g_context->OMSetRenderTargets(1, &g_target, nullptr);
  g_context->ClearRenderTargetView(g_target, rgba);
}

void Present() {
  LARGE_INTEGER before, after, frequency;
  QueryPerformanceCounter(&before);
  HRESULT result = g_swap_chain->Present(1, 0);
  QueryPerformanceCounter(&after);
  QueryPerformanceFrequency(&frequency);
  // A Present that takes more than a tenth of a second (it waits one refresh): logged, at most once a second, with how
  // many since the last line.
  static uint32_t slow = 0;
  static double slowest = 0.0;
  static LONGLONG logged_at = 0;
  double ms = double(after.QuadPart - before.QuadPart) * 1000.0 / double(frequency.QuadPart);
  // A Present that waited half a second or more: the screen's flip never came (after the game left its exclusive
  // fullscreen, every Present waited 3 s until another window came in front: Gustavo, 2026-11-04). The swap chain is
  // made again, at most once every 5 s.
  static LONGLONG recreated_at = 0;
  if (ms >= 500.0 && after.QuadPart - recreated_at > 5 * frequency.QuadPart) {
    recreated_at = after.QuadPart;
    char why[64];
    std::snprintf(why, sizeof(why), "a Present took %.0f ms", ms);
    RecreateSwapChain(why);
  }
  if (ms > 100.0) {
    ++slow;
    slowest = std::max(slowest, ms);
    if (after.QuadPart - logged_at > frequency.QuadPart) {
      launcher::Log("Direct3D 11: Present took %.0f ms (%u over 100 ms since the last line, the longest %.0f ms)", ms,
                    slow, slowest);
      logged_at = after.QuadPart;
      slow = 0;
      slowest = 0.0;
    }
  }
  // Logged when it changes: DXGI_STATUS_OCCLUDED (0x087A0001) while the window is hidden or covered is expected; a
  // lost device would leave the window showing its last picture.
  static HRESULT last = S_OK;
  if (result != last) {
    last = result;
    if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET) {
      launcher::Log("Direct3D 11: Present 0x%08lX, the device is lost (reason 0x%08lX)",
                    static_cast<unsigned long>(result), static_cast<unsigned long>(g_device->GetDeviceRemovedReason()));
    } else {
      launcher::Log("Direct3D 11: Present 0x%08lX", static_cast<unsigned long>(result));
    }
  }
}

ImTextureID CreateTexture(const uint8_t* rgba, int width, int height) {
  if (!g_device || width <= 0 || height <= 0) {
    return ImTextureID_Invalid;
  }
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = UINT(width);
  desc.Height = UINT(height);
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_IMMUTABLE;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA data = {rgba, UINT(width) * 4, 0};
  ID3D11Texture2D* texture = nullptr;
  if (FAILED(g_device->CreateTexture2D(&desc, &data, &texture))) {
    return ImTextureID_Invalid;
  }
  ID3D11ShaderResourceView* view = nullptr;
  g_device->CreateShaderResourceView(texture, nullptr, &view);
  texture->Release();
  // The DX11 backend takes the shader resource view as the texture id.
  return ImTextureID(view);
}

void DestroyTexture(ImTextureID texture) {
  if (texture != ImTextureID_Invalid) {
    reinterpret_cast<ID3D11ShaderResourceView*>(texture)->Release();
  }
}

}  // namespace ui::gfx
