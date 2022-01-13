#ifndef HML_RENDERER
#define HML_RENDERER

#include <memory>
#include <vector>
#include <unordered_map>
#include <optional>

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
    uint32_t arrayLength;
};

struct HmlShaderLayout {
    // For each set number
    std::vector<std::vector<HmlShaderLayoutItem>> items;

    HmlShaderLayout& addUniformBuffer(uint32_t set, uint32_t binding, VkShaderStageFlags stages) {
        prepareSetIfAbsent(set);
        items[set].push_back(HmlShaderLayoutItem{
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .binding = binding,
            .stages = stages,
            .arrayLength = 1
        });
        return *this;
    }

    HmlShaderLayout& addTexture(uint32_t set, uint32_t binding, VkShaderStageFlags stages) {
        prepareSetIfAbsent(set);
        items[set].push_back(HmlShaderLayoutItem{
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .binding = binding,
            .stages = stages,
            .arrayLength = 1
        });
        return *this;
    }

    HmlShaderLayout& addTextureArray(uint32_t set, uint32_t binding, VkShaderStageFlags stages, uint32_t arrayLength) {
        prepareSetIfAbsent(set);
        items[set].push_back(HmlShaderLayoutItem{
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .binding = binding,
            .stages = stages,
            .arrayLength = arrayLength
        });
        return *this;
    }

