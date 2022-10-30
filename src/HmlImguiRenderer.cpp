#include "HmlImguiRenderer.h"


std::vector<std::unique_ptr<HmlPipeline>> HmlImguiRenderer::createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept {
    std::vector<std::unique_ptr<HmlPipeline>> pipelines;

    {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions = {
            VkVertexInputBindingDescription{
                .binding = 0, // index of current Binding in the array of Bindings
                .stride = sizeof(ImDrawVert),
                .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            }
        };

        std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
            VkVertexInputAttributeDescription{
                .location = 0, // as in shader
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(ImDrawVert, pos)
            },
            VkVertexInputAttributeDescription{
                .location = 1, // as in shader
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(ImDrawVert, uv)
            },
            VkVertexInputAttributeDescription{
                .location = 2, // as in shader
                .binding = 0,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .offset = offsetof(ImDrawVert, col)
            },
        };

        HmlGraphicsPipelineConfig config{
            .bindingDescriptions   = bindingDescriptions,
            .attributeDescriptions = attributeDescriptions,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .hmlShaders = HmlShaders()
                .addVertex("../shaders/out/imgui.vert.spv")
                .addFragment("../shaders/out/imgui.frag.spv"),
            .renderPass = hmlRenderPass->renderPass,
            .extent = hmlRenderPass->extent,
            .polygoneMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .descriptorSetLayouts = descriptorSetLayouts,
            .pushConstantsStages = VK_SHADER_STAGE_VERTEX_BIT,
            .pushConstantsSizeBytes = sizeof(PushConstant),
            .tessellationPatchPoints = 0,
            .lineWidth = 1.0f,
            .colorAttachmentCount = hmlRenderPass->colorAttachmentCount,
            .withBlending = true,
            .withDepthTest = false,
        };

        pipelines.push_back(HmlPipeline::createGraphics(hmlContext->hmlDevice, std::move(config)));
    }

    return pipelines;
}


std::unique_ptr<HmlImguiRenderer> HmlImguiRenderer::create(
        std::shared_ptr<std::unique_ptr<HmlBuffer>> vertexBuffer,
        std::shared_ptr<std::unique_ptr<HmlBuffer>> indexBuffer,
        std::shared_ptr<HmlContext> hmlContext) noexcept {
    auto hmlImguiRenderer = std::make_unique<HmlImguiRenderer>();
    hmlImguiRenderer->hmlContext = hmlContext;
    hmlImguiRenderer->vertexBuffer = vertexBuffer;
    hmlImguiRenderer->indexBuffer = indexBuffer;

    // Create font texture
    {
        unsigned char* fontData;
        int texWidth, texHeight;
        const uint32_t componentsCount = 4;

        ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&fontData, &texWidth, &texHeight);

        const VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
        hmlImguiRenderer->fontTexture = hmlContext->hmlResourceManager->newTextureResourceFromData(
            texWidth, texHeight, componentsCount, fontData, format, std::nullopt);
        if (!hmlImguiRenderer->fontTexture) return { nullptr };
        hmlImguiRenderer->fontTexture->sampler = hmlContext->hmlResourceManager->createTextureSamplerForFonts();
    }

    // DescriptorPool
    hmlImguiRenderer->descriptorPool = hmlContext->hmlDescriptors->buildDescriptorPool()
        .withTextures(1)
        .maxDescriptorSets(1)
        .build(hmlContext->hmlDevice);
    if (!hmlImguiRenderer->descriptorPool) return { nullptr };

    // DescriptorSetLayout
    hmlImguiRenderer->descriptorSetLayout = hmlContext->hmlDescriptors->buildDescriptorSetLayout()
        .withTextureAt(0, VK_SHADER_STAGE_FRAGMENT_BIT)
        .build(hmlContext->hmlDevice);
    if (!hmlImguiRenderer->descriptorSetLayout) return { nullptr };
    hmlImguiRenderer->descriptorSetLayouts.push_back(hmlImguiRenderer->descriptorSetLayout);

    // DescritorSet
    hmlImguiRenderer->descriptorSet = hmlContext->hmlDescriptors->createDescriptorSets(1,
        hmlImguiRenderer->descriptorSetLayout, hmlImguiRenderer->descriptorPool)[0];
    if (!hmlImguiRenderer->descriptorSet) return { nullptr };

    // Update(write) DescriptorSet
    HmlDescriptorSetUpdater(hmlImguiRenderer->descriptorSet)
        .textureAt(0, hmlImguiRenderer->fontTexture->sampler, hmlImguiRenderer->fontTexture->view).update(hmlContext->hmlDevice);

    return hmlImguiRenderer;
}


