#include "Himmel.h"


bool Himmel::init() noexcept {
    const char* windowName = "Planes game";

    hmlWindow = HmlWindow::create(1920, 1080, windowName);
    if (!hmlWindow) return false;

    hmlDevice = HmlDevice::create(hmlWindow);
    if (!hmlDevice) return false;

    hmlDescriptors = HmlDescriptors::create(hmlDevice);
    if (!hmlDescriptors) return false;

    hmlCommands = HmlCommands::create(hmlDevice);
    if (!hmlCommands) return false;

    hmlResourceManager = HmlResourceManager::create(hmlDevice, hmlCommands);
    if (!hmlResourceManager) return false;

    hmlSwapchain = HmlSwapchain::create(hmlWindow, hmlDevice, hmlResourceManager, std::nullopt);
    if (!hmlSwapchain) return false;


    commandBuffers = hmlCommands->allocatePrimary(hmlSwapchain->imageCount(), hmlCommands->commandPoolOnetimeFrames);
    commandBuffers2 = hmlCommands->allocatePrimary(hmlSwapchain->imageCount(), hmlCommands->commandPoolOnetimeFrames);


    for (size_t i = 0; i < hmlSwapchain->imageCount(); i++) {
        const auto size = sizeof(GeneralUbo);
        auto ubo = hmlResourceManager->createUniformBuffer(size);
        ubo->map();
        viewProjUniformBuffers.push_back(std::move(ubo));
    }
    for (size_t i = 0; i < hmlSwapchain->imageCount(); i++) {
        const auto size = sizeof(LightUbo);
        auto ubo = hmlResourceManager->createUniformBuffer(size);
        ubo->map();
        lightUniformBuffers.push_back(std::move(ubo));
    }


    generalDescriptorPool = hmlDescriptors->buildDescriptorPool()
        .withUniformBuffers(hmlSwapchain->imageCount() + hmlSwapchain->imageCount()) // GeneralUbo + LightUbo
        .maxSets(hmlSwapchain->imageCount()) // they are in the same set
        .build(hmlDevice);
    if (!generalDescriptorPool) return false;

    generalDescriptorSetLayout = hmlDescriptors->buildDescriptorSetLayout()
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
        .build(hmlDevice);
    if (!generalDescriptorSetLayout) return false;

    generalDescriptorSet_0_perImage = hmlDescriptors->createDescriptorSets(
        hmlSwapchain->imageCount(), generalDescriptorSetLayout, generalDescriptorPool);
    if (generalDescriptorSet_0_perImage.empty()) return false;
    for (size_t imageIndex = 0; imageIndex < hmlSwapchain->imageCount(); imageIndex++) {
        HmlDescriptorSetUpdater(generalDescriptorSet_0_perImage[imageIndex])
            .uniformBufferAt(0,
                viewProjUniformBuffers[imageIndex]->buffer,
                sizeof(GeneralUbo))
            .uniformBufferAt(1,
                lightUniformBuffers[imageIndex]->buffer,
                sizeof(LightUbo))
            .update(hmlDevice);
    }


    weather = Weather{
        // .fogColor = glm::vec3(0.0, 0.0, 0.0),
        .fogColor = glm::vec3(0.8f, 0.8f, 0.8f),
        .fogDensity = -1.0f,
        // .fogDensity = 0.011,
    };


    // General RenderPass
    hmlDepthResource = hmlResourceManager->newDepthResource(hmlSwapchain->extent);
    if (!hmlDepthResource) return false;
    hmlRenderPassGeneral = createGeneralRenderPass(hmlSwapchain, hmlDepthResource, weather);
    if (!hmlRenderPassGeneral) return false;
    // UI RenderPass
    hmlRenderPassUi = createUiRenderPass(hmlSwapchain);
    if (!hmlRenderPassUi) return false;


    hmlRenderer = HmlRenderer::create(hmlWindow, hmlDevice, hmlCommands,
        hmlRenderPassGeneral, hmlResourceManager, hmlDescriptors, generalDescriptorSetLayout, maxFramesInFlight);
    if (!hmlRenderer) return false;

    hmlUiRenderer = HmlUiRenderer::create(hmlWindow, hmlDevice, hmlCommands,
        hmlRenderPassUi, hmlResourceManager, hmlDescriptors, maxFramesInFlight);
    if (!hmlUiRenderer) return false;


    const auto SIZE_MAP = 250.0f;
    const auto SIZE_SNOW = 70.0f;
    const auto snowCount = 400000;
    const auto HEIGHT_MAP = 70.0f;
    const HmlSnowParticleRenderer::SnowBounds snowBounds = HmlSnowParticleRenderer::SnowCameraBounds{ SIZE_SNOW };
    hmlSnowRenderer = HmlSnowParticleRenderer::createSnowRenderer(snowCount, snowBounds, hmlWindow,
        hmlDevice, hmlCommands, hmlRenderPassGeneral, hmlResourceManager, hmlDescriptors, generalDescriptorSetLayout, maxFramesInFlight);
    if (!hmlSnowRenderer) return false;


    const auto terrainBounds = HmlTerrainRenderer::Bounds{
        .posStart = glm::vec2(-SIZE_MAP, -SIZE_MAP),
        .posFinish = glm::vec2(SIZE_MAP, SIZE_MAP),
        .height = HEIGHT_MAP,
        .yOffset = 0.0f,
    };
    const uint32_t granularity = 4;
    hmlTerrainRenderer = HmlTerrainRenderer::create("models/heightmap.png", granularity, "models/grass-small.png",
        terrainBounds, generalDescriptorSet_0_perImage, hmlWindow,
        hmlDevice, hmlCommands, hmlRenderPassGeneral, hmlResourceManager, hmlDescriptors, generalDescriptorSetLayout, maxFramesInFlight);
    if (!hmlTerrainRenderer) return false;


    // Add lights
    for (size_t i = 0; i < 20; i++) {
        const auto pos = glm::vec3(
            hml::getRandomUniformFloat(-SIZE_MAP, SIZE_MAP),
            hml::getRandomUniformFloat(HEIGHT_MAP/2.0f, HEIGHT_MAP),
            hml::getRandomUniformFloat(-SIZE_MAP, SIZE_MAP)
        );
        const auto color = glm::vec3(
            hml::getRandomUniformFloat(0.0f, 1.0f),
            hml::getRandomUniformFloat(0.0f, 1.0f),
            hml::getRandomUniformFloat(0.0f, 1.0f)
        );
        pointLights.push_back(PointLight{
            .color = color,
            .intensity = 3000.0f,
            .position = pos,
            .reserved = 0.0f,
        });
    }
    {
        const float unit = 40.0f;
        pointLights.push_back(PointLight{
            .color = glm::vec3(1.0, 0.0, 0.0),
            .intensity = 5000.0f,
            .position = glm::vec3(0.0, unit, 0.0),
            .reserved = 0.0f,
        });
        pointLights.push_back(PointLight{
            .color = glm::vec3(0.0, 1.0, 0.0),
            .intensity = 3000.0f,
            .position = glm::vec3(3*unit, unit, 0.0),
            .reserved = 0.0f,
        });
        pointLights.push_back(PointLight{
            .color = glm::vec3(0.0, 0.0, 1.0),
            .intensity = 3800.0f,
            .position = glm::vec3(-2*unit, unit, -3*unit),
            .reserved = 0.0f,
        });
    }


    camera = HmlCamera{{ 25.0f, 30.0f, 35.0f }};
    camera.rotateDir(-15.0f, -40.0f);
    cursor = hmlWindow->getCursor();
    proj = projFrom(hmlSwapchain->extentAspect());


    {
        // {
        //     std::vector<HmlSimpleModel::Vertex> vertices;
        //     vertices.push_back(HmlSimpleModel::Vertex{
        //         .pos = {-0.5f, -0.5f, 0.0f},
        //         .texCoord = {1.0f, 0.0f},
        //         .normal = {0.0f, 0.0f, 1.0f},
        //     });
        //     vertices.push_back(HmlSimpleModel::Vertex{
        //         .pos = {0.5f, -0.5f, 0.0f},
        //         .texCoord = {0.0f, 0.0f},
        //         .normal = {0.0f, 0.0f, 1.0f},
        //     });
        //     vertices.push_back(HmlSimpleModel::Vertex{
        //         .pos = {0.5f, 0.5f, 0.0f},
        //         .texCoord = {0.0f, 1.0f},
        //         .normal = {0.0f, 0.0f, 1.0f},
        //     });
        //     vertices.push_back(HmlSimpleModel::Vertex{
        //         .pos = {-0.5f, 0.5f, 0.0f},
        //         .texCoord = {1.0f, 1.0f},
        //         .normal = {0.0f, 0.0f, 1.0f},
        //     });
        //
        //     std::vector<uint32_t> indices = { 0, 1, 2, 2, 3, 0 };
        //
        //     const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
        //     const auto model = hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
        //     models.push_back(model);
        // }

        {
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
            const auto model = hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices, "models/girl.png", VK_FILTER_LINEAR);
            models.push_back(model);
        }

        {
            std::vector<HmlSimpleModel::Vertex> vertices;
            std::vector<uint32_t> indices;
            if (!HmlSimpleModel::load("models/viking_room.obj", vertices, indices)) return false;

            const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
            // const auto model = hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices, "models/viking_room.png", VK_FILTER_LINEAR);
            const auto model = hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
            models.push_back(model);
        }

        {
            std::vector<HmlSimpleModel::Vertex> vertices;
            std::vector<uint32_t> indices;
            if (!HmlSimpleModel::load("models/plane.obj", vertices, indices)) return false;

            const auto verticesSizeBytes = sizeof(vertices[0]) * vertices.size();
            const auto model = hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
            models.push_back(model);
        }

        const auto& phonyModel = models[0];
        const auto& vikingModel = models[1];
        const auto& planeModel = models[2];


        // Add Entities

        // PHONY with a texture
        entities.push_back(std::make_shared<HmlRenderer::Entity>(phonyModel));
        entities.back()->modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3{0.0f, -50.0f, 0.0f});

        entities.push_back(std::make_shared<HmlRenderer::Entity>(planeModel, glm::vec3{ 1.0f, 1.0f, 1.0f }));
        entities.back()->modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3{0.0f, 25.0f, 30.0f});

        entities.push_back(std::make_shared<HmlRenderer::Entity>(planeModel, glm::vec3{ 0.8f, 0.2f, 0.5f }));
        entities.back()->modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3{0.0f, 35.0f, 60.0f});

        entities.push_back(std::make_shared<HmlRenderer::Entity>(planeModel, glm::vec3{ 0.2f, 0.2f, 0.9f }));
        entities.back()->modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3{0.0f, 25.0f, 90.0f});

        for (const auto& pointLight : pointLights) {
            entities.push_back(std::make_shared<HmlRenderer::Entity>(vikingModel));
            entities.back()->modelMatrix = glm::translate(glm::mat4(1.0f), pointLight.position);
            entities.back()->color = pointLight.color;
        }


        hmlRenderer->specifyEntitiesToRender(entities);
    }

    if (!createSyncObjects()) return false;

    successfulInit = true;
    return true;
}


