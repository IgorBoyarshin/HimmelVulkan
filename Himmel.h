#include <memory> // shaded_ptr

#include "HmlWindow.h"
#include "HmlDevice.h"


struct Himmel {
    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;


    bool init() {
        const auto windowName = "Planes game";
        hmlWindow = HmlWindow::create(1920, 1080, windowName);
        hmlDevice = HmlDevice::create(hmlWindow);

        return true;
    }
    // ========================================================================
    // ========================================================================
    // ========================================================================
};
