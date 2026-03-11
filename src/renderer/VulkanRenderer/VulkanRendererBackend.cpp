#include "VulkanRendererBackend.h"
#include "render/camera.h"
#include "core/scene.h"
#include "gfx/vertex.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

static void VK_CHECK(VkResult r, const char* msg) {
    if (r != VK_SUCCESS) throw std::runtime_error(msg);
}

namespace {

std::vector<char> ReadBinaryFile(const char* path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error(path);

    const std::streamsize size = file.tellg();
    if (size <= 0) throw std::runtime_error(path);

    std::vector<char> data(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(data.data(), size);
    return data;
}

uint32_t FindMemoryType(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }

    throw std::runtime_error("No suitable memory type");
}

VkFormat FindSupportedFormat(VkPhysicalDevice phys,
                             const std::vector<VkFormat>& candidates,
                             VkImageTiling tiling,
                             VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(phys, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        }
        if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("No supported format found");
}

bool HasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

void CreateBuffer(VkDevice device,
                  VkPhysicalDevice phys,
                  VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags props,
                  VkBuffer& outBuffer,
                  VkDeviceMemory& outMemory) {
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size;
    bci.usage = usage;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &outBuffer), "vkCreateBuffer failed");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(device, outBuffer, &req);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(phys, req.memoryTypeBits, props);
    VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &outMemory), "vkAllocateMemory failed");
    VK_CHECK(vkBindBufferMemory(device, outBuffer, outMemory, 0), "vkBindBufferMemory failed");
}

void DestroyBuffer(VkDevice device, VkBuffer& buffer, VkDeviceMemory& memory) {
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

void CreateImage(VkDevice device,
                 VkPhysicalDevice phys,
                 uint32_t width,
                 uint32_t height,
                 VkFormat format,
                 VkImageTiling tiling,
                 VkImageUsageFlags usage,
                 VkMemoryPropertyFlags props,
                 VkImage& outImage,
                 VkDeviceMemory& outMemory) {
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.extent = { width, height, 1 };
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.format = format;
    ici.tiling = tiling;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.usage = usage;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateImage(device, &ici, nullptr, &outImage), "vkCreateImage failed");

    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, outImage, &req);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = FindMemoryType(phys, req.memoryTypeBits, props);
    VK_CHECK(vkAllocateMemory(device, &mai, nullptr, &outMemory), "vkAllocateMemory image failed");
    VK_CHECK(vkBindImageMemory(device, outImage, outMemory, 0), "vkBindImageMemory failed");
}

void DestroyImage(VkDevice device, VkImage& image, VkDeviceMemory& memory, VkImageView& view) {
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, view, nullptr);
        view = VK_NULL_HANDLE;
    }
    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(device, image, nullptr);
        image = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

VkShaderModule CreateShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device, &ci, nullptr, &module), "vkCreateShaderModule failed");
    return module;
}

VkCommandBuffer BeginImmediateCommands(VkDevice device, uint32_t graphicsFamily, VkCommandPool& pool) {
    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.queueFamilyIndex = graphicsFamily;
    pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    VK_CHECK(vkCreateCommandPool(device, &pci, nullptr, &pool), "vkCreateCommandPool upload failed");

    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &ai, &cmd), "vkAllocateCommandBuffers upload failed");

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &bi), "vkBeginCommandBuffer upload failed");
    return cmd;
}

void EndImmediateCommands(VkDevice device,
                          VkQueue queue,
                          VkCommandPool pool,
                          VkCommandBuffer cmd) {
    VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer upload failed");

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    VK_CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE), "vkQueueSubmit upload failed");
    VK_CHECK(vkQueueWaitIdle(queue), "vkQueueWaitIdle upload failed");

    vkDestroyCommandPool(device, pool, nullptr);
}

