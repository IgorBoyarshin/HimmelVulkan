#ifndef HML_RENDER_PASS
#define HML_RENDER_PASS

#include <optional>
#include <vector>
#include <memory>
#include <algorithm>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "settings.h"
#include "HmlDevice.h"
#include "HmlCommands.h"
#include "HmlResourceManager.h"


struct HmlRenderPass {
    struct LoadColor {
        enum class Type {
            Clear, Load, DontCare
        } type;

        std::optional<VkClearColorValue> clearColor;

        inline static LoadColor Clear(VkClearColorValue clearColor) noexcept {
            return LoadColor(Type::Clear, { clearColor });
        }
        inline static LoadColor Load() noexcept {
            return LoadColor(Type::Load, std::nullopt);
        }
        inline static LoadColor DontCare() noexcept {
            return LoadColor(Type::DontCare, std::nullopt);
        }

        private:
        inline LoadColor(Type type, std::optional<VkClearColorValue> clearColor) noexcept : type(type), clearColor(std::move(clearColor)) {}
    };

    struct ColorAttachment {
        std::vector<std::shared_ptr<HmlImageResource>> imageResources;
        LoadColor loadColor;
        VkImageLayout preLayout;
        VkImageLayout postLayout;
    };
    struct DepthStencilAttachment {
        std::vector<std::shared_ptr<HmlImageResource>> imageResources;
        std::optional<VkClearDepthStencilValue> clearColor;
        bool store;
        VkImageLayout preLayout;
        VkImageLayout postLayout;
    };


    struct Config {
        std::vector<ColorAttachment> colorAttachments;
        std::optional<DepthStencilAttachment> depthStencilAttachment;
        VkExtent2D extent;
        std::vector<VkSubpassDependency> subpassDependencies;
    };


    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;

    VkRenderPass renderPass;
    std::vector<VkFramebuffer> framebuffers;

    // Both color and depth
    // The order (indexing) must match the attachments!
    std::vector<VkClearValue> clearValues;

    VkExtent2D extent;


    uint32_t colorAttachmentCount;


    void begin(VkCommandBuffer commandBuffer, uint32_t imageIndex) const noexcept;
    void end(VkCommandBuffer commandBuffer) const noexcept;
    static std::unique_ptr<HmlRenderPass> create(
            std::shared_ptr<HmlDevice> hmlDevice,
            std::shared_ptr<HmlCommands> hmlCommands,
            Config&& config) noexcept;
    VkFramebuffer createFramebuffer(const std::vector<VkImageView>& colorAndDepthImageViews) const noexcept;

    inline size_t imageCount() const noexcept {
        return framebuffers.size();
    }

    ~HmlRenderPass() noexcept;
};

#endif
