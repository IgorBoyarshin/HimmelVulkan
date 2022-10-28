#ifndef HML_HIMMEL
#define HML_HIMMEL

#include <memory>
#include <vector>
#include <chrono>
#include <optional>
#include <iostream>
#include <algorithm>

#include "settings.h"
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
#include "HmlQueries.h"
#include "HmlContext.h"

#include "../libs/stb_image.h"


struct Himmel {
    // TODO move out
    struct World {
        glm::vec2 start;
        glm::vec2 finish;
        float height;

        using Coord = std::pair<size_t, size_t>;

        Coord heightmapSize;
        // As per stbi: y rows, x pixels each
        std::vector<std::vector<float>> heightmap;

        inline World(const glm::vec2& start, const glm::vec2& finish, float height, const char* heightmapFilepath)
                : start(start), finish(finish), height(height) {
            int mapWidth, mapHeight, channels;
            stbi_uc* pixels = stbi_load(heightmapFilepath, &mapWidth, &mapHeight, &channels, STBI_grey);
            if (!pixels) {
                std::cerr << "::> Failed to load texture image using stb library while creating World: " << heightmapFilepath << ".\n";
                return;
            }

            heightmapSize = { mapWidth, mapHeight };

            size_t i = 0;
            heightmap.reserve(mapHeight);
            for (size_t y = 0; y < static_cast<size_t>(mapHeight); y++) {
                std::vector<float> row(mapWidth);
                for (size_t x = 0; x < static_cast<size_t>(mapWidth); x++) {
                    row[x] = static_cast<float>(pixels[i++]) / 255.0f * height;
                }
                heightmap.push_back(std::move(row));
            }

            stbi_image_free(pixels);
        }


        inline float heightAt(glm::vec2 pos) const noexcept {
            pos = glm::clamp(pos, start, finish);
            const auto mapSize = finish - start;
            const auto mapPos = glm::vec2(heightmapSize.first, heightmapSize.second) * (pos - start) / mapSize;

            const auto mapPosFloor = glm::vec2(std::floor(mapPos.x), std::floor(mapPos.y));
            const Coord mapCoordFloor{ static_cast<size_t>(mapPosFloor.x), static_cast<size_t>(mapPosFloor.y) };
            const auto t = mapPos - mapPosFloor;
            const auto res = heightAtCoordWithT(mapCoordFloor, t);
            return res;
        }

        private:
        // Coord is [0..size-2]
        // t is [0..1)
        inline float heightAtCoordWithT(Coord coord, glm::vec2 t) const noexcept {
            // The renderer treats squares as two triangles with such diagonal orientation:
            // [0,1]--[1,1]
            //   |  \  |
            //   |   \ |
            // [0,0]--[1,0]
            // We account for that while interpolation between the coordinates to
            // create this edge.

            const size_t x = std::clamp(coord.first,  0ull, heightmapSize.first  - 2);
            const size_t y = std::clamp(coord.second, 0ull, heightmapSize.second - 2);
            t = glm::clamp(t, {0.0f, 0.0f}, {1.0f, 1.0f});

            const auto bottomLeftHeight  = heightmap[y    ][x];
            const auto bottomRightHeight = heightmap[y    ][x + 1];
            const auto topLeftHeight     = heightmap[y + 1][x    ];
            const auto topRightHeight    = heightmap[y + 1][x + 1];
            if (t.x + t.y < 1.0f) { // bottom-left triangle
                const auto diffX = bottomRightHeight - bottomLeftHeight;
                const auto diffY = topLeftHeight - bottomLeftHeight;
                return bottomLeftHeight + t.x * diffX + t.y * diffY;
            } else { // top-right triangle
                const auto diffX = topRightHeight - topLeftHeight;
                const auto diffY = topRightHeight - bottomRightHeight;
                return topRightHeight - (1.0 - t.x) * diffX - (1.0 - t.y) * diffY;
            }
        }
    };


    // TODO move out
    struct Car {
        // float width;
        // float length;
        // glm::vec2 size;

        Car(const glm::vec3& posCenter, float sizeScaler) : posCenter(posCenter), sizeScaler(sizeScaler) {}