void TransitionImageLayout(VkCommandBuffer cmd,
                           VkImage image,
                           VkFormat format,
                           VkImageLayout oldLayout,
                           VkImageLayout newLayout,
                           VkPipelineStageFlags srcStage,
                           VkPipelineStageFlags dstStage,
                           VkAccessFlags srcAccess,
                           VkAccessFlags dstAccess) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.aspectMask =
        (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            ? VK_IMAGE_ASPECT_DEPTH_BIT
            : VK_IMAGE_ASPECT_COLOR_BIT;
    if ((barrier.subresourceRange.aspectMask & VK_IMAGE_ASPECT_DEPTH_BIT) && HasStencilComponent(format)) {
        barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd,
                         srcStage,
                         dstStage,
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);
}

void CreateScenePipeline(VulkanContext& ctx,
                         RenderPass& renderPass,
                         VkPipelineLayout layout,
                         VkPipeline& outPipeline) {
    VkDevice device = ctx.GetDevice();

    const std::vector<char> vertCode = ReadBinaryFile("shaders/vert.spv");
    const std::vector<char> fragCode = ReadBinaryFile("shaders/frag.spv");
    VkShaderModule vert = CreateShaderModule(device, vertCode);
    VkShaderModule frag = CreateShaderModule(device, fragCode);

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vert;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = frag;
    shaderStages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(Vertex3D);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 4> attrs{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex3D, pos))};
    attrs[1] = {1, 0, VK_FORMAT_R32_UINT, static_cast<uint32_t>(offsetof(Vertex3D, color))};
    attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex3D, uv))};
    attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(Vertex3D, normal))};

    VkPipelineVertexInputStateCreateInfo vi{};
    vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &binding;
    vi.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrs.size());
    vi.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.lineWidth = 1.0f;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState cbAttach{};
    cbAttach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                              VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1;
    cb.pAttachments = &cbAttach;

    const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynamicStates;

    VkPipelineDepthStencilStateCreateInfo ds{};
    ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_LESS;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.stageCount = 2;
    pci.pStages = shaderStages;
    pci.pVertexInputState = &vi;
    pci.pInputAssemblyState = &ia;
    pci.pViewportState = &vp;
    pci.pRasterizationState = &rs;
    pci.pMultisampleState = &ms;
    pci.pDepthStencilState = &ds;
    pci.pColorBlendState = &cb;
    pci.pDynamicState = &dyn;
    pci.layout = layout;
    pci.renderPass = renderPass.Get();
    pci.subpass = 0;

    VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pci, nullptr, &outPipeline),
             "vkCreateGraphicsPipelines failed");

    vkDestroyShaderModule(device, frag, nullptr);
    vkDestroyShaderModule(device, vert, nullptr);
}