std::unique_ptr<HmlRenderPass> Himmel::createGeneralRenderPass(
        std::shared_ptr<HmlSwapchain> hmlSwapchain,
        std::shared_ptr<HmlImageResource> hmlDepthResource,
        const Weather& weather) const noexcept {
    // std::array<VkClearValue, 2> clearValues{};
    // clearValues[0].color = {{weather.fogColor.x, weather.fogColor.y, weather.fogColor.z, 1.0}};
    // clearValues[1].depthStencil = {1.0f, 0}; // 1.0 is farthest
    const HmlRenderPass::ColorAttachment colorAttachment{
        .imageFormat = hmlSwapchain->imageFormat,
        .imageViews = hmlSwapchain->imageViews,
        .clearColor = {{ weather.fogColor.x, weather.fogColor.y, weather.fogColor.z, 1.0f }},
    };
    const HmlRenderPass::DepthStencilAttachment depthAttachment{
        .imageFormat = hmlDepthResource->format,
        .imageView = hmlDepthResource->view,
        .clearColor = VkClearDepthStencilValue{ 1.0f, 0 }, // 1.0 is farthest
        .saveDepth = false,
    };
    HmlRenderPass::Config config{
        .colorAttachments = { colorAttachment },
        .depthStencilAttachment = { depthAttachment },
        .extent = hmlSwapchain->extent,
        .hasPrevious = false,
        .hasNext = true,
    };
    auto hmlRenderPass = HmlRenderPass::create(hmlDevice, hmlCommands, std::move(config));
    if (!hmlRenderPass) {
        std::cerr << "::> Failed to create General HmlRenderPass.\n";
        return { nullptr };
    }
    return hmlRenderPass;
}


