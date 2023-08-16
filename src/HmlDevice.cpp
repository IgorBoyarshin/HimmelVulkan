#include "HmlDevice.h"


std::unique_ptr<HmlDevice> HmlDevice::create(std::shared_ptr<HmlWindow> hmlWindow) noexcept {
    auto hmlDevice = std::make_unique<HmlDevice>();

    if (const auto obj = createInstance(hmlWindow->name); obj) {
        hmlDevice->instance = obj;
    } else return { nullptr };

    if (!loadVulkanFunctions(hmlDevice->instance)) return { nullptr };

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


HmlDevice::~HmlDevice() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlDevice...\n";
#endif
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    if (enableValidationLayers) {
        vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
    }
    vkDestroyInstance(instance, nullptr);
}
// ========================================================================
// ========================================================================
// ========================================================================
// Must be queried each time at recreation because swapChainExtent could be
// restricted by the window manager to match the actual OS window.
HmlDevice::SwapChainSupportDetails HmlDevice::querySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) noexcept {
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


VkInstance HmlDevice::createInstance(const char* applicationName) noexcept {
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
    appInfo.apiVersion = VK_API_VERSION_1_1;

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


bool HmlDevice::checkValidationLayerSupport(std::span<const char* const> requested) noexcept {
    const auto available = getAvailableLayers();
    return std::all_of(requested.begin(), requested.end(), [&](const char* layerName){
        return std::any_of(available.cbegin(), available.cend(), [&](const auto& layerProperties){
            return strcmp(layerName, layerProperties.layerName) == 0;
        });
    });
}


std::vector<VkLayerProperties> HmlDevice::getAvailableLayers() noexcept {
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


std::vector<const char*> HmlDevice::getRequiredExtensions(bool enableValidationLayers) noexcept {
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
VkSurfaceKHR HmlDevice::createSurface(VkInstance instance, GLFWwindow* window) noexcept {
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
VkDebugUtilsMessengerEXT HmlDevice::createDebugMessanger(const VkInstance& instance) noexcept {
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
    if (vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
        std::cerr << "::> Failed to set up debug messenger.\n";
        return VK_NULL_HANDLE;
    }
    return debugMessenger;
}


// TODO save all of them into a log and print (or don't print) at controlled
// point in time rather than directly into std::err(cout) at random time.
VKAPI_ATTR VkBool32 VKAPI_CALL HmlDevice::debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) noexcept {

    // To get rid of numerous "Device Extension" infos
    if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) return VK_FALSE;

    static int counter = 0;
    // std::cerr << "(validation layer) ";
    std::cerr << "(" << counter++ << ")  ";

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
    std::cout << '\n';

    // if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    return VK_FALSE;
}
// ========================================================================
// ========================================================================
// ========================================================================
VkPhysicalDevice HmlDevice::pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) noexcept {
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


void HmlDevice::describePhysicalDevice(VkPhysicalDevice physicalDevice) noexcept {
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
bool HmlDevice::isPhysicalDeviceSuitable(const VkPhysicalDevice& physicalDevice, const VkSurfaceKHR& surface) noexcept {
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


// NOTE interesting approach with erase()
bool HmlDevice::checkDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice, std::span<const char* const> requiredDeviceExtensions) noexcept {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}


std::vector<VkQueueFamilyProperties> HmlDevice::queryQueueFamilies(VkPhysicalDevice physicalDevice) noexcept {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
    return queueFamilies;
}


HmlDevice::QueueFamilyIndices HmlDevice::pickQueueFamilyIndices(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) noexcept {
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


VkDevice HmlDevice::createLogicalDevice(VkPhysicalDevice physicalDevice,
        const QueueFamilyIndices& queueFamilyIndices) noexcept {
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
    deviceFeatures.fillModeNonSolid = VK_TRUE;
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.tessellationShader = VK_TRUE;
    deviceFeatures.geometryShader = VK_TRUE;
    deviceFeatures.wideLines = VK_TRUE;

    // Shader draw parameters
    VkPhysicalDeviceShaderDrawParametersFeatures shaderDrawParameterFeatures = {};
    shaderDrawParameterFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS_FEATURES;
    shaderDrawParameterFeatures.shaderDrawParameters = VK_TRUE;

    // VkDevice itself
    VkDeviceCreateInfo createInfo = {};
    createInfo.pNext = &shaderDrawParameterFeatures;
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
        std::cerr << "::> Failed to create logical device.\n";
        return VK_NULL_HANDLE;
    }

    return device;
}
// ========================================================================
// ========================================================================
// ========================================================================
std::unordered_map<const char*, PFN_vkVoidFunction> vulkanFunctionsDispatchTable;


// NOTE we could get rid of this function entirely and just load the pointer
// on demand into static cached variables in each respective function.
bool HmlDevice::loadVulkanFunctions(VkInstance instance) {
    std::vector<const char*> functionNames = {
        "vkCreateDebugUtilsMessengerEXT",
        "vkDestroyDebugUtilsMessengerEXT",
        "vkSubmitDebugUtilsMessageEXT",

        "vkCmdBeginDebugUtilsLabelEXT",
        "vkCmdInsertDebugUtilsLabelEXT",
        "vkCmdEndDebugUtilsLabelEXT",

        "vkQueueBeginDebugUtilsLabelEXT",
        "vkQueueInsertDebugUtilsLabelEXT",
        "vkQueueEndDebugUtilsLabelEXT",
    };

    for (const char* functionName : functionNames) {
        PFN_vkVoidFunction fp = vkGetInstanceProcAddr(instance, functionName);
        if (!fp) { // check shouldn't be necessary (based on spec)
            std::cerr << "::> Failed to load " << functionName << ".\n";
            // TODO maybe have separate lists for "required" and "nice to have" extensions
            return false;
        }
        vulkanFunctionsDispatchTable[functionName] = fp;
    }

    return true;
}
// ========================================================================
VKAPI_ATTR VkResult VKAPI_CALL vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pMessenger
) {
    static PFN_vkCreateDebugUtilsMessengerEXT cachedPtr = nullptr;
    if (!cachedPtr) [[unlikely]] {
        cachedPtr = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vulkanFunctionsDispatchTable.at("vkCreateDebugUtilsMessengerEXT"));
    }
    return cachedPtr(instance, pCreateInfo, pAllocator, pMessenger);
}

VKAPI_ATTR void VKAPI_CALL vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT messenger,
    const VkAllocationCallbacks* pAllocator
) {
    static PFN_vkDestroyDebugUtilsMessengerEXT cachedPtr = nullptr;
    if (!cachedPtr) [[unlikely]] {
        cachedPtr = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vulkanFunctionsDispatchTable.at("vkDestroyDebugUtilsMessengerEXT"));
    }
    return cachedPtr(instance, messenger, pAllocator);
}

