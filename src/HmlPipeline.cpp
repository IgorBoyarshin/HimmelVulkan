#include "HmlPipeline.h"


std::optional<std::pair<std::vector<VkShaderModule>, std::vector<VkPipelineShaderStageCreateInfo>>>
        HmlPipeline::createShaders(const HmlShaders& hmlShaders) noexcept {
    std::vector<VkShaderModule> shaderModules;
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

    if (hmlShaders.vertex) {
        if (auto shaderModule = createShaderModule(readFile(hmlShaders.vertex)); shaderModule) {
            shaderModules.push_back(shaderModule);
            shaderStages.push_back(createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, shaderModule, hmlShaders.vertexSpecializationInfo));
        } else return std::nullopt;
    }
    if (hmlShaders.tessellationControl) {
        if (auto shaderModule = createShaderModule(readFile(hmlShaders.tessellationControl)); shaderModule) {
            shaderModules.push_back(shaderModule);
            shaderStages.push_back(createShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, shaderModule, hmlShaders.tessellationControlSpecializationInfo));
        } else return std::nullopt;
    }
    if (hmlShaders.tessellationEvaluation) {
        if (auto shaderModule = createShaderModule(readFile(hmlShaders.tessellationEvaluation)); shaderModule) {
            shaderModules.push_back(shaderModule);
            shaderStages.push_back(createShaderStageInfo(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, shaderModule, hmlShaders.tessellationEvaluationSpecializationInfo));
        } else return std::nullopt;
    }
    if (hmlShaders.geometry) {
        if (auto shaderModule = createShaderModule(readFile(hmlShaders.geometry)); shaderModule) {
            shaderModules.push_back(shaderModule);
            shaderStages.push_back(createShaderStageInfo(VK_SHADER_STAGE_GEOMETRY_BIT, shaderModule, hmlShaders.geometrySpecializationInfo));
        } else return std::nullopt;
    }
    if (hmlShaders.fragment) {
        if (auto shaderModule = createShaderModule(readFile(hmlShaders.fragment)); shaderModule) {
            shaderModules.push_back(shaderModule);
            shaderStages.push_back(createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, shaderModule, hmlShaders.fragmentSpecializationInfo));
        } else return std::nullopt;
    }

    return { std::make_pair(shaderModules, shaderStages) };
}


