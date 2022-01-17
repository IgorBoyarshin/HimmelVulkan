#include "HmlCommands.h"


std::unique_ptr<HmlCommands> HmlCommands::create(std::shared_ptr<HmlDevice> hmlDevice) noexcept {
    auto hmlCommands = std::make_unique<HmlCommands>();
    hmlCommands->hmlDevice = hmlDevice;

    hmlCommands->generalQueue = hmlDevice->graphicsQueue; // XXX queue type
    hmlCommands->generalQueueIndex = hmlDevice->queueFamilyIndices.graphicsFamily.value(); // XXX queue type
    hmlCommands->commandPoolGeneral = hmlCommands->createCommandPool(hmlCommands->generalQueueIndex,
        static_cast<VkCommandPoolCreateFlagBits>(0));
    if (!hmlCommands->commandPoolGeneral) return { nullptr };

    hmlCommands->onetimeQueue = hmlDevice->graphicsQueue; // XXX queue type
    hmlCommands->onetimeQueueIndex = hmlDevice->queueFamilyIndices.graphicsFamily.value(); // XXX queue type
    hmlCommands->commandPoolOnetime = hmlCommands->createCommandPool(hmlCommands->onetimeQueueIndex,
        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
    if (!hmlCommands->commandPoolOnetime) return { nullptr };

    hmlCommands->onetimeQueue = hmlDevice->graphicsQueue; // XXX queue type
    hmlCommands->onetimeFrameQueueIndex = hmlDevice->queueFamilyIndices.graphicsFamily.value(); // XXX queue type
    hmlCommands->commandPoolOnetimeFrames = hmlCommands->createCommandPool(hmlCommands->onetimeFrameQueueIndex,
        static_cast<VkCommandPoolCreateFlagBits>(VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT));
    if (!hmlCommands->commandPoolOnetimeFrames) return { nullptr };

    return hmlCommands;
}


HmlCommands::~HmlCommands() noexcept {
    std::cout << ":> Destroying HmlCommands...\n";
    vkDestroyCommandPool(hmlDevice->device, commandPoolOnetime, nullptr);
    vkDestroyCommandPool(hmlDevice->device, commandPoolGeneral, nullptr);
    vkDestroyCommandPool(hmlDevice->device, commandPoolOnetimeFrames, nullptr);
}


VkCommandPool HmlCommands::createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlagBits flags) noexcept {
    // A CommandPool is specific to each QueueFamily
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    // flags:
    // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: Hint that command buffers are
    // rerecorded with new commands very often (may change memory allocation behavior);
    // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: Allow command buffers to be
    // rerecorded individually, without this flag they all have to be reset together.
    poolInfo.flags = flags;

    VkCommandPool commandPool;
    if (vkCreateCommandPool(hmlDevice->device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cerr << "Failed to create CommandPool.\n";
        return VK_NULL_HANDLE;
    }

    return commandPool;
}


void HmlCommands::resetCommandPool(VkCommandPool commandPool) noexcept {
    vkResetCommandPool(hmlDevice->device, commandPool, 0);
}


std::vector<VkCommandBuffer> HmlCommands::allocatePrimary(size_t count, VkCommandPool commandPool) noexcept {
    std::vector<VkCommandBuffer> commandBuffers(count);

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(hmlDevice->device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "::> Failed to allocate CommandBuffers.\n";
        return {};
    }

    return commandBuffers;
}


std::vector<VkCommandBuffer> HmlCommands::allocateSecondary(size_t count, VkCommandPool commandPool) noexcept {
    std::vector<VkCommandBuffer> commandBuffers(count);

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

    if (vkAllocateCommandBuffers(hmlDevice->device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "::> Failed to allocate CommandBuffers.\n";
        return {};
    }

    return commandBuffers;
}


void HmlCommands::beginRecordingPrimaryOnetime(VkCommandBuffer commandBuffer) noexcept {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;

    // This call implicitly resets the buffer
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "::> Failed to begin CommandBuffer recording.\n";
        return;
    }
}


void HmlCommands::beginRecordingSecondaryOnetime(VkCommandBuffer commandBuffer,
        const VkCommandBufferInheritanceInfo* inheritanceInfo) noexcept {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
                    | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    beginInfo.pInheritanceInfo = inheritanceInfo;

    // This call implicitly resets the buffer
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "::> Failed to begin CommandBuffer recording.\n";
        return;
    }
}


void HmlCommands::endRecording(VkCommandBuffer commandBuffer) noexcept {
    // Errors of recording are reported only here
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "::> Failed to record CommandBuffer.\n";
        return;
    }
}


VkCommandBuffer HmlCommands::beginSingleTimeCommands() noexcept {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPoolOnetime;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(hmlDevice->device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}


void HmlCommands::endSingleTimeCommands(VkCommandBuffer commandBuffer) noexcept {
    // Errors of recording are reported only here
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        std::cerr << "::> There wa a problem recording a one-time command buffer.\n";
        return;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(onetimeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(onetimeQueue);
    // We also could have waited for completion using the vkWaitForFences and
    // specifying a fence as the last argument to the submit call. This could
    // be better if we had multiple transfers to wait on.

    vkFreeCommandBuffers(hmlDevice->device, commandPoolOnetime, 1, &commandBuffer);
}
