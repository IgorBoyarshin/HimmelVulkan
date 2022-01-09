#ifndef HML_RESOURCE_MANAGER
#define HML_RESOURCE_MANAGER

#include "HmlDevice.h"
#include "HmlCommands.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


// TODO
// Maybe store resources themselves inside a Renderer, so that when a Renderer
// is no londer needed the respective resources are freed.
//
// Or maybe store resources here, but upon the creation return a smart_ptr with
// the ID, so that when a renderer dies, its ptr dies, and the resources are freed
// automatically.
// TODO


struct HmlModelResource {
    using Id = uint32_t;
    Id id;

    uint32_t indicesCount;

    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer       indexBuffer;
    VkDeviceMemory indexBufferMemory;

    bool hasTexture = false;
    // TODO in future will be multiple textures (for materials)
    VkImage        textureImage;
    VkDeviceMemory textureImageMemory;
    VkImageView    textureImageView;
    // TODO move sampler to Renderer and make it shared
    VkSampler      textureSampler;

    HmlModelResource() : id(newId()) {}

    // static HmlModelResource create() {
    //     return HmlModelResource{};
    // }

    private:
    static Id newId() {
        static Id id = 0;
        return id++;
    }
};


struct HmlResourceManager {
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;

    // TODO make vectors of vectors
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    // NOTE We store the models ourselves as well (despite the fact that they
    // are shared_ptr) because there may be a time when there are no Entities
    // of this particular model at a given time, but there will be later.
    std::vector<std::shared_ptr<HmlModelResource>> models;


    static std::unique_ptr<HmlResourceManager> create(
            std::shared_ptr<HmlDevice> hmlDevice, std::shared_ptr<HmlCommands> hmlCommands) {
        auto hmlResourceManager = std::make_unique<HmlResourceManager>();
        hmlResourceManager->hmlDevice = hmlDevice;
        hmlResourceManager->hmlCommands = hmlCommands;
        return hmlResourceManager;
    }


    ~HmlResourceManager() {
        for (const auto& model : models) {
            if (model->hasTexture) {
                vkDestroySampler(hmlDevice->device, model->textureSampler, nullptr);
                vkDestroyImageView(hmlDevice->device, model->textureImageView, nullptr);
                vkDestroyImage(hmlDevice->device, model->textureImage, nullptr);
                vkFreeMemory(hmlDevice->device, model->textureImageMemory, nullptr);
            }

            vkDestroyBuffer(hmlDevice->device, model->indexBuffer, nullptr);
            vkFreeMemory(hmlDevice->device, model->indexBufferMemory, nullptr);
            vkDestroyBuffer(hmlDevice->device, model->vertexBuffer, nullptr);
            vkFreeMemory(hmlDevice->device, model->vertexBufferMemory, nullptr);
        }

        // TODO range
        for (size_t i = 0; i < uniformBuffers.size(); i++) {
            vkDestroyBuffer(hmlDevice->device, uniformBuffers[i], nullptr);
            vkFreeMemory(hmlDevice->device, uniformBuffersMemory[i], nullptr);
        }
    }


    void updateUniformBuffer(uint32_t uboIndex, const void* newData, size_t sizeBytes) {
        void* data;
        vkMapMemory(hmlDevice->device, uniformBuffersMemory[uboIndex], 0, sizeBytes, 0, &data);
            memcpy(data, newData, sizeBytes);
        vkUnmapMemory(hmlDevice->device, uniformBuffersMemory[uboIndex]);
    }


    // TODO return ID
    // TODO make vectors
    void newUniformBuffers(size_t count, size_t sizeBytes) {
        createUniformBuffers(uniformBuffers, uniformBuffersMemory, count, sizeBytes);
    }


    // TODO return ID
    // TODO make vectors
    // void newTexture(const char* fileName) {
    //     createTextureImage(textureImage, textureImageMemory, fileName);
    //     textureImageView = createTextureImageView(textureImage);
    //     textureSampler = createTextureSampler();
    // }


    // Model with color
    std::shared_ptr<HmlModelResource> newModel(const void* vertices, size_t verticesSizeBytes, const std::vector<uint32_t>& indices) {
        auto model = std::make_shared<HmlModelResource>();
        model->indicesCount = indices.size();
        model->hasTexture = false;

        createVertexBufferThroughStaging(model->vertexBuffer, model->vertexBufferMemory, vertices, verticesSizeBytes);
        createIndexBufferThroughStaging(model->indexBuffer, model->indexBufferMemory, indices);

        models.push_back(model);
        return model;
    }


