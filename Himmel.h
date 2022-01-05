#include <memory> // shaded_ptr

#include "HmlWindow.h"
#include "HmlDevice.h"
#include "HmlCommands.h"
#include "HmlSwapchain.h"
#include "HmlResourceManager.h"
#include "HmlRenderer.h"


struct Himmel {
    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;
    std::shared_ptr<HmlRenderer> hmlRenderer;


    bool init() {
        const char* windowName = "Planes game";

        hmlWindow = HmlWindow::create(1920, 1080, windowName);
        if (!hmlWindow) return false;

        hmlDevice = HmlDevice::create(hmlWindow);
        if (!hmlDevice) return false;

        hmlCommands = HmlCommands::create(hmlDevice);
        if (!hmlCommands) return false;

        hmlResourceManager = HmlResourceManager::create(hmlDevice, hmlCommands);
        if (!hmlResourceManager) return false;

        hmlSwapchain = HmlSwapchain::create(hmlWindow, hmlDevice, hmlResourceManager);
        if (!hmlSwapchain) return false;

        hmlRenderer = HmlRenderer::createSimpleRenderer(hmlDevice, hmlSwapchain, hmlResourceManager);
        if (!hmlRenderer) return false;

        return true;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
};
