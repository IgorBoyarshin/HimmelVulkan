#ifndef HML_RENDERER
#define HML_RENDERER

#include <memory>
#include <vector>

#include "HmlPipeline.h"
#include "HmlSwapchain.h"
#include "HmlModel.h"
#include "HmlResourceManager.h"


struct HmlShaderLayoutItem {
    VkDescriptorType descriptorType;
    uint32_t binding;
    VkShaderStageFlags stages;
};

struct HmlShaderLayout {
    std::vector<HmlShaderLayoutItem> items;

    HmlShaderLayout& addUniformBuffer(uint32_t binding, VkShaderStageFlags stages) {
        items.push_back(HmlShaderLayoutItem{
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .binding = binding,
            .stages = stages
        });
        return *this;
    }

    HmlShaderLayout& addTexture(uint32_t binding, VkShaderStageFlags stages) {
        items.push_back(HmlShaderLayoutItem{
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .binding = binding,
            .stages = stages
        });
        return *this;
    }

    std::vector<VkDescriptorSetLayoutBinding> createLayoutBindings() {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (const auto& shaderLayout : items) {
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = shaderLayout.binding,
                .descriptorType = shaderLayout.descriptorType,
                .descriptorCount = 1,
                .stageFlags = shaderLayout.stages,
                .pImmutableSamplers = nullptr
            });
        }

        return bindings;
    }
};


