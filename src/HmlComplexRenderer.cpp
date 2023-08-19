#include "HmlComplexRenderer.h"


HmlComplexRenderer::Entity::Id HmlComplexRenderer::Entity::nextFreeId = HmlComplexRenderer::Entity::Id{} + 1;


std::vector<std::unique_ptr<HmlPipeline>> HmlComplexRenderer::createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    std::vector<std::unique_ptr<HmlPipeline>> pipelines;

    // NOTE we drain entitiesToRenderForModelForHmlAttributes into entitiesToRenderForModelForPipelineId
    // NOTE Unique hmlAttributes implies unique HmlPipeline, so there is always a 1-1 correspondance while converting
    for (const auto& [hmlAttributes, entitiesToRenderForModel] : entitiesToRenderForModelForHmlAttributes) {
        HmlGraphicsPipelineConfig config{
            .bindingDescriptions   = hmlAttributes.bindingDescriptions,
            .attributeDescriptions = hmlAttributes.attributeDescriptions,
            .topology = hmlAttributes.topology,
            .hmlShaders = ShaderManager::generateComplexFor(hmlAttributes),
            .renderPass = hmlRenderPass->renderPass,
            .extent = hmlRenderPass->extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            // .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayouts = descriptorSetLayouts,
            .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pushConstantsSizeBytes = sizeof(PushConstantRegular),
            .tessellationPatchPoints = 0,
            .lineWidth = 1.0f,
            .colorAttachmentCount = hmlRenderPass->colorAttachmentCount,
            .withBlending = false,
            .withDepthTest = true,
        };

        pipelines.push_back(HmlPipeline::createGraphics(hmlContext->hmlDevice, std::move(config)));
        entitiesToRenderForModelForPipelineId[pipelines.back()->id] = std::move(entitiesToRenderForModel);
    }

    return pipelines;
}


std::unique_ptr<HmlComplexRenderer> HmlComplexRenderer::create(
        std::shared_ptr<HmlContext> hmlContext,
        VkDescriptorSetLayout generalDescriptorSetLayout) noexcept {
    auto hmlRenderer = std::make_unique<HmlComplexRenderer>();
    hmlRenderer->hmlContext = hmlContext;

    const auto imageCount = hmlContext->imageCount();
    const auto texturesInMaterial = HmlMaterial::PlaceCount;

    hmlRenderer->descriptorPool = hmlContext->hmlDescriptors->buildDescriptorPool()
        .withTextures(MAX_MODELS * texturesInMaterial)
        .withStorageBuffers(imageCount)
        .maxDescriptorSets(1 + imageCount)
        .build(hmlContext->hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    {
        hmlRenderer->descriptorSetLayouts.push_back(generalDescriptorSetLayout);
    }

    {
        hmlRenderer->descriptorSetLayoutMaterial = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
            .withTextureArrayAt(0, VK_SHADER_STAGE_FRAGMENT_BIT, texturesInMaterial)
            .build(hmlContext->hmlDevice);
        if (!hmlRenderer->descriptorSetLayoutMaterial) return { nullptr };
        hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutMaterial);
    }

    {
        // hmlRenderer->descriptorSetLayoutTextures = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
        //     .withTextureArrayAt(0, VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURES_COUNT)
        //     .build(hmlContext->hmlDevice);
        // if (!hmlRenderer->descriptorSetLayoutTextures) return { nullptr };
        // hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutTextures);

        // hmlRenderer->descriptorSet_textures_1 = hmlContext->hmlDescriptors->createDescriptorSets(1,
        //     hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool)[0];
        // if (!hmlRenderer->descriptorSet_textures_1) return { nullptr };
    }

    {

        // hmlRenderer->descriptorSetLayoutInstances = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
        //     .withStorageBufferAt(0, VK_SHADER_STAGE_VERTEX_BIT)
        //     .withTextureArrayAt(1, VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURES_COUNT)
        //     .build(hmlContext->hmlDevice);
        // if (!hmlRenderer->descriptorSetLayoutInstances) return { nullptr };
        // hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutInstances);
        //
        // auto descriptorSets = hmlContext->hmlDescriptors->createDescriptorSets(imageCount,
        //     hmlRenderer->descriptorSetLayoutInstances, hmlRenderer->descriptorPool);
        // // TODO WTF??
        // // hmlRenderer->descriptorSet_instances_2_queue = std::queue<VkDescriptorSet>(std::move(descriptorSets));
        // // hmlRenderer->descriptorSet_instances_2_queue = std::queue<VkDescriptorSet>(descriptorSets.begin(), descriptorSets.end());
        // for (VkDescriptorSet set : descriptorSets) hmlRenderer->descriptorSet_instances_2_queue.push(set);
        // if (hmlRenderer->descriptorSet_instances_2_queue.empty()) return { nullptr };
    }

    return hmlRenderer;
}


