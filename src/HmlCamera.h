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

struct HmlCameraFreeFly;
struct HmlCameraFollow;

struct HmlCamera {
    enum class Type {
        FreeFly, Follow
    };
    const Type type;
    inline bool isFreeFly() const noexcept { return type == Type::FreeFly; }
    inline bool isFollow() const noexcept { return type == Type::Follow; }
    virtual inline HmlCameraFreeFly* asFreeFly() noexcept { return nullptr; }
    virtual inline HmlCameraFollow* asFollow() noexcept { return nullptr; }

    inline HmlCamera(Type type) noexcept : type(type) {}

    glm::vec3 pos   = {0, 0, 0};
    glm::vec3 dirUp = {0, 1, 0};

    // NOTE 0-degree direction is towards -Z
    float pitch = 0.0f; /* (-90;+90) */
    float yaw   = 0.0f; /* [0; 360) */

    static glm::vec3 calcDirForward(float pitch, float yaw) noexcept;
    glm::vec3 calcDirRight() const noexcept;
    std::pair<float, float> getCursorDeltaAndUpdateState(std::shared_ptr<HmlWindow> hmlWindow) noexcept;
    std::pair<int32_t, int32_t> cursor;
    bool firstTime = true;

    // Must provide an override implementation
    virtual glm::mat4 view() noexcept = 0;

    // Probably will provide an override implementation
    virtual inline void handleInput(const std::shared_ptr<HmlWindow>& hmlWindow, float dt, bool ignore) noexcept {}

    // Probably won't provide and override implementation
    virtual void printStats() const noexcept;
    virtual inline ~HmlCamera() noexcept {}
};

struct HmlCameraFreeFly : HmlCamera {
    inline HmlCameraFreeFly(const glm::vec3& initialPos) noexcept : HmlCamera(Type::FreeFly) { pos = initialPos; }
    inline HmlCameraFreeFly* asFreeFly() noexcept override { return dynamic_cast<HmlCameraFreeFly*>(this); }

    // Will override
    glm::mat4 view() noexcept override;
    void handleInput(const std::shared_ptr<HmlWindow>& hmlWindow, float dt, bool ignore) noexcept override;

    std::optional<glm::mat4> cachedView = std::nullopt;
    inline void invalidateCache() noexcept { cachedView = std::nullopt; }
    void recacheView() noexcept;

    void setDir(float pitch, float yaw) noexcept;
    void setPos(const glm::vec3& newPos) noexcept;
    void rotateDir(float dPitch, float dYaw) noexcept;
    void forward(float length) noexcept;
    void right(float length) noexcept;
    void lift(float length) noexcept;
};

struct HmlCameraFollow : HmlCamera {
    inline HmlCameraFollow(float distance) noexcept : HmlCamera(Type::Follow), distance(distance) {}
    inline HmlCameraFollow* asFollow() noexcept override { return dynamic_cast<HmlCameraFollow*>(this); }

    // Will override
    glm::mat4 view() noexcept override;
    void handleInput(const std::shared_ptr<HmlWindow>& hmlWindow, float dt, bool ignore) noexcept override;

    void rotateDir(float dPitch, float dYaw) noexcept;

    struct Followable {
        virtual glm::vec3 queryPos() const noexcept = 0;
    };
    std::weak_ptr<Followable> followable;

    float distance;

    inline void target(std::weak_ptr<Followable> newFollowable) noexcept {
        followable = newFollowable;
    }
};


#endif
