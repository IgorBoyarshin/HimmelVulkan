#include "HmlResourceManager.h"


HmlImageResource::~HmlImageResource() noexcept {
#if LOG_DESTROYS
    switch (type) {
        case Type::BLANK:           std::cout << ":> Destroying HmlImageResource (blank).\n"; break;
        case Type::TEXTURE:         std::cout << ":> Destroying HmlImageResource (texture).\n"; break;
        case Type::RENDER_TARGET:   std::cout << ":> Destroying HmlImageResource (render target).\n"; break;
        case Type::DEPTH:           std::cout << ":> Destroying HmlImageResource (depth).\n"; break;
        case Type::SHADOW_MAP:      std::cout << ":> Destroying HmlImageResource (shadow map).\n"; break;
        case Type::SWAPCHAIN_IMAGE: std::cout << ":> Destroying HmlImageResource (swapchain image).\n"; break;
    }
#endif

    vkDestroyImageView(hmlDevice->device, view, nullptr);

    // We do not own any of the underlying resources, so don't destroy them
    if (type == Type::SWAPCHAIN_IMAGE) return;

    if (sampler) vkDestroySampler(hmlDevice->device, sampler, nullptr);
    vkDestroyImage(hmlDevice->device, image, nullptr);
    vkFreeMemory(hmlDevice->device, memory, nullptr);
}
// ============================================================================
// ============================================================================
// ============================================================================
HmlModelResource::~HmlModelResource() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlModelResource.\n";
#endif
}


HmlModelResource::Id HmlModelResource::newId() noexcept {
    static Id id = 0;
    return id++;
}
// ============================================================================
// ============================================================================
// ============================================================================
bool HmlBuffer::map() noexcept {
    if (!mappable) {
        std::cerr << "::> HmlBuffer: trying to map an unmappable buffer.\n";
        return false;
    }
    if (!mappedPtr) vkMapMemory(hmlDevice->device, memory, 0, sizeBytes, 0, &mappedPtr);
    return true;
}


bool HmlBuffer::unmap() noexcept {
    // NOTE we silently ignore if the buffer is unmappable to make calling unmap always safe
    if (!mappable) return false;
    if (mappedPtr) vkUnmapMemory(hmlDevice->device, memory);
    mappedPtr = nullptr;
    return true;
}


bool HmlBuffer::update(const void* newData) noexcept {
    if (!mappable) {
        std::cerr << "::> HmlBuffer: trying to update an unmappable buffer.\n";
        return false;
    }
    if (!mappedPtr) {
        std::cerr << "::> HmlBuffer: trying to update a buffer that has not been mapped.\n";
        return false;
    }
    memcpy(mappedPtr, newData, sizeBytes);
    return true;
}


bool HmlBuffer::update(const void* newData, VkDeviceSize customUpdateSizeBytes) noexcept {
    if (!mappable) {
        std::cerr << "::> HmlBuffer: trying to update an unmappable buffer.\n";
        return false;
    }
    if (customUpdateSizeBytes > sizeBytes) {
        std::cerr << "::> HmlBuffer: trying to update with size larger than allocated buffer.\n";
        return false;
    }
    if (!mappedPtr) {
        std::cerr << "::> HmlBuffer: trying to update a buffer that has not been mapped.\n";
        return false;
    }
    memcpy(mappedPtr, newData, customUpdateSizeBytes);
    return true;
}


HmlBuffer::HmlBuffer(bool mappable) noexcept : mappable(mappable) {}


