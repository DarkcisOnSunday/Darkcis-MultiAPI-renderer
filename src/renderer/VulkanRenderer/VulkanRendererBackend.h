#pragma once

#include "renderer/IRenderer.h"
#include "platform/IPresentSurface.h"

#include "VulkanContext.h"
#include "Swapchain.h"
#include "RenderPass.h"
#include "Framebuffers.h"
#include "FrameManager.h"

#include <cstdint>

class VulkanRendererBackend final : public IRenderer {
public:
    explicit VulkanRendererBackend(IPresentSurface& surface)
        : surface_(dynamic_cast<INativeWindowSurface&>(surface))
    {
        ctx_.CreateInstance(surface_);
        ctx_.CreateSurface(surface_);
        ctx_.PickPhysicalDevice();
        ctx_.CreateDevice();
        uint32_t w, h;
        surface_.GetSize(w, h);
        swap_.Create(ctx_, w, h);
        currentW_ = w;
        currentH_ = h;

        renderPass_.Create(ctx_.GetDevice(), swap_.GetFormat());
        framebuffers_.Create(ctx_.GetDevice(),
                             renderPass_.Get(),
                             swap_.ImageViews(),
                             swap_.GetExtent());
        frames_.Create(ctx_.GetDevice(), ctx_.GetGraphicsFamily());
        swapchainReady_ = true;
    }
    ~VulkanRendererBackend();

    void OnResize(uint32_t w, uint32_t h) override;
    void BeginFrame(const FrameContext& ctx) override;
    void RenderScene(const Scene& scene, const Camera& cam) override;
    void EndFrame() override;

private:
    INativeWindowSurface& surface_;
    VulkanContext ctx_;
    Swapchain swap_;
    RenderPass renderPass_;
    Framebuffers framebuffers_;
    FrameManager frames_;

    uint32_t imageIndex_ = 0;
    bool frameActive_ = false;
    bool swapchainReady_ = false;

    uint32_t currentW_;
    uint32_t currentH_;
};