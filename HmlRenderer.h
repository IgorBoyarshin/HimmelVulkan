#ifndef HML_RENDERER
#define HML_RENDERER

#include <memory>
#include <vector>
#include <chrono>

#include "HmlWindow.h"
#include "HmlPipeline.h"
#include "HmlSwapchain.h"
#include "HmlCommands.h"
#include "HmlModel.h"
#include "HmlResourceManager.h"


struct HmlShaderLayoutItem {
    VkDescriptorType descriptorType;
    uint32_t binding;
    VkShaderStageFlags stages;
};

struct HmlShaderLayout {
    std::vector<HmlShaderLayoutItem> items;

    HmlShaderLayout& addUniformBuffer(uint32_t binding, VkShaderStageFlags stages) {
        items.push_back(HmlShaderLayoutItem{
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .binding = binding,
            .stages = stages
        });
        return *this;
    }

    HmlShaderLayout& addTexture(uint32_t binding, VkShaderStageFlags stages) {
        items.push_back(HmlShaderLayoutItem{
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .binding = binding,
            .stages = stages
        });
        return *this;
    }

    std::vector<VkDescriptorSetLayoutBinding> createLayoutBindings() {
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        for (const auto& shaderLayout : items) {
            bindings.push_back(VkDescriptorSetLayoutBinding{
                .binding = shaderLayout.binding,
                .descriptorType = shaderLayout.descriptorType,
                .descriptorCount = 1,
                .stageFlags = shaderLayout.stages,
                .pImmutableSamplers = nullptr
            });
        }

        return bindings;
    }
};


struct HmlRenderer {
    struct SimpleUniformBufferObject {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view;
        alignas(16) glm::mat4 proj;
    };


    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;

    std::shared_ptr<HmlPipeline> hmlPipeline;
    HmlShaderLayout hmlShaderLayout;

    VkDescriptorPool descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets; // per swapChainImage
    VkDescriptorSetLayout descriptorSetLayout;


    std::vector<uint32_t> entitiesIndicesCount;


    // Sync objects
    uint32_t maxFramesInFlight = 2; // TODO make a part of SimpleRenderer creation
    std::vector<VkSemaphore> imageAvailableSemaphores; // for each frame in flight
    std::vector<VkSemaphore> renderFinishedSemaphores; // for each frame in flight
    std::vector<VkFence> inFlightFences;               // for each frame in flight
    std::vector<VkFence> imagesInFlight;               // for each swapChainImage


    // TODO where to store them??
    // Cleaned automatically upon CommandPool destruction
    std::vector<VkCommandBuffer> commandBuffers;


    // TODO maybe pass only a subset of swapchain members
    static std::unique_ptr<HmlRenderer> createSimpleRenderer(
            std::shared_ptr<HmlWindow> hmlWindow,
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlCommands> hmlCommands,
            std::shared_ptr<HmlSwapchain> hmlSwapchain,
            std::shared_ptr<HmlResourceManager> hmlResourceManager) {
        auto hmlRenderer = std::make_unique<HmlRenderer>();
        hmlRenderer->hmlWindow = hmlWindow;
        hmlRenderer->hmlDevice = hmlDevice;
        hmlRenderer->hmlCommands = hmlCommands;
        hmlRenderer->hmlSwapchain = hmlSwapchain;
        hmlRenderer->hmlResourceManager = hmlResourceManager;


        hmlRenderer->hmlShaderLayout = HmlShaderLayout()
            .addUniformBuffer(0, VK_SHADER_STAGE_VERTEX_BIT)
            .addTexture(1, VK_SHADER_STAGE_FRAGMENT_BIT);

        hmlRenderer->descriptorSetLayout = hmlRenderer->createDescriptorSetLayout(hmlRenderer->hmlShaderLayout.createLayoutBindings());
        if (!hmlRenderer->descriptorSetLayout) return { nullptr };


        HmlGraphicsPipelineConfig pipelineConfig{
            .bindingDescriptions   = HmlSimpleModel::Vertex::getBindingDescriptions(),
            .attributeDescriptions = HmlSimpleModel::Vertex::getAttributeDescriptions(),
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .hmlShaders = HmlShaders()
                .addVertex("vertex.spv")
                .addFragment("fragment.spv"), // TODO rename to "simple"
            .renderPass = hmlSwapchain->renderPass,
            .swapchainExtent = hmlSwapchain->extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayout = hmlRenderer->descriptorSetLayout
        };
        hmlRenderer->hmlPipeline = HmlPipeline::createGraphics(hmlDevice, std::move(pipelineConfig));
        if (!hmlRenderer->hmlPipeline) return { nullptr };

        {
            const uint32_t texturesCount = 3; // TODO
            const uint32_t ubosCount = 3; // TODO
            hmlRenderer->descriptorPool = hmlRenderer->createDescriptorPool(texturesCount, ubosCount);
            if (!hmlRenderer->descriptorPool) return { nullptr };
        }

        hmlRenderer->descriptorSets = hmlRenderer->createDescriptorSets();
        if (hmlRenderer->descriptorSets.empty()) return { nullptr };

        hmlRenderer->commandBuffers = hmlCommands->allocate(hmlSwapchain->imagesCount());

        // TODO will be moved from here
        hmlResourceManager->newUniformBuffer(hmlSwapchain->imagesCount(), sizeof(SimpleUniformBufferObject));
        hmlResourceManager->newTexture("girl.png");

        hmlRenderer->updateDescriptorSets();

        hmlRenderer->createSyncObjects();

        return hmlRenderer;
    }


