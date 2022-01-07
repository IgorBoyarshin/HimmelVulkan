#ifndef HML_CAMERA
#define HML_CAMERA

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


struct HmlCamera {
    HmlCamera() { recacheView(); }
    // HmlCamera(const glm::vec3& pos, const glm::vec3& lookAtPos)
    //     : pos(pos), dirForward(lookAtPos - pos), dirUp({ 0.0f, 1.0f, 0.0f }) { recacheView(); }
    HmlCamera(const glm::vec3& pos)
        : pos(pos), dirUp({ 0.0f, 1.0f, 0.0f }) { recacheView(); }

    const glm::mat4& view() const noexcept {
        return cachedView;
    }

    void rotateDir(float dPitch, float dYaw) {
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

    void forward(float length) {
        pos += length * calcDirForward();
        recacheView();
    }

    void right(float length) {
        pos += length * calcDirRight();
        recacheView();
    }

    void lift(float length) {
        pos += length * dirUp;
        recacheView();
    }


    private:
    glm::vec3 pos;
    glm::vec3 dirUp;

    // NOTE 0-degree direction is towards -Z
    float pitch = 0.0f; /* (-90;+90) */
    float yaw   = 0.0f; /* [0; 360) */

    glm::mat4 cachedView;

    void recacheView() {
        cachedView = glm::lookAt(pos, pos + calcDirForward(), dirUp);
        // const auto f = calcDirForward();
        // std::cout << "Pos = " << pos.x << "," << pos.y << "," << pos.z
        //     << "; For = " << f.x << "," << f.y << "," << f.z << '\n';
    }

    glm::vec3 calcDirForward() const noexcept {
        const auto p = glm::radians(pitch);
        const auto y = glm::radians(yaw);
        const auto cosp = glm::cos(p);
        const auto sinp = glm::sin(p);
        const auto cosy = glm::cos(y);
        const auto siny = glm::sin(y);
        return glm::normalize(glm::vec3(siny * cosp, sinp, -cosy * cosp));
    }

    glm::vec3 calcDirRight() const noexcept {
        return glm::normalize(glm::cross(calcDirForward(), dirUp));
    }
};

#endif
