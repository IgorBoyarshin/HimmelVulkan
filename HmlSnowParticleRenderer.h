#ifndef HML_SNOW_PARTICLE_RENDERER
#define HML_SNOW_PARTICLE_RENDERER

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <optional>
#include <random>

#include "HmlWindow.h"
#include "HmlPipeline.h"
#include "HmlSwapchain.h"
#include "HmlCommands.h"
#include "HmlModel.h"
#include "HmlDevice.h"
#include "HmlResourceManager.h"
#include "HmlDescriptors.h"


struct HmlSnowParticleRenderer {
    // TODO move from here
    // float, double, long double
    // [low, high)
    template<typename T = float>
    inline T getRandomUniformFloat(T low, T high) {
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
        float angleRadians;
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
            VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept;
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
            uint32_t framesInFlight) noexcept;
    ~HmlSnowParticleRenderer() noexcept;
    void createSnow(uint32_t count, const SnowBounds& bounds) noexcept;
    void updateForDt(float dt) noexcept;
    void updateForImage(uint32_t imageIndex) noexcept;
    VkCommandBuffer draw(uint32_t frameIndex, uint32_t imageIndex, VkDescriptorSet descriptorSet_0) noexcept;

    // TODO in order for each type of Renderer to properly replace its pipeline,
    // store a member in Renderer which specifies its type, and recreate the pipeline
    // based on its value.
    void replaceSwapchain(std::shared_ptr<HmlSwapchain> newHmlSwapChain) noexcept;
};

#endif
