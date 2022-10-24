#ifndef HML_CAMERA
#define HML_CAMERA

#include <iostream>
#include <optional>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "settings.h"


struct HmlCamera {
    inline HmlCamera() {
        recacheView();
    }

    inline HmlCamera(const glm::vec3& pos) : pos(pos), dirUp({ 0.0f, 1.0f, 0.0f }) {
        recacheView();
    }

    inline const glm::mat4& view() noexcept {
        if (!cachedView) recacheView();
        return *cachedView;
    }

    inline glm::vec3 getPos() const noexcept {
        return pos;
    }

    void rotateDir(float dPitch, float dYaw) noexcept;
    void forward(float length) noexcept;
    void right(float length) noexcept;
    void lift(float length) noexcept;

    void printStats() const noexcept;

    inline void resetChanged() noexcept {
        positionChanged = false;
        rotationChanged = false;
    }

    inline bool positionHasChanged() const noexcept { return positionChanged; }
    inline bool rotationHasChanged() const noexcept { return rotationChanged; }
    inline bool somethingHasChanged() const noexcept { return rotationChanged || positionChanged; }

    private:
    glm::vec3 pos;
    glm::vec3 dirUp;

    // Just some flags for outside usage not to duplicate the checking logic in multiple places.
    // Must be manually reset by resetChanged().
    bool positionChanged = true;
    bool rotationChanged = true;

    // NOTE 0-degree direction is towards -Z
    float pitch = 0.0f; /* (-90;+90) */
    float yaw   = 0.0f; /* [0; 360) */

    std::optional<glm::mat4> cachedView = std::nullopt;

    void recacheView() noexcept;
    glm::vec3 calcDirForward() const noexcept;
    glm::vec3 calcDirRight() const noexcept;
};

#endif
