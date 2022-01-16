#ifndef HML_DESCRIPTORS
#define HML_DESCRIPTORS

#include <memory>

#include "HmlDevice.h"
#include "HmlModel.h"
#include "HmlShaderLayout.h"


struct HmlDescriptors {
    std::shared_ptr<HmlDevice> hmlDevice;


    static std::unique_ptr<HmlDescriptors> create(std::shared_ptr<HmlDevice> hmlDevice) {
        auto hmlDescriptors = std::make_unique<HmlDescriptors>();
        hmlDescriptors->hmlDevice = hmlDevice;
        return hmlDescriptors;
    }


    ~HmlDescriptors() {
        std::cout << ":> Destroying HmlDescriptors...\n";
    }


    void updateDescriptorSetUniformBuffer(VkDescriptorSet descriptorSet, VkBuffer uniformBuffer, VkDeviceSize sizeBytes) {
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


    std::vector<VkDescriptorSet> createDescriptorSets(uint32_t count, VkDescriptorSetLayout layout, VkDescriptorPool descriptorPool) {
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


    struct HmlDescriptorSetLayoutBuilder {
        HmlDescriptorSetLayoutBuilder& withUniformBufferAt(uint32_t binding, VkShaderStageFlags stages) {
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = binding,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = stages,
                .pImmutableSamplers = nullptr
            });
            return *this;
        }


        HmlDescriptorSetLayoutBuilder& withTextureAt(uint32_t binding, VkShaderStageFlags stages) {
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = binding,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = stages,
                .pImmutableSamplers = nullptr
            });
            return *this;
        }


        HmlDescriptorSetLayoutBuilder& withTextureArrayAt(uint32_t binding, VkShaderStageFlags stages, uint32_t arrayLength) {
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = binding,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = arrayLength,
                .stageFlags = stages,
                .pImmutableSamplers = nullptr
            });
            return *this;
        }


        VkDescriptorSetLayout build(std::shared_ptr<HmlDevice> hmlDevice) {
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
    HmlDescriptorSetLayoutBuilder buildDescriptorSetLayout() {
        return HmlDescriptorSetLayoutBuilder{};
    }


    struct HmlDescriptorPoolBuilder {
        uint32_t textureCount = 0;
        uint32_t uboCount = 0;
        uint32_t descriptorSetCount = 0;

        HmlDescriptorPoolBuilder& withTextures(uint32_t count) {
            textureCount = count;
            return *this;
        }

        HmlDescriptorPoolBuilder& withUniformBuffers(uint32_t count) {
            uboCount = count;
            return *this;
        }

        HmlDescriptorPoolBuilder& maxSets(uint32_t count) {
            descriptorSetCount = count;
            return *this;
        }

        VkDescriptorPool build(std::shared_ptr<HmlDevice> hmlDevice) {
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
    };


    HmlDescriptorPoolBuilder buildDescriptorPool() {
        return HmlDescriptorPoolBuilder{};
    }
};

#endif