std::unique_ptr<HmlRenderPass> Himmel::createUiRenderPass(std::shared_ptr<HmlSwapchain> hmlSwapchain) const noexcept {
    const HmlRenderPass::ColorAttachment colorAttachment{
        .imageFormat = hmlSwapchain->imageFormat,
        .imageViews = hmlSwapchain->imageViews,
        .clearColor = std::nullopt,
    };
    HmlRenderPass::Config config{
        .colorAttachments = { colorAttachment },
        .depthStencilAttachment = std::nullopt,
        .extent = hmlSwapchain->extent,
        .hasPrevious = true,
        .hasNext = false,
    };
    auto hmlRenderPass = HmlRenderPass::create(hmlDevice, hmlCommands, std::move(config));
    if (!hmlRenderPass) {
        std::cerr << "::> Failed to create UI HmlRenderPass.\n";
        return { nullptr };
    }
    return hmlRenderPass;
}


bool Himmel::run() noexcept {
    static auto startTime = std::chrono::high_resolution_clock::now();
    while (!hmlWindow->shouldClose()) {
        glfwPollEvents();

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

        if (!drawFrame()) return false;

        // const auto fps = 1.0 / deltaSeconds;
        // std::cout << "Delta = " << deltaSeconds * 1000.0f << "ms [FPS = " << fps << "]"
        //     << " Update = " << updateMs << "ms"
        //     << '\n';
    }
    vkDeviceWaitIdle(hmlDevice->device);
    return true;
}


