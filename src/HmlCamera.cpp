#include "HmlCamera.h"


void HmlCameraFreeFly::setDir(float newPitch, float newYaw) noexcept {
    rotateDir(-pitch + newPitch, -yaw + newYaw);
}


void HmlCameraFreeFly::setPos(const glm::vec3& newPos) noexcept {
    pos = newPos;
    cachedView = std::nullopt;
}


void HmlCameraFreeFly::rotateDir(float dPitch, float dYaw) noexcept {
    static const float MIN_PITCH = -89.9f;
    static const float MAX_PITCH = +89.9f;
    static const float YAW_PERIOD = 360.0f;

    pitch += dPitch;
    if (pitch < MIN_PITCH) pitch = MIN_PITCH;
    if (pitch > MAX_PITCH) pitch = MAX_PITCH;

    yaw += dYaw;
    while (yaw < 0.0f)       yaw += YAW_PERIOD;
    while (YAW_PERIOD <= yaw) yaw -= YAW_PERIOD;

    cachedView = std::nullopt;
}


void HmlCameraFreeFly::forward(float length) noexcept {
    pos += length * calcDirForward(pitch, yaw);
    cachedView = std::nullopt;
}


void HmlCameraFreeFly::right(float length) noexcept {
    pos += length * calcDirRight();
    cachedView = std::nullopt;
}


void HmlCameraFreeFly::lift(float length) noexcept {
    pos += length * dirUp;
    cachedView = std::nullopt;
}


glm::vec3 HmlCamera::calcDirForward(float pitch, float yaw) noexcept {
    const auto p = glm::radians(pitch);
    const auto y = glm::radians(yaw);
    const auto cosp = glm::cos(p);
    const auto sinp = glm::sin(p);
    const auto cosy = glm::cos(y);
    const auto siny = glm::sin(y);
    return glm::normalize(glm::vec3(siny * cosp, sinp, -cosy * cosp));
}


glm::vec3 HmlCamera::calcDirRight() const noexcept {
    return glm::normalize(glm::cross(calcDirForward(pitch, yaw), dirUp));
}

std::pair<float, float> HmlCamera::getCursorDeltaAndUpdateState(std::shared_ptr<HmlWindow> hmlWindow) noexcept {
    const auto newCursor = hmlWindow->getCursor();
    if (firstTime) {
        firstTime = false;
        cursor = newCursor;
    }
    const int32_t dx = newCursor.first - cursor.first;
    const int32_t dy = newCursor.second - cursor.second;
    cursor = newCursor;
    return { dx, dy };
}


void HmlCamera::printStats() const noexcept {
    std::cout << "Camera: [" << pos.x << ";" << pos.y << ";" << pos.z << "] pitch=" << pitch << " yaw =" << yaw << "\n";
}

void HmlCameraFreeFly::recacheView() noexcept {
    // From where, to where, up
    cachedView = { glm::lookAt(pos, pos + calcDirForward(pitch, yaw), dirUp) };
}

void HmlCameraFreeFly::handleInput(const std::shared_ptr<HmlWindow>& hmlWindow, float dt, bool ignore) noexcept {
    // Keyboard
    if (!ignore) {
        constexpr float movementSpeed = 16.0f;
        constexpr float boostUp       = 10.0f;
        constexpr float boostDown     = 0.2f;

        float length = movementSpeed * dt;
        if      (glfwGetKey(hmlWindow->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) length *= boostUp;
        else if (glfwGetKey(hmlWindow->window, GLFW_KEY_LEFT_ALT)   == GLFW_PRESS) length *= boostDown;

        if (glfwGetKey(hmlWindow->window, GLFW_KEY_W)     == GLFW_PRESS) forward(length);
        if (glfwGetKey(hmlWindow->window, GLFW_KEY_S)     == GLFW_PRESS) forward(-length);
        if (glfwGetKey(hmlWindow->window, GLFW_KEY_D)     == GLFW_PRESS) right(length);
        if (glfwGetKey(hmlWindow->window, GLFW_KEY_A)     == GLFW_PRESS) right(-length);
        if (glfwGetKey(hmlWindow->window, GLFW_KEY_SPACE) == GLFW_PRESS) lift(length);
        if (glfwGetKey(hmlWindow->window, GLFW_KEY_C)     == GLFW_PRESS) lift(-length);

        static bool pPressed = false;
        if (!pPressed && glfwGetKey(hmlWindow->window, GLFW_KEY_P) == GLFW_PRESS) {
            pPressed = true;
            printStats();
        } else if (pPressed && glfwGetKey(hmlWindow->window, GLFW_KEY_P) == GLFW_RELEASE) {
            pPressed = false;
        }
    }

    // Mouse
    {
        const auto [dx, dy] = getCursorDeltaAndUpdateState(hmlWindow);
        if (!ignore) {
            constexpr float rotateSpeed = 0.05f;
            if (dx || dy) rotateDir(-dy * rotateSpeed, dx * rotateSpeed);
        }
    }
}

glm::mat4 HmlCameraFreeFly::view() noexcept {
    if (!cachedView) recacheView();
    return *cachedView;
}


void HmlCameraFollow::rotateDir(float dPitch, float dYaw) noexcept {
    static const float MIN_PITCH = -89.9f;
    static const float MAX_PITCH = +89.9f;
    static const float YAW_PERIOD = 360.0f;

    pitch += dPitch;
    if (pitch < MIN_PITCH) pitch = MIN_PITCH;
    if (pitch > MAX_PITCH) pitch = MAX_PITCH;

    yaw += dYaw;
    while (yaw < 0.0f)       yaw += YAW_PERIOD;
    while (YAW_PERIOD <= yaw) yaw -= YAW_PERIOD;
}


glm::mat4 HmlCameraFollow::view() noexcept {
    // From where, to where, up
    const auto& toWhere = followable.lock()->queryPos();
    const auto dirForward = calcDirForward(pitch, yaw);
    pos = toWhere - dirForward * distance;
    return glm::lookAt(pos, toWhere, dirUp);
}

void HmlCameraFollow::handleInput(const std::shared_ptr<HmlWindow>& hmlWindow, float dt, bool ignore) noexcept {
    // Keyboard
    if (!ignore) {
        constexpr float movementSpeed = 700.0f;
        constexpr float boostUp       = 5.0f;
        constexpr float boostDown     = 0.2f;

        float length = movementSpeed * dt;
        if      (glfwGetKey(hmlWindow->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) length *= boostUp;
        else if (glfwGetKey(hmlWindow->window, GLFW_KEY_LEFT_ALT)   == GLFW_PRESS) length *= boostDown;

        constexpr float minDistance = 0.1f;
        constexpr float maxDistance = 2000.0f;
        distance += -length * hmlWindow->scrollDiff.second;
        if (distance < minDistance) distance = minDistance;
        if (distance > maxDistance) distance = maxDistance;
    }

    // Mouse
    {
        const auto [dx, dy] = getCursorDeltaAndUpdateState(hmlWindow);
        if (!ignore) {
            constexpr float rotateSpeed = 0.05f;
            if (dx || dy) rotateDir(+dy * rotateSpeed, -dx * rotateSpeed);
        }
    }
}