    std::vector<VkDescriptorSetLayoutBinding> layoutBindingsForSet(uint32_t set) {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (const auto& shaderLayout : items[set]) {
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = shaderLayout.binding,
                .descriptorType = shaderLayout.descriptorType,
                .descriptorCount = shaderLayout.arrayLength,
                .stageFlags = shaderLayout.stages,
                .pImmutableSamplers = nullptr
            });
        }

        return bindings;
    }

    size_t setsCount() const noexcept {
        return items.size();
    }

    private:
    void prepareSetIfAbsent(uint32_t set) {
        while (items.size() <= set) items.push_back({});
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
        int32_t textureIndex;
    };


    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;

    std::unique_ptr<HmlPipeline> hmlPipeline;
    HmlShaderLayout hmlShaderLayout;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSet_0_perFrame;
    VkDescriptorSet              descriptorSet_1;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;


    static constexpr uint32_t MAX_TEXTURES_COUNT = 32; // XXX must match the shader. NOTE can be increased (80+)
    static constexpr int32_t NO_TEXTURE_MARK = -1;
    uint32_t nextFreeTextureIndex;
    std::shared_ptr<HmlModelResource> anyModelWithTexture;
    std::unordered_map<HmlModelResource::Id, int32_t> textureIndexFor;
    std::unordered_map<HmlModelResource::Id, std::vector<std::shared_ptr<HmlSimpleEntity>>> entitiesToRenderForModel;


    // TODO where to store them??
    // Cleaned automatically upon CommandPool destruction
    std::vector<VkCommandBuffer> commandBuffers;


    static std::unique_ptr<HmlPipeline> createSimplePipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
            VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) {
        HmlGraphicsPipelineConfig config{
            .bindingDescriptions   = HmlSimpleModel::Vertex::getBindingDescriptions(),
            .attributeDescriptions = HmlSimpleModel::Vertex::getAttributeDescriptions(),
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .hmlShaders = HmlShaders()
                .addVertex("shaders/out/simple.vert.spv")
                .addFragment("shaders/out/simple.frag.spv"),
            .renderPass = renderPass,
            .swapchainExtent = extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            // .cullMode = VK_CULL_MODE_BACK_BIT,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayouts = descriptorSetLayouts,
            .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
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
            std::shared_ptr<HmlResourceManager> hmlResourceManager,
            uint32_t framesInFlight) {
        auto hmlRenderer = std::make_unique<HmlRenderer>();
        hmlRenderer->hmlWindow = hmlWindow;
        hmlRenderer->hmlDevice = hmlDevice;
        hmlRenderer->hmlCommands = hmlCommands;
        hmlRenderer->hmlSwapchain = hmlSwapchain;
        hmlRenderer->hmlResourceManager = hmlResourceManager;


        hmlRenderer->hmlShaderLayout = HmlShaderLayout()
            .addUniformBuffer(0, 0, VK_SHADER_STAGE_VERTEX_BIT)
            .addTextureArray(1, 0, VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURES_COUNT);

        for (size_t set = 0; set < hmlRenderer->hmlShaderLayout.setsCount(); set++) {
            auto bindings = hmlRenderer->hmlShaderLayout.layoutBindingsForSet(set);
            const auto layout = hmlRenderer->createDescriptorSetLayout(std::move(bindings));
            if (!layout) return { nullptr };
            hmlRenderer->descriptorSetLayouts.push_back(layout);
        }

        // TODO Move further after descriptors
        hmlRenderer->hmlPipeline = createSimplePipeline(hmlDevice,
            hmlSwapchain->extent, hmlSwapchain->renderPass, hmlRenderer->descriptorSetLayouts);
        if (!hmlRenderer->hmlPipeline) return { nullptr };

        /*
         * We have a UBO for each frame in flight, so we need a descriptor set
         * bound to 0 for each UBO. It is bound once per frame.
         * We have a texture for each model at 1. We canot have a descriptor set
         * for each texture resource, so we have a single set whose contents are
         * updated per model binding.
         */
        const uint32_t descriptorSetCount = framesInFlight + 1;

        {
            hmlRenderer->descriptorPool = hmlRenderer->createDescriptorPool(MAX_TEXTURES_COUNT, framesInFlight, descriptorSetCount);
            if (!hmlRenderer->descriptorPool) return { nullptr };
        }

        hmlRenderer->descriptorSet_0_perFrame = hmlRenderer->createDescriptorSets(framesInFlight, hmlRenderer->descriptorSetLayouts[0]);
        if (hmlRenderer->descriptorSet_0_perFrame.empty()) return { nullptr };
        hmlRenderer->descriptorSet_1 = hmlRenderer->createDescriptorSets(1, hmlRenderer->descriptorSetLayouts[1])[0];
        if (!hmlRenderer->descriptorSet_1) return { nullptr };

        hmlRenderer->commandBuffers = hmlCommands->allocate(framesInFlight, hmlCommands->commandPoolOnetimeFrames);

        return hmlRenderer;
    }


    ~HmlRenderer() {
        std::cout << ":> Destroying HmlRenderer...\n";

        // TODO depends on swapchain recreation, but because it only depends on the
        // TODO number of images, which most likely will not change, we ignore it.
        // DescriptorSets are freed automatically upon the deletion of the pool
        vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
        for (auto layout : descriptorSetLayouts) {
            vkDestroyDescriptorSetLayout(hmlDevice->device, layout, nullptr);
        }
    }


    void specifyEntitiesToRender(const std::vector<std::shared_ptr<HmlSimpleEntity>>& entities) {
        nextFreeTextureIndex = 0;
        textureIndexFor.clear();
        entitiesToRenderForModel.clear();

        for (const auto& entity : entities) {
            const auto& model = entity->modelResource;
            const auto id = model->id;
            if (!textureIndexFor.contains(id)) {
                entitiesToRenderForModel[id] = {};
                if (model->hasTexture) {
                    if (!anyModelWithTexture) anyModelWithTexture = model;
                    textureIndexFor[id] = nextFreeTextureIndex++;
                } else {
                    textureIndexFor[id] = NO_TEXTURE_MARK;
                }
            }
            entitiesToRenderForModel[id].push_back(entity);
        }

        updateDescriptorSetTextures();
    }


    void updateDescriptorSetGeneral(uint32_t frameIndex) {
        VkDescriptorBufferInfo bufferInfo{
            .buffer = hmlResourceManager->uniformBuffers[frameIndex], // XXX TODO
            .offset = 0,
            .range = sizeof(SimpleUniformBufferObject)
        };
        VkWriteDescriptorSet descriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet_0_perFrame[frameIndex],
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


    void updateDescriptorSetTextures() {
        static bool mustDefaultInit = true;
        const auto usedTexturesCount = mustDefaultInit ? MAX_TEXTURES_COUNT : nextFreeTextureIndex;
        std::vector<VkDescriptorImageInfo> imageInfos(usedTexturesCount);

        // NOTE Is done only because the validation layer complains that corresponding
        // textures are not set, although supposedly being used in shared. In
        // reality, however, the logic is solid and they are not used.
        if (mustDefaultInit) {
            for (uint32_t i = nextFreeTextureIndex; i < MAX_TEXTURES_COUNT; i++) {
                imageInfos[i] = VkDescriptorImageInfo{
                    .sampler = anyModelWithTexture->textureSampler,
                        .imageView = anyModelWithTexture->textureImageView,
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
            }
            mustDefaultInit = false;
        }

        // NOTE textureIndex always covers range [0..usedTexturesCount)
        for (const auto& [modelId, textureIndex] : textureIndexFor) {
            // Pick any Model with this texture
            const auto& model = entitiesToRenderForModel[modelId][0]->modelResource;
            if (!model->hasTexture) continue;
            imageInfos[textureIndex] = VkDescriptorImageInfo{
                .sampler = model->textureSampler,
                .imageView = model->textureImageView,
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            };
        }

        VkWriteDescriptorSet descriptorWrite{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = descriptorSet_1,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = static_cast<uint32_t>(usedTexturesCount),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo       = imageInfos.data(),
            .pBufferInfo      = nullptr,
            .pTexelBufferView = nullptr
        };

        vkUpdateDescriptorSets(hmlDevice->device, 1, &descriptorWrite, 0, nullptr);
    }


    // void updateDescriptorSets() {
    //     for (size_t setIndex = 0; setIndex < hmlSwapchain->imagesCount(); setIndex++) {
    //         std::vector<VkWriteDescriptorSet> descriptorWrites;
    //
    //         // Because descriptorWrites stores memory addresses, we cannot allow
    //         // those objects to live on stack in the inner scope.
    //         // The objects can be disposed of after the call to vkUpdateDescriptorSets
    //         std::vector<VkDescriptorBufferInfo> tmpBufferInfos;
    //         std::vector<VkDescriptorImageInfo> tmpImageInfos;
    //         for (const auto& shaderLayout : hmlShaderLayout.items) {
    //             switch (shaderLayout.descriptorType) {
    //                 case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
    //                     tmpBufferInfos.push_back(VkDescriptorBufferInfo{
    //                         .buffer = hmlResourceManager->uniformBuffers[setIndex], // XXX TODO
    //                         .offset = 0,
    //                         .range = sizeof(SimpleUniformBufferObject)
    //                     });
    //
    //                     descriptorWrites.push_back(VkWriteDescriptorSet{
    //                         .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    //                         .pNext = nullptr,
    //                         .dstSet = descriptorSets[setIndex],
    //                         .dstBinding = shaderLayout.binding,
    //                         .dstArrayElement = 0,
    //                         .descriptorCount = shaderLayout.arrayLength, // XXX
    //                         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    //                         .pImageInfo       = nullptr,
    //                         .pBufferInfo      = &tmpBufferInfos.back(),
    //                         .pTexelBufferView = nullptr
    //                     });
    //
    //                     break;
    //                 }
    //                 case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
    //                     tmpImageInfos.push_back(VkDescriptorImageInfo{
    //                         .sampler = hmlResourceManager->textureSampler, // XXX TODO
    //                         .imageView = hmlResourceManager->textureImageView, // XXX TODO
    //                         .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    //                     });
    //
    //                     descriptorWrites.push_back(VkWriteDescriptorSet{
    //                         .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    //                         .pNext = nullptr,
    //                         .dstSet = descriptorSets[setIndex],
    //                         .dstBinding = shaderLayout.binding,
    //                         .dstArrayElement = 0,
    //                         .descriptorCount = shaderLayout.arrayLength, // XXX
    //                         .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    //                         .pImageInfo       = &tmpImageInfos.back(),
    //                         .pBufferInfo      = nullptr,
    //                         .pTexelBufferView = nullptr
    //                     });
    //                     break;
    //                 }
    //                 default:
    //                     std::cerr << "::> Unexpected descriptorType in updateDescriptorSets(): " << shaderLayout.descriptorType << ".\n";
    //                     return;
    //             }
    //         }
    //
    //         vkUpdateDescriptorSets(hmlDevice->device, static_cast<uint32_t>(descriptorWrites.size()),
    //             descriptorWrites.data(), 0, nullptr);
    //     }
    // }


    VkCommandBuffer draw(uint32_t frameIndex, uint32_t imageIndex) {
        auto commandBuffer = commandBuffers[frameIndex];

        // Must be done prior to recording
        updateDescriptorSetGeneral(frameIndex);

        hmlCommands->beginRecording(commandBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);


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
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);
        {
            const auto setIndex = 0;
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                hmlPipeline->layout, setIndex, 1, &descriptorSet_0_perFrame[frameIndex], 0, nullptr);
        }
        {
            const auto setIndex = 1;
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                hmlPipeline->layout, setIndex, 1, &descriptorSet_1, 0, nullptr);
        }


        for (const auto& [modelId, entities] : entitiesToRenderForModel) {
            // NOTE We know there is at least 1 such entity (ensured by login of specifyEntitiesToRender)
            const auto& model = entities[0]->modelResource;

            VkBuffer vertexBuffers[] = { model->vertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            // NOTE advanced usage world have a single VkBuffer for both
            // VertexBuffer and IndexBuffer and specify different offsets into it.
            // Second and third arguments specify the offset and number of bindings
            // we're going to specify VertexBuffers for.
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            // XXX Indices type is specified here:
            vkCmdBindIndexBuffer(commandBuffer, model->indexBuffer, 0, VK_INDEX_TYPE_UINT32); // 0 is offset

            for (const auto& entity : entities) {
                // NOTE Yes, in theory having a textureIndex per Entity is redundant
                SimplePushConstant pushConstant{
                    .model = entity->modelMatrix,
                    .textureIndex = model->hasTexture ? textureIndexFor[modelId] : NO_TEXTURE_MARK
                };
                vkCmdPushConstants(commandBuffer, hmlPipeline->layout,
                    hmlPipeline->pushConstantsStages, 0, sizeof(SimplePushConstant), &pushConstant);

                // NOTE could use firstInstance to supply a single integer to gl_BaseInstance
                const uint32_t instanceCount = 1;
                const uint32_t firstInstance = 0;
                const uint32_t firstIndex = 0;
                const uint32_t offsetToAddToIndices = 0;
                // const uint32_t vertexCount = vertices.size();
                // const uint32_t firstVertex = 0;
                // vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
                vkCmdDrawIndexed(commandBuffer, model->indicesCount,
                    instanceCount, firstIndex, offsetToAddToIndices, firstInstance);
            }
        }

        vkCmdEndRenderPass(commandBuffer);

        hmlCommands->endRecording(commandBuffer);

        return commandBuffer;
    }


    std::vector<VkDescriptorSet> createDescriptorSets(uint32_t count, VkDescriptorSetLayout layout) {
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


    VkDescriptorPool createDescriptorPool(uint32_t totalTextureCount, uint32_t framesInFlight, uint32_t descriptorSetCount) {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = framesInFlight;
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = MAX_TEXTURES_COUNT;

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


    // TODO in order for each type of Renderer to properly replace its pipeline,
    // store a member in Renderer which specifies its type, and recreate the pipeline
    // based on its value.
    void replaceSwapchain(std::shared_ptr<HmlSwapchain> newHmlSwapChain) {
        hmlSwapchain = newHmlSwapChain;
        hmlPipeline = createSimplePipeline(hmlDevice, hmlSwapchain->extent, hmlSwapchain->renderPass, descriptorSetLayouts);
        // NOTE The command pool is reset for all renderers prior to calling this function.
        // NOTE commandBuffers must be rerecorded -- is done during baking
    }
};

#endif
