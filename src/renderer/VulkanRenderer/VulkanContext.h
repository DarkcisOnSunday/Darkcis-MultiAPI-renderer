#pragma once

#ifndef NDEBUG
constexpr bool kEnableValidation = true;
#else
constexpr bool kEnableValidation = false;
#endif

#include <vulkan/vulkan.h>
#include <vector>

struct INativeWindowSurface;

struct QueueFamilyIndices {
    int graphics = -1;
    int present  = -1;

    bool IsComplete() const { return graphics >= 0 && present >= 0; }
};

class VulkanContext {
public:
    void CreateInstance(INativeWindowSurface& surface);
    void DestroyInstance();

    VkInstance GetInstance() const { return instance_; }

    void CreateSurface(INativeWindowSurface& nativeSurface);
    void DestroySurface();

    VkSurfaceKHR GetSurface() const { return surface_; }

    void PickPhysicalDevice();

    VkPhysicalDevice GetPhysicalDevice() const { return phys_; }
    uint32_t GetGraphicsFamily() const { return (uint32_t)graphicsFamily_; }
    uint32_t GetPresentFamily() const { return (uint32_t)presentFamily_; }

    void CreateDevice();
    void DestroyDevice();

    VkDevice GetDevice() const { return device_; }
    VkQueue GetGraphicsQueue() const { return graphicsQueue_; }
    VkQueue GetPresentQueue() const { return presentQueue_; }

    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& outBuffer, VkDeviceMemory& outMemory);
    void DestroyBuffer(VkBuffer& buffer, VkDeviceMemory& memory);
    VkShaderModule CreateShaderModule(const std::vector<char>& code);

private:
    void SetupDebugMessenger();
    void DestroyDebugMessenger();

    bool CheckValidationLayerSupport() const;
    bool CheckInstanceExtensionsSupport(const char* const* exts, uint32_t count) const;

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice dev) const;
    bool CheckDeviceExtensionSupport(VkPhysicalDevice dev) const;
    bool IsDeviceSuitable(VkPhysicalDevice dev) const;

private:
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;

    VkPhysicalDevice phys_ = VK_NULL_HANDLE;
    int graphicsFamily_ = -1;
    int presentFamily_  = -1;

    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    bool validationEnabled_ = false;

#ifndef NDEBUG
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
#endif
};
