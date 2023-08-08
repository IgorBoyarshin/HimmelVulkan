#include "HmlDeferredRenderer.h"


std::vector<std::unique_ptr<HmlPipeline>> HmlDeferredRenderer::createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    std::vector<std::unique_ptr<HmlPipeline>> pipelines;

    {
        // NOTE These need to live until the config has been used up
        const std::array<VkSpecializationMapEntry, 3> specializationMapEntries = {
            VkSpecializationMapEntry{
                .constantID = 0,
                .offset = 0 * sizeof(float),
                .size   = 1 * sizeof(float),
            },
            VkSpecializationMapEntry{
                .constantID = 1,
                .offset = 1 * sizeof(float),
                .size   = 1 * sizeof(float),
            },
            VkSpecializationMapEntry{
                .constantID = 2,
                .offset = 2 * sizeof(float),
                .size   = 1 * sizeof(float),
            }
        };
        const float fov = 45.0f;
        const float aspect_w_h = hmlContext->hmlSwapchain->extentAspect();
        const auto [y, p] = calculateSphereYawPitch(aspect_w_h, fov);
        const float sphereX = glm::sin(y) * glm::cos(p);
        const float sphereY = glm::sin(p);
        const float sphereZ = glm::cos(y) * glm::cos(p);

        const std::array<float, 3> byteData = { sphereX, -sphereY, sphereZ };

        HmlGraphicsPipelineConfig config{
            .bindingDescriptions   = {},
            .attributeDescriptions = {},
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .hmlShaders = HmlShaders()
                .addVertex("../shaders/out/deferred.vert.spv", { VkSpecializationInfo{
                    .mapEntryCount = specializationMapEntries.size(),
                    .pMapEntries = specializationMapEntries.data(),
                    .dataSize = byteData.size() * sizeof(byteData[0]),
                    .pData = byteData.data(),
                }})
                .addFragment("../shaders/out/deferred.frag.spv"),
            .renderPass = hmlRenderPass->renderPass,
            .extent = hmlRenderPass->extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            // .cullMode = VK_CULL_MODE_BACK_BIT,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayouts = descriptorSetLayouts,
            .pushConstantsStages = 0,
            .pushConstantsSizeBytes = 0,
            .tessellationPatchPoints = 0,
            .lineWidth = 1.0f,
            .colorAttachmentCount = hmlRenderPass->colorAttachmentCount,
            .withBlending = false,
            .withDepthTest = false,
        };

        pipelines.push_back(HmlPipeline::createGraphics(hmlContext->hmlDevice, std::move(config)));
    }

    return pipelines;
}


std::unique_ptr<HmlDeferredRenderer> HmlDeferredRenderer::create(
        std::shared_ptr<HmlContext> hmlContext,
        VkDescriptorSetLayout viewProjDescriptorSetLayout) noexcept {
    auto hmlRenderer = std::make_unique<HmlDeferredRenderer>();
    hmlRenderer->hmlContext = hmlContext;

    const auto imageCount = hmlContext->imageCount();

    hmlRenderer->descriptorPool = hmlContext->hmlDescriptors->buildDescriptorPool()
        .withTextures(imageCount * G_COUNT)
        .maxDescriptorSets(imageCount)
        .build(hmlContext->hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    // TODO NOTE set number is specified implicitly by vector index
    hmlRenderer->descriptorSetLayouts.push_back(viewProjDescriptorSetLayout);

    hmlRenderer->descriptorSetLayoutTextures = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
        .withTextureArrayAt(0, VK_SHADER_STAGE_FRAGMENT_BIT, G_COUNT)
        .build(hmlContext->hmlDevice);
    if (!hmlRenderer->descriptorSetLayoutTextures) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutTextures);

    hmlRenderer->descriptorSet_textures_1_perImage = hmlContext->hmlDescriptors->createDescriptorSets(imageCount,
        hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool);
    if (hmlRenderer->descriptorSet_textures_1_perImage.empty()) return { nullptr };


    // hmlRenderer->hmlPipeline = createPipeline(hmlContext->hmlDevice, hmlRenderPass->extent,
    //     hmlRenderPass->renderPass, hmlRenderer->descriptorSetLayouts);
    // if (!hmlRenderer->hmlPipeline) return { nullptr };


    // hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(framesInFlight, hmlCommands->commandPoolOnetimeFrames);

    return hmlRenderer;
}


