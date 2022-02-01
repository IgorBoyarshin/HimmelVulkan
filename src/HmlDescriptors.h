#ifndef HML_DESCRIPTORS
#define HML_DESCRIPTORS

#include <memory>
#include <iostream>
#include <vector>
#include <cassert>

#include "HmlDevice.h"
#include "HmlModel.h"



// NOTE
// NOTE
// This could be a global class that manages all possible sets.
// For each "pipeline" it creates it and maps it to a vector of Sets, so that we
// can access sets[0].
// NOTE
// NOTE


struct HmlDescriptorSetUpdater {
    VkDescriptorSet descriptorSet;

    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkWriteDescriptorSet> descriptorWrites;

    // NOTE because we would like the helper buffers to remain a vector (for resizability),
    // but because they re-allocate themselves, we cannot assign addresses from them upfront.
    // So we store the indices into them and assign the correct addresses upon build.
    std::vector<size_t> bufferInfosIndices;
    std::vector<size_t> imageInfosIndices;


    inline HmlDescriptorSetUpdater(VkDescriptorSet descriptorSet) noexcept : descriptorSet(descriptorSet) {}

    HmlDescriptorSetUpdater& storageBufferAt(uint32_t binding, VkBuffer buffer, VkDeviceSize sizeBytes) noexcept;
    HmlDescriptorSetUpdater& uniformBufferAt(uint32_t binding, VkBuffer buffer, VkDeviceSize sizeBytes) noexcept;
    HmlDescriptorSetUpdater& textureAt(uint32_t binding, VkSampler textureSampler, VkImageView textureImageView) noexcept;
    HmlDescriptorSetUpdater& textureArrayAt(uint32_t binding, const std::vector<VkSampler>& textureSamplers,
            const std::vector<VkImageView>& textureImageViews) noexcept;

    void update(std::shared_ptr<HmlDevice> hmlDevice) noexcept;
};
// ============================================================================
// ============================================================================
// ============================================================================
struct HmlDescriptors {
    std::shared_ptr<HmlDevice> hmlDevice;

    static std::unique_ptr<HmlDescriptors> create(std::shared_ptr<HmlDevice> hmlDevice) noexcept;
    std::vector<VkDescriptorSet> createDescriptorSets(uint32_t count, VkDescriptorSetLayout layout, VkDescriptorPool descriptorPool) noexcept;


    struct HmlDescriptorSetLayoutBuilder {
        HmlDescriptorSetLayoutBuilder& withStorageBufferAt(uint32_t binding, VkShaderStageFlags stages) noexcept;
        HmlDescriptorSetLayoutBuilder& withUniformBufferAt(uint32_t binding, VkShaderStageFlags stages) noexcept;
        HmlDescriptorSetLayoutBuilder& withTextureAt(uint32_t binding, VkShaderStageFlags stages) noexcept;
        HmlDescriptorSetLayoutBuilder& withTextureArrayAt(uint32_t binding, VkShaderStageFlags stages, uint32_t arrayLength) noexcept;
        VkDescriptorSetLayout build(std::shared_ptr<HmlDevice> hmlDevice) noexcept;

        private:
        std::vector<VkDescriptorSetLayoutBinding> bindings;
    };


    // "layout (set=M, binding=N) uniform sampler2D variableNameArray[I];":
    // -- M refers the the M'th descriptor set layout in the pSetLayouts member of the pipeline layout
    // -- N refers to the N'th descriptor set (binding) in M's pBindings member of the descriptor set layout
    // -- I is the index into the array of descriptors in N's descriptor set
    //
    // For UBO: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    // For texture: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    // stageFlags: see VkShaderStageFlagBits
    // descriptorCount: e.g. to specify a transformation for each of the bones
    //                  in a skeleton for skeletal animation
    inline HmlDescriptorSetLayoutBuilder buildDescriptorSetLayout() const noexcept {
        return HmlDescriptorSetLayoutBuilder{};
    }


    struct HmlDescriptorPoolBuilder {
        uint32_t textureCount = 0;
        uint32_t uboCount = 0;
        uint32_t ssboCount = 0;
        uint32_t descriptorSetCount = 0;

        HmlDescriptorPoolBuilder& withTextures(uint32_t count) noexcept;
        HmlDescriptorPoolBuilder& withUniformBuffers(uint32_t count) noexcept;
        HmlDescriptorPoolBuilder& withStorageBuffers(uint32_t count) noexcept;
        HmlDescriptorPoolBuilder& maxSets(uint32_t count) noexcept;
        VkDescriptorPool build(std::shared_ptr<HmlDevice> hmlDevice) noexcept;
    };


    inline HmlDescriptorPoolBuilder buildDescriptorPool() const noexcept {
        return HmlDescriptorPoolBuilder{};
    }
};

#endif