void CreateSceneResources(VulkanContext& ctx,
                          VkDescriptorSetLayout& frameSetLayout,
                          VkDescriptorSetLayout& materialSetLayout,
                          VkDescriptorPool& descriptorPool,
                          std::array<VkBuffer, FrameManager::kMaxFrames>& frameUboBuffers,
                          std::array<VkDeviceMemory, FrameManager::kMaxFrames>& frameUboMem,
                          std::array<VkDescriptorSet, FrameManager::kMaxFrames>& frameSets,
                          VkPipelineLayout& pipelineLayout) {
    VkDevice device = ctx.GetDevice();

    VkDescriptorSetLayoutBinding frameBinding{};
    frameBinding.binding = 0;
    frameBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    frameBinding.descriptorCount = 1;
    frameBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo frameLayoutCI{};
    frameLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    frameLayoutCI.bindingCount = 1;
    frameLayoutCI.pBindings = &frameBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &frameLayoutCI, nullptr, &frameSetLayout),
             "vkCreateDescriptorSetLayout frame failed");

    VkDescriptorSetLayoutBinding materialBinding{};
    materialBinding.binding = 0;
    materialBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialBinding.descriptorCount = 1;
    materialBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo materialLayoutCI{};
    materialLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    materialLayoutCI.bindingCount = 1;
    materialLayoutCI.pBindings = &materialBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &materialLayoutCI, nullptr, &materialSetLayout),
             "vkCreateDescriptorSetLayout material failed");

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, FrameManager::kMaxFrames};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64};

    VkDescriptorPoolCreateInfo poolCI{};
    poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCI.maxSets = FrameManager::kMaxFrames + 64;
    poolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolCI.pPoolSizes = poolSizes.data();
    VK_CHECK(vkCreateDescriptorPool(device, &poolCI, nullptr, &descriptorPool),
             "vkCreateDescriptorPool failed");

    std::array<VkDescriptorSetLayout, FrameManager::kMaxFrames> frameLayouts{};
    frameLayouts.fill(frameSetLayout);
    VkDescriptorSetAllocateInfo frameSetAI{};
    frameSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    frameSetAI.descriptorPool = descriptorPool;
    frameSetAI.descriptorSetCount = FrameManager::kMaxFrames;
    frameSetAI.pSetLayouts = frameLayouts.data();
    VK_CHECK(vkAllocateDescriptorSets(device, &frameSetAI, frameSets.data()),
             "vkAllocateDescriptorSets frame failed");

    for (uint32_t i = 0; i < FrameManager::kMaxFrames; ++i) {
        CreateBuffer(device,
                     ctx.GetPhysicalDevice(),
                     sizeof(GPUFrameUBO),
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     frameUboBuffers[i],
                     frameUboMem[i]);

        VkDescriptorBufferInfo dbi{};
        dbi.buffer = frameUboBuffers[i];
        dbi.offset = 0;
        dbi.range = sizeof(GPUFrameUBO);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = frameSets[i];
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.pBufferInfo = &dbi;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(GPUObjectPC);

    std::array<VkDescriptorSetLayout, 2> setLayouts = {frameSetLayout, materialSetLayout};
    VkPipelineLayoutCreateInfo plCI{};
    plCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCI.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    plCI.pSetLayouts = setLayouts.data();
    plCI.pushConstantRangeCount = 1;
    plCI.pPushConstantRanges = &pushRange;
    VK_CHECK(vkCreatePipelineLayout(device, &plCI, nullptr, &pipelineLayout),
             "vkCreatePipelineLayout failed");
}

} // namespace

VulkanRendererBackend::~VulkanRendererBackend() {
    VkDevice device = ctx_.GetDevice();
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);

        for (auto& [_, mat] : materialCache_) {
            if (mat.sampler != VK_NULL_HANDLE) vkDestroySampler(device, mat.sampler, nullptr);
            if (mat.view != VK_NULL_HANDLE) vkDestroyImageView(device, mat.view, nullptr);
            if (mat.image != VK_NULL_HANDLE) vkDestroyImage(device, mat.image, nullptr);
            if (mat.imageMem != VK_NULL_HANDLE) vkFreeMemory(device, mat.imageMem, nullptr);
        }
        materialCache_.clear();
        for (auto& [_, mesh] : meshCache_) {
            DestroyBuffer(device, mesh.vb, mesh.vbMem);
            DestroyBuffer(device, mesh.ib, mesh.ibMem);
        }
        meshCache_.clear();

        for (uint32_t i = 0; i < FrameManager::kMaxFrames; ++i) {
            DestroyBuffer(device, frameUboBuffers_[i], frameUboMem_[i]);
            frameSets_[i] = VK_NULL_HANDLE;
        }

        DestroyImage(device, depthImage_, depthMemory_, depthView_);

        if (scenePipeline_ != VK_NULL_HANDLE) {
            vkDestroyPipeline(device, scenePipeline_, nullptr);
            scenePipeline_ = VK_NULL_HANDLE;
        }
        if (scenePipelineLayout_ != VK_NULL_HANDLE) {
            vkDestroyPipelineLayout(device, scenePipelineLayout_, nullptr);
            scenePipelineLayout_ = VK_NULL_HANDLE;
        }
        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
        }
        if (materialSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, materialSetLayout_, nullptr);
            materialSetLayout_ = VK_NULL_HANDLE;
        }
        if (frameSetLayout_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(device, frameSetLayout_, nullptr);
            frameSetLayout_ = VK_NULL_HANDLE;
        }

        framebuffers_.Destroy(device);
        renderPass_.Destroy(device);
        swap_.Destroy(device);
        frames_.Destroy(device);
        ctx_.DestroyDevice();
    }

    ctx_.DestroySurface();
    ctx_.DestroyInstance();
}