HmlDeferredRenderer::~HmlDeferredRenderer() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlDeferredRenderer...\n";
#endif

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlContext->hmlDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, descriptorSetLayoutTextures, nullptr);
}


void HmlDeferredRenderer::specify(const std::array<std::vector<std::shared_ptr<HmlImageResource>>, G_COUNT>& resources) noexcept {
    const auto imageCount = resources[0].size();
    for (size_t imageIndex = 0; imageIndex < imageCount; imageIndex++) {
        std::vector<VkSampler> samplers;
        std::vector<VkImageView> views;
        for (const auto& resource : resources) {
            const auto& texture = resource[imageIndex];
            samplers.push_back(texture->sampler);
            views.push_back(texture->view);
        }
        HmlDescriptorSetUpdater(descriptorSet_textures_1_perImage[imageIndex])
            .textureArrayAt(0, samplers, views).update(hmlContext->hmlDevice);
    }
}


std::pair<float, float> HmlDeferredRenderer::calculateSphereYawPitch(float aspect_w_h, float cameraFov) noexcept {
    assert(cameraFov == 45.0f && "The logic for FOV!=45 has not been implemented.");

    // We need to satisfy the following two equations:
    // 1) A = sin(y) * ctg(p)  ==>  p = acot(A / sin(y))
    static const auto pitch1 = [](float A, float yDegrees){
        return glm::atan(glm::sin(glm::radians(yDegrees)) / A);
    };
    // 2) p = 22.55 * cos(y / 59.1)  [for fov=45]
    static const auto pitch2 = [](float yDegrees){
        return glm::radians(22.55) * (glm::cos((yDegrees / 59.1)));
    };

    const float A = aspect_w_h;
    static constexpr float accuracy = 0.01f;
    float boundMin = 0.0f;
    float boundMax = 90.0f;
    while (true) {
        const float yDegrees = 0.5f * (boundMin + boundMax);
        const float p1 = pitch1(A, yDegrees);
        const float p2 = pitch2(yDegrees);
        if (p1 > p2) boundMax = yDegrees;
        else         boundMin = yDegrees;

        if (boundMax - boundMin < accuracy) break;
    }

    const float yaw = 0.5f * (boundMin + boundMax);
    const float pitch = pitch2(yaw);
    return std::make_pair(glm::radians(yaw), pitch);
}


VkCommandBuffer HmlDeferredRenderer::draw(const HmlFrameData& frameData) noexcept {
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
    hml::DebugLabel debugLabel(commandBuffer, "Deferred");
#endif
#if USE_TIMESTAMP_QUERIES
    hmlContext->hmlQueries->registerEvent("HmlDeferredRenderer: begin", "Dw", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
#endif

    assert(getCurrentPipelines().size() == 1 && "::> Expected only a single pipeline in HmlDeferredRenderer.\n");
    const auto& hmlPipeline = getCurrentPipelines()[0];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

    std::array<VkDescriptorSet, 2> descriptorSets = {
        frameData.generalDescriptorSet_0, descriptorSet_textures_1_perImage[frameData.swapchainImageIndex]
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    const uint32_t instanceCount = 1;
    const uint32_t firstInstance = 0;
    const uint32_t vertexCount = 6; // a simple square
    const uint32_t firstVertex = 0;
    vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

#if USE_TIMESTAMP_QUERIES
    hmlContext->hmlQueries->registerEvent("HmlDeferredRenderer: end", "D", commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
#endif
#if USE_DEBUG_LABELS
    debugLabel.end();
#endif
    hmlContext->hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}
