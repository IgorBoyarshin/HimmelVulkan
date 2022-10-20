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
    VkFormat imageFormat;
    VkExtent2D extent;

    std::vector<VkImageView> imageViews;

    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;


    static std::unique_ptr<HmlSwapchain> create(
            std::shared_ptr<HmlWindow> hmlWindow,
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlResourceManager> hmlResourceManager,
            const std::optional<VkSwapchainKHR>& oldSwapchain) noexcept;
    ~HmlSwapchain() noexcept;

    inline size_t imageCount() const noexcept {
        return imageViews.size();
    }

    inline float extentAspect() const noexcept {
        return static_cast<float>(extent.width) / static_cast<float>(extent.height);
    }

    private:

    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) noexcept;
    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) noexcept;
    static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
            const std::pair<uint32_t, uint32_t>& framebufferSize) noexcept;
    std::vector<VkImageView> createSwapchainImageViews() noexcept;
};

#endif
