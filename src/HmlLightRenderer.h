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
#include "renderer.h"


struct HmlLightRenderer : HmlDrawer {
    struct PointLight {
        glm::vec3 color;
        float intensity;
        glm::vec3 position;
        float radius;
    };
    uint32_t pointLightsCount = 0;

    std::shared_ptr<HmlWindow> hmlWindow;
    // std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    // std::shared_ptr<HmlRenderPass> hmlRenderPass;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlDescriptors> hmlDescriptors;
    // std::unique_ptr<HmlPipeline> hmlPipeline;
    // std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkCommandBuffer> commandBuffers;

    // Store so that we can perform a bake upon RenderPass replacement and upon count specification
    std::vector<VkDescriptorSet> descriptorSet_0_perImage;


    std::unique_ptr<HmlPipeline> createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
            VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept override;
    static std::unique_ptr<HmlLightRenderer> create(
            std::shared_ptr<HmlWindow> hmlWindow,
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlCommands> hmlCommands,
            // std::shared_ptr<HmlRenderPass> hmlRenderPass,
            std::shared_ptr<HmlResourceManager> hmlResourceManager,
            std::shared_ptr<HmlDescriptors> hmlDescriptors,
            VkDescriptorSetLayout viewProjDescriptorSetLayout,
            const std::vector<VkDescriptorSet>& generalDescriptorSet_0_perImage) noexcept;
    ~HmlLightRenderer() noexcept;
    void specify(uint32_t count) noexcept;
    void bake() noexcept;
    VkCommandBuffer draw(const HmlFrameData& frameData) noexcept override;
    // void replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept override;

    inline void addRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept override {
        if (pipelineForRenderPassStorage.size() > 0) {
            std::cerr << "::> Adding more than one HmlRenderPass for HmlLightRenderer.\n";
            return;
        }
        auto hmlPipeline = createPipeline(hmlDevice, newHmlRenderPass->extent, newHmlRenderPass->renderPass, descriptorSetLayouts);
        pipelineForRenderPassStorage.emplace_back(newHmlRenderPass, std::move(hmlPipeline));
        currentRenderPass = newHmlRenderPass;
        bake();
    }
};

#endif
