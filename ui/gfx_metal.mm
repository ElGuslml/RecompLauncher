// The launcher's Metal device on macOS (gfx.h): a CAMetalLayer on the SDL window, drawn by Dear ImGui's Metal backend.

#include "ui/gfx.h"

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <SDL3/SDL.h>
#include <imgui_impl_metal.h>

namespace ui::gfx {
namespace {

SDL_MetalView g_view = nullptr;
CAMetalLayer* g_layer = nil;
id<MTLDevice> g_device = nil;
id<MTLCommandQueue> g_queue = nil;
MTLRenderPassDescriptor* g_pass = nil;
id<CAMetalDrawable> g_drawable = nil;
id<MTLCommandBuffer> g_commands = nil;
id<MTLRenderCommandEncoder> g_encoder = nil;
SDL_Window* g_window = nullptr;
bool g_imgui = false;  // the backend initialized (on the first frame: ImGui's context comes after Init, main.cpp's order)

}  // namespace

bool Init(void* window) {
  g_window = static_cast<SDL_Window*>(window);
  g_view = SDL_Metal_CreateView(g_window);
  if (!g_view) return false;
  g_layer = (__bridge CAMetalLayer*)SDL_Metal_GetLayer(g_view);
  g_device = MTLCreateSystemDefaultDevice();
  if (!g_layer || !g_device) {
    Shutdown();
    return false;
  }
  g_layer.device = g_device;
  g_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
  g_layer.displaySyncEnabled = YES;
  g_queue = [g_device newCommandQueue];
  g_pass = [MTLRenderPassDescriptor new];
  int width = 0;
  int height = 0;
  SDL_GetWindowSizeInPixels(g_window, &width, &height);
  Resize(width, height);
  return true;
}

void Shutdown() {
  if (g_imgui) ImGui_ImplMetal_Shutdown();
  g_imgui = false;
  g_encoder = nil;
  g_commands = nil;
  g_drawable = nil;
  g_pass = nil;
  g_queue = nil;
  g_device = nil;
  g_layer = nil;
  if (g_view) {
    SDL_Metal_DestroyView(g_view);
    g_view = nullptr;
  }
}

ID3D11Device* Device() { return nullptr; }
ID3D11DeviceContext* Context() { return nullptr; }

void Resize(int width, int height) {
  if (g_layer && width > 0 && height > 0) g_layer.drawableSize = CGSizeMake(width, height);
}

void NewFrame() {
  if (!g_imgui) g_imgui = ImGui_ImplMetal_Init(g_device);
  // The drawable now: the backend takes the formats of its pipeline from the pass's texture.
  int width = 0;
  int height = 0;
  SDL_GetWindowSizeInPixels(g_window, &width, &height);
  Resize(width, height);
  // Each frame's autoreleased objects go with it (the loop is C++, with no pool of its own); the globals keep what
  // the frame still needs.
  @autoreleasepool {
    g_drawable = [g_layer nextDrawable];
    g_pass.colorAttachments[0].texture = g_drawable ? g_drawable.texture : nil;
    if (g_drawable) ImGui_ImplMetal_NewFrame(g_pass);
  }
}

void BeginFrame(ImU32 clear_color) {
  if (!g_drawable) return;
  ImVec4 c = ImGui::ColorConvertU32ToFloat4(clear_color);
  g_pass.colorAttachments[0].loadAction = MTLLoadActionClear;
  g_pass.colorAttachments[0].storeAction = MTLStoreActionStore;
  g_pass.colorAttachments[0].clearColor = MTLClearColorMake(c.x, c.y, c.z, c.w);
  @autoreleasepool {
    g_commands = [g_queue commandBuffer];
    g_encoder = [g_commands renderCommandEncoderWithDescriptor:g_pass];
  }
}

void Render(ImDrawData* draw_data) {
  if (!g_encoder) return;
  @autoreleasepool {
    ImGui_ImplMetal_RenderDrawData(draw_data, g_commands, g_encoder);
  }
}

void Present() {
  if (!g_encoder) return;
  @autoreleasepool {
    [g_encoder endEncoding];
    [g_commands presentDrawable:g_drawable];
    [g_commands commit];
  }
  g_encoder = nil;
  g_commands = nil;
  g_drawable = nil;
}

ImTextureID CreateTexture(const uint8_t* rgba, int width, int height) {
  if (!g_device || width <= 0 || height <= 0) return ImTextureID_Invalid;
  MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                  width:NSUInteger(width)
                                                                                 height:NSUInteger(height)
                                                                              mipmapped:NO];
  desc.usage = MTLTextureUsageShaderRead;
  desc.storageMode = MTLStorageModeShared;
  id<MTLTexture> texture = [g_device newTextureWithDescriptor:desc];
  if (!texture) return ImTextureID_Invalid;
  [texture replaceRegion:MTLRegionMake2D(0, 0, NSUInteger(width), NSUInteger(height))
             mipmapLevel:0
               withBytes:rgba
             bytesPerRow:NSUInteger(width) * 4];
  // The Metal backend takes the texture as the id; the launcher owns it until DestroyTexture.
  return ImTextureID(CFBridgingRetain(texture));
}

void DestroyTexture(ImTextureID texture) {
  if (texture != ImTextureID_Invalid) CFBridgingRelease(reinterpret_cast<void*>(texture));
}

}  // namespace ui::gfx
