#include "Himmel.h"


#define SNOW_IS_ON    1
#define DEBUG_TERRAIN 0


bool Himmel::init() noexcept {
    if (!initContext()) return false;

    const char* heightmapFile = "../models/heightmap.png";
    { // World
        const auto worldHalfSize = 250.0f;
        const auto worldStart  = glm::vec2(-worldHalfSize, -worldHalfSize);
        const auto worldFinish = glm::vec2( worldHalfSize,  worldHalfSize);
        const auto worldHeight = 70.0f;
        world = std::make_unique<World>(worldStart, worldFinish, worldHeight, heightmapFile);
    }

    { // Car
        const float carX = 0.0f;
        const float carZ = 0.0f;
        const auto carPos = glm::vec3{carX, world->heightAt({ carX, carZ }), carZ};
        const auto carSizeScaler = 2.5f;
        car = std::make_shared<Car>(carPos, carSizeScaler);
    }


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


    // ================ Weather
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


    if (!initRenderers(heightmapFile)) return false;


    // ================ Camera
    hmlCamera = std::make_unique<HmlCameraFreeFly>(glm::vec3{ 0, 91.5, 50 });
    dynamic_cast<HmlCameraFreeFly*>(hmlCamera.get())->rotateDir(0, 0);
    // hmlCamera = std::make_unique<HmlCameraFollow>(100);
    // dynamic_cast<HmlCameraFollow*>(hmlCamera.get())->rotateDir(-28.0f, 307.0f);
    // dynamic_cast<HmlCameraFollow*>(hmlCamera.get())->target(car);

    proj = projFrom(hmlContext->hmlSwapchain->extentAspect());

    if (!initLights()) return false;
    if (!initModels()) return false;
#if WITH_PHYSICS
    if (!initPhysics()) return false;
#endif
    if (!initEntities()) return false; // NOTE must be after Physics because is expected to set the last entity

    // NOTE must be after initEntities() so that HmlComplexRenderer knows what pipelines to create
    if (!prepareResources()) return false;

    successfulInit = true;
    return true;
}


bool Himmel::initContext() noexcept {
    const char* windowName = "Planes game";

    hmlContext = std::make_shared<HmlContext>();
    hmlContext->maxFramesInFlight = 3;

    hmlContext->hmlWindow = HmlWindow::create(1900, 1000, windowName, false);
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

#if USE_TIMESTAMP_QUERIES
    hmlContext->hmlQueries = HmlQueries::create(hmlContext->hmlDevice, hmlContext->hmlCommands, 2 * hmlContext->imageCount());
    if (!hmlContext->hmlQueries) return false;
#endif

#if WITH_IMGUI
    hmlContext->hmlImgui = HmlImgui::create(hmlContext->hmlWindow, hmlContext->hmlResourceManager, hmlContext->maxFramesInFlight);
    if (!hmlContext->hmlImgui) return false;
#endif

    return true;
}


bool Himmel::initRenderers(const char* heightmapFile) noexcept {
    hmlRenderer = HmlRenderer::create(hmlContext, generalDescriptorSetLayout);
    if (!hmlRenderer) return false;

    hmlComplexRenderer = HmlComplexRenderer::create(hmlContext, generalDescriptorSetLayout);
    if (!hmlComplexRenderer) return false;

    hmlDeferredRenderer = HmlDeferredRenderer::create(hmlContext, generalDescriptorSetLayout);
    if (!hmlDeferredRenderer) return false;

    hmlUiRenderer = HmlUiRenderer::create(hmlContext);
    if (!hmlUiRenderer) return false;

    hmlBlurRenderer = HmlBlurRenderer::create(hmlContext);
    if (!hmlBlurRenderer) return false;

    hmlBloomRenderer = HmlBloomRenderer::create(hmlContext);
    if (!hmlBloomRenderer) return false;

#if WITH_IMGUI
    // NOTE specify buffers explicitly, even though we have context already, to highlight this dependency
    hmlImguiRenderer = HmlImguiRenderer::create(hmlContext->hmlImgui->vertexBuffers, hmlContext->hmlImgui->indexBuffers, hmlContext);
    if (!hmlImguiRenderer) return false;
#endif


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

    hmlLightRenderer = HmlLightRenderer::create(hmlContext, generalDescriptorSetLayout, generalDescriptorSet_0_perImage);
    if (!hmlLightRenderer) return false;

    return true;
}


bool Himmel::initEntities() noexcept {
    { // Arbitrary Entities
        // PHONY with a texture
        {
            entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.phony));
            auto modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, { 0.0f, 50.0f, -200.0f });
            modelMatrix = glm::scale(modelMatrix, glm::vec3(150.0f));
            modelMatrix = glm::rotate(modelMatrix, glm::radians(180.0f), glm::vec3(0.0, 0.0, 1.0));
            entities.back()->modelMatrix = modelMatrix;
        }
        { // back side
            entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.phony));
            auto modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, { 0.0f, 50.0f, -200.0f - 0.001f });
            modelMatrix = glm::scale(modelMatrix, glm::vec3(150.0f));
            modelMatrix = glm::rotate(modelMatrix, glm::radians(180.0f), glm::vec3(0.0, 0.0, 1.0));
            modelMatrix = glm::rotate(modelMatrix, glm::radians(180.0f), glm::vec3(0.0, 1.0, 0.0));
            entities.back()->modelMatrix = modelMatrix;
        }

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.plane, glm::vec3{ 1.0f, 1.0f, 1.0f }));
        entities.back()->modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3{0.0f, 35.0f, 30.0f});

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.plane, glm::vec3{ 0.8f, 0.2f, 0.5f }));
        entities.back()->modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3{0.0f, 45.0f, 60.0f});

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.plane, glm::vec3{ 0.2f, 0.2f, 0.9f }));
        entities.back()->modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3{0.0f, 45.0f, 90.0f});

        // Flat surface with a tree
        // { // #1
        //     entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.flat, glm::vec3{ 0.0f, 0.0f, 0.0f }));
        //     auto modelMatrix = glm::mat4(1.0f);
        //     modelMatrix = glm::translate(modelMatrix, { 0.0f, 40.0f, 0.0f });
        //     modelMatrix = glm::scale(modelMatrix, glm::vec3(40.0f));
        //     modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f + 180.0f), glm::vec3(1, 0, 0));
        //     entities.back()->modelMatrix = modelMatrix;
        // }
        // { // #1 back side
        //     entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.flat, glm::vec3{ 0.0f, 0.0f, 0.0f }));
        //     auto modelMatrix = glm::mat4(1.0f);
        //     modelMatrix = glm::translate(modelMatrix, { 0.0f, 40.0f - 0.001f, 0.0f });
        //     modelMatrix = glm::scale(modelMatrix, glm::vec3(40.0f));
        //     modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(1, 0, 0));
        //     entities.back()->modelMatrix = modelMatrix;
        // }
        // { // $2
        //     entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.flat, glm::vec3{ 1.0f, 1.0f, 1.0f }));
        //     auto modelMatrix = glm::mat4(1.0f);
        //     modelMatrix = glm::translate(modelMatrix, { 0.0f, 60.0f, -20.0f });
        //     modelMatrix = glm::scale(modelMatrix, glm::vec3(40.0f));
        //     // modelMatrix = glm::rotate(modelMatrix, glm::radians(90.0f), glm::vec3(0, 0, 1));
        //     entities.back()->modelMatrix = modelMatrix;
        // }
        // { // $2 back side
        //     entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.flat, glm::vec3{ 1.0f, 1.0f, 1.0f }));
        //     auto modelMatrix = glm::mat4(1.0f);
        //     modelMatrix = glm::translate(modelMatrix, { 0.0f, 60.0f, -20.0f - 0.001f });
        //     modelMatrix = glm::scale(modelMatrix, glm::vec3(40.0f));
        //     modelMatrix = glm::rotate(modelMatrix, glm::radians(180.0f), glm::vec3(1, 0, 0));
        //     entities.back()->modelMatrix = modelMatrix;
        // }
        // {
        //     entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.tree2));
        //     const auto pos = glm::vec3{ 0, 40, 0 };
        //     const float scale = 7.0f;
        //     auto modelMatrix = glm::mat4(1.0f);
        //     modelMatrix = glm::translate(modelMatrix, pos);
        //     modelMatrix = glm::scale(modelMatrix, glm::vec3(scale));
        //     entities.back()->modelMatrix = std::move(modelMatrix);
        // }
        // entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.viking));
        // entities.back()->modelMatrix = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(10.0, 10.0, 10.0)), glm::vec3{40.0f, 30.0f, 70.0f});

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

                entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.tree));
                entities.back()->modelMatrix = modelMatrix;
            }
        }
