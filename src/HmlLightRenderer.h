#ifndef HML_LIGHT_RENDERER
#define HML_LIGHT_RENDERER

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


struct HmlLightRenderer : HmlDrawer {
    struct PointLight {
        glm::vec3 color;
        float intensity;
        glm::vec3 position;
        float radius;
    };
    uint32_t pointLightsCount = 0;

    // std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    // Store so that we can perform a bake upon RenderPass replacement and upon count specification
    std::vector<VkDescriptorSet> descriptorSet_0_perImage;


    std::vector<std::unique_ptr<HmlPipeline>> createPipelines(
            std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept override;
    static std::unique_ptr<HmlLightRenderer> create(
            std::shared_ptr<HmlContext> hmlContext,
            VkDescriptorSetLayout viewProjDescriptorSetLayout,
            const std::vector<VkDescriptorSet>& generalDescriptorSet_0_perImage) noexcept;
    ~HmlLightRenderer() noexcept;
    void specify(uint32_t count) noexcept;
    void bake() noexcept;
    VkCommandBuffer draw(const HmlFrameData& frameData) noexcept override;
    void addRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept override;
};

#endif
