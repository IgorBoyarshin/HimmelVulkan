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


    for (size_t i = 0; i < hmlSwapchain->imageCount(); i++) {
        const auto size = sizeof(GeneralUbo);
        auto ubo = hmlResourceManager->createUniformBuffer(size);
        ubo->map();
        viewProjUniformBuffers.push_back(std::move(ubo));
    }


    generalDescriptorPool = hmlDescriptors->buildDescriptorPool()
        .withUniformBuffers(hmlSwapchain->imageCount())
        .maxSets(hmlSwapchain->imageCount())
        .build(hmlDevice);
    if (!generalDescriptorPool) return false;

    generalDescriptorSetLayout = hmlDescriptors->buildDescriptorSetLayout()
        .withUniformBufferAt(0, VK_SHADER_STAGE_VERTEX_BIT)
        .build(hmlDevice);
    if (!generalDescriptorSetLayout) return false;

    generalDescriptorSet_0_perImage = hmlDescriptors->createDescriptorSets(
        hmlSwapchain->imageCount(), generalDescriptorSetLayout, generalDescriptorPool);
    if (generalDescriptorSet_0_perImage.empty()) return false;
    for (size_t imageIndex = 0; imageIndex < hmlSwapchain->imageCount(); imageIndex++) {
        const auto set = generalDescriptorSet_0_perImage[imageIndex];
        const auto buffer = viewProjUniformBuffers[imageIndex]->uniformBuffer;
        const auto size = sizeof(GeneralUbo);
        HmlDescriptorSetUpdater(set).uniformBufferAt(0, buffer, size).update(hmlDevice);
    }


    hmlRenderer = HmlRenderer::createSimpleRenderer(hmlWindow, hmlDevice, hmlCommands,
        hmlSwapchain, hmlResourceManager, hmlDescriptors, generalDescriptorSetLayout, maxFramesInFlight);
    if (!hmlRenderer) return false;


    const auto SIZE = 100.0f;
    const auto HEIGHT = 25;
    const auto snowCount = 400000;
    const auto snowBounds = HmlSnowParticleRenderer::SnowBounds {
        .xMin = -SIZE,
        .xMax = +SIZE,
        .yMin = 0.0f,
        .yMax = +2.0f * HEIGHT,
        .zMin = -SIZE,
        .zMax = +SIZE
    };
    hmlSnowRenderer = HmlSnowParticleRenderer::createSnowRenderer(snowCount, snowBounds, hmlWindow,
        hmlDevice, hmlCommands, hmlSwapchain, hmlResourceManager, hmlDescriptors, generalDescriptorSetLayout, maxFramesInFlight);
    if (!hmlSnowRenderer) return false;


    const auto terrainBounds = HmlTerrainRenderer::Bounds{
        .posStart = glm::vec2(-SIZE, -SIZE),
        .posFinish = glm::vec2(SIZE, SIZE),
        .height = HEIGHT,
        .yOffset = 0,
    };
    hmlTerrainRenderer = HmlTerrainRenderer::create("models/heightmap.png", "models/grass3.png",
        terrainBounds, generalDescriptorSet_0_perImage, hmlWindow,
        hmlDevice, hmlCommands, hmlSwapchain, hmlResourceManager, hmlDescriptors, generalDescriptorSetLayout, maxFramesInFlight);
    if (!hmlTerrainRenderer) return false;


    camera = HmlCamera{{ 0.0f, 0.0f, 2.0f }};
    cursor = hmlWindow->getCursor();
    proj = projFrom(hmlSwapchain->extentAspect());


    {
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
            const auto model = hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices);
            models.push_back(model);
        }

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
            const auto model = hmlResourceManager->newModel(vertices.data(), verticesSizeBytes, indices, "models/viking_room.png", VK_FILTER_LINEAR);
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

        entities.push_back(std::make_shared<HmlRenderer::Entity>(models[3], glm::vec3{ 0.8f, 0.2f, 0.5f }));
        entities.push_back(std::make_shared<HmlRenderer::Entity>(models[0], glm::vec3{ 1.0f, 0.0f, 0.0f }));
        entities.push_back(std::make_shared<HmlRenderer::Entity>(models[2]));
        entities.push_back(std::make_shared<HmlRenderer::Entity>(models[1]));
        entities.push_back(std::make_shared<HmlRenderer::Entity>(models[3], glm::vec3{ 0.1f, 0.8f, 0.5f }));
        entities.push_back(std::make_shared<HmlRenderer::Entity>(models[0], glm::vec3{ 0.0f, 1.0f, 0.0f }));
        entities.push_back(std::make_shared<HmlRenderer::Entity>(models[1]));
        entities.push_back(std::make_shared<HmlRenderer::Entity>(models[2]));
        entities.push_back(std::make_shared<HmlRenderer::Entity>(models[3], glm::vec3{ 0.8f, 0.2f, 0.5f }));

        hmlRenderer->specifyEntitiesToRender(entities);
    }

    if (!createSyncObjects()) return false;

    return true;
}


void Himmel::run() noexcept {
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
        update(deltaSeconds, sinceStartSeconds);

        drawFrame();

        // const auto fps = 1.0 / deltaSeconds;
        // std::cout << "Delta = " << deltaSeconds * 1000.0f << "ms [FPS = " << fps << "]\n";
    }
    vkDeviceWaitIdle(hmlDevice->device);
}


