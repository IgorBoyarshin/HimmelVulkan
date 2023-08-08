#include "HmlTerrainRenderer.h"


std::vector<std::unique_ptr<HmlPipeline>> HmlTerrainRenderer::createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    /*
     * There are two ways of doing this debug pipeline along with the regular one.
     * 1) run both pipelines within the same RenderPass.
     * For this both pipelines must be created and stored for the same RenderPass.
     * 2) have two separate RenderPasses, each one having its own pipeline.
     * For this the two pipelines must be created separately for each respective
     * RenderPass and stored for them.
     */

    // std::vector<std::unique_ptr<HmlPipeline>> pipelines(PIPELINE_COUNT);
    std::vector<std::unique_ptr<HmlPipeline>> pipelines;

    switch (mode) {
        case Mode::Regular: {
            HmlGraphicsPipelineConfig config{
                .bindingDescriptions   = {},
                .attributeDescriptions = {},
                .topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
                .hmlShaders = HmlShaders()
                    .addVertex("../shaders/out/terrain.vert.spv")
                    .addTessellationControl("../shaders/out/terrain.tesc.spv")
                    .addTessellationEvaluation("../shaders/out/terrain_deferred.tese.spv")
                    .addFragment("../shaders/out/terrain_deferred.frag.spv"),
                .renderPass = hmlRenderPass->renderPass,
                .extent = hmlRenderPass->extent,
                // .polygoneMode = VK_POLYGON_MODE_LINE,
                .polygoneMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                // .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .descriptorSetLayouts = descriptorSetLayouts,
                .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                .pushConstantsSizeBytes = sizeof(PushConstant),
                .tessellationPatchPoints = 4,
                .lineWidth = 1.0f,
                .colorAttachmentCount = hmlRenderPass->colorAttachmentCount,
                .withBlending = false,
                .withDepthTest = true,
            };

            // pipelines[PIPELINE_REGULAR_INDEX] = HmlPipeline::createGraphics(hmlDevice, std::move(config));
            pipelines.push_back(HmlPipeline::createGraphics(hmlContext->hmlDevice, std::move(config)));
        } break;

        case Mode::Debug: {
            HmlGraphicsPipelineConfig config{
                .bindingDescriptions   = {},
                .attributeDescriptions = {},
                .topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
                .hmlShaders = HmlShaders()
                    .addVertex("../shaders/out/terrain.vert.spv")
                    .addTessellationControl("../shaders/out/terrain.tesc.spv")
                    .addTessellationEvaluation("../shaders/out/terrain_debug.tese.spv")
                    .addGeometry("../shaders/out/terrain_debug.geom.spv")
                    .addFragment("../shaders/out/terrain_debug.frag.spv"),
                .renderPass = hmlRenderPass->renderPass,
                .extent = hmlRenderPass->extent,
                // .polygoneMode = VK_POLYGON_MODE_LINE,
                .polygoneMode = VK_POLYGON_MODE_FILL, // NOTE does not matter, we output a line strip
                .cullMode = VK_CULL_MODE_BACK_BIT,
                // .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .descriptorSetLayouts = descriptorSetLayouts,
                .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                .pushConstantsSizeBytes = sizeof(PushConstant),
                .tessellationPatchPoints = 4,
                .lineWidth = 2.0f,
                .colorAttachmentCount = hmlRenderPass->colorAttachmentCount,
                .withBlending = false,
                .withDepthTest = true,
            };

            // pipelines[PIPELINE_DEBUG_INDEX] = HmlPipeline::createGraphics(hmlDevice, std::move(config));
            pipelines.push_back(HmlPipeline::createGraphics(hmlContext->hmlDevice, std::move(config)));
        } break;

        case Mode::Shadowmap: {
            HmlGraphicsPipelineConfig config{
                .bindingDescriptions   = {},
                .attributeDescriptions = {},
                .topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
                .hmlShaders = HmlShaders()
                    .addVertex("../shaders/out/terrain.vert.spv")
                    .addTessellationControl("../shaders/out/terrain.tesc.spv")
                    .addTessellationEvaluation("../shaders/out/terrain_shadow_deferred.tese.spv"),
                .renderPass = hmlRenderPass->renderPass,
                .extent = hmlRenderPass->extent,
                // .polygoneMode = VK_POLYGON_MODE_LINE,
                .polygoneMode = VK_POLYGON_MODE_FILL,
                .cullMode = VK_CULL_MODE_BACK_BIT,
                // .cullMode = VK_CULL_MODE_NONE,
                .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .descriptorSetLayouts = descriptorSetLayouts,
                .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                .pushConstantsSizeBytes = sizeof(PushConstant),
                .tessellationPatchPoints = 4,
                .lineWidth = 1.0f,
                .colorAttachmentCount = hmlRenderPass->colorAttachmentCount,
                .withBlending = false,
                .withDepthTest = true,
            };

            // pipelines[PIPELINE_REGULAR_INDEX] = HmlPipeline::createGraphics(hmlDevice, std::move(config));
            pipelines.push_back(HmlPipeline::createGraphics(hmlContext->hmlDevice, std::move(config)));
        } break;

        default:
            assert(false && "::> HmlTerrainRenderer: Unhandled Mode in createPipelines.\n");
    }

    return pipelines;
}