void VulkanRendererBackend::OnResize(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) return;

    const bool needSwapchainRecreate = (w != currentW_) || (h != currentH_) || !swapchainReady_;
    currentW_ = w;
    currentH_ = h;

    VkDevice device = ctx_.GetDevice();
    vkDeviceWaitIdle(device);

    if (scenePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, scenePipeline_, nullptr);
        scenePipeline_ = VK_NULL_HANDLE;
    }

    framebuffers_.Destroy(device);
    DestroyImage(device, depthImage_, depthMemory_, depthView_);
    renderPass_.Destroy(device);

    if (needSwapchainRecreate) {
        swap_.Recreate(ctx_, w, h);
    }

    if (depthFormat_ == VK_FORMAT_UNDEFINED) {
        depthFormat_ = FindSupportedFormat(ctx_.GetPhysicalDevice(),
                                           {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                                           VK_IMAGE_TILING_OPTIMAL,
                                           VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    CreateImage(device,
                ctx_.GetPhysicalDevice(),
                swap_.GetExtent().width,
                swap_.GetExtent().height,
                depthFormat_,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                depthImage_,
                depthMemory_);

    VkImageViewCreateInfo depthViewCI{};
    depthViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewCI.image = depthImage_;
    depthViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewCI.format = depthFormat_;
    depthViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (HasStencilComponent(depthFormat_)) {
        depthViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    depthViewCI.subresourceRange.baseMipLevel = 0;
    depthViewCI.subresourceRange.levelCount = 1;
    depthViewCI.subresourceRange.baseArrayLayer = 0;
    depthViewCI.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(device, &depthViewCI, nullptr, &depthView_), "vkCreateImageView depth failed");

    VkCommandPool uploadPool = VK_NULL_HANDLE;
    VkCommandBuffer uploadCmd = BeginImmediateCommands(device, ctx_.GetGraphicsFamily(), uploadPool);
    TransitionImageLayout(uploadCmd,
                          depthImage_,
                          depthFormat_,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                          0,
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    EndImmediateCommands(device, ctx_.GetGraphicsQueue(), uploadPool, uploadCmd);

    renderPass_.Create(device, swap_.GetFormat(), depthFormat_);
    framebuffers_.Create(device, renderPass_.Get(), swap_.ImageViews(), depthView_, swap_.GetExtent());

    if (scenePipelineLayout_ != VK_NULL_HANDLE) {
        CreateScenePipeline(ctx_, renderPass_, scenePipelineLayout_, scenePipeline_);
    }
}

void VulkanRendererBackend::BeginFrame(const FrameContext& ctx) {
    (void)ctx;

    frameActive_ = false;

    VkDevice device = ctx_.GetDevice();

    if (descriptorPool_ == VK_NULL_HANDLE) {
        CreateSceneResources(ctx_,
                             frameSetLayout_,
                             materialSetLayout_,
                             descriptorPool_,
                             frameUboBuffers_,
                             frameUboMem_,
                             frameSets_,
                             scenePipelineLayout_);
    }

    if (!swapchainReady_ || depthView_ == VK_NULL_HANDLE || scenePipeline_ == VK_NULL_HANDLE) {
        uint32_t w = currentW_;
        uint32_t h = currentH_;
        if (ctx.width > 0 && ctx.height > 0) {
            w = ctx.width;
            h = ctx.height;
        }
        OnResize(w, h);
    }

    FrameData& f = frames_.Cur();

    VK_CHECK(vkWaitForFences(device, 1, &f.inFlight, VK_TRUE, UINT64_MAX),
             "vkWaitForFences failed");
    VK_CHECK(vkResetFences(device, 1, &f.inFlight),
             "vkResetFences failed");

    VkResult acq = vkAcquireNextImageKHR(
        device,
        swap_.Get(),
        UINT64_MAX,
        f.imageAvailable,
        VK_NULL_HANDLE,
        &imageIndex_);

    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        uint32_t w = currentW_;
        uint32_t h = currentH_;
        surface_.GetSize(w, h);
        OnResize(w, h);
        return;
    }

    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("vkAcquireNextImageKHR failed");
    }

    VK_CHECK(vkResetCommandPool(device, f.pool, 0), "vkResetCommandPool failed");

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(f.cmd, &bi), "vkBeginCommandBuffer failed");

    std::array<VkClearValue, 2> clears{};
    clears[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
    clears[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = renderPass_.Get();
    rp.framebuffer = framebuffers_.GetAll()[imageIndex_];
    rp.renderArea.offset = {0, 0};
    rp.renderArea.extent = swap_.GetExtent();
    rp.clearValueCount = static_cast<uint32_t>(clears.size());
    rp.pClearValues = clears.data();

    vkCmdBeginRenderPass(f.cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);
    frameActive_ = true;
}
void VulkanRendererBackend::RenderScene(const Scene& scene, const Camera& cam) {
    if (!frameActive_) return;

    FrameData& f = frames_.Cur();
    VkCommandBuffer cmd = f.cmd;

    GPUFrameUBO frame{};
    frame.view = cam.view;
    frame.proj = cam.proj;

    const Vec3 L = Vec3(0.3f, 0.7f, 1.0f).Normalized();
    const Vec4 L4 = cam.view * Vec4(L.x, L.y, L.z, 0.0f);
    frame.lightDirVS_ambient = Vec4(L4.x, L4.y, L4.z, 0.15f);
    UpdateFrameUBO(frames_.Index(), frame);

    VkPipeline pipeline = GetScenePipeline();
    VkPipelineLayout layout = GetScenePipelineLayout();
    if (pipeline == VK_NULL_HANDLE || layout == VK_NULL_HANDLE) {
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkExtent2D ext = GetCurrentExtent();

    VkViewport vp{};
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = static_cast<float>(ext.width);
    vp.height = static_cast<float>(ext.height);
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);

    VkRect2D sc{};
    sc.offset = {0, 0};
    sc.extent = ext;
    vkCmdSetScissor(cmd, 0, 1, &sc);

    VkDescriptorSet frameSet = GetFrameSet(frames_.Index());
    if (frameSet != VK_NULL_HANDLE) {
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                layout,
                                0,
                                1,
                                &frameSet,
                                0,
                                nullptr);
    }

    for (const RenderObject& obj : scene.objects) {
        if (!obj.mesh || !obj.material) continue;

        GpuMeshRef gmesh = EnsureGpuMesh(*obj.mesh);
        if (!gmesh.valid || gmesh.indexCount == 0) continue;

        GpuMaterialRef gmat = EnsureGpuMaterial(*obj.material);
        if (!gmat.valid || gmat.set == VK_NULL_HANDLE) continue;

        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                layout,
                                1,
                                1,
                                &gmat.set,
                                0,
                                nullptr);

        GPUObjectPC pc{};
        pc.model = obj.model;
        pc.materialFlags = gmat.flags;
        vkCmdPushConstants(cmd,
                           layout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(GPUObjectPC),
                           &pc);

        VkBuffer vb = gmesh.vb;
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &offset);
        vkCmdBindIndexBuffer(cmd, gmesh.ib, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, gmesh.indexCount, 1, 0, 0, 0);
    }
}

