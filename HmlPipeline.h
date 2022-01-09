#ifndef HML_PIPELINE
#define HML_PIPELINE

#include <optional>
#include <fstream>

#include "HmlDevice.h"


struct HmlShaders {
    const char* vertex;
    const char* fragment;
    // const char* geometry;
    // const char* compute;

    // TODO add compatibility check
    HmlShaders& addVertex(const char* fileNameSpv) {
        vertex = fileNameSpv;
        return *this;
    }

    // TODO add compatibility check
    HmlShaders& addFragment(const char* fileNameSpv) {
        fragment = fileNameSpv;
        return *this;
    }
};


struct HmlGraphicsPipelineConfig {
    std::vector<VkVertexInputBindingDescription>   bindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    VkPrimitiveTopology topology;
    HmlShaders hmlShaders;
    VkRenderPass renderPass;
    VkExtent2D swapchainExtent;
    // VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
    // VK_POLYGON_MODE_LINE: polygon edges are drawn as lines. REQUIRES GPU FEATURE
    // VK_POLYGON_MODE_POINT: polygon vertices are drawn as points. REQUIRES GPU FEATURE
    VkPolygonMode polygoneMode;
    // VK_CULL_MODE_BACK_BIT
    // VK_CULL_MODE_NONE
    VkCullModeFlags cullMode;
    // VK_FRONT_FACE_COUNTER_CLOCKWISE
    // VK_FRONT_FACE_CLOCKWISE
    VkFrontFace frontFace;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

    // TODO Later will probably be an optional
    VkShaderStageFlags pushConstantsStages;
    uint32_t pushConstantsSizeBytes;
};


struct HmlPipeline {
    VkPipelineLayout layout;
    VkPipeline pipeline;

    VkShaderStageFlags pushConstantsStages;

    std::shared_ptr<HmlDevice> hmlDevice;


    static std::unique_ptr<HmlPipeline> createGraphics(std::shared_ptr<HmlDevice> hmlDevice,
            HmlGraphicsPipelineConfig&& hmlPipelineConfig) {
        auto hmlPipeline = std::make_unique<HmlPipeline>();
        hmlPipeline->pushConstantsStages = hmlPipelineConfig.pushConstantsStages;
        hmlPipeline->hmlDevice = hmlDevice;

        // --------- FIXED: Vertex input ----------
        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(hmlPipelineConfig.bindingDescriptions.size());
        vertexInputInfo.pVertexBindingDescriptions = hmlPipelineConfig.bindingDescriptions.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(hmlPipelineConfig.attributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = hmlPipelineConfig.attributeDescriptions.data();

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
        viewport.width = (float) hmlPipelineConfig.swapchainExtent.width; // may differ from WIDTH
        viewport.height = (float) hmlPipelineConfig.swapchainExtent.height; // may differ from HEIGHT
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = hmlPipelineConfig.swapchainExtent;

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
        // Thicker than 1.0 requires wideLines GPU feature
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = hmlPipelineConfig.cullMode;
        rasterizer.frontFace = hmlPipelineConfig.frontFace;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

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
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
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
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
        // To implement opacity:
        // colorBlendAttachment.blendEnable = VK_TRUE;
        // colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        // colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        // colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        // colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        // colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        // colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        // You can find all of the possible operations in the VkBlendFactor and
        // VkBlendOp enumerations in the specification.

        // Global color blending settings
        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
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
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
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

            VkGraphicsPipelineCreateInfo pipelineInfo = {};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = shaderStages.size();
            pipelineInfo.pStages = shaderStages.data();
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = &depthStencil;
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = nullptr; // Optional
            pipelineInfo.layout = hmlPipeline->layout;
            pipelineInfo.renderPass = hmlPipelineConfig.renderPass;
            pipelineInfo.subpass = 0;
            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
            pipelineInfo.basePipelineIndex = -1; // Optional
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

    ~HmlPipeline() {
        std::cout << ":> Destroying HmlPipeline...\n";
        vkDestroyPipelineLayout(hmlDevice->device, layout, nullptr);
        vkDestroyPipeline(hmlDevice->device, pipeline, nullptr);
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
    private:


    // TODO try auto
    using HmlShadersCreated = std::optional<std::pair<std::vector<VkShaderModule>, std::vector<VkPipelineShaderStageCreateInfo>>>;

    HmlShadersCreated createShaders(const HmlShaders& hmlShaders) {
        std::vector<VkShaderModule> shaderModules;
        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

        if (hmlShaders.vertex) {
            if (auto shaderModule = createShaderModule(readFile(hmlShaders.vertex)); shaderModule) {
                shaderModules.push_back(shaderModule);
                shaderStages.push_back(createShaderStageInfo(VK_SHADER_STAGE_VERTEX_BIT, shaderModule));
            } else return std::nullopt;
        }
        if (hmlShaders.fragment) {
            if (auto shaderModule = createShaderModule(readFile(hmlShaders.fragment)); shaderModule) {
                shaderModules.push_back(shaderModule);
                shaderStages.push_back(createShaderStageInfo(VK_SHADER_STAGE_FRAGMENT_BIT, shaderModule));
            } else return std::nullopt;
        }

        return { std::make_pair(shaderModules, shaderStages) };
    }


    static VkPipelineShaderStageCreateInfo createShaderStageInfo(VkShaderStageFlagBits stage, VkShaderModule shaderModule) {
        VkPipelineShaderStageCreateInfo shaderStageInfo = {};
        shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStageInfo.stage = stage;
        shaderStageInfo.module = shaderModule;
        shaderStageInfo.pName = "main";
        // pSpecializationInfo member can be used to set constants, which is
        // more efficient than doing so at render time
        return shaderStageInfo;
    }


    VkShaderModule createShaderModule(std::vector<char>&& code) {
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


    static std::vector<char> readFile(const char* fileName) {
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
};

#endif
