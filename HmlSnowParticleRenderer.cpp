#include "HmlSnowParticleRenderer.h"


std::unique_ptr<HmlPipeline> HmlSnowParticleRenderer::createSnowPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
        VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    HmlGraphicsPipelineConfig config{
        .bindingDescriptions   = {},
        .attributeDescriptions = {},
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .hmlShaders = HmlShaders()
            .addVertex("shaders/out/snow.vert.spv")
            .addFragment("shaders/out/snow.frag.spv"),
        .renderPass = renderPass,
        .extent = extent,
        .polygoneMode = VK_POLYGON_MODE_FILL,
        // .cullMode = VK_CULL_MODE_BACK_BIT,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .descriptorSetLayouts = descriptorSetLayouts,
        .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT,
        .pushConstantsSizeBytes = sizeof(PushConstant),
        .tessellationPatchPoints = 0,
        .lineWidth = 1.0f,
    };

    return HmlPipeline::createGraphics(hmlDevice, std::move(config));
}


std::unique_ptr<HmlSnowParticleRenderer> HmlSnowParticleRenderer::createSnowRenderer(
        uint32_t snowCount,
        const SnowBounds& snowBounds,
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands,
        std::shared_ptr<HmlRenderPass> hmlRenderPass,
        std::shared_ptr<HmlResourceManager> hmlResourceManager,
        std::shared_ptr<HmlDescriptors> hmlDescriptors,
        VkDescriptorSetLayout viewProjDescriptorSetLayout,
        uint32_t framesInFlight) noexcept {
    auto hmlRenderer = std::make_unique<HmlSnowParticleRenderer>();
    hmlRenderer->hmlWindow = hmlWindow;
    hmlRenderer->hmlDevice = hmlDevice;
    hmlRenderer->hmlCommands = hmlCommands;
    hmlRenderer->hmlRenderPass = hmlRenderPass;
    hmlRenderer->hmlResourceManager = hmlResourceManager;
    hmlRenderer->hmlDescriptors = hmlDescriptors;
    // if (snowCount > MAX_SNOW_COUNT) {
    //     std::cout << "::> Exceeded MAX_SNOW_COUNT while creating HmlSnowParticleRenderer.\n";
    //     return { nullptr };
    // }
    hmlRenderer->createSnow(snowCount, snowBounds);

    for (size_t i = 0; i < hmlRenderPass->imageCount(); i++) {
        const auto size = snowCount * 4 * sizeof(float);
        auto ssbo = hmlResourceManager->createStorageBuffer(size);
        ssbo->map();
        if (std::holds_alternative<SnowCameraBounds>(snowBounds)) {
            ssbo->update(hmlRenderer->snowInstances.data());
        }
        hmlRenderer->snowInstancesStorageBuffers.push_back(std::move(ssbo));
    }


    hmlRenderer->snowTextureResources.push_back(hmlResourceManager->newTextureResource("models/snow/snowflake1.png", VK_FILTER_NEAREST));
    hmlRenderer->snowTextureResources.push_back(hmlResourceManager->newTextureResource("models/snow/snowflake2.png", VK_FILTER_NEAREST));


    hmlRenderer->descriptorPool = hmlDescriptors->buildDescriptorPool()
        .withTextures(hmlRenderer->snowTextureResources.size())
        .withStorageBuffers(hmlRenderPass->imageCount())
        .maxSets(hmlRenderPass->imageCount() + 1)
        .build(hmlDevice);
    if (!hmlRenderer->descriptorPool) return { nullptr };

    // TODO NOTE set number is specified implicitly by vector index
    hmlRenderer->descriptorSetLayouts.push_back(viewProjDescriptorSetLayout);
    const auto descriptorSetLayoutTextures = hmlDescriptors->buildDescriptorSetLayout()
        .withTextureArrayAt(0, VK_SHADER_STAGE_FRAGMENT_BIT, hmlRenderer->snowTextureResources.size())
        .build(hmlDevice);
    if (!descriptorSetLayoutTextures) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(descriptorSetLayoutTextures);
    hmlRenderer->descriptorSetLayoutsSelf.push_back(descriptorSetLayoutTextures);

    const auto descriptorSetLayoutInstances = hmlDescriptors->buildDescriptorSetLayout()
        .withStorageBufferAt(0, VK_SHADER_STAGE_VERTEX_BIT)
        .build(hmlDevice);
    if (!descriptorSetLayoutInstances) return { nullptr };
    hmlRenderer->descriptorSetLayouts.push_back(descriptorSetLayoutInstances);
    hmlRenderer->descriptorSetLayoutsSelf.push_back(descriptorSetLayoutInstances);

    hmlRenderer->descriptorSet_textures_1 = hmlDescriptors->createDescriptorSets(1,
        descriptorSetLayoutTextures, hmlRenderer->descriptorPool)[0];
    if (!hmlRenderer->descriptorSet_textures_1) return { nullptr };

    hmlRenderer->descriptorSet_instances_2_perImage = hmlDescriptors->createDescriptorSets(hmlRenderPass->imageCount(),
        descriptorSetLayoutInstances, hmlRenderer->descriptorPool);
    if (hmlRenderer->descriptorSet_instances_2_perImage.empty()) return { nullptr };


    hmlRenderer->hmlPipeline = createSnowPipeline(hmlDevice,
        hmlRenderPass->extent, hmlRenderPass->renderPass, hmlRenderer->descriptorSetLayouts);
    if (!hmlRenderer->hmlPipeline) return { nullptr };


    hmlRenderer->commandBuffers = hmlCommands->allocateSecondary(framesInFlight, hmlCommands->commandPoolOnetimeFrames);


    for (size_t imageIndex = 0; imageIndex < hmlRenderPass->imageCount(); imageIndex++) {
        const auto set = hmlRenderer->descriptorSet_instances_2_perImage[imageIndex];
        const auto buffer = hmlRenderer->snowInstancesStorageBuffers[imageIndex]->buffer;
        const auto size = hmlRenderer->snowInstancesStorageBuffers[imageIndex]->sizeBytes;
        HmlDescriptorSetUpdater(set).storageBufferAt(0, buffer, size).update(hmlDevice);
    }


    std::vector<VkSampler> samplers;
    std::vector<VkImageView> imageViews;
    for (const auto& resource : hmlRenderer->snowTextureResources) {
        samplers.push_back(resource->sampler);
        imageViews.push_back(resource->view);
    }
    HmlDescriptorSetUpdater(hmlRenderer->descriptorSet_textures_1).textureArrayAt(0, samplers, imageViews).update(hmlDevice);


    return hmlRenderer;
}


