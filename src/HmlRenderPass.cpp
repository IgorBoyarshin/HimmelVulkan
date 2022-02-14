#include "HmlRenderPass.h"


void HmlRenderPass::begin(VkCommandBuffer commandBuffer, uint32_t imageIndex) const noexcept {
    hmlCommands->beginRecordingPrimaryOnetime(commandBuffer);

    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = VK_NULL_HANDLE,
        .renderPass = renderPass,
        .framebuffer = framebuffers[imageIndex],
        .renderArea = {
            .offset = {0, 0},
            .extent = extent,
        },
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };

    // const auto type = isPrimary ? VK_SUBPASS_CONTENTS_INLINE : VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
    const auto type = VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, type);
}


void HmlRenderPass::end(VkCommandBuffer commandBuffer) const noexcept {
    vkCmdEndRenderPass(commandBuffer);
    hmlCommands->endRecording(commandBuffer);
}


std::unique_ptr<HmlRenderPass> HmlRenderPass::create(
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands,
        Config&& config) noexcept {
    std::vector<VkAttachmentDescription> attachments;

    // Color
    std::vector<VkAttachmentReference> colorAttachmentRefs;
    for (const auto& attachment : config.colorAttachments) {
        const uint32_t index = attachments.size();
        colorAttachmentRefs.push_back({
            .attachment = index,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        });

        attachments.push_back({
            .flags = 0,
            .format = attachment.imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT, // multisampling
            // loadOp:
            // VK_ATTACHMENT_LOAD_OP_LOAD: Preserve the existing contents of the attachment;
            // VK_ATTACHMENT_LOAD_OP_CLEAR: Clear the values to a constant at the start;
            // VK_ATTACHMENT_LOAD_OP_DONT_CARE: Existing contents are undefined; we don't care about them.
            .loadOp = attachment.clearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            // storeOp:
            // VK_ATTACHMENT_STORE_OP_STORE: Rendered contents will be stored
            // in memory and can be read later;
            // VK_ATTACHMENT_STORE_OP_DONT_CARE: Contents of the framebuffer will be
            // undefined after the rendering operation.
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = attachment.preLayout,
            .finalLayout = attachment.postLayout,
        });
    }

    // Depth
    VkAttachmentReference depthAttachmentRef;
    if (config.depthStencilAttachment) {
        const auto attachment = *config.depthStencilAttachment;
        const uint32_t index = attachments.size();
        depthAttachmentRef = VkAttachmentReference{
            .attachment = index,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };
        attachments.push_back({
            .flags = 0,
            .format = attachment.imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = attachment.clearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = (config.depthStencilAttachment->saveDepth) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = (attachment.hasPrevious) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        });
    }


    VkSubpassDescription subpass = {};
    {
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = colorAttachmentRefs.size();
        subpass.pColorAttachments = colorAttachmentRefs.data();
        subpass.pDepthStencilAttachment = config.depthStencilAttachment ? &depthAttachmentRef : VK_NULL_HANDLE;
    }


    VkSubpassDependency dependency = {};
    {
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT // XXX
            | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT // because we have a loadOp that clears
            | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;;
    }


    VkRenderPassCreateInfo renderPassInfo = {};
    {
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
    }

    VkRenderPass renderPass;
    if (vkCreateRenderPass(hmlDevice->device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        std::cerr << "::> Failed to create HmlRenderPass.\n";
        return { nullptr };
    }

    auto hmlRenderPass = std::make_unique<HmlRenderPass>();
    hmlRenderPass->hmlDevice = hmlDevice;
    hmlRenderPass->hmlCommands = hmlCommands;
    hmlRenderPass->renderPass = renderPass;
    hmlRenderPass->extent = config.extent;

    if (config.depthStencilAttachment) {
        // NOTE In order to make sure the depth resource is not destroyed before we are finished
        // hmlRenderPass->_hmlDepthResource = config.depthStencilAttachment->hmlDepthResource;
    }

    // Create and store framebuffers
    assert(!config.colorAttachments.empty() && "::> No color attachments specified for HmlRenderPass.");
    const auto images = config.colorAttachments[0].imageViews.size();
    for (size_t i = 0; i < images; i++) {
        std::vector<VkImageView> imageViews;
        for (const auto& colorAttachment : config.colorAttachments) {
            imageViews.push_back(colorAttachment.imageViews[i]);
        }
        // TODO XXX Why is this true?
        // The same depth image can be shared because only a single subpass
        // is running at the same time due to our semaphores.
        if (config.depthStencilAttachment) {
            imageViews.push_back(config.depthStencilAttachment->imageView);
        }
        const auto framebuffer = hmlRenderPass->createFramebuffer(imageViews);
        if (!framebuffer) {
            std::cerr << "::> Failed to create HmlRenderPass.\n";
            return { nullptr };
        }
        hmlRenderPass->framebuffers.push_back(framebuffer);
    }

    // Prepare and store clearValues for beginRenderPass
    for (const auto& attachment : config.colorAttachments) {
        VkClearValue value;
        value.color = attachment.clearColor.value_or(VkClearColorValue{}),
        hmlRenderPass->clearValues.push_back(value); // will be ignored if not used
    }
    if (config.depthStencilAttachment) {
        VkClearValue value;
        value.depthStencil = config.depthStencilAttachment->clearColor.value_or(VkClearDepthStencilValue{});
        hmlRenderPass->clearValues.push_back(value); // will be ignored if not used
    }

    return hmlRenderPass;
}


VkFramebuffer HmlRenderPass::createFramebuffer(const std::vector<VkImageView>& colorAndDepthImageViews) const noexcept {
    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = static_cast<uint32_t>(colorAndDepthImageViews.size());
    framebufferInfo.pAttachments = colorAndDepthImageViews.data();
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = 1; // number of layers in image arrays

    VkFramebuffer framebuffer;
    if (vkCreateFramebuffer(hmlDevice->device, &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
        std::cerr << "::> Failed to create a Framebuffer.\n";
        return VK_NULL_HANDLE;
    }

    return framebuffer;
}


HmlRenderPass::~HmlRenderPass() noexcept {
    std::cout << ":> Destroying HmlRenderPass.\n";

    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(hmlDevice->device, framebuffer, nullptr);
    }
    vkDestroyRenderPass(hmlDevice->device, renderPass, nullptr);
}
