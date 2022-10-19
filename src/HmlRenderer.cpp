#include "HmlRenderer.h"


std::vector<std::unique_ptr<HmlPipeline>> HmlRenderer::createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    std::vector<std::unique_ptr<HmlPipeline>> pipelines;

    { // Regular Entities
        HmlGraphicsPipelineConfig config{
            .bindingDescriptions   = HmlSimpleModel::Vertex::getBindingDescriptions(),
            .attributeDescriptions = HmlSimpleModel::Vertex::getAttributeDescriptions(),
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .hmlShaders = HmlShaders()
                .addVertex("../shaders/out/simple_deferred.vert.spv")
                .addFragment("../shaders/out/simple_deferred.frag.spv"),
            .renderPass = hmlRenderPass->renderPass,
            .extent = hmlRenderPass->extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            // .cullMode = VK_CULL_MODE_BACK_BIT,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayouts = descriptorSetLayouts,
            .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pushConstantsSizeBytes = sizeof(PushConstantRegular),
            .tessellationPatchPoints = 0,
            .lineWidth = 1.0f,
            .colorAttachmentCount = hmlRenderPass->colorAttachmentCount,
            .withBlending = true,
        };

        pipelines.push_back(HmlPipeline::createGraphics(hmlContext->hmlDevice, std::move(config)));
    }

    { // Instanced (static Entities)
        HmlGraphicsPipelineConfig config{
            .bindingDescriptions   = HmlSimpleModel::Vertex::getBindingDescriptions(),
            .attributeDescriptions = HmlSimpleModel::Vertex::getAttributeDescriptions(),
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .hmlShaders = HmlShaders()
                .addVertex("../shaders/out/simple_deferred_instance.vert.spv")
                .addFragment("../shaders/out/simple_deferred_instance.frag.spv"),
            .renderPass = hmlRenderPass->renderPass,
            .extent = hmlRenderPass->extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            // .cullMode = VK_CULL_MODE_BACK_BIT,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayouts = descriptorSetLayouts,
            .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pushConstantsSizeBytes = sizeof(PushConstantInstanced),
            .tessellationPatchPoints = 0,
            .lineWidth = 1.0f,
            .colorAttachmentCount = hmlRenderPass->colorAttachmentCount,
            .withBlending = true,
        };

        pipelines.push_back(HmlPipeline::createGraphics(hmlContext->hmlDevice, std::move(config)));
    }

    return pipelines;
}


std::unique_ptr<HmlRenderer> HmlRenderer::create(
        std::shared_ptr<HmlContext> hmlContext,
        VkDescriptorSetLayout generalDescriptorSetLayout) noexcept {
    auto hmlRenderer = std::make_unique<HmlRenderer>();
    hmlRenderer->hmlContext = hmlContext;

    const auto imageCount = hmlContext->imageCount();

    hmlRenderer->descriptorPool = hmlContext->hmlDescriptors->buildDescriptorPool()
        .withTextures(MAX_TEXTURES_COUNT + imageCount * MAX_TEXTURES_COUNT)
        .withStorageBuffers(imageCount)
        .maxSets(1 + imageCount)
        .build(hmlContext->hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    {
        hmlRenderer->descriptorSetLayouts.push_back(generalDescriptorSetLayout);
    }

    {
        hmlRenderer->descriptorSetLayoutTextures = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
            .withTextureArrayAt(0, VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURES_COUNT)
            .build(hmlContext->hmlDevice);
        if (!hmlRenderer->descriptorSetLayoutTextures) return { nullptr };
        hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutTextures);

        hmlRenderer->descriptorSet_textures_1 = hmlContext->hmlDescriptors->createDescriptorSets(1,
            hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool)[0];
        if (!hmlRenderer->descriptorSet_textures_1) return { nullptr };
    }

    {

        hmlRenderer->descriptorSetLayoutInstances = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
            .withStorageBufferAt(0, VK_SHADER_STAGE_VERTEX_BIT)
            .withTextureArrayAt(1, VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURES_COUNT)
            .build(hmlContext->hmlDevice);
        if (!hmlRenderer->descriptorSetLayoutInstances) return { nullptr };
        hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutInstances);

        auto descriptorSets = hmlContext->hmlDescriptors->createDescriptorSets(imageCount,
            hmlRenderer->descriptorSetLayoutInstances, hmlRenderer->descriptorPool);
        // TODO WTF??
        // hmlRenderer->descriptorSet_instances_2_queue = std::queue<VkDescriptorSet>(std::move(descriptorSets));
        // hmlRenderer->descriptorSet_instances_2_queue = std::queue<VkDescriptorSet>(descriptorSets.begin(), descriptorSets.end());
        for (VkDescriptorSet set : descriptorSets) hmlRenderer->descriptorSet_instances_2_queue.push(set);
        if (hmlRenderer->descriptorSet_instances_2_queue.empty()) return { nullptr };
    }

    return hmlRenderer;
}


