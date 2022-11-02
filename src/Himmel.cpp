#include "Himmel.h"


#define SNOW_IS_ON    0
#define DEBUG_TERRAIN 0


bool Himmel::init() noexcept {
    const char* windowName = "Planes game";

    hmlContext = std::make_shared<HmlContext>();
    hmlContext->maxFramesInFlight = 2;

    hmlContext->hmlWindow = HmlWindow::create(1080, 720, windowName);
    if (!hmlContext->hmlWindow) return false;

    hmlContext->hmlDevice = HmlDevice::create(hmlContext->hmlWindow);
    if (!hmlContext->hmlDevice) return false;

    hmlContext->hmlDescriptors = HmlDescriptors::create(hmlContext->hmlDevice);
    if (!hmlContext->hmlDescriptors) return false;

    hmlContext->hmlCommands = HmlCommands::create(hmlContext->hmlDevice);
    if (!hmlContext->hmlCommands) return false;

    hmlContext->hmlResourceManager = HmlResourceManager::create(hmlContext->hmlDevice, hmlContext->hmlCommands);
    if (!hmlContext->hmlResourceManager) return false;

    hmlContext->hmlSwapchain = HmlSwapchain::create(hmlContext->hmlWindow, hmlContext->hmlDevice, hmlContext->hmlResourceManager, std::nullopt);
    if (!hmlContext->hmlSwapchain) return false;

    hmlContext->hmlQueries = HmlQueries::create(hmlContext->hmlDevice, hmlContext->hmlCommands, hmlContext->imageCount());
    if (!hmlContext->hmlQueries) return false;

    hmlContext->hmlImgui = HmlImgui::create(hmlContext->hmlWindow, hmlContext->hmlResourceManager);
    if (!hmlContext->hmlImgui) return false;


    for (size_t i = 0; i < hmlContext->imageCount(); i++) {
        const auto size = sizeof(GeneralUbo);
        auto ubo = hmlContext->hmlResourceManager->createUniformBuffer(size);
        ubo->map();
        viewProjUniformBuffers.push_back(std::move(ubo));
    }
    for (size_t i = 0; i < hmlContext->imageCount(); i++) {
        const auto size = sizeof(LightUbo);
        auto ubo = hmlContext->hmlResourceManager->createUniformBuffer(size);
        ubo->map();
        lightUniformBuffers.push_back(std::move(ubo));
    }


    generalDescriptorPool = hmlContext->hmlDescriptors->buildDescriptorPool()
        .withUniformBuffers(hmlContext->imageCount() + hmlContext->imageCount()) // GeneralUbo + LightUbo
        .maxDescriptorSets(hmlContext->imageCount()) // they are in the same set
        .build(hmlContext->hmlDevice);
    if (!generalDescriptorPool) return false;

    generalDescriptorSetLayout = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
        .withUniformBufferAt(0,
            VK_SHADER_STAGE_VERTEX_BIT |
            VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT |
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
            VK_SHADER_STAGE_GEOMETRY_BIT |
            VK_SHADER_STAGE_FRAGMENT_BIT
        )
        .withUniformBufferAt(1,
            VK_SHADER_STAGE_VERTEX_BIT |
            VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
            VK_SHADER_STAGE_FRAGMENT_BIT
        )
        .build(hmlContext->hmlDevice);
    if (!generalDescriptorSetLayout) return false;

    generalDescriptorSet_0_perImage = hmlContext->hmlDescriptors->createDescriptorSets(
        hmlContext->imageCount(), generalDescriptorSetLayout, generalDescriptorPool);
    if (generalDescriptorSet_0_perImage.empty()) return false;
    for (size_t imageIndex = 0; imageIndex < hmlContext->imageCount(); imageIndex++) {
        HmlDescriptorSetUpdater(generalDescriptorSet_0_perImage[imageIndex])
            .uniformBufferAt(0,
                viewProjUniformBuffers[imageIndex]->buffer,
                sizeof(GeneralUbo))
            .uniformBufferAt(1,
                lightUniformBuffers[imageIndex]->buffer,
                sizeof(LightUbo))
            .update(hmlContext->hmlDevice);
    }


    weather = Weather{
#if 1
        // Night (no fog):
        .fogColor = glm::vec3(0.0, 0.0, 0.0),
        .fogDensity = -1.0f,
#else
        // Day:
        .fogColor = glm::vec3(0.8f, 0.8f, 0.8f),
        .fogDensity = 0.011,
#endif
    };


    hmlRenderer = HmlRenderer::create(hmlContext, generalDescriptorSetLayout);
    if (!hmlRenderer) return false;

    hmlDeferredRenderer = HmlDeferredRenderer::create(hmlContext, generalDescriptorSetLayout);
    if (!hmlDeferredRenderer) return false;

    hmlUiRenderer = HmlUiRenderer::create(hmlContext);
    if (!hmlUiRenderer) return false;

    // hmlBlurRenderer = HmlBlurRenderer::create(hmlContext);
    // if (!hmlBlurRenderer) return false;

    hmlBloomRenderer = HmlBloomRenderer::create(hmlContext);
    if (!hmlBloomRenderer) return false;

    // NOTE specify buffers explicitly, even though we have context already, to highlight this dependency
    hmlImguiRenderer = HmlImguiRenderer::create(hmlContext->hmlImgui->vertexBuffer, hmlContext->hmlImgui->indexBuffer, hmlContext);
    if (!hmlImguiRenderer) return false;


    const char* heightmapFile = "../models/heightmap.png";
    {
        const auto worldHalfSize = 250.0f;
        const auto worldStart  = glm::vec2(-worldHalfSize, -worldHalfSize);
        const auto worldFinish = glm::vec2( worldHalfSize,  worldHalfSize);
        const auto worldHeight = 70.0f;
        world = std::make_unique<World>(worldStart, worldFinish, worldHeight, heightmapFile);
    }


    {
        const float carX = 0.0f;
        const float carZ = 0.0f;
        const auto carPos = glm::vec3{carX, world->heightAt({ carX, carZ }), carZ};
        const auto carSizeScaler = 0.5f;
        car = std::make_unique<Car>(carPos, carSizeScaler);
    }


#if SNOW_IS_ON
    const auto snowCount = 400000;
    const auto sizeSnow = world->height;
    const HmlSnowParticleRenderer::SnowBounds snowBounds = HmlSnowParticleRenderer::SnowCameraBounds{ sizeSnow };
    hmlSnowRenderer = HmlSnowParticleRenderer::createSnowRenderer(snowCount, snowBounds, hmlContext, generalDescriptorSetLayout);
    if (!hmlSnowRenderer) return false;
#endif


    const auto terrainBounds = HmlTerrainRenderer::Bounds{
        .posStart = world->start,
        .posFinish = world->finish,
        .height = world->height,
        .yOffset = 0.0f,
    };
    const uint32_t granularity = 1;
    hmlTerrainRenderer = HmlTerrainRenderer::create(heightmapFile, granularity, "../models/grass-small.png",
        terrainBounds, hmlContext, generalDescriptorSetLayout);
    if (!hmlTerrainRenderer) return false;


    // Add lights
    const float LIGHT_RADIUS = 2.0f;
    const size_t lightsCount = 20;
    for (size_t i = 0; i < lightsCount; i++) {
        const auto pos = glm::vec3(
            hml::getRandomUniformFloat(world->start.x, world->finish.x),
            hml::getRandomUniformFloat(world->height/2.0f, world->height),
            hml::getRandomUniformFloat(world->start.y, world->finish.y)
        );
        const auto color = glm::vec3(
            hml::getRandomUniformFloat(0.0f, 1.0f),
            hml::getRandomUniformFloat(0.0f, 1.0f),
            hml::getRandomUniformFloat(0.0f, 1.0f)
        );
        pointLightsStatic.push_back(HmlLightRenderer::PointLight{
            .color = color,
            .intensity = 5000.0f,
            .position = pos,
            .radius = LIGHT_RADIUS,
        });
    }
    { // The light for 2 flat planes
        const auto pos = glm::vec3(0.0, 60.0, 20.0);
        const auto color = glm::vec3(1.0, 1.0, 1.0);
        pointLightsStatic.push_back(HmlLightRenderer::PointLight{
            .color = color,
            .intensity = 100.0f,
            .position = pos,
            .radius = LIGHT_RADIUS,
        });
    }
    {
        pointLightsDynamic.push_back(HmlLightRenderer::PointLight{
            .color = glm::vec3(1.0, 0.0, 0.0),
            .intensity = 5000.0f,
            .position = {},
            .radius = LIGHT_RADIUS,
        });
        pointLightsDynamic.push_back(HmlLightRenderer::PointLight{
            .color = glm::vec3(0.0, 1.0, 0.0),
            .intensity = 3000.0f,
            .position = {},
            .radius = LIGHT_RADIUS,
        });
        pointLightsDynamic.push_back(HmlLightRenderer::PointLight{
            .color = glm::vec3(0.0, 0.0, 1.0),
            .intensity = 3800.0f,
            .position = {},
            .radius = LIGHT_RADIUS,
        });
    }

    hmlLightRenderer = HmlLightRenderer::create(hmlContext, generalDescriptorSetLayout, generalDescriptorSet_0_perImage);
    if (!hmlLightRenderer) return false;
    hmlLightRenderer->specify(pointLightsStatic.size() + pointLightsDynamic.size());


    hmlCamera = HmlCamera{{ 205.0f, 135.0f, 215.0f }};
    hmlCamera.rotateDir(-28.0f, 307.0f);
    cursor = hmlContext->hmlWindow->getCursor();
    proj = projFrom(hmlContext->hmlSwapchain->extentAspect());


    {
        { // 2D plane of a girl picture
            std::vector<HmlSimpleModel::Vertex> vertices;
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {-0.5f, -0.5f, 0.0f},
                .texCoord = {1.0f, 0.0f},
                .normal = {0.0f, 0.0f, 1.0f},
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {0.5f, -0.5f, 0.0f},
                .texCoord = {0.0f, 0.0f},
                .normal = {0.0f, 0.0f, 1.0f},
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {0.5f, 0.5f, 0.0f},
                .texCoord = {0.0f, 1.0f},
                .normal = {0.0f, 0.0f, 1.0f},
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {-0.5f, 0.5f, 0.0f},
                .texCoord = {1.0f, 1.0f},
                .normal = {0.0f, 0.0f, 1.0f},
            });

            std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

            const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
            const auto model = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices, "../models/girl.png", VK_FILTER_LINEAR);
            models.push_back(model);
        }

        { // Just a 2D plane
            std::vector<HmlSimpleModel::Vertex> vertices;
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {-0.5f, -0.5f, 0.0f},
                .texCoord = {1.0f, 0.0f},
                .normal = {0.0f, 0.0f, 1.0f},
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {0.5f, -0.5f, 0.0f},
                .texCoord = {0.0f, 0.0f},
                .normal = {0.0f, 0.0f, 1.0f},
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {0.5f, 0.5f, 0.0f},
                .texCoord = {0.0f, 1.0f},
                .normal = {0.0f, 0.0f, 1.0f},
            });
            vertices.push_back(HmlSimpleModel::Vertex{
                .pos = {-0.5f, 0.5f, 0.0f},
                .texCoord = {1.0f, 1.0f},
                .normal = {0.0f, 0.0f, 1.0f},
            });

            std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };

            const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
            const auto model = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
            models.push_back(model);
        }

        { // A viking kitchen box
            std::vector<HmlSimpleModel::Vertex> vertices;
            std::vector<uint32_t> indices;
            if (!HmlSimpleModel::load("../models/viking_room.obj", vertices, indices)) return false;

            const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
            const auto model = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices, "../models/viking_room.png", VK_FILTER_LINEAR);
            // const auto model = hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
            models.push_back(model);
        }

        { // Plane
            std::vector<HmlSimpleModel::Vertex> vertices;
            std::vector<uint32_t> indices;
            if (!HmlSimpleModel::load("../models/plane.obj", vertices, indices)) return false;

            const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
            const auto model = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
            models.push_back(model);
        }

        { // Car
            std::vector<HmlSimpleModel::Vertex> vertices;
            std::vector<uint32_t> indices;
            if (!HmlSimpleModel::load("../models/my_car.obj", vertices, indices)) return false;

            const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
            const auto model = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
            models.push_back(model);
        }

        { // Tree#1
            std::vector<HmlSimpleModel::Vertex> vertices;
            std::vector<uint32_t> indices;
            if (!HmlSimpleModel::load("../models/tree/basic_tree.obj", vertices, indices)) return false;

            const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
            const auto model = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices, "../models/tree/basic_tree.png", VK_FILTER_LINEAR);
            models.push_back(model);
        }
        { // Tree#2
            std::vector<HmlSimpleModel::Vertex> vertices;
            std::vector<uint32_t> indices;
            if (!HmlSimpleModel::load("../models/tree/basic_tree_2.obj", vertices, indices)) return false;

            const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
            const auto model = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices, "../models/tree/basic_tree_2.png", VK_FILTER_LINEAR);
            models.push_back(model);
        }

        const auto& phonyModel = models[0];
        const auto& flatModel = models[1];
        const auto& vikingModel = models[2];
        const auto& planeModel = models[3];
        const auto& carModel = models[4];
        const auto& treeModel = models[5];
        const auto& treeModel2 = models[6];


        // Add Entities

        { // Arbitrary Entities
            // PHONY with a texture
            {
                entities.push_back(std::make_shared<HmlRenderer::Entity>(phonyModel));
                auto modelMatrix = glm::mat4(1.0f);
                modelMatrix = glm::translate(modelMatrix, { 0.0f, 50.0f, -200.0f });
                modelMatrix = glm::scale(modelMatrix, glm::vec3(150.0f));
                modelMatrix = glm::rotate(modelMatrix, glm::radians(180.0f), glm::vec3(0.0, 0.0, 1.0));
                entities.back()->modelMatrix = modelMatrix;
            }
            { // back side
                entities.push_back(std::make_shared<HmlRenderer::Entity>(phonyModel));
                auto modelMatrix = glm::mat4(1.0f);
                modelMatrix = glm::translate(modelMatrix, { 0.0f, 50.0f, -200.0f - 0.001f });
                modelMatrix = glm::scale(modelMatrix, glm::vec3(150.0f));
                modelMatrix = glm::rotate(modelMatrix, glm::radians(180.0f), glm::vec3(0.0, 0.0, 1.0));
                modelMatrix = glm::rotate(modelMatrix, glm::radians(180.0f), glm::vec3(0.0, 1.0, 0.0));
                entities.back()->modelMatrix = modelMatrix;
            }

            entities.push_back(std::make_shared<HmlRenderer::Entity>(planeModel, glm::vec3{ 1.0f, 1.0f, 1.0f }));
            entities.back()->modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3{0.0f, 35.0f, 30.0f});

            entities.push_back(std::make_shared<HmlRenderer::Entity>(planeModel, glm::vec3{ 0.8f, 0.2f, 0.5f }));
            entities.back()->modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3{0.0f, 45.0f, 60.0f});

            entities.push_back(std::make_shared<HmlRenderer::Entity>(planeModel, glm::vec3{ 0.2f, 0.2f, 0.9f }));
            entities.back()->modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3{0.0f, 45.0f, 90.0f});

            // Flat surface with a tree
            { // #1
                entities.push_back(std::make_shared<HmlRenderer::Entity>(flatModel, glm::vec3{ 1.0f, 1.0f, 1.0f }));
                auto modelMatrix = glm::mat4(1.0f);
                modelMatrix = glm::translate(modelMatrix, { 0.0f, 40.0f, 0.0f });
                modelMatrix = glm::scale(modelMatrix, glm::vec3(40.0f));
                modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f + 180.0f), glm::vec3(1, 0, 0));
                entities.back()->modelMatrix = modelMatrix;
            }
            { // #1 back side
                entities.push_back(std::make_shared<HmlRenderer::Entity>(flatModel, glm::vec3{ 1.0f, 1.0f, 1.0f }));
                auto modelMatrix = glm::mat4(1.0f);
                modelMatrix = glm::translate(modelMatrix, { 0.0f, 40.0f - 0.001f, 0.0f });
                modelMatrix = glm::scale(modelMatrix, glm::vec3(40.0f));
                modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(1, 0, 0));
                entities.back()->modelMatrix = modelMatrix;
            }
            { // $2
                entities.push_back(std::make_shared<HmlRenderer::Entity>(flatModel, glm::vec3{ 1.0f, 1.0f, 1.0f }));
                auto modelMatrix = glm::mat4(1.0f);
                modelMatrix = glm::translate(modelMatrix, { 0.0f, 60.0f, -20.0f });
                modelMatrix = glm::scale(modelMatrix, glm::vec3(40.0f));
                // modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(0, 0, 1));
                entities.back()->modelMatrix = modelMatrix;
            }
            { // $2 back side
                entities.push_back(std::make_shared<HmlRenderer::Entity>(flatModel, glm::vec3{ 1.0f, 1.0f, 1.0f }));
                auto modelMatrix = glm::mat4(1.0f);
                modelMatrix = glm::translate(modelMatrix, { 0.0f, 60.0f, -20.0f - 0.001f });
                modelMatrix = glm::scale(modelMatrix, glm::vec3(40.0f));
                modelMatrix = glm::rotate(modelMatrix, glm::radians(180.0f), glm::vec3(1, 0, 0));
                entities.back()->modelMatrix = modelMatrix;
            }
            {
                entities.push_back(std::make_shared<HmlRenderer::Entity>(treeModel2));
                const auto pos = glm::vec3{ 0, 40, 0 };
                const float scale = 7.0f;
                auto modelMatrix = glm::mat4(1.0f);
                modelMatrix = glm::translate(modelMatrix, pos);
                modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
                entities.back()->modelMatrix = std::move(modelMatrix);
            }
            entities.push_back(std::make_shared<HmlRenderer::Entity>(vikingModel));
            entities.back()->modelMatrix = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(10.0, 10.0, 10.0)), glm::vec3{40.0f, 30.0f, 70.0f});