HmlSnowParticleRenderer::~HmlSnowParticleRenderer() noexcept {
    std::cout << ":> Destroying HmlSnowParticleRenderer...\n";

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
    for (auto layout : descriptorSetLayoutsSelf) {
        vkDestroyDescriptorSetLayout(hmlDevice->device, layout, nullptr);
    }
}


void HmlSnowParticleRenderer::createSnow(uint32_t count, const SnowBounds& bounds) noexcept {
    snowBounds = bounds;

    snowInstances.resize(count);

    if (std::holds_alternative<SnowBoxBounds>(snowBounds)) {
        snowVelocities.resize(count);

        for (size_t i = 0; i < snowVelocities.size(); i++) {
            snowVelocities[i] = 0.4f * glm::vec3(0.5f, -2.0f, -0.1f);
        }

        const auto& bounds = std::get<SnowBoxBounds>(snowBounds);
        for (size_t i = 0; i < snowInstances.size(); i++) {
            snowInstances[i].position[0] = hml::getRandomUniformFloat(bounds.xMin, bounds.xMax);
            snowInstances[i].position[1] = hml::getRandomUniformFloat(bounds.yMin, bounds.yMax);
            snowInstances[i].position[2] = hml::getRandomUniformFloat(bounds.zMin, bounds.zMax);
            snowInstances[i].angleRadians = hml::getRandomUniformFloat(0.0f, 6.28f);
        }
    } else { // SnowCameraBounds
        for (size_t i = 0; i < snowInstances.size(); i++) {
            snowInstances[i].position[0] = hml::getRandomUniformFloat(-1.0f, 1.0f);
            snowInstances[i].position[1] = hml::getRandomUniformFloat(-1.0f, 1.0f);
            snowInstances[i].position[2] = hml::getRandomUniformFloat(-1.0f, 1.0f);
            snowInstances[i].angleRadians = hml::getRandomUniformFloat(0.0f, 6.28f);
        }
    }
}