HmlRenderer::~HmlRenderer() noexcept {
    std::cout << ":> Destroying HmlRenderer...\n";

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlContext->hmlDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, descriptorSetLayoutTextures, nullptr);
    vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, descriptorSetLayoutInstances, nullptr);
}


void HmlRenderer::specifyEntitiesToRender(std::span<const std::shared_ptr<Entity>> entities) noexcept {
    int32_t nextFreeTextureIndex = 0;
    textureIndexFor.clear();
    entitiesToRenderForModel.clear();

    for (const auto& entity : entities) {
        const auto& model = entity->modelResource;
        const auto id = model->id;
        if (textureIndexFor.find(id) == textureIndexFor.cend()) {
            if (model->textureResource) {
                textureIndexFor[id] = nextFreeTextureIndex++;
            } else {
                textureIndexFor[id] = NO_TEXTURE_MARK;
            }
        }
        // NOTE The vector will be automatically created if the entry is not present
        entitiesToRenderForModel[id].push_back(entity);
    }

    if (nextFreeTextureIndex == 0) {
        std::cerr << "::> No Entity with a texture has been specified.\n";
        exit(-1);
    }

    std::vector<EntitiesData> entitiesData;
    entitiesData.reserve(entitiesToRenderForModel.size());
    for (const auto& [modelId, entities] : entitiesToRenderForModel) {
        const auto textureIndex = textureIndexFor[modelId];
        const auto modelResource = entities[0]->modelResource;
        entitiesData.emplace_back(textureIndex, modelResource);
    }

    const auto& [samplers, imageViews] = prepareTextureUpdateData(entitiesData);
    HmlDescriptorSetUpdater(descriptorSet_textures_1)
        .textureArrayAt(0, samplers, imageViews)
        .update(hmlContext->hmlDevice);
}


void HmlRenderer::specifyStaticEntitiesToRender(std::span<const Entity> staticEntities) noexcept {
    instancedCounts.clear();
    staticEntitiesDataForModel.clear();

    // Group Entities by Model
    int32_t nextFreeStaticTextureIndex = 0;
    std::unordered_map<HmlModelResource::Id, std::vector<glm::mat4>> modelMatricesForModel;
    for (const auto& entity : staticEntities) {
        const auto& model = entity.modelResource;
        const auto id = model->id;

        if (!staticEntitiesDataForModel.contains(id)) {
            const auto textureIndex = model->textureResource ? nextFreeStaticTextureIndex++ : NO_TEXTURE_MARK;
            EntitiesData entitiesData{textureIndex, model};
            staticEntitiesDataForModel[id] = std::move(entitiesData);
        }

        // NOTE The vector will be automatically created if the entry is not present
        modelMatricesForModel[id].push_back(entity.modelMatrix);
    }


    // Gather data for SSBO:
    // -- flatten all vectors of model matrices;
    // -- remember sizes of those vectors for rendering;
    std::vector<glm::mat4> instancesData;
    instancesData.reserve(staticEntities.size());
    for (const auto& [_modelId, modelMatrices] : modelMatricesForModel) {
        instancesData.insert(instancesData.end(), modelMatrices.begin(), modelMatrices.end());
        instancedCounts.push_back(modelMatrices.size());
    }


    // Replace the SSBO and write the data
    hmlContext->hmlResourceManager->markForRelease(std::move(instancedEntitiesStorageBuffer));
    const auto size = staticEntities.size() * sizeof(EntityInstanceData);
    instancedEntitiesStorageBuffer = hmlContext->hmlResourceManager->createStorageBuffer(size);
    instancedEntitiesStorageBuffer->map();
    instancedEntitiesStorageBuffer->update(instancesData.data());

    // Cycle the queue and update the new head
    descriptorSet_instances_2_queue.push(descriptorSet_instances_2_queue.front());
    descriptorSet_instances_2_queue.pop();
    const auto set = descriptorSet_instances_2_queue.front();
    const auto buffer = instancedEntitiesStorageBuffer->buffer;

    std::vector<EntitiesData> entitiesData;
    entitiesData.reserve(entitiesToRenderForModel.size());
    for (const auto& [_, entityData] : staticEntitiesDataForModel) entitiesData.push_back(entityData);
    const auto& [samplers, imageViews] = prepareTextureUpdateData(entitiesData);
    HmlDescriptorSetUpdater(set)
        .storageBufferAt(0, buffer, size)
        .textureArrayAt(1, samplers, imageViews)
        .update(hmlContext->hmlDevice);
}


HmlRenderer::TextureUpdateData HmlRenderer::prepareTextureUpdateData(std::span<const EntitiesData> entitiesData) noexcept {
    // NOTE Setting the rest of indices to a dummy texture is done only because
    // the validation layer complains that corresponding textures are not set,
    // although supposedly being used in shared.
    // In reality, however, the logic is solid and they are not used.

    std::array<VkSampler,   MAX_TEXTURES_COUNT> samplers;
    std::array<VkImageView, MAX_TEXTURES_COUNT> imageViews;
    const auto dummySampler = hmlContext->hmlResourceManager->dummyTextureResource->sampler;
    const auto dummyView    = hmlContext->hmlResourceManager->dummyTextureResource->view;
    std::fill(samplers.begin(), samplers.end(), dummySampler);
    std::fill(imageViews.begin(), imageViews.end(), dummyView);

    for (const auto& [textureIndex, modelResource] : entitiesData) {
        if (!modelResource->textureResource) continue;
        samplers  [textureIndex] = modelResource->textureResource->sampler;
        imageViews[textureIndex] = modelResource->textureResource->view;
    }

    return std::make_pair(std::move(samplers), std::move(imageViews));
}


