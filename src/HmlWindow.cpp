#include "HmlWindow.h"


std::unique_ptr<HmlWindow> HmlWindow::create(uint32_t width, uint32_t height, const char* name) noexcept {
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // disable OpenGL
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    HmlWindow* hmlWindow = new HmlWindow;
    hmlWindow->name = name;
    hmlWindow->window = glfwCreateWindow(width, height, name, nullptr, nullptr);

    // glfwSetWindowOpacity(hmlWindow->window, 0.7f);
    glfwSetWindowUserPointer(hmlWindow->window, hmlWindow); // to be used inside resizeCallback
    glfwSetFramebufferSizeCallback(hmlWindow->window, resizeCallback);
    glfwSetScrollCallback(hmlWindow->window, scrollCallback);
    // glfwSetInputMode(hmlWindow->window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    glfwSetInputMode(hmlWindow->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    return std::unique_ptr<HmlWindow>{ hmlWindow };
}


std::pair<int32_t, int32_t> HmlWindow::getCursor() const noexcept {
    double xpos, ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    return std::make_pair( static_cast<int32_t>(xpos), static_cast<int32_t>(ypos) );
}


std::pair<uint32_t, uint32_t> HmlWindow::getFramebufferSize() const noexcept {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    return std::make_pair( static_cast<uint32_t>(width), static_cast<uint32_t>(height) );
}


void HmlWindow::resizeCallback(GLFWwindow* window, int width, int height) noexcept {
    std::cout << ":> Resize request triggered through GLFW callback.\n";
    auto app = reinterpret_cast<HmlWindow*>(glfwGetWindowUserPointer(window));
    app->framebufferResizeRequested = true;
}


void HmlWindow::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) noexcept {
    auto app = reinterpret_cast<HmlWindow*>(glfwGetWindowUserPointer(window));
    app->scrollDiff.first  += xoffset;
    app->scrollDiff.second += yoffset;
}


HmlWindow::~HmlWindow() noexcept {
#if LOG_DESTROYS
    std::cout << ":> Destroying HmlWindow...\n";
#endif
    glfwDestroyWindow(window);
    glfwTerminate();
}


bool HmlWindow::shouldClose() const noexcept {
    return glfwWindowShouldClose(window);
}