std::unique_ptr<HmlPipeline> HmlPipeline::createGraphics(std::shared_ptr<HmlDevice> hmlDevice,
        HmlGraphicsPipelineConfig&& hmlPipelineConfig) noexcept {
    auto hmlPipeline = std::make_unique<HmlPipeline>();
    hmlPipeline->pushConstantsStages = hmlPipelineConfig.pushConstantsStages;
    hmlPipeline->hmlDevice = hmlDevice;

    // --------- FIXED: Vertex input ----------
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (!hmlPipelineConfig.bindingDescriptions.empty()) {
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(hmlPipelineConfig.bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = hmlPipelineConfig.bindingDescriptions.data();
    }
    if (!hmlPipelineConfig.attributeDescriptions.empty()) {
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(hmlPipelineConfig.attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = hmlPipelineConfig.attributeDescriptions.data();
    }
    // --------- FIXED: Input assembly ----------
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    // Topology cases:
    // VK_PRIMITIVE_TOPOLOGY_POINT_LIST: points from vertices;
    // VK_PRIMITIVE_TOPOLOGY_LINE_LIST: line from every 2 vertices without reuse;
    // VK_PRIMITIVE_TOPOLOGY_LINE_STRIP: the end vertex of every line is
    // used as start vertex for the next line;
    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: triangle from every 3 vertices without reuse;
    // VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: the second and third vertex of
    // every triangle are used as first two vertices of the next triangle.
    inputAssembly.topology = hmlPipelineConfig.topology;
    // Normally, the vertices are loaded from the vertex buffer by index in
    // sequential order, but with an element buffer you can specify the indices
    // to use yourself. This allows you to perform optimizations like
    // reusing vertices. If you set the primitiveRestartEnable member
    // to VK_TRUE, then it's possible to break up lines and triangles in
    // the _STRIP topology modes by using a special index of 0xFFFF (or 0xFFFFFFFF).
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // ----------- Viewport and scissor ------------
    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) hmlPipelineConfig.extent.width; // may differ from WIDTH
    viewport.height = (float) hmlPipelineConfig.extent.height; // may differ from HEIGHT
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = hmlPipelineConfig.extent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    // --------- FIXED: Rasterizer ----------
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    // VK_TRUE => clamp whatever is beyond the near and far planes. Used in shadow maps.
    // Requires a GPU feature
    rasterizer.depthClampEnable = VK_FALSE;
    // VK_TRUE => basically disables any output to the framebuffer
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = hmlPipelineConfig.polygoneMode;
    // NOTE Thicker than 1.0 requires wideLines GPU feature
    rasterizer.lineWidth = hmlPipelineConfig.lineWidth;
    rasterizer.cullMode = hmlPipelineConfig.cullMode;
    rasterizer.frontFace = hmlPipelineConfig.frontFace;
    // rasterizer.depthBiasEnable = VK_FALSE;
    // rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    // rasterizer.depthBiasSlopeFactor = 0.0f; // Optional
    rasterizer.depthBiasEnable = VK_TRUE;
    rasterizer.depthBiasConstantFactor = 4.0f;
    rasterizer.depthBiasSlopeFactor = 1.5f;
    rasterizer.depthBiasClamp = 0.0f; // Optional

    // ----------- Multisampling -----------
    // (disable for now)
    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional

    // -------------- Depth and stencil testing ----------
    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = hmlPipelineConfig.withDepthTest;
    depthStencil.depthWriteEnable = hmlPipelineConfig.withDepthTest;
    // depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.minDepthBounds = 0.0f; // skip fragment if out-of-bounds
    depthStencil.maxDepthBounds = 1.0f; // skip fragment if out-of-bounds
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = {}; // Optional
    depthStencil.back = {}; // Optional

    // ----------- Color blending -----------
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    if (hmlPipelineConfig.withBlending) {
        // To implement opacity:
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        // colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        // colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    } else {
        colorBlendAttachment.blendEnable = VK_FALSE;
    }

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(hmlPipelineConfig.colorAttachmentCount, colorBlendAttachment);

    // Global color blending settings
    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
    colorBlending.attachmentCount = blendAttachments.size();
    colorBlending.pAttachments = blendAttachments.data();
    colorBlending.blendConstants[0] = 0.0f; // Optional
    colorBlending.blendConstants[1] = 0.0f; // Optional
    colorBlending.blendConstants[2] = 0.0f; // Optional
    colorBlending.blendConstants[3] = 0.0f; // Optional

    // --------- Dynamic state --------
    // (not used as of now). (If used, the specified parameters would be ignored
    // and we would have to specify them at each draw call)
    // VkDynamicState dynamicStates[] = {
    //     VK_DYNAMIC_STATE_VIEWPORT,
    //     VK_DYNAMIC_STATE_LINE_WIDTH
    // };
    // VkPipelineDynamicStateCreateInfo dynamicState = {};
    // dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    // dynamicState.dynamicStateCount = 2;
    // dynamicState.pDynamicStates = dynamicStates;

    // --------- Push constants --------
    // NOTE We use unified push constants (across all stages) because there
    // would be no particular benefit (e.g. with regards to max size) to have
    // separate push constants for each stage, but it would add complexity
    // to the setup.
    VkPushConstantRange pushConstant;
    pushConstant.offset = 0;
    pushConstant.size = hmlPipelineConfig.pushConstantsSizeBytes;
    pushConstant.stageFlags = hmlPipelineConfig.pushConstantsStages;

    // ------------- Pipeline layout (uniforms in shaders) ----------
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = hmlPipelineConfig.descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = hmlPipelineConfig.descriptorSetLayouts.data();
    if (hmlPipelineConfig.pushConstantsSizeBytes) {
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    }
    if (vkCreatePipelineLayout(hmlDevice->device, &pipelineLayoutInfo, nullptr, &(hmlPipeline->layout)) != VK_SUCCESS) {
        std::cerr << "::> Failed to create PipelineLayout.\n";
        return { nullptr };
    }

    // --------------------------------------------------------------------
    // ------------------ Pipeline ---------------------
    {
        const auto shadersCreated = hmlPipeline->createShaders(hmlPipelineConfig.hmlShaders);
        if (!shadersCreated){
            std::cerr << "::> Failed to create a GraphicsPipeline.\n";
            return { nullptr };
        }
        const auto& [shaderModules, shaderStages] = *shadersCreated;

        const auto tessellation = VkPipelineTessellationStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = 0,
            .patchControlPoints = hmlPipelineConfig.tessellationPatchPoints,
        };

        const auto pipelineInfo = VkGraphicsPipelineCreateInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = VK_NULL_HANDLE,
            .flags = 0,
            .stageCount = static_cast<uint32_t>(shaderStages.size()),
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pTessellationState = (hmlPipelineConfig.tessellationPatchPoints > 0) ? &tessellation : VK_NULL_HANDLE,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = nullptr, // Optional
            .layout = hmlPipeline->layout,
            .renderPass = hmlPipelineConfig.renderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE, // Optional
            .basePipelineIndex = -1, // Optional
        };
        if (vkCreateGraphicsPipelines(hmlDevice->device, VK_NULL_HANDLE, 1, &pipelineInfo,
                    nullptr, &(hmlPipeline->pipeline)) != VK_SUCCESS) {
            std::cerr << "::> Failed to create Pipeline.\n";
            return { nullptr };
        }

        // Can be destroyed only after the pipeline has been created
        for (auto shaderModule : shaderModules) {
            vkDestroyShaderModule(hmlDevice->device, shaderModule, nullptr);
        }
    }

    return hmlPipeline;
}


