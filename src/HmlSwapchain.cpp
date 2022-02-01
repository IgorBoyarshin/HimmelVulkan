#include "HmlSwapchain.h"


std::unique_ptr<HmlSwapchain> HmlSwapchain::create(
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlResourceManager> hmlResourceManager,
        const std::optional<VkSwapchainKHR>& oldSwapchain) noexcept {
    auto hmlSwapchain = std::make_unique<HmlSwapchain>();
    hmlSwapchain->hmlDevice = hmlDevice;
    hmlSwapchain->hmlResourceManager = hmlResourceManager;

    const auto swapChainSupportDetails = HmlDevice::querySwapChainSupport(hmlDevice->physicalDevice, hmlDevice->surface);

    auto surfaceFormat = chooseSwapSurfaceFormat(swapChainSupportDetails.formats);
    hmlSwapchain->imageFormat = surfaceFormat.format;
    hmlSwapchain->extent = hmlSwapchain->chooseSwapExtent(swapChainSupportDetails.capabilities, hmlWindow->getFramebufferSize());
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
    createInfo.oldSwapchain = oldSwapchain ? (*oldSwapchain) : VK_NULL_HANDLE; // for when resizing happens

    auto res = vkCreateSwapchainKHR(hmlDevice->device, &createInfo, nullptr, &(hmlSwapchain->swapchain));
    if (res == VK_ERROR_NATIVE_WINDOW_IN_USE_KHR) {
        std::cerr << "::> Failed to create Swapchain because NATIVE_WINDOW_IN_USE.\n";
        return { nullptr };
    } else if (res != VK_SUCCESS) {
        std::cerr << "::> Failed to create Swapchain.\n";
        return { nullptr };
    }

    hmlSwapchain->imageViews = hmlSwapchain->createSwapchainImageViews();
    if (hmlSwapchain->imageViews.empty()) {
        std::cerr << "::> Failed to create Swapchain ImageViews.\n";
        return { nullptr };
    }

    return hmlSwapchain;
}


HmlSwapchain::~HmlSwapchain() noexcept {
    std::cout << ":> Destroying HmlSwapchain...\n";

    for (auto imageView : imageViews) {
        vkDestroyImageView(hmlDevice->device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(hmlDevice->device, swapchain, nullptr);
}


VkSurfaceFormatKHR HmlSwapchain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) noexcept {
    // Surface format === color depth
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}


VkPresentModeKHR HmlSwapchain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) noexcept {
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


VkExtent2D HmlSwapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
        const std::pair<uint32_t, uint32_t>& framebufferSize) noexcept {
    if (capabilities.currentExtent.width == UINT32_MAX) {
        // The window manager allows us to differ from the resolution of the window

        const auto minWidth  = capabilities.minImageExtent.width;
        const auto maxWidth  = capabilities.maxImageExtent.width;
        const auto minHeight = capabilities.minImageExtent.height;
        const auto maxHeight = capabilities.maxImageExtent.height;
        return VkExtent2D{
            std::clamp(framebufferSize.first, minWidth, maxWidth),
            std::clamp(framebufferSize.second, minHeight, maxHeight)
        };
    } else {
        // Must match the size of the actual OS window
        return capabilities.currentExtent;
    }
}


// TODO either inline or extract the other code of swapchain creation into a separate function
std::vector<VkImageView> HmlSwapchain::createSwapchainImageViews() noexcept {
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