#if 0
            { // Test comparing to static Entities
                // NOTE We don't need the Color component of Entity
                const size_t amount = 500;
                for (size_t i = 0; i < amount; i++) {
                    // TODO utilize world height access
                    const auto pos = glm::vec3{
                        hml::getRandomUniformFloat(world->start.x, world->finish.x),
                        60.0f,
                        hml::getRandomUniformFloat(world->start.y, world->finish.y),
                    };
                    const float scale = 10.0f;
                    auto modelMatrix = glm::mat4(1.0f);
                    modelMatrix = glm::translate(modelMatrix, pos);
                    modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
                    modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));

                    entities.push_back(std::make_shared<HmlRenderer::Entity>(treeModel));
                    entities.back()->modelMatrix = modelMatrix;
                }
            }
#endif

            // XXX We rely on it being last in update() XXX
            entities.push_back(std::make_shared<HmlRenderer::Entity>(carModel, glm::vec3{ 0.2f, 0.7f, 0.4f }));
            entities.back()->modelMatrix = car->getView();


            hmlRenderer->specifyEntitiesToRender(entities);
        }

        { // Static Entities
            // NOTE We don't need the Color component of Entity
            const size_t amount = 1000;
            staticEntities.reserve(amount);
            for (size_t i = 0; i < amount; i++) {
                const auto x = hml::getRandomUniformFloat(world->start.x, world->finish.x);
                const auto z = hml::getRandomUniformFloat(world->start.y, world->finish.y);
                const auto y = world->heightAt({ x, z });
                const auto pos = glm::vec3{ x, y, z };
                const float scale = 7.0f;
                auto modelMatrix = glm::mat4(1.0f);
                modelMatrix = glm::translate(modelMatrix, pos);
                modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
                // modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(1.0, 0.0, 0.0));

                staticEntities.emplace_back((i % 2) ? treeModel : treeModel2, modelMatrix);
            }

            hmlRenderer->specifyStaticEntitiesToRender(staticEntities);
        }
    }

    if (!createSyncObjects()) return false;
    if (!prepareResources()) return false;


    successfulInit = true;
    return true;
}