void Himmel::updateForDt(float dt, float sinceStart) noexcept {
#define LAST_COUNT 3
    size_t index = entities.size() - LAST_COUNT;
    const float unit = 40.0f;
    for (size_t i = 0; i < LAST_COUNT; i++) {
        glm::mat4 model(1.0);
        if (i == 0) {
            model = glm::translate(model, glm::vec3(0.0, unit, 2*unit));
        } else if (i == 1) {
            model = glm::translate(model, glm::vec3(3*unit, unit, 0.0));
        } else if (i == 2) {
            model = glm::translate(model, glm::vec3(-2*unit, unit, -3*unit));
        }
        model = glm::rotate(model, (i+1) * 0.7f * sinceStart + i * 1.0f, glm::vec3(0.0, 1.0, 0.0));
        model = glm::translate(model, glm::vec3(50.0f, 0.0f, 0.0f));
        pointLights[pointLights.size() - LAST_COUNT + i].position = model[3];
        entities[index + i]->modelMatrix = model;
    }


    {
        const auto newCursor = hmlWindow->getCursor();
        const int32_t dx = newCursor.first - cursor.first;
        const int32_t dy = newCursor.second - cursor.second;
        cursor = newCursor;
        const float rotateSpeed = 0.05f;
        if (dx || dy) camera.rotateDir(-dy * rotateSpeed, dx * rotateSpeed);
    }
    {
        constexpr float movementSpeed = 16.0f;
        constexpr float boostUp = 10.0f;
        constexpr float boostDown = 0.2f;
        float length = movementSpeed * dt;
        if (glfwGetKey(hmlWindow->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            length *= boostUp;
        } else if (glfwGetKey(hmlWindow->window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
            length *= boostDown;
        }

        if (glfwGetKey(hmlWindow->window, GLFW_KEY_W) == GLFW_PRESS) {
            camera.forward(length);
        }
        if (glfwGetKey(hmlWindow->window, GLFW_KEY_S) == GLFW_PRESS) {
            camera.forward(-length);
        }
        if (glfwGetKey(hmlWindow->window, GLFW_KEY_D) == GLFW_PRESS) {
            camera.right(length);
        }
        if (glfwGetKey(hmlWindow->window, GLFW_KEY_A) == GLFW_PRESS) {
            camera.right(-length);
        }
        if (glfwGetKey(hmlWindow->window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            camera.lift(length);
        }
        if (glfwGetKey(hmlWindow->window, GLFW_KEY_C) == GLFW_PRESS) {
            camera.lift(-length);
        }

        static bool pPressed = false;
        if (!pPressed && glfwGetKey(hmlWindow->window, GLFW_KEY_P) == GLFW_PRESS) {
            pPressed = true;
            camera.printStats();
        } else if (pPressed && glfwGetKey(hmlWindow->window, GLFW_KEY_P) == GLFW_RELEASE) {
            pPressed = false;
        }
    }


    hmlSnowRenderer->updateForDt(dt, sinceStart);
    hmlTerrainRenderer->update(camera.getPos());
}


void Himmel::updateForImage(uint32_t imageIndex) noexcept {
    GeneralUbo generalUbo{
        .view = camera.view(),
        .proj = proj,
        .globalLightDir = glm::normalize(glm::vec3(0.5f, -1.0f, -1.0f)),
        .ambientStrength = 0.0f,
        .fogColor = weather.fogColor,
        .fogDensity = weather.fogDensity,
        .cameraPos = camera.getPos(),
    };
    viewProjUniformBuffers[imageIndex]->update(&generalUbo);

    LightUbo lightUbo;
    lightUbo.count = pointLights.size();
    for (size_t i = 0; i < pointLights.size(); i++) {
        lightUbo.pointLights[i] = pointLights[i];
    }
    lightUniformBuffers[imageIndex]->update(&lightUbo);

    hmlSnowRenderer->updateForImage(imageIndex);
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
    vkWaitForFences(hmlDevice->device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

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
    if (const VkResult result = vkAcquireNextImageKHR(hmlDevice->device, hmlSwapchain->swapchain, UINT64_MAX,
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
        vkWaitForFences(hmlDevice->device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }

    // The image has at least finished being rendered.
    // Mark the image as now being in use by this frame
    imagesInFlight[imageIndex] = inFlightFences[currentFrame];

    // Once we know what image we work with...

    updateForImage(imageIndex);

    // NOTE Only this part depends on a Renderer (commandBuffers)
    {
        const auto commandBuffer = commandBuffers[imageIndex];
        hmlRenderPassGeneral->begin(commandBuffer, imageIndex);
        {
            // NOTE These are parallelizable
            std::vector<VkCommandBuffer> secondaryCommandBuffers;
            secondaryCommandBuffers.push_back(hmlTerrainRenderer->draw(imageIndex, generalDescriptorSet_0_perImage[imageIndex]));
            secondaryCommandBuffers.push_back(hmlRenderer->draw(currentFrame, imageIndex, generalDescriptorSet_0_perImage[imageIndex]));
            secondaryCommandBuffers.push_back(hmlSnowRenderer->draw(currentFrame, imageIndex, generalDescriptorSet_0_perImage[imageIndex]));
            vkCmdExecuteCommands(commandBuffer, secondaryCommandBuffers.size(), secondaryCommandBuffers.data());
        }
        hmlRenderPassGeneral->end(commandBuffer);

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        // VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        VkSemaphore signalSemaphores[] = { generalFinishedSemaphores[currentFrame] };

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signalSemaphores,
        };

        // vkResetFences(hmlDevice->device, 1, &inFlightFences[currentFrame]);
        // The specified fence will get signaled when the command buffer finishes executing.
        // if (vkQueueSubmit(hmlDevice->graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
        if (vkQueueSubmit(hmlDevice->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
            std::cerr << "::> Failed to submit draw command buffer.\n";
            return false;
        }
    }
    // NOTE
    // NOTE It is also possible to combine the two commandBuffers into a single submission.
    // NOTE However, it might be prudent to do so only when we parallelize draws,
    // NOTE because otherwise right now we take advantage from the fact that
    // NOTE the first submission is already being processed while we record the second commandBuffer
    // NOTE
    {
        const auto commandBuffer = commandBuffers2[imageIndex];
        hmlRenderPassUi->begin(commandBuffer, imageIndex);
        {
            // NOTE These are parallelizable
            std::vector<VkCommandBuffer> secondaryCommandBuffers;
            secondaryCommandBuffers.push_back(hmlUiRenderer->draw(currentFrame, imageIndex));
            vkCmdExecuteCommands(commandBuffer, secondaryCommandBuffers.size(), secondaryCommandBuffers.data());
        }
        hmlRenderPassUi->end(commandBuffer);

        VkSemaphore waitSemaphores[] = { generalFinishedSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signalSemaphores,
        };

        vkResetFences(hmlDevice->device, 1, &inFlightFences[currentFrame]);
        // The specified fence will get signaled when the command buffer finishes executing.
        if (vkQueueSubmit(hmlDevice->graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            std::cerr << "::> Failed to submit draw command buffer.\n";
            return false;
        }
    }

    {
        VkSemaphore waitSemaphores[] = { renderFinishedSemaphores[currentFrame] };

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = waitSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &(hmlSwapchain->swapchain);
        presentInfo.pImageIndices = &imageIndex;

        // NOTE We recreate the swapchain twice: first we get notified through
        // vkQueuePresentKHR and on the next iteration through framebufferResizeRequested.
        // The second call is probably redundant but non-harmful.
        if (const VkResult result = vkQueuePresentKHR(hmlDevice->presentQueue, &presentInfo);
                result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || hmlWindow->framebufferResizeRequested) {
            hmlWindow->framebufferResizeRequested = false;
            recreateSwapchain();
        } else if (result != VK_SUCCESS) {
            std::cerr << "::> Failed to present swapchain image.\n";
            return false;
        }
    }

    currentFrame = (currentFrame + 1) % maxFramesInFlight;
    return true;
}


void Himmel::recreateSwapchain() noexcept {
    std::cout << ":> Recreating the Swapchain.\n";

    // Handle window minimization
    for (;;) {
        const auto [width, height] = hmlWindow->getFramebufferSize();
        if (width > 0 && height > 0) break;
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(hmlDevice->device);

    hmlSwapchain = HmlSwapchain::create(hmlWindow, hmlDevice, hmlResourceManager, { hmlSwapchain->swapchain });

    // Because they have to be rerecorder, and for that they need to be reset first
    hmlCommands->resetCommandPool(hmlCommands->commandPoolGeneral);


    // General RenderPass
    hmlDepthResource = hmlResourceManager->newDepthResource(hmlSwapchain->extent);
    if (!hmlDepthResource) return;
    hmlRenderPassGeneral = createGeneralRenderPass(hmlSwapchain, hmlDepthResource, weather);
    if (!hmlRenderPassGeneral) return;
    // UI RenderPass
    hmlRenderPassUi = createUiRenderPass(hmlSwapchain);
    if (!hmlRenderPassUi) return;


    // TODO foreach Renderer
    hmlRenderer->replaceRenderPass(hmlRenderPassGeneral);
    hmlSnowRenderer->replaceRenderPass(hmlRenderPassGeneral);
    hmlTerrainRenderer->replaceRenderPass(hmlRenderPassGeneral);
    hmlUiRenderer->replaceRenderPass(hmlRenderPassUi);
    // TODO maybe store this as as flag inside the Renderer so that it knows automatically
    // hmlTerrainRenderer->bake(generalDescriptorSet_0_perImage);

    proj = projFrom(hmlSwapchain->extentAspect());

    std::cout << '\n';
}


bool Himmel::createSyncObjects() noexcept {
    imageAvailableSemaphores.resize(maxFramesInFlight);
    renderFinishedSemaphores.resize(maxFramesInFlight);
    generalFinishedSemaphores.resize(maxFramesInFlight);
    inFlightFences.resize(maxFramesInFlight);
    imagesInFlight.resize(hmlSwapchain->imageCount(), VK_NULL_HANDLE); // initially not a single frame is using an image

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // So that the fences start in signaled state (which means the frame is
    // available to be acquired by CPU). Subsequent frames are signaled by
    // Vulkan upon command buffer execution finish.
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        if (vkCreateSemaphore(hmlDevice->device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i])  != VK_SUCCESS ||
            vkCreateSemaphore(hmlDevice->device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i])  != VK_SUCCESS ||
            vkCreateSemaphore(hmlDevice->device, &semaphoreInfo, nullptr, &generalFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence    (hmlDevice->device, &fenceInfo,     nullptr, &inFlightFences[i])            != VK_SUCCESS) {

            std::cerr << "::> Failed to create sync objects.\n";
            return false;
        }
    }

    return true;
}


Himmel::~Himmel() noexcept {
    std::cout << ":> Destroying Himmel...\n";
    // vkDeviceWaitIdle(hmlDevice->device);

    if (!successfulInit) {
        std::cerr << "::> Himmel init has not finished successfully, so no proper cleanup is node.\n";
        return;
    }

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        vkDestroySemaphore(hmlDevice->device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(hmlDevice->device, generalFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(hmlDevice->device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(hmlDevice->device, inFlightFences[i], nullptr);
    }

    // NOTE depends on swapchain recreation, but because it only depends on the
    // NOTE number of images, which most likely will not change, we ignore it.
    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlDevice->device, generalDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlDevice->device, generalDescriptorSetLayout, nullptr);
}


glm::mat4 Himmel::projFrom(float aspect_w_h) noexcept {
    const float near = 0.1f;
    const float far = 1000.0f;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect_w_h, near, far);
    proj[1][1] *= -1; // fix the inverted Y axis of GLM
    return proj;
}
