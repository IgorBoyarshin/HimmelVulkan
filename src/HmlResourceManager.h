#ifndef HML_RESOURCE_MANAGER
#define HML_RESOURCE_MANAGER

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_set>


#include "settings.h"
#include "HmlDevice.h"
#include "HmlCommands.h"

#include "../libs/stb_image.h"


struct HmlImageResource {
    enum class Type {
        BLANK, TEXTURE, RENDER_TARGET, DEPTH, SHADOW_MAP, SWAPCHAIN_IMAGE
    } type;

    std::shared_ptr<HmlDevice> hmlDevice;

    VkImage        image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView    view = VK_NULL_HANDLE;
    VkFormat       format = VK_FORMAT_UNDEFINED;
    VkImageLayout  layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkSampler      sampler = VK_NULL_HANDLE; // NOTE only applicable to textures
    // TODO find a way to reuse sampler across textures

    uint32_t width = 0;
    uint32_t height = 0;

    bool blockingTransitionLayoutTo(VkImageLayout newLayout, std::shared_ptr<HmlCommands> hmlCommands) noexcept;
    bool transitionLayoutTo(VkImageLayout newLayout, VkCommandBuffer commandBuffer) noexcept;
    bool hasStencilComponent() const noexcept;

    ~HmlImageResource() noexcept;
};


struct HmlBuffer {
    enum class Type {
        STAGING, UNIFORM, STORAGE, VERTEX, INDEX
    } type;

    struct Pack {
        void* vertices;
        VkDeviceSize sizeBytes;
        inline Pack(void* vertices, VkDeviceSize sizeBytes) : vertices(vertices), sizeBytes(sizeBytes) {}
    };

    std::shared_ptr<HmlDevice> hmlDevice;

    VkDeviceSize   sizeBytes;
    VkBuffer       buffer;
    VkDeviceMemory memory;
    void* mappedPtr = nullptr;

    bool map() noexcept;
    bool unmap() noexcept;
    bool update(const void* newData) noexcept;
    bool update(const void* newData, VkDeviceSize customUpdateSizeBytes) noexcept;
    HmlBuffer(bool mappable) noexcept;
    ~HmlBuffer() noexcept;

    private:
    bool mappable = false;
};


struct HmlModelResource {
    using Id = uint32_t;
    Id id;

    std::shared_ptr<HmlDevice> hmlDevice;

    uint32_t indicesCount;

    std::unique_ptr<HmlBuffer> vertexBuffer;
    std::unique_ptr<HmlBuffer> indexBuffer;


    std::unique_ptr<HmlImageResource> textureResource;

    inline HmlModelResource() noexcept : id(newId()) {}

    ~HmlModelResource() noexcept;

    private:
    static Id newId() noexcept;
};


struct HmlResourceManager {
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;

    // NOTE We store the models ourselves as well (despite the fact that they
    // are shared_ptr) because there may be a time when there are no Entities
    // of this particular model at a given time, but there will be later.
    // NOTE NOTE Actually, the above is false. If there are no other Entities of
    // this particular Model left, then it cannot be accessed and therefore used
    // again (so the only way is to recreate it). So the only reason to store
    // the models is to eventually implement a system to access Models from here
    // in the first place (not just store them here as well).
    std::vector<std::shared_ptr<HmlModelResource>> models;
    // ========================================================================
    struct ReleaseData {
        std::unique_ptr<HmlBuffer> hmlBuffer;
        // After this frame the HmlBuffer can be safely deleted
        uint32_t lastAliveFrame;

        inline auto operator<=>(const ReleaseData&) const = default;
    };

    struct ReleaseDataHasher {
        size_t operator()(const ReleaseData& releaseData) const;
    };

    std::unordered_set<ReleaseData, ReleaseDataHasher> releaseQueue;

    void markForRelease(std::unique_ptr<HmlBuffer>&& hmlBuffer, uint32_t currentFrame) noexcept;
    // NOTE Must only be called by the main game loop to advance release logic by one frame
    void tickFrame(uint32_t currentFrame) noexcept;
    // ========================================================================
    std::unique_ptr<HmlImageResource> dummyTextureResource;