    ~HmlRenderer() {
        std::cout << ":> Destroying HmlRenderer...\n";

        for (size_t i = 0; i < maxFramesInFlight; i++) {
            vkDestroySemaphore(hmlDevice->device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(hmlDevice->device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(hmlDevice->device, inFlightFences[i], nullptr);
        }

        // XXX TODO depends on swapchain recreation
        // Is done here because it depends on the number of swapchain images,
        // which may change upon swapchain recreation.
        // DescriptorSets are freed automatically upon the deletion of the pool
        vkDestroyDescriptorPool(hmlDevice->device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(hmlDevice->device, descriptorSetLayout, nullptr);
    }


    // TODO in future return ID
    void addEntity(const void* vertices, size_t verticesSizeBytes, const std::vector<uint16_t>& indices) {
        // TODO handle IDs
        auto id1 = hmlResourceManager->newVertexBuffer(vertices, verticesSizeBytes);
        auto id2 = hmlResourceManager->newIndexBuffer(indices);
        id1 = id2; id2 = id1;

        entitiesIndicesCount.push_back(indices.size());
    }


    // TODO
    // void reassignTexture() {
    //     updateDescriptorSets(3); // TODO count
    // }


    // TODO new texture index via argument
    // XXX TODO remove direct access of HmlResourceManager members
    void updateDescriptorSets() {
        for (size_t setIndex = 0; setIndex < hmlSwapchain->imagesCount(); setIndex++) {
            std::vector<VkWriteDescriptorSet> descriptorWrites;

            // Because descriptorWrites stores memory addresses, we cannot allow
            // those objects to live on stack in the inner scope.
            // The objects can be disposed of after the call to vkUpdateDescriptorSets
            std::vector<VkDescriptorBufferInfo> tmpBufferInfos;
            std::vector<VkDescriptorImageInfo> tmpImageInfos;
            for (const auto& shaderLayout : hmlShaderLayout.items) {
                switch (shaderLayout.descriptorType) {
                    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
                        tmpBufferInfos.push_back(VkDescriptorBufferInfo{
                            .buffer = hmlResourceManager->uniformBuffers[setIndex], // XXX TODO
                            .offset = 0,
                            .range = sizeof(SimpleUniformBufferObject)
                        });

                        descriptorWrites.push_back(VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .pNext = nullptr,
                            .dstSet = descriptorSets[setIndex],
                            .dstBinding = shaderLayout.binding,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            .pImageInfo       = nullptr,
                            .pBufferInfo      = &tmpBufferInfos.back(),
                            .pTexelBufferView = nullptr
                        });

                        break;
                    }
                    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                        tmpImageInfos.push_back(VkDescriptorImageInfo{
                            .sampler = hmlResourceManager->textureSampler, // XXX TODO
                            .imageView = hmlResourceManager->textureImageView, // XXX TODO
                            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                        });

                        descriptorWrites.push_back(VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .pNext = nullptr,
                            .dstSet = descriptorSets[setIndex],
                            .dstBinding = shaderLayout.binding,
                            .dstArrayElement = 0,
                            .descriptorCount = 1,
                            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                            .pImageInfo       = &tmpImageInfos.back(),
                            .pBufferInfo      = nullptr,
                            .pTexelBufferView = nullptr
                        });
                        break;
                    }
                    default:
                        std::cerr << "::> Unexpected descriptorType in updateDescriptorSets(): " << shaderLayout.descriptorType << ".\n";
                        return;
                }
            }

            vkUpdateDescriptorSets(hmlDevice->device, static_cast<uint32_t>(descriptorWrites.size()),
                descriptorWrites.data(), 0, nullptr);
        }
    }


    // TODO handle multiple Entities
    void bakeCommandBuffers() {
        for (size_t i = 0; i < hmlSwapchain->imagesCount(); i++) {
            hmlCommands->beginRecording(commandBuffers[i]);


            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = hmlSwapchain->renderPass;
            renderPassInfo.framebuffer = hmlSwapchain->framebuffers[i];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = hmlSwapchain->extent;

            // Used for VK_ATTACHMENT_LOAD_OP_CLEAR
            std::array<VkClearValue, 2> clearValues{};
            // The order (indexing) must be the same as in attachments!
            clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            clearValues[1].depthStencil = {1.0f, 0}; // 1.0 is farthest
            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            // Last argument to vkCmdBeginRenderPass:
            // VK_SUBPASS_CONTENTS_INLINE: The render pass commands will be embedded in the
            // primary command buffer itself and no secondary command buffers will be executed;
            // VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: The render pass commands
            // will be executed from secondary command buffers.
            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

            VkBuffer vertexBuffers[] = { hmlResourceManager->vertexBuffer };
            VkDeviceSize offsets[] = { 0 };
            // NOTE: advanced usage world have a single VkBuffer for both
            // VertexBuffer and IndexBuffer and specify different offsets into it.
            // Second and third arguments specify the offset and number of bindings
            // we're going to specify VertexBuffers for.
            vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
            // XXX Indices type is specified here:
            vkCmdBindIndexBuffer(commandBuffers[i], hmlResourceManager->indexBuffer, 0, VK_INDEX_TYPE_UINT16); // 0 is offset
            // Descriptors are not unique to graphics pipelines, unlike Vertex
            // and Index buffers, so we specify where to bind: graphics or compute
            vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                    hmlPipeline->layout, 0, 1, &descriptorSets[i], 0, nullptr);

            // NOTE: could use firstInstance to supply a single integer to gl_BaseInstance
            const uint32_t instanceCount = 1;
            const uint32_t firstInstance = 0;
            const uint32_t firstIndex = 0;
            const uint32_t offsetToAddToIndices = 0;
            // const uint32_t vertexCount = vertices.size();
            // const uint32_t firstVertex = 0;
            // vkCmdDraw(commandBuffers[i], vertexCount, instanceCount, firstVertex, firstInstance);
            vkCmdDrawIndexed(commandBuffers[i], entitiesIndicesCount[0], // TODO XXX count
                instanceCount, firstIndex, offsetToAddToIndices, firstInstance);
            vkCmdEndRenderPass(commandBuffers[i]);


            hmlCommands->endRecording(commandBuffers[i]);
        }
    }


    void run() {
        while (!glfwWindowShouldClose(hmlWindow->window)) {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(hmlDevice->device);
    }


    /*
     * "In flight" -- "in GPU rendering pipeline": either waiting to be rendered,
     * or already being rendered; but not finished rendering yet!
     * So, its commandBuffer has been submitted, but has not finished yet.
     * */
    void drawFrame() {
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
            // recreateSwapchain(queueFamilyIndices);
            // TODO XXX
            // TODO XXX
            // TODO XXX
            // TODO XXX
            // TODO XXX
            return;
        } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
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
        // Takes care of screen resizing
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        const float aspect_w_h = hmlSwapchain->extentAspect();
        const float near = 0.1f;
        const float far = 10.0f;
        SimpleUniformBufferObject ubo{
            .model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .view = glm::lookAt(
                glm::vec3(0.0f, 1.0f, 1.0f), // position in Worls space
                glm::vec3(0.0f, 0.0f, 0.0f), // where to look
                glm::vec3(0.0f, 1.0f, 0.0f)), // where the head is
            .proj = glm::perspective(glm::radians(45.0f), aspect_w_h, near, far)
        };
        ubo.proj[1][1] *= -1; // fix the inverted Y axis of GLM
        hmlResourceManager->updateUniformBuffer(imageIndex, &ubo, sizeof(ubo));


        {
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
                throw std::runtime_error("failed to submit draw command buffer!");
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

            if (const VkResult result = vkQueuePresentKHR(hmlDevice->presentQueue, &presentInfo);
                    result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || hmlWindow->framebufferResizeRequested) {
                hmlWindow->framebufferResizeRequested = false;
                // recreateSwapchain(queueFamilyIndices);
                // TODO XXX
                // TODO XXX
                // TODO XXX
                // TODO XXX
                // TODO XXX
            } else if (result != VK_SUCCESS) {
                throw std::runtime_error("failed to present swapchain image!");
            }
        }

        currentFrame = (currentFrame + 1) % maxFramesInFlight;
    }


    std::vector<VkDescriptorSet> createDescriptorSets() {
        std::vector<VkDescriptorSetLayout> layouts(hmlSwapchain->imagesCount(), descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(hmlSwapchain->imagesCount());
        allocInfo.pSetLayouts = layouts.data();

        std::vector<VkDescriptorSet> descriptorSets(hmlSwapchain->imagesCount());
        if (vkAllocateDescriptorSets(hmlDevice->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS) {
            std::cerr << "::> Failed to allocare DescriptorSets.\n";
            return {};
        }
        return descriptorSets;
    }


    // "layout (set=M, binding=N) uniform sampler2D variableNameArray[I];":
    // -- M refers the the M'th descriptor set layout in the pSetLayouts member of the pipeline layout
    // -- N refers to the N'th descriptor set (binding) in M's pBindings member of the descriptor set layout
    // -- I is the index into the array of descriptors in N's descriptor set
    //
    // For UBO: VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    // For texture: VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    // stageFlags: see VkShaderStageFlagBits
    // descriptorCount: e.g. to specify a transformation for each of the bones
    //                  in a skeleton for skeletal animation
    VkDescriptorSetLayout createDescriptorSetLayout(std::vector<VkDescriptorSetLayoutBinding>&& bindings) {
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        VkDescriptorSetLayout descriptorSetLayout;
        if (vkCreateDescriptorSetLayout(hmlDevice->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
            std::cerr << "::> Failed to create DescriptorSetLayout.\n";
            return VK_NULL_HANDLE;
        }
        return descriptorSetLayout;
    }


    VkDescriptorPool createDescriptorPool(uint32_t texturesCount, uint32_t ubosCount) {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = ubosCount * hmlSwapchain->imagesCount();
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = texturesCount * hmlSwapchain->imagesCount();

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = hmlSwapchain->imagesCount(); // TODO what is
        // Can use VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT to specify
        // if individual descriptor sets can be freed or not. Because we are not
        // going to touch them after creating, we leave flags as 0.
        poolInfo.flags = 0;

        VkDescriptorPool descriptorPool;
        if (vkCreateDescriptorPool(hmlDevice->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
            std::cerr << "::> Failed to create DescriptorPool.\n";
            return VK_NULL_HANDLE;
        }
        return descriptorPool;
    }


    void createSyncObjects() {
        imageAvailableSemaphores.resize(maxFramesInFlight);
        renderFinishedSemaphores.resize(maxFramesInFlight);
        inFlightFences.resize(maxFramesInFlight);
        imagesInFlight.resize(hmlSwapchain->imagesCount(), VK_NULL_HANDLE); // initially not a single frame is using an image

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

                throw std::runtime_error("failed to create semaphores!");
            }
        }
    }
};

#endif
