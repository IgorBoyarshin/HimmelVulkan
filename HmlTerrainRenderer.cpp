#include "HmlTerrainRenderer.h"


std::unique_ptr<HmlPipeline> HmlTerrainRenderer::createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
        VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    HmlGraphicsPipelineConfig config{
        .bindingDescriptions   = {},
        .attributeDescriptions = {},
        .topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,
        .hmlShaders = HmlShaders()
            .addVertex("shaders/out/terrain.vert.spv")
            .addFragment("shaders/out/terrain.frag.spv")
            .addTessellationControl("shaders/out/terrain.tesc.spv")
            .addTessellationEvaluation("shaders/out/terrain.tese.spv"),
        .renderPass = renderPass,
        .swapchainExtent = extent,
        // .polygoneMode = VK_POLYGON_MODE_LINE,
        .polygoneMode = VK_POLYGON_MODE_FILL,
        // .cullMode = VK_CULL_MODE_BACK_BIT,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .descriptorSetLayouts = descriptorSetLayouts,
        .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
        .pushConstantsSizeBytes = sizeof(PushConstant),
        .tessellationPatchPoints = 4,
    };

    return HmlPipeline::createGraphics(hmlDevice, std::move(config));
}


std::unique_ptr<HmlTerrainRenderer> HmlTerrainRenderer::create(
        const char* heightmapFilename,
        uint32_t granularity,
        const char* grassFilename,
        const Bounds& bounds,
        const std::vector<VkDescriptorSet>& descriptorSet_0_perImage,
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands,
        std::shared_ptr<HmlSwapchain> hmlSwapchain,
        std::shared_ptr<HmlResourceManager> hmlResourceManager,
        std::shared_ptr<HmlDescriptors> hmlDescriptors,
        VkDescriptorSetLayout viewProjDescriptorSetLayout,
        uint32_t framesInFlight) noexcept {
    auto hmlRenderer = std::make_unique<HmlTerrainRenderer>();
    hmlRenderer->hmlWindow = hmlWindow;
    hmlRenderer->hmlDevice = hmlDevice;
    hmlRenderer->hmlCommands = hmlCommands;
    hmlRenderer->hmlSwapchain = hmlSwapchain;
    hmlRenderer->hmlResourceManager = hmlResourceManager;
    hmlRenderer->hmlDescriptors = hmlDescriptors;

    hmlRenderer->bounds = bounds;
    hmlRenderer->heightmapTexture = hmlResourceManager->newTextureResource(heightmapFilename, VK_FILTER_LINEAR);
    hmlRenderer->grassTexture = hmlResourceManager->newTextureResource(grassFilename, VK_FILTER_LINEAR);
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


    hmlRenderer->descriptorPool = hmlDescriptors->buildDescriptorPool()
        .withTextures(2)
        .maxSets(1)
        .build(hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    // TODO NOTE set number is specified implicitly by vector index
    hmlRenderer->descriptorSetLayouts.push_back(viewProjDescriptorSetLayout);

    const auto descriptorSetLayoutHeightmap = hmlDescriptors->buildDescriptorSetLayout()
        .withTextureAt(0, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
        .withTextureAt(1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(hmlDevice);
    if (!descriptorSetLayoutHeightmap) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(descriptorSetLayoutHeightmap);
    hmlRenderer->descriptorSetLayoutsSelf.push_back(descriptorSetLayoutHeightmap);

    hmlRenderer->descriptorSet_heightmap_1 = hmlDescriptors->createDescriptorSets(1,
        descriptorSetLayoutHeightmap, hmlRenderer->descriptorPool)[0];
    if (!hmlRenderer->descriptorSet_heightmap_1) return { nullptr };


    hmlRenderer->hmlPipeline = createPipeline(hmlDevice,
        hmlSwapchain->extent, hmlSwapchain->renderPass, hmlRenderer->descriptorSetLayouts);
    if (!hmlRenderer->hmlPipeline) return { nullptr };


    hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(hmlSwapchain->imageCount(), hmlCommands->commandPoolOnetimeFrames);

    // std::vector<VkSampler> samplers;
    // std::vector<VkImageView> imageViews;
    //     samplers.push_back(hmlRenderer->heightmapTexture->sampler);
    //     imageViews.push_back(hmlRenderer->heightmapTexture->imageView);
    //     samplers.push_back(hmlRenderer->grassTexture->sampler);
    //     imageViews.push_back(hmlRenderer->grassTexture->imageView);
    // HmlDescriptorSetUpdater(hmlRenderer->descriptorSet_heightmap_1).textureArrayAt(0, samplers, imageViews).update(hmlDevice);

    // HmlDescriptorSetUpdater(hmlRenderer->descriptorSet_heightmap_1)
    //     .textureAt(0,
    //         hmlRenderer->heightmapTexture->sampler,
    //         hmlRenderer->heightmapTexture->imageView)
    //     .textureAt(1,
    //         hmlRenderer->grassTexture->sampler,
    //         hmlRenderer->grassTexture->imageView)
    //     .update(hmlDevice);
    // TODO
    // TODO
    // TODO Why does not work in bulk and have to do by one??
    // TODO
    // TODO
    HmlDescriptorSetUpdater(hmlRenderer->descriptorSet_heightmap_1)
        .textureAt(0,
            hmlRenderer->heightmapTexture->sampler,
            hmlRenderer->heightmapTexture->imageView)
        .update(hmlDevice);
    HmlDescriptorSetUpdater(hmlRenderer->descriptorSet_heightmap_1)
        .textureAt(1,
            hmlRenderer->grassTexture->sampler,
            hmlRenderer->grassTexture->imageView)
        .update(hmlDevice);

    return hmlRenderer;
}


HmlTerrainRenderer::~HmlTerrainRenderer() noexcept {
    std::cout << ":> Destroying HmlTerrainRenderer...\n";

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
    for (auto layout : descriptorSetLayoutsSelf) {
        vkDestroyDescriptorSetLayout(hmlDevice->device, layout, nullptr);
    }
}


void HmlTerrainRenderer::constructTree(SubTerrain& subTerrain, const glm::vec3& cameraPos) const noexcept {
    static const auto tryJustIncreasingTessPower = [](Patch& patch, const glm::vec3& camera, float yOffset){
        static constexpr float RATIO = 64.0f;
        // NOTE must be 3 to absolutely ensure perfect stitching.
        // However, in practice due to the linear nature of tesselation change,
        // we can get away with higher numbers. Especially with a good algorithm.
        // NOTE After the introduction of SubTerrain, this value might need to
        // be decremented... not sure. The aforementioned logic still applies, tho.
        static constexpr int MAX_TESS_POWER = 3; // 3
        const auto center = glm::vec3(patch.center.x, yOffset, patch.center.y);
        const float dist = glm::distance(center, camera);
        float edge = 0.5f * (patch.size.x + patch.size.y) / (1 << patch.tessPower);
        while (dist / edge < RATIO) {
            if (patch.tessPower >= MAX_TESS_POWER) return false;
            edge /= 2.0f;
            patch.tessPower++;
        }

        return true;
    };

    static const auto canMatchNoNeedToSubdivide = [](Patch& patch, const glm::vec3& camera, float yOffset){
        const bool enough = tryJustIncreasingTessPower(patch, camera, yOffset);
        if (enough) return true;

        static constexpr int PATCH_MAX_LEVEL = 4; // 4
        if (patch.level >= PATCH_MAX_LEVEL) return true;

        return false;
    };


    // Create all patches starting from root
    auto& patches = subTerrain.patches;
    patches.clear();
    patches.reserve(MAX_PATCHES);
    const auto texCoordStart = subTerrain.texCoordStart;
    const auto texCoordStep  = subTerrain.texCoordStep;
    patches.emplace_back(subTerrain.center, subTerrain.size, texCoordStart, texCoordStep, 0, 0); // root
    for (size_t i = 0; i < patches.size(); i++) {
        auto& patch = patches[i];
        if (canMatchNoNeedToSubdivide(patch, cameraPos, bounds.yOffset)) continue;

        const auto texCoordStep = 0.5f * patch.texCoordStep;
        const auto quaterSize = 0.25f * patch.size;

        // NOTE prefer to start tesselationPower from 0 rather than (patch.tessPower - 1)
        // to ensure we avoid unnecessary geometry (if it's possible).

        patches.emplace_back(
            glm::vec2{ patch.center.x - quaterSize.x, patch.center.y + quaterSize.y },
            0.5f * patch.size,
            glm::vec2{ patch.texCoordStart.x, patch.texCoordStart.y + texCoordStep.y },
            texCoordStep,
            patch.level + 1,
            0
        );
        patch.leftUp = &patches.back();

        patches.emplace_back(
            glm::vec2{ patch.center.x + quaterSize.x, patch.center.y + quaterSize.y },
            0.5f * patch.size,
            glm::vec2{ patch.texCoordStart.x + texCoordStep.x, patch.texCoordStart.y + texCoordStep.y },
            texCoordStep,
            patch.level + 1,
            0
        );
        patch.rightUp = &patches.back();

        patches.emplace_back(
            glm::vec2{ patch.center.x - quaterSize.x, patch.center.y - quaterSize.y },
            0.5f * patch.size,
            glm::vec2{ patch.texCoordStart.x, patch.texCoordStart.y },
            texCoordStep,
            patch.level + 1,
            0
        );
        patch.leftDown = &patches.back();

        patches.emplace_back(
            glm::vec2{ patch.center.x + quaterSize.x, patch.center.y - quaterSize.y },
            0.5f * patch.size,
            glm::vec2{ patch.texCoordStart.x + texCoordStep.x, patch.texCoordStart.y },
            texCoordStep,
            patch.level + 1,
            0
        );
        patch.rightDown = &patches.back();
    }

    // Ensure stitching
    doAll(patches[0]); // root
}


// TODO is done twice for each edge. Make traversal directional
void HmlTerrainRenderer::doAll(Patch& patch) const noexcept {
    if (patch.isTerminal()) return;

    int max;

    max = std::max(findUpFor(*patch.leftDown), findDownFor(*patch.leftUp));
    propagateUpFor(*patch.leftDown, max);
    propagateDownFor(*patch.leftUp, max);

    max = std::max(findUpFor(*patch.rightDown), findDownFor(*patch.rightUp));
    propagateUpFor(*patch.rightDown, max);
    propagateDownFor(*patch.rightUp, max);

    max = std::max(findRightFor(*patch.leftDown), findLeftFor(*patch.rightDown));
    propagateRightFor(*patch.leftDown, max);
    propagateLeftFor(*patch.rightDown, max);

    max = std::max(findRightFor(*patch.leftUp), findLeftFor(*patch.rightUp));
    propagateRightFor(*patch.leftUp, max);
    propagateLeftFor(*patch.rightUp, max);

    doAll(*patch.leftUp);
    doAll(*patch.leftDown);
    doAll(*patch.rightUp);
    doAll(*patch.rightDown);
}


// TODO is done twice for each edge. Make traversal directional
// NOTE The max between the original power and the edge-based one is there so
// that we don't need to assign all edged the original value before the start of
// the traversal algorithm. The reason we would have to do that is so that
// inter-Subterrain part of the algorithm gets proper values.
int HmlTerrainRenderer::findUpFor(const Patch& patch) const noexcept {
    if (patch.isTerminal()) return std::max(patch.tessPower, patch.tessPowerUp);
    return 1 + std::max(findUpFor(*patch.leftUp), findUpFor(*patch.rightUp));
}
int HmlTerrainRenderer::findDownFor(const Patch& patch) const noexcept {
    if (patch.isTerminal()) return std::max(patch.tessPower, patch.tessPowerDown);
    return 1 + std::max(findDownFor(*patch.leftDown), findDownFor(*patch.rightDown));
}
int HmlTerrainRenderer::findLeftFor(const Patch& patch) const noexcept {
    if (patch.isTerminal()) return std::max(patch.tessPower, patch.tessPowerLeft);
    return 1 + std::max(findLeftFor(*patch.leftDown), findLeftFor(*patch.leftUp));
}
int HmlTerrainRenderer::findRightFor(const Patch& patch) const noexcept {
    if (patch.isTerminal()) return std::max(patch.tessPower, patch.tessPowerRight);
    return 1 + std::max(findRightFor(*patch.rightDown), findRightFor(*patch.rightUp));
}


// TODO is done twice for each edge. Make traversal directional
void HmlTerrainRenderer::propagateUpFor(Patch& patch, int tessPower) const noexcept {
    if (patch.isTerminal()) patch.tessPowerUp = (tessPower > 6) ? 6 : tessPower;
    else {
        propagateUpFor(*patch.leftUp,  tessPower - 1);
        propagateUpFor(*patch.rightUp, tessPower - 1);
    }
}
void HmlTerrainRenderer::propagateDownFor(Patch& patch, int tessPower) const noexcept {
    if (patch.isTerminal()) patch.tessPowerDown = (tessPower > 6) ? 6 : tessPower;
    else {
        propagateDownFor(*patch.leftDown,  tessPower - 1);
        propagateDownFor(*patch.rightDown, tessPower - 1);
    }
}
void HmlTerrainRenderer::propagateLeftFor(Patch& patch, int tessPower) const noexcept {
    if (patch.isTerminal()) patch.tessPowerLeft = (tessPower > 6) ? 6 : tessPower;
    else {
        propagateLeftFor(*patch.leftDown, tessPower - 1);
        propagateLeftFor(*patch.leftUp,   tessPower - 1);
    }
}
void HmlTerrainRenderer::propagateRightFor(Patch& patch, int tessPower) const noexcept {
    if (patch.isTerminal()) patch.tessPowerRight = (tessPower > 6) ? 6 : tessPower;
    else {
        propagateRightFor(*patch.rightDown, tessPower - 1);
        propagateRightFor(*patch.rightUp,   tessPower - 1);
    }
}


void HmlTerrainRenderer::update(const glm::vec3& cameraPos) noexcept {
    for (auto& subTerrain : subTerrains) constructTree(subTerrain, cameraPos);

    for (uint32_t x = 0; x < granularity; x++) {
        for (uint32_t y = 0; y < granularity; y++) {
            auto& root  = subTerrains[granularity * (x + 0) + (y + 0)].patches[0];
            if (y + 1 < granularity) {
                auto& rootUp = subTerrains[granularity * (x + 0) + (y + 1)].patches[0];
                const auto max = std::max(findUpFor(root), findDownFor(rootUp));
                propagateUpFor(root, max);
                propagateDownFor(rootUp, max);
            }
            if (x + 1 < granularity) {
                auto& rootRight = subTerrains[granularity * (x + 1) + (y + 0)].patches[0];
                const auto max = std::max(findRightFor(root), findLeftFor(rootRight));
                propagateRightFor(root, max);
                propagateLeftFor(rootRight, max);
            }
        }
    }
}


VkCommandBuffer HmlTerrainRenderer::draw(uint32_t imageIndex, VkDescriptorSet descriptorSet_0) noexcept {
    auto commandBuffer = commandBuffers[imageIndex];
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
        descriptorSet_0, descriptorSet_heightmap_1
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    for (const auto& subTerrain : subTerrains) {
        for (const auto& patch : subTerrain.patches) {
            if (patch.isParent()) continue;
            PushConstant pushConstant{
                .center = patch.center,
                .size = patch.size,
                .texCoordStart = patch.texCoordStart,
                .texCoordStep = patch.texCoordStep,
                .tessLevelLeft  = static_cast<float>(1 << (patch.tessPowerLeft  ? patch.tessPowerLeft  : patch.tessPower)),
                .tessLevelDown  = static_cast<float>(1 << (patch.tessPowerDown  ? patch.tessPowerDown  : patch.tessPower)),
                .tessLevelRight = static_cast<float>(1 << (patch.tessPowerRight ? patch.tessPowerRight : patch.tessPower)),
                .tessLevelUp    = static_cast<float>(1 << (patch.tessPowerUp    ? patch.tessPowerUp    : patch.tessPower)),
                .offsetY = bounds.yOffset,
                .maxHeight = bounds.height,
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

    hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}


// TODO in order for each type of Renderer to properly replace its pipeline,
// store a member in Renderer which specifies its type, and recreate the pipeline
// based on its value.
void HmlTerrainRenderer::replaceSwapchain(std::shared_ptr<HmlSwapchain> newHmlSwapChain) noexcept {
    hmlSwapchain = newHmlSwapChain;
    hmlPipeline = createPipeline(hmlDevice, hmlSwapchain->extent, hmlSwapchain->renderPass, descriptorSetLayouts);
    // NOTE The command pool is reset for all renderers prior to calling this function.
    // NOTE commandBuffers must be rerecorded -- is done during baking
}
