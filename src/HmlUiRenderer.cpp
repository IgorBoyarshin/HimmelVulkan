#include "HmlUiRenderer.h"


std::vector<std::unique_ptr<HmlPipeline>> HmlUiRenderer::createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    std::vector<std::unique_ptr<HmlPipeline>> pipelines;

    {
        HmlGraphicsPipelineConfig config{
            .bindingDescriptions   = {},
            .attributeDescriptions = {},
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .hmlShaders = HmlShaders()
                .addVertex("../shaders/out/ui.vert.spv")
                .addFragment("../shaders/out/ui.frag.spv"),
            .renderPass = hmlRenderPass->renderPass,
            .extent = hmlRenderPass->extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            // .cullMode = VK_CULL_MODE_BACK_BIT,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayouts = descriptorSetLayouts,
            .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            .pushConstantsSizeBytes = sizeof(PushConstant),
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


std::unique_ptr<HmlUiRenderer> HmlUiRenderer::create(std::shared_ptr<HmlContext> hmlContext) noexcept {
    auto hmlRenderer = std::make_unique<HmlUiRenderer>();
    hmlRenderer->hmlContext = hmlContext;

    const auto imageCount = hmlContext->imageCount();

    hmlRenderer->descriptorPool = hmlContext->hmlDescriptors->buildDescriptorPool()
        .withTextures(imageCount * MAX_TEXTURES_COUNT)
        .maxDescriptorSets(imageCount)
        .build(hmlContext->hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    hmlRenderer->descriptorSetLayoutTextures = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
        .withTextureArrayAt(0, VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURES_COUNT)
        .build(hmlContext->hmlDevice);
    if (!hmlRenderer->descriptorSetLayoutTextures) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutTextures);

    hmlRenderer->descriptorSet_textures_0_perImage = hmlContext->hmlDescriptors->createDescriptorSets(imageCount,
        hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool);
    if (hmlRenderer->descriptorSet_textures_0_perImage.empty()) return { nullptr };


    // hmlRenderer->hmlPipeline = createPipeline(hmlDevice, hmlRenderPass->extent,
    //     hmlRenderPass->renderPass, hmlRenderer->descriptorSetLayouts);
    // if (!hmlRenderer->hmlPipeline) return { nullptr };


    // hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(framesInFlight, hmlCommands->commandPoolOnetimeFrames);

    return hmlRenderer;
}


HmlUiRenderer::~HmlUiRenderer() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlUiRenderer...\n";
#endif

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlContext->hmlDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, descriptorSetLayoutTextures, nullptr);
}


void HmlUiRenderer::specify(const std::vector<std::vector<std::shared_ptr<HmlImageResource>>>& resources) noexcept {
    texturesCount = resources.size();
    const auto imageCount = resources[0].size();

    for (size_t imageIndex = 0; imageIndex < imageCount; imageIndex++) {
        std::vector<VkSampler> samplers;
        std::vector<VkImageView> views;
        for (const auto& resource : resources) {
            const auto& texture = resource[imageIndex];
            samplers.push_back(texture->sampler);
            views.push_back(texture->view);
        }
        HmlDescriptorSetUpdater(descriptorSet_textures_0_perImage[imageIndex])
            .textureArrayAt(0, samplers, views).update(hmlContext->hmlDevice);
    }
}


VkCommandBuffer HmlUiRenderer::draw(const HmlFrameData& frameData) noexcept {
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
    hmlContext->hmlQueries->registerEvent("HmlUiRenderer: begin", "UIw", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    assert(getCurrentPipelines().size() == 1 && "::> Expected only a single pipeline in HmlUiRenderer.\n");
    const auto& hmlPipeline = getCurrentPipelines()[0];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

    std::array<VkDescriptorSet, 1> descriptorSets = {
        descriptorSet_textures_0_perImage[frameData.imageIndex]
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    for (int32_t i = 0; i < static_cast<int32_t>(texturesCount); i++) {
        PushConstant pushConstant{
            .textureIndex = i,
            .shift = (2.0f / MAX_TEXTURES_COUNT) * i,
        };
        vkCmdPushConstants(commandBuffer, hmlPipeline->layout,
            hmlPipeline->pushConstantsStages, 0, sizeof(PushConstant), &pushConstant);

        const uint32_t instanceCount = 1;
        const uint32_t firstInstance = 0;
        const uint32_t vertexCount = 6; // a simple square
        const uint32_t firstVertex = 0;
        vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
    }

    hmlContext->hmlQueries->registerEvent("HmlUiRenderer: end", "UI", commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    hmlContext->hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}
