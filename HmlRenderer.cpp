#include "HmlRenderer.h"


std::unique_ptr<HmlPipeline> HmlRenderer::createSimplePipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
        VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
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

    return HmlPipeline::createGraphics(hmlDevice, std::move(config));
}


std::unique_ptr<HmlRenderer> HmlRenderer::createSimpleRenderer(
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands,
        std::shared_ptr<HmlSwapchain> hmlSwapchain,
        std::shared_ptr<HmlResourceManager> hmlResourceManager,
        std::shared_ptr<HmlDescriptors> hmlDescriptors,
        VkDescriptorSetLayout generalDescriptorSetLayout,
        uint32_t framesInFlight) noexcept {
    auto hmlRenderer = std::make_unique<HmlRenderer>();
    hmlRenderer->hmlWindow = hmlWindow;
    hmlRenderer->hmlDevice = hmlDevice;
    hmlRenderer->hmlCommands = hmlCommands;
    hmlRenderer->hmlSwapchain = hmlSwapchain;
    hmlRenderer->hmlResourceManager = hmlResourceManager;
    hmlRenderer->hmlDescriptors = hmlDescriptors;


    hmlRenderer->descriptorPool = hmlDescriptors->buildDescriptorPool()
        .withTextures(MAX_TEXTURES_COUNT)
        .maxSets(1)
        .build(hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    hmlRenderer->descriptorSetLayouts.push_back(generalDescriptorSetLayout);
    hmlRenderer->descriptorSetLayoutTextures = hmlDescriptors->buildDescriptorSetLayout()
        .withTextureArrayAt(0, VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURES_COUNT)
        .build(hmlDevice);
    if (!hmlRenderer->descriptorSetLayoutTextures) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutTextures);

    hmlRenderer->descriptorSet_textures_1 = hmlDescriptors->createDescriptorSets(1,
        hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool)[0];
    if (!hmlRenderer->descriptorSet_textures_1) return { nullptr };


    hmlRenderer->hmlPipeline = createSimplePipeline(hmlDevice,
        hmlSwapchain->extent, hmlSwapchain->renderPass, hmlRenderer->descriptorSetLayouts);
    if (!hmlRenderer->hmlPipeline) return { nullptr };


    hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(framesInFlight, hmlCommands->commandPoolOnetimeFrames);

    return hmlRenderer;
}


HmlRenderer::~HmlRenderer() noexcept {
    std::cout << ":> Destroying HmlRenderer...\n";

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlDevice->device, descriptorSetLayoutTextures, nullptr);
}


void HmlRenderer::specifyEntitiesToRender(const std::vector<std::shared_ptr<Entity>>& entities) noexcept {
    nextFreeTextureIndex = 0;
    textureIndexFor.clear();
    entitiesToRenderForModel.clear();

    for (const auto& entity : entities) {
        const auto& model = entity->modelResource;
        const auto id = model->id;
        if (!textureIndexFor.contains(id)) {
            entitiesToRenderForModel[id] = {};
            if (model->textureResource) {
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


void HmlRenderer::updateDescriptorSetTextures() noexcept {
    static bool mustDefaultInit = true;
    const auto usedTexturesCount = mustDefaultInit ? MAX_TEXTURES_COUNT : nextFreeTextureIndex;

    std::vector<VkSampler> samplers(usedTexturesCount);
    std::vector<VkImageView> imageViews(usedTexturesCount);

    // NOTE Is done only because the validation layer complains that corresponding
    // textures are not set, although supposedly being used in shared. In
    // reality, however, the logic is solid and they are not used.
    if (mustDefaultInit) {
        mustDefaultInit = false;
        for (uint32_t i = nextFreeTextureIndex; i < MAX_TEXTURES_COUNT; i++) {
            samplers[i] = anyModelWithTexture->textureResource->sampler;
            imageViews[i] = anyModelWithTexture->textureResource->imageView;
        }
    }

    // NOTE textureIndex always covers range [0..usedTexturesCount)
    for (const auto& [modelId, textureIndex] : textureIndexFor) {
        // Pick any Model with this texture
        const auto& model = entitiesToRenderForModel[modelId][0]->modelResource;
        if (!model->textureResource) continue;
        samplers[textureIndex] = model->textureResource->sampler;
        imageViews[textureIndex] = model->textureResource->imageView;
    }

    HmlDescriptorSetUpdater(descriptorSet_textures_1).textureArrayAt(0, samplers, imageViews).update(hmlDevice);
}


VkCommandBuffer HmlRenderer::draw(uint32_t frameIndex, uint32_t imageIndex, VkDescriptorSet descriptorSet_0) noexcept {
    auto commandBuffer = commandBuffers[frameIndex];
    const auto inheritanceInfo = VkCommandBufferInheritanceInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = VK_NULL_HANDLE,
        .renderPass = hmlSwapchain->renderPass,
        .subpass = 0, // we only have a single one
        .framebuffer = hmlSwapchain->framebuffers[imageIndex],
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = static_cast<VkQueryControlFlags>(0),
        .pipelineStatistics = static_cast<VkQueryPipelineStatisticFlags>(0)
    };
    hmlCommands->beginRecordingSecondaryOnetime(commandBuffer, &inheritanceInfo);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

    std::array<VkDescriptorSet, 2> descriptorSets = {
        descriptorSet_0, descriptorSet_textures_1
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

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
        vkCmdBindIndexBuffer(commandBuffer, model->indexBuffer, 0, VK_INDEX_TYPE_UINT32); // 0 is offset

        for (const auto& entity : entities) {
            // NOTE Yes, in theory having a textureIndex per Entity is redundant
            SimplePushConstant pushConstant{
                .model = entity->modelMatrix,
                .textureIndex = model->textureResource ? textureIndexFor[modelId] : NO_TEXTURE_MARK
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

    hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}


// TODO in order for each type of Renderer to properly replace its pipeline,
// store a member in Renderer which specifies its type, and recreate the pipeline
// based on its value.
void HmlRenderer::replaceSwapchain(std::shared_ptr<HmlSwapchain> newHmlSwapChain) noexcept {
    hmlSwapchain = newHmlSwapChain;
    hmlPipeline = createSimplePipeline(hmlDevice, hmlSwapchain->extent, hmlSwapchain->renderPass, descriptorSetLayouts);
    // NOTE The command pool is reset for all renderers prior to calling this function.
    // NOTE commandBuffers must be rerecorded -- is done during baking
}