    std::unique_ptr<HmlImageResource> newDummyTextureResource() noexcept;

    static std::unique_ptr<HmlResourceManager> create(
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands) noexcept;
    ~HmlResourceManager() noexcept;
    // TODO consistency between new and create
    std::unique_ptr<HmlBuffer> createStagingBuffer(VkDeviceSize sizeBytes) const noexcept;
    std::unique_ptr<HmlBuffer> createUniformBuffer(VkDeviceSize sizeBytes) const noexcept;
    std::unique_ptr<HmlBuffer> createStorageBuffer(VkDeviceSize sizeBytes) const noexcept;
    std::unique_ptr<HmlBuffer> createVertexBuffer(VkDeviceSize sizeBytes) const noexcept;
    std::unique_ptr<HmlBuffer> createIndexBuffer(VkDeviceSize sizeBytes) const noexcept;
    std::unique_ptr<HmlBuffer> createVertexBufferWithData(const void* data, VkDeviceSize sizeBytes) const noexcept;
    std::unique_ptr<HmlBuffer> createIndexBufferWithData(const void* data, VkDeviceSize sizeBytes) const noexcept;
    std::unique_ptr<HmlImageResource> newShadowResource(VkExtent2D extent, VkFormat format) noexcept;
    std::unique_ptr<HmlImageResource> newRenderTargetImageResource(VkExtent2D extent, VkFormat format) noexcept;
    std::unique_ptr<HmlImageResource> newTextureResourceFromData(uint32_t width, uint32_t height, uint32_t componentsCount, unsigned char* data, VkFormat format, std::optional<VkFilter> filter) noexcept;
    std::unique_ptr<HmlImageResource> newTextureResource(const char* fileName, uint32_t componentsCount, VkFormat format, VkFilter filter) noexcept;
    std::unique_ptr<HmlImageResource> newImageResource(VkExtent2D extent) noexcept;
    std::unique_ptr<HmlImageResource> newDepthResource(VkExtent2D extent) noexcept;
    std::vector<std::shared_ptr<HmlImageResource>> wrapSwapchainImagesIntoImageResources(
        std::vector<VkImage>&& images, VkFormat format, VkExtent2D extent) noexcept;
    std::unique_ptr<HmlImageResource> newBlankImageResource(
        VkExtent2D extent, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlagBits aspect) noexcept;
    // Model with color
    std::shared_ptr<HmlModelResource> newModel(const void* vertices, size_t verticesSizeBytes, const std::vector<uint32_t>& indices) noexcept;
    // Model with texture
    std::shared_ptr<HmlModelResource> newModel(const void* vertices, size_t verticesSizeBytes, const std::vector<uint32_t>& indices, const char* textureFileName, VkFilter filter) noexcept;
    // ========================================================================
    // NOTE: unused
    // TODO remove
    void createVertexBufferHost(VkBuffer& vertexBuffer,
            VkDeviceMemory& vertexBufferMemory, const void* vertices, VkDeviceSize sizeBytes) noexcept;
    // ========================================================================
    VkSampler createTextureSamplerForFonts() noexcept;
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
        VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const noexcept;
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
    // struct BufferTransition {
    //     VkBuffer buffer;
    //     VkAccessFlags currentAccess;
    //     VkAccessFlags newAccess;
    //     uint32_t currentQueueFamily;
    //     uint32_t newQueueFamily;
    // };

    // Before the barrier, we wait for stages in stagesBeforeBarrier, and once
    // those stages complete, we unblock the pipeline stages in stagesAfterBarrier
    // void transitionBuffer(const std::vector<BufferTransition>& bufferTransitions,
    //     VkPipelineStageFlags stagesBeforeBarrier, VkPipelineStageFlags stagesAfterBarrier) noexcept;
    static bool hasStencilComponent(VkFormat format) noexcept;
    // ========================================================================
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept;
    // ========================================================================
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const noexcept;
    void copyBufferToImage(VkBuffer buffer, VkImage image, VkExtent2D extent) const noexcept;
};

#endif