HmlBuffer::~HmlBuffer() noexcept {
#if LOG_DESTROYS
    switch (type) {
        case Type::STAGING: std::cout << ":> Destroying HmlBuffer (staging).\n"; break;
        case Type::UNIFORM: std::cout << ":> Destroying HmlBuffer (uniform).\n"; break;
        case Type::STORAGE: std::cout << ":> Destroying HmlBuffer (storage).\n"; break;
        case Type::VERTEX: std::cout << ":> Destroying HmlBuffer (vertex).\n"; break;
        case Type::INDEX: std::cout << ":> Destroying HmlBuffer (index).\n"; break;
    }
#endif

    unmap();
    vkDestroyBuffer(hmlDevice->device, buffer, nullptr);
    vkFreeMemory(hmlDevice->device, memory, nullptr);
}
// ============================================================================
// ============================================================================
// ============================================================================
size_t HmlResourceManager::ReleaseDataHasher::operator()(const ReleaseData& releaseData) const {
    return std::hash<std::unique_ptr<HmlBuffer>>()(releaseData.hmlBuffer);
}


void HmlResourceManager::markForRelease(std::unique_ptr<HmlBuffer>&& hmlBuffer, uint32_t currentFrame) noexcept {
    static constexpr uint32_t FRAMES_WAIT_TILL_RELEASE = 3;
    const auto framesWaitTillRelease = FRAMES_WAIT_TILL_RELEASE;
    // const auto framesWaitTillRelease = hmlSwapchain->imageCount(); // TODO
    const auto lastAliveFrame = currentFrame + framesWaitTillRelease;
    releaseQueue.emplace(std::move(hmlBuffer), lastAliveFrame);
}


void HmlResourceManager::tickFrame(uint32_t currentFrame) noexcept {
#if LOG_INFO
    const auto oldSize = releaseQueue.size();
#endif
    std::erase_if(releaseQueue, [&](const auto& releaseData) {
        return currentFrame > releaseData.lastAliveFrame;
    });
#if LOG_INFO
    const auto newSize = releaseQueue.size();
    if (newSize != oldSize) {
        std::cout << ":> HmlResourceManager::tickFrame() has just released " << (oldSize - newSize) << " HmlBuffers.\n";
    }
#endif
}


std::unique_ptr<HmlResourceManager> HmlResourceManager::create(
        std::shared_ptr<HmlDevice> hmlDevice, std::shared_ptr<HmlCommands> hmlCommands) noexcept {
    auto hmlResourceManager = std::make_unique<HmlResourceManager>();
    hmlResourceManager->hmlDevice = hmlDevice;
    hmlResourceManager->hmlCommands = hmlCommands;

    hmlResourceManager->dummyTextureResource = hmlResourceManager->newDummyTextureResource();

    return hmlResourceManager;
}


HmlResourceManager::~HmlResourceManager() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlResourceManager.\n";
#endif
}


std::unique_ptr<HmlBuffer> HmlResourceManager::createStagingBuffer(VkDeviceSize sizeBytes) const noexcept {
    auto buffer = std::make_unique<HmlBuffer>(true);
    buffer->hmlDevice = hmlDevice;
    buffer->type = HmlBuffer::Type::STAGING;
    buffer->sizeBytes = sizeBytes;
    const auto usage      = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    const auto memoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(sizeBytes, usage, memoryType, buffer->buffer, buffer->memory);
    return buffer;
}


std::unique_ptr<HmlBuffer> HmlResourceManager::createUniformBuffer(VkDeviceSize sizeBytes) const noexcept {
    auto buffer = std::make_unique<HmlBuffer>(true);
    buffer->hmlDevice = hmlDevice;
    buffer->type = HmlBuffer::Type::UNIFORM;
    buffer->sizeBytes = sizeBytes;
    const auto usage      = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    const auto memoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(sizeBytes, usage, memoryType, buffer->buffer, buffer->memory);
    return buffer;
}


std::unique_ptr<HmlBuffer> HmlResourceManager::createStorageBuffer(VkDeviceSize sizeBytes) const noexcept {
    auto buffer = std::make_unique<HmlBuffer>(true);
    buffer->hmlDevice = hmlDevice;
    buffer->type = HmlBuffer::Type::STORAGE;
    buffer->sizeBytes = sizeBytes;
    const auto usage      = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    const auto memoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(sizeBytes, usage, memoryType, buffer->buffer, buffer->memory);
    return buffer;
}