#endif

        // XXX We rely on it being last in update() XXX
        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.car, glm::vec3{ 0.2f, 0.7f, 0.4f }));
        entities.back()->modelMatrix = car->getModelMatrix();


        hmlRenderer->specifyEntitiesToRender(entities);

        {
            auto modelMatrix = glm::mat4(1.0f);
            // modelMatrix = glm::translate(modelMatrix, glm::vec3{0.0f, 60.0f, 60.0f});
            // modelMatrix = glm::scale(modelMatrix, glm::vec3(3.0f));

            complexEntities.push_back(std::make_shared<HmlComplexRenderer::Entity>(modelStorage.complexCube));
            complexEntities.back()->modelMatrix = modelMatrix;

            coolCubeEntity = complexEntities.back();
        }
        {
            auto modelMatrix = glm::mat4(1.0f);
            // modelMatrix = glm::translate(modelMatrix, glm::vec3{0.0f, 60.0f, 60.0f});
            // modelMatrix = glm::scale(modelMatrix, glm::vec3(3.0f));

            complexEntities.push_back(std::make_shared<HmlComplexRenderer::Entity>(modelStorage.rustyCube));
            complexEntities.back()->modelMatrix = modelMatrix;

            rustyCubeEntity = complexEntities.back();
        }
        {
            auto modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, glm::vec3{0.0f, 60.0f, 40.0f});
            modelMatrix = glm::scale(modelMatrix, glm::vec3(5.0f));

            complexEntities.push_back(std::make_shared<HmlComplexRenderer::Entity>(modelStorage.damagedHelmet));
            complexEntities.back()->modelMatrix = modelMatrix;
        }
        {
            auto modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, glm::vec3{0.0f, 60.0f, 20.0f});
            modelMatrix = glm::scale(modelMatrix, glm::vec3(7.0f));

            complexEntities.push_back(std::make_shared<HmlComplexRenderer::Entity>(modelStorage.damagedHelmet));
            complexEntities.back()->modelMatrix = modelMatrix;
        }

        hmlComplexRenderer->specifyEntitiesToRender(complexEntities);
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

            staticEntities.emplace_back((i % 2) ? modelStorage.tree1 : modelStorage.tree2, modelMatrix);
        }

        hmlRenderer->specifyStaticEntitiesToRender(staticEntities);
    }

    return true;
}


bool Himmel::initLights() noexcept {
    // Add lights
    const float LIGHT_RADIUS = 2.0f;
    const size_t lightsCount = 2;
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
    { // The light for the setup
        const auto pos = glm::vec3(0, 157, 0);
        const auto color = glm::vec3(1.0, 1.0, 1.0);
        pointLightsStatic.push_back(HmlLightRenderer::PointLight{
            .color = color,
            .intensity = 9000.0f,
            .position = pos,
            .radius = LIGHT_RADIUS,
        });
    }
    { // The light for CoolCube
        const auto pos = glm::vec3(10, 70, 50);
        const auto color = glm::vec3(1.0, 1.0, 1.0);
        pointLightsStatic.push_back(HmlLightRenderer::PointLight{
            .color = color,
            .intensity = 1800.0f,
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
        pointLightsDynamic.push_back(HmlLightRenderer::PointLight{
            .color = glm::vec3(1.0, 1.0, 1.0),
            .intensity = 2000.0f,
            .position = {},
            .radius = LIGHT_RADIUS * 0.5f,
        });
    }

    hmlLightRenderer->specify(pointLightsStatic.size() + pointLightsDynamic.size());

    return true;
}


bool Himmel::initModels() noexcept {
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
        modelStorage.phony = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices, "../models/girl.png", VK_FILTER_LINEAR);
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
        modelStorage.flat = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
    }

    { // A viking kitchen box
        std::vector<HmlSimpleModel::Vertex> vertices;
        std::vector<uint32_t> indices;
        if (!HmlSimpleModel::load("../models/viking_room.obj", vertices, indices)) return false;

        const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
        modelStorage.viking = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices, "../models/viking_room.png", VK_FILTER_LINEAR);
    }

    { // Plane
        std::vector<HmlSimpleModel::Vertex> vertices;
        std::vector<uint32_t> indices;
        if (!HmlSimpleModel::load("../models/plane.obj", vertices, indices)) return false;

        const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
        modelStorage.plane = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
    }

    { // Car
        std::vector<HmlSimpleModel::Vertex> vertices;
        std::vector<uint32_t> indices;
        if (!HmlSimpleModel::load("../models/my_car.obj", vertices, indices)) return false;

        const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
        modelStorage.car = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
    }

    { // Tree#1
        std::vector<HmlSimpleModel::Vertex> vertices;
        std::vector<uint32_t> indices;
        if (!HmlSimpleModel::load("../models/tree/basic_tree.obj", vertices, indices)) return false;

        const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
        modelStorage.tree1 = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices, "../models/tree/basic_tree.png", VK_FILTER_LINEAR);
    }
    { // Tree#2
        std::vector<HmlSimpleModel::Vertex> vertices;
        std::vector<uint32_t> indices;
        if (!HmlSimpleModel::load("../models/tree/basic_tree_2.obj", vertices, indices)) return false;

        const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
        modelStorage.tree2 = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices, "../models/tree/basic_tree_2.png", VK_FILTER_LINEAR);
    }

    { // Sphere
        std::vector<HmlSimpleModel::Vertex> vertices;
        std::vector<uint32_t> indices;
        if (!HmlSimpleModel::load("../models/isosphere.obj", vertices, indices)) return false;

        const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
        modelStorage.sphere = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
    }

    { // Cube
        std::vector<HmlSimpleModel::Vertex> vertices;
        std::vector<uint32_t> indices;
        if (!HmlSimpleModel::load("../models/cube.obj", vertices, indices)) return false;

        const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
        modelStorage.cube = hmlContext->hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
    }

    // ==== Complex Models (glTF)

    // { // Sponza
    //     auto hmlScene = hmlContext->hmlResourceManager->loadAsset("../models/sponza/NewSponza_Main_glTF_002.gltf");
    //     modelStorage.sponza = std::move(hmlScene->hmlComplexModelResources.at(0));
    // }

    { // CoolCube
        auto hmlScene = hmlContext->hmlResourceManager->loadAsset("../models/CoolCube/Cube.gltf");
        modelStorage.complexCube = std::move(hmlScene->hmlComplexModelResources.at(0));
    }

    { // RustyCube
        auto hmlScene = hmlContext->hmlResourceManager->loadAsset("../models/RustyCube/Cube.gltf");
        modelStorage.rustyCube = std::move(hmlScene->hmlComplexModelResources.at(0));
    }

    { // Damanged helmet
        auto hmlScene = hmlContext->hmlResourceManager->loadAsset("../models/DamagedHelmet.gltf");
        modelStorage.damagedHelmet = std::move(hmlScene->hmlComplexModelResources.at(0));
    }

    // return false;
    return true;
}


bool Himmel::initPhysics() noexcept {
    // hmlPhysics = std::make_unique<HmlPhysics>(HmlPhysics::Mode::SameThread);
    // hmlPhysics = std::make_unique<HmlPhysics>(HmlPhysics::Mode::SameThreadAndHelperThreads);
    hmlPhysics = std::make_unique<HmlPhysics>(HmlPhysics::Mode::AnotherThread);

    initPhysicsTestbench();

    return true;
}


void Himmel::initPhysicsTestbench() noexcept {
    // testbenchBoxWithObjects();
    // testbenchFriction();
}


