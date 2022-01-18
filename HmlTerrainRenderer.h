#ifndef HML_TERRAIN_RENDERER
#define HML_TERRAIN_RENDERER

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


struct HmlTerrainRenderer {
    struct PushConstant {
        glm::vec4 posStartFinish;
        uint32_t dimX;
        uint32_t dimY;
        uint32_t dimZ;
        uint32_t offsetY;
    };

    struct Bounds {
        glm::vec2 posStart;
        glm::vec2 posFinish;
        uint32_t height;
        uint32_t yOffset;
    };
    Bounds bounds;

    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlDescriptors> hmlDescriptors;
    std::unique_ptr<HmlPipeline> hmlPipeline;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet              descriptorSet_heightmap_1;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkDescriptorSetLayout> descriptorSetLayoutsSelf;


    std::vector<VkCommandBuffer> commandBuffers;

    std::unique_ptr<HmlTextureResource> heightmapTexture;
    std::unique_ptr<HmlTextureResource> grassTexture;


    static std::unique_ptr<HmlPipeline> createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
            VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept;
    static std::unique_ptr<HmlTerrainRenderer> create(
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
            uint32_t framesInFlight) noexcept;
    ~HmlTerrainRenderer() noexcept;
    void bake(const std::vector<VkDescriptorSet>& descriptorSet_0_perImage) noexcept;
    VkCommandBuffer draw(uint32_t imageIndex) noexcept;

    // TODO in order for each type of Renderer to properly replace its pipeline,
    // store a member in Renderer which specifies its type, and recreate the pipeline
    // based on its value.
    void replaceSwapchain(std::shared_ptr<HmlSwapchain> newHmlSwapChain) noexcept;
};

#endif