    // Model with texture
    std::shared_ptr<HmlModelResource> newModel(const void* vertices, size_t verticesSizeBytes, const std::vector<uint32_t>& indices, const char* textureFileName) {
        auto model = std::make_shared<HmlModelResource>();
        model->indicesCount = indices.size();
        model->hasTexture = true;

        createVertexBufferThroughStaging(model->vertexBuffer, model->vertexBufferMemory, vertices, verticesSizeBytes);
        createIndexBufferThroughStaging(model->indexBuffer, model->indexBufferMemory, indices);

        createTextureImage(model->textureImage, model->textureImageMemory, textureFileName);
        model->textureImageView = createTextureImageView(model->textureImage);
        model->textureSampler = createTextureSampler();

        models.push_back(model);
        return model;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    // NOTE: unused
    // TODO remove
    void createVertexBufferHost(VkBuffer& vertexBuffer,
            VkDeviceMemory& vertexBufferMemory, const void* vertices, size_t sizeBytes) {
        createBuffer(static_cast<VkDeviceSize>(sizeBytes), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                vertexBuffer, vertexBufferMemory);
        // ======== Fill VertexBuffer
        // The driver may not immediately copy the data into the buffer memory,
        // for example because of caching. It is also possible that writes to
        // the buffer are not visible in the mapped memory yet.
        // There are two ways to deal with that problem:
        // 1) Use a memory heap that is host coherent,
        //    indicated with VK_MEMORY_PROPERTY_HOST_COHERENT_BIT.
        // 2) Call vkFlushMappedMemoryRanges after writing to the mapped memory,
        //    and call vkInvalidateMappedMemoryRanges before reading from the
        //    mapped memory.
        // We've gone with the first approach, which may lead to slight
        // performance loss.

        // Keep in mind that the following operation only makes the driver aware
        // of our writes to the buffer, but it does not mean the data is actually
        // visible on the GPU yet. The transfer of data to the GPU is an operation
        // that happens in the background and the specification simply tells us
        // that it is guaranteed to be complete as of the next call to vkQueueSubmit.
        void* data;
        vkMapMemory(hmlDevice->device, vertexBufferMemory, 0, static_cast<VkDeviceSize>(sizeBytes), 0, &data);
            memcpy(data, vertices, sizeBytes);
        vkUnmapMemory(hmlDevice->device, vertexBufferMemory);
    }


    // By using a staging buffer here we improve the overall performance because
    // this is a one-time operation.
    void createVertexBufferThroughStaging(VkBuffer& vertexBuffer,
            VkDeviceMemory& vertexBufferMemory, const void* vertices, size_t sizeBytes) {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(static_cast<VkDeviceSize>(sizeBytes), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(hmlDevice->device, stagingBufferMemory, 0, static_cast<VkDeviceSize>(sizeBytes), 0, &data);
            memcpy(data, vertices, sizeBytes);
        vkUnmapMemory(hmlDevice->device, stagingBufferMemory);

        createBuffer(static_cast<VkDeviceSize>(sizeBytes), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

        copyBuffer(stagingBuffer, vertexBuffer, static_cast<VkDeviceSize>(sizeBytes));

        vkDestroyBuffer(hmlDevice->device, stagingBuffer, nullptr);
        vkFreeMemory(hmlDevice->device, stagingBufferMemory, nullptr);
    }


    void createIndexBufferThroughStaging(VkBuffer& indexBuffer,
            VkDeviceMemory& indexBufferMemory, const std::vector<uint32_t>& indices) {
        size_t sizeBytes = sizeof(indices[0]) * indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(static_cast<VkDeviceSize>(sizeBytes), VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(hmlDevice->device, stagingBufferMemory, 0, static_cast<VkDeviceSize>(sizeBytes), 0, &data);
            memcpy(data, indices.data(), sizeBytes);
        vkUnmapMemory(hmlDevice->device, stagingBufferMemory);

        createBuffer(static_cast<VkDeviceSize>(sizeBytes), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(stagingBuffer, indexBuffer, static_cast<VkDeviceSize>(sizeBytes));

        vkDestroyBuffer(hmlDevice->device, stagingBuffer, nullptr);
        vkFreeMemory(hmlDevice->device, stagingBufferMemory, nullptr);
    }


    void createUniformBuffers(std::vector<VkBuffer>& uniformBuffers,
            std::vector<VkDeviceMemory>& uniformBuffersMemory, size_t count, size_t sizeBytes) {
        uniformBuffers.resize(count);
        uniformBuffersMemory.resize(count);

        for (size_t i = 0; i < count; i++) {
            // We'll be updating it every frame, so makes sense to store it in Host memory
            createBuffer(sizeBytes, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                uniformBuffers[i], uniformBuffersMemory[i]);
        }
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    void createTextureImage(VkImage& textureImage, VkDeviceMemory& textureImageMemory, const char* fileName) {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(fileName, &texWidth, &texHeight, &texChannels,
            STBI_rgb_alpha); // will force alpha even if it is not present
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            stagingBuffer, stagingBufferMemory);

        void* data;
        vkMapMemory(hmlDevice->device, stagingBufferMemory, 0, imageSize, 0, &data);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(hmlDevice->device, stagingBufferMemory);

        stbi_image_free(pixels);

        createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_TILING_OPTIMAL,
            // Is a destination; we want to access it from the shader
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            textureImage, textureImageMemory);

        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
            // UNDEFINED because that's what the image was created with.
            // Can do this because we don't care about the contents before the copy.
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, textureImage,
            static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(hmlDevice->device, stagingBuffer, nullptr);
        vkFreeMemory(hmlDevice->device, stagingBufferMemory, nullptr);
    }


    VkImageView createTextureImageView(VkImage textureImage) {
        return createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
    }


    VkSampler createTextureSampler() {
        // For anisotropic filtering
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(hmlDevice->physicalDevice, &properties);

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        // VK_SAMPLER_ADDRESS_MODE_REPEAT
        // VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
        // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
        // VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
        // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE; // FALSE to turn off, set max = 1.0f
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy; // use the best available
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE; // if TRUE => [0; width) instead of [0;1)
        samplerInfo.compareEnable = VK_FALSE; // used for percentage-closer filtering (shadow maps)
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        VkSampler textureSampler;
        if (vkCreateSampler(hmlDevice->device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
            std::cerr << "::> Failed to create TextureSampler.\n";
            return VK_NULL_HANDLE;
        }
        return textureSampler;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    // VkBufferUsageFlagBits:
    // VK_IMAGE_USAGE_TRANSFER_SRC_BIT
    // VK_IMAGE_USAGE_TRANSFER_DST_BIT
    // VK_IMAGE_USAGE_SAMPLED_BIT
    // VK_IMAGE_USAGE_STORAGE_BIT
    // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
    // VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
    // VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
    // VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
    void createImage(uint32_t width, uint32_t height, VkFormat format,
            VkImageTiling tiling, VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties, VkImage& image,
            VkDeviceMemory& imageMemory) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        // VK_IMAGE_TILING_LINEAR -- texels in row-major order
        // VK_IMAGE_TILING_OPTIMAL -- texels in implementation-defined order,
        //                            for efficient access from the shader
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        // The image will only be used by one queue family: the one that supports
        // graphics (and therefore also) transfer operations.
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.flags = 0; // for sparse images

        if (vkCreateImage(hmlDevice->device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(hmlDevice->device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(hmlDevice->device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(hmlDevice->device, image, imageMemory, 0);
    }


    VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D; // 1D, 2D, 3D or CubeMap
        viewInfo.format = format;
        // e.g. can map all of the channels to the red channel for a monochrome texture.
        // VK_COMPONENT_SWIZZLE_IDENTITY
        // VK_COMPONENT_SWIZZLE_ZERO
        // VK_COMPONENT_SWIZZLE_ONE
        // VK_COMPONENT_SWIZZLE_R
        // VK_COMPONENT_SWIZZLE_G
        // VK_COMPONENT_SWIZZLE_B
        // VK_COMPONENT_SWIZZLE_A
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY; // (defined as 0)
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY; // (defined as 0)
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY; // (defined as 0)
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY; // (defined as 0)
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VkImageView imageView;
        if (vkCreateImageView(hmlDevice->device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            std::cerr << "::> Failed to create a VkImageView.\n";
            return VK_NULL_HANDLE;
        }

        return imageView;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
            VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(hmlDevice->physicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }


    VkFormat findDepthFormat() {
        return findSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }


    void createDepthResources(VkImage& depthImage, VkDeviceMemory& depthImageMemory,
            VkImageView& depthImageView, VkExtent2D extent) {
        VkFormat depthFormat = findDepthFormat();
        createImage(extent.width, extent.height, depthFormat,
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
        depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
        transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }
    // ========================================================================
    // ========================================================================
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
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
        // ======== Create a Buffer
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        // Will only be used by one queue at a time (the GraphicsQueue):
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        bufferInfo.flags = 0; // default. To configure sparse buffer memory
        if (vkCreateBuffer(hmlDevice->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vertex buffer!");
        }
        // ======== Allocate memory for it
        // There is a limit to the amount of simultanious allocations (around 4096),
        // so in real applications one should use custom allocators and offsets
        // into a single large chunk of allocated memory.

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(hmlDevice->device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(hmlDevice->device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vertex buffer memory!");
        }
        // ======== Bind the allocated memory to the buffer
        // The last argument -- offset within the allocated region of memory.
        // Must be divisible by memRequirements.alignment.
        vkBindBufferMemory(hmlDevice->device, buffer, bufferMemory, 0);
    }
    // ========================================================================
    // ========================================================================
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

    // transitionBuffer({{ vertexBuffer,
    //     VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
    //     VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED }},
    //     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);

    // Before the barrier, we wait for stages in stagesBeforeBarrier, and once
    // those stages complete, we unblock the pipeline stages in stagesAfterBarrier
    void transitionBuffer(const std::vector<BufferTransition>& bufferTransitions,
            VkPipelineStageFlags stagesBeforeBarrier, VkPipelineStageFlags stagesAfterBarrier) {
        std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
        for (const auto& bufferTransition : bufferTransitions) {
            VkBufferMemoryBarrier bufferMemoryBarrier;
            bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
            bufferMemoryBarrier.pNext = nullptr;
            bufferMemoryBarrier.offset = 0;
            bufferMemoryBarrier.size = VK_WHOLE_SIZE;
            bufferMemoryBarrier.buffer =              bufferTransition.buffer;
            bufferMemoryBarrier.srcAccessMask =       bufferTransition.currentAccess;
            bufferMemoryBarrier.dstAccessMask =       bufferTransition.newAccess;
            bufferMemoryBarrier.srcQueueFamilyIndex = bufferTransition.currentQueueFamily;
            bufferMemoryBarrier.dstQueueFamilyIndex = bufferTransition.newQueueFamily;

            bufferMemoryBarriers.push_back(bufferMemoryBarrier);
        }

        VkCommandBuffer commandBuffer = hmlCommands->beginSingleTimeCommands();
        vkCmdPipelineBarrier(
            commandBuffer,
            stagesBeforeBarrier, stagesAfterBarrier,
            0, // or VK_DEPENDENCY_BY_REGION_BIT
            // (global) MemoryBarrier:
            0, nullptr,
            // BufferMemoryBarrier:
            static_cast<uint32_t>(bufferMemoryBarriers.size()), bufferMemoryBarriers.data(),
            // ImageMemoryBarrier:
            0, nullptr);
        hmlCommands->endSingleTimeCommands(commandBuffer);
    }


    static bool hasStencilComponent(VkFormat format) {
        return (format == VK_FORMAT_D32_SFLOAT_S8_UINT) || (format == VK_FORMAT_D24_UNORM_S8_UINT);
    }


    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        VkCommandBuffer commandBuffer = hmlCommands->beginSingleTimeCommands();

        // One of the most common ways to perform layout transitions is using
        // an image memory barrier. A pipeline barrier like that is generally
        // used to synchronize access to resources, like ensuring that a write
        // to a buffer completes before reading from it, but it can also be used
        // to transition image layouts and transfer queue family ownership when
        // VK_SHARING_MODE_EXCLUSIVE is used. There is an equivalent buffer
        // memory barrier to do this for buffers.
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        // Use VK_IMAGE_LAYOUT_UNDEFINED as oldLayout if you don't care about
        // the existing contents of the image.
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        // If you are using the barrier to transfer queue family ownership,
        // then these two fields should be the indices of the queue families.
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (hasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            // The writes don't have to wait on anything
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // no stage
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // no stage
            // The writing happens in VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT.
            // We need to pick the earliest; the reading happens in:
            destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        } else {
            throw std::invalid_argument("unsupported layout transition!");
        }

        // For example, if you're going to read from a uniform after the barrier,
        // you would specify a usage of VK_ACCESS_UNIFORM_READ_BIT and the
        // earliest shader that will read from the uniform as pipeline stage,
        // for example VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT.
        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, // in which pipeline stage the operations occur that should happen before the barrier
            destinationStage, // the pipeline stage in which operations will wait on the barrier
            0, // or VK_DEPENDENCY_BY_REGION_BIT:
            // (turns the barrier into a per-region condition.
            // That means that the implementation is allowed to already begin
            // reading from the parts of a resource that were written so far).
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        hmlCommands->endSingleTimeCommands(commandBuffer);
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        for (uint32_t i = 0; i < hmlDevice->memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && ((hmlDevice->memProperties.memoryTypes[i].propertyFlags & properties) == properties)) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = hmlCommands->beginSingleTimeCommands();

        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        hmlCommands->endSingleTimeCommands(commandBuffer);
    }


    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
        VkCommandBuffer commandBuffer = hmlCommands->beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };
        vkCmdCopyBufferToImage(
            commandBuffer,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );

        hmlCommands->endSingleTimeCommands(commandBuffer);
    }
};

#endif
