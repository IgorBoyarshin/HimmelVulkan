#include <memory>
#include <vector>
#include <chrono>
#include <optional>
#include <iostream>

#include "HmlWindow.h"
#include "HmlDevice.h"
#include "HmlDescriptors.h"
#include "HmlCommands.h"
#include "HmlSwapchain.h"
#include "HmlResourceManager.h"
#include "HmlRenderer.h"
#include "HmlSnowParticleRenderer.h"
#include "HmlTerrainRenderer.h"
#include "HmlModel.h"
#include "HmlCamera.h"
#include "util.h"
#include "HmlRenderPass.h"


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

    struct PointLight {
        glm::vec3 color;
        float intensity;
        glm::vec3 position;
        float reserved;
    };
    std::vector<PointLight> pointLights;

    static const uint32_t MAX_POINT_LIGHTS = 32;
    struct LightUbo {
        PointLight pointLights[MAX_POINT_LIGHTS];
        uint32_t count;
    };

    struct Weather {
        glm::vec3 fogColor;
        float fogDensity;
    };
    Weather weather;

    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlDepthResource> hmlDepthResource;
    std::shared_ptr<HmlDescriptors> hmlDescriptors;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;
    std::shared_ptr<HmlRenderPass> hmlRenderPassGeneral;
    std::shared_ptr<HmlRenderer> hmlRenderer;
    std::shared_ptr<HmlTerrainRenderer> hmlTerrainRenderer;
    std::shared_ptr<HmlSnowParticleRenderer> hmlSnowRenderer;


    HmlCamera camera;
    glm::mat4 proj;
    std::pair<int32_t, int32_t> cursor;


    std::vector<VkCommandBuffer> commandBuffers;


    const glm::vec4 FOG_COLOR = glm::vec4(0.7, 0.7, 0.7, 1.0);


    // Sync objects
    uint32_t maxFramesInFlight = 2; // TODO make a part of SimpleRenderer creation TODO ????
    std::vector<VkSemaphore> imageAvailableSemaphores; // for each frame in flight
    std::vector<VkSemaphore> renderFinishedSemaphores; // for each frame in flight
    std::vector<VkFence> inFlightFences;               // for each frame in flight
    std::vector<VkFence> imagesInFlight;               // for each swapChainImage

    std::vector<std::unique_ptr<HmlUniformBuffer>> viewProjUniformBuffers;

    // NOTE if they are static, we can share a single UBO
    std::vector<std::unique_ptr<HmlUniformBuffer>> lightUniformBuffers;


    VkDescriptorPool generalDescriptorPool;
    VkDescriptorSetLayout generalDescriptorSetLayout;
    std::vector<VkDescriptorSet> generalDescriptorSet_0_perImage;


    // NOTE Later it will probably be a hashmap from model name
    std::vector<std::shared_ptr<HmlModelResource>> models;

    std::vector<std::shared_ptr<HmlRenderer::Entity>> entities;


    bool init() noexcept;
    void run() noexcept;
    void updateForDt(float dt, float sinceStart) noexcept;
    void updateForImage(uint32_t imageIndex) noexcept;
    bool drawFrame() noexcept;
    void recordDrawBegin(VkCommandBuffer commandBuffer, uint32_t imageIndex) noexcept;
    void recordDrawEnd(VkCommandBuffer commandBuffer) noexcept;
    void recreateSwapchain() noexcept;
    bool createSyncObjects() noexcept;
    ~Himmel() noexcept;
    static glm::mat4 projFrom(float aspect_w_h) noexcept;
};
