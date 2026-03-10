#pragma once
#include <cstdint>

#include "platform/IPresentSurface.h"
#include "DcisML/include/window.h"

class WindowSurfaceAdapter final : public INativeWindowSurface {
public:
    explicit WindowSurfaceAdapter(dcis::AppWindow& window)
        : window_(window) {}

    void Present(const void* pixels, uint32_t w, uint32_t h) override {
        window_.PresentAppWindow(
            static_cast<const uint32_t*>(pixels),
            static_cast<int>(w),
            static_cast<int>(h)
        );
    }

    void GetSize(uint32_t& outW, uint32_t& outH) const override {
        int w = 0, h = 0;
        window_.GetAppWindowSize(w, h);
        outW = (w > 0) ? static_cast<uint32_t>(w) : 0u;
        outH = (h > 0) ? static_cast<uint32_t>(h) : 0u;
    }

    void* GetNativeHwnd() const { return window_.GetNativeHwnd(); }
    void* GetNativeHinstance() const { return window_.GetNativeHinstance(); }
    void* GetExtensions(uint32_t* extensions_count, bool debug) const {return window_.GetRequiredVulkanInstanceExtensions(extensions_count, debug); }

private:
    dcis::AppWindow& window_;
};