void VulkanRendererBackend::EndFrame() {
    if (!frameActive_) return;

    FrameData& f = frames_.Cur();

    vkCmdEndRenderPass(f.cmd);
    VK_CHECK(vkEndCommandBuffer(f.cmd), "vkEndCommandBuffer failed");

    VkSemaphore renderFinished = swap_.GetRenderFinishedSemaphore(imageIndex_);
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = &f.imageAvailable;
    si.pWaitDstStageMask = &waitStage;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &f.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &renderFinished;
    VK_CHECK(vkQueueSubmit(ctx_.GetGraphicsQueue(), 1, &si, f.inFlight),
             "vkQueueSubmit failed");

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &renderFinished;

    VkSwapchainKHR sc = swap_.Get();
    pi.swapchainCount = 1;
    pi.pSwapchains = &sc;
    pi.pImageIndices = &imageIndex_;

    VkResult pres = vkQueuePresentKHR(ctx_.GetPresentQueue(), &pi);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
        uint32_t w = currentW_;
        uint32_t h = currentH_;
        surface_.GetSize(w, h);
        OnResize(w, h);
    } else if (pres != VK_SUCCESS) {
        throw std::runtime_error("vkQueuePresentKHR failed");
    }

    frames_.Next();
}

GpuMeshRef VulkanRendererBackend::EnsureGpuMesh(const Mesh& mesh) {
    auto it = meshCache_.find(&mesh);
    if (it != meshCache_.end()) return it->second;

    GpuMeshRef ref{};
    ref.indexCount = static_cast<uint32_t>(mesh.indices.size());
    if (mesh.vertices.empty() || mesh.indices.empty()) {
        meshCache_.emplace(&mesh, ref);
        return ref;
    }

    VkDevice device = ctx_.GetDevice();
    VkPhysicalDevice phys = ctx_.GetPhysicalDevice();

    CreateBuffer(device,
                 phys,
                 sizeof(Vertex3D) * mesh.vertices.size(),
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 ref.vb,
                 ref.vbMem);

    void* vbData = nullptr;
    VK_CHECK(vkMapMemory(device, ref.vbMem, 0, VK_WHOLE_SIZE, 0, &vbData), "vkMapMemory VB failed");
    std::memcpy(vbData, mesh.vertices.data(), sizeof(Vertex3D) * mesh.vertices.size());
    vkUnmapMemory(device, ref.vbMem);

    CreateBuffer(device,
                 phys,
                 sizeof(uint32_t) * mesh.indices.size(),
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 ref.ib,
                 ref.ibMem);

    void* ibData = nullptr;
    VK_CHECK(vkMapMemory(device, ref.ibMem, 0, VK_WHOLE_SIZE, 0, &ibData), "vkMapMemory IB failed");
    std::memcpy(ibData, mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size());
    vkUnmapMemory(device, ref.ibMem);

    ref.valid = true;
    meshCache_.emplace(&mesh, ref);
    return ref;
}