std::unique_ptr<HmlTerrainRenderer> HmlTerrainRenderer::create(
        const char* heightmapFilename,
        uint32_t granularity,
        const char* grassFilename,
        const Bounds& bounds,
        std::shared_ptr<HmlContext> hmlContext,
        VkDescriptorSetLayout viewProjDescriptorSetLayout) noexcept {
    auto hmlRenderer = std::make_unique<HmlTerrainRenderer>();
    hmlRenderer->hmlContext = hmlContext;

    hmlRenderer->bounds = bounds;
    hmlRenderer->heightmapTexture = hmlContext->hmlResourceManager->newTextureResource(heightmapFilename, 1, VK_FORMAT_R8_UNORM, VK_FILTER_LINEAR);
    hmlRenderer->grassTexture = hmlContext->hmlResourceManager->newTextureResource(grassFilename, 4, VK_FORMAT_R8G8B8A8_SRGB, VK_FILTER_LINEAR);
    hmlRenderer->granularity = granularity;

    {
        const glm::vec2 delta{ 1.0f / granularity, 1.0f / granularity };
        const glm::vec2 unit = (bounds.posFinish - bounds.posStart) / static_cast<float>(granularity);
        for (uint32_t x = 0; x < granularity; x++) {
            for (uint32_t y = 0; y < granularity; y++) {
                hmlRenderer->subTerrains.emplace_back(
                    bounds.posStart + 0.5f * unit + glm::vec2{ x * unit.x, y * unit.y },
                    unit,
                    glm::vec2{ x * delta.x, y * delta.y },
                    delta
                );
            }
        }
    }


    hmlRenderer->descriptorPool = hmlContext->hmlDescriptors->buildDescriptorPool()
        .withTextures(2)
        .maxDescriptorSets(1)
        .build(hmlContext->hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    // TODO NOTE set number is specified implicitly by vector index
    hmlRenderer->descriptorSetLayouts.push_back(viewProjDescriptorSetLayout);

    const auto descriptorSetLayoutHeightmap = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
        .withTextureAt(0, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .withTextureAt(1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(hmlContext->hmlDevice);
    if (!descriptorSetLayoutHeightmap) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(descriptorSetLayoutHeightmap);
    hmlRenderer->descriptorSetLayoutsSelf.push_back(descriptorSetLayoutHeightmap);

    hmlRenderer->descriptorSet_heightmap_1 = hmlContext->hmlDescriptors->createDescriptorSets(1,
        descriptorSetLayoutHeightmap, hmlRenderer->descriptorPool)[0];
    if (!hmlRenderer->descriptorSet_heightmap_1) return { nullptr };


    // hmlRenderer->hmlPipeline = createPipeline(hmlDevice,
    //     hmlRenderPass->extent, hmlRenderPass->renderPass, hmlRenderer->descriptorSetLayouts);
    // if (!hmlRenderer->hmlPipeline) return { nullptr };
    // hmlRenderer->hmlPipelineDebug = createPipelineDebug(hmlDevice,
    //     hmlRenderPass->extent, hmlRenderPass->renderPass, hmlRenderer->descriptorSetLayouts);
    // if (!hmlRenderer->hmlPipelineDebug) return { nullptr };


    // hmlRenderer->commandBuffersForRenderPass.push_back(hmlCommands->allocateSecondary(imageCount, hmlCommands->commandPoolOnetimeFrames));

    HmlDescriptorSetUpdater(hmlRenderer->descriptorSet_heightmap_1)
        .textureAt(0,
            hmlRenderer->heightmapTexture->sampler,
            hmlRenderer->heightmapTexture->view)
        .textureAt(1,
            hmlRenderer->grassTexture->sampler,
            hmlRenderer->grassTexture->view)
        .update(hmlContext->hmlDevice);
    return hmlRenderer;
}


HmlTerrainRenderer::~HmlTerrainRenderer() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlTerrainRenderer...\n";
#endif

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlContext->hmlDevice->device, descriptorPool, nullptr);
    for (auto layout : descriptorSetLayoutsSelf) {
        vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, layout, nullptr);
    }
}


