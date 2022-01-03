#ifndef HML_WINDOW
#define HML_WINDOW

#include <utility> // pair
#include <memory> // unique_ptr

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h> // will include the vulkan header


struct HmlWindow {
    GLFWwindow* window;
    bool framebufferResizeRequested = false;
    const char* name;


    static std::unique_ptr<HmlWindow> create(uint32_t width, uint32_t height, const char* name) {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // disable OpenGL
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        HmlWindow* hmlWindow = new HmlWindow;
        hmlWindow->name = name;
        hmlWindow->window = glfwCreateWindow(width, height, name, nullptr, nullptr);

        // glfwSetWindowOpacity(window, 0.7f);
        glfwSetWindowUserPointer(hmlWindow->window, hmlWindow); // to be used inside resizeCallback
        glfwSetFramebufferSizeCallback(hmlWindow->window, resizeCallback);
        // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

        return std::unique_ptr<HmlWindow>{ hmlWindow };
    }


    std::pair<float, float> getCursor() noexcept {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);
        return std::make_pair( static_cast<float>(xpos), static_cast<float>(ypos) );
    }


    static void resizeCallback(GLFWwindow* window, int width, int height) {
        // std::cout << ":> Resize request triggered through GLFW callback.\n";
        auto app = reinterpret_cast<HmlWindow*>(glfwGetWindowUserPointer(window));
        app->framebufferResizeRequested = true;
    }


    ~HmlWindow() {
        std::cout << ":> Destroying HmlWindow...\n";
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

#endif