void HmlSnowParticleRenderer::updateForDt(float dt, float timeSinceStart) noexcept {
    if (std::holds_alternative<SnowBoxBounds>(snowBounds)) {
        const auto& bounds = std::get<SnowBoxBounds>(snowBounds);
        static const auto bound = [](const SnowBoxBounds& bounds, glm::vec3& pos) {
            const auto xRange = bounds.xMax - bounds.xMin;
            const auto yRange = bounds.yMax - bounds.yMin;
            const auto zRange = bounds.zMax - bounds.zMin;
            auto& x = pos[0];
            auto& y = pos[1];
            auto& z = pos[2];
            if      (x < bounds.xMin) x += xRange;
            else if (x > bounds.xMax) x -= xRange;
            if      (y < bounds.yMin) y += yRange;
            else if (y > bounds.yMax) y -= yRange;
            if      (z < bounds.zMin) z += zRange;
            else if (z > bounds.zMax) z -= zRange;
        };

        static const float rotationVelocity = 1.5f;
        for (size_t i = 0; i < snowVelocities.size(); i++) {
            snowInstances[i].position += dt * snowVelocities[i];
            snowInstances[i].angleRadians += dt * rotationVelocity;
            bound(bounds, snowInstances[i].position);
        }
    } else { // SnowCameraBounds
        sinceStart = timeSinceStart;
    }
}


void HmlSnowParticleRenderer::updateForImage(uint32_t imageIndex) noexcept {
    if (std::holds_alternative<SnowCameraBounds>(snowBounds)) {
        snowInstancesStorageBuffers[imageIndex]->update(snowInstances.data());
    }
}


VkCommandBuffer HmlSnowParticleRenderer::draw(uint32_t frameIndex, uint32_t imageIndex, VkDescriptorSet descriptorSet_0) noexcept {
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

    std::array<VkDescriptorSet, 3> descriptorSets = {
        descriptorSet_0, descriptorSet_textures_1, descriptorSet_instances_2_perImage[imageIndex]
    };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    if (!std::holds_alternative<SnowCameraBounds>(snowBounds)) {
        std::cerr << "::> Unsupported SnowBounds type in SnowRenderer.\n";
        return VK_NULL_HANDLE;
    }
    const float half = std::get<SnowCameraBounds>(snowBounds);
    PushConstant pushConstant{
        .halfSize = glm::vec3(half, half, half),
        .time = sinceStart,
        .velocity = 0.7f * glm::vec3(0.6f, -2.0f, -0.2f), // TODO make interesting
        .snowMode = SNOW_MODE_CAMERA,
    };
    vkCmdPushConstants(commandBuffer, hmlPipeline->layout,
        hmlPipeline->pushConstantsStages, 0, sizeof(PushConstant), &pushConstant);


    const uint32_t instanceCount = snowInstances.size();
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
void HmlSnowParticleRenderer::replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept {
    hmlRenderPass = newHmlRenderPass;
    hmlPipeline = createSnowPipeline(hmlDevice, hmlRenderPass->extent, hmlRenderPass->renderPass, descriptorSetLayouts);
    // NOTE The command pool is reset for all renderers prior to calling this function.
    // NOTE commandBuffers must be rerecorded -- is done during baking
}
