#ifndef HML_SNOW_PARTICLE_RENDERER
#define HML_SNOW_PARTICLE_RENDERER

#include <memory>
#include <vector>
#include <unordered_map>
#include <optional>
#include <random>

#include "HmlWindow.h"
#include "HmlPipeline.h"
#include "HmlSwapchain.h"
#include "HmlCommands.h"
#include "HmlModel.h"
#include "HmlResourceManager.h"
#include "HmlDescriptors.h"


struct HmlSnowParticleRenderer {
    // static constexpr size_t MAX_SNOW_COUNT = 4000;


    // float, double, long double
    // [low, high)
    template<typename T = float>
    T getRandomUniformFloat(T low, T high) {
        static std::random_device rd;
        /* static std::seed_seq seed{1, 2, 3, 300}; */
        /* static std::mt19937 e2(seed); */
        static std::mt19937 e2(rd());
        std::uniform_real_distribution<T> dist(low, high);

        return dist(e2);
    }


    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlDescriptors> hmlDescriptors;

    std::unique_ptr<HmlPipeline> hmlPipeline;
    // HmlShaderLayout hmlShaderLayout;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet              descriptorSet_textures_1;
    std::vector<VkDescriptorSet> descriptorSet_instances_2_perImage;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkDescriptorSetLayout> descriptorSetLayoutsSelf;


    std::vector<VkCommandBuffer> commandBuffers;


    struct SnowInstance {
        glm::vec3 position;
        float rotation;
    };
    std::vector<SnowInstance> snowInstances;
    std::vector<glm::vec3> snowVelocities;
    std::vector<std::unique_ptr<HmlStorageBuffer>> snowInstancesStorageBuffers;

    struct SnowBounds {
        float xMin;
        float xMax;
        float yMin;
        float yMax;
        float zMin;
        float zMax;
    };
    SnowBounds snowBounds;
    std::vector<std::unique_ptr<HmlTextureResource>> snowTextureResources;


