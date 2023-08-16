#ifndef HML_COMPLEX_RENDERER
#define HML_COMPLEX_RENDERER

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>
#include <optional>
#include <queue>
#include <span>

#include "settings.h"
#include "util.h"
#include "HmlContext.h"
#include "HmlPipeline.h"
// #include "HmlModel.h"
#include "HmlResourceManager.h"
#include "HmlRenderPass.h"
#include "renderer.h"


// NOTE
// In theory, it is easily possible to also recreate desriptor stuff upon
// swapchain recreation by recreating the whole Renderer object.
// NOTE
struct HmlComplexRenderer : HmlDrawer {
    struct Entity {
        using Id = uint16_t;
        static const Id INVALID_ID = 0;
        Id id = INVALID_ID;
        static Id nextFreeId; // is initted in cpp

        // NOTE We prefer to access the complexModelResource via a pointer rather than by
        // id because it allows for O(1) access speed.
        std::shared_ptr<HmlComplexModelResource> complexModelResource;
        // HmlResourceManager::HmlComplexModelResource::Id modelId;
        glm::mat4 modelMatrix;
        // glm::vec3 color = { 1.0f, 1.0f, 1.0f };


        inline Entity(std::shared_ptr<HmlComplexModelResource> complexModelResource)
            : id(nextFreeId++), complexModelResource(complexModelResource) {}
        // inline Entity(std::shared_ptr<HmlComplexModelResource> complexModelResource, const glm::vec3& color)
        //     : id(nextFreeId++), complexModelResource(complexModelResource), color(color) {}
        // inline Entity(std::shared_ptr<HmlComplexModelResource> complexModelResource, const glm::mat4& modelMatrix)
        //     : id(nextFreeId++), complexModelResource(complexModelResource), modelMatrix(modelMatrix) {}
    };


    struct PushConstantRegular {
        alignas(16) glm::mat4 model;
        glm::vec4 color;
        int32_t textureIndex;
        Entity::Id id;
    };

    // struct PushConstantInstanced {
    //     int32_t textureIndex;
    //     float time;
    // };


    // enum class Mode {
    //     Regular, Shadowmap
    // } mode;
    //
    // inline void setMode(Mode newMode) noexcept { mode = newMode; }


    VkDescriptorPool descriptorPool;
    VkDescriptorSet  descriptorSet_textures_1;
    std::queue<VkDescriptorSet> descriptorSet_instances_2_queue; // FIFO
    VkDescriptorSetLayout descriptorSetLayoutTextures;
    VkDescriptorSetLayout descriptorSetLayoutInstances;

    // NOTE we don't really need to permanently store the array of them in RAM.
    struct EntityInstanceData {
        glm::mat4 model;
        inline EntityInstanceData(glm::mat4&& model) : model(model) {}
    };
    std::vector<uint32_t> instancedCounts;

    struct EntitiesData {
        int32_t textureIndex;
        std::shared_ptr<HmlComplexModelResource> complexModelResource;
        // uint32_t count;

        inline EntitiesData() {}
        inline EntitiesData(int32_t textureIndex, std::shared_ptr<HmlComplexModelResource> complexModelResource)
            : textureIndex(textureIndex), complexModelResource(complexModelResource) {}
    };

    std::unique_ptr<HmlBuffer> instancedEntitiesStorageBuffer;
    std::unordered_map<HmlComplexModelResource::Id, EntitiesData> staticEntitiesDataForModel;


    static constexpr uint32_t MAX_TEXTURES_COUNT = 32; // XXX must match the shader. NOTE can be increased (80+)
    static constexpr int32_t NO_TEXTURE_MARK = -1;

    // std::unordered_map<HmlComplexModelResource::Id, int32_t> textureIndexFor;
    // NOTE temporary; used only to pass data to createPipelines() from specifyEntities()
    std::unordered_map<HmlAttributes, std::unordered_map<HmlComplexModelResource::Id, std::vector<std::shared_ptr<Entity>>>> entitiesToRenderForModelForHmlAttributes;
    // NOTE actually used at each draw
    std::unordered_map<HmlPipeline::Id, std::unordered_map<HmlComplexModelResource::Id, std::vector<std::shared_ptr<Entity>>>> entitiesToRenderForModelForPipelineId;

    using TextureUpdateData = std::pair<std::array<VkSampler, MAX_TEXTURES_COUNT>, std::array<VkImageView, MAX_TEXTURES_COUNT>>;

    TextureUpdateData prepareTextureUpdateData(std::span<const EntitiesData> entitiesData) noexcept;

    std::vector<std::unique_ptr<HmlPipeline>> createPipelines(
        std::shared_ptr<HmlRenderPass> hmlRenderPass, const std::vector<VkDescriptorSetLayout>& descriptorSetLayouts) noexcept override;
    static std::unique_ptr<HmlComplexRenderer> create(std::shared_ptr<HmlContext> hmlContext, VkDescriptorSetLayout generalDescriptorSetLayout) noexcept;
    virtual ~HmlComplexRenderer() noexcept;
    void specifyEntitiesToRender(std::span<const std::shared_ptr<Entity>> entities) noexcept;
    // void specifyStaticEntitiesToRender(std::span<const Entity> staticEntities) noexcept;
    void updateDescriptorSetStaticTextures() noexcept;
    VkCommandBuffer draw(const HmlFrameData& frameData) noexcept override;
};

#endif
