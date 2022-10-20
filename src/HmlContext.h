#ifndef HML_CONTEXT
#define HML_CONTEXT

#include <memory>

#include "HmlWindow.h"
#include "HmlDevice.h"
#include "HmlDescriptors.h"
#include "HmlCommands.h"
#include "HmlSwapchain.h"
#include "HmlResourceManager.h"
#include "HmlQueries.h"


struct HmlContext {
    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlDevice> hmlDevice;
    std::shared_ptr<HmlDescriptors> hmlDescriptors;
    std::shared_ptr<HmlCommands> hmlCommands;
    std::shared_ptr<HmlSwapchain> hmlSwapchain;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;
    std::shared_ptr<HmlQueries> hmlQueries;

    uint32_t maxFramesInFlight = 0;

    uint32_t currentFrame = 0;


    inline uint32_t imageCount() const noexcept {
        return hmlSwapchain->imageCount();
    }

    inline uint32_t framesInFlight() const noexcept {
        return maxFramesInFlight;
    }
};


#endif
