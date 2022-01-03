#ifndef HML_DEVICE
#define HML_DEVICE

#include <optional>
#include <vector>
#include <cstring> // strcmp
#include <set>

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

        bool isComplete() const {
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
    static constexpr auto requiredValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    static constexpr auto requiredDeviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };


    static std::unique_ptr<HmlDevice> create(std::shared_ptr<HmlWindow> hmlWindow) {
        auto hmlDevice = std::make_unique<HmlDevice>();

        if (const auto obj = createInstance(hmlWindow->name); obj) {
            hmlDevice->instance = obj;
        } else return { nullptr };

        if (enableValidationLayers) {
            if (const auto obj = createDebugMessanger(hmlDevice->instance); obj) {
                hmlDevice->debugMessenger = obj;
            } else return { nullptr };
        }

        if (const auto obj = createSurface(hmlDevice->instance, hmlWindow->window); obj) {
            hmlDevice->surface = obj;
        } else return { nullptr };

        if (const auto physicalDevice = pickPhysicalDevice(hmlDevice->instance, hmlDevice->surface); physicalDevice) {
            hmlDevice->physicalDevice = physicalDevice;
            hmlDevice->queueFamilyIndices = pickQueueFamilyIndices(physicalDevice, hmlDevice->surface);
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &(hmlDevice->memProperties));
        } else return { nullptr };

        if (const auto device = createLogicalDevice(hmlDevice->physicalDevice, hmlDevice->queueFamilyIndices); device) {
            hmlDevice->device = device;
            // The penultimate parameter is the index of the queue within the family
            vkGetDeviceQueue(device, hmlDevice->queueFamilyIndices.graphicsFamily.value(), 0, &(hmlDevice->graphicsQueue));
            vkGetDeviceQueue(device, hmlDevice->queueFamilyIndices.presentFamily.value(),  0, &(hmlDevice->presentQueue));
        } else return { nullptr };

        return hmlDevice;
    }


    ~HmlDevice() {
        std::cout << ":> Destroying HmlDevice...\n";
        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        if (enableValidationLayers) {
            destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }
        vkDestroyInstance(instance, nullptr);
    }


    // Must be queried each time at recreation because swapChainExtent could be
    // restricted by the window manager to match the actual OS window.
    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
        SwapChainSupportDetails details;

        // Capabilities
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

        // Format
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface,
                    &formatCount, details.formats.data());
        }

        // Present mode
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface,
                    &presentModeCount, details.presentModes.data());
        }

        return details;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    private:

    static VkInstance createInstance(const char* applicationName) {
        if (enableValidationLayers && !checkValidationLayerSupport(requiredValidationLayers)) {
            std::cerr << "::> Validation layers requested, but not available.\n";
            return VK_NULL_HANDLE;
        }

        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = applicationName;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;
        const auto requiredExtensions = getRequiredExtensions(enableValidationLayers);
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(requiredValidationLayers.size());
            createInfo.ppEnabledLayerNames = std::data(requiredValidationLayers);
        } else {
            createInfo.enabledLayerCount = 0;
        }

        VkInstance instance;
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            std::cerr << "::> Failed to create VkInstance.\n";
            return VK_NULL_HANDLE;
        }
        return instance;
    }


    // TODO: use a range instead of a vector
    static bool checkValidationLayerSupport(const std::vector<const char*>& requestedValidationLayers) {
        const auto availableLayers = getAvailableLayers();
        for (const char* layerName : requestedValidationLayers) {
            const bool available = std::find_if(
                availableLayers.cbegin(), availableLayers.cend(),
                [&layerName](auto layerProperties) {
                    return strcmp(layerName, layerProperties.layerName) == 0;
                }) != availableLayers.cend();
            if (!available) return false;
        }

        return true;
    }


    static std::vector<VkLayerProperties> getAvailableLayers() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        return availableLayers;
    }


    // static std::vector<VkExtensionProperties> getAvailableExtensions() {
    //     uint32_t vkExtensionCount = 0;
    //     vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, nullptr);
    //     std::vector<VkExtensionProperties> vkExtensions(vkExtensionCount);
    //     vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, vkExtensions.data());
    //     return vkExtensions;
    // }


    static std::vector<const char*> getRequiredExtensions(bool enableValidationLayers) {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        // This function returns an array of names of Vulkan instance extensions
        // required by GLFW for creating Vulkan surfaces for GLFW windows.
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            // This extension allows us to create a debug messenger
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    static VkSurfaceKHR createSurface(VkInstance instance, GLFWwindow* window) {
        VkSurfaceKHR surface;
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
            std::cerr << "::> Failed to create VkSurface.\n";
            return VK_NULL_HANDLE;
        }
        return surface;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    static VkDebugUtilsMessengerEXT createDebugMessanger(const VkInstance& instance) {
        VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            // VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;
        // To pass data to function. For example, pointer to the Main class
        // createInfo.pUserData = nullptr;

        VkDebugUtilsMessengerEXT debugMessenger;
        if (createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            std::cerr << "::> Failed to set up debug messenger.\n";
            return VK_NULL_HANDLE;
        }
        return debugMessenger;
    }


    static VkResult createDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger) {
        const auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
        if (func) return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        else      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }


    static void destroyDebugUtilsMessengerEXT(
            VkInstance instance,
            VkDebugUtilsMessengerEXT debugMessenger,
            const VkAllocationCallbacks* pAllocator) {
        const auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(instance, debugMessenger, pAllocator);
    }


    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData) {

        // To get rid of numerous "Device Extension" infos
        if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) return VK_FALSE;

        std::cerr << "(validation layer) ";

        std::cerr << '[';
        switch (messageSeverity) {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: std::cerr << "verbose"; break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: std::cerr << "warning"; break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:   std::cerr << "error";   break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:    std::cerr << "info";    break;
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT: break; // stub
        }
        std::cerr << "]";

        std::cerr << '[';
        switch (messageType) {
            case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:     std::cerr << "general";     break;
            case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:  std::cerr << "validation";  break;
            case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: std::cerr << "performance"; break;
        }
        std::cerr << "]";

        std::cerr << " : " << pCallbackData->pMessage << '\n';

        // if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        return VK_FALSE;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    static VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            std::cerr << "::> Failed to find any GPU with Vulkan support.\n";
            return VK_NULL_HANDLE;
        }
        std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices.data());

        for (auto physicalDevice : physicalDevices) {
            if (isPhysicalDeviceSuitable(physicalDevice, surface)) return physicalDevice;
        }

        std::cerr << "::> Failed to find a suitable GPU.\n";
        return VK_NULL_HANDLE;
    }


    static void describePhysicalDevice(VkPhysicalDevice physicalDevice) {
        const auto queueFamilies = queryQueueFamilies(physicalDevice);

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);

        // NOTE: VkPhysicalDeviceLimits properties.limits
        std::cout
            << "\t[device name] = " << properties.deviceName << "\n"
            << "\t[api version] = " << properties.apiVersion << "\n"
            << "\t[driver version] = " << properties.driverVersion << "\n"
            << "\t[vendor id] = " << properties.vendorID << "\n"
            << "\t[device id] = " << properties.deviceID << "\n"
            << "\t[device type] = ";
        switch (properties.deviceType) {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: std::cout << "Integrated GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: std::cout << "Discrete GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: std::cout << "Virtual GPU"; break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU: std::cout << "CPU"; break;
            default: std::cout << "OTHER";
        }
        std::cout << '\n';

        std::cout << "\tQueue families[" << queueFamilies.size() << "]:\n";
        for (unsigned int i = 0; i < queueFamilies.size(); i++) {
            const auto queueFamily = queueFamilies[i];
            std::cout << "\t\t-- queue family #" << i << " has " << queueFamily.queueCount << " queues and supports: ";
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) std::cout << "GRAPHICS; ";
            if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) std::cout << "COMPUTE; ";
            if (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) std::cout << "TRANSFER; ";
            if (queueFamily.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) std::cout << "SPARSE BINDING; ";
            if (queueFamily.queueFlags & VK_QUEUE_PROTECTED_BIT) std::cout << "PROTECTED; ";
