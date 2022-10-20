#if 0

#include "HmlBlurRenderer.h"


std::unique_ptr<HmlPipeline> HmlBlurRenderer::createPipeline(std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    HmlGraphicsPipelineConfig config{
        .bindingDescriptions   = {},
        .attributeDescriptions = {},
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .hmlShaders = HmlShaders()
            .addVertex("../shaders/out/blur.vert.spv")
            .addFragment("../shaders/out/blur.frag.spv"),
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
    };

    return HmlPipeline::createGraphics(hmlDevice, std::move(config));
}


std::unique_ptr<HmlBlurRenderer> HmlBlurRenderer::create(
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands,
        // std::shared_ptr<HmlRenderPass> hmlRenderPass,
        std::shared_ptr<HmlResourceManager> hmlResourceManager,
        std::shared_ptr<HmlDescriptors> hmlDescriptors,
        uint32_t imageCount,
        uint32_t framesInFlight) noexcept {
    auto hmlRenderer = std::make_unique<HmlBlurRenderer>();
    hmlRenderer->hmlWindow = hmlWindow;
    hmlRenderer->hmlDevice = hmlDevice;
    hmlRenderer->hmlCommands = hmlCommands;
    // hmlRenderer->hmlRenderPass = hmlRenderPass;
    hmlRenderer->hmlResourceManager = hmlResourceManager;
    hmlRenderer->hmlDescriptors = hmlDescriptors;

    hmlRenderer->framesInFlight = framesInFlight;


    hmlRenderer->descriptorPool = hmlDescriptors->buildDescriptorPool()
        .withTextures(imageCount * 2)
        .maxSets(imageCount * 2)
        .build(hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    hmlRenderer->descriptorSetLayoutTextures = hmlDescriptors->buildDescriptorSetLayout()
        .withTextureAt(0, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(hmlDevice);
    if (!hmlRenderer->descriptorSetLayoutTextures) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutTextures);

    hmlRenderer->descriptorSet_first_0_perImage = hmlDescriptors->createDescriptorSets(imageCount,
        hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool);
    if (hmlRenderer->descriptorSet_first_0_perImage.empty()) return { nullptr };

    hmlRenderer->descriptorSet_second_0_perImage = hmlDescriptors->createDescriptorSets(imageCount,
        hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool);
    if (hmlRenderer->descriptorSet_second_0_perImage.empty()) return { nullptr };


    // hmlRenderer->hmlPipeline = createPipeline(hmlDevice, hmlRenderPass->extent,
    //     hmlRenderPass->renderPass, hmlRenderer->descriptorSetLayouts);
    // if (!hmlRenderer->hmlPipeline) return { nullptr };


    // hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(framesInFlight, hmlCommands->commandPoolOnetimeFrames);

    return hmlRenderer;
}


HmlBlurRenderer::~HmlBlurRenderer() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlBlurRenderer...\n";
#endif

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlDevice->device, descriptorSetLayoutTextures, nullptr);
}


void HmlBlurRenderer::specify(
        const std::vector<std::shared_ptr<HmlImageResource>>& firstTextures,
        const std::vector<std::shared_ptr<HmlImageResource>>& secondTextures) noexcept {
    const auto imageCount = firstTextures.size();
    for (size_t imageIndex = 0; imageIndex < imageCount; imageIndex++) {
        HmlDescriptorSetUpdater(descriptorSet_first_0_perImage[imageIndex])
            .textureAt(0, firstTextures[imageIndex]->sampler, firstTextures[imageIndex]->view).update(hmlDevice);
        HmlDescriptorSetUpdater(descriptorSet_second_0_perImage[imageIndex])
            .textureAt(0, secondTextures[imageIndex]->sampler, secondTextures[imageIndex]->view).update(hmlDevice);
    }
}


void HmlBlurRenderer::modeHorizontal() noexcept {
    isVertical = 0;
}


void HmlBlurRenderer::modeVertical() noexcept {
    isVertical = 1;
}


VkCommandBuffer HmlBlurRenderer::draw(const HmlFrameData& frameData) noexcept {
    auto commandBuffer = commandBuffersPerRenderPass[getCurrentRenderPassIndex()][frameData.frameIndex];
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
    hmlCommands->beginRecordingSecondaryOnetime(commandBuffer, &inheritanceInfo);

    const auto& hmlPipelines = getCurrentPipelines();
    assert(hmlPipelines.size() == 1 && "::> Expected only a single pipeline in HmlBlurRenderer.\n");
    const auto& hmlPipeline = hmlPipelines[0];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

    std::array<VkDescriptorSet, 1> descriptorSets = {
        (isVertical == 0) ?
            descriptorSet_first_0_perImage[frameData.imageIndex] :
            descriptorSet_second_0_perImage[frameData.imageIndex]
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    PushConstant pushConstant{
        isVertical = isVertical,
    };
    vkCmdPushConstants(commandBuffer, hmlPipeline->layout,
        hmlPipeline->pushConstantsStages, 0, sizeof(PushConstant), &pushConstant);

    const uint32_t instanceCount = 1;
    const uint32_t firstInstance = 0;
    const uint32_t vertexCount = 6; // a simple square
    const uint32_t firstVertex = 0;
    vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

    hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}


// void selectRenderPass(std::shared_ptr<HmlRenderPass> renderPass) noexcept {
//     currentRenderPass = renderPass;
// }
//
//
// void addRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept {
//     auto hmlPipeline = createPipeline(hmlDevice, newHmlRenderPass->extent, newHmlRenderPass, descriptorSetLayouts);
//     pipelineForRenderPassStorage.emplace_back(newHmlRenderPass, std::move(hmlPipeline));
// }
//
//
// void clearRenderPasses() noexcept {
//     currentRenderPass.reset();
//     pipelineForRenderPassStorage.clear();
// }


// void HmlBlurRenderer::replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept {
//     hmlRenderPass = newHmlRenderPass;
//     hmlPipeline = createPipeline(hmlDevice, hmlRenderPass->extent, hmlRenderPass->renderPass, descriptorSetLayouts);
//     // NOTE The command pool is reset for all renderers prior to calling this function.
//     // NOTE commandBuffers must be rerecorded -- is done during baking
// }

#endif