void HmlTerrainRenderer::constructTree(SubTerrain& subTerrain, const glm::vec3& cameraPos) const noexcept {
    static const auto shouldSubdivide = [](Patch& patch, const glm::vec3& camera, float yOffset){
        static constexpr int PATCH_MAX_LEVEL = 3;
        if (patch.level >= PATCH_MAX_LEVEL) return false;

        static constexpr float RATIO = 128.0f; // inversely proportional to LOD
        const auto center = glm::vec3(patch.center.x, yOffset, patch.center.y);
        const float dist = glm::distance(center, camera);
        float edge = 0.5f * (patch.size.x + patch.size.y);
        return (dist / edge < RATIO);
    };

    // Create all patches starting from root
    auto& patches = subTerrain.patches;
    patches.clear();
    // No need to reserve because we do not store addresses anymore.
    // The size is amortized now.
    const auto texCoordStart = subTerrain.texCoordStart;
    const auto texCoordStep  = subTerrain.texCoordStep;
    patches.emplace_back(subTerrain.center, subTerrain.size, texCoordStart, texCoordStep, 0); // root
    for (size_t i = 0; i < patches.size(); i++) { // NOTE size is dynamic
        auto& patch = patches[i];
        if (!shouldSubdivide(patch, cameraPos, bounds.yOffset)) continue;
        patch.isParent = true;
        const auto parent = patch; // need a copy because we're gonna reallocate the parent

        const auto texCoordStep = 0.5f * parent.texCoordStep;
        const auto quaterSize = 0.25f * parent.size;

        patches.emplace_back(
            glm::vec2{ parent.center.x - quaterSize.x, parent.center.y + quaterSize.y },
            0.5f * parent.size,
            glm::vec2{ parent.texCoordStart.x, parent.texCoordStart.y + texCoordStep.y },
            texCoordStep,
            parent.level + 1
        );
        patches.emplace_back(
            glm::vec2{ parent.center.x + quaterSize.x, parent.center.y + quaterSize.y },
            0.5f * parent.size,
            glm::vec2{ parent.texCoordStart.x + texCoordStep.x, parent.texCoordStart.y + texCoordStep.y },
            texCoordStep,
            parent.level + 1
        );
        patches.emplace_back(
            glm::vec2{ parent.center.x - quaterSize.x, parent.center.y - quaterSize.y },
            0.5f * parent.size,
            glm::vec2{ parent.texCoordStart.x, parent.texCoordStart.y },
            texCoordStep,
            parent.level + 1
        );
        patches.emplace_back(
            glm::vec2{ parent.center.x + quaterSize.x, parent.center.y - quaterSize.y },
            0.5f * parent.size,
            glm::vec2{ parent.texCoordStart.x + texCoordStep.x, parent.texCoordStart.y },
            texCoordStep,
            parent.level + 1
        );
    }
}