void Himmel::testbenchBoxWithObjects() noexcept {
    const float halfSide = 50.0f;
    const float baseHeight = 50.0f;
    const float halfHeight = halfSide;
    const float wallThickness = 2.0f;
#if 1
    { // Bottom
        auto object = HmlPhysics::Object::createBox({ 0, baseHeight, 0 }, { halfSide, wallThickness, halfSide });

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.7f, 1.0f, 0.7f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
    { // Up
        auto object = HmlPhysics::Object::createBox({ 0, baseHeight + 2 * halfHeight, 0 }, { halfSide, wallThickness, halfSide });

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.8f, 0.8f, 0.8f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
    { // Far
        auto object = HmlPhysics::Object::createBox({ 0, baseHeight + halfHeight, -halfSide }, { halfSide, halfHeight, wallThickness });

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.7f, 0.7f, 1.0f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
    { // Left
        auto object = HmlPhysics::Object::createBox({ -halfSide, baseHeight + halfHeight, 0 }, { wallThickness, halfHeight, halfSide });

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 1.0f, 0.7f, 0.7f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
    { // Right
        auto object = HmlPhysics::Object::createBox({ +halfSide, baseHeight + halfHeight, 0 }, { wallThickness, halfHeight, halfSide });

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.8f, 0.8f, 0.8f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
    { // Near
        auto object = HmlPhysics::Object::createBox({ 0, baseHeight + halfHeight, +halfSide }, { halfSide, halfHeight, wallThickness });

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.8f, 0.8f, 0.8f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
#endif


#if 1
    { // Platform
        auto object = HmlPhysics::Object::createBox({ 0, baseHeight + 0.8f*halfHeight, 0 },
                { 0.35f*halfSide, 0.6f*wallThickness, 0.35f*halfSide });
        object.orientation = glm::rotate(glm::quat(1, glm::vec3{}), 2 * 0.2f, glm::vec3(0,0,1));

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.1f, 0.1f, 0.1f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
#endif


    const float density = 4.0f;
    const size_t boxesCount = 40;
    const size_t spheresCount = 60;
    const float maxSpeed = 6.0f;
#if 1
    for (size_t i = 0; i < boxesCount; i++) {
        const auto pos = glm::vec3{
            hml::getRandomUniformFloat(-halfSide*0.8f, halfSide*0.8f),
            baseHeight + 1.3f * halfHeight,
            // hml::getRandomUniformFloat(55, 65),
            hml::getRandomUniformFloat(-halfSide*0.8f, halfSide*0.8f)
        };
        const auto velocity = glm::vec3{
            hml::getRandomUniformFloat(-maxSpeed, maxSpeed),
            0.0f,
            hml::getRandomUniformFloat(-maxSpeed, maxSpeed)
        };
        const auto color = glm::vec3{
            hml::getRandomUniformFloat(0.0f, 1.0f),
            hml::getRandomUniformFloat(0.0f, 1.0f),
            hml::getRandomUniformFloat(0.0f, 1.0f)
        };
        const auto halfDimensions = glm::vec3{
            hml::getRandomUniformFloat(0.4f, 2.0f),
            // hml::getRandomUniformFloat(0.2f, 2.0f),
            2.0f,
            hml::getRandomUniformFloat(0.4f, 2.0f)
        };

        const float volume = 8 * halfDimensions.x * halfDimensions.y * halfDimensions.z;
        const float mass = volume * density;
        auto object = HmlPhysics::Object::createBox(pos, halfDimensions, mass, velocity, glm::vec3{0,0,0});

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, color));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
#endif
#if 1
    for (size_t i = 0; i < spheresCount; i++) {
        const auto pos = glm::vec3{
            hml::getRandomUniformFloat(-halfSide*0.8f, halfSide*0.8f),
            baseHeight + 1.3f * halfHeight,
            // hml::getRandomUniformFloat(55, 65),
            hml::getRandomUniformFloat(-halfSide*0.8f, halfSide*0.8f)
        };
        const auto velocity = glm::vec3{
            hml::getRandomUniformFloat(-maxSpeed, maxSpeed),
            0.0f,
            hml::getRandomUniformFloat(-maxSpeed, maxSpeed)
        };
        const auto color = glm::vec3{
            hml::getRandomUniformFloat(0.0f, 1.0f),
            hml::getRandomUniformFloat(0.0f, 1.0f),
            hml::getRandomUniformFloat(0.0f, 1.0f)
        };

        const float radius = hml::getRandomUniformFloat(0.4f, 2.0f);
        const float volume = 4.0f / 3.0f * glm::pi<float>() * radius * radius * radius;
        const float mass = volume * density;
        auto object = HmlPhysics::Object::createSphere(pos, radius, mass, velocity, glm::vec3{});

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.sphere, color));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
#endif
}


void Himmel::testbenchImpulse() noexcept {
#if 0
    { // Dyn wall 1
        auto object = HmlPhysics::Object::createBox(
            { 5, baseHeight + 1.2f * halfHeight, 0 }, { 0.8f, 4, 0.8f }, 6, glm::vec3{ 0, 0, 0 }, glm::vec3{0,0,0});
        object.orientation = glm::rotate(glm::quat(1, glm::vec3{}), 2 * 0.5f, glm::vec3(0,0,1));

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.9f, 0.9f, 0.9f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
        debugId = id;
    }
    // { // Dyn wall 2
    //     const float halfHeight = 2.0f;
    //     auto object = HmlPhysics::Object::createBox({ -10, 59, 25 }, { 6, halfHeight, 4 }, 5, glm::vec3{ 0, 0, 0 }, glm::vec3{0,0,0});
    //     // object.orientation = glm::rotate(glm::quat(1, glm::vec3{}), 0.3f, glm::vec3(1,0,0));
    //
    //     entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.6f, 1.0f, 0.6f}));
    //     entities.back()->modelMatrix = object.modelMatrix();
    //
    //     const auto id = hmlPhysics->registerObject(std::move(object));
    //     physicsIdToEntity[id] = entities.back();
    // }
#endif
#if 0
    { // Dyn wall 3
        const float halfHeight = 4.0f;
        auto object = HmlPhysics::Object::createBox({ -6.5f, 61, 5.7f }, { 1, halfHeight, 1 }, 5, glm::vec3{ 0, 0, 0 }, glm::vec3{0,0,0});
        // object.orientation = glm::rotate(glm::quat(1, glm::vec3{}), 1.0f, glm::vec3(0,1,0));

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.9f, 0.9f, 0.9f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
    { // Dyn wall 4
        const float halfHeight = 2.0f;
        auto object = HmlPhysics::Object::createBox({ -10, 60, 10 }, { 6, halfHeight, 4 }, 5, glm::vec3{ 0, 0, 8 }, glm::vec3{0,1,0});
        // object.orientation = glm::rotate(glm::quat(1, glm::vec3{}), 1.0f, glm::vec3(1,0,0));

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.6f, 1.0f, 0.6f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
#endif
}


void Himmel::testbenchContactPoints() noexcept {

}


void Himmel::testbenchFriction() noexcept {
    hmlCamera = std::make_unique<HmlCameraFreeFly>(glm::vec3{ 5, 55, 25 });
    dynamic_cast<HmlCameraFreeFly*>(hmlCamera.get())->setDir(0, 0);
    // dynamic_cast<HmlCameraFreeFly*>(hmlCamera.get())->setPos();

    const float halfSide = 50.0f;
    const float baseHeight = 50.0f;
    const float wallThickness = 2.0f;
    { // Bottom
        auto object = HmlPhysics::Object::createBox({ 0, baseHeight, 0 }, { halfSide, wallThickness, halfSide });

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.7f, 1.0f, 0.7f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }

    { // Box
        auto object = HmlPhysics::Object::createBox({ 0, baseHeight + 5.1f, 0 }, { 1, 3, 1 }, 5, glm::vec3{ 1, 0, 0 }, glm::vec3{0,0,0});
        // object.orientation = glm::rotate(glm::quat(1, glm::vec3{}), 1.0f, glm::vec3(0,1,0));

        entities.push_back(std::make_shared<HmlRenderer::Entity>(modelStorage.cube, glm::vec3{ 0.1f, 0.2f, 0.9f}));
        entities.back()->modelMatrix = object.modelMatrix();

        const auto id = hmlPhysics->registerObject(std::move(object));
        physicsIdToEntity[id] = entities.back();
    }
}


bool Himmel::run() noexcept {
    static auto startTime = std::chrono::high_resolution_clock::now();
    while (!hmlContext->hmlWindow->shouldClose() && !exitRequested) {
#if USE_TIMESTAMP_QUERIES
        hmlContext->hmlQueries->beginFrame();
#endif

        // To force initial update of stuff
        // if (hmlContext->currentFrame) hmlCamera.invalidateCache();

        glfwPollEvents();

#if WITH_IMGUI
        hmlContext->hmlImgui->beginFrame();
#endif

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

#if WITH_IMGUI
        {
            static constexpr uint32_t showEvery = 20;
            static uint32_t showCounter = 0;
            static float showedElapsedMicrosWaitNextInFlightFrame = 0;
            static float showedElapsedMicrosAcquire = 0;
            static float showedElapsedMicrosWaitSwapchainImage = 0;
            static float showedElapsedMicrosRecord = 0;
            static float showedElapsedMicrosSubmit = 0;
            static float showedElapsedMicrosPresent = 0;
            static float showedFps = 0;
            static float showedDeltaMillis = 0;
#if USE_TIMESTAMP_QUERIES
            static float showedElapsedMicrosGpu = 0;
#endif
#if WITH_PHYSICS
            static float showedElapsedMicrosPhysics = 0;
            static float showedPhysicsSubsteps = 0;
#endif
            ImGui::SetNextWindowBgAlpha(0.5f);
            ImGuiWindowFlags window_flags =
                // ImGuiWindowFlags_NoDecoration |
                // ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav;
            if (ImGui::Begin("Performance", nullptr, window_flags)) {
                if (showCounter == 0) {
                    showedFps = 1.0 / deltaSeconds;
                    showedDeltaMillis = deltaSeconds * 1000.0f;
                    showedElapsedMicrosWaitNextInFlightFrame = frameStats.cpu.elapsedMicrosWaitNextInFlightFrame;
                    showedElapsedMicrosAcquire = frameStats.cpu.elapsedMicrosAcquire;
                    showedElapsedMicrosWaitSwapchainImage = frameStats.cpu.elapsedMicrosWaitSwapchainImage;
                    showedElapsedMicrosRecord = frameStats.cpu.elapsedMicrosRecord;
                    showedElapsedMicrosSubmit = frameStats.cpu.elapsedMicrosSubmit;
                    showedElapsedMicrosPresent = frameStats.cpu.elapsedMicrosPresent;
#if USE_TIMESTAMP_QUERIES
                    showedElapsedMicrosGpu = frameStats.elapsedMicrosGpu;
#endif
#if WITH_PHYSICS
                    showedElapsedMicrosPhysics = frameStats.elapsedMicrosPhysics;
                    {
                        const auto stats = hmlPhysics->getThreadedStats();
                        if (stats) showedPhysicsSubsteps = stats->substeps;
                    }
#endif
                }
                ImGui::Text("FPS = %.0f", showedFps);
                ImGui::Text("Delta = %.1fms", showedDeltaMillis);
                ImGui::Separator();
#if WITH_PHYSICS
                ImGui::Text("CPU Physics = %.0f mks", showedElapsedMicrosPhysics);
                if (hmlPhysics->hasSelfThread()) {
                    ImGui::Text("Physics substeps = %.0f", showedPhysicsSubsteps);
                }
#endif
                ImGui::Text("CPU WaitNextInFlightFrame = %.0f mks", showedElapsedMicrosWaitNextInFlightFrame);
                ImGui::Text("CPU Acquire = %.0f mks", showedElapsedMicrosAcquire);
                ImGui::Text("CPU WaitSwapchainImage = %.0f mks", showedElapsedMicrosWaitSwapchainImage);
                ImGui::Text("CPU Record = %.0f mks", showedElapsedMicrosRecord);
                ImGui::Text("CPU Submit = %.0f mks", showedElapsedMicrosSubmit);
                ImGui::Text("CPU Present = %.0f mks", showedElapsedMicrosPresent);
#if USE_TIMESTAMP_QUERIES
                ImGui::Separator();
                ImGui::Text("GPU time = %.2fms", showedElapsedMicrosGpu / 1000.0f);
#endif
            }
            ImGui::End();

            showCounter++;
            showCounter %= showEvery;
        }
#endif // WITH_IMGUI

        const auto frameResult = hmlDispatcher->doFrame();
        if (!frameResult) {
            std::cerr << "::> Himmel: something bad happened while doing a frame: exiting.\n";
            return false;
        }
        if (frameResult->mustRecreateSwapchain) {
            recreateSwapchain();
            // continue;
        }
        if (frameResult->stats) frameStats.cpu = *(frameResult->stats);


        hmlContext->hmlResourceManager->tickFrame(hmlContext->currentFrame);

#if USE_TIMESTAMP_QUERIES
        hmlContext->hmlQueries->endFrame(hmlContext->currentFrame);
#endif

#if LOG_FPS
        const auto fps = 1.0 / deltaSeconds;
        std::cout << "Delta = " << deltaSeconds * 1000.0f << "ms [FPS = " << fps << "]"
            // << " Update = " << updateMs << "ms"
            << '\n';
#endif

#if USE_TIMESTAMP_QUERIES
        const auto frameStatOpt = hmlContext->hmlQueries->popOldestFrameStat();
#endif

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
                frameStats.elapsedMicrosGpu = (*lastOpt - *firstOpt) / 1000;
            }
        }
#endif // WITH_IMGUI

        hmlContext->currentFrame++;

        hmlContext->hmlWindow->resetScrollState();
    }
    vkDeviceWaitIdle(hmlContext->hmlDevice->device);
    return true;
}


void Himmel::updateForDt(float dt, float sinceStart) noexcept {
    sun.updateForDt(dt);

    { // CoolCube
        auto modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3{0.0f, 60.0f, 60.0f});
        modelMatrix = glm::scale(modelMatrix, glm::vec3(3.0f));
        // modelMatrix = glm::rotate(modelMatrix, glm::radians(20 * sinceStart), glm::vec3(0.0, 1.0, 0.0));

        coolCubeEntity->modelMatrix = modelMatrix;
    }

    { // RustyCube
        auto modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, glm::vec3{20.0f, 62.0f, 60.0f});
        modelMatrix = glm::scale(modelMatrix, glm::vec3(3.0f));
        // modelMatrix = glm::rotate(modelMatrix, glm::radians(20 * sinceStart), glm::vec3(0.0, 1.0, 0.0));

        rustyCubeEntity->modelMatrix = modelMatrix;
    }

    for (size_t i = 0; i < pointLightsDynamic.size(); i++) {
        const float unit = 40.0f;
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

    { // Special dynamic light for PBR testing
        static float t = 0.0f;
        const float speed = 1.0f;
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_J) == GLFW_PRESS) t -= speed;
        if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_K) == GLFW_PRESS) t += speed;

        glm::mat4 model(1.0);
        model = glm::translate(model, glm::vec3(0, 60, 60));
        model = glm::rotate(model, glm::radians(t), glm::vec3(1.0, 0.0, 0.0));
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 10.0f)); // inner radius
        pointLightsDynamic[pointLightsDynamic.size() - 1].position = model[3];
    }

    if (glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        exitRequested = true;
    }

    { // Toggle ambient occlusion
        static bool pressed = false;
        static bool with = true;
        if (!pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_O) == GLFW_PRESS) {
            pressed = true;

            with = !with;
            hmlComplexRenderer->withAO = with;
        } else if (pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_O) == GLFW_RELEASE) {
            pressed = false;
        }
    }
    { // Toggle normal map
        static bool pressed = false;
        static bool with = true;
        if (!pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_N) == GLFW_PRESS) {
            pressed = true;

            with = !with;
            hmlComplexRenderer->withNormals = with;
        } else if (pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_N) == GLFW_RELEASE) {
            pressed = false;
        }
    }
    { // Toggle emissive map
        static bool pressed = false;
        static bool with = true;
        if (!pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_E) == GLFW_PRESS) {
            pressed = true;

            with = !with;
            hmlComplexRenderer->withEmissive = with;
        } else if (pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_E) == GLFW_RELEASE) {
            pressed = false;
        }
    }


    static bool uiInFocus = false;