HmlComplexRenderer::~HmlComplexRenderer() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlComplexRenderer...\n";
#endif

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlContext->hmlDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, descriptorSetLayoutMaterial, nullptr);
    // vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, descriptorSetLayoutTextures, nullptr);
    // vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, descriptorSetLayoutInstances, nullptr);
}


void HmlComplexRenderer::specifyEntitiesToRender(std::span<const std::shared_ptr<Entity>> entities) noexcept {
    entitiesToRenderForModelForHmlAttributes.clear();
    entitiesToRenderForModelForPipelineId.clear(); // NOTE can't clear this in createPipelines() because that func is called per render pass

    // ======== Sort entities into buckets by model by pipeline

    hmlMaterialForModel.clear();
    textureDescriptorSetForModel.clear();
    for (const auto& entity : entities) {
        const auto& model = entity->complexModelResource;
        const auto id = model->id;
        const auto& hmlAttributes = model->hmlAttributes;

        entitiesToRenderForModelForHmlAttributes[hmlAttributes][id].push_back(entity);

        textureDescriptorSetForModel[id] = VkDescriptorSet{}; // just to make an entry in the map
        hmlMaterialForModel[id] = model->hmlMaterial;
    }

    // ======== Allocate new descriptor sets

    const auto modelsCount = textureDescriptorSetForModel.size();
    assert(modelsCount <= MAX_MODELS &&
       "::> There are more models than we've allocated textures in descriptor pool for.");
    if (allocatedTextureDescriptorSets.size() < modelsCount) {
        const auto count = modelsCount - allocatedTextureDescriptorSets.size();
        auto newDescriptorSets = hmlContext->hmlDescriptors->createDescriptorSets(count, descriptorSetLayoutMaterial, descriptorPool);
        allocatedTextureDescriptorSets.insert(allocatedTextureDescriptorSets.cend(), newDescriptorSets.begin(), newDescriptorSets.end());
    }
    size_t i = 0;
    for (auto& [_model, descriptorSet] : textureDescriptorSetForModel) {
        descriptorSet = allocatedTextureDescriptorSets[i++];
    }

    // ======== Update descriptor sets

    // TODO @SPEED Could probably batch up all writes to vkUpdateDescriptorSets(right now is done per model)
    std::array<VkSampler,   HmlMaterial::PlaceCount> samplers;
    std::array<VkImageView, HmlMaterial::PlaceCount> imageViews;
    const auto dummySampler = hmlContext->hmlResourceManager->dummyTextureResource->sampler;
    const auto dummyView    = hmlContext->hmlResourceManager->dummyTextureResource->view;
    for (const auto& [modelId, hmlMaterial] : hmlMaterialForModel) {
        assert(HmlMaterial::PlaceCount == hmlMaterial.textures.size());
        for (size_t i = 0; i < hmlMaterial.textures.size(); i++) {
            const auto& hmlImageResource = hmlMaterial.textures[i];
            samplers  [i] = hmlImageResource ? hmlImageResource->sampler : dummySampler;
            imageViews[i] = hmlImageResource ? hmlImageResource->view    : dummyView;
        }
        HmlDescriptorSetUpdater(textureDescriptorSetForModel[modelId])
            .textureArrayAt(0, samplers, imageViews)
            .update(hmlContext->hmlDevice);
    }
}


VkCommandBuffer HmlComplexRenderer::draw(const HmlFrameData& frameData) noexcept {
    auto commandBuffer = getCurrentCommands()[frameData.frameInFlightIndex];
    const auto inheritanceInfo = VkCommandBufferInheritanceInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = VK_NULL_HANDLE,
        .renderPass = currentRenderPass->renderPass,
        .subpass = 0, // we only have a single one
        .framebuffer = currentRenderPass->framebuffers[frameData.swapchainImageIndex],
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = static_cast<VkQueryControlFlags>(0),
        .pipelineStatistics = static_cast<VkQueryPipelineStatisticFlags>(0)
    };
    hmlContext->hmlCommands->beginRecordingSecondaryOnetime(commandBuffer, &inheritanceInfo);

#if USE_DEBUG_LABELS
    hml::DebugLabel debugLabel(commandBuffer, "Complex Entities");
