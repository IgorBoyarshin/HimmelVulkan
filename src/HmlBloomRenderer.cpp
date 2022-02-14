#include "HmlBloomRenderer.h"


std::unique_ptr<HmlPipeline> HmlBloomRenderer::createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
        VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    HmlGraphicsPipelineConfig config{
        .bindingDescriptions   = {},
        .attributeDescriptions = {},
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .hmlShaders = HmlShaders()
            .addVertex("../shaders/out/bloom.vert.spv")
            .addFragment("../shaders/out/bloom.frag.spv"),
        .renderPass = renderPass,
        .extent = extent,
        .polygoneMode = VK_POLYGON_MODE_FILL,
        // .cullMode = VK_CULL_MODE_BACK_BIT,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .descriptorSetLayouts = descriptorSetLayouts,
        .pushConstantsStages = 0,
        .pushConstantsSizeBytes = 0,
        .tessellationPatchPoints = 0,
        .lineWidth = 1.0f,
        .colorAttachmentCount = 1,
        .withBlending = false,
    };

    return HmlPipeline::createGraphics(hmlDevice, std::move(config));
}


std::unique_ptr<HmlBloomRenderer> HmlBloomRenderer::create(
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands,
        std::shared_ptr<HmlResourceManager> hmlResourceManager,
        std::shared_ptr<HmlDescriptors> hmlDescriptors,
        uint32_t imageCount,
        uint32_t framesInFlight) noexcept {
    auto hmlRenderer = std::make_unique<HmlBloomRenderer>();
    hmlRenderer->hmlWindow = hmlWindow;
    hmlRenderer->hmlDevice = hmlDevice;
    hmlRenderer->hmlCommands = hmlCommands;
    // hmlRenderer->hmlRenderPass = hmlRenderPass;
    hmlRenderer->hmlResourceManager = hmlResourceManager;
    hmlRenderer->hmlDescriptors = hmlDescriptors;


    hmlRenderer->descriptorPool = hmlDescriptors->buildDescriptorPool()
        .withTextures(imageCount * 2)
        .maxSets(imageCount)
        .build(hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    hmlRenderer->descriptorSetLayoutTextures = hmlDescriptors->buildDescriptorSetLayout()
        .withTextureAt(0, VK_SHADER_STAGE_FRAGMENT_BIT)
        .withTextureAt(1, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(hmlDevice);
    if (!hmlRenderer->descriptorSetLayoutTextures) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutTextures);

    hmlRenderer->descriptorSet_textures_0_perImage = hmlDescriptors->createDescriptorSets(imageCount,
        hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool);
    if (hmlRenderer->descriptorSet_textures_0_perImage.empty()) return { nullptr };


    // hmlRenderer->hmlPipeline = createPipeline(hmlDevice, hmlRenderPass->extent,
    //     hmlRenderPass->renderPass, hmlRenderer->descriptorSetLayouts);
    // if (!hmlRenderer->hmlPipeline) return { nullptr };


    hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(framesInFlight, hmlCommands->commandPoolOnetimeFrames);

    return hmlRenderer;
}


HmlBloomRenderer::~HmlBloomRenderer() noexcept {
    std::cout << ":> Destroying HmlBloomRenderer...\n";

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlDevice->device, descriptorSetLayoutTextures, nullptr);
}


void HmlBloomRenderer::specify(
        const std::vector<std::shared_ptr<HmlImageResource>>& mainTextures,
        const std::vector<std::shared_ptr<HmlImageResource>>& brightnessTextures
        ) noexcept {
    const auto imageCount = mainTextures.size();
    for (size_t imageIndex = 0; imageIndex < imageCount; imageIndex++) {
        HmlDescriptorSetUpdater(descriptorSet_textures_0_perImage[imageIndex])
            .textureAt(0, mainTextures[imageIndex]->sampler, mainTextures[imageIndex]->view)
            .textureAt(1, brightnessTextures[imageIndex]->sampler, brightnessTextures[imageIndex]->view)
            .update(hmlDevice);
    }
}


VkCommandBuffer HmlBloomRenderer::draw(const HmlFrameData& frameData) noexcept {
    auto commandBuffer = commandBuffers[frameData.frameIndex];
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

    const auto& hmlPipeline = getCurrentPipeline();
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

    std::array<VkDescriptorSet, 1> descriptorSets = {
        descriptorSet_textures_0_perImage[frameData.imageIndex]
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    const uint32_t instanceCount = 1;
    const uint32_t firstInstance = 0;
    const uint32_t vertexCount = 6; // a simple square
    const uint32_t firstVertex = 0;
    vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

    hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}


// void HmlBloomRenderer::replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept {
//     hmlRenderPass = newHmlRenderPass;
//     hmlPipeline = createPipeline(hmlDevice, hmlRenderPass->extent, hmlRenderPass->renderPass, descriptorSetLayouts);
//     // NOTE The command pool is reset for all renderers prior to calling this function.
//     // NOTE commandBuffers must be rerecorded -- is done during baking
// }
