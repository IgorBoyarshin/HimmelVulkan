#include <memory>
#include <vector>
#include <chrono>
#include <optional>
#include <iostream>
#include <algorithm>

#include "HmlWindow.h"
#include "HmlDevice.h"
#include "HmlDescriptors.h"
#include "HmlCommands.h"
#include "HmlSwapchain.h"
#include "HmlResourceManager.h"
#include "HmlRenderer.h"
#include "HmlSnowParticleRenderer.h"
#include "HmlTerrainRenderer.h"
#include "HmlUiRenderer.h"
// #include "HmlBlurRenderer.h"
#include "HmlBloomRenderer.h"
#include "HmlDeferredRenderer.h"
#include "HmlLightRenderer.h"
#include "HmlModel.h"
#include "HmlCamera.h"
#include "util.h"
#include "HmlRenderPass.h"
#include "HmlPipe.h"


struct Himmel {
    struct GeneralUbo {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        glm::vec3 globalLightDir;
        float ambientStrength;
        glm::vec3 fogColor;
        float fogDensity;
        glm::vec3 cameraPos;
    };

    std::vector<HmlLightRenderer::PointLight> pointLightsStatic;
    std::vector<HmlLightRenderer::PointLight> pointLightsDynamic;

    struct LightUbo {
        std::array<HmlLightRenderer::PointLight, 32> pointLights;
        uint32_t count;
    };

    struct Weather {
        glm::vec3 fogColor;
        float fogDensity;
    };
    Weather weather;
    const glm::vec4 FOG_COLOR = glm::vec4(0.7, 0.7, 0.7, 1.0);

    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlDescriptors> hmlDescriptors;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;
    std::shared_ptr<HmlRenderPass> hmlRenderPassDeferredPrep;
    std::shared_ptr<HmlRenderPass> hmlRenderPassDeferred;
    std::shared_ptr<HmlRenderPass> hmlRenderPassForward;
    std::shared_ptr<HmlRenderPass> hmlRenderPassUi;
    std::shared_ptr<HmlRenderer> hmlRenderer;
    std::shared_ptr<HmlUiRenderer> hmlUiRenderer;
    // std::shared_ptr<HmlBlurRenderer> hmlBlurRenderer;
    std::shared_ptr<HmlBloomRenderer> hmlBloomRenderer;
    std::shared_ptr<HmlDeferredRenderer> hmlDeferredRenderer;
    std::shared_ptr<HmlTerrainRenderer> hmlTerrainRenderer;
    std::shared_ptr<HmlSnowParticleRenderer> hmlSnowRenderer;
    std::shared_ptr<HmlLightRenderer> hmlLightRenderer;

    std::vector<std::shared_ptr<HmlImageResource>> gBufferPositions;
    std::vector<std::shared_ptr<HmlImageResource>> gBufferNormals;
    std::vector<std::shared_ptr<HmlImageResource>> gBufferColors;
    std::vector<std::shared_ptr<HmlImageResource>> brightness1Textures;
    // std::vector<std::shared_ptr<HmlImageResource>> brightness2Textures;
    std::vector<std::shared_ptr<HmlImageResource>> mainTextures;
    std::shared_ptr<HmlImageResource> hmlDepthResource;


    HmlCamera camera;
    glm::mat4 proj;
    std::pair<int32_t, int32_t> cursor;


    // Sync objects
    const uint32_t maxFramesInFlight = 2;
    std::vector<VkSemaphore> imageAvailableSemaphores; // for each frame in flight
    std::vector<VkSemaphore> renderFinishedSemaphores; // for each frame in flight
    std::vector<VkFence> inFlightFences;               // for each frame in flight
    std::vector<VkFence> imagesInFlight;               // for each swapChainImage

    std::vector<std::unique_ptr<HmlBuffer>> viewProjUniformBuffers;

    // NOTE if they are static, we can share a single UBO
    std::vector<std::unique_ptr<HmlBuffer>> lightUniformBuffers;


    VkDescriptorPool generalDescriptorPool;
    VkDescriptorSetLayout generalDescriptorSetLayout;
    std::vector<VkDescriptorSet> generalDescriptorSet_0_perImage;

    std::unique_ptr<HmlPipe> hmlPipe;


    // NOTE Later it will probably be a hashmap from model name
    std::vector<std::shared_ptr<HmlModelResource>> models;

    std::vector<std::shared_ptr<HmlRenderer::Entity>> entities;

    bool successfulInit = false;

    bool init() noexcept;
    bool run() noexcept;
    void updateForDt(float dt, float sinceStart) noexcept;
    void updateForImage(uint32_t imageIndex) noexcept;
    bool drawFrame() noexcept;
    void recordDrawBegin(VkCommandBuffer commandBuffer, uint32_t imageIndex) noexcept;
    void recordDrawEnd(VkCommandBuffer commandBuffer) noexcept;
    bool prepareResources() noexcept;
    void recreateSwapchain() noexcept;
    bool createSyncObjects() noexcept;
    ~Himmel() noexcept;
    static glm::mat4 projFrom(float aspect_w_h) noexcept;
};
