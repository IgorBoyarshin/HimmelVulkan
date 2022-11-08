#ifndef HML_CAMERA
#define HML_CAMERA

#include <iostream>
#include <optional>
#include <memory>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "settings.h"
#include "HmlWindow.h"


struct HmlCamera {
    glm::vec3 pos   = {0, 0, 0};
    glm::vec3 dirUp = {0, 1, 0};

    // NOTE 0-degree direction is towards -Z
    float pitch = 0.0f; /* (-90;+90) */
    float yaw   = 0.0f; /* [0; 360) */

    std::optional<glm::mat4> cachedView = std::nullopt;

    static glm::vec3 calcDirForward(float pitch, float yaw) noexcept;
    glm::vec3 calcDirRight() const noexcept;

    inline void invalidateCache() noexcept { cachedView = std::nullopt; }

    inline const glm::mat4& view() noexcept {
        if (!cachedView) recacheView();
        return *cachedView;
    }

    virtual void recacheView() noexcept = 0;
    virtual inline void handleInput(const std::shared_ptr<HmlWindow>& hmlWindow, float dt, bool ignore) noexcept {}
    virtual void printStats() const noexcept = 0;

    virtual inline ~HmlCamera() noexcept {}
};

struct HmlCameraFreeFly : HmlCamera {
    void recacheView() noexcept override;
    void handleInput(const std::shared_ptr<HmlWindow>& hmlWindow, float dt, bool ignore) noexcept override;
    void printStats() const noexcept override;

    void rotateDir(float dPitch, float dYaw) noexcept;
    void forward(float length) noexcept;
    void right(float length) noexcept;
    void lift(float length) noexcept;

    virtual inline ~HmlCameraFreeFly() noexcept {}
};

// struct HmlCameraFollow : HmlCamera {
//     void handleInput(const std::shared_ptr<HmlWindow>& hmlWindow, float dt, bool ignore) noexcept override;
//     void printStats() const noexcept override;
// };


#endif
