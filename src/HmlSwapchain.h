#ifndef HML_SWAPCHAIN
#define HML_SWAPCHAIN

#include <iostream>
#include <optional>
#include <algorithm>

#include "settings.h"
#include "HmlWindow.h"
#include "HmlDevice.h"
#include "HmlResourceManager.h"


struct HmlSwapchain {
    VkSwapchainKHR swapchain;
    // VkFormat imageFormat;
    // VkExtent2D extent;

    // std::vector<VkImageView> imageViews;

    std::vector<std::shared_ptr<HmlImageResource>> imageResources;

    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;


    static std::unique_ptr<HmlSwapchain> create(
            std::shared_ptr<HmlWindow> hmlWindow,
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlResourceManager> hmlResourceManager,
            const std::optional<VkSwapchainKHR>& oldSwapchain) noexcept;
    ~HmlSwapchain() noexcept;

    inline size_t imageCount() const noexcept {
        return imageResources.size();
    }

    inline VkExtent2D extent() const noexcept {
        assert((!imageResources.empty()) && (imageResources[0]) && "::> Swapchain image resources are unexpectedly empty.\n");
        const auto& image = imageResources[0];
        return VkExtent2D{ image->width, image->height };
    }

    inline float extentAspect() const noexcept {
        const auto e = extent();
        return static_cast<float>(e.width) / static_cast<float>(e.height);
    }

    private:

    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) noexcept;
    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) noexcept;
    static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
        const std::pair<uint32_t, uint32_t>& framebufferSize) noexcept;
    // TODO nocheckin remove
    // std::vector<VkImageView> createSwapchainImageViews() noexcept;
};

#endif