void Himmel::update(float dt, float sinceStart) noexcept {
    int index = 0;
    for (auto& entity : entities) {
        const float dir = (index % 2) ? 1.0f : -1.0f;
        auto matrix = glm::mat4(1.0f);
        // matrix = glm::rotate(matrix, dir * sinceStart * glm::radians(40.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        // matrix = glm::translate(matrix, glm::vec3{0.0f, 15.0f, dir * index * 8.0f + 5.0f * glm::sin(sinceStart)});
        matrix = glm::translate(matrix, glm::vec3{0.0f, 15.0f, dir * index * 8.0f});
        entity->modelMatrix = matrix;

        index++;
    }

    {
        const auto newCursor = hmlWindow->getCursor();
        const int32_t dx = newCursor.first - cursor.first;
        const int32_t dy = newCursor.second - cursor.second;
        cursor = newCursor;
        if (dx || dy) camera.rotateDir(-dy, dx);
    }
    {
        constexpr float movementSpeed = 15.0f;
        constexpr float boostUp = 8.0f;
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
    }


    hmlSnowRenderer->updateForDt(dt);
}


/*
 * "In flight" -- "in GPU rendering pipeline": either waiting to be rendered,
 * or already being rendered; but not finished rendering yet!
 * So, its commandBuffer has been submitted, but has not finished yet.
 * */
void Himmel::drawFrame() noexcept {
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
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        std::cerr << "::> Failed to acquire swap chain image.\n";
        return;
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

    GeneralUbo ubo{
        .view = camera.view(),
        .proj = proj,
        .globalLightDir = glm::normalize(glm::vec3(0.5f, -1.0f, -1.0f)),
        .ambientStrength = 0.1f,
        .fogDensity = 0.015,
    };
    viewProjUniformBuffers[imageIndex]->update(&ubo);

    hmlSnowRenderer->updateForImage(imageIndex);


    // NOTE Only this part depends on a Renderer (commandBuffers)
    {
        recordDrawBegin(commandBuffers[imageIndex], imageIndex);

        std::vector<VkCommandBuffer> secondaryCommandBuffers;
        secondaryCommandBuffers.push_back(hmlTerrainRenderer->draw(imageIndex));
        secondaryCommandBuffers.push_back(hmlRenderer->draw(currentFrame, imageIndex, generalDescriptorSet_0_perImage[imageIndex]));
        secondaryCommandBuffers.push_back(hmlSnowRenderer->draw(currentFrame, imageIndex, generalDescriptorSet_0_perImage[imageIndex]));
        vkCmdExecuteCommands(commandBuffers[imageIndex], secondaryCommandBuffers.size(), secondaryCommandBuffers.data());

        recordDrawEnd(commandBuffers[imageIndex]);

        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        vkResetFences(hmlDevice->device, 1, &inFlightFences[currentFrame]);
        // The specified fence will get signaled when the command buffer finishes executing.
        if (vkQueueSubmit(hmlDevice->graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            std::cerr << "::> Failed to submit draw command buffer.\n";
            return;
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
            return;
        }
    }

    currentFrame = (currentFrame + 1) % maxFramesInFlight;
}


void Himmel::recordDrawBegin(VkCommandBuffer commandBuffer, uint32_t imageIndex) noexcept {
    hmlCommands->beginRecordingPrimaryOnetime(commandBuffer);

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = hmlSwapchain->renderPass;
    renderPassInfo.framebuffer = hmlSwapchain->framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = hmlSwapchain->extent;

    // Used for VK_ATTACHMENT_LOAD_OP_CLEAR
    std::array<VkClearValue, 2> clearValues{};
    // The order (indexing) must be the same as in attachments!
    clearValues[0].color = {{FOG_COLOR.x, FOG_COLOR.y, FOG_COLOR.z, FOG_COLOR.w}};
    clearValues[1].depthStencil = {1.0f, 0}; // 1.0 is farthest
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
}


void Himmel::recordDrawEnd(VkCommandBuffer commandBuffer) noexcept {
    vkCmdEndRenderPass(commandBuffer);
    hmlCommands->endRecording(commandBuffer);
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

    // TODO foreach Renderer
    hmlRenderer->replaceSwapchain(hmlSwapchain);
    hmlSnowRenderer->replaceSwapchain(hmlSwapchain);
    hmlTerrainRenderer->replaceSwapchain(hmlSwapchain);
    // TODO maybe store this as as flag inside the Renderer so that it knows automatically
    hmlTerrainRenderer->bake(generalDescriptorSet_0_perImage);

    proj = projFrom(hmlSwapchain->extentAspect());

    std::cout << '\n';
}


bool Himmel::createSyncObjects() noexcept {
    imageAvailableSemaphores.resize(maxFramesInFlight);
    renderFinishedSemaphores.resize(maxFramesInFlight);
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
        if (vkCreateSemaphore(hmlDevice->device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(hmlDevice->device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence    (hmlDevice->device, &fenceInfo,     nullptr, &inFlightFences[i])           != VK_SUCCESS) {

            std::cerr << "::> Failed to create sync objects.\n";
            return false;
        }
    }

    return true;
}


Himmel::~Himmel() noexcept {
    for (size_t i = 0; i < maxFramesInFlight; i++) {
        vkDestroySemaphore(hmlDevice->device, renderFinishedSemaphores[i], nullptr);
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