HmlPipeline::~HmlPipeline() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlPipeline...\n";
#endif
    vkDestroyPipelineLayout(hmlDevice->device, layout, nullptr);
    vkDestroyPipeline(hmlDevice->device, pipeline, nullptr);
}


VkPipelineShaderStageCreateInfo HmlPipeline::createShaderStageInfo(VkShaderStageFlagBits stage,
        VkShaderModule shaderModule, const std::optional<VkSpecializationInfo>& specializationInfoOpt) noexcept {
    VkPipelineShaderStageCreateInfo shaderStageInfo = {};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = stage;
    shaderStageInfo.module = shaderModule;
    shaderStageInfo.pName = "main";
    shaderStageInfo.pSpecializationInfo = specializationInfoOpt ? &(*specializationInfoOpt) : nullptr;
    return shaderStageInfo;
}


VkShaderModule HmlPipeline::createShaderModule(std::vector<char>&& code) noexcept {
    if (code.empty()) {
        std::cerr << "::> Failed to create shader module because the shader code is zero size.\n";
        return VK_NULL_HANDLE;
    }

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    // code.data satisfies the alignment requirement enfored by uint32_t
    // thanks to the default vector allocator
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(hmlDevice->device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        std::cerr << "::> Failed to create shader module.\n";
        return VK_NULL_HANDLE;
    }

    // NOTE The code buffer can be freed as of now!!!
    return shaderModule;
}


std::vector<char> HmlPipeline::readFile(const char* fileName) noexcept {
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "::> Failed to open file " << fileName << ".\n";
        return {};
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}


HmlPipeline::Id HmlPipeline::newId() noexcept {
    static Id id = 0;
    return id++;
}
