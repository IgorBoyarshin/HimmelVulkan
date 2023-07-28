#ifndef HML_WINDOW
#define HML_WINDOW

#include <utility> // pair
#include <iostream>
#include <vector>
#include <memory>

#include "settings.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h> // will include the vulkan header


struct HmlWindow {
    GLFWwindow* window;
    bool framebufferResizeRequested = false;
    const char* name;

    std::pair<int32_t, int32_t> scrollDiff;
    inline void resetScrollState() noexcept { scrollDiff = {0, 0}; }

    static std::unique_ptr<HmlWindow> create(uint32_t width, uint32_t height, const char* name, bool fullscreen) noexcept;
    std::pair<int32_t, int32_t> getCursor() const noexcept;
    std::pair<uint32_t, uint32_t> getFramebufferSize() const noexcept;
    static void resizeCallback(GLFWwindow* window, int width, int height) noexcept;
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) noexcept;
    ~HmlWindow() noexcept;
    bool shouldClose() const noexcept;
};

#endif
