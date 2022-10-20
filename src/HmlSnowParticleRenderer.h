#ifndef HML_SNOW_PARTICLE_RENDERER
#define HML_SNOW_PARTICLE_RENDERER

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <optional>
#include <variant>

#include "settings.h"
#include "HmlContext.h"
#include "HmlPipeline.h"
#include "HmlRenderPass.h"
#include "HmlModel.h"
#include "util.h"
#include "renderer.h"


struct HmlSnowParticleRenderer : HmlDrawer {
    static constexpr float SNOW_MODE_BOX = +1.0f;
    static constexpr float SNOW_MODE_CAMERA = -1.0f;
    struct PushConstant {
        glm::vec3 halfSize;
        float time;
        glm::vec3 velocity;
        float snowMode;
    };
    float sinceStart;


    std::shared_ptr<HmlWindow> hmlWindow;

    // HmlShaderLayout hmlShaderLayout;

    VkDescriptorPool descriptorPool;
    VkDescriptorSet              descriptorSet_textures_1;
    // NOTE perImage because snowInstancesStorageBuffers are perImage
    std::vector<VkDescriptorSet> descriptorSet_instances_2_perImage;
    // std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkDescriptorSetLayout> descriptorSetLayoutsSelf;


    struct SnowInstance {
        glm::vec3 position;
        float angleRadians;
    };
    std::vector<SnowInstance> snowInstances;
    std::vector<glm::vec3> snowVelocities;
    // NOTE for the case when we use SnowCameraBounds, having multiple buffers is redundant
    std::vector<std::unique_ptr<HmlBuffer>> snowInstancesStorageBuffers;

    struct SnowBoxBounds {
        float xMin;
        float xMax;
        float yMin;
        float yMax;
        float zMin;
        float zMax;
    };
    using SnowCameraBounds = float;
    using SnowBounds = std::variant<SnowBoxBounds, SnowCameraBounds>;
    SnowBounds snowBounds;
    std::vector<std::unique_ptr<HmlImageResource>> snowTextureResources;


    std::vector<std::unique_ptr<HmlPipeline>> createPipelines(
            std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept override;
    static std::unique_ptr<HmlSnowParticleRenderer> createSnowRenderer(
            uint32_t snowCount,
            const std::variant<SnowBoxBounds, SnowCameraBounds>& snowBounds,
            std::shared_ptr<HmlContext> hmlContext,
            VkDescriptorSetLayout viewProjDescriptorSetLayout) noexcept;
    ~HmlSnowParticleRenderer() noexcept;
    void createSnow(uint32_t count, const SnowBounds& bounds) noexcept;
    void updateForDt(float dt, float timeSinceStart) noexcept;
    void updateForImage(uint32_t imageIndex) noexcept;
    VkCommandBuffer draw(const HmlFrameData& frameData) noexcept override;
};

#endif