bool Himmel::run() noexcept {
    static auto startTime = std::chrono::high_resolution_clock::now();
    while (!hmlContext->hmlWindow->shouldClose()) {
        hmlContext->hmlQueries->beginFrame();
        // To force initial update of stuff
        // if (hmlContext->currentFrame) hmlCamera.invalidateCache();

        glfwPollEvents();
        hmlContext->hmlImgui->beginFrame();

        static auto mark = startTime;
        const auto newMark = std::chrono::high_resolution_clock::now();
        const auto sinceLast = std::chrono::duration_cast<std::chrono::microseconds>(newMark - mark).count();
        const auto sinceStart = std::chrono::duration_cast<std::chrono::microseconds>(newMark - startTime).count();
        const float deltaSeconds = static_cast<float>(sinceLast) / 1'000'000.0f;
        const float sinceStartSeconds = static_cast<float>(sinceStart) / 1'000'000.0f;
        mark = newMark;
        updateForDt(deltaSeconds, sinceStartSeconds);
        // const auto mark1 = std::chrono::high_resolution_clock::now();
        // const auto updateMs = static_cast<float>(std::chrono::duration_cast<std::chrono::microseconds>(mark1 - newMark).count()) / 1'000.0f;

#if WITH_IMGUI && USE_TIMESTAMP_QUERIES
        static uint32_t timeMks = 0;
        {
            static constexpr uint32_t showEvery = 10;
            static uint32_t showCounter = 0;
            static uint32_t showedTime = 0;
            ImGui::SetNextWindowBgAlpha(0.5f);
            ImGuiWindowFlags window_flags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav;
            if (ImGui::Begin("Perf", nullptr, window_flags)) {
                if (showCounter == 0) {
                    showedTime = timeMks;
                }
                ImGui::Text("GPU time = %.1fms", showedTime / 1000.0f);
            }
            ImGui::End();

            showCounter++;
            showCounter %= showEvery;
        }
#endif // WITH_IMGUI

        if (!drawFrame()) return false;

        hmlContext->hmlResourceManager->tickFrame(hmlContext->currentFrame);
        hmlContext->hmlQueries->endFrame(hmlContext->currentFrame);

#if LOG_FPS
        const auto fps = 1.0 / deltaSeconds;
        std::cout << "Delta = " << deltaSeconds * 1000.0f << "ms [FPS = " << fps << "]"
            // << " Update = " << updateMs << "ms"
            << '\n';
#endif

#if USE_TIMESTAMP_QUERIES
        const auto frameStatOpt = hmlContext->hmlQueries->popOldestFrameStat();
#endif // USE_TIMESTAMP_QUERIES

#if LOG_STATS
        if (frameStatOpt) {
            static uint64_t lastFrameStart = 0;

            // for (const auto& eventName : *(frameStatOpt->layout)) std::cout << eventName << "===";
            // std::cout << '\n';

            uint64_t last = 0;
            auto it = frameStatOpt->layout->cbegin();
            for (const auto& dataOpt : frameStatOpt->data) {
                if (dataOpt) {
                    if (!last) {
                        last = *dataOpt; // to make the first one display as zero
                        std::cout << "Since d=" << (*dataOpt - lastFrameStart)/1000 << "mks     ";
                        lastFrameStart = *dataOpt;
                    }
                    const auto value = ((*dataOpt) - last) / 1000;
                    std::cout << (it++)->shortName << "=" << value << "mks";
                    last = *dataOpt;
                } else std::cout << "---";

                std::cout << "  ";
            }
            std::cout << '\n';
        }
#endif // LOG_STATS

#if WITH_IMGUI && USE_TIMESTAMP_QUERIES
        if (frameStatOpt) {
            const auto& firstOpt = frameStatOpt->data.front();
            const auto& lastOpt = frameStatOpt->data.back();
            if (frameStatOpt->data.size() >= 2 && firstOpt && lastOpt) {
                timeMks = (*lastOpt - *firstOpt) / 1000;
            }
        }
#endif // WITH_IMGUI

        hmlContext->currentFrame++;
    }
    vkDeviceWaitIdle(hmlContext->hmlDevice->device);
    return true;
}