HmlImguiRenderer::~HmlImguiRenderer() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlImguiRenderer...\n";
#endif

    // DescriptorSets are freed automatically upon the deletion of the pool
    vkDestroyDescriptorPool(hmlContext->hmlDevice->device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(hmlContext->hmlDevice->device, descriptorSetLayout, nullptr);
}


VkCommandBuffer HmlImguiRenderer::draw(const HmlFrameData& frameData) noexcept {
    auto commandBuffer = getCurrentCommands()[frameData.frameIndex];
    const auto inheritanceInfo = VkCommandBufferInheritanceInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
        .pNext = VK_NULL_HANDLE,
        .renderPass = currentRenderPass->renderPass,
        .subpass = 0, // we only have a single one
        .framebuffer = currentRenderPass->framebuffers[frameData.imageIndex],
        .occlusionQueryEnable = VK_FALSE,
        .queryFlags = static_cast<VkQueryControlFlags>(0),
        .pipelineStatistics = static_cast<VkQueryPipelineStatisticFlags>(0)
    };
    hmlContext->hmlCommands->beginRecordingSecondaryOnetime(commandBuffer, &inheritanceInfo);
    hmlContext->hmlQueries->registerEvent("HmlImguiRenderer: begin", "ImG(w)", commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

    assert(getCurrentPipelines().size() == 1 && "::> Expected only a single pipeline in HmlImguiRenderer.\n");
    const auto& hmlPipeline = getCurrentPipelines()[0];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hmlPipeline->pipeline);

    std::array<VkDescriptorSet, 1> descriptorSets = { descriptorSet };
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
        hmlPipeline->layout, 0, descriptorSets.size(), descriptorSets.data(), 0, nullptr);

    ImGuiIO& io = ImGui::GetIO();
    // VkViewport viewport = {};
    // viewport.width = io.DisplaySize.x;
    // viewport.height = io.DisplaySize.y;
    // viewport.minDepth = 0.0f;
    // viewport.maxDepth = 1.0f;
    // vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    PushConstant pushConstant{
        .scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y),
        .translate = glm::vec2(-1.0f),
    };
    vkCmdPushConstants(commandBuffer, hmlPipeline->layout,
        hmlPipeline->pushConstantsStages, 0, sizeof(PushConstant), &pushConstant);

    ImDrawData* imDrawData = ImGui::GetDrawData();
    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;
    if (imDrawData->CmdListsCount > 0) {
        assert((vertexBuffer && indexBuffer) && "::> Vertex or index buffer is null.");
        VkDeviceSize offsets[1] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &((*vertexBuffer)->buffer), offsets);
        vkCmdBindIndexBuffer(commandBuffer, (*indexBuffer)->buffer, 0, VK_INDEX_TYPE_UINT16);

        for (int32_t i = 0; i < imDrawData->CmdListsCount; i++) {
            const ImDrawList* cmd_list = imDrawData->CmdLists[i];
            for (int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
                const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[j];
                // VkRect2D scissorRect;
                // scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
                // scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
                // scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
                // scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
                // vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
                vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
                indexOffset += pcmd->ElemCount;
            }
            vertexOffset += cmd_list->VtxBuffer.Size;
        }
    }

    hmlContext->hmlQueries->registerEvent("HmlImguiRenderer: end", "ImG", commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
    hmlContext->hmlCommands->endRecording(commandBuffer);
    return commandBuffer;
}
