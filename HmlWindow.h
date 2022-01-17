#ifndef HML_WINDOW
#define HML_WINDOW

#include <utility> // pair
#include <iostream>
#include <vector>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h> // will include the vulkan header


struct HmlWindow {
    GLFWwindow* window;
    bool framebufferResizeRequested = false;
    const char* name;

    static std::unique_ptr<HmlWindow> create(uint32_t width, uint32_t height, const char* name) noexcept;
    std::pair<int32_t, int32_t> getCursor() const noexcept;
    std::pair<uint32_t, uint32_t> getFramebufferSize() const noexcept;
    static void resizeCallback(GLFWwindow* window, int width, int height) noexcept;
    ~HmlWindow() noexcept;
    bool shouldClose() const noexcept;
};

#endif