std::unique_ptr<HmlBuffer> HmlResourceManager::createVertexBuffer(VkDeviceSize sizeBytes) const noexcept {
    auto buffer = std::make_unique<HmlBuffer>(true);
    buffer->hmlDevice = hmlDevice;
    buffer->type = HmlBuffer::Type::VERTEX;
    buffer->sizeBytes = sizeBytes;
    const auto usage      = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto memoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(sizeBytes, usage, memoryType, buffer->buffer, buffer->memory);
    return buffer;
}


std::unique_ptr<HmlBuffer> HmlResourceManager::createIndexBuffer(VkDeviceSize sizeBytes) const noexcept {
    auto buffer = std::make_unique<HmlBuffer>(true);
    buffer->hmlDevice = hmlDevice;
    buffer->type = HmlBuffer::Type::INDEX;
    buffer->sizeBytes = sizeBytes;
    const auto usage      = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    const auto memoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(sizeBytes, usage, memoryType, buffer->buffer, buffer->memory);
    return buffer;
}


std::unique_ptr<HmlBuffer> HmlResourceManager::createVertexBufferWithData(const void* data, VkDeviceSize sizeBytes) const noexcept {
    auto stagingBuffer = createStagingBuffer(sizeBytes);
    stagingBuffer->map();
    stagingBuffer->update(data);
    stagingBuffer->unmap();

    auto buffer = std::make_unique<HmlBuffer>(false);
    buffer->hmlDevice = hmlDevice;
    buffer->type = HmlBuffer::Type::VERTEX;
    buffer->sizeBytes = sizeBytes;

    const auto usage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto memoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    createBuffer(sizeBytes, usage, memoryType, buffer->buffer, buffer->memory);

    copyBuffer(stagingBuffer->buffer, buffer->buffer, sizeBytes);

    return buffer;
}


std::unique_ptr<HmlBuffer> HmlResourceManager::createIndexBufferWithData(const void* data, VkDeviceSize sizeBytes) const noexcept {
    auto stagingBuffer = createStagingBuffer(sizeBytes);
    stagingBuffer->map();
    stagingBuffer->update(data);
    stagingBuffer->unmap();

    auto buffer = std::make_unique<HmlBuffer>(false);
    buffer->hmlDevice = hmlDevice;
    buffer->type = HmlBuffer::Type::INDEX;
    buffer->sizeBytes = sizeBytes;

    const auto usage      = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    const auto memoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    createBuffer(sizeBytes, usage, memoryType, buffer->buffer, buffer->memory);

    copyBuffer(stagingBuffer->buffer, buffer->buffer, sizeBytes);

    return buffer;
}


std::unique_ptr<HmlImageResource> HmlResourceManager::newTextureResourceFromData(uint32_t width, uint32_t height, uint32_t componentsCount, unsigned char* data, VkFormat format, std::optional<VkFilter> filter) noexcept {
    // ======== Load data to a staging buffer
    auto stagingBuffer = createStagingBuffer(width * height * componentsCount);
    stagingBuffer->map();
    stagingBuffer->update(data);
    stagingBuffer->unmap();

    // ======== Create the image resource
    const VkExtent2D            extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    const VkImageUsageFlags     usage  = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    const VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    auto resource = newBlankImageResource(extent, format, usage, aspect);

    // ======== Copy data from the staging buffer to the image resource
    if (!resource->blockingTransitionLayoutTo(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, hmlCommands)) {
        std::cerr << "::> Failed to create HmlImageResource as a texture resource.\n";
        return { nullptr };
    }
    copyBufferToImage(stagingBuffer->buffer, resource->image, extent);
    if (!resource->blockingTransitionLayoutTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, hmlCommands)) {
        std::cerr << "::> Failed to create HmlImageResource as a texture resource.\n";
        return { nullptr };
    }

    // ======== Finish initializing resource members
    resource->type = HmlImageResource::Type::TEXTURE;
    resource->width = width;
    resource->height = height;
    if (filter) resource->sampler = createTextureSampler(*filter, *filter);

    return resource;
}


