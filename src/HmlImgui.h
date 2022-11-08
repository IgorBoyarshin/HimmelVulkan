#ifndef HML_IMGUI
#define HML_IMGUI

#include "../libs/imgui/imgui.h"
#include "../libs/imgui/imgui_impl_glfw.h"

#include "settings.h"
#include "HmlWindow.h"
#include "HmlResourceManager.h"


struct HmlImgui {
    // NOTE we use this compound approach in order to achieve such a logic that
    // we can update the stored HmlBuffer (by replacing it with a new object),
    // but all the users (shared_ptrs) of it should become aware of this change
    // without any additional logic. Without the inner smart_ptr we can just
    // replace the HmlBuffer for a *particular* shared_ptr, not for *all*.
    std::vector<std::shared_ptr<std::unique_ptr<HmlBuffer>>> vertexBuffers; // per frame in flight
    std::vector<std::shared_ptr<std::unique_ptr<HmlBuffer>>> indexBuffers; // per frame in flight

    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;

    void updateForDt(float dt) noexcept;
    void beginFrame() noexcept;
    void finilize(uint32_t currentFrameIndex, uint32_t frameInFlightIndex) noexcept;
    static std::unique_ptr<HmlImgui> create(
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlResourceManager> hmlResourceManager,
        uint32_t framesInFlight) noexcept;
    ~HmlImgui();
};


#endif
