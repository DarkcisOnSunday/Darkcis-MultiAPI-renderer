#include "Framebuffers.h"

#include <stdexcept>

static void VK_CHECK(VkResult r, const char* msg) {
    if (r != VK_SUCCESS) throw std::runtime_error(msg);
}

void Framebuffers::Create(VkDevice device, VkRenderPass renderPass, const std::vector<VkImageView>& views, VkExtent2D extent)
{
    fbs_.resize(views.size());

    for (size_t i = 0; i < views.size(); i++) {
        VkImageView attachments[] = { views[i] };

        VkFramebufferCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        ci.renderPass = renderPass;
        ci.attachmentCount = 1;
        ci.pAttachments = attachments;
        ci.width  = extent.width;
        ci.height = extent.height;
        ci.layers = 1;

        VK_CHECK(vkCreateFramebuffer(device, &ci, nullptr, &fbs_[i]),
                 "vkCreateFramebuffer failed");
    }
}

void Framebuffers::Destroy(VkDevice device) {
    for (auto fb : fbs_) {
        if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    }
    fbs_.clear();
}