void Himmel::updateForDt(float dt, float sinceStart) noexcept {
    const float unit = 40.0f;
    for (size_t i = 0; i < pointLightsDynamic.size(); i++) {
        glm::mat4 model(1.0);
        switch (i) {
            case 0: model = glm::translate(model, glm::vec3(0.0, unit, 2*unit)); break;
            case 1: model = glm::translate(model, glm::vec3(3*unit, unit, 0.0)); break;
            case 2: model = glm::translate(model, glm::vec3(-2*unit, unit, -3*unit)); break;
            default: model = glm::translate(model, glm::vec3(i * unit, unit, -i*unit));
        }
        model = glm::rotate(model, (i+1) * 0.7f * sinceStart + i * 1.0f, glm::vec3(0.0, 1.0, 0.0));
        model = glm::translate(model, glm::vec3(50.0f, 0.0f, 0.0f)); // inner radius
        pointLightsDynamic[i].position = model[3];
    }


    static bool uiInFocus = false;
#ifdef WITH_IMGUI
    { // Imgui stuff
        static bool f1Pressed = false;
        if (!f1Pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_F1) == GLFW_PRESS) {
            f1Pressed = true;
            uiInFocus = !uiInFocus;
            if (uiInFocus) {
                glfwSetInputMode(hmlContext->hmlWindow->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetInputMode(hmlContext->hmlWindow->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                // Unfocus Imgui windows to not accidentally capture game input (e.g. spacebar to minimize window)
                ImGui::SetWindowFocus(nullptr);
            }
        } else if (f1Pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_F1) == GLFW_RELEASE) {
            f1Pressed = false;
        }
    }
#endif

    // NOTE allow cursor position update and only disable camera rotation reaction
    // in order not to have the camera jump on re-acquiring control from the ui
    // due to the cursor (pointer) position change that will have happened.
    { // Camera rotation from mouse
        const auto newCursor = hmlContext->hmlWindow->getCursor();
        const int32_t dx = newCursor.first - cursor.first;
        const int32_t dy = newCursor.second - cursor.second;
        cursor = newCursor;
        const float rotateSpeed = 0.05f;
        if (!uiInFocus) {
            if (dx || dy) hmlCamera.rotateDir(-dy * rotateSpeed, dx * rotateSpeed);
        }
    }

    if (!uiInFocus) { // Camera movement from keyboard
        constexpr float movementSpeed = 16.0f;
        constexpr float boostUp = 10.0f;
        constexpr float boostDown = 0.2f;
        float length = movementSpeed * dt;
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            length *= boostUp;
        } else if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
            length *= boostDown;
        }

        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_W) == GLFW_PRESS) {
            hmlCamera.forward(length);
        }
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_S) == GLFW_PRESS) {
            hmlCamera.forward(-length);
        }
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_D) == GLFW_PRESS) {
            hmlCamera.right(length);
        }
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_A) == GLFW_PRESS) {
            hmlCamera.right(-length);
        }
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            hmlCamera.lift(length);
        }
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_C) == GLFW_PRESS) {
            hmlCamera.lift(-length);
        }
    }

    if (!uiInFocus) { // Camera print position
        static bool pPressed = false;
        if (!pPressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_P) == GLFW_PRESS) {
            pPressed = true;
            hmlCamera.printStats();
        } else if (pPressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_P) == GLFW_RELEASE) {
            pPressed = false;
        }
    }

    if (!uiInFocus) { // Car movement
        constexpr float carSpeed = 10.0f;
        constexpr float boostUp = 10.0f;
        constexpr float boostDown = 0.2f;
        float carDistance = carSpeed * dt;
        if      (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) { carDistance *= 0.4f * boostUp; }
        else if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_LEFT_ALT)   == GLFW_PRESS) { carDistance *= boostDown; }
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_RIGHT) == GLFW_PRESS) { car->moveRight(carDistance); }
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_LEFT)  == GLFW_PRESS) { car->moveRight(-carDistance); }
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_UP)    == GLFW_PRESS) { car->moveForward(carDistance); }
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_DOWN)  == GLFW_PRESS) { car->moveForward(-carDistance); }
    }

    if (!car->cachedViewValid) {
        const auto carPos = glm::vec2{car->posCenter.x, car->posCenter.z};
        const auto worldHeight = world->heightAt(carPos);
        car->setHeight(worldHeight);

        // XXX We rely on the Car to be the last Entity here XXX
        entities.back()->modelMatrix = car->getView();
        // NOTE no need to call hmlRenderer->specifyEntitiesToRender(entities)
        // because the Entities are stored as shared_ptr there, which we have
        // just updated.
    }


