#ifndef HML_RESOURCE_MANAGER
#define HML_RESOURCE_MANAGER

#include <memory>
#include <vector>
#include <iostream>

#include "HmlDevice.h"
#include "HmlCommands.h"




struct HmlTextureResource {
    std::shared_ptr<HmlDevice> hmlDevice;

    VkImage        image;
    VkDeviceMemory imageMemory;
    VkImageView    imageView;
    // TODO move sampler to Renderer and make it shared
    VkSampler      sampler;

    ~HmlTextureResource() noexcept;
};


struct HmlModelResource {
    using Id = uint32_t;
    Id id;

    std::shared_ptr<HmlDevice> hmlDevice;

    uint32_t indicesCount;

    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer       indexBuffer;
    VkDeviceMemory indexBufferMemory;


    std::unique_ptr<HmlTextureResource> textureResource;

    inline HmlModelResource() noexcept : id(newId()) {}

    ~HmlModelResource() noexcept;

    private:
    static Id newId() noexcept;
};


struct HmlUniformBuffer {
    std::shared_ptr<HmlDevice> hmlDevice;

    VkDeviceSize sizeBytes;
    VkBuffer       uniformBuffer;
    VkDeviceMemory uniformBufferMemory;
    void* mappedPtr = nullptr;

    void map() noexcept;
    void unmap() noexcept;
    void update(void* newData) noexcept;
    ~HmlUniformBuffer() noexcept;
};


struct HmlStorageBuffer {
    std::shared_ptr<HmlDevice> hmlDevice;

    VkDeviceSize sizeBytes;
    VkBuffer       storageBuffer;
    VkDeviceMemory storageBufferMemory;
    void* mappedPtr = nullptr;

    void map() noexcept;
    void unmap() noexcept;
    void update(void* newData) noexcept;
    ~HmlStorageBuffer() noexcept;
};


struct HmlResourceManager {
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;

    // NOTE We store the models ourselves as well (despite the fact that they
    // are shared_ptr) because there may be a time when there are no Entities
    // of this particular model at a given time, but there will be later.
    std::vector<std::shared_ptr<HmlModelResource>> models;


    static std::unique_ptr<HmlResourceManager> create(
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands) noexcept;
    ~HmlResourceManager() noexcept;
    std::unique_ptr<HmlUniformBuffer> createUniformBuffer(VkDeviceSize sizeBytes) noexcept;
    // TODO add options for VERTEX_INPUT etc.
    std::unique_ptr<HmlStorageBuffer> createStorageBuffer(VkDeviceSize sizeBytes) noexcept;
    std::unique_ptr<HmlTextureResource> newTextureResource(const char* fileName, VkFilter filter) noexcept;
    // Model with color
    std::shared_ptr<HmlModelResource> newModel(const void* vertices, size_t verticesSizeBytes, const std::vector<uint32_t>& indices) noexcept;
    // Model with texture
    std::shared_ptr<HmlModelResource> newModel(const void* vertices, size_t verticesSizeBytes, const std::vector<uint32_t>& indices, const char* textureFileName, VkFilter filter) noexcept;
    // ========================================================================
    // NOTE: unused
    // TODO remove
    void createVertexBufferHost(VkBuffer& vertexBuffer,
            VkDeviceMemory& vertexBufferMemory, const void* vertices, size_t sizeBytes) noexcept;

    void createVertexBufferThroughStaging(VkBuffer& vertexBuffer,
            VkDeviceMemory& vertexBufferMemory, const void* vertices, size_t sizeBytes) noexcept;
    void createIndexBufferThroughStaging(VkBuffer& indexBuffer,
            VkDeviceMemory& indexBufferMemory, const std::vector<uint32_t>& indices) noexcept;
    // ========================================================================
    bool createTextureImage(VkImage& textureImage, VkDeviceMemory& textureImageMemory, const char* fileName) noexcept;
    VkImageView createTextureImageView(VkImage textureImage) noexcept;
    VkSampler createTextureSampler(VkFilter magFilter, VkFilter minFilter) noexcept;
    // ========================================================================
    bool createImage(uint32_t width, uint32_t height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties, VkImage& image,
            VkDeviceMemory& imageMemory) noexcept;
    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) noexcept;
    // ========================================================================
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
            VkImageTiling tiling, VkFormatFeatureFlags features) const noexcept;
    VkFormat findDepthFormat() const noexcept;
    bool createDepthResources(VkImage& depthImage, VkDeviceMemory& depthImageMemory,
            VkImageView& depthImageView, VkExtent2D extent) noexcept;
    // ========================================================================
    // TODO add device, physicalDevice, memProperties
    // VkBufferUsageFlagBits:
    // VK_BUFFER_USAGE_TRANSFER_SRC_BIT
    // VK_BUFFER_USAGE_TRANSFER_DST_BIT
    // VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
    // VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT
    // VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT
    // VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
    // VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    // VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    // VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) noexcept;
    // ========================================================================
    // VkAccessFlags:
    // VK_ACCESS_INDIRECT_COMMAND_READ_BIT
    // VK_ACCESS_INDEX_READ_BIT
    // VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
    // VK_ACCESS_UNIFORM_READ_BIT
    // VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
    // VK_ACCESS_SHADER_READ_BIT
    // VK_ACCESS_SHADER_WRITE_BIT
    // VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
    // VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    // VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
    // VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    // VK_ACCESS_TRANSFER_READ_BIT
    // VK_ACCESS_TRANSFER_WRITE_BIT
    // VK_ACCESS_HOST_READ_BIT
    // VK_ACCESS_HOST_WRITE_BIT
    // VK_ACCESS_MEMORY_READ_BIT
    // VK_ACCESS_MEMORY_WRITE_BIT
    // etc...

    // Use VK_QUEUE_FAMILY_IGNORED if the family does not change
    struct BufferTransition {
        VkBuffer buffer;
        VkAccessFlags currentAccess;
        VkAccessFlags newAccess;
        uint32_t currentQueueFamily;
        uint32_t newQueueFamily;
    };

    // Before the barrier, we wait for stages in stagesBeforeBarrier, and once
    // those stages complete, we unblock the pipeline stages in stagesAfterBarrier
    void transitionBuffer(const std::vector<BufferTransition>& bufferTransitions,
            VkPipelineStageFlags stagesBeforeBarrier, VkPipelineStageFlags stagesAfterBarrier) noexcept;
    static bool hasStencilComponent(VkFormat format) noexcept;
    bool transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) noexcept;
    // ========================================================================
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept;
    // ========================================================================
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) noexcept;
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) noexcept;
};

#endif
