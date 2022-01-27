#ifndef HML_DEFERRED_RENDERER
#define HML_DEFERRED_RENDERER

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <optional>

#include "HmlWindow.h"
#include "HmlPipeline.h"
#include "HmlCommands.h"
#include "HmlModel.h"
#include "HmlDevice.h"
#include "HmlResourceManager.h"
#include "HmlDescriptors.h"
#include "HmlRenderPass.h"


// NOTE
// In theory, it is easily possible to also recreate desriptor stuff upon
// swapchain recreation by recreating the whole Renderer object.
// NOTE
struct HmlDeferredRenderer {
    // struct PushConstant {
    //     int32_t textureIndex;
    //     float shift;
    // };


    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlRenderPass> hmlRenderPass;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlDescriptors> hmlDescriptors;

    std::unique_ptr<HmlPipeline> hmlPipeline;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet>  descriptorSet_textures_0_perImage;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts; // NOTE stores only 1 (for textures)
    VkDescriptorSetLayout descriptorSetLayoutTextures;

    // std::unique_ptr<HmlImageResource> textureResource;
    std::vector<std::shared_ptr<HmlImageResource>> imageResources;


    static constexpr uint32_t G_COUNT = 3; // XXX must match the shader
    // static constexpr int32_t NO_TEXTURE_MARK = -1;
    // uint32_t texturesCount = 0;

    // std::shared_ptr<HmlModelResource> anyModelWithTexture;
    // std::unordered_map<HmlModelResource::Id, int32_t> textureIndexFor;
    // std::unordered_map<HmlModelResource::Id, std::vector<std::shared_ptr<Entity>>> entitiesToRenderForModel;


    std::vector<VkCommandBuffer> commandBuffers;


    static std::unique_ptr<HmlPipeline> createPipeline(std::shared_ptr<HmlDevice> hmlDevice, VkExtent2D extent,
        VkRenderPass renderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept;
    static std::unique_ptr<HmlDeferredRenderer> create(
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlDevice> hmlDevice,
        std::shared_ptr<HmlCommands> hmlCommands,
        std::shared_ptr<HmlRenderPass> hmlRenderPass,
        std::shared_ptr<HmlResourceManager> hmlResourceManager,
        std::shared_ptr<HmlDescriptors> hmlDescriptors,
        uint32_t framesInFlight) noexcept;
    ~HmlDeferredRenderer() noexcept;
    void specify(const std::array<std::vector<std::shared_ptr<HmlImageResource>>, G_COUNT>& resources) noexcept;
    VkCommandBuffer draw(uint32_t frameIndex, uint32_t imageIndex) noexcept;

    // TODO in order for each type of Renderer to properly replace its pipeline,
    // store a member in Renderer which specifies its type, and recreate the pipeline
    // based on its value.
    void replaceRenderPass(std::shared_ptr<HmlRenderPass> newHmlRenderPass) noexcept;
};

#endif