std::unique_ptr<HmlImageResource> HmlResourceManager::newDummyTextureResource() noexcept {
    const uint32_t width = 2;
    const uint32_t height = 2;
    const uint32_t componentsCount = 4;
    unsigned char pixels[width * height * componentsCount] = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 255, 255
    };

    const VkFilter filter = VK_FILTER_NEAREST;
    const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    auto textureResource = newTextureResourceFromData(width, height, componentsCount, pixels, format, { filter });

    return textureResource;
}


std::unique_ptr<HmlImageResource> HmlResourceManager::newTextureResource(const char* fileName, uint32_t componentsCount, VkFormat format, VkFilter filter) noexcept {
    int width, height, channels;
    // NOTE Will force alpha even if it is not present
    stbi_uc* pixels = stbi_load(fileName, &width, &height, &channels, componentsCount);
    if (!pixels) {
        std::cerr << "::> Failed to load texture image using stb library: " << fileName << ".\n";
        return { nullptr };
    }

    auto textureResource = newTextureResourceFromData(
        static_cast<uint32_t>(width), static_cast<uint32_t>(height), componentsCount, pixels, format, { filter });

    stbi_image_free(pixels);

    return textureResource;
}


std::unique_ptr<HmlImageResource> HmlResourceManager::newBlankImageResource(
        VkExtent2D extent,
        VkFormat format,
        VkImageUsageFlags usage,
        VkImageAspectFlagBits aspect) noexcept {
    const VkImageTiling            tiling     = VK_IMAGE_TILING_OPTIMAL;
    const VkMemoryPropertyFlagBits memoryType = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    auto resource = std::make_unique<HmlImageResource>();
    resource->hmlDevice = hmlDevice;
    resource->type = HmlImageResource::Type::BLANK;
    resource->format = format;
    if (!createImage(extent.width, extent.height, format, tiling, usage, memoryType, resource->image, resource->memory)) return { nullptr };
    resource->view = createImageView(resource->image, format, aspect);
    return resource;
}


std::unique_ptr<HmlImageResource> HmlResourceManager::newShadowResource(VkExtent2D extent, VkFormat format) noexcept {
    const VkImageUsageFlags     usage  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    // const VkFormat              format = VK_FORMAT_R8G8B8A8_SRGB;
    const VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    auto resource = newBlankImageResource(extent, format, usage, aspect);
    resource->sampler = createTextureSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR); // TODO XXX does not belong with this function name
    resource->type = HmlImageResource::Type::SHADOW_MAP;

    if (!resource->blockingTransitionLayoutTo(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, hmlCommands)) {
        std::cerr << "::> Failed to create HmlImageResource as a shadow map.\n";
        return { nullptr };
    }

    return resource;
}


std::unique_ptr<HmlImageResource> HmlResourceManager::newRenderTargetImageResource(VkExtent2D extent, VkFormat format) noexcept {
    const VkImageUsageFlags     usage  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    // const VkFormat              format = VK_FORMAT_R8G8B8A8_SRGB;
    const VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    auto resource = newBlankImageResource(extent, format, usage, aspect);
    resource->sampler = createTextureSampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST); // TODO XXX does not belong with this function name
    resource->type = HmlImageResource::Type::RENDER_TARGET;

    if (!resource->blockingTransitionLayoutTo(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, hmlCommands)) {
        std::cerr << "::> Failed to create HmlImageResource as a render target.\n";
        return { nullptr };
    }

    return resource;
}


std::unique_ptr<HmlImageResource> HmlResourceManager::newDepthResource(VkExtent2D extent) noexcept {
    const VkImageUsageFlags     usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    const VkFormat              format = findDepthFormat();
    const VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    auto resource = newBlankImageResource(extent, format, usage, aspect);
    resource->type = HmlImageResource::Type::DEPTH;

    if (!resource->blockingTransitionLayoutTo(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, hmlCommands)) {
        std::cerr << "::> Failed to create HmlImageResource as a depth resource.\n";
        return { nullptr };
    }

    return resource;
}


