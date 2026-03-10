#pragma once

#include "renderer/CPURenderer/renderer.h"
#include "renderer/CPURenderer/rasterizer.h"
#include "renderer/CPURenderer/frame_buffer.h"
#include "renderer/IRenderer.h"
#include "platform/IPresentSurface.h"
#include "platform/WindowSurfaceAdapter.h"

#include <cstdint>

class CpuRendererBackend final : public IRenderer {
public:
    CpuRendererBackend(IPresentSurface& surface)
        : surface(surface) {}

    void OnResize(uint32_t w, uint32_t h) override {
        fb = FrameBuffer((int)w, (int)h);
        renderer.Resize((int)w, (int)h);
    }   

    void BeginFrame(const FrameContext& ctx) override {
        (void)ctx;  
        fb.Clear();
        // если нужно: renderer.SetViewport(...)
    }

    void RenderScene(const Scene& scene, const Camera& cam) override {
        renderer.SetCamera(&cam);
        renderer.Render(fb, rast, scene);
    }

    void EndFrame() override {
        surface.Present(fb.GetBuffer(), fb.GetWidth(), fb.GetHeight());
    }

private:
    IPresentSurface& surface;
    FrameBuffer fb{1,1};
    Rasterizer rast;
    Renderer renderer;
};