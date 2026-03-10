#include "VulkanRendererBackend.h"

#include <stdexcept>
#include <fstream>

static void VK_CHECK(VkResult r, const char* msg) {
    if (r != VK_SUCCESS) throw std::runtime_error(msg);
}

VulkanRendererBackend::~VulkanRendererBackend() = default;

void VulkanRendererBackend::OnResize(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) return;
    if (w == currentW_ && h == currentH_) return;
    currentW_ = w;
    currentH_ = h;

    auto device = ctx_.GetDevice();
    vkDeviceWaitIdle(device);
    framebuffers_.Destroy(device);
    renderPass_.Destroy(device);
    swap_.Recreate(ctx_, w, h);
    renderPass_.Create(device, swap_.GetFormat());
    framebuffers_.Create(device, renderPass_.Get(), swap_.ImageViews(), swap_.GetExtent());
}

void VulkanRendererBackend::BeginFrame(const FrameContext& ctx) {
    (void)ctx;

    frameActive_ = false;

    VkDevice device = ctx_.GetDevice();
    FrameData& f = frames_.Cur();

    VK_CHECK(vkWaitForFences(device, 1, &f.inFlight, VK_TRUE, UINT64_MAX),
             "vkWaitForFences failed");
    VK_CHECK(vkResetFences(device, 1, &f.inFlight),
             "vkResetFences failed");

    VkResult acq = vkAcquireNextImageKHR(
        device,
        swap_.Get(),
        UINT64_MAX,
        f.imageAvailable,
        VK_NULL_HANDLE,
        &imageIndex_
    );

    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        // окно/свопчейн изменился — пересоздаём и пропускаем кадр
        uint32_t w,h; surface_.GetSize(w,h);
        OnResize(w,h);
        return;
    }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("vkAcquireNextImageKHR failed");
    }

    // 3) подготовка cmd
    VK_CHECK(vkResetCommandPool(device, f.pool, 0), "vkResetCommandPool failed");

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(f.cmd, &bi), "vkBeginCommandBuffer failed");

    // 4) begin render pass с CLEAR
    VkClearValue clear{};
    clear.color = { { 0.05f, 0.08f, 0.12f, 1.0f } }; // можешь менять

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = renderPass_.Get();
    rp.framebuffer = framebuffers_.GetAll()[imageIndex_];
    rp.renderArea.offset = { 0, 0 };
    rp.renderArea.extent = swap_.GetExtent();
    rp.clearValueCount = 1;
    rp.pClearValues = &clear;

    vkCmdBeginRenderPass(f.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    // пока ничего не рисуем
    vkCmdEndRenderPass(f.cmd);

    VK_CHECK(vkEndCommandBuffer(f.cmd), "vkEndCommandBuffer failed");
    frameActive_ = true;
}

void VulkanRendererBackend::RenderScene(const Scene&, const Camera&) {}

void VulkanRendererBackend::EndFrame() {
    if (!frameActive_) return;

    FrameData& f = frames_.Cur();

    VkSemaphore renderFinished = swap_.GetRenderFinishedSemaphore(imageIndex_);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &f.imageAvailable;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &f.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &renderFinished;

    VK_CHECK(vkQueueSubmit(ctx_.GetGraphicsQueue(), 1, &si, f.inFlight),
             "vkQueueSubmit failed");

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &renderFinished;

    VkSwapchainKHR sc = swap_.Get();
    pi.swapchainCount = 1;
    pi.pSwapchains = &sc;
    pi.pImageIndices = &imageIndex_;

    VkResult pres = vkQueuePresentKHR(ctx_.GetPresentQueue(), &pi);

    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
        uint32_t w, h;
        surface_.GetSize(w, h);
        OnResize(w, h);
    } else if (pres != VK_SUCCESS) {
        throw std::runtime_error("vkQueuePresentKHR failed");
    }

    frames_.Next();
}