struct HmlRenderer {
    struct SimpleUniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };


    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;

    std::shared_ptr<HmlPipeline> hmlPipeline;
    HmlShaderLayout hmlShaderLayout;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets; // per swapChainImage
    VkDescriptorSetLayout descriptorSetLayout;


    // TODO maybe pass only a subset of swapchain members
    static std::unique_ptr<HmlRenderer> createSimpleRenderer(
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlSwapchain> hmlSwapchain, // XXX not stored
            std::shared_ptr<HmlResourceManager> hmlResourceManager) {
        auto hmlRenderer = std::make_unique<HmlRenderer>();
        hmlRenderer->hmlDevice = hmlDevice;
        hmlRenderer->hmlResourceManager = hmlResourceManager;


        hmlRenderer->hmlShaderLayout = HmlShaderLayout()
            .addUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT)
            .addTexture(1, VK_SHADER_STAGE_FRAGMENT_BIT);

        hmlRenderer->descriptorSetLayout = hmlRenderer->createDescriptorSetLayout(hmlRenderer->hmlShaderLayout.createLayoutBindings());
        if (!hmlRenderer->descriptorSetLayout) return { nullptr };


        HmlGraphicsPipelineConfig pipelineConfig{
            .bindingDescriptions   = HmlSimpleModel::Vertex::getBindingDescriptions(),
            .attributeDescriptions = HmlSimpleModel::Vertex::getAttributeDescriptions(),
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .hmlShaders = HmlShaders()
                .addVertex("vertex.spv")
                .addFragment("fragment.spv"), // TODO rename to "simple"
            .renderPass = hmlSwapchain->renderPass,
            .swapchainExtent = hmlSwapchain->extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayout = hmlRenderer->descriptorSetLayout
        };
        hmlRenderer->hmlPipeline = HmlPipeline::createGraphics(hmlDevice, std::move(pipelineConfig));
        if (!hmlRenderer->hmlPipeline) return { nullptr };

        const uint32_t swapchainImagesCount = 3; // TODO
        {
            const uint32_t texturesCount = 3; // TODO
            const uint32_t ubosCount = 3; // TODO
            hmlRenderer->descriptorPool = hmlRenderer->createDescriptorPool(texturesCount, ubosCount, swapchainImagesCount);
            if (!hmlRenderer->descriptorPool) return { nullptr };
        }

        hmlRenderer->descriptorSets = hmlRenderer->createDescriptorSets(swapchainImagesCount);
        if (hmlRenderer->descriptorSets.empty()) return { nullptr };

        // TODO will be moved from here
        hmlResourceManager->newUniformBuffer(swapchainImagesCount, sizeof(SimpleUniformBufferObject));
        hmlResourceManager->newTexture("girl.png");

        hmlRenderer->updateDescriptorSets(swapchainImagesCount);

        return hmlRenderer;
    }


    ~HmlRenderer() {
        std::cout << ":> Destroying HmlRenderer...\n";

        // XXX TODO depends on swapchain recreation
        // Is done here because it depends on the number of swapchain images,
        // which may change upon swapchain recreation.
        // DescriptorSets are freed automatically upon the deletion of the pool
        vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(hmlDevice->device, descriptorSetLayout, nullptr);
    }


    // TODO
    // void reassignTexture() {
    //     updateDescriptorSets(3); // TODO count
    // }


    // TODO new texture index via argument
    // XXX TODO remove direct access of HmlResourceManager members
    void updateDescriptorSets(size_t swapchainImagesCount) {
        for (size_t setIndex = 0; setIndex < swapchainImagesCount; setIndex++) {
            std::vector<VkWriteDescriptorSet> descriptorWrites;

            // Because descriptorWrites stores memory addresses, we cannot allow
            // those objects to live on stack in the inner scope.
            // The objects can be disposed of after the call to vkUpdateDescriptorSets
            std::vector<VkDescriptorBufferInfo> tmpBufferInfos;
            std::vector<VkDescriptorImageInfo> tmpImageInfos;
            for (const auto& shaderLayout : hmlShaderLayout.items) {
                switch (shaderLayout.descriptorType) {
                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
                        tmpBufferInfos.push_back(VkDescriptorBufferInfo{
                            .buffer = hmlResourceManager->uniformBuffers[setIndex], // XXX TODO
                            .offset = 0,
                            .range = sizeof(SimpleUniformBufferObject)
                        });

                        descriptorWrites.push_back(VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .pNext = nullptr,
                            .dstSet = descriptorSets[setIndex],
                            .dstBinding = shaderLayout.binding,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .pImageInfo       = nullptr,
                            .pBufferInfo      = &tmpBufferInfos.back(),
                            .pTexelBufferView = nullptr
                        });

                        break;
                    }
                    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                        tmpImageInfos.push_back(VkDescriptorImageInfo{
                            .sampler = hmlResourceManager->textureSampler, // XXX TODO
                            .imageView = hmlResourceManager->textureImageView, // XXX TODO
                            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                        });

                        descriptorWrites.push_back(VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .pNext = nullptr,
                            .dstSet = descriptorSets[setIndex],
                            .dstBinding = shaderLayout.binding,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo       = &tmpImageInfos.back(),
                            .pBufferInfo      = nullptr,
                            .pTexelBufferView = nullptr
                        });
                        break;
                    }
                    default:
                        std::cerr << "::> Unexpected descriptorType in updateDescriptorSets(): " << shaderLayout.descriptorType << ".\n";
                        return;
                }
            }

            vkUpdateDescriptorSets(hmlDevice->device, static_cast<uint32_t>(descriptorWrites.size()),
                descriptorWrites.data(), 0, nullptr);
        }
    }


    std::vector<VkDescriptorSet> createDescriptorSets(size_t swapchainImagesCount) {
        std::vector<VkDescriptorSetLayout> layouts(swapchainImagesCount, descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImagesCount);
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> descriptorSets(swapchainImagesCount);
        if (vkAllocateDescriptorSets(hmlDevice->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            std::cerr << "::> Failed to allocare DescriptorSets.\n";
            return {};
        }
        return descriptorSets;
    }


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
    VkDescriptorSetLayout createDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding>&& bindings) {
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


    VkDescriptorPool createDescriptorPool(uint32_t texturesCount, uint32_t ubosCount, uint32_t swapchainImagesCount) {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = ubosCount * swapchainImagesCount;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = texturesCount * swapchainImagesCount;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = swapchainImagesCount; // TODO what is
        // Can use VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT to specify
        // if individual descriptor sets can be freed or not. Because we are not
        // going to touch them after creating, we leave flags as 0.
        poolInfo.flags = 0;

        VkDescriptorPool descriptorPool;
        if (vkCreateDescriptorPool(hmlDevice->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            std::cerr << "::> Failed to create DescriptorPool.\n";
            return VK_NULL_HANDLE;
        }
        return descriptorPool;
    }
};

#endif