        bool cachedViewValid = false;
        glm::mat4 cachedView;

        glm::vec3 posCenter;
        glm::vec3 dirForward;
        glm::vec3 dirRight;
        float sizeScaler;

        // float deviationAngle = 0.0f;
        //
        // void setLeft() {
        //     deviationAngle = -45.0f;
        // }
        // void setRight() {
        //     deviationAngle = +45.0f;
        // }
        // void setStraight() {
        //     deviationAngle = 0.0f;
        // }

        void moveForward(float dist) {
            // XXX Let -Z be forward
            cachedViewValid = false;
            posCenter.z -= dist;
        }
        void moveRight(float dist) {
            cachedViewValid = false;
            posCenter.x += dist;
        }
        void setHeight(float height) {
            cachedViewValid = false;
            posCenter.y = height;
        }

        const glm::mat4& getView() noexcept {
            if (!cachedViewValid) [[unlikely]] {
                cachedViewValid = true;

                cachedView = glm::mat4(1.0f);
                cachedView = glm::translate(cachedView, posCenter);
                cachedView = glm::scale(cachedView, glm::vec3{sizeScaler, sizeScaler, sizeScaler});
                // NOTE fix model 90-degree rotation
                cachedView = glm::rotate(cachedView, glm::radians(-90.0f), glm::vec3{0, 1, 0});
                // NOTE fix move up
                cachedView = glm::translate(cachedView, glm::vec3{0.0f, 1.0f, 0.0f});
            }

            return cachedView;
        }

        // void place(float yLeftBack, float yRightBack, float yLeftFront, float yRightFront) {
        //     glm::vec3 leftBack(posCenter.x - 0.5f * size.x, yLeftBack, posCenter.z - 0.5f * size.y);
        //     glm::vec3 leftFront(posCenter.x - 0.5f * size.x, yLeftFront, posCenter.z + 0.5f * size.y);
        //     glm::vec3 rightBack(posCenter.x + 0.5f * size.x, yRightBack, posCenter.z - 0.5f * size.y);
        //     dirForward = glm::normalize(glm::vec3(size.x, size.z));
        // }
    };


    struct GeneralUbo {
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
        alignas(16) glm::mat4 globalLightView;
        alignas(16) glm::mat4 globalLightProj;
        glm::vec3 globalLightDir;
        float ambientStrength;
        glm::vec3 fogColor;
        float fogDensity;
        glm::vec3 cameraPos;
    };

    std::vector<HmlLightRenderer::PointLight> pointLightsStatic;
    std::vector<HmlLightRenderer::PointLight> pointLightsDynamic;

    static constexpr size_t MAX_POINT_LIGHTS = 64; // XXX sync with all shaders
    struct LightUbo {
        std::array<HmlLightRenderer::PointLight, MAX_POINT_LIGHTS> pointLights;
        uint32_t count;
    };

    struct Weather {
        glm::vec3 fogColor;
        float fogDensity;
    };
    Weather weather;
    const glm::vec4 FOG_COLOR = glm::vec4(0.7, 0.7, 0.7, 1.0);

    std::shared_ptr<HmlContext> hmlContext;

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
    std::vector<std::shared_ptr<HmlImageResource>> gBufferLightSpacePositions;
    std::vector<std::shared_ptr<HmlImageResource>> brightness1Textures;
    // std::vector<std::shared_ptr<HmlImageResource>> brightness2Textures;
    std::vector<std::shared_ptr<HmlImageResource>> mainTextures;
    std::shared_ptr<HmlImageResource> hmlDepthResource;
    std::shared_ptr<HmlImageResource> hmlShadow;


    std::unique_ptr<World> world;
    std::unique_ptr<Car> car;


    HmlCamera hmlCamera;
    glm::mat4 proj;
    std::pair<int32_t, int32_t> cursor;


    // Sync objects
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
    // NOTE Since it's really a bad performance thing to have an array of pointers,
    // here we try to see if a basic approach works architecturally-wise.
    std::vector<HmlRenderer::Entity> staticEntities;

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


#endif
