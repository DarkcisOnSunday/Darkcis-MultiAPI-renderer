#include "Swapchain.h"
#include "VulkanContext.h"

#include <algorithm>
#include <stdexcept>
#include <iostream>

static void VK_CHECK(VkResult r, const char* msg) {
    if (r != VK_SUCCESS) throw std::runtime_error(msg);
}

VkSurfaceFormatKHR Swapchain::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const {
    // Хотим SRGB если доступен (правильная гамма)
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return f;
        }
    }
    // иначе первый допустимый
    return formats[0];
}

VkPresentModeKHR Swapchain::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) const {
    // MAILBOX лучше (если есть) — low latency + no tearing
    for (auto m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    }
    // FIFO гарантированно доступен (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, uint32_t w, uint32_t h) const {
    // На Windows обычно currentExtent фиксирован (не UINT32_MAX)
    if (caps.currentExtent.width != UINT32_MAX) {
        return caps.currentExtent;
    }

    VkExtent2D e{};
    e.width  = std::clamp(w, caps.minImageExtent.width,  caps.maxImageExtent.width);
    e.height = std::clamp(h, caps.minImageExtent.height, caps.maxImageExtent.height);
    return e;
}

void Swapchain::Create(VulkanContext& ctx, uint32_t w, uint32_t h) {
    VkPhysicalDevice phys = ctx.GetPhysicalDevice();
    VkDevice device = ctx.GetDevice();
    VkSurfaceKHR surface = ctx.GetSurface();

    // --- capabilities ---
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &caps),
             "vkGetPhysicalDeviceSurfaceCapabilitiesKHR failed");

    // --- formats ---
    uint32_t formatCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, nullptr),
             "vkGetPhysicalDeviceSurfaceFormatsKHR(count) failed");
    if (formatCount == 0) throw std::runtime_error("No surface formats available");

    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys, surface, &formatCount, formats.data()),
             "vkGetPhysicalDeviceSurfaceFormatsKHR(data) failed");

    // --- present modes ---
    uint32_t modeCount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, nullptr),
             "vkGetPhysicalDeviceSurfacePresentModesKHR(count) failed");
    if (modeCount == 0) throw std::runtime_error("No present modes available");

    std::vector<VkPresentModeKHR> modes(modeCount);
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(phys, surface, &modeCount, modes.data()),
             "vkGetPhysicalDeviceSurfacePresentModesKHR(data) failed");

    // --- choose ---
    VkSurfaceFormatKHR chosenFormat = ChooseSurfaceFormat(formats);
    VkPresentModeKHR presentMode = ChoosePresentMode(modes);
    VkExtent2D extent = ChooseExtent(caps, w, h);

    // image count
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    // sharing mode: у тебя graphics==present, значит EXCLUSIVE
    uint32_t graphicsFamily = ctx.GetGraphicsFamily();
    uint32_t presentFamily  = ctx.GetPresentFamily();

    VkSwapchainCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface = surface;
    ci.minImageCount = imageCount;
    ci.imageFormat = chosenFormat.format;
    ci.imageColorSpace = chosenFormat.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (graphicsFamily == presentFamily) {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    } else {
        uint32_t indices[] = { graphicsFamily, presentFamily };
        ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices = indices;
    }

    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode = presentMode;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(device, &ci, nullptr, &swapchain_),
             "vkCreateSwapchainKHR failed");

    // --- get images ---
    uint32_t scImageCount = 0;
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain_, &scImageCount, nullptr),
             "vkGetSwapchainImagesKHR(count) failed");
    images_.resize(scImageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain_, &scImageCount, images_.data()),
             "vkGetSwapchainImagesKHR(data) failed");

    renderFinished_.resize(images_.size());
    VkSemaphoreCreateInfo sci{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (size_t i=0;i<renderFinished_.size();++i)
        VK_CHECK(vkCreateSemaphore(device, &sci, nullptr, &renderFinished_[i]),
                "vkCreateSemaphore renderFinished failed");

    // --- create image views ---
    views_.resize(images_.size());
    for (size_t i = 0; i < images_.size(); i++) {
        VkImageViewCreateInfo vci{};
        vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image = images_[i];
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = chosenFormat.format;
        vci.components = {
            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY
        };
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.baseMipLevel = 0;
        vci.subresourceRange.levelCount = 1;
        vci.subresourceRange.baseArrayLayer = 0;
        vci.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(device, &vci, nullptr, &views_[i]),
                 "vkCreateImageView failed");
    }

    // store chosen
    format_ = chosenFormat.format;
    extent_ = extent;

    std::cout << "[vk] Swapchain created: images=" << images_.size()
              << " extent=" << extent_.width << "x" << extent_.height << "\n";
}

void Swapchain::Destroy(VkDevice device) {
    for (auto v : views_) {
        if (v) vkDestroyImageView(device, v, nullptr);
    }
    views_.clear();
    images_.clear();

    for (auto s : renderFinished_) vkDestroySemaphore(device, s, nullptr);
    renderFinished_.clear();

    if (swapchain_) {
        vkDestroySwapchainKHR(device, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }

    format_ = VK_FORMAT_UNDEFINED;
    extent_ = {};
}

void Swapchain::Recreate(VulkanContext& ctx, uint32_t w, uint32_t h) {
    // Правило: перед пересозданием swapchain GPU не должен использовать старый.
    // На старте проще всего: vkDeviceWaitIdle.
    vkDeviceWaitIdle(ctx.GetDevice());

    Destroy(ctx.GetDevice());
    Create(ctx, w, h);
}