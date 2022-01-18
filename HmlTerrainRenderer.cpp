#include "HmlTerrainRenderer.h"


std::unique_ptr<HmlPipeline> HmlTerrainRenderer::createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
        VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    HmlGraphicsPipelineConfig config{
        .bindingDescriptions   = {},
        .attributeDescriptions = {},
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .hmlShaders = HmlShaders()
            .addVertex("shaders/out/terrain.vert.spv")
            .addFragment("shaders/out/terrain.frag.spv"),
        .renderPass = renderPass,
        .swapchainExtent = extent,
        // .polygoneMode = VK_POLYGON_MODE_LINE,
        .polygoneMode = VK_POLYGON_MODE_FILL,
        // .cullMode = VK_CULL_MODE_BACK_BIT,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .descriptorSetLayouts = descriptorSetLayouts,
        .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT,
        .pushConstantsSizeBytes = sizeof(PushConstant)
    };

    return HmlPipeline::createGraphics(hmlDevice, std::move(config));
}


std::unique_ptr<HmlTerrainRenderer> HmlTerrainRenderer::create(
        const char* heightmapFilename,
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


    hmlRenderer->descriptorPool = hmlDescriptors->buildDescriptorPool()
        .withTextures(2)
        .maxSets(1)
        .build(hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    // TODO NOTE set number is specified implicitly by vector index
    hmlRenderer->descriptorSetLayouts.push_back(viewProjDescriptorSetLayout);

    const auto descriptorSetLayoutHeightmap = hmlDescriptors->buildDescriptorSetLayout()
        .withTextureAt(0, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
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


    hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(hmlSwapchain->imageCount(), hmlCommands->commandPoolGeneral);

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

    hmlRenderer->bake(descriptorSet_0_perImage);


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


void HmlTerrainRenderer::bake(const std::vector<VkDescriptorSet>& descriptorSet_0_perImage) noexcept {
    const auto imageCount = descriptorSet_0_perImage.size(); // must be equal to swapchain.imageCount()
    for (uint32_t imageIndex = 0; imageIndex < imageCount; imageIndex++) {
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
        hmlCommands->beginRecordingSecondary(commandBuffer, &inheritanceInfo);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

        std::array<VkDescriptorSet, 2> descriptorSets = {
            descriptorSet_0_perImage[imageIndex], descriptorSet_heightmap_1
        };
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

        PushConstant pushConstant{
            .posStartFinish = glm::vec4(bounds.posStart.x, bounds.posStart.y, bounds.posFinish.x, bounds.posFinish.y),
            .dimX = heightmapTexture->width,
            .dimY = bounds.height,
            .dimZ = heightmapTexture->height,
            .offsetY = bounds.yOffset,
        };
        vkCmdPushConstants(commandBuffer, hmlPipeline->layout,
            hmlPipeline->pushConstantsStages, 0, sizeof(PushConstant), &pushConstant);

        const uint32_t instanceCount = (heightmapTexture->width - 1) * (heightmapTexture->height - 1);
        const uint32_t firstInstance = 0;
        const uint32_t vertexCount = 6; // a simple square
        const uint32_t firstVertex = 0;
        vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

        hmlCommands->endRecording(commandBuffer);
    }
}


VkCommandBuffer HmlTerrainRenderer::draw(uint32_t imageIndex) noexcept {
    return commandBuffers[imageIndex];
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
