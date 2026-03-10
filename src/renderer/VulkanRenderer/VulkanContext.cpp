#include "VulkanContext.h"
#include "platform/IPresentSurface.h"

#include <vector>
#include <cstring>
#include <stdexcept>
#include <iostream>

static const char* kValidationLayerName = "VK_LAYER_KHRONOS_validation";

static const char* kDeviceExtensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

static void VK_CHECK(VkResult r, const char* msg) {
    if (r != VK_SUCCESS) throw std::runtime_error(msg);
}

bool VulkanContext::CheckValidationLayerSupport() const {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());

    for (auto& l : layers) {
        if (std::strcmp(l.layerName, kValidationLayerName) == 0)
            return true;
    }
    return false;
}

bool VulkanContext::CheckInstanceExtensionsSupport(const char* const* exts, uint32_t count) const {
    uint32_t availCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &availCount, nullptr);
    std::vector<VkExtensionProperties> avail(availCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &availCount, avail.data());

    for (uint32_t i = 0; i < count; i++) {
        bool found = false;
        for (auto& a : avail) {
            if (std::strcmp(exts[i], a.extensionName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

void VulkanContext::CreateInstance(INativeWindowSurface& surface) {
    if constexpr (kEnableValidation) {
        if (!CheckValidationLayerSupport())
            throw std::runtime_error("Validation layer VK_LAYER_KHRONOS_validation not found");
    }

    uint32_t extensions_count = 0;
    char** extensions = (char**)surface.GetExtensions(&extensions_count, kEnableValidation);

    if (!CheckInstanceExtensionsSupport(extensions, extensions_count))
        throw std::runtime_error("Required instance extensions not supported");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "SoftRender";
    appInfo.applicationVersion = VK_MAKE_VERSION(0,0,1);
    appInfo.pEngineName = "SoftRenderEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(0,0,1);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &appInfo;
    ci.enabledExtensionCount = extensions_count;
    ci.ppEnabledExtensionNames = extensions;

    const char* layers[] = { kValidationLayerName };
    if  constexpr (kEnableValidation) {
        ci.enabledLayerCount = 1;
        ci.ppEnabledLayerNames = layers;
    }

    VK_CHECK(vkCreateInstance(&ci, nullptr, &instance_), "vkCreateInstance failed");

    std::cout << "[vk] Instance created. Validation: " << (kEnableValidation ? "ON" : "OFF") << "\n";

#ifndef NDEBUG
    if constexpr (kEnableValidation) SetupDebugMessenger();
#endif
}

#ifndef NDEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data,
    void*) 
{
    // warning+error enouth
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[vk validation] " << data->pMessage << "\n";
    }
    return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    VkDebugUtilsMessengerEXT* messenger)
{
    auto fn = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT");
    if (!fn) return VK_ERROR_EXTENSION_NOT_PRESENT;
    return fn(instance, createInfo, nullptr, messenger);
}

static void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger) {
    auto fn = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (fn) fn(instance, messenger, nullptr);
}

void VulkanContext::SetupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = DebugCallback;

    VK_CHECK(CreateDebugUtilsMessengerEXT(instance_, &ci, &debugMessenger_),
             "CreateDebugUtilsMessengerEXT failed");
    std::cout << "[vk] Debug messenger created.\n";
}
#endif

void VulkanContext::DestroyInstance() {
#ifndef NDEBUG
    if constexpr (kEnableValidation) {
        if (debugMessenger_) {
            DestroyDebugUtilsMessengerEXT(instance_, debugMessenger_);
            debugMessenger_ = VK_NULL_HANDLE;
        }
    }
#endif
    if (instance_) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
}