VkCommandBuffer HmlRenderer::draw(const HmlFrameData& frameData) noexcept {
    auto commandBuffer = getCurrentCommands()[frameData.frameIndex];
    const auto inheritanceInfo = VkCommandBufferInheritanceInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = VK_NULL_HANDLE,
        .renderPass = currentRenderPass->renderPass,
        .subpass = 0, // we only have a single one
        .framebuffer = currentRenderPass->framebuffers[frameData.imageIndex],
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = static_cast<VkQueryControlFlags>(0),
        .pipelineStatistics = static_cast<VkQueryPipelineStatisticFlags>(0)
    };
    hmlContext->hmlCommands->beginRecordingSecondaryOnetime(commandBuffer, &inheritanceInfo);

    assert(getCurrentPipelines().size() == 2 && "::> Expected two pipelines in HmlRenderer.\n");

    { // Regular Entities
        const auto& hmlPipeline = getCurrentPipelines()[0];
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

        std::array<VkDescriptorSet, 3> descriptorSets = {
            frameData.generalDescriptorSet_0, descriptorSet_textures_1, descriptorSet_instances_2_queue.front()
        };
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

        for (const auto& [modelId, entities] : entitiesToRenderForModel) {
            // NOTE We know there is at least 1 such entity (ensured by logic of specifyEntitiesToRender)
            const auto& model = entities[0]->modelResource;

            VkBuffer vertexBuffers[] = { model->vertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            // NOTE advanced usage would be to have a single VkBuffer for both
            // VertexBuffer and IndexBuffer and specify different offsets into it.
            // Second and third arguments specify the offset and number of bindings
            // we're going to specify VertexBuffers for.
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, model->indexBuffer, 0, VK_INDEX_TYPE_UINT32); // 0 is offset

            for (const auto& entity : entities) {
                // NOTE Yes, in theory having a textureIndex per Entity is redundant
                PushConstantRegular pushConstant{
                    .model = entity->modelMatrix,
                    .color = glm::vec4(entity->color, 1.0f),
                    .textureIndex = model->textureResource ? textureIndexFor[modelId] : NO_TEXTURE_MARK,
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
                vkCmdDrawIndexed(commandBuffer, model->indicesCount,
                        instanceCount, firstIndex, offsetToAddToIndices, firstInstance);
            }
        }
    }

    if (!instancedCounts.empty()) { // Instanced (static Entities)
        const auto& hmlPipeline = getCurrentPipelines()[1];
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

        std::array<VkDescriptorSet, 3> descriptorSets = {
            frameData.generalDescriptorSet_0, descriptorSet_textures_1, descriptorSet_instances_2_queue.front()
        };
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

        uint32_t dispatchedInstancesCount = 0;
        auto instancedCountsIt = instancedCounts.begin();
        for (const auto& [modelId, entitiesData] : staticEntitiesDataForModel) {
            // NOTE We know there is at least 1 such entity (ensured by logic of specifyStaticEntitiesToRender)
            const auto& [textureIndex, modelResource] = entitiesData;

            VkBuffer vertexBuffers[] = { modelResource->vertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            // NOTE advanced usage would be to have a single VkBuffer for both
            // VertexBuffer and IndexBuffer and specify different offsets into it.
            // Second and third arguments specify the offset and number of bindings
            // we're going to specify VertexBuffers for.
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, modelResource->indexBuffer, 0, VK_INDEX_TYPE_UINT32); // 0 is offset

            // NOTE Yes, in theory having a textureIndex per Entity is redundant
            PushConstantInstanced pushConstant{
                .textureIndex = textureIndex,
                .time = 0.0f, // TODO
            };
            vkCmdPushConstants(commandBuffer, hmlPipeline->layout,
                hmlPipeline->pushConstantsStages, 0, sizeof(PushConstantInstanced), &pushConstant);

            // NOTE could use firstInstance to supply a single integer to gl_BaseInstance
            const uint32_t instanceCount = *instancedCountsIt;
            const uint32_t firstInstance = dispatchedInstancesCount;
            const uint32_t firstIndex = 0;
            const uint32_t offsetToAddToIndices = 0;
            // const uint32_t vertexCount = vertices.size();
            // const uint32_t firstVertex = 0;
            // vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
            vkCmdDrawIndexed(commandBuffer, modelResource->indicesCount,
            instanceCount, firstIndex, offsetToAddToIndices, firstInstance);

            dispatchedInstancesCount += *instancedCountsIt;
            std::advance(instancedCountsIt, 1);
        }
    }

    hmlContext->hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}
