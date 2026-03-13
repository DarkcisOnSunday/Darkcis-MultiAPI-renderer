#pragma once

#include "VulkanContext.h"
#include "math/mat4.h"
#include "math/vec4.h"

struct GPUFrameUBO {
    Mat4 view = Mat4::Identity();
    Mat4 proj = Mat4::Identity();
    Vec4 lightDirVS_ambient;
};

struct GPUObjectPC {
    Mat4 model;
    uint32_t materialFlags;
    float _pad[3];
};

struct GpuMeshRef {
    VkBuffer vb = VK_NULL_HANDLE;
    VkDeviceMemory vbMem = VK_NULL_HANDLE;
    VkBuffer ib = VK_NULL_HANDLE;
    VkDeviceMemory ibMem = VK_NULL_HANDLE;
    uint32_t indexCount = 0;
    bool valid = false;
};

struct GpuMaterialRef {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory imageMem = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
    uint32_t flags = 0;
    bool valid = false;
};
