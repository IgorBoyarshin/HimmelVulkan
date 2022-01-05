#ifndef HML_SWAPCHAIN
#define HML_SWAPCHAIN

#include "HmlWindow.h"
#include "HmlDevice.h"
#include "HmlResourceManager.h"
#include "util.h"


struct HmlSwapchain {
    VkSwapchainKHR swapchain;
    VkFormat imageFormat;
    VkExtent2D extent;
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;
    VkImage depthImage;
    VkDeviceMemory depthImageMemory;
    VkImageView depthImageView;
    VkRenderPass renderPass;


    // Is stored in order to lighten the recreation later
    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;


    // VkSwapchainKHR
    static std::unique_ptr<HmlSwapchain> create(
            std::shared_ptr<HmlWindow> hmlWindow,
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlResourceManager> hmlResourceManager) {
        auto hmlSwapchain = std::make_unique<HmlSwapchain>();
        hmlSwapchain->hmlDevice = hmlDevice;
        hmlSwapchain->hmlWindow = hmlWindow;
        hmlSwapchain->hmlResourceManager = hmlResourceManager;

        const auto swapChainSupportDetails = HmlDevice::querySwapChainSupport(hmlDevice->physicalDevice, hmlDevice->surface);

        auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupportDetails.formats);
        hmlSwapchain->imageFormat = surfaceFormat.format;
        hmlSwapchain->extent = hmlSwapchain->chooseSwapExtent(swapChainSupportDetails.capabilities);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupportDetails.presentModes);

        // NOTE for current use the logic can be simplified
        uint32_t imageCount = swapChainSupportDetails.capabilities.minImageCount + 1;
        const bool supportsUnlimitedAmount = swapChainSupportDetails.capabilities.maxImageCount == 0;
        if (!supportsUnlimitedAmount && imageCount > swapChainSupportDetails.capabilities.maxImageCount) {
            imageCount = swapChainSupportDetails.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = hmlDevice->surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = hmlSwapchain->extent;
        createInfo.imageArrayLayers = 1; // always 1
        // Any from VkImageUsageFlags.
        // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT is always supported.
        // VK_IMAGE_USAGE_TRANSFER_DST_BIT  -- for post-processing.
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        const auto graphicsFamily = hmlDevice->queueFamilyIndices.graphicsFamily.value();
        const auto presentFamily  = hmlDevice->queueFamilyIndices.presentFamily.value();
        if (graphicsFamily == presentFamily) { // most likely
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // best performance
        } else {
            uint32_t indices[] = { graphicsFamily, presentFamily };
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = indices;
        }
        // e.g. do a 90-degree rotation or horizontal flip here
        createInfo.preTransform = swapChainSupportDetails.capabilities.currentTransform; // do not transform
        // Whether to blend with other windows in the window system
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // ignore, no blending
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE; // clip pixels that are obscured by other windows
        createInfo.oldSwapchain = VK_NULL_HANDLE; // for when resizing happens

        if (vkCreateSwapchainKHR(hmlDevice->device, &createInfo, nullptr, &(hmlSwapchain->swapchain)) != VK_SUCCESS) {
            std::cerr << "::> Failed to create Swapchain.\n";
            return { nullptr };
        }

        hmlSwapchain->imageViews = hmlSwapchain->createSwapchainImageViews();
        if (hmlSwapchain->imageViews.empty()) {
            std::cerr << "::> Failed to create Swapchain ImageViews.\n";
            return { nullptr };
        }

        hmlResourceManager->createDepthResources(
            hmlSwapchain->depthImage,
            hmlSwapchain->depthImageMemory,
            hmlSwapchain->depthImageView,
            hmlSwapchain->extent);

        hmlSwapchain->renderPass = hmlSwapchain->createRenderPass();

        hmlSwapchain->framebuffers = hmlSwapchain->createFramebuffers();
        if (hmlSwapchain->framebuffers.empty()) {
            std::cerr << "::> Failed to create Swapchain Framebuffers.\n";
            return { nullptr };
        }

        return hmlSwapchain;
    }


    ~HmlSwapchain() {
        std::cout << ":> Destroying HmlSwapchain...\n";

        vkDestroyImageView(hmlDevice->device, depthImageView, nullptr);
        vkDestroyImage(hmlDevice->device, depthImage, nullptr);
        vkFreeMemory(hmlDevice->device, depthImageMemory, nullptr);

        for (auto framebuffer : framebuffers) {
            vkDestroyFramebuffer(hmlDevice->device, framebuffer, nullptr);
        }

        vkDestroyRenderPass(hmlDevice->device, renderPass, nullptr);

        for (auto imageView : imageViews) {
            vkDestroyImageView(hmlDevice->device, imageView, nullptr);
        }
        vkDestroySwapchainKHR(hmlDevice->device, swapchain, nullptr);
    }


    size_t imagesCount() const noexcept {
        return imageViews.size();
    }


    float extentAspect() const noexcept {
        return static_cast<float>(extent.width) / static_cast<float>(extent.height);
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    private:

    std::vector<VkFramebuffer> createFramebuffers() {
        std::vector<VkFramebuffer> swapChainFramebuffers(imageViews.size());
        for (size_t i = 0; i < imageViews.size(); i++) {
            // The same depth image can be shared because only a single subpass
            // is running at the same time due to our semaphores.
            std::array<VkImageView, 2> attachments = {
                imageViews[i],
                depthImageView
            };

            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = extent.width;
            framebufferInfo.height = extent.height;
            framebufferInfo.layers = 1; // number of layers in image arrays

            if (vkCreateFramebuffer(hmlDevice->device, &framebufferInfo, nullptr,
                    &swapChainFramebuffers[i]) != VK_SUCCESS) {
                return {};
            }
        }

        return swapChainFramebuffers;
    }

    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        // Surface format === color depth
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }

        return availableFormats[0];
    }


    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        // FIFO will display all generated images in-order.
        // Mailbox will always display the most recent available image.

        // Try to find Mailbox -- analog of tripple buffering.
        // for (const auto& availablePresentMode : availablePresentModes) {
        //     if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        //         return availablePresentMode;
        //     }
        // }

        // return VK_PRESENT_MODE_IMMEDIATE_KHR;
        return VK_PRESENT_MODE_FIFO_KHR; // guaranteed to be available
    }


    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width == UINT32_MAX) {
            // The window manager allows us to differ from the resolution of the window

            const auto size = hmlWindow->getFramebufferSize();
            const auto minWidth  = capabilities.minImageExtent.width;
            const auto maxWidth  = capabilities.maxImageExtent.width;
            const auto minHeight = capabilities.minImageExtent.height;
            const auto maxHeight = capabilities.maxImageExtent.height;
            return {
                std::clamp(size.first, minWidth, maxWidth),
                std::clamp(size.second, minHeight, maxHeight)
            };
        } else {
            // Must match the size of the actual OS window
            return capabilities.currentExtent;
        }
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    VkRenderPass createRenderPass() {
        VkAttachmentDescription colorAttachment = {};
        {
            colorAttachment.format = imageFormat;
            colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // relates to multisampling
            // loadOp:
            // VK_ATTACHMENT_LOAD_OP_LOAD: Preserve the existing contents of the attachment;
            // VK_ATTACHMENT_LOAD_OP_CLEAR: Clear the values to a constant at the start;
            // VK_ATTACHMENT_LOAD_OP_DONT_CARE: Existing contents are undefined; we don't care about them.
            colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // color and depth data
            // storeOp:
            // VK_ATTACHMENT_STORE_OP_STORE: Rendered contents will be stored
            // in memory and can be read later;
            // VK_ATTACHMENT_STORE_OP_DONT_CARE: Contents of the framebuffer will be
            // undefined after the rendering operation.
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // color and depth data
            colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        VkAttachmentReference colorAttachmentRef = {};
        {
            colorAttachmentRef.attachment = 0;
            colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }


        // TODO
        // TODO depthFormat should probably be cached just as imageFormat above
        // TODO
        VkAttachmentDescription depthAttachment{};
        {
            depthAttachment.format = hmlResourceManager->findDepthFormat();
            depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            // Because won't be used after the drawing has finished:
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // don't care about previous contents
            depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        VkAttachmentReference depthAttachmentRef{};
        {
            depthAttachmentRef.attachment = 1;
            depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }

        VkSubpassDescription subpass = {};
        {
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorAttachmentRef; // directly referenced in fragment shader output!
            subpass.pDepthStencilAttachment = &depthAttachmentRef;
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


        std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

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
            throw std::runtime_error("failed to create render pass!"); // TODO
        }
        return renderPass;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    // TODO either inline or extract the other code of swapchain creation into a separate function
    std::vector<VkImageView> createSwapchainImageViews() {
        uint32_t imageCount;
        vkGetSwapchainImagesKHR(hmlDevice->device, swapchain, &imageCount, nullptr);
        std::vector<VkImage> swapChainImages(imageCount);
        vkGetSwapchainImagesKHR(hmlDevice->device, swapchain, &imageCount, swapChainImages.data());

        std::vector<VkImageView> imageViews(imageCount);
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            imageViews[i] = hmlResourceManager->createImageView(swapChainImages[i], imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
            if (!imageViews[i]) return {};
        }

        return imageViews;
    }
};

#endif
