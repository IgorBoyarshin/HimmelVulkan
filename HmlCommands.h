#ifndef HML_COMMANDS
#define HML_COMMANDS

#include <iostream>
#include <memory>
#include <vector>

#include "HmlDevice.h"


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


    static std::unique_ptr<HmlCommands> create(std::shared_ptr<HmlDevice> hmlDevice) noexcept;
    ~HmlCommands() noexcept;
    VkCommandPool createCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlagBits flags) noexcept;
    void resetCommandPool(VkCommandPool commandPool) noexcept;
    std::vector<VkCommandBuffer> allocatePrimary(size_t count, VkCommandPool commandPool) noexcept;
    std::vector<VkCommandBuffer> allocateSecondary(size_t count, VkCommandPool commandPool) noexcept;
    void beginRecordingPrimaryOnetime(VkCommandBuffer commandBuffer) noexcept;
    void beginRecordingSecondaryOnetime(VkCommandBuffer commandBuffer,
            const VkCommandBufferInheritanceInfo* inheritanceInfo) noexcept;
    void beginRecordingSecondary(VkCommandBuffer commandBuffer,
            const VkCommandBufferInheritanceInfo* inheritanceInfo) noexcept;
    void endRecording(VkCommandBuffer commandBuffer) noexcept;
    VkCommandBuffer beginSingleTimeCommands() noexcept;
    void endSingleTimeCommands(VkCommandBuffer commandBuffer) noexcept;
};

#endif
