#pragma once

#include <vulkan/vulkan.h>
#include <vector>

class Framebuffers {
public:
    void Create(VkDevice device,
                VkRenderPass renderPass,
                const std::vector<VkImageView>& views,
                VkImageView depthView,
                VkExtent2D extent);

    void Destroy(VkDevice device);

    const std::vector<VkFramebuffer>& GetAll() const { return fbs_; }

private:
    std::vector<VkFramebuffer> fbs_;
};