void VulkanContext::CreateSurface(INativeWindowSurface& nativeSurface) {
    if (!instance_) throw std::runtime_error("CreateSurface: instance not created");

    auto hwnd  = (HWND)nativeSurface.GetNativeHwnd();
    auto hinst = (HINSTANCE)nativeSurface.GetNativeHinstance();

    if (!hwnd || !hinst) throw std::runtime_error("CreateSurface: HWND/HINSTANCE is null");

    VkWin32SurfaceCreateInfoKHR ci{};
    ci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    ci.hwnd = hwnd;
    ci.hinstance = hinst;

    VK_CHECK(vkCreateWin32SurfaceKHR(instance_, &ci, nullptr, &surface_),
             "vkCreateWin32SurfaceKHR failed");
}

void VulkanContext::DestroySurface() {
    if (surface_) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
}

bool VulkanContext::CheckDeviceExtensionSupport(VkPhysicalDevice dev) const {
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> exts(count);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &count, exts.data());

    for (const char* req : kDeviceExtensions) {
        bool found = false;
        for (const auto& e : exts) {
            if (std::strcmp(req, e.extensionName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice dev) const {
    QueueFamilyIndices out;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, props.data());

    for (uint32_t i = 0; i < count; i++) {
        // graphics?
        if (props[i].queueCount > 0 && (props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            if (out.graphics < 0) out.graphics = (int)i;
        }
        
        // present?
        VkBool32 presentSupport = VK_FALSE;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &presentSupport),
                 "vkGetPhysicalDeviceSurfaceSupportKHR failed");
        if (props[i].queueCount > 0 && presentSupport) {
            if (out.present < 0) out.present = (int)i;
        }

        if (out.IsComplete()) break;
    }

    return out;
}

bool VulkanContext::IsDeviceSuitable(VkPhysicalDevice dev) const {
    const QueueFamilyIndices q = FindQueueFamilies(dev);
    if (!q.IsComplete()) return false;

    if (!CheckDeviceExtensionSupport(dev)) return false;

    return true;
}

void VulkanContext::PickPhysicalDevice() {
    if (!instance_) throw std::runtime_error("PickPhysicalDevice: instance not created");
    if (!surface_)  throw std::runtime_error("PickPhysicalDevice: surface not created");

    uint32_t count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &count, nullptr),
             "vkEnumeratePhysicalDevices failed");
    if (count == 0) throw std::runtime_error("No Vulkan physical devices found");

    std::vector<VkPhysicalDevice> devs(count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &count, devs.data()),
             "vkEnumeratePhysicalDevices (2) failed");

    for (VkPhysicalDevice d : devs) {
        if (!IsDeviceSuitable(d)) continue;

        phys_ = d;
        QueueFamilyIndices q = FindQueueFamilies(d);
        graphicsFamily_ = q.graphics;
        presentFamily_  = q.present;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(phys_, &props);

        std::cout << "[vk] Selected GPU: " << props.deviceName << "\n";
        std::cout << "[vk] Queue families: graphics=" << graphicsFamily_
                  << " present=" << presentFamily_ << "\n";

        return;
    }

    throw std::runtime_error("No suitable GPU found (need graphics+present+swapchain)");
}

void VulkanContext::CreateDevice() {
    if (!phys_) throw std::runtime_error("CreateDevice: physical device not selected");

    const float priority = 1.0f;

    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = (uint32_t)graphicsFamily_; // = presentFamily_
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;

    VkPhysicalDeviceFeatures features{}; 
    // пока ничего не включаем. (анизотропию включим позже)

    const char* exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.pEnabledFeatures = &features;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = exts;

    VK_CHECK(vkCreateDevice(phys_, &dci, nullptr, &device_), "vkCreateDevice failed");

    vkGetDeviceQueue(device_, (uint32_t)graphicsFamily_, 0, &graphicsQueue_);
    vkGetDeviceQueue(device_, (uint32_t)presentFamily_, 0, &presentQueue_);

    std::cout << "[vk] Logical device created\n";
}

void VulkanContext::DestroyDevice() {
    if (device_) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
        graphicsQueue_ = VK_NULL_HANDLE;
        presentQueue_  = VK_NULL_HANDLE;
    }
}