#ifdef WITH_IMGUI
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav;
    if (ImGui::Begin("Stats", nullptr, window_flags)) {
        {
            if (uiInFocus) ImGui::Text("Focus: Imgui");
            else ImGui::Text("Focus: Himmel");
        }
        ImGui::Separator();
        { // Camera
            auto& pos = hmlCamera.pos;
            auto& pitch = hmlCamera.pitch;
            auto& yaw = hmlCamera.yaw;

            const float minPos = -1000.0f;
            const float maxPos = 1000.0f;
            const float minYaw = 0.0f;
            const float maxYaw = 360.0f;
            const float minPitch = -90.0f;
            const float maxPitch = 90.0f;

            ImGui::Text("Camera");
            ImGui::Spacing();
            ImGui::Text("[%.2f;%.2f;%.2f] p=%.1f y=%.1f", pos.x, pos.y, pos.z, pitch, yaw);
            ImGui::DragFloat3("pos", &pos[0], 0.8f, minPos, maxPos);
            ImGui::DragScalar("pitch", ImGuiDataType_Float, &pitch, 0.3f, &minPitch, &maxPitch, "%f");
            ImGui::DragScalar("yaw", ImGuiDataType_Float, &yaw, 0.7f, &minYaw, &maxYaw, "%f");

            static auto cachedPos = pos;
            static auto cachedPitch = pitch;
            static auto cachedYaw = yaw;
            if (pos != cachedPos || pitch != cachedPitch || yaw != cachedYaw) {
                hmlCamera.invalidateCache();
                cachedPos = pos;
                cachedPitch = pitch;
                cachedYaw = yaw;
            }
        }
        ImGui::Separator();
        { // Car
            auto& pos = car->posCenter;

            const float minPos = -1000.0f;
            const float maxPos = 1000.0f;

            ImGui::Text("Car");
            ImGui::Spacing();
            ImGui::Text("[%.2f;%.2f;%.2f]", pos.x, pos.y, pos.z);
            ImGui::DragFloat3("position", &pos[0], 0.3f, minPos, maxPos);
            // ImGui::DragFloat3("dirForward", &car->dirForward[0]);
            // ImGui::DragFloat3("dirRight", &car->dirRight[0]);

            static auto cachedPos = car->posCenter;
            if (pos != cachedPos) {
                car->cachedViewValid = false;
                cachedPos = pos;
            }
        }
    }
    ImGui::End();
#endif


    hmlContext->hmlImgui->updateForDt(dt);


#if SNOW_IS_ON
    hmlSnowRenderer->updateForDt(dt, sinceStart);
#endif
    {
        // NOTE Takes up to 60mks with (current) CPU-patch implementation
        // const auto startTime = std::chrono::high_resolution_clock::now();
        hmlTerrainRenderer->update(hmlCamera.getPos());
        // const auto newMark = std::chrono::high_resolution_clock::now();
        // const auto sinceStart = std::chrono::duration_cast<std::chrono::microseconds>(newMark - startTime).count();
        // const float sinceStartMksSeconds = static_cast<float>(sinceStart);
        // std::cout << "UPD = " << sinceStartMksSeconds << '\n';
    }

}


void Himmel::updateForImage(uint32_t imageIndex) noexcept {
    {
        // NOTE cannot use hmlCamera.somethingHasChanged because we have multiple UBOs (per image)
        const auto globalLightDir = glm::normalize(glm::vec3(0.5f, -1.0f, -1.0f)); // NOTE probably unused
        // const auto DST = 4.0f;
        // const auto globalLightPos = glm::vec3(DST * world->start.x, DST * world->height, DST * world->finish.y);
        // const auto globalLightView = glm::lookAt(globalLightPos, globalLightPos + globalLightDir, glm::vec3{0, 1, 0});
        static float near = 300.0f;
        static float far = 1100.0f;
        static float yaw = 210.0f;
        static float pitch = 30.0f;
        const float minDepth = 0.1f;
        const float maxDepth = 1200.0f;
        const float minYaw = 0.0f;
        const float maxYaw = 360.0f;
        const float minPitch = 0.0f;
        const float maxPitch = 90.0f;
        const float distanceFromCenter = 700.0f;
        // const auto globalLightPos = glm::vec3(-330, 150, 400);
        const auto globalLightPos = HmlCamera::calcDirForward(pitch, yaw) * distanceFromCenter;
        const auto globalLightView = glm::lookAt(globalLightPos, glm::vec3{0,0,0}, glm::vec3{0, 1, 0});
        // const auto globalLightView = glm::lookAt(globalLightPos, globalLightPos + calcDirForward(-18.15f, 42.4f), glm::vec3{0, 1, 0});
        const auto globalLightProj = [&](){
            // glm::mat4 clip = glm::mat4(1.0f, 0.0f, 0.0f, 0.0f,
            //                            0.0f,-1.0f, 0.0f, 0.0f,
            //                            0.0f, 0.0f, 0.5f, 0.0f,
            //                            0.0f, 0.0f, 0.5f, 1.0f);
            // const float near = 0.1f;
            // const float far = 1000.0f;
            const float width = hmlContext->hmlSwapchain->extent.width;
            const float height = hmlContext->hmlSwapchain->extent.height;
            const float f = 0.2f;
            glm::mat4 proj = glm::ortho(-width*f, width*f, -height*f, height*f, near, far);
            // glm::mat4 proj = glm::perspective(glm::radians(45.0f), hmlContext->hmlSwapchain->extentAspect(), near, far);
            proj[1][1] *= -1; // fix the inverted Y axis of GLM
            return proj;
        }();
        GeneralUbo generalUbo{
            .view = hmlCamera.view(),
                .proj = proj,
                // .globalLightView = hmlCamera.view(),
                .globalLightView = globalLightView,
                .globalLightProj = globalLightProj,
                .globalLightDir = globalLightDir,
                .ambientStrength = 0.1f,
                .fogColor = weather.fogColor,
                .fogDensity = weather.fogDensity,
                .cameraPos = hmlCamera.getPos(),
        };
        viewProjUniformBuffers[imageIndex]->update(&generalUbo);
#ifdef WITH_IMGUI
        ImGui::Begin("Shadowmap");
        ImGui::DragScalar("near", ImGuiDataType_Float, &near, 0.1f, &minDepth, &maxDepth, "%f");
        ImGui::DragScalar("far", ImGuiDataType_Float, &far, 1.0f, &minDepth, &maxDepth, "%f");
        ImGui::DragScalar("yaw", ImGuiDataType_Float, &yaw, 0.5f, &minYaw, &maxYaw, "%f");
        ImGui::DragScalar("pitch", ImGuiDataType_Float, &pitch, 0.3f, &minPitch, &maxPitch, "%f");
        ImGui::Text("Sun position [%.2f;%.2f;%.2f]", globalLightPos.x, globalLightPos.y, globalLightPos.z);
        ImGui::End();
#endif
    }

    {
        const auto lightsCount = pointLightsStatic.size() + pointLightsDynamic.size();
        assert(lightsCount <= MAX_POINT_LIGHTS && "::> Reserved std::array size in LightUbo for lights count exceeded.\n");
        LightUbo lightUbo;
        lightUbo.count = lightsCount;
        size_t i = 0;
        for (const auto& light : pointLightsStatic)  lightUbo.pointLights[i++] = light;
        for (const auto& light : pointLightsDynamic) lightUbo.pointLights[i++] = light;
        // Sort to enable proper transparency
        std::sort(lightUbo.pointLights.begin(), lightUbo.pointLights.begin() + lightsCount,
                [cameraPos = hmlCamera.getPos()](const auto& l1, const auto& l2){
                const auto v1 = cameraPos - l1.position;
                const auto v2 = cameraPos - l2.position;
                return glm::dot(v1, v1) > glm::dot(v2, v2);
                });
        lightUniformBuffers[imageIndex]->update(&lightUbo);
    }

#if SNOW_IS_ON
    hmlSnowRenderer->updateForImage(imageIndex);
#endif
}


