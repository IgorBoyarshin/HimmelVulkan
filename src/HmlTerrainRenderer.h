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
#include "HmlRenderPass.h"
#include "HmlCommands.h"
#include "HmlModel.h"
#include "HmlDevice.h"
#include "HmlResourceManager.h"
#include "HmlDescriptors.h"
#include "renderer.h"


struct HmlTerrainRenderer : HmlDrawer {
    struct Patch {
        inline Patch(const glm::vec2& center, const glm::vec2& size,
                const glm::vec2& texCoordStart, const glm::vec2& texCoordStep, int level) noexcept
            : center(center), size(size), texCoordStart(texCoordStart), texCoordStep(texCoordStep), level(level) {}

        glm::vec2 center;
        glm::vec2 size;
        glm::vec2 texCoordStart;
        glm::vec2 texCoordStep;
        int level; // 0 for root
        bool isParent = false;
    };

    struct SubTerrain {
        glm::vec2 center;
        glm::vec2 size;
        glm::vec2 texCoordStart;
        glm::vec2 texCoordStep;
        std::vector<Patch> patches;
        inline SubTerrain(const glm::vec2& center, const glm::vec2& size,
                const glm::vec2& texCoordStart, const glm::vec2& texCoordStep) noexcept
            : center(center), size(size), texCoordStart(texCoordStart), texCoordStep(texCoordStep) {}
    };
    std::vector<SubTerrain> subTerrains;
    uint32_t granularity;

    struct PushConstant {
        glm::vec2 center;
        glm::vec2 size;
        glm::vec2 texCoordStart;
        glm::vec2 texCoordStep;
        float offsetY;
        float maxHeight;
        int level;
    };

    struct Bounds {
        glm::vec2 posStart;
        glm::vec2 posFinish;
        float height;
        float yOffset;
    };
    Bounds bounds;

    std::shared_ptr<HmlWindow> hmlWindow;
    // std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    // std::shared_ptr<HmlRenderPass> hmlRenderPass;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlDescriptors> hmlDescriptors;
    // std::unique_ptr<HmlPipeline> hmlPipeline;
    // std::unique_ptr<HmlPipeline> hmlPipelineDebug;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet              descriptorSet_heightmap_1;
    // std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkDescriptorSetLayout> descriptorSetLayoutsSelf;


    std::vector<VkCommandBuffer> commandBuffers;

    std::unique_ptr<HmlImageResource> heightmapTexture;
    std::unique_ptr<HmlImageResource> grassTexture;


    std::unique_ptr<HmlPipeline> createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
            VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept override;
    // static std::unique_ptr<HmlPipeline> createPipelineDebug(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
    //         VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept;
    static std::unique_ptr<HmlTerrainRenderer> create(
            const char* heightmapFilename,
            uint32_t granularity,
            const char* grassFilename,
            const Bounds& bounds,
            std::shared_ptr<HmlWindow> hmlWindow,
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlCommands> hmlCommands,
            // std::shared_ptr<HmlRenderPass> hmlRenderPass,
            std::shared_ptr<HmlResourceManager> hmlResourceManager,
            std::shared_ptr<HmlDescriptors> hmlDescriptors,
            VkDescriptorSetLayout viewProjDescriptorSetLayout,
            uint32_t imageCount,
            uint32_t framesInFlight) noexcept;
    ~HmlTerrainRenderer() noexcept;
    void constructTree(SubTerrain& subTerrain, const glm::vec3& cameraPos) const noexcept;
    void update(const glm::vec3& cameraPos) noexcept;
    VkCommandBuffer draw(const HmlFrameData& frameData) noexcept override;
    // void replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept override;
};

#endif