std::vector<std::shared_ptr<HmlImageResource>> HmlResourceManager::wrapSwapchainImagesIntoImageResources(
        std::vector<VkImage>&& images, VkFormat format, VkExtent2D extent) noexcept {
    std::vector<std::shared_ptr<HmlImageResource>> imageResources;
    for (const auto image : images) {
        const auto imageView = createImageView(image, format, VK_IMAGE_ASPECT_COLOR_BIT);
        if (!imageView) return {};

        auto imageResource = std::make_shared<HmlImageResource>();
        imageResource->hmlDevice = hmlDevice;
        imageResource->image = image;
        imageResource->type = HmlImageResource::Type::SWAPCHAIN_IMAGE;
        imageResource->format = format;
        imageResource->width = extent.width;
        imageResource->height = extent.height;
        imageResource->layout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageResource->view = imageView;

        imageResources.push_back(imageResource);
    }

    return imageResources;
}


// Model with color
std::shared_ptr<HmlModelResource> HmlResourceManager::newModel(const void* vertices, size_t verticesSizeBytes, const std::vector<uint32_t>& indices) noexcept {
    auto model = std::make_shared<HmlModelResource>();
    model->hmlDevice = hmlDevice;
    model->indicesCount = indices.size();
    model->textureResource = { nullptr };
    model->vertexBuffer = createVertexBufferWithData(vertices, verticesSizeBytes);
    model->indexBuffer = createIndexBufferWithData(indices.data(), indices.size() * sizeof(indices[0]));

    models.push_back(model);
    return model;
}