VKAPI_ATTR void VKAPI_CALL vkSubmitDebugUtilsMessageEXT(
    VkInstance instance,
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData
) {
    static PFN_vkSubmitDebugUtilsMessageEXT cachedPtr = nullptr;
    if (!cachedPtr) [[unlikely]] {
        cachedPtr = reinterpret_cast<PFN_vkSubmitDebugUtilsMessageEXT>(
            vulkanFunctionsDispatchTable.at("vkSubmitDebugUtilsMessageEXT"));
    }
    return cachedPtr(instance, messageSeverity, messageTypes, pCallbackData);
}
// ========================================================================
VKAPI_ATTR void VKAPI_CALL vkQueueBeginDebugUtilsLabelEXT(
    VkQueue queue,
    const VkDebugUtilsLabelEXT* pLabelInfo
) {
    static PFN_vkQueueBeginDebugUtilsLabelEXT cachedPtr = nullptr;
    if (!cachedPtr) [[unlikely]] {
        cachedPtr = reinterpret_cast<PFN_vkQueueBeginDebugUtilsLabelEXT>(
            vulkanFunctionsDispatchTable.at("vkQueueBeginDebugUtilsLabelEXT"));
    }
    return cachedPtr(queue, pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL vkQueueInsertDebugUtilsLabelEXT(
    VkQueue queue,
    const VkDebugUtilsLabelEXT* pLabelInfo
) {
    static PFN_vkQueueInsertDebugUtilsLabelEXT cachedPtr = nullptr;
    if (!cachedPtr) [[unlikely]] {
        cachedPtr = reinterpret_cast<PFN_vkQueueInsertDebugUtilsLabelEXT>(
            vulkanFunctionsDispatchTable.at("vkQueueInsertDebugUtilsLabelEXT"));
    }
    return cachedPtr(queue, pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL vkQueueEndDebugUtilsLabelEXT(
    VkQueue queue
) {
    static PFN_vkQueueEndDebugUtilsLabelEXT cachedPtr = nullptr;
    if (!cachedPtr) [[unlikely]] {
        cachedPtr = reinterpret_cast<PFN_vkQueueEndDebugUtilsLabelEXT>(
            vulkanFunctionsDispatchTable.at("vkQueueEndDebugUtilsLabelEXT"));
    }
    return cachedPtr(queue);
}
// ========================================================================
VKAPI_ATTR void VKAPI_CALL vkCmdBeginDebugUtilsLabelEXT(
    VkCommandBuffer commandBuffer,
    const VkDebugUtilsLabelEXT* pLabelInfo
) {
    static PFN_vkCmdBeginDebugUtilsLabelEXT cachedPtr = nullptr;
    if (!cachedPtr) [[unlikely]] {
        cachedPtr = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vulkanFunctionsDispatchTable.at("vkCmdBeginDebugUtilsLabelEXT"));
    }
    return cachedPtr(commandBuffer, pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL vkCmdInsertDebugUtilsLabelEXT(
    VkCommandBuffer commandBuffer,
    const VkDebugUtilsLabelEXT* pLabelInfo
) {
    static PFN_vkCmdInsertDebugUtilsLabelEXT cachedPtr = nullptr;
    if (!cachedPtr) [[unlikely]] {
        cachedPtr = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(
            vulkanFunctionsDispatchTable.at("vkCmdInsertDebugUtilsLabelEXT"));
    }
    return cachedPtr(commandBuffer, pLabelInfo);
}

VKAPI_ATTR void VKAPI_CALL vkCmdEndDebugUtilsLabelEXT(
    VkCommandBuffer commandBuffer
) {
    static PFN_vkCmdEndDebugUtilsLabelEXT cachedPtr = nullptr;
    if (!cachedPtr) [[unlikely]] {
        cachedPtr = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vulkanFunctionsDispatchTable.at("vkCmdEndDebugUtilsLabelEXT"));
    }
    return cachedPtr(commandBuffer);
}
