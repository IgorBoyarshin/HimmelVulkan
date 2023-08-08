#ifndef HML_UTIL
#define HML_UTIL

#include <random>

#include <vulkan/vulkan.hpp>

#include "settings.h"


namespace hml {


// float, double, long double
// [low, high)
template<typename T = float>
inline T getRandomUniformFloat(T low, T high) noexcept {
    // static std::random_device rd;
    // static std::mt19937 e2(rd());
    static std::seed_seq seed{1, 2, 3, 300};
    static std::mt19937 e2(seed);
    std::uniform_real_distribution<T> dist(low, high);

    return dist(e2);
}


// TODO can make template specialization for command buffer and queue
struct DebugLabel {
    inline DebugLabel(VkCommandBuffer commandBuffer, const char* label) noexcept
            : commandBuffer(commandBuffer) {
        assert(commandBuffer && "::> Trying to create a DebugLabel with null commandBuffer.\n");

        VkDebugUtilsLabelEXT debugLabel = {};
        debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        debugLabel.pLabelName = label;
        vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);
    }

    inline DebugLabel(VkQueue queue, const char* label) noexcept
            : queue(queue) {
        assert(queue && "::> Trying to create a DebugLabel with null queue.\n");

        VkDebugUtilsLabelEXT debugLabel = {};
        debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        debugLabel.pLabelName = label;
        vkQueueBeginDebugUtilsLabelEXT(queue, &debugLabel);
    }

    inline ~DebugLabel() noexcept {
        end();
    }

    inline void insert(const char* label) noexcept {
        if (commandBuffer) {
            VkDebugUtilsLabelEXT debugLabel = {};
            debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            debugLabel.pLabelName = label;
            vkCmdInsertDebugUtilsLabelEXT(commandBuffer, &debugLabel);
        } else if (queue) {
            VkDebugUtilsLabelEXT debugLabel = {};
            debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            debugLabel.pLabelName = label;
            vkQueueInsertDebugUtilsLabelEXT(queue, &debugLabel);
        } else assert(false);
    }

    // NOTE may be called multiple times (for explicit end() and then for destructor)
    inline void end() noexcept {
        if (commandBuffer) {
            vkCmdEndDebugUtilsLabelEXT(commandBuffer);
            commandBuffer = nullptr;
        } else if (queue) {
            vkQueueEndDebugUtilsLabelEXT(queue);
            queue = nullptr;
        }
    }

private:
    VkCommandBuffer commandBuffer = nullptr;
    VkQueue         queue         = nullptr;
};


}


#endif