// Model with texture
std::shared_ptr<HmlModelResource> HmlResourceManager::newModel(const void* vertices, size_t verticesSizeBytes, const std::vector<uint32_t>& indices, const char* textureFileName, VkFilter filter) noexcept {
    auto model = std::make_shared<HmlModelResource>();
    model->hmlDevice = hmlDevice;
    model->indicesCount = indices.size();
    model->textureResource = newTextureResource(textureFileName, 4, VK_FORMAT_R8G8B8A8_SRGB, filter);
    model->vertexBuffer = createVertexBufferWithData(vertices, verticesSizeBytes);
    model->indexBuffer = createIndexBufferWithData(indices.data(), indices.size() * sizeof(indices[0]));

    models.push_back(model);
    return model;
}
// ========================================================================
// ========================================================================
// ========================================================================
// NOTE: unused
// TODO remove
void HmlResourceManager::createVertexBufferHost(VkBuffer& vertexBuffer,
        VkDeviceMemory& vertexBufferMemory, const void* vertices, VkDeviceSize sizeBytes) noexcept {
    const auto usage      = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    const auto memoryType = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    createBuffer(sizeBytes, usage, memoryType, vertexBuffer, vertexBufferMemory);
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
// ========================================================================
// ========================================================================
// ========================================================================
VkSampler HmlResourceManager::createTextureSamplerForFonts() noexcept {
    const VkFilter magFilter = VK_FILTER_LINEAR;
    const VkFilter minFilter = VK_FILTER_LINEAR;
    const VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    const VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    // For anisotropic filtering
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(hmlDevice->physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // VK_FILTER_LINEAR
    samplerInfo.magFilter = magFilter;
    samplerInfo.minFilter = minFilter;
    // VK_SAMPLER_ADDRESS_MODE_REPEAT
    // VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
    // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
    // VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE
    // VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    samplerInfo.anisotropyEnable = VK_TRUE; // FALSE to turn off, set max = 1.0f
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy; // use the best available
    samplerInfo.borderColor = borderColor;
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


VkSampler HmlResourceManager::createTextureSampler(VkFilter magFilter, VkFilter minFilter) noexcept {
    // For anisotropic filtering
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(hmlDevice->physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    // VK_FILTER_LINEAR
    samplerInfo.magFilter = magFilter;
    samplerInfo.minFilter = minFilter;
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
// VK_IMAGE_USAGE_TRANSFER_DST_BIT -- we will manually copy data to it
// VK_IMAGE_USAGE_SAMPLED_BIT -- sampled from the shader
// VK_IMAGE_USAGE_STORAGE_BIT
// VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
// VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
// VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT
// VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT
bool HmlResourceManager::createImage(uint32_t width, uint32_t height, VkFormat format,
        VkImageTiling tiling, VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties, VkImage& image,
        VkDeviceMemory& imageMemory) noexcept {
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
        std::cerr << "::> Failed to create image.\n";
        return false;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(hmlDevice->device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(hmlDevice->device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        std::cerr << "::> Failed to allocate image memory.\n";
        return false;
    }

    vkBindImageMemory(hmlDevice->device, image, imageMemory, 0);

    return true;
}


VkImageView HmlResourceManager::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) noexcept {
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
VkFormat HmlResourceManager::findSupportedFormat(const std::vector<VkFormat>& candidates,
        VkImageTiling tiling, VkFormatFeatureFlags features) const noexcept {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(hmlDevice->physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    std::cerr << "::> Failed to find supported VkFormat.\n";
    return static_cast<VkFormat>(0);
}


VkFormat HmlResourceManager::findDepthFormat() const noexcept {
    return findSupportedFormat(
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}
// ========================================================================
// ========================================================================
// ========================================================================
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
bool HmlResourceManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) const noexcept {
    // ======== Create a Buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    // Will only be used by one queue at a time (the GraphicsQueue):
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bufferInfo.flags = 0; // default. To configure sparse buffer memory
    if (vkCreateBuffer(hmlDevice->device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        std::cerr << "::> Failed to create vertex buffer.\n";
        return false;
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
        std::cerr << "::> Failed to allocate vertex buffer memory.\n";
        return false;
    }
    // ======== Bind the allocated memory to the buffer
    // The last argument -- offset within the allocated region of memory.
    // Must be divisible by memRequirements.alignment.
    vkBindBufferMemory(hmlDevice->device, buffer, bufferMemory, 0);

    return true;
}
// ========================================================================
// ========================================================================
// ========================================================================
// Before the barrier, we wait for stages in stagesBeforeBarrier, and once
// those stages complete, we unblock the pipeline stages in stagesAfterBarrier
// void HmlResourceManager::transitionBuffer(const std::vector<BufferTransition>& bufferTransitions,
//         VkPipelineStageFlags stagesBeforeBarrier, VkPipelineStageFlags stagesAfterBarrier) noexcept {
//     std::vector<VkBufferMemoryBarrier> bufferMemoryBarriers;
//     for (const auto& bufferTransition : bufferTransitions) {
//         VkBufferMemoryBarrier bufferMemoryBarrier;
//         bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
//         bufferMemoryBarrier.pNext = nullptr;
//         bufferMemoryBarrier.offset = 0;
//         bufferMemoryBarrier.size = VK_WHOLE_SIZE;
//         bufferMemoryBarrier.buffer =              bufferTransition.buffer;
//         bufferMemoryBarrier.srcAccessMask =       bufferTransition.currentAccess;
//         bufferMemoryBarrier.dstAccessMask =       bufferTransition.newAccess;
//         bufferMemoryBarrier.srcQueueFamilyIndex = bufferTransition.currentQueueFamily;
//         bufferMemoryBarrier.dstQueueFamilyIndex = bufferTransition.newQueueFamily;
//
//         bufferMemoryBarriers.push_back(bufferMemoryBarrier);
//     }
//
//     VkCommandBuffer commandBuffer = hmlCommands->beginSingleTimeCommands();
//     vkCmdPipelineBarrier(
//         commandBuffer,
//         stagesBeforeBarrier, stagesAfterBarrier,
//         0, // or VK_DEPENDENCY_BY_REGION_BIT
//         // (global) MemoryBarrier:
//         0, nullptr,
//         // BufferMemoryBarrier:
//         static_cast<uint32_t>(bufferMemoryBarriers.size()), bufferMemoryBarriers.data(),
//         // ImageMemoryBarrier:
//         0, nullptr);
//     hmlCommands->endSingleTimeCommands(commandBuffer);
// }


bool HmlImageResource::hasStencilComponent() const noexcept {
    return (format == VK_FORMAT_D32_SFLOAT_S8_UINT) || (format == VK_FORMAT_D24_UNORM_S8_UINT);
}


bool HmlImageResource::blockingTransitionLayoutTo(VkImageLayout newLayout, std::shared_ptr<HmlCommands> hmlCommands) noexcept {
    VkCommandBuffer commandBuffer = hmlCommands->beginSingleTimeCommands();
    const auto res = transitionLayoutTo(newLayout, commandBuffer);
    if (!res) return false;
    hmlCommands->endSingleTimeCommands(commandBuffer);
    return true;
}


bool HmlImageResource::transitionLayoutTo(VkImageLayout newLayout, VkCommandBuffer commandBuffer) noexcept {
    const auto oldLayout = layout;

    // One of the most common ways to perform layout transitions is using
    // an image memory barrier. A pipeline barrier like that is generally
    // used to synchronize access to resources, like ensuring that a write
    // to a buffer completes before reading from it, but it can also be used
    // to transition image layouts and transfer queue family ownership when
    // VK_SHARING_MODE_EXCLUSIVE is used. There is an equivalent buffer
    // memory barrier to do this for buffers.
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    // If you are using the barrier to transfer queue family ownership,
    // then these two fields should be the indices of the queue families.
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencilComponent()) {
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
/*
    VkAccessFlagBits:
    VK_ACCESS_INDIRECT_COMMAND_READ_BIT
    VK_ACCESS_INDEX_READ_BIT
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
    VK_ACCESS_UNIFORM_READ_BIT
    VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
    VK_ACCESS_SHADER_READ_BIT
    VK_ACCESS_SHADER_WRITE_BIT
    VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
    VK_ACCESS_TRANSFER_READ_BIT
    VK_ACCESS_TRANSFER_WRITE_BIT
    VK_ACCESS_HOST_READ_BIT
    VK_ACCESS_HOST_WRITE_BIT
    VK_ACCESS_MEMORY_READ_BIT
    VK_ACCESS_MEMORY_WRITE_BIT

    VkPipelineStageFlagBits:
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
    VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT
    VK_PIPELINE_STAGE_VERTEX_INPUT_BIT
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT
    VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT
    VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
    VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
    VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
    VK_PIPELINE_STAGE_TRANSFER_BIT
    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    VK_PIPELINE_STAGE_HOST_BIT
    VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
 */
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        // The writes don't have to wait on anything
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // no stage
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        // NOTE To render target
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // no stage
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // no stage
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        // NOTE From render sampler to render target
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        // NOTE From render target to render sampler
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        destinationStage = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // no stage
        // The writing happens in VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT.
        // We need to pick the earliest; the reading happens in:
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else {
        std::cerr << "::> Provided layout transition is not implemented: " << oldLayout << "->" << newLayout << ".\n";
        return false;
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

    layout = newLayout;

    return true;
}
// ========================================================================
// ========================================================================
// ========================================================================
uint32_t HmlResourceManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept {
    for (uint32_t i = 0; i < hmlDevice->memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && ((hmlDevice->memProperties.memoryTypes[i].propertyFlags & properties) == properties)) {
            return i;
        }
    }

    std::cerr << "::> Failed to find suitable memory type.\n";
    return 0;
}
// ========================================================================
// ========================================================================
// ========================================================================
void HmlResourceManager::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) const noexcept {
    VkCommandBuffer commandBuffer = hmlCommands->beginSingleTimeCommands();

    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0; // Optional
    copyRegion.dstOffset = 0; // Optional
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    hmlCommands->endSingleTimeCommands(commandBuffer);
}


void HmlResourceManager::copyBufferToImage(VkBuffer buffer, VkImage image, VkExtent2D extent) const noexcept {
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
    region.imageExtent = { extent.width, extent.height, 1 };
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