#ifdef VK_ENABLE_BETA_EXTENSIONS
            if (queueFamily.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) std::cout << "VIDEO DECODE; ";
            if (queueFamily.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) std::cout << "VIDEO ENCODE; ";
#endif
            if (!(queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) &&
                    ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) ||
                     (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))) {
                std::cout << "TRANSFER (implicit); ";
            }
            std::cout << '\n';
        }

        // VkPhysicalDeviceFeatures features;
        // vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    }


    // NOTE can use info from describePhysicalDevice
    static bool isPhysicalDeviceSuitable(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface) {
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);
        if (!supportedFeatures.samplerAnisotropy) return false;

        const auto queueFamilyIndices = pickQueueFamilyIndices(physicalDevice, surface);
        if (!queueFamilyIndices.isComplete()) return false;

        if (!checkDeviceExtensionSupport(physicalDevice, requiredDeviceExtensions)) return false;

        const auto swapChainSupportDetails = querySwapChainSupport(physicalDevice, surface);
        if (swapChainSupportDetails.formats.empty() || swapChainSupportDetails.presentModes.empty()) return false;

        return true;
    }


    // XXX interesting approach with erase()
    static bool checkDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice, const std::vector<const char*>& requiredDeviceExtensions) {
        // XXX: print on demand
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(
                requiredDeviceExtensions.cbegin(), requiredDeviceExtensions.cend());
        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }


    static std::vector<VkQueueFamilyProperties> queryQueueFamilies(VkPhysicalDevice physicalDevice) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
        return queueFamilies;
    }


    static QueueFamilyIndices pickQueueFamilyIndices(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
        QueueFamilyIndices indices;
        const auto queueFamilies = queryQueueFamilies(physicalDevice);
        for (unsigned int i = 0; i < queueFamilies.size(); i++) {
            const auto& queueFamily = queueFamilies[i];

            // Supports graphics?
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
            }

            // Supports presentation?
            // NOTE: very likely to be the same queue as Graphics.
            // Prefer to use the same queue for better performance.
            VkBool32 supportsPresentation = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportsPresentation);
            if (supportsPresentation) indices.presentFamily = i;

            if (indices.isComplete()) return { indices };
        }

        std::cerr << "::> Failed to find suitable queue families.\n";
        return indices;
    }


    static VkDevice createLogicalDevice(VkPhysicalDevice physicalDevice,
            const QueueFamilyIndices& queueFamilyIndices) {
        // Collect queues to be created
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {
            queueFamilyIndices.graphicsFamily.value(),
            queueFamilyIndices.presentFamily.value()
        };
        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // To be filled using PhysicalFeatures
        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        // VkDevice itself
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredDeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = std::data(requiredDeviceExtensions);
        // These fields are ignored by modern implementations:
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(requiredValidationLayers.size());
            createInfo.ppEnabledLayerNames = std::data(requiredValidationLayers);
        } else {
            createInfo.enabledLayerCount = 0;
        }

        VkDevice device;
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        return device;
    }
};

#endif
