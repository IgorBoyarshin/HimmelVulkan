#include "HmlDescriptors.h"


HmlDescriptorSetUpdater& HmlDescriptorSetUpdater::storageBufferAt(
        uint32_t binding, VkBuffer buffer, VkDeviceSize sizeBytes) noexcept {
    bufferInfos.push_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = 0,
        .range = sizeBytes
    });

    descriptorWrites.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo       = nullptr,
        .pBufferInfo      = &bufferInfos.back(),
        .pTexelBufferView = nullptr
    });

    return *this;
}


HmlDescriptorSetUpdater& HmlDescriptorSetUpdater::uniformBufferAt(
        uint32_t binding, VkBuffer buffer, VkDeviceSize sizeBytes) noexcept {
    bufferInfos.push_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = 0,
        .range = sizeBytes
    });

    descriptorWrites.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo       = nullptr,
        .pBufferInfo      = &bufferInfos.back(),
        .pTexelBufferView = nullptr
    });

    return *this;
}


HmlDescriptorSetUpdater& HmlDescriptorSetUpdater::textureAt(
        uint32_t binding, VkSampler textureSampler, VkImageView textureImageView) noexcept {
    imageInfos.push_back(VkDescriptorImageInfo{
        .sampler = textureSampler,
        .imageView = textureImageView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    });

    descriptorWrites.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo       = &imageInfos.back(),
        .pBufferInfo      = nullptr,
        .pTexelBufferView = nullptr
    });

    return *this;
}


HmlDescriptorSetUpdater& HmlDescriptorSetUpdater::textureArrayAt(
        uint32_t binding, const std::vector<VkSampler>& textureSamplers, const std::vector<VkImageView>& textureImageViews) noexcept {
    assert((textureSamplers.size() == textureImageViews.size()) && "::> Passed vector size while updating textures array do not match.");
    const auto count = textureSamplers.size();

    for (size_t i = 0; i < count; i++) {
        imageInfos.push_back(VkDescriptorImageInfo{
            .sampler = textureSamplers[i],
            .imageView = textureImageViews[i],
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        });
    }

    descriptorWrites.push_back(VkWriteDescriptorSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet,
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<uint32_t>(count),
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo       = &(*(imageInfos.cend() - count)), // TODO simplify??
        .pBufferInfo      = nullptr,
        .pTexelBufferView = nullptr
    });

    return *this;
}


void HmlDescriptorSetUpdater::update(std::shared_ptr<HmlDevice> hmlDevice) const noexcept {
    vkUpdateDescriptorSets(hmlDevice->device,
        static_cast<uint32_t>(descriptorWrites.size()),
        descriptorWrites.data(), 0, nullptr);
}
// ============================================================================
// ============================================================================
// ============================================================================
std::unique_ptr<HmlDescriptors> HmlDescriptors::create(std::shared_ptr<HmlDevice> hmlDevice) noexcept {
    auto hmlDescriptors = std::make_unique<HmlDescriptors>();
    hmlDescriptors->hmlDevice = hmlDevice;
    return hmlDescriptors;
}


void HmlDescriptors::updateDescriptorSetUniformBuffer(VkDescriptorSet descriptorSet,
        VkBuffer uniformBuffer, VkDeviceSize sizeBytes) noexcept {
    VkDescriptorBufferInfo bufferInfo{
        .buffer = uniformBuffer,
        .offset = 0,
        .range = sizeBytes
    };
    VkWriteDescriptorSet descriptorWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo       = nullptr,
        .pBufferInfo      = &bufferInfo,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(hmlDevice->device, 1, &descriptorWrite, 0, nullptr);
}