GpuMaterialRef VulkanRendererBackend::EnsureGpuMaterial(const Material& mat) {
    auto it = materialCache_.find(&mat);
    if (it != materialCache_.end()) return it->second;

    GpuMaterialRef ref{};
    ref.flags = static_cast<uint32_t>(mat.mode);

    const Texture2D* tex = mat.albedo;
    static Texture2D fallbackWhite = [] {
        Texture2D t;
        t.w = 1;
        t.h = 1;
        t.pixels = {0xFFFFFFFF};
        return t;
    }();
    if (!tex) tex = &fallbackWhite;

    VkDevice device = ctx_.GetDevice();
    VkPhysicalDevice phys = ctx_.GetPhysicalDevice();

    VkDeviceSize uploadSize = static_cast<VkDeviceSize>(tex->w) * static_cast<VkDeviceSize>(tex->h) * 4;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    CreateBuffer(device,
                 phys,
                 uploadSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer,
                 stagingMemory);

    void* pixels = nullptr;
    VK_CHECK(vkMapMemory(device, stagingMemory, 0, uploadSize, 0, &pixels), "vkMapMemory texture staging failed");
    std::memcpy(pixels, tex->pixels.data(), static_cast<size_t>(uploadSize));
    vkUnmapMemory(device, stagingMemory);
    CreateImage(device,
                phys,
                static_cast<uint32_t>(tex->w),
                static_cast<uint32_t>(tex->h),
                VK_FORMAT_B8G8R8A8_UNORM,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                ref.image,
                ref.imageMem);

    VkCommandPool uploadPool = VK_NULL_HANDLE;
    VkCommandBuffer uploadCmd = BeginImmediateCommands(device, ctx_.GetGraphicsFamily(), uploadPool);

    TransitionImageLayout(uploadCmd,
                          ref.image,
                          VK_FORMAT_B8G8R8A8_UNORM,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          0,
                          VK_ACCESS_TRANSFER_WRITE_BIT);

    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.mipLevel = 0;
    copy.imageSubresource.baseArrayLayer = 0;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {static_cast<uint32_t>(tex->w), static_cast<uint32_t>(tex->h), 1};
    vkCmdCopyBufferToImage(uploadCmd,
                           stagingBuffer,
                           ref.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &copy);

    TransitionImageLayout(uploadCmd,
                          ref.image,
                          VK_FORMAT_B8G8R8A8_UNORM,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT);

    EndImmediateCommands(device, ctx_.GetGraphicsQueue(), uploadPool, uploadCmd);
    DestroyBuffer(device, stagingBuffer, stagingMemory);

    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image = ref.image;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_B8G8R8A8_UNORM;
    vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    vci.subresourceRange.baseMipLevel = 0;
    vci.subresourceRange.levelCount = 1;
    vci.subresourceRange.baseArrayLayer = 0;
    vci.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(device, &vci, nullptr, &ref.view), "vkCreateImageView failed");

    VkSamplerCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sci.magFilter = VK_FILTER_NEAREST;
    sci.minFilter = VK_FILTER_NEAREST;
    sci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    sci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sci.maxAnisotropy = 1.0f;
    sci.maxLod = 1.0f;
    VK_CHECK(vkCreateSampler(device, &sci, nullptr, &ref.sampler), "vkCreateSampler failed");

    VkDescriptorSetAllocateInfo dsa{};
    dsa.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsa.descriptorPool = descriptorPool_;
    dsa.descriptorSetCount = 1;
    dsa.pSetLayouts = &materialSetLayout_;
    VK_CHECK(vkAllocateDescriptorSets(device, &dsa, &ref.set), "vkAllocateDescriptorSets material failed");

    VkDescriptorImageInfo di{};
    di.sampler = ref.sampler;
    di.imageView = ref.view;
    di.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = ref.set;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &di;
    vkUpdateDescriptorSets(device, 1, &w, 0, nullptr);

    ref.valid = true;
    materialCache_.emplace(&mat, ref);
    return ref;
}