    static std::unique_ptr<HmlPipeline> createSnowPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
            VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) {
        HmlGraphicsPipelineConfig config{
            .bindingDescriptions   = {},
            .attributeDescriptions = {},
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .hmlShaders = HmlShaders()
                .addVertex("shaders/out/snow.vert.spv")
                .addFragment("shaders/out/snow.frag.spv"),
            .renderPass = renderPass,
            .swapchainExtent = extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            // .cullMode = VK_CULL_MODE_BACK_BIT,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayouts = descriptorSetLayouts,
            .pushConstantsStages = 0,
            .pushConstantsSizeBytes = 0
        };

        return HmlPipeline::createGraphics(hmlDevice, std::move(config));
    }


    static std::unique_ptr<HmlSnowParticleRenderer> createSnowRenderer(
            uint32_t snowCount,
            const SnowBounds& snowBounds,
            std::shared_ptr<HmlWindow> hmlWindow,
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlCommands> hmlCommands,
            std::shared_ptr<HmlSwapchain> hmlSwapchain,
            std::shared_ptr<HmlResourceManager> hmlResourceManager,
            std::shared_ptr<HmlDescriptors> hmlDescriptors,
            VkDescriptorSetLayout viewProjDescriptorSetLayout,
            uint32_t framesInFlight) {
        auto hmlRenderer = std::make_unique<HmlSnowParticleRenderer>();
        hmlRenderer->hmlWindow = hmlWindow;
        hmlRenderer->hmlDevice = hmlDevice;
        hmlRenderer->hmlCommands = hmlCommands;
        hmlRenderer->hmlSwapchain = hmlSwapchain;
        hmlRenderer->hmlResourceManager = hmlResourceManager;
        hmlRenderer->hmlDescriptors = hmlDescriptors;
        // if (snowCount > MAX_SNOW_COUNT) {
        //     std::cout << "::> Exceeded MAX_SNOW_COUNT while creating HmlSnowParticleRenderer.\n";
        //     return { nullptr };
        // }
        hmlRenderer->createSnow(snowCount, snowBounds);

        for (size_t i = 0; i < hmlSwapchain->imagesCount(); i++) {
            const auto size = snowCount * 4 * sizeof(float);
            auto ssbo = hmlResourceManager->createStorageBuffer(size);
            ssbo->map();
            hmlRenderer->snowInstancesStorageBuffers.push_back(std::move(ssbo));
        }


        hmlRenderer->snowTextureResources.push_back(hmlResourceManager->newTextureResource("models/snow/snowflake1.png", VK_FILTER_NEAREST));
        hmlRenderer->snowTextureResources.push_back(hmlResourceManager->newTextureResource("models/snow/snowflake2.png", VK_FILTER_NEAREST));


        hmlRenderer->descriptorPool = hmlDescriptors->buildDescriptorPool()
            .withTextures(hmlRenderer->snowTextureResources.size())
            .withStorageBuffers(hmlSwapchain->imagesCount())
            .maxSets(hmlSwapchain->imagesCount() + 1)
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

        hmlRenderer->descriptorSet_instances_2_perImage = hmlDescriptors->createDescriptorSets(hmlSwapchain->imagesCount(),
            descriptorSetLayoutInstances, hmlRenderer->descriptorPool);
        if (hmlRenderer->descriptorSet_instances_2_perImage.empty()) return { nullptr };


        hmlRenderer->hmlPipeline = createSnowPipeline(hmlDevice,
            hmlSwapchain->extent, hmlSwapchain->renderPass, hmlRenderer->descriptorSetLayouts);
        if (!hmlRenderer->hmlPipeline) return { nullptr };


        hmlRenderer->commandBuffers = hmlCommands->allocate(framesInFlight, hmlCommands->commandPoolOnetimeFrames, false);


        for (size_t imageIndex = 0; imageIndex < hmlSwapchain->imagesCount(); imageIndex++) {
            const auto set = hmlRenderer->descriptorSet_instances_2_perImage[imageIndex];
            const auto buffer = hmlRenderer->snowInstancesStorageBuffers[imageIndex]->storageBuffer;
            const auto size = hmlRenderer->snowInstancesStorageBuffers[imageIndex]->sizeBytes;
            HmlDescriptorSetUpdater(set).storageBufferAt(0, buffer, size).update(hmlDevice);
        }


        std::vector<VkSampler> samplers;
        std::vector<VkImageView> imageViews;
        for (const auto& resource : hmlRenderer->snowTextureResources) {
            samplers.push_back(resource->sampler);
            imageViews.push_back(resource->imageView);
        }
        HmlDescriptorSetUpdater(hmlRenderer->descriptorSet_textures_1).textureArrayAt(0, samplers, imageViews).update(hmlDevice);


        return hmlRenderer;
    }


    ~HmlSnowParticleRenderer() {
        std::cout << ":> Destroying HmlSnowParticleRenderer...\n";

        // NOTE depends on swapchain recreation, but because it only depends on the
        // NOTE number of images, which most likely will not change, we ignore it.
        // DescriptorSets are freed automatically upon the deletion of the pool
        vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
        for (auto layout : descriptorSetLayoutsSelf) {
            vkDestroyDescriptorSetLayout(hmlDevice->device, layout, nullptr);
        }
    }


    void createSnow(uint32_t count, const SnowBounds& bounds) {
        snowBounds = bounds;
        snowInstances.resize(count);
        snowVelocities.resize(count);

        for (size_t i = 0; i < snowVelocities.size(); i++) {
            snowVelocities[i] = 0.4f * glm::vec3(0.5f, -2.0f, -0.1f);
        }

        for (size_t i = 0; i < snowInstances.size(); i++) {
            snowInstances[i].position[0] = getRandomUniformFloat(snowBounds.xMin, snowBounds.xMax);
            snowInstances[i].position[1] = getRandomUniformFloat(snowBounds.yMin, snowBounds.yMax);
            snowInstances[i].position[2] = getRandomUniformFloat(snowBounds.zMin, snowBounds.zMax);
            snowInstances[i].rotation = 0.0f; // TODO
        }
    }


    void updateForDt(float dt) {
        static const auto bound = [](const SnowBounds& snowBounds, glm::vec3& pos) {
            const auto xRange = snowBounds.xMax - snowBounds.xMin;
            const auto yRange = snowBounds.yMax - snowBounds.yMin;
            const auto zRange = snowBounds.zMax - snowBounds.zMin;
            auto& x = pos[0];
            auto& y = pos[1];
            auto& z = pos[2];
            if      (x < snowBounds.xMin) x += xRange;
            else if (x > snowBounds.xMax) x -= xRange;
            if      (y < snowBounds.yMin) y += yRange;
            else if (y > snowBounds.yMax) y -= yRange;
            if      (z < snowBounds.zMin) z += zRange;
            else if (z > snowBounds.zMax) z -= zRange;
        };

        static const float rotationVelocity = 1.5f;
        for (size_t i = 0; i < snowVelocities.size(); i++) {
            snowInstances[i].position += dt * snowVelocities[i];
            snowInstances[i].rotation += dt * rotationVelocity;
            bound(snowBounds, snowInstances[i].position);
        }
    }


    void updateForImage(uint32_t imageIndex) {
        snowInstancesStorageBuffers[imageIndex]->update(snowInstances.data());
    }


    VkCommandBuffer draw(uint32_t frameIndex, uint32_t imageIndex, VkDescriptorSet descriptorSet_0) {
        auto commandBuffer = commandBuffers[frameIndex];
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
        hmlCommands->beginRecording(commandBuffer,
            VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT | VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT,
            &inheritanceInfo);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

        std::array<VkDescriptorSet, 3> descriptorSets = {
            descriptorSet_0, descriptorSet_textures_1, descriptorSet_instances_2_perImage[imageIndex]
        };
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

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
    void replaceSwapchain(std::shared_ptr<HmlSwapchain> newHmlSwapChain) {
        hmlSwapchain = newHmlSwapChain;
        hmlPipeline = createSnowPipeline(hmlDevice, hmlSwapchain->extent, hmlSwapchain->renderPass, descriptorSetLayouts);
        // NOTE The command pool is reset for all renderers prior to calling this function.
        // NOTE commandBuffers must be rerecorded -- is done during baking
    }
};

#endif
