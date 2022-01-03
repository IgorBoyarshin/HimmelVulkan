#include <memory> // shaded_ptr

#include "HmlWindow.h"
#include "HmlDevice.h"
#include "HmlSwapchain.h"


struct Himmel {
    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;


    bool init() {
        const auto windowName = "Planes game";

        hmlWindow = HmlWindow::create(1920, 1080, windowName);
        if (!hmlWindow) return false;

        hmlDevice = HmlDevice::create(hmlWindow);
        if (!hmlDevice) return false;

        hmlSwapchain = HmlSwapchain::create(hmlWindow, hmlDevice);
        if (!hmlSwapchain) return false;

        return true;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
};