void VulkanRendererBackend::UpdateFrameUBO(uint32_t frameIndex, const GPUFrameUBO& d) {
    frameUbo_ = d;
    if (frameIndex >= FrameManager::kMaxFrames) return;
    if (frameUboMem_[frameIndex] == VK_NULL_HANDLE) return;

    void* ptr = nullptr;
    VK_CHECK(vkMapMemory(ctx_.GetDevice(), frameUboMem_[frameIndex], 0, sizeof(GPUFrameUBO), 0, &ptr),
             "vkMapMemory frame UBO failed");
    std::memcpy(ptr, &d, sizeof(GPUFrameUBO));
    vkUnmapMemory(ctx_.GetDevice(), frameUboMem_[frameIndex]);
}

VkDescriptorSet VulkanRendererBackend::GetFrameSet(uint32_t frameIndex) {
    if (frameIndex < FrameManager::kMaxFrames) {
        return frameSets_[frameIndex];
    }
    return VK_NULL_HANDLE;
}

VkPipeline VulkanRendererBackend::GetScenePipeline() {
    return scenePipeline_;
}

VkPipelineLayout VulkanRendererBackend::GetScenePipelineLayout() {
    return scenePipelineLayout_;
}

VkExtent2D VulkanRendererBackend::GetCurrentExtent() {
    return swap_.GetExtent();
}