#endif
#if USE_TIMESTAMP_QUERIES
    // if (mode == Mode::Regular) {
        hmlContext->hmlQueries->registerEvent("HmlComplexRenderer: begin regular entities", "CEw", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    // } else {
    //     hmlContext->hmlQueries->registerEvent("HmlComplexRenderer: begin regular entities", "CEw(s)", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    // }
#endif

    for (const auto& hmlPipeline : getCurrentPipelines()) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

        const auto& entitiesToRenderForModel = entitiesToRenderForModelForPipelineId[hmlPipeline->id];
        for (const auto& [modelId, entities] : entitiesToRenderForModel) {
            // NOTE We know there is at least 1 such entity (ensured by logic of specifyEntitiesToRender)
            const auto& model = entities[0]->complexModelResource;

            std::array<VkDescriptorSet, 2> descriptorSets = {
                frameData.generalDescriptorSet_0, textureDescriptorSetForModel[modelId]
            };
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

            const auto bindingCount = model->vertexBufferViews.size();
            std::vector<VkBuffer> vertexBuffers;
            std::vector<VkDeviceSize> vertexBufferOffsets;
            vertexBuffers.reserve(bindingCount);
            vertexBufferOffsets.reserve(bindingCount);
            for (const auto& hmlBufferView : model->vertexBufferViews) {
                assert(hmlBufferView.hmlBuffer);
                assert(hmlBufferView.hmlBuffer->buffer);
                vertexBuffers.push_back(hmlBufferView.hmlBuffer->buffer);
                vertexBufferOffsets.push_back(hmlBufferView.offset);
            }

            // NOTE advanced usage would be to have a single VkBuffer for both
            // VertexBuffer and IndexBuffer and specify different offsets into it.
            // Second and third arguments specify the offset and number of bindings
            // we're going to specify VertexBuffers for.
            const uint32_t firstBinding = 0;
            vkCmdBindVertexBuffers(commandBuffer, firstBinding, bindingCount, vertexBuffers.data(), vertexBufferOffsets.data());
            const auto indexBuffer = model->indexBufferView.hmlBuffer->buffer;
            const auto indexBufferOffset = model->indexBufferView.offset;
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, indexBufferOffset, model->indicesCount.type); // 0 is offset

            const auto& hmlMaterial = hmlMaterialForModel[modelId];
            static const auto indexForPlace = [](HmlMaterial::TexturePlace place, const HmlMaterial& hmlMaterial){
                return hmlMaterial.textures[place] ? place : NO_TEXTURE_MARK;
            };
            for (const auto& entity : entities) {
                // NOTE Yes, in theory having a textureIndex per Entity is redundant
                PushConstantRegular pushConstant{
                    .model = entity->modelMatrix,
                    .color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
                    .baseColorTextureIndex = indexForPlace(HmlMaterial::PlaceBaseColor, hmlMaterial),
                    .metallicRoughnessTextureIndex = indexForPlace(HmlMaterial::PlaceMetallicRoughness, hmlMaterial),
                    .normalTextureIndex = withNormals ? indexForPlace(HmlMaterial::PlaceNormal, hmlMaterial) : NO_TEXTURE_MARK,
                    .occlusionTextureIndex = withAO ? indexForPlace(HmlMaterial::PlaceOcclusion, hmlMaterial) : NO_TEXTURE_MARK,
                    .emissiveTextureIndex = withEmissive ? indexForPlace(HmlMaterial::PlaceEmissive, hmlMaterial) : NO_TEXTURE_MARK,
                    .id = entity->id,
                };
                vkCmdPushConstants(commandBuffer, hmlPipeline->layout,
                    hmlPipeline->pushConstantsStages, 0, sizeof(PushConstantRegular), &pushConstant);

                // NOTE could use firstInstance to supply a single integer to gl_BaseInstance
                const uint32_t instanceCount = 1;
                const uint32_t firstInstance = 0;
                const uint32_t firstIndex = 0;
                const uint32_t offsetToAddToIndices = 0;
                // const uint32_t vertexCount = vertices.size();
                // const uint32_t firstVertex = 0;
                // vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
                vkCmdDrawIndexed(commandBuffer, model->indicesCount.count,
                    instanceCount, firstIndex, offsetToAddToIndices, firstInstance);
            }
        }
    }

#if USE_TIMESTAMP_QUERIES
    // if (mode == Mode::Regular) {
        hmlContext->hmlQueries->registerEvent("HmlComplexRenderer: end regular entities", "CE", commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    // } else {
    //     hmlContext->hmlQueries->registerEvent("HmlComplexRenderer: end regular entities", "CE(s)", commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    // }
#endif

    hmlContext->hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}