void HmlTerrainRenderer::update(const glm::vec3& cameraPos) noexcept {
    static glm::vec3 cachedCameraPos;
    if (cachedCameraPos == cameraPos) return;
    cachedCameraPos = cameraPos;

    for (auto& subTerrain : subTerrains) constructTree(subTerrain, cameraPos);
}


VkCommandBuffer HmlTerrainRenderer::draw(const HmlFrameData& frameData) noexcept {
    auto commandBuffer = getCurrentCommands()[frameData.swapchainImageIndex];
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
    hml::DebugLabel debugLabel{commandBuffer, "Terrain"};
#endif
#if USE_TIMESTAMP_QUERIES
    if (mode == Mode::Regular) {
        hmlContext->hmlQueries->registerEvent("HmlTerrainRenderer: begin", "Tw(r)", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    } else if (mode == Mode::Debug) {
        hmlContext->hmlQueries->registerEvent("HmlTerrainRenderer: begin", "Tw(d)", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    } else if (mode == Mode::Shadowmap) {
        hmlContext->hmlQueries->registerEvent("HmlTerrainRenderer: begin", "Tw(s)", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
    }
#endif

    assert(getCurrentPipelines().size() == 1 && "::> Expected only a single pipeline in HmlTerrainRenderer.\n");
    const auto& hmlPipeline = getCurrentPipelines()[0];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

    std::array<VkDescriptorSet, 2> descriptorSets = {
        frameData.generalDescriptorSet_0, descriptorSet_heightmap_1
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    for (const auto& subTerrain : subTerrains) {
        for (const auto& patch : subTerrain.patches) {
            if (patch.isParent) continue;
            PushConstant pushConstant{
                .center = patch.center,
                .size = patch.size,
                .texCoordStart = patch.texCoordStart,
                .texCoordStep = patch.texCoordStep,
                .offsetY = bounds.yOffset,
                .maxHeight = bounds.height,
                .level = patch.level,
            };
            vkCmdPushConstants(commandBuffer, hmlPipeline->layout,
                hmlPipeline->pushConstantsStages, 0, sizeof(PushConstant), &pushConstant);

            const uint32_t instanceCount = 1;
            const uint32_t firstInstance = 0;
            const uint32_t vertexCount = 4; // a simple square
            const uint32_t firstVertex = 0;
            vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
        }
    }

#if USE_TIMESTAMP_QUERIES
    if (mode == Mode::Regular) {
        hmlContext->hmlQueries->registerEvent("HmlTerrainRenderer: end", "T(r)", commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    } else if (mode == Mode::Debug) {
        hmlContext->hmlQueries->registerEvent("HmlTerrainRenderer: end", "T(d)", commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    } else if (mode == Mode::Shadowmap) {
        hmlContext->hmlQueries->registerEvent("HmlTerrainRenderer: end", "T(s)", commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    }
#endif
#if USE_DEBUG_LABELS
    debugLabel.end();
#endif
    hmlContext->hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}


void HmlTerrainRenderer::addRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept {
    currentRenderPass = newHmlRenderPass;

    auto hmlPipelines = createPipelines(newHmlRenderPass, descriptorSetLayouts);
    pipelineForRenderPassStorage.emplace_back(newHmlRenderPass, std::move(hmlPipelines));

    const auto count = hmlContext->imageCount();
    const auto pool = hmlContext->hmlCommands->commandPoolOnetimeFrames;
    auto commands = hmlContext->hmlCommands->allocateSecondary(count, pool);
    commandBuffersForRenderPass.push_back(std::move(commands));
}
