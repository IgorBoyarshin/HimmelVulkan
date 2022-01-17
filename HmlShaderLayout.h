#ifndef HML_SHADER_LAYOUT
#define HML_SHADER_LAYOUT

#include <vector>
#include <cassert>

#include <vulkan.hpp>


struct HmlSetUpdater {
    VkDescriptorSet descriptorSet;

    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkWriteDescriptorSet> descriptorWrites;


    HmlSetUpdater(VkDescriptorSet descriptorSet) : descriptorSet(descriptorSet) {}


    HmlSetUpdater& storageBufferAt(uint32_t binding, VkBuffer buffer, VkDeviceSize sizeBytes) {
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


    HmlSetUpdater& uniformBufferAt(uint32_t binding, VkBuffer buffer, VkDeviceSize sizeBytes) {
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


    HmlSetUpdater& textureAt(uint32_t binding, VkSampler textureSampler, VkImageView textureImageView) {
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


    HmlSetUpdater& textureArrayAt(uint32_t binding, const std::vector<VkSampler>& textureSamplers, const std::vector<VkImageView>& textureImageViews) {
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


    void update(std::shared_ptr<HmlDevice> hmlDevice) const noexcept {
        vkUpdateDescriptorSets(hmlDevice->device,
            static_cast<uint32_t>(descriptorWrites.size()),
            descriptorWrites.data(), 0, nullptr);
    }
};


// struct HmlSetLayout {
// };



// TODO can be disposed of after refactoring
// struct HmlShaderLayoutItem {
//     VkDescriptorType descriptorType;
//     uint32_t binding;
//     VkShaderStageFlags stages;
//     uint32_t arrayLength;
// };


// NOTE
// NOTE
// This could be a global class that manages all possible sets.
// For each "pipeline" it creates it and maps it to a vector of Sets, so that we
// can access sets[0].
// NOTE
// NOTE


// TODO
// TODO
// TODO can rewrite to directly store the final data
// TODO
// TODO
// struct HmlShaderLayout {
//     // For each set number
//     std::vector<std::vector<HmlShaderLayoutItem>> items;
//
//     HmlShaderLayout& addUniformBuffer(uint32_t set, uint32_t binding, VkShaderStageFlags stages) {
//         prepareSetIfAbsent(set);
//         items[set].push_back(HmlShaderLayoutItem{
//             .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//             .binding = binding,
//             .stages = stages,
//             .arrayLength = 1
//         });
//         return *this;
//     }
//
//     HmlShaderLayout& addTexture(uint32_t set, uint32_t binding, VkShaderStageFlags stages) {
//         prepareSetIfAbsent(set);
//         items[set].push_back(HmlShaderLayoutItem{
//             .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//             .binding = binding,
//             .stages = stages,
//             .arrayLength = 1
//         });
//         return *this;
//     }
//
//     HmlShaderLayout& addTextureArray(uint32_t set, uint32_t binding, VkShaderStageFlags stages, uint32_t arrayLength) {
//         prepareSetIfAbsent(set);
//         items[set].push_back(HmlShaderLayoutItem{
//             .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//             .binding = binding,
//             .stages = stages,
//             .arrayLength = arrayLength
//         });
//         return *this;
//     }
//
//     std::vector<VkDescriptorSetLayoutBinding> layoutBindingsForSet(uint32_t set) {
//         std::vector<VkDescriptorSetLayoutBinding> bindings;
//         for (const auto& shaderLayout : items[set]) {
//             bindings.push_back(VkDescriptorSetLayoutBinding{
//                 .binding = shaderLayout.binding,
//                 .descriptorType = shaderLayout.descriptorType,
//                 .descriptorCount = shaderLayout.arrayLength,
//                 .stageFlags = shaderLayout.stages,
//                 .pImmutableSamplers = nullptr
//             });
//         }
//
//         return bindings;
//     }
//
//     size_t setsCount() const noexcept {
//         return items.size();
//     }
//
//     private:
//     void prepareSetIfAbsent(uint32_t set) {
//         while (items.size() <= set) items.push_back({});
//     }
// };

#endif
