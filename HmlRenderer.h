#ifndef HML_RENDERER
#define HML_RENDERER

#include <memory>
#include <vector>

#include "HmlWindow.h"
#include "HmlPipeline.h"
#include "HmlSwapchain.h"
#include "HmlCommands.h"
#include "HmlModel.h"
#include "HmlResourceManager.h"


struct HmlSimpleEntity {
    // NOTE We prefer to access the modelResource via a pointer rather than by
    // id because it allows for O(1) access speed.
    std::shared_ptr<HmlModelResource> modelResource;
    // HmlResourceManager::HmlModelResource::Id modelId;
    glm::mat4 modelMatrix;


    HmlSimpleEntity(std::shared_ptr<HmlModelResource> modelResource)
        : modelResource(modelResource) {}
    HmlSimpleEntity(std::shared_ptr<HmlModelResource> modelResource, const glm::mat4& modelMatrix)
        : modelResource(modelResource), modelMatrix(modelMatrix) {}
};


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


// TODO
// In theory, it is easily possible to also recreate desriptor stuff upon
// swapchain recreation by recreating the whole Renderer object.
// TODO
struct HmlRenderer {
    struct SimpleUniformBufferObject {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };


    struct SimplePushConstant {
        alignas(16) glm::mat4 model;
    };


    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;

    std::unique_ptr<HmlPipeline> hmlPipeline;
    HmlShaderLayout hmlShaderLayout;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets; // per swapChainImage
    VkDescriptorSetLayout descriptorSetLayout;


    std::vector<std::shared_ptr<HmlSimpleEntity>> entitiesToRender;


    // TODO where to store them??
    // Cleaned automatically upon CommandPool destruction
    std::vector<VkCommandBuffer> commandBuffers;