std::vector<VkDescriptorSet> HmlDescriptors::createDescriptorSets(uint32_t count,
        VkDescriptorSetLayout layout, VkDescriptorPool descriptorPool) noexcept {
    std::vector<VkDescriptorSetLayout> layouts(count, layout);

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = layouts.size();
    allocInfo.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> descriptorSets(count);
    if (vkAllocateDescriptorSets(hmlDevice->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
        std::cerr << "::> Failed to allocare DescriptorSets.\n";
        return {};
    }
    return descriptorSets;
}
// ============================================================================
// ============================================================================
// ============================================================================
HmlDescriptors::HmlDescriptorSetLayoutBuilder& HmlDescriptors::HmlDescriptorSetLayoutBuilder::withStorageBufferAt(
        uint32_t binding, VkShaderStageFlags stages) noexcept {
    bindings.push_back(VkDescriptorSetLayoutBinding{
        .binding = binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = stages,
        .pImmutableSamplers = nullptr
    });
    return *this;
}


HmlDescriptors::HmlDescriptorSetLayoutBuilder& HmlDescriptors::HmlDescriptorSetLayoutBuilder::withUniformBufferAt(
        uint32_t binding, VkShaderStageFlags stages) noexcept {
    bindings.push_back(VkDescriptorSetLayoutBinding{
        .binding = binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = stages,
        .pImmutableSamplers = nullptr
    });
    return *this;
}


HmlDescriptors::HmlDescriptorSetLayoutBuilder& HmlDescriptors::HmlDescriptorSetLayoutBuilder::withTextureAt(
        uint32_t binding, VkShaderStageFlags stages) noexcept {
    bindings.push_back(VkDescriptorSetLayoutBinding{
        .binding = binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = stages,
        .pImmutableSamplers = nullptr
    });
    return *this;
}


HmlDescriptors::HmlDescriptorSetLayoutBuilder& HmlDescriptors::HmlDescriptorSetLayoutBuilder::withTextureArrayAt(
        uint32_t binding, VkShaderStageFlags stages, uint32_t arrayLength) noexcept {
    bindings.push_back(VkDescriptorSetLayoutBinding{
        .binding = binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = arrayLength,
        .stageFlags = stages,
        .pImmutableSamplers = nullptr
    });
    return *this;
}


VkDescriptorSetLayout HmlDescriptors::HmlDescriptorSetLayoutBuilder::build(std::shared_ptr<HmlDevice> hmlDevice) noexcept {
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout;
    if (vkCreateDescriptorSetLayout(hmlDevice->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        std::cerr << "::> Failed to create DescriptorSetLayout.\n";
        return VK_NULL_HANDLE;
    }
    return descriptorSetLayout;
}
// ============================================================================
// ============================================================================
// ============================================================================
HmlDescriptors::HmlDescriptorPoolBuilder& HmlDescriptors::HmlDescriptorPoolBuilder::withTextures(uint32_t count) noexcept {
    textureCount = count;
    return *this;
}


HmlDescriptors::HmlDescriptorPoolBuilder& HmlDescriptors::HmlDescriptorPoolBuilder::withUniformBuffers(uint32_t count) noexcept {
    uboCount = count;
    return *this;
}


HmlDescriptors::HmlDescriptorPoolBuilder& HmlDescriptors::HmlDescriptorPoolBuilder::withStorageBuffers(uint32_t count) noexcept {
    ssboCount = count;
    return *this;
}


HmlDescriptors::HmlDescriptorPoolBuilder& HmlDescriptors::HmlDescriptorPoolBuilder::maxSets(uint32_t count) noexcept {
    descriptorSetCount = count;
    return *this;
}


VkDescriptorPool HmlDescriptors::HmlDescriptorPoolBuilder::build(std::shared_ptr<HmlDevice> hmlDevice) noexcept {
    std::vector<VkDescriptorPoolSize> poolSizes{};
    if (uboCount) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = uboCount
        });
    }
    if (textureCount) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = textureCount
        });
    }
    if (ssboCount) {
        poolSizes.push_back(VkDescriptorPoolSize{
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = ssboCount
        });
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = descriptorSetCount;
    // Can use VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT to specify
    // if individual descriptor sets can be freed or not.
    poolInfo.flags = 0;

    VkDescriptorPool descriptorPool;
    if (vkCreateDescriptorPool(hmlDevice->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        std::cerr << "::> Failed to create DescriptorPool.\n";
        return VK_NULL_HANDLE;
    }
    return descriptorPool;
}
