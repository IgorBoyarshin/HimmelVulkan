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


struct DebugLabel {
    VkCommandBuffer commandBuffer;

    inline DebugLabel(VkCommandBuffer commandBuffer, const char* label) noexcept
            : commandBuffer(commandBuffer) {
        assert(commandBuffer && "::> Trying to create a DebugLabel with null commandBuffer.\n");

        VkDebugUtilsLabelEXT debugLabel = {};
        debugLabel.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        debugLabel.pLabelName = label;
        vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &debugLabel);
    }

    inline ~DebugLabel() noexcept {
        end();
    }

    inline void end() noexcept {
        if (commandBuffer) [[likely]] {
            vkCmdEndDebugUtilsLabelEXT(commandBuffer);
            commandBuffer = nullptr;
        }
    }
};


}


#endif