    static std::unique_ptr<HmlPipeline> createSimplePipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
            VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout) {
        HmlGraphicsPipelineConfig config{
            .bindingDescriptions   = HmlSimpleModel::Vertex::getBindingDescriptions(),
            .attributeDescriptions = HmlSimpleModel::Vertex::getAttributeDescriptions(),
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .hmlShaders = HmlShaders()
                .addVertex("vertex.spv")
                .addFragment("fragment.spv"), // TODO rename to "simple"
            .renderPass = renderPass,
            .swapchainExtent = extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayout = descriptorSetLayout,
            .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT,
            .pushConstantsSizeBytes = sizeof(SimplePushConstant)
        };

        auto hmlPipeline = HmlPipeline::createGraphics(hmlDevice, std::move(config));
        return hmlPipeline;
    }


    static std::unique_ptr<HmlRenderer> createSimpleRenderer(
            std::shared_ptr<HmlWindow> hmlWindow,
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlCommands> hmlCommands,
            std::shared_ptr<HmlSwapchain> hmlSwapchain,
            std::shared_ptr<HmlResourceManager> hmlResourceManager) {
        auto hmlRenderer = std::make_unique<HmlRenderer>();
        hmlRenderer->hmlWindow = hmlWindow;
        hmlRenderer->hmlDevice = hmlDevice;
        hmlRenderer->hmlCommands = hmlCommands;
        hmlRenderer->hmlSwapchain = hmlSwapchain;
        hmlRenderer->hmlResourceManager = hmlResourceManager;


        hmlRenderer->hmlShaderLayout = HmlShaderLayout()
            .addUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT)
            .addTexture(1, VK_SHADER_STAGE_FRAGMENT_BIT);

        hmlRenderer->descriptorSetLayout = hmlRenderer->createDescriptorSetLayout(hmlRenderer->hmlShaderLayout.createLayoutBindings());
        if (!hmlRenderer->descriptorSetLayout) return { nullptr };

        hmlRenderer->hmlPipeline = createSimplePipeline(hmlDevice,
            hmlSwapchain->extent, hmlSwapchain->renderPass, hmlRenderer->descriptorSetLayout);
        if (!hmlRenderer->hmlPipeline) return { nullptr };

        {
            const uint32_t texturesCount = 3; // TODO
            const uint32_t ubosCount = 3; // TODO
            hmlRenderer->descriptorPool = hmlRenderer->createDescriptorPool(texturesCount, ubosCount);
            if (!hmlRenderer->descriptorPool) return { nullptr };
        }

        hmlRenderer->descriptorSets = hmlRenderer->createDescriptorSets();
        if (hmlRenderer->descriptorSets.empty()) return { nullptr };

        hmlRenderer->commandBuffers = hmlCommands->allocate(hmlSwapchain->imagesCount(), hmlCommands->commandPoolOnetimeFrames);

        // TODO will be moved from here
        hmlResourceManager->newUniformBuffer(hmlSwapchain->imagesCount(), sizeof(SimpleUniformBufferObject));
        hmlResourceManager->newTexture("viking_room.png");

        hmlRenderer->updateDescriptorSets();

        return hmlRenderer;
    }


    ~HmlRenderer() {
        std::cout << ":> Destroying HmlRenderer...\n";

        // TODO depends on swapchain recreation, but because it only depends on the
        // TODO number of images, which most likely will not change, we ignore it.
        // DescriptorSets are freed automatically upon the deletion of the pool
        vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(hmlDevice->device, descriptorSetLayout, nullptr);
    }


    // TODO
    // void reassignTexture() {
    //     updateDescriptorSets(3);
    // }


    void specifyEntitiesToRender(const std::vector<std::shared_ptr<HmlSimpleEntity>>& entities) {
        entitiesToRender = entities;
    }


    // TODO new texture index via argument
    // XXX TODO remove direct access of HmlResourceManager members
    void updateDescriptorSets() {
        for (size_t setIndex = 0; setIndex < hmlSwapchain->imagesCount(); setIndex++) {
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


    // TODO optimization: sort by model and bind each model only once
    VkCommandBuffer* draw(uint32_t imageIndex) {
        hmlCommands->beginRecording(commandBuffers[imageIndex], VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);


        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = hmlSwapchain->renderPass;
        renderPassInfo.framebuffer = hmlSwapchain->framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = hmlSwapchain->extent;

        // Used for VK_ATTACHMENT_LOAD_OP_CLEAR
        std::array<VkClearValue, 2> clearValues{};
        // The order (indexing) must be the same as in attachments!
        clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0}; // 1.0 is farthest
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        // Last argument to vkCmdBeginRenderPass:
        // VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the
        // primary command buffer itself and no secondary command buffers will be executed;
        // VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: The render pass commands
        // will be executed from secondary command buffers.
        vkCmdBeginRenderPass(commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

        // Descriptors are not unique to graphics pipelines, unlike Vertex
        // and Index buffers, so we specify where to bind: graphics or compute
        vkCmdBindDescriptorSets(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS,
                hmlPipeline->layout, 0, 1, &descriptorSets[imageIndex], 0, nullptr);

        for (const auto& entity : entitiesToRender) {
            const auto& model = entity->modelResource;

            VkBuffer vertexBuffers[] = { model->vertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            // NOTE advanced usage world have a single VkBuffer for both
            // VertexBuffer and IndexBuffer and specify different offsets into it.
            // Second and third arguments specify the offset and number of bindings
            // we're going to specify VertexBuffers for.
            vkCmdBindVertexBuffers(commandBuffers[imageIndex], 0, 1, vertexBuffers, offsets);
            // XXX Indices type is specified here:
            vkCmdBindIndexBuffer(commandBuffers[imageIndex], model->indexBuffer, 0, VK_INDEX_TYPE_UINT32); // 0 is offset

            SimplePushConstant pushConstant{
                .model = entity->modelMatrix
            };
            vkCmdPushConstants(commandBuffers[imageIndex], hmlPipeline->layout,
                hmlPipeline->pushConstantsStages, 0, sizeof(SimplePushConstant), &pushConstant);

            // NOTE could use firstInstance to supply a single integer to gl_BaseInstance
            const uint32_t instanceCount = 1;
            const uint32_t firstInstance = 0;
            const uint32_t firstIndex = 0;
            const uint32_t offsetToAddToIndices = 0;
            // const uint32_t vertexCount = vertices.size();
            // const uint32_t firstVertex = 0;
            // vkCmdDraw(commandBuffers[imageIndex], vertexCount, instanceCount, firstVertex, firstInstance);
            vkCmdDrawIndexed(commandBuffers[imageIndex], model->indicesCount,
                instanceCount, firstIndex, offsetToAddToIndices, firstInstance);
        }

        vkCmdEndRenderPass(commandBuffers[imageIndex]);

        hmlCommands->endRecording(commandBuffers[imageIndex]);

        return &commandBuffers[imageIndex];
    }


    // void bakeCommandBuffers() {
    //     for (size_t i = 0; i < hmlSwapchain->imagesCount(); i++) {
    //     }
    // }


    std::vector<VkDescriptorSet> createDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(hmlSwapchain->imagesCount(), descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(hmlSwapchain->imagesCount());
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> descriptorSets(hmlSwapchain->imagesCount());
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


    VkDescriptorPool createDescriptorPool(uint32_t texturesCount, uint32_t ubosCount) {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = ubosCount * hmlSwapchain->imagesCount();
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = texturesCount * hmlSwapchain->imagesCount();

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = hmlSwapchain->imagesCount(); // TODO what is
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


    // TODO in order for each type of Renderer to properly replace its pipeline,
    // store a member in Renderer which specifies its type, and recreate the pipeline
    // based on its value.
    void replaceSwapchain(std::shared_ptr<HmlSwapchain> newHmlSwapChain) {
        hmlSwapchain = newHmlSwapChain;
        hmlPipeline = createSimplePipeline(hmlDevice, hmlSwapchain->extent, hmlSwapchain->renderPass, descriptorSetLayout);
        // NOTE The command pool is reset for all renderers prior to calling this function.
        // NOTE commandBuffers must be rerecorded -- is done during baking
    }
};

#endif