/*
 * "In flight" -- "in GPU rendering pipeline": either waiting to be rendered,
 * or already being rendered; but not finished rendering yet!
 * So, its commandBuffer has been submitted, but has not finished yet.
 * */
bool Himmel::drawFrame() noexcept {
    static uint32_t currentFrame = 0;

    // Wait for next-in-order frame to become rendered (for its commandBuffer
    // to finish). This ensures that no more than MAX_FRAMES_IN_FLIGHT frames
    // are inside the rendering pipeline at the same time.
    vkWaitForFences(hmlContext->hmlDevice->device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    // vkAcquireNextImageKHR only specifies which image will be made
    // available next, so that we can e.g. start recording command
    // buffers that reference this image. However, the image itself may
    // not be available yet. So we use (preferably) a semaphore in order
    // to tell the commands when the image actually becomes available.
    // Thus we keep the syncronization on the GPU, improving the performance.
    // If we for some reason wanted to actually syncronize with CPU, we'd
    // use a fence.
    uint32_t imageIndex; // the available image we will be given by the presentation engine from the swapchain
    // The next-in-order imageAvailableSemaphore has already retired because
    // its inFlightFence has just been waited upon.
    if (const VkResult result = vkAcquireNextImageKHR(hmlContext->hmlDevice->device, hmlContext->hmlSwapchain->swapchain, UINT64_MAX,
                imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
            result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return true;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "::> Failed to acquire swap chain image.\n";
        return false;
    }

    if (imagesInFlight[imageIndex] != VK_NULL_HANDLE) [[likely]] {
        // For each image we submit, we track which inFlightFence was bound
        // to it; for cases when the to-be-acquired image is still in use by
        // the pipeline (too slow pipeline; out-of-order acquisition;
        // MAX_FRAMES_IN_FLIGHT > swapChainImages.size()) -- we wait until
        // this particular image exits the pipeline.
        vkWaitForFences(hmlContext->hmlDevice->device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    // The image has at least finished being rendered.
    // Mark the image as now being in use by this frame
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    // Once we know what image we work with...

    updateForImage(imageIndex);
// ============================================================================
    const HmlFrameData frameData{
        .frameIndex = currentFrame,
        .imageIndex = imageIndex,
        .generalDescriptorSet_0 = generalDescriptorSet_0_perImage[imageIndex],
    };
    hmlPipe->run(frameData);



// ============================================================================

    {
        VkSemaphore waitSemaphores[] = { renderFinishedSemaphores[currentFrame] };

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = waitSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &(hmlContext->hmlSwapchain->swapchain);
        presentInfo.pImageIndices = &imageIndex;

        // NOTE We recreate the swapchain twice: first we get notified through
        // vkQueuePresentKHR and on the next iteration through framebufferResizeRequested.
        // The second call is probably redundant but non-harmful.
        if (const VkResult result = vkQueuePresentKHR(hmlContext->hmlDevice->presentQueue, &presentInfo);
                result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || hmlContext->hmlWindow->framebufferResizeRequested) {
            hmlContext->hmlWindow->framebufferResizeRequested = false;
            recreateSwapchain();
        } else if (result != VK_SUCCESS) {
            std::cerr << "::> Failed to present swapchain image.\n";
            return false;
        }
    }

    currentFrame = (currentFrame + 1) % hmlContext->maxFramesInFlight;
    return true;
}


bool Himmel::prepareResources() noexcept {
    static const auto viewsFrom = [](const std::vector<std::shared_ptr<HmlImageResource>>& resources){
        std::vector<VkImageView> views;
        for (const auto& res : resources) views.push_back(res->view);
        return views;
    };

    const auto extent = hmlContext->hmlSwapchain->extent;
    const size_t count = hmlContext->imageCount();
    gBufferPositions.resize(count);
    gBufferNormals.resize(count);
    gBufferColors.resize(count);
    gBufferLightSpacePositions.resize(count);
    brightness1Textures.resize(count);
    // brightness2Textures.resize(count);
    mainTextures.resize(count);
    for (size_t i = 0; i < count; i++) {
        // XXX TODO use modified findDepthFormat()
        gBufferPositions[i]           = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R16G16B16A16_SFLOAT);
        // gBufferPositions[i]           = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R32G32B32A32_SFLOAT);
        gBufferNormals[i]             = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R16G16B16A16_SFLOAT);
        gBufferColors[i]              = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R8G8B8A8_SRGB);
        // gBufferLightSpacePositions[i] = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R32G32B32A32_SFLOAT);
        gBufferLightSpacePositions[i] = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R16G16B16A16_SFLOAT);
        // gBufferLightSpacePositions[i] = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R16G16B16A16_SFLOAT);
        brightness1Textures[i]        = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R8G8B8A8_SRGB);
        // brightness2Textures[i]        = hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R8G8B8A8_SRGB);
        mainTextures[i]               = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R8G8B8A8_SRGB);
        if (!gBufferPositions[i])           return false;
        if (!gBufferNormals[i])             return false;
        if (!gBufferColors[i])              return false;
        if (!gBufferLightSpacePositions[i]) return false;
        if (!brightness1Textures[i])        return false;
        // if (!brightness2Textures[i])        return false;
        if (!mainTextures[i])               return false;
    }
    hmlDepthResource = hmlContext->hmlResourceManager->newDepthResource(extent);
    if (!hmlDepthResource) return false;

    hmlShadow = hmlContext->hmlResourceManager->newShadowResource(extent, VK_FORMAT_D32_SFLOAT);
    if (!hmlShadow) return false;


    hmlDeferredRenderer->specify({ gBufferPositions, gBufferNormals, gBufferColors, gBufferLightSpacePositions, {hmlShadow,hmlShadow,hmlShadow} });
    hmlUiRenderer->specify({ gBufferPositions, gBufferNormals, gBufferColors, gBufferLightSpacePositions, {hmlShadow,hmlShadow,hmlShadow} });
    // hmlDeferredRenderer->specify({ gBufferPositions, gBufferNormals, gBufferColors, gBufferLightSpacePositions });
    // hmlUiRenderer->specify({ gBufferPositions, gBufferNormals, gBufferColors, gBufferLightSpacePositions });
    // hmlUiRenderer->specify({ gBufferPositions, gBufferNormals, gBufferColors });
    // hmlBlurRenderer->specify(brightness1Textures, brightness2Textures);
    hmlBloomRenderer->specify(mainTextures, brightness1Textures);


    hmlPipe = std::make_unique<HmlPipe>(hmlContext, imageAvailableSemaphores, renderFinishedSemaphores, inFlightFences);
    const float VERY_FAR = 10000.0f;
    bool allGood = true;

    // ================================= PASS =================================
    // ======== Render shadow-casting geometry into shadow map ========
    hmlTerrainRenderer->setMode(HmlTerrainRenderer::Mode::Shadowmap);
    hmlRenderer->setMode(HmlRenderer::Mode::Shadowmap);
    allGood &= hmlPipe->addStage(
        [&](){ // pre func
            hmlTerrainRenderer->setMode(HmlTerrainRenderer::Mode::Shadowmap);
            hmlRenderer->setMode(HmlRenderer::Mode::Shadowmap);
        },
        { hmlTerrainRenderer, hmlRenderer }, // drawers
        {}, // output color attachments
        { // optional depth attachment
            HmlRenderPass::DepthStencilAttachment{
                .imageFormat = hmlShadow->format,
                .imageView = hmlShadow->view,
                .clearColor = VkClearDepthStencilValue{ 1.0f, 0 }, // 1.0 is farthest
                .store = true,
                .preLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            }
        },
        {}, // post transitions
        std::nullopt // post func
        // [&](uint32_t imageIndex){ // post func
        //     hmlRenderer->setMode(HmlRenderer::Mode::Regular);
        // }
    );
    // ================================= PASS =================================
    // ======== Renders main geometry into the GBuffer ========
    hmlTerrainRenderer->setMode(HmlTerrainRenderer::Mode::Regular);
    hmlRenderer->setMode(HmlRenderer::Mode::Regular);
    allGood &= hmlPipe->addStage( // deferred prep
        [&](){ // pre func
            hmlTerrainRenderer->setMode(HmlTerrainRenderer::Mode::Regular);
            hmlRenderer->setMode(HmlRenderer::Mode::Regular);
        },
        { hmlTerrainRenderer, hmlRenderer }, // drawers
        { // output color attachments
            HmlRenderPass::ColorAttachment{
                .imageFormat = gBufferPositions[0]->format,
                .imageViews = viewsFrom(gBufferPositions),
                .clearColor = {{ VERY_FAR, VERY_FAR, VERY_FAR, 1.0f }},
                .preLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                // .postLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            HmlRenderPass::ColorAttachment{
                .imageFormat = gBufferNormals[0]->format,
                .imageViews = viewsFrom(gBufferNormals),
                .clearColor = {{ 0.0f, 0.0f, 0.0f, 1.0f }},
                .preLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                // .postLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            HmlRenderPass::ColorAttachment{
                .imageFormat = gBufferColors[0]->format,
                .imageViews = viewsFrom(gBufferColors),
                .clearColor = {{ weather.fogColor.x, weather.fogColor.y, weather.fogColor.z, 1.0f }},
                .preLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                // .postLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            HmlRenderPass::ColorAttachment{
                .imageFormat = gBufferLightSpacePositions[0]->format,
                .imageViews = viewsFrom(gBufferLightSpacePositions),
                .clearColor = {{ 2.0f, 2.0f, 2.0f, 2.0f }}, // TODO i dunno!!!
                .preLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
        },
        { // optional depth attachment
            HmlRenderPass::DepthStencilAttachment{
                .imageFormat = hmlDepthResource->format,
                .imageView = hmlDepthResource->view,
                .clearColor = VkClearDepthStencilValue{ 1.0f, 0 }, // 1.0 is farthest
                .store = true,
                .preLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .postLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }
        },
        {}, // post transitions
        std::nullopt // post func
        // [&](uint32_t imageIndex){ // post func
        //     gBufferPositions[imageIndex].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        //     gBufferNormals[imageIndex].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        //     gBufferColors[imageIndex].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        // }
    );
    // ================================= PASS =================================
    // ======== Renders a 2D texture using the GBuffer ========
    allGood &= hmlPipe->addStage( // deferred
        std::nullopt, // pre func
        { hmlDeferredRenderer }, // drawers
        { // output color attachments
            HmlRenderPass::ColorAttachment{
                // .imageFormat = hmlSwapchain->imageFormat,
                // .imageViews = hmlSwapchain->imageViews,
                .imageFormat = mainTextures[0]->format,
                .imageViews = viewsFrom(mainTextures),
                .clearColor = {{ 1.0f, 0.0f, 0.0f, 1.0f }}, // TODO confirm that validation complains otherwise
                .preLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .postLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                // .postLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
        },
        std::nullopt, // optional depth attachment
        {}, // post transitions
        std::nullopt // post func
    );
    // Renders on top of the deferred texture using the depth info from the prep pass
#if DEBUG_TERRAIN
    hmlTerrainRenderer->setMode(HmlTerrainRenderer::Mode::Debug);
#endif
    allGood &= hmlPipe->addStage( // forward
        [&](){ // pre func
#if DEBUG_TERRAIN
            hmlTerrainRenderer->setMode(HmlTerrainRenderer::Mode::Debug);
#endif
        },
        {
#if SNOW_IS_ON
            hmlSnowRenderer,
#endif
            hmlLightRenderer
#if DEBUG_TERRAIN
            , hmlTerrainRenderer
#endif
        },
        { // output color attachments
            HmlRenderPass::ColorAttachment{
                .imageFormat = mainTextures[0]->format,
                .imageViews = viewsFrom(mainTextures),
                .clearColor = std::nullopt,
                // .clearColor = {{ 1.0f, 0.0f, 0.0f, 1.0f }}, // TODO confirm that validation complains otherwise
                .preLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            HmlRenderPass::ColorAttachment{
                .imageFormat = brightness1Textures[0]->format,
                .imageViews = viewsFrom(brightness1Textures),
                .clearColor = {{ 0.0f, 0.0f, 0.0f, 1.0f }},
                .preLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                // .postLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
        },
        { // optional depth attachment
            HmlRenderPass::DepthStencilAttachment{
                .imageFormat = hmlDepthResource->format,
                .imageView = hmlDepthResource->view,
                .clearColor = std::nullopt,
                .store = false,
                .preLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }
        },
        { // post transitions
            // {
            //     .resourcePerImage = brightness1Textures,
            //     .dstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            // },
            // {
            //     .resourcePerImage = mainTextures,
            //     .dstLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            // },
        },
        std::nullopt // post func
    );

    // ================================= PASS =================================
    // hmlBlurRenderer->modeVertical();
    // hmlBlurRenderer->modeHorizontal();
    // for (auto i = 0; i < 1; i++) {
    //     allGood &= hmlPipe->addStage( // horizontal blur
    //         { hmlBlurRenderer }, // drawers
    //         { // output color attachments
    //             {
    //                 .imageFormat = brightness2Textures[0]->format,
    //                 .imageViews = viewsFrom(brightness2Textures),
    //                 .clearColor = (i == 0) ? std::make_optional<VkClearColorValue>({ 0.0f, 0.0f, 0.0f, 1.0f }) : std::nullopt,
    //                 .preLayout = (i == 0) ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //                 .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //             },
    //         },
    //         std::nullopt, // optional depth attachment
    //         {}, // post transitions
    //         [hmlBlurRenderer=hmlBlurRenderer](uint32_t){ hmlBlurRenderer->modeVertical(); } // post func
    //     );
    //
    //     allGood &= hmlPipe->addStage( // vertical blur
    //         { hmlBlurRenderer }, // drawers
    //         { // output color attachments
    //             HmlRenderPass::ColorAttachment{
    //                 .imageFormat = brightness1Textures[0]->format,
    //                 .imageViews = viewsFrom(brightness1Textures),
    //                 .clearColor = std::nullopt,
    //                 .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //                 .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //             },
    //         },
    //         std::nullopt, // optional depth attachment
    //         {}, // post transitions
    //         [hmlBlurRenderer=hmlBlurRenderer](uint32_t){ hmlBlurRenderer->modeHorizontal(); } // post func
    //     );
    // }
    // ================================= PASS =================================
    allGood &= hmlPipe->addStage( // bloom
        std::nullopt, // pre func
        { hmlBloomRenderer }, // drawers
        { // output color attachments
            HmlRenderPass::ColorAttachment{
                .imageFormat = hmlContext->hmlSwapchain->imageFormat,
                .imageViews = hmlContext->hmlSwapchain->imageViews,
                .clearColor = {{ 0.0f, 0.0f, 0.0f, 1.0f }}, // TODO confirm that validation complains otherwise
                .preLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .postLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        },
        std::nullopt, // optional depth attachment
        {}, // post transitions
        std::nullopt // post func
    );
    // ================================= PASS =================================
    // ======== Renders multiple 2D textures on top of everything ========
    allGood &= hmlPipe->addStage( // ui
        [&](){ // pre func
            hmlContext->hmlImgui->finilize();
        },
        { hmlUiRenderer, hmlImguiRenderer }, // drawers
        { // output color attachments
            HmlRenderPass::ColorAttachment{
                .imageFormat = hmlContext->hmlSwapchain->imageFormat,
                .imageViews = hmlContext->hmlSwapchain->imageViews,
                .clearColor = std::nullopt,
                .preLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
        },
        std::nullopt, // optional depth attachment
        {}, // post transitions
        std::nullopt // post func
        // [&](uint32_t imageIndex){ // post func
        //     hmlContext->hmlImgui->beginFrame();
        // }
    );

    if (!allGood) {
        std::cerr << "::> Could not create some stages in HmlPipe.\n";
        return false;
    }

    return true;
}


void Himmel::recreateSwapchain() noexcept {
    std::cout << ":> Recreating the Swapchain.\n";

    // Handle window minimization
    for (;;) {
        const auto [width, height] = hmlContext->hmlWindow->getFramebufferSize();
        if (width > 0 && height > 0) break;
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(hmlContext->hmlDevice->device);

    hmlContext->hmlSwapchain = HmlSwapchain::create(hmlContext->hmlWindow, hmlContext->hmlDevice, hmlContext->hmlResourceManager, { hmlContext->hmlSwapchain->swapchain });

    // Because they have to be rerecorder, and for that they need to be reset first
    hmlContext->hmlCommands->resetCommandPool(hmlContext->hmlCommands->commandPoolGeneral);

    if (!prepareResources()) {
        std::cerr << "::> Failed to prepare resources.\n";
        return;
    }

    proj = projFrom(hmlContext->hmlSwapchain->extentAspect());
}


bool Himmel::createSyncObjects() noexcept {
    imageAvailableSemaphores.resize(hmlContext->maxFramesInFlight);
    renderFinishedSemaphores.resize(hmlContext->maxFramesInFlight);
    inFlightFences.resize(hmlContext->maxFramesInFlight);
    imagesInFlight.resize(hmlContext->imageCount(), VK_NULL_HANDLE); // initially not a single frame is using an image

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // So that the fences start in signaled state (which means the frame is
    // available to be acquired by CPU). Subsequent frames are signaled by
    // Vulkan upon command buffer execution finish.
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < hmlContext->maxFramesInFlight; i++) {
        if (vkCreateSemaphore(hmlContext->hmlDevice->device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(hmlContext->hmlDevice->device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence    (hmlContext->hmlDevice->device, &fenceInfo,     nullptr, &inFlightFences[i])           != VK_SUCCESS) {

            std::cerr << "::> Failed to create sync objects.\n";
            return false;
        }
    }

    return true;
}


Himmel::~Himmel() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying Himmel...\n";
#endif
    // vkDeviceWaitIdle(hmlDevice->device);

    if (!successfulInit) {
        std::cerr << "::> Himmel init has not finished successfully, so no proper cleanup is node.\n";
        return;
    }

    for (size_t i = 0; i < hmlContext->maxFramesInFlight; i++) {
        vkDestroySemaphore(hmlContext->hmlDevice->device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(hmlContext->hmlDevice->device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(hmlContext->hmlDevice->device, inFlightFences[i], nullptr);
    }

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlContext->hmlDevice->device, generalDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, generalDescriptorSetLayout, nullptr);
}


glm::mat4 Himmel::projFrom(float aspect_w_h) noexcept {
    const float near = 0.1f;
    const float far = 1000.0f;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect_w_h, near, far);
    proj[1][1] *= -1; // fix the inverted Y axis of GLM
    return proj;
}