#if WITH_IMGUI
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
    { // Switch camera
        static bool f2Pressed = false;
        if (!f2Pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_F2) == GLFW_PRESS) {
            f2Pressed = true;

            if (hmlCamera->isFollow()) {
                const auto oldPitch = hmlCamera->pitch;
                const auto oldYaw = hmlCamera->yaw;
                hmlCamera = std::make_unique<HmlCameraFreeFly>(hmlCamera->pos);
                hmlCamera->asFreeFly()->rotateDir(oldPitch, oldYaw);
            } else if (hmlCamera->isFreeFly()) {
                const auto oldPitch = hmlCamera->pitch;
                const auto oldYaw = hmlCamera->yaw;
                hmlCamera = std::make_unique<HmlCameraFollow>(100);
                hmlCamera->asFollow()->rotateDir(oldPitch, oldYaw);
                hmlCamera->asFollow()->target(car);
            } else {
                assert(false && "::> Unhandled HmlCamera type in update().");
            }
        } else if (f2Pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_F2) == GLFW_RELEASE) {
            f2Pressed = false;
        }
    }

#if WITH_PHYSICS
    { // Print HmlPhysics stats
        static bool f3Pressed = false;
        if (!f3Pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_F3) == GLFW_PRESS) {
            f3Pressed = true;

            hmlPhysics->printStats();
        } else if (f3Pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_F3) == GLFW_RELEASE) {
            f3Pressed = false;
        }
    }

    { // Pause physics
        static bool f4Pressed = false;
        if (!f4Pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_F4) == GLFW_PRESS) {
            f4Pressed = true;

            physicsPaused = !physicsPaused;
        } else if (f4Pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_F4) == GLFW_RELEASE) {
            f4Pressed = false;
        }
    }

    { // Step physics
        static bool f5Pressed = false;
        if (!f5Pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_F5) == GLFW_PRESS) {
            f5Pressed = true;

            if (physicsPaused) hmlPhysics->updateForDt(dt);
        } else if (f5Pressed && glfwGetKey(hmlContext->hmlWindow->window, GLFW_KEY_F5) == GLFW_RELEASE) {
            f5Pressed = false;
        }
    }

    { // Set gravity
        const auto key = GLFW_KEY_G;
        static bool pressed = false;
        if (!pressed && glfwGetKey(hmlContext->hmlWindow->window, key) == GLFW_PRESS) {
            pressed = true;

            const float mag = 10.0f;
            hmlPhysics->setGravity(glm::vec3{
                hml::getRandomUniformFloat(-mag, mag),
                hml::getRandomUniformFloat(-mag, mag),
                hml::getRandomUniformFloat(-mag, mag)
            });
        } else if (pressed && glfwGetKey(hmlContext->hmlWindow->window, key) == GLFW_RELEASE) {
            pressed = false;
        }
    }
