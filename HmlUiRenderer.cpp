#include "HmlUiRenderer.h"


std::unique_ptr<HmlPipeline> HmlUiRenderer::createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
        VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    HmlGraphicsPipelineConfig config{
        .bindingDescriptions   = {},
        .attributeDescriptions = {},
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .hmlShaders = HmlShaders()
            .addVertex("shaders/out/ui.vert.spv")
            .addFragment("shaders/out/ui.frag.spv"),
        .renderPass = renderPass,
        .extent = extent,
        .polygoneMode = VK_POLYGON_MODE_FILL,
        // .cullMode = VK_CULL_MODE_BACK_BIT,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .descriptorSetLayouts = descriptorSetLayouts,
        .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .pushConstantsSizeBytes = sizeof(PushConstant),
        .tessellationPatchPoints = 0,
        .lineWidth = 1.0f,
    };

    return HmlPipeline::createGraphics(hmlDevice, std::move(config));
}


std::unique_ptr<HmlUiRenderer> HmlUiRenderer::create(
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands,
        std::shared_ptr<HmlRenderPass> hmlRenderPass,
        std::shared_ptr<HmlResourceManager> hmlResourceManager,
        std::shared_ptr<HmlDescriptors> hmlDescriptors,
        uint32_t framesInFlight) noexcept {
    auto hmlRenderer = std::make_unique<HmlUiRenderer>();
    hmlRenderer->hmlWindow = hmlWindow;
    hmlRenderer->hmlDevice = hmlDevice;
    hmlRenderer->hmlCommands = hmlCommands;
    hmlRenderer->hmlRenderPass = hmlRenderPass;
    hmlRenderer->hmlResourceManager = hmlResourceManager;
    hmlRenderer->hmlDescriptors = hmlDescriptors;

    hmlRenderer->textureResource = hmlResourceManager->newTextureResource("models/girl.png", VK_FILTER_LINEAR);


    hmlRenderer->descriptorPool = hmlDescriptors->buildDescriptorPool()
        .withTextures(hmlRenderPass->imageCount() * MAX_TEXTURES_COUNT)
        .maxSets(hmlRenderPass->imageCount())
        .build(hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    hmlRenderer->descriptorSetLayoutTextures = hmlDescriptors->buildDescriptorSetLayout()
        .withTextureArrayAt(0, VK_SHADER_STAGE_FRAGMENT_BIT, MAX_TEXTURES_COUNT)
        .build(hmlDevice);
    if (!hmlRenderer->descriptorSetLayoutTextures) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(hmlRenderer->descriptorSetLayoutTextures);

    hmlRenderer->descriptorSet_textures_0_perImage = hmlDescriptors->createDescriptorSets(hmlRenderPass->imageCount(),
        hmlRenderer->descriptorSetLayoutTextures, hmlRenderer->descriptorPool);
    if (hmlRenderer->descriptorSet_textures_0_perImage.empty()) return { nullptr };


    hmlRenderer->hmlPipeline = createPipeline(hmlDevice, hmlRenderPass->extent,
        hmlRenderPass->renderPass, hmlRenderer->descriptorSetLayouts);
    if (!hmlRenderer->hmlPipeline) return { nullptr };


    hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(framesInFlight, hmlCommands->commandPoolOnetimeFrames);


    std::vector<VkSampler> samplers;
    std::vector<VkImageView> imageViews;
    samplers.push_back(hmlRenderer->textureResource->sampler);
    imageViews.push_back(hmlRenderer->textureResource->imageView);
    for (size_t i = 0; i < hmlRenderPass->imageCount(); i++) {
        HmlDescriptorSetUpdater(hmlRenderer->descriptorSet_textures_0_perImage[i]).textureArrayAt(0, samplers, imageViews).update(hmlDevice);
    }

    return hmlRenderer;
}


HmlUiRenderer::~HmlUiRenderer() noexcept {
    std::cout << ":> Destroying HmlUiRenderer...\n";

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlDevice->device, descriptorSetLayoutTextures, nullptr);
}


