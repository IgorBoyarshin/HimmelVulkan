#include "HmlCamera.h"


void HmlCamera::rotateDir(float dPitch, float dYaw) noexcept {
    static const float rotateSpeed = 0.05f;
    static const float MIN_PITCH = -89.0f;
    static const float MAX_PITCH = +89.0f;
    static const float YAW_PERIOD = 360.0f;

    pitch += dPitch * rotateSpeed;
    if (pitch < MIN_PITCH) pitch = MIN_PITCH;
    if (pitch > MAX_PITCH) pitch = MAX_PITCH;

    yaw += dYaw * rotateSpeed;
    while (yaw < 0.0f)       yaw += YAW_PERIOD;
    while (YAW_PERIOD <= yaw) yaw -= YAW_PERIOD;

    // std::cout << "Pitch = " << pitch << "; Yaw = " << yaw << '\n';

    recacheView();
}


void HmlCamera::forward(float length) noexcept {
    pos += length * calcDirForward();
    recacheView();
}


void HmlCamera::right(float length) noexcept {
    pos += length * calcDirRight();
    recacheView();
}


void HmlCamera::lift(float length) noexcept {
    pos += length * dirUp;
    recacheView();
}


void HmlCamera::recacheView() noexcept {
    cachedView = glm::lookAt(pos, pos + calcDirForward(), dirUp);
    // const auto f = calcDirForward();
    // std::cout << "Pos = " << pos.x << "," << pos.y << "," << pos.z
    //     << "; For = " << f.x << "," << f.y << "," << f.z << '\n';
}


glm::vec3 HmlCamera::calcDirForward() const noexcept {
    const auto p = glm::radians(pitch);
    const auto y = glm::radians(yaw);
    const auto cosp = glm::cos(p);
    const auto sinp = glm::sin(p);
    const auto cosy = glm::cos(y);
    const auto siny = glm::sin(y);
    return glm::normalize(glm::vec3(siny * cosp, sinp, -cosy * cosp));
}


glm::vec3 HmlCamera::calcDirRight() const noexcept {
    return glm::normalize(glm::cross(calcDirForward(), dirUp));
}
