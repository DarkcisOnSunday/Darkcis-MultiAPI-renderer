#include "FrameManager.h"
#include <stdexcept>

static void VK_CHECK(VkResult r, const char* msg) {
    if (r != VK_SUCCESS) throw std::runtime_error(msg);
}

void FrameManager::Create(VkDevice device, uint32_t graphicsFamily) {
    for (uint32_t i = 0; i < kMaxFrames; i++) {
        // Command pool
        VkCommandPoolCreateInfo pci{};
        pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pci.queueFamilyIndex = graphicsFamily;
        VK_CHECK(vkCreateCommandPool(device, &pci, nullptr, &frames_[i].pool),
                 "vkCreateCommandPool failed");

        // Command buffer
        VkCommandBufferAllocateInfo ai{};
        ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool = frames_[i].pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(device, &ai, &frames_[i].cmd),
                 "vkAllocateCommandBuffers failed");

        // Fence (start signaled so first frame won't stall)
        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(device, &fci, nullptr, &frames_[i].inFlight),
                 "vkCreateFence failed");

        // Semaphores
        VkSemaphoreCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &frames_[i].imageAvailable),
                 "vkCreateSemaphore imageAvailable failed");
        VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &frames_[i].renderFinished),
                 "vkCreateSemaphore renderFinished failed");
    }
}

void FrameManager::Destroy(VkDevice device) {
    for (uint32_t i = 0; i < kMaxFrames; i++) {
        if (frames_[i].renderFinished) vkDestroySemaphore(device, frames_[i].renderFinished, nullptr);
        if (frames_[i].imageAvailable) vkDestroySemaphore(device, frames_[i].imageAvailable, nullptr);
        if (frames_[i].inFlight) vkDestroyFence(device, frames_[i].inFlight, nullptr);
        if (frames_[i].pool) vkDestroyCommandPool(device, frames_[i].pool, nullptr);

        frames_[i] = {};
    }
    frameIndex_ = 0;
}