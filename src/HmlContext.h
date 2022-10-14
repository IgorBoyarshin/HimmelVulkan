#ifndef HML_CONTEXT_H
#define HML_CONTEXT_H

#include <memory>

#include "HmlWindow.h"
#include "HmlDevice.h"
#include "HmlDescriptors.h"
#include "HmlCommands.h"
#include "HmlSwapchain.h"
#include "HmlResourceManager.h"


struct HmlContext {
    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlDescriptors> hmlDescriptors;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;

    uint32_t maxFramesInFlight;


    inline uint32_t imageCount() const noexcept {
        return hmlSwapchain->imageCount();
    }

    inline uint32_t framesInFlight() const noexcept {
        return maxFramesInFlight;
    }
};


#endif
