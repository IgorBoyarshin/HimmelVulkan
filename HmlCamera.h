#ifndef HML_CAMERA
#define HML_CAMERA

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>


struct HmlCamera {
    inline HmlCamera() {
        recacheView();
    }

    inline HmlCamera(const glm::vec3& pos) : pos(pos), dirUp({ 0.0f, 1.0f, 0.0f }) {
        recacheView();
    }

    inline const glm::mat4& view() const noexcept {
        return cachedView;
    }

    void rotateDir(float dPitch, float dYaw) noexcept;
    void forward(float length) noexcept;
    void right(float length) noexcept;
    void lift(float length) noexcept;


    private:
    glm::vec3 pos;
    glm::vec3 dirUp;

    // NOTE 0-degree direction is towards -Z
    float pitch = 0.0f; /* (-90;+90) */
    float yaw   = 0.0f; /* [0; 360) */

    glm::mat4 cachedView;

    void recacheView() noexcept;
    glm::vec3 calcDirForward() const noexcept;
    glm::vec3 calcDirRight() const noexcept;
};

#endif
