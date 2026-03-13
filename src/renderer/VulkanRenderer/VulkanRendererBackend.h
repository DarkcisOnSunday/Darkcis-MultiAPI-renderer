#pragma once

#include "renderer/IRenderer.h"
#include "platform/IPresentSurface.h"

#include "VulkanContext.h"
#include "VulkanObjects.h"
#include "Swapchain.h"
#include "RenderPass.h"
#include "Framebuffers.h"
#include "FrameManager.h"
#include "core/mesh.h"
#include "core/material.h"

#include "math/mat4.h"
#include "math/vec4.h"

#include <array>
#include <cstdint>
#include <unordered_map>

struct Mesh;
struct Material;

class VulkanRendererBackend final : public IRenderer {
public:
    VulkanRendererBackend(IPresentSurface& surface);
    ~VulkanRendererBackend();

    void OnResize(uint32_t w, uint32_t h) override;
    void BeginFrame(const FrameContext& ctx) override;
    void RenderScene(const Scene& scene, const Camera& cam) override;
    void EndFrame() override;

private:
    GpuMeshRef EnsureGpuMesh(const Mesh& mesh);
    GpuMaterialRef EnsureGpuMaterial(const Material& mat);
    void UpdateFrameUBO(uint32_t frameIndex, const GPUFrameUBO& d);
    VkDescriptorSet GetFrameSet(uint32_t frameIndex);

private:
    INativeWindowSurface& surface_;
    VulkanContext vulkanCtx_;
    Swapchain swap_;
    RenderPass renderPass_;
    Framebuffers framebuffers_;
    FrameManager frames_;

    uint32_t imageIndex_ = 0;
    bool frameActive_ = false;
    bool swapchainReady_ = false;

    uint32_t currentW_ = 0;
    uint32_t currentH_ = 0;

    std::unordered_map<const Mesh*, GpuMeshRef> meshCache_;
    std::unordered_map<const Material*, GpuMaterialRef> materialCache_;
    GPUFrameUBO frameUbo_{};

    VkPipeline scenePipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout scenePipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout frameSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout materialSetLayout_ = VK_NULL_HANDLE;
    std::array<VkBuffer, FrameManager::kMaxFrames> frameUboBuffers_{};
    std::array<VkDeviceMemory, FrameManager::kMaxFrames> frameUboMem_{};
    std::array<VkDescriptorSet, FrameManager::kMaxFrames> frameSets_{};

    VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
    VkImage depthImage_ = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory_ = VK_NULL_HANDLE;
    VkImageView depthView_ = VK_NULL_HANDLE;
};



