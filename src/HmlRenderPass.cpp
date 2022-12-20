#include "HmlRenderPass.h"


void HmlRenderPass::begin(VkCommandBuffer commandBuffer, uint32_t imageIndex) const noexcept {
    // hmlCommands->beginRecordingPrimaryOnetime(commandBuffer);

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
    // hmlCommands->endRecording(commandBuffer);
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

        assert(!attachment.imageResources.empty() && "::> ColorAttachment with no imageResources.");
        const auto imageFormat = attachment.imageResources[0]->format;
        VkAttachmentLoadOp loadOp;
        switch (attachment.loadColor.type) {
            case LoadColor::Type::Clear:    loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; break;
            case LoadColor::Type::Load:     loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; break;
            case LoadColor::Type::DontCare: loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; break;
            default: assert(false && "::> Unexpected loadColor type."); loadOp = static_cast<VkAttachmentLoadOp>(0); // stub
        }
        attachments.push_back({
            .flags = 0,
            .format = imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT, // multisampling
            .loadOp = loadOp,
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
        const auto& attachment = *config.depthStencilAttachment;
        const uint32_t index = attachments.size();
        depthAttachmentRef = VkAttachmentReference{
            .attachment = index,
            .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        };

        assert(!attachment.imageResources.empty() && "::> DepthStencilAttachment with no imageResources.");
        const auto imageFormat = attachment.imageResources[0]->format;
        attachments.push_back({
            .flags = 0,
            .format = imageFormat,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = attachment.clearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = (config.depthStencilAttachment->store) ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = attachment.preLayout,
            .finalLayout = attachment.postLayout,
        });
    }


    VkSubpassDescription subpass = {};
    {
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = colorAttachmentRefs.size();
        subpass.pColorAttachments = colorAttachmentRefs.data();
        subpass.pDepthStencilAttachment = config.depthStencilAttachment ? &depthAttachmentRef : VK_NULL_HANDLE;
    }


    // VkSubpassDependency dependency = {};
    // {
    //     dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    //     dependency.dstSubpass = 0;
    //     dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    //     dependency.srcAccessMask = 0;
    //     dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    //     dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT // XXX
    //         | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; // because we have a loadOp that clears
    // }


    VkRenderPassCreateInfo renderPassInfo = {};
    {
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = config.subpassDependencies.size();
        renderPassInfo.pDependencies = config.subpassDependencies.data();
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
    hmlRenderPass->colorAttachmentCount = config.colorAttachments.size();

    // Create and store framebuffers
    // NOTE The logic allows to specify only one imageResource if the top-level
    // decides not to create multiple resources per image-in-flight. However,
    // as of now there must be at least one Attachment (color/depth) that has
    // all (3) imageResources specified, so that we can deduce the size (3).
    // NOTE Maybe it is worth passing the image-in-flight count to avoid this edger case?
    assert((!config.colorAttachments.empty() || config.depthStencilAttachment) && "::> Trying to create HmlRenderPass with no color/depth attachments.");
    size_t imageCount = 0;
    for (const auto& attachment : config.colorAttachments) {
        imageCount = std::max(imageCount, attachment.imageResources.size());
    }
    if (config.depthStencilAttachment) {
        imageCount = std::max(imageCount, config.depthStencilAttachment->imageResources.size());
    }
    assert(imageCount && "::> Trying to create HmlRenderPass with no HmlImageResources in color/depth attachments.");
    // NOTE remove this check once we have improved the count logic:
    assert(imageCount == 3 && "::> At least one attachment must have all (3) HmlImageResources specified!");
    for (size_t i = 0; i < imageCount; i++) {
        std::vector<VkImageView> imageViews;
        for (const auto& colorAttachment : config.colorAttachments) {
            const bool singular = colorAttachment.imageResources.size() == 1;
            const auto imageView = singular ? colorAttachment.imageResources[0]->view : colorAttachment.imageResources[i]->view;
            imageViews.push_back(imageView);
        }
        if (config.depthStencilAttachment) {
            const bool singular = config.depthStencilAttachment->imageResources.size() == 1;
            const auto imageView = singular ? config.depthStencilAttachment->imageResources[0]->view : config.depthStencilAttachment->imageResources[i]->view;
            imageViews.push_back(imageView);
        }
        const auto framebuffer = hmlRenderPass->createFramebuffer(imageViews);
        if (!framebuffer) {
            std::cerr << "::> Failed to create framebuffer.\n";
            return { nullptr };
        }
        hmlRenderPass->framebuffers.push_back(framebuffer);
    }

    // Prepare and store clearValues for beginRenderPass
    for (const auto& attachment : config.colorAttachments) {
        VkClearValue value;
        value.color = attachment.loadColor.clearColor.value_or(VkClearColorValue{}),
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
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlRenderPass.\n";
#endif

    for (auto framebuffer : framebuffers) {
        vkDestroyFramebuffer(hmlDevice->device, framebuffer, nullptr);
    }
    vkDestroyRenderPass(hmlDevice->device, renderPass, nullptr);
}
