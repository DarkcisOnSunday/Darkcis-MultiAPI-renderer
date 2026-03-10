#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

struct FrameData {
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence inFlight = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
};

class FrameManager {
public:
    static constexpr uint32_t kMaxFrames = 2;

    void Create(VkDevice device, uint32_t graphicsFamily);
    void Destroy(VkDevice device);

    FrameData& Cur() { return frames_[frameIndex_]; }
    uint32_t Index() const { return frameIndex_; }
    void Next() { frameIndex_ = (frameIndex_ + 1) % kMaxFrames; }

private:
    FrameData frames_[kMaxFrames]{};
    uint32_t frameIndex_ = 0;
};