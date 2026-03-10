#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

class VulkanContext;

class Swapchain {
public:
    void Create(VulkanContext& ctx, uint32_t w, uint32_t h);
    void Destroy(VkDevice device);

    void Recreate(VulkanContext& ctx, uint32_t w, uint32_t h);

    VkSwapchainKHR Get() const { return swapchain_; }
    VkFormat GetFormat() const { return format_; }
    VkExtent2D GetExtent() const { return extent_; }
    VkSemaphore GetRenderFinishedSemaphore(uint32_t imageIndex) const { return renderFinished_[imageIndex]; }

    const std::vector<VkImage>& Images() const { return images_; }
    const std::vector<VkImageView>& ImageViews() const { return views_; }

private:
    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) const;

private:
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};

    std::vector<VkSemaphore> renderFinished_;

    std::vector<VkImage> images_;
    std::vector<VkImageView> views_;
};