#endif

    // NOTE allow cursor position update and only disable camera rotation reaction
    // in order not to have the camera jump on re-acquiring control from the ui
    // due to the cursor (pointer) position change that will have happened.
    const bool cameraIgnoreInput = uiInFocus;
    hmlCamera->handleInput(hmlContext->hmlWindow, dt, cameraIgnoreInput);

    // Car movement
    if (!uiInFocus) {
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
        entities.back()->modelMatrix = car->getModelMatrix();
        // NOTE no need to call hmlRenderer->specifyEntitiesToRender(entities)
        // because the Entities are stored as shared_ptr there, which we have
        // just updated.
    }


#if WITH_PHYSICS
    // Upload data from physics to entities
    for (const auto& [id, mm] : hmlPhysics->getModelMatrices()) {
        physicsIdToEntity[id]->modelMatrix = mm;
    }
#endif


#if WITH_IMGUI
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
            auto& pos = hmlCamera->pos;
            auto& pitch = hmlCamera->pitch;
            auto& yaw = hmlCamera->yaw;

            const float minPos = -1000.0f;
            const float maxPos = 1000.0f;
            const float minYaw = 0.0f;
            const float maxYaw = 360.0f;
            const float minPitch = -90.0f;
            const float maxPitch = 90.0f;

            if      (hmlCamera->isFreeFly()) ImGui::Text("Camera: FreeFly");
            else if (hmlCamera->isFollow())  ImGui::Text("Camera: Follow");
            ImGui::Spacing();
            ImGui::Text("[%.2f;%.2f;%.2f] p=%.1f y=%.1f", pos.x, pos.y, pos.z, pitch, yaw);
            if (hmlCamera->isFreeFly()) {
                ImGui::DragFloat3("pos", &pos[0], 0.8f, minPos, maxPos);
            }
            ImGui::DragScalar("pitch", ImGuiDataType_Float, &pitch, 0.3f, &minPitch, &maxPitch, "%f");
            ImGui::DragScalar("yaw", ImGuiDataType_Float, &yaw, 0.7f, &minYaw, &maxYaw, "%f");

            static auto cachedPos = pos;
            static auto cachedPitch = pitch;
            static auto cachedYaw = yaw;
            if (pos != cachedPos || pitch != cachedPitch || yaw != cachedYaw) {
                if (hmlCamera->isFreeFly()) hmlCamera->asFreeFly()->invalidateCache();
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

    hmlContext->hmlImgui->updateForDt(dt);
#endif // WITH_IMGUI

#if WITH_PHYSICS
    {
        const auto startTime = std::chrono::high_resolution_clock::now();
        if (!physicsPaused) hmlPhysics->updateForDt(dt);
        const auto newMark = std::chrono::high_resolution_clock::now();
        const auto sinceStart = std::chrono::duration_cast<std::chrono::microseconds>(newMark - startTime).count();
        const float sinceStartMks = static_cast<float>(sinceStart);
        frameStats.elapsedMicrosPhysics = sinceStartMks;
    }
#endif


#if SNOW_IS_ON
    hmlSnowRenderer->updateForDt(dt, sinceStart);
#endif
    {
        // NOTE Takes up to 60mks with (current) CPU-patch implementation
        // const auto startTime = std::chrono::high_resolution_clock::now();
        hmlTerrainRenderer->update(hmlCamera->pos);
        // const auto newMark = std::chrono::high_resolution_clock::now();
        // const auto sinceStart = std::chrono::duration_cast<std::chrono::microseconds>(newMark - startTime).count();
        // const float sinceStartMksSeconds = static_cast<float>(sinceStart);
        // std::cout << "UPD = " << sinceStartMksSeconds << '\n';
    }
}


