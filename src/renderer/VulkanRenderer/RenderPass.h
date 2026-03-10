#pragma once

#include <vulkan/vulkan.h>

class RenderPass {
public:
    void Create(VkDevice device, VkFormat swapchainFormat);
    void Destroy(VkDevice device);

    VkRenderPass Get() const { return pass_; }

private:
    VkRenderPass pass_ = VK_NULL_HANDLE;
};