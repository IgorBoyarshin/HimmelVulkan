#include "HmlLightRenderer.h"


std::unique_ptr<HmlPipeline> HmlLightRenderer::createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
        VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    HmlGraphicsPipelineConfig config{
        .bindingDescriptions   = {},
        .attributeDescriptions = {},
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .hmlShaders = HmlShaders()
            .addVertex("../shaders/out/light.vert.spv")
            .addFragment("../shaders/out/light.frag.spv"),
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
        .colorAttachmentCount = 2,
        .withBlending = true,
    };

    return HmlPipeline::createGraphics(hmlDevice, std::move(config));
}


std::unique_ptr<HmlLightRenderer> HmlLightRenderer::create(
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands,
        // std::shared_ptr<HmlRenderPass> hmlRenderPass,
        std::shared_ptr<HmlResourceManager> hmlResourceManager,
        std::shared_ptr<HmlDescriptors> hmlDescriptors,
        VkDescriptorSetLayout viewProjDescriptorSetLayout,
        const std::vector<VkDescriptorSet>& generalDescriptorSet_0_perImage) noexcept {
    auto hmlRenderer = std::make_unique<HmlLightRenderer>();
    hmlRenderer->hmlWindow = hmlWindow;
    hmlRenderer->hmlDevice = hmlDevice;
    hmlRenderer->hmlCommands = hmlCommands;
    // hmlRenderer->hmlRenderPass = hmlRenderPass;
    hmlRenderer->hmlResourceManager = hmlResourceManager;
    hmlRenderer->hmlDescriptors = hmlDescriptors;

    hmlRenderer->descriptorSet_0_perImage = generalDescriptorSet_0_perImage;

    // TODO NOTE set number is specified implicitly by vector index
    hmlRenderer->descriptorSetLayouts.push_back(viewProjDescriptorSetLayout);

    // hmlRenderer->hmlPipeline = createPipeline(hmlDevice,
    //     hmlRenderPass->extent, hmlRenderPass->renderPass, hmlRenderer->descriptorSetLayouts);
    // if (!hmlRenderer->hmlPipeline) return { nullptr };

    const auto imageCount = generalDescriptorSet_0_perImage.size();
    hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(imageCount, hmlCommands->commandPoolGeneralResettable);

    return hmlRenderer;
}


HmlLightRenderer::~HmlLightRenderer() noexcept {
    std::cout << ":> Destroying HmlLightRenderer...\n";

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    // vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
    // for (auto layout : descriptorSetLayoutsSelf) {
    //     vkDestroyDescriptorSetLayout(hmlDevice->device, layout, nullptr);
    // }
}


void HmlLightRenderer::specify(uint32_t count) noexcept {
    if (count != pointLightsCount) {
        pointLightsCount = count;
        // if (currentRenderPass) bake();
    }
}


void HmlLightRenderer::bake() noexcept {
    for (size_t imageIndex = 0; imageIndex < descriptorSet_0_perImage.size(); imageIndex++) {
        auto commandBuffer = commandBuffers[imageIndex];
        const auto inheritanceInfo = VkCommandBufferInheritanceInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
            .pNext = VK_NULL_HANDLE,
            .renderPass = currentRenderPass->renderPass,
            .subpass = 0, // we only have a single one
            .framebuffer = currentRenderPass->framebuffers[imageIndex],
            .occlusionQueryEnable = VK_FALSE,
            .queryFlags = static_cast<VkQueryControlFlags>(0),
            .pipelineStatistics = static_cast<VkQueryPipelineStatisticFlags>(0)
        };
        hmlCommands->beginRecordingSecondary(commandBuffer, &inheritanceInfo);

        const auto& hmlPipeline = getCurrentPipeline();
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

        std::array<VkDescriptorSet, 1> descriptorSets = {
            descriptorSet_0_perImage[imageIndex]
        };
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

        const uint32_t instanceCount = pointLightsCount;
        const uint32_t firstInstance = 0;
        const uint32_t vertexCount = 6; // a simple square
        const uint32_t firstVertex = 0;
        vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex, firstInstance);

        hmlCommands->endRecording(commandBuffer);
    }
}


VkCommandBuffer HmlLightRenderer::draw(const HmlFrameData& frameData) noexcept {
    return commandBuffers[frameData.imageIndex];
}


// void HmlLightRenderer::replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept {
//     hmlRenderPass = newHmlRenderPass;
//     hmlPipeline = createPipeline(hmlDevice, hmlRenderPass->extent, hmlRenderPass->renderPass, descriptorSetLayouts);
//     bake();
//     // NOTE The command pool is reset for all renderers prior to calling this function.
//     // NOTE commandBuffers must be rerecorded -- is done during baking
// }
