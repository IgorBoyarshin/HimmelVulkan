#ifndef HML_LIGHT_RENDERER
#define HML_LIGHT_RENDERER

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <optional>
#include <variant>

#include "HmlWindow.h"
#include "HmlPipeline.h"
#include "HmlRenderPass.h"
#include "HmlCommands.h"
#include "HmlModel.h"
#include "HmlDevice.h"
#include "HmlResourceManager.h"
#include "HmlDescriptors.h"
#include "util.h"


struct HmlLightRenderer {
    struct PointLight {
        glm::vec3 color;
        float intensity;
        glm::vec3 position;
        float radius;
    };
    uint32_t pointLightsCount = 0;

    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlRenderPass> hmlRenderPass;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlDescriptors> hmlDescriptors;
    std::unique_ptr<HmlPipeline> hmlPipeline;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkCommandBuffer> commandBuffers;

    // Store so that we can perform a bake upon RenderPass replacement and upon count specification
    std::vector<VkDescriptorSet> descriptorSet_0_perImage;


    static std::unique_ptr<HmlPipeline> createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
            VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept;
    static std::unique_ptr<HmlLightRenderer> create(
            std::shared_ptr<HmlWindow> hmlWindow,
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlCommands> hmlCommands,
            std::shared_ptr<HmlRenderPass> hmlRenderPass,
            std::shared_ptr<HmlResourceManager> hmlResourceManager,
            std::shared_ptr<HmlDescriptors> hmlDescriptors,
            VkDescriptorSetLayout viewProjDescriptorSetLayout,
            const std::vector<VkDescriptorSet>& generalDescriptorSet_0_perImage) noexcept;
    ~HmlLightRenderer() noexcept;
    void specify(uint32_t count) noexcept;
    void bake() noexcept;
    VkCommandBuffer draw(uint32_t imageIndex) noexcept;

    // TODO in order for each type of Renderer to properly replace its pipeline,
    // store a member in Renderer which specifies its type, and recreate the pipeline
    // based on its value.
    void replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept;
};

#endif
