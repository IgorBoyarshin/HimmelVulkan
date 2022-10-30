#ifndef HML_IMGUI_RENDERER
#define HML_IMGUI_RENDERER

#include <vector>
#include <memory>

#include "settings.h"
#include "HmlContext.h"
#include "renderer.h"

#include "../libs/imgui/imgui.h"
// #include "../libs/imgui/imgui_impl_glfw.h"
// #include "imgui_impl_vulkan.h"



struct HmlImguiRenderer : HmlDrawer {
    struct PushConstant {
        glm::vec2 scale;
        glm::vec2 translate;
    };


    VkDescriptorPool descriptorPool;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorSet  descriptorSet;

    std::unique_ptr<HmlImageResource> fontTexture;

    // NOTE see explanation about the data structure in HmlImgui.h
    std::shared_ptr<std::unique_ptr<HmlBuffer>> vertexBuffer;
    std::shared_ptr<std::unique_ptr<HmlBuffer>> indexBuffer;


    std::vector<std::unique_ptr<HmlPipeline>> createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept override;
    static std::unique_ptr<HmlImguiRenderer> create(
        std::shared_ptr<std::unique_ptr<HmlBuffer>> vertexBuffer,
        std::shared_ptr<std::unique_ptr<HmlBuffer>> indexBuffer,
        std::shared_ptr<HmlContext> hmlContext) noexcept;
    ~HmlImguiRenderer() noexcept;
    VkCommandBuffer draw(const HmlFrameData& frameData) noexcept override;
};


#endif
