#ifndef HML_DEVICE
#define HML_DEVICE

#include <iostream>
#include <optional>
#include <vector>
#include <cstring> // strcmp
#include <set>
#include <algorithm>
#include <span>
#include <unordered_map>
#include <array>

#include "settings.h"
#include "HmlWindow.h"


struct HmlDevice {
    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };


    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        inline bool isComplete() const noexcept {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };


    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice; // implicitly cleaned-up upon VkInstance destruction
    QueueFamilyIndices queueFamilyIndices;
    VkPhysicalDeviceMemoryProperties memProperties;
    VkDevice device;
    VkQueue graphicsQueue; // implicitly cleaned-up upon VkDevice destruction
    VkQueue presentQueue;  // implicitly cleaned-up upon VkDevice destruction


    static constexpr bool enableValidationLayers = true;
    static constexpr std::array<const char*, 1> requiredValidationLayers = {{
        "VK_LAYER_KHRONOS_validation"
    }};
    static constexpr std::array<const char*, 1> requiredDeviceExtensions = {{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    }};


    static std::unique_ptr<HmlDevice> create(std::shared_ptr<HmlWindow> hmlWindow) noexcept;
    ~HmlDevice() noexcept;
    // ========================================================================
    // Must be queried each time at recreation because swapChainExtent could be
    // restricted by the window manager to match the actual OS window.
    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) noexcept;
    // ========================================================================

    private:

    static VkInstance createInstance(const char* applicationName) noexcept;
    static bool checkValidationLayerSupport(std::span<const char* const> requestedValidationLayers) noexcept;
    static std::vector<VkLayerProperties> getAvailableLayers() noexcept;
    static std::vector<const char*> getRequiredExtensions(bool enableValidationLayers) noexcept;
    static VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window) noexcept;
    static VkDebugUtilsMessengerEXT createDebugMessanger(const VkInstance& instance) noexcept;
    static bool loadVulkanFunctions(VkInstance instance);
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData) noexcept;
    static VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) noexcept;
    static void describePhysicalDevice(VkPhysicalDevice physicalDevice) noexcept;
    static bool isPhysicalDeviceSuitable(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface) noexcept;
    static bool checkDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice, std::span<const char* const> requiredDeviceExtensions) noexcept;
    static std::vector<VkQueueFamilyProperties> queryQueueFamilies(VkPhysicalDevice physicalDevice) noexcept;
    static QueueFamilyIndices pickQueueFamilyIndices(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) noexcept;
    static VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice,
            const QueueFamilyIndices& queueFamilyIndices) noexcept;
};

#endif