void Himmel::updateForImage(uint32_t imageIndex) noexcept {
    // NOTE Do a direct access (rather than download the whole buffer) because
    // we are only interested in one value.
    static uint32_t prev = 0;
    auto [cursor_x, cursor_y] = hmlContext->hmlWindow->getCursor();
    const uint32_t width = hmlContext->hmlSwapchain->extent().width;
    const uint32_t height = hmlContext->hmlSwapchain->extent().height;
    cursor_x = std::clamp(uint32_t(cursor_x), 0u, width);
    cursor_y = std::clamp(uint32_t(cursor_y), 0u, height);
    // const uint32_t idValue = static_cast<uint32_t*>(idsBuffer->mappedPtr)[cursor_x * height + cursor_y];
    const uint32_t idValue = static_cast<uint32_t*>(idsBuffer->mappedPtr)[cursor_y * width + cursor_x];
    if (idValue != prev) {
        std::cout << "Cursor is at object with id = " << idValue << '\n';
    }
    prev = idValue;

    {
        // NOTE cannot use hmlCamera.somethingHasChanged because we have multiple UBOs (per image)
        // const auto globalLightDir = glm::normalize(glm::vec3(0.5f, -1.0f, -1.0f)); // NOTE probably unused
        // const auto DST = 4.0f;
        // const auto globalLightPos = glm::vec3(DST * world->start.x, DST * world->height, DST * world->finish.y);
        // const auto globalLightView = glm::lookAt(globalLightPos, globalLightPos + globalLightDir, glm::vec3{0, 1, 0});
        static float near = 300.0f;
        static float far = 1100.0f;
#if WITH_IMGUI
        const float minDepth = 0.1f;
        const float maxDepth = 1200.0f;
#endif

#define FROM_SUN 1
#if FROM_SUN // from Sun position
        const auto globalLightPos = sun.calculatePosNorm() * sun.shadowCalcRadius;
#else // manually
        static float yaw = 25.0f;
        static float pitch = 30.0f;
        const float minYaw = 0.0f;
        const float maxYaw = 360.0f;
        const float minPitch = 0.0f;
        const float maxPitch = 90.0f;
        const auto globalLightPos = HmlCamera::calcDirForward(pitch, yaw) * sun.shadowCalcRadius;
#endif
        const auto globalLightView = glm::lookAt(globalLightPos, glm::vec3{0,0,0}, glm::vec3{0, 1, 0});
        const auto globalLightProj = [&](){
            // NOTE Width and height are calculated in such a way as to contain
            // without cut-offs the biggest object in the scene, which happens
            // to be the World.
            static const float sqrt2 = std::sqrt(2.0f);
            const float width = world->finish.x;
            const float height = world->finish.y;
            // NOTE should theoretically produce better result for small pitch values,
            // however in practice doesn't give noticable benefit to justify more
            // complex calculation.
            // NOTE However, this complex calculation results in height being more
            // precise than width, which makes the benefit pretty useless...
            // const float tallestObjectHeight = (50.0f + 75.0f); // girl
            // const float tallestObjectHeight = world->height; // skrew the girl, care only about the World
            // const float height = std::clamp(world->finish.y * std::sqrt(1.0f * pitch / maxPitch), tallestObjectHeight / sqrt2, 11111.1f);

            const float f = sqrt2;
            glm::mat4 proj = glm::ortho(-width*f, width*f, -height*f, height*f, near, far);
            // glm::mat4 proj = glm::perspective(glm::radians(45.0f), hmlContext->hmlSwapchain->extentAspect(), near, far);
            proj[1][1] *= -1; // fix the inverted Y axis of GLM
            return proj;
        }();
        // const auto globalLightDir = HmlCamera::calcDirForward(-pitch, yaw + 180.0f);
        const auto globalLightDir = -glm::normalize(globalLightPos);
        GeneralUbo generalUbo{
            .view = hmlCamera->view(),
            .proj = proj,
            // .globalLightView = hmlCamera->view(),
            .globalLightView = globalLightView,
            .globalLightProj = globalLightProj,
            .globalLightDir = globalLightDir,
            .ambientStrength = 0.5f,
            .fogColor = weather.fogColor,
            .fogDensity = weather.fogDensity,
            .cameraPos = hmlCamera->pos,
            .dayNightCycleT = sun.regularT()
        };
        viewProjUniformBuffers[imageIndex]->update(&generalUbo);
#if WITH_IMGUI
        ImGuiWindowFlags window_flags =
            // ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;
        ImGui::Begin("Shadowmap", nullptr, window_flags);
        ImGui::DragScalar("near", ImGuiDataType_Float, &near, 0.1f, &minDepth, &maxDepth, "%f");
        ImGui::DragScalar("far", ImGuiDataType_Float, &far, 1.0f, &minDepth, &maxDepth, "%f");
#if !FROM_SUN
        ImGui::DragScalar("yaw", ImGuiDataType_Float, &yaw, 0.5f, &minYaw, &maxYaw, "%f");
        ImGui::DragScalar("pitch", ImGuiDataType_Float, &pitch, 0.3f, &minPitch, &maxPitch, "%f");
#endif
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
                [cameraPos = hmlCamera->pos](const auto& l1, const auto& l2){
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


bool Himmel::prepareResources() noexcept {
    // static const auto viewsFrom = [](const std::vector<std::shared_ptr<HmlImageResource>>& resources){
    //     std::vector<VkImageView> views;
    //     for (const auto& res : resources) views.push_back(res->view);
    //     return views;
    // };

    const auto extent = hmlContext->hmlSwapchain->extent();
    const size_t count = hmlContext->imageCount();
    gBufferPositions.resize(count);
    gBufferNormals.resize(count);
    gBufferColors.resize(count);
    gBufferLightSpacePositions.resize(count);
    gBufferIds.resize(count);
    gBufferMaterials.resize(count);
    blurTempTextures.resize(count);
    brightness1Textures.resize(count);
    mainTextures.resize(count);
    hmlDepthResources.resize(count);
    hmlShadows.resize(count);
    for (size_t i = 0; i < count; i++) {
        // XXX TODO use modified findDepthFormat()
        gBufferPositions[i]           = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R16G16B16A16_SFLOAT);
        // gBufferPositions[i]           = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R32G32B32A32_SFLOAT);
        gBufferNormals[i]             = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R16G16B16A16_SFLOAT);
        gBufferColors[i]              = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R8G8B8A8_SRGB);
        // gBufferLightSpacePositions[i] = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R32G32B32A32_SFLOAT);
        gBufferLightSpacePositions[i] = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R16G16B16A16_SFLOAT);
        gBufferIds[i]                 = hmlContext->hmlResourceManager->newReadableRenderable(extent, VK_FORMAT_R32_UINT);
        gBufferMaterials[i]           = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R8G8B8A8_UNORM);
        blurTempTextures[i]           = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R8G8B8A8_SRGB);
        brightness1Textures[i]        = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R8G8B8A8_SRGB);
        mainTextures[i]               = hmlContext->hmlResourceManager->newRenderTargetImageResource(extent, VK_FORMAT_R8G8B8A8_SRGB);
        hmlDepthResources[i]          = hmlContext->hmlResourceManager->newDepthResource(extent);
        hmlShadows[i]                 = hmlContext->hmlResourceManager->newShadowResource(shadowmapExtent, VK_FORMAT_D32_SFLOAT);
        if (!gBufferPositions[i])           return false;
        if (!gBufferNormals[i])             return false;
        if (!gBufferColors[i])              return false;
        if (!gBufferLightSpacePositions[i]) return false;
        if (!gBufferIds[i])                 return false;
        if (!gBufferMaterials[i])           return false;
        if (!blurTempTextures[i])           return false;
        if (!brightness1Textures[i])        return false;
        if (!mainTextures[i])               return false;
        if (!hmlDepthResources[i])          return false;
        if (!hmlShadows[i])                 return false;
    }

    size_t idsBufferSizeBytes;
    switch (gBufferIds[0]->format) {
        case VK_FORMAT_R32_UINT: idsBufferSizeBytes = gBufferIds[0]->width * gBufferIds[0]->height * 32; break;
        case VK_FORMAT_R16_UINT: idsBufferSizeBytes = gBufferIds[0]->width * gBufferIds[0]->height * 16; break;
        default: std::cerr << "::> Unimplemented format for idsBuffer. Provide size calculation.\n"; return false;
    }
    idsBuffer = hmlContext->hmlResourceManager->createStagingBufferToHost(idsBufferSizeBytes);
    idsBuffer->map();


    hmlDeferredRenderer->specify({ gBufferPositions, gBufferNormals, gBufferColors, gBufferLightSpacePositions, hmlShadows, gBufferMaterials });
    hmlUiRenderer->specify({ gBufferPositions, gBufferNormals, gBufferColors, gBufferLightSpacePositions, hmlShadows });
    hmlBlurRenderer->specify(brightness1Textures, blurTempTextures);
    hmlBloomRenderer->specify(mainTextures, brightness1Textures);


    hmlDispatcher = std::make_unique<HmlDispatcher>(hmlContext, generalDescriptorSet_0_perImage, [&](uint32_t imageIndex){ updateForImage(imageIndex); });

    // Perform initial layout transitions to set up the layouts as they would
    // have been before starting the next iteration in an ongoing looping.
    {
        VkCommandBuffer commandBuffer = hmlContext->hmlCommands->beginSingleTimeCommands();

        // NOTE oldLayout is correct for the moment because the resources have just been created with correct layouts specified
        for (auto res : hmlShadows)                 res->transitionLayoutTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        for (auto res : mainTextures)               res->transitionLayoutTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        for (auto res : blurTempTextures)           res->transitionLayoutTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        for (auto res : brightness1Textures)        res->transitionLayoutTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        for (auto res : gBufferPositions)           res->transitionLayoutTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        for (auto res : gBufferColors)              res->transitionLayoutTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        for (auto res : gBufferNormals)             res->transitionLayoutTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        for (auto res : gBufferLightSpacePositions) res->transitionLayoutTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        // for (auto res : gBufferIds)                 res->transitionLayoutTo(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, commandBuffer); // no need because already created in this layout
        for (auto res : gBufferMaterials)           res->transitionLayoutTo(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);
        for (auto res : hmlContext->hmlSwapchain->imageResources) res->transitionLayoutTo(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, commandBuffer);

        hmlContext->hmlCommands->endSingleTimeCommands(commandBuffer);
    }

    static const auto STAGE_NAME_SHADOW_PASS   = "Shadow pass";
    static const auto STAGE_NAME_GEOMETRY_PASS = "Geometry pass";
    static const auto STAGE_NAME_DEFERRED_PASS = "Deferred pass";
    static const auto STAGE_NAME_AFTER_PASS    = "After pass";
    static const auto STAGE_NAME_BLUR_PASS     = "Blur pass";
    static const auto STAGE_NAME_BLOOM_PASS    = "Bloom pass";
    static const auto STAGE_NAME_UI_PASS       = "UI pass";
    const float VERY_FAR = 10000.0f;
    const float DEFAULT_METALLIC = 0.0f;
    const float DEFAULT_ROUGHNESS = 1.0f;
    bool allGood = true;
    // TODO Use a Builder pattern to create the Dispatcher and seal it at the end,
    // thus allowing it to check itself for correctness.
    // ================================= PASS =================================
    // ======== Render shadow-casting geometry into shadow map ========
    allGood &= hmlDispatcher->addStage(HmlDispatcher::StageCreateInfo{
        .name = STAGE_NAME_SHADOW_PASS,
        .preFunc = [&](HmlDispatcher& dispatcher, bool prepPhase, const HmlFrameData& frameData){
            hmlTerrainRenderer->setMode(HmlTerrainRenderer::Mode::Shadowmap);
            hmlRenderer->setMode(HmlRenderer::Mode::Shadowmap);
        },
        .drawers = { hmlTerrainRenderer, hmlRenderer },
        .differentExtent = { shadowmapExtent },
        .colorAttachments = {},
        .depthAttachment = {
            HmlRenderPass::DepthStencilAttachment{
                .imageResources = hmlShadows,
                .clearColor = VkClearDepthStencilValue{ 1.0f, 0 }, // 1.0 is farthest
                .store = true,
                .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            }
        },
        .subpassDependencies = {},
        .postFunc = std::nullopt,
        .flags = HmlDispatcher::STAGE_NO_FLAGS,
    });
    // ================================= PASS =================================
    // ======== Renders main geometry into the GBuffer ========
    allGood &= hmlDispatcher->addStage(HmlDispatcher::StageCreateInfo{
        .name = STAGE_NAME_GEOMETRY_PASS,
        .preFunc = [&](HmlDispatcher& dispatcher, bool prepPhase, const HmlFrameData& frameData){
            hmlTerrainRenderer->setMode(HmlTerrainRenderer::Mode::Regular);
            hmlRenderer->setMode(HmlRenderer::Mode::Regular);

            if (!prepPhase) {
                // const auto startWait = std::chrono::high_resolution_clock::now();
                const auto fence = dispatcher.getFenceForStageFinish(STAGE_NAME_GEOMETRY_PASS);
                assert(fence && "Fence signal for stage is not set up");
                vkWaitForFences(hmlContext->hmlDevice->device, 1, &(*fence), VK_TRUE, UINT64_MAX);
                // const auto endWait = std::chrono::high_resolution_clock::now();
                // const auto elapsedMicros = static_cast<float>(std::chrono::duration_cast
                //     <std::chrono::microseconds>(endWait - startWait).count()) / 1.0f;
                // std::cout << "Wait for Fence for cursor data from previous frame took " << elapsedMicros << "mks.\n";
                // TODO dispatcher.registerStats("Waiting for stage from previous frame", begin, end);
            }
        },
        .drawers = { hmlTerrainRenderer, hmlRenderer, hmlComplexRenderer },
        .differentExtent = std::nullopt,
        .colorAttachments = {
            HmlRenderPass::ColorAttachment{
                .imageResources = gBufferPositions,
                .loadColor = HmlRenderPass::LoadColor::Clear({{ VERY_FAR, VERY_FAR, VERY_FAR, 1.0f }}),
                .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            HmlRenderPass::ColorAttachment{
                .imageResources = gBufferNormals,
                .loadColor = HmlRenderPass::LoadColor::Clear({{ 0.0f, 0.0f, 0.0f, 1.0f }}),
                .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            HmlRenderPass::ColorAttachment{
                .imageResources = gBufferColors,
                .loadColor = HmlRenderPass::LoadColor::Clear({{ weather.fogColor.x, weather.fogColor.y, weather.fogColor.z, 1.0f }}),
                .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            HmlRenderPass::ColorAttachment{
                .imageResources = gBufferLightSpacePositions,
                .loadColor = HmlRenderPass::LoadColor::Clear({{ 2.0f, 2.0f, 2.0f, 2.0f }}), // TODO i dunno!
                .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            HmlRenderPass::ColorAttachment{
                .imageResources = gBufferIds,
                .loadColor = HmlRenderPass::LoadColor::Clear({{ 0 }}),
                .preLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            },
            HmlRenderPass::ColorAttachment{
                .imageResources = brightness1Textures,
                .loadColor = HmlRenderPass::LoadColor::Clear({{ 0.0f, 0.0f, 0.0f, 0.0f }}),
                .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            HmlRenderPass::ColorAttachment{
                .imageResources = gBufferMaterials,
                .loadColor = HmlRenderPass::LoadColor::Clear({{ DEFAULT_METALLIC, DEFAULT_ROUGHNESS }}),
                .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
        },
        .depthAttachment = {
            HmlRenderPass::DepthStencilAttachment{
                .imageResources = hmlDepthResources,
                .clearColor = VkClearDepthStencilValue{ 1.0f, 0 }, // 1.0 is farthest
                .store = true,
                .preLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }
        },
        .subpassDependencies = {
            VkSubpassDependency{
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .dependencyFlags = 0,
            }
        },
        .postFunc = [&](bool prepPhase, const HmlFrameData& frameData, VkCommandBuffer commandBuffer){
            if (!prepPhase) {
                // Transition layout and specify a dependency between output finish and transfer start
                auto& imageResource = gBufferIds[frameData.swapchainImageIndex];
                // NOTE I think we achieve the same thing via subpass dependency
                // imageResource->barrier(commandBuffer,
                //     VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                //     VK_ACCESS_TRANSFER_READ_BIT,
                //     VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                //     VK_PIPELINE_STAGE_TRANSFER_BIT);

                // Perform the actual transfer
                VkExtent2D extent{
                    .width = imageResource->width,
                    .height = imageResource->height,
                };
                hmlContext->hmlResourceManager->copyImageToBuffer(imageResource->image, idsBuffer->buffer, extent, commandBuffer);
            }
        },
        .flags = HmlDispatcher::STAGE_FLAG_SIGNAL_WHEN_DONE,
        // .flags = HmlDispatcher::STAGE_NO_FLAGS,
    });
    // ================================= PASS =================================
    // ======== Renders a 2D texture using the GBuffer ========
    allGood &= hmlDispatcher->addStage(HmlDispatcher::StageCreateInfo{
        .name = STAGE_NAME_DEFERRED_PASS,
        .preFunc = std::nullopt,
        .drawers = { hmlDeferredRenderer },
        .differentExtent = std::nullopt,
        .colorAttachments = {
            HmlRenderPass::ColorAttachment{
                .imageResources = mainTextures,
                .loadColor = HmlRenderPass::LoadColor::DontCare(),
                .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        },
        .depthAttachment = std::nullopt,
        .subpassDependencies = {
            VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                // Wait for all these to finish:
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT // gbuffer color outputs
                              | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, // shadowmap depth outputs
                // ... and only after that allow us to:
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                               | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT, // read textures in FS
                            //    | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // XXX not allowed for FS! perform a clearing loadOp
                .dependencyFlags = 0,
            }
        },
        .postFunc = std::nullopt,
        .flags = HmlDispatcher::STAGE_NO_FLAGS,
    });
    // ================================= PASS =================================
    // ======== Renders on top of the deferred texture using the depth info from the prep pass ========
    // Input: ---
    allGood &= hmlDispatcher->addStage(HmlDispatcher::StageCreateInfo{
        .name = STAGE_NAME_AFTER_PASS,
        .preFunc = [&](HmlDispatcher& dispatcher, bool prepPhase, const HmlFrameData& frameData){
#if DEBUG_TERRAIN
            hmlTerrainRenderer->setMode(HmlTerrainRenderer::Mode::Debug);
#endif
        },
        .drawers = {
#if SNOW_IS_ON
            hmlSnowRenderer,
#endif
            hmlLightRenderer
#if DEBUG_TERRAIN
            , hmlTerrainRenderer
#endif
        },
        .differentExtent = std::nullopt,
        .colorAttachments = {
            HmlRenderPass::ColorAttachment{
                .imageResources = mainTextures,
                .loadColor = HmlRenderPass::LoadColor::Load(),
                .preLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            HmlRenderPass::ColorAttachment{
                .imageResources = brightness1Textures,
                .loadColor = HmlRenderPass::LoadColor::Load(),
                // .loadColor = HmlRenderPass::LoadColor::Clear({{ 0.0f, 0.0f, 0.0f, 1.0f }}),
                .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
        },
        .depthAttachment = {
            HmlRenderPass::DepthStencilAttachment{
                .imageResources = hmlDepthResources,
                .clearColor = std::nullopt,
                .store = false,
                .preLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }
        },
        .subpassDependencies = {
            VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                // Wait for all these to finish:
                // NOTE we don't specify the depth because it has been done by the start of the previous stage
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,// deferred mainTexture output
                // ... and only after that allow us to:
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                              | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, // for the depth read
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT // read textures in FS
                            //    | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT // perform a clearing loadOp
                               | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT, // read and compare depth
                .dependencyFlags = 0,
            }
        },
        .postFunc = std::nullopt,
        .flags = HmlDispatcher::STAGE_NO_FLAGS,
    });
    // ================================= PASS =================================
    // ======== Blurs the brightness1Textures texture (horizontal pass)
    // Input: brightness1Textures
    allGood &= hmlDispatcher->addStage(HmlDispatcher::StageCreateInfo{
        .name = STAGE_NAME_BLUR_PASS,
        .preFunc = [&](HmlDispatcher& dispatcher, bool prepPhase, const HmlFrameData& frameData){
            hmlBlurRenderer->modeHorizontal();
        },
        .drawers = { hmlBlurRenderer },
        .differentExtent = std::nullopt,
        .colorAttachments = {
            HmlRenderPass::ColorAttachment{
                .imageResources = blurTempTextures,
                .loadColor = HmlRenderPass::LoadColor::DontCare(),
                .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
        },
        .depthAttachment = std::nullopt,
        .subpassDependencies = {
            VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                // Wait for all these to finish:
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // output to blurTempTextures
                // ... and only after that allow us to:
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT, // read blurTempTextures in FS
                .dependencyFlags = 0,
            }
        },
        .postFunc = std::nullopt,
        .flags = HmlDispatcher::STAGE_NO_FLAGS,
    });
    // ================================= PASS =================================
    // ======== Blurs the brightness1Textures texture (vertical pass)
    // Input: blurTempTextures
    allGood &= hmlDispatcher->addStage(HmlDispatcher::StageCreateInfo{
        .name = STAGE_NAME_BLUR_PASS,
        .preFunc = [&](HmlDispatcher& dispatcher, bool prepPhase, const HmlFrameData& frameData){
            hmlBlurRenderer->modeVertical();
        },
        .drawers = { hmlBlurRenderer },
        .differentExtent = std::nullopt,
        .colorAttachments = {
            HmlRenderPass::ColorAttachment{
                .imageResources = brightness1Textures,
                .loadColor = HmlRenderPass::LoadColor::DontCare(),
                .preLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
        },
        .depthAttachment = std::nullopt,
        .subpassDependencies = {
            VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                // Wait for all these to finish:
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // output to blurTempTextures
                // ... and only after that allow us to:
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT, // read blurTempTextures in FS
                .dependencyFlags = 0,
            }
        },
        .postFunc = std::nullopt,
        .flags = HmlDispatcher::STAGE_NO_FLAGS,
    });
    // ================================= PASS =================================
    // TODO write description
    // Input: mainTexture, brightness1Textures
    allGood &= hmlDispatcher->addStage(HmlDispatcher::StageCreateInfo{
        .name = STAGE_NAME_BLOOM_PASS,
        .preFunc = std::nullopt,
        .drawers = { hmlBloomRenderer },
        .differentExtent = std::nullopt,
        .colorAttachments = {
            HmlRenderPass::ColorAttachment{
                .imageResources = hmlContext->hmlSwapchain->imageResources,
                .loadColor = HmlRenderPass::LoadColor::DontCare(),
                .preLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                .postLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        },
        .depthAttachment = std::nullopt,
        .subpassDependencies = {
            VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                // Wait for all these to finish:
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // mainTexture, brightness1Textures (and swapchain image) output
                // ... and only after that allow us to:
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT, // read textures in FS
                            //    | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // perform a clearing loadOp
                .dependencyFlags = 0,
            }
        },
        .postFunc = std::nullopt,
        .flags = HmlDispatcher::STAGE_FLAG_FIRST_USAGE_OF_SWAPCHAIN_IMAGE,
    });
    // ================================= PASS =================================
    // ======== Renders multiple 2D textures on top of everything ========
    // Input: shadowmap, all gBuffers
    allGood &= hmlDispatcher->addStage(HmlDispatcher::StageCreateInfo{
        .name = STAGE_NAME_UI_PASS,
        .preFunc = [&](HmlDispatcher& dispatcher, bool prepPhase, const HmlFrameData& frameData){
#if WITH_IMGUI
            if (!prepPhase) hmlContext->hmlImgui->finilize(frameData.currentFrameIndex, frameData.frameInFlightIndex);
#endif
        },
        .drawers = {
            hmlUiRenderer
#if WITH_IMGUI
            , hmlImguiRenderer
#endif
        },
        .differentExtent = std::nullopt,
        .colorAttachments = {
            HmlRenderPass::ColorAttachment{
                .imageResources = hmlContext->hmlSwapchain->imageResources,
                .loadColor = HmlRenderPass::LoadColor::Load(),
                .preLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .postLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
        },
        .depthAttachment = std::nullopt,
        .subpassDependencies = {
            VkSubpassDependency{
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                // Wait for all these to finish:
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // into swapchain image
                // ... and only after that allow us to:
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_SHADER_READ_BIT, // read textures in FS
                            //    | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = 0,
            }
        },
        .postFunc = std::nullopt,
        .flags = HmlDispatcher::STAGE_FLAG_LAST_USAGE_OF_SWAPCHAIN_IMAGE,
    });

    if (!allGood) {
        std::cerr << "::> Could not create some stages in HmlDispatcher.\n";
        return false;
    }

    return true;
}


void Himmel::recreateSwapchain() noexcept {
#if LOG_INFO
    std::cout << ":> Recreating the Swapchain.\n";
#endif

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


Himmel::~Himmel() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying Himmel...\n";
#endif
    // vkDeviceWaitIdle(hmlDevice->device);

    if (!successfulInit) {
        std::cerr << "::> Himmel init has not finished successfully, so no proper cleanup is done.\n";
        return;
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
