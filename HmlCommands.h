#ifndef HML_COMMANDS
#define HML_COMMANDS


struct HmlCommands {
    // Reasons to use a command pool:
    // 1) There may be short-lived command buffers, and creation/destruction of
    //    command buffers is expensive.
    // 2) Command buffers use shared (CPU & GPU) memory, which can be mapped only
    //    in large granularities. That means that small command buffers may waste
    //    memory.
    // 3) Memory mapping is expensive because it involves modifying page table
    //    and TLB cache invalidation. So it is better to map a large command pool
    //    once and allocate smaller command buffers within it.


    // TODO create a separate pool for Graphics and Presentation
    VkCommandPool commandPoolGeneral;
    // TODO do not recreate a one-time command each time, just rerecord it
    VkCommandPool commandPoolOnetime;
    VkCommandPool commandPoolOnetimeFrames;
    VkQueue generalQueue;
    VkQueue onetimeQueue;
    uint32_t generalQueueIndex;
    uint32_t onetimeQueueIndex;
    uint32_t onetimeFrameQueueIndex;

    // Cleaned automatically upon CommandPool destruction
    // std::vector<VkCommandBuffer> commandBuffers;

    std::shared_ptr<HmlDevice> hmlDevice;



    static std::unique_ptr<HmlCommands> create(std::shared_ptr<HmlDevice> hmlDevice) {
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


    ~HmlCommands() {
        std::cout << ":> Destroying HmlCommands...\n";
        vkDestroyCommandPool(hmlDevice->device, commandPoolOnetime, nullptr);
        vkDestroyCommandPool(hmlDevice->device, commandPoolGeneral, nullptr);
        vkDestroyCommandPool(hmlDevice->device, commandPoolOnetimeFrames, nullptr);
    }


    VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlagBits flags) {
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


    void resetGeneralCommandPool() {
        vkResetCommandPool(hmlDevice->device, commandPoolGeneral, 0);
    }


    std::vector<VkCommandBuffer> allocate(size_t count, VkCommandPool commandPool) {
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


    void beginRecording(VkCommandBuffer commandBuffer, VkCommandBufferUsageFlags flags) {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        // flags:
        // VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: The command buffer will
        // be rerecorded right after executing it once;
        // VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: This is a secondary
        // command buffer that will be entirely within a single render pass;
        // VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: The command buffer can
        // be resubmitted while it is also already pending execution.
        beginInfo.flags = flags;
        beginInfo.pInheritanceInfo = nullptr; // Optional

        // This call implicitly resets the buffer
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            std::cerr << "::> Failed to begin CommandBuffer recording.\n";
            return;
        }
    }


    void endRecording(VkCommandBuffer commandBuffer) {
        // Errors of recording are reported only here
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            std::cerr << "::> Failed to record CommandBuffer.\n";
            return;
        }
    }


    VkCommandBuffer beginSingleTimeCommands() {
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


    void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
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
    // ========================================================================
    // ========================================================================
    // ========================================================================
};

#endif
