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
    // however all the users (shared_ptrs) of it should be aware of this change
    // without any additional logic. Without the inner smart_ptr we can just
    // replace the HmlBuffer for a *particular* shared_ptr, not for *all*.
    std::shared_ptr<std::unique_ptr<HmlBuffer>> vertexBuffer;
    std::shared_ptr<std::unique_ptr<HmlBuffer>> indexBuffer;

    std::shared_ptr<HmlWindow> hmlWindow;
    std::shared_ptr<HmlResourceManager> hmlResourceManager;

    void updateForDt(float dt) noexcept;
    void beginFrame() noexcept;
    void finilize(uint32_t currentFrame) noexcept;
    static std::unique_ptr<HmlImgui> create(
        std::shared_ptr<HmlWindow> hmlWindow,
        std::shared_ptr<HmlResourceManager> hmlResourceManager) noexcept;
    ~HmlImgui();
};


#endif