// void HmlUiRenderer::specifyEntitiesToRender(const std::vector<std::shared_ptr<Entity>>& entities) noexcept {
//     nextFreeTextureIndex = 0;
//     textureIndexFor.clear();
//     entitiesToRenderForModel.clear();
//
//     for (const auto& entity : entities) {
//         const auto& model = entity->modelResource;
//         const auto id = model->id;
//         if (!textureIndexFor.contains(id)) {
//             entitiesToRenderForModel[id] = {};
//             if (model->textureResource) {
//                 if (!anyModelWithTexture) anyModelWithTexture = model;
//                 textureIndexFor[id] = nextFreeTextureIndex++;
//             } else {
//                 textureIndexFor[id] = NO_TEXTURE_MARK;
//             }
//         }
//         entitiesToRenderForModel[id].push_back(entity);
//     }
//
//     if (nextFreeTextureIndex == 0) {
//         std::cerr << "::> No Entity with a texture has been specified.\n";
//         exit(-1);
//     }
//
//     updateDescriptorSetTextures();
// }


// void HmlUiRenderer::updateDescriptorSetTextures() noexcept {
//     static bool mustDefaultInit = true;
//     const auto usedTexturesCount = mustDefaultInit ? MAX_TEXTURES_COUNT : nextFreeTextureIndex;
//
//     std::vector<VkSampler> samplers(usedTexturesCount);
//     std::vector<VkImageView> imageViews(usedTexturesCount);
//
//     // NOTE Is done only because the validation layer complains that corresponding
//     // textures are not set, although supposedly being used in shared. In
//     // reality, however, the logic is solid and they are not used.
//     if (mustDefaultInit) {
//         mustDefaultInit = false;
//         for (uint32_t i = nextFreeTextureIndex; i < MAX_TEXTURES_COUNT; i++) {
//             samplers[i] = anyModelWithTexture->textureResource->sampler;
//             imageViews[i] = anyModelWithTexture->textureResource->imageView;
//         }
//     }
//
//     // NOTE textureIndex always covers range [0..usedTexturesCount)
//     for (const auto& [modelId, textureIndex] : textureIndexFor) {
//         // Pick any Model with this texture
//         const auto& model = entitiesToRenderForModel[modelId][0]->modelResource;
//         if (!model->textureResource) continue;
//         samplers[textureIndex] = model->textureResource->sampler;
//         imageViews[textureIndex] = model->textureResource->imageView;
//     }
//
//     HmlDescriptorSetUpdater(descriptorSet_textures_0_perImage[0]).textureArrayAt(0, samplers, imageViews).update(hmlDevice);
// }


VkCommandBuffer HmlUiRenderer::draw(uint32_t frameIndex, uint32_t imageIndex) noexcept {
    auto commandBuffer = commandBuffers[frameIndex];
    const auto inheritanceInfo = VkCommandBufferInheritanceInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = VK_NULL_HANDLE,
        .renderPass = hmlRenderPass->renderPass,
        .subpass = 0, // we only have a single one
        .framebuffer = hmlRenderPass->framebuffers[imageIndex],
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = static_cast<VkQueryControlFlags>(0),
        .pipelineStatistics = static_cast<VkQueryPipelineStatisticFlags>(0)
    };
    hmlCommands->beginRecordingSecondaryOnetime(commandBuffer, &inheritanceInfo);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

    std::array<VkDescriptorSet, 1> descriptorSets = {
        descriptorSet_textures_0_perImage[imageIndex]
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    PushConstant pushConstant{
        .textureIndex = 0, // TODO
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


// TODO in order for each type of Renderer to properly replace its pipeline,
// store a member in Renderer which specifies its type, and recreate the pipeline
// based on its value.
void HmlUiRenderer::replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept {
    hmlRenderPass = newHmlRenderPass;
    hmlPipeline = createPipeline(hmlDevice, hmlRenderPass->extent, hmlRenderPass->renderPass, descriptorSetLayouts);
    // NOTE The command pool is reset for all renderers prior to calling this function.
    // NOTE commandBuffers must be rerecorded -- is done during baking
}
