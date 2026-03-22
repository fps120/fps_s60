#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace CS {

class Camera {
public:
    glm::vec3 pos    = {0,1.62f,0};
    float     yaw    = 0.f;      // radians, horizontal (0=+Z)
    float     pitch  = 0.f;      // radians, vertical (clamped ±88°)
    float     fovY   = 90.f;     // vertical FOV in degrees (CS default)

    // ── view matrix ──────────────────────────────────────────────────────────
    glm::mat4 viewMatrix() const {
        return glm::lookAt(pos, pos + forward(), up());
    }

    // ── projection matrix ────────────────────────────────────────────────────
    glm::mat4 projMatrix(float aspect, float zNear=0.05f, float zFar=500.f) const {
        return glm::perspective(glm::radians(fovY), aspect, zNear, zFar);
    }

    // ── viewmodel projection (separate frustum to avoid wall-clip) ───────────
    glm::mat4 weaponProjMatrix(float aspect) const {
        return glm::perspective(glm::radians(fovY * 0.85f), aspect, 0.01f, 10.f);
    }

    // ── direction vectors ────────────────────────────────────────────────────
    glm::vec3 forward() const {
        return glm::vec3(
            cosf(pitch) * sinf(yaw),
            sinf(pitch),
            cosf(pitch) * cosf(yaw)
        );
    }

    glm::vec3 right() const {
        return glm::normalize(glm::cross(forward(), glm::vec3(0,1,0)));
    }

    glm::vec3 up() const {
        return glm::normalize(glm::cross(right(), forward()));
    }

    // Horizontal forward (for movement - ignores pitch)
    glm::vec3 flatForward() const {
        return glm::normalize(glm::vec3(sinf(yaw), 0, cosf(yaw)));
    }

    glm::vec3 flatRight() const {
        return glm::normalize(glm::cross(flatForward(), glm::vec3(0,1,0)));
    }

    // ── mouse look ────────────────────────────────────────────────────────────
    void rotate(float dYaw, float dPitch, float sensitivity = 0.002f) {
        yaw   += dYaw   * sensitivity;
        pitch -= dPitch * sensitivity;  // invert Y
        // Clamp pitch to ±88°
        constexpr float MAX_PITCH = glm::radians(88.f);
        pitch = std::clamp(pitch, -MAX_PITCH, MAX_PITCH);
        // Wrap yaw
        if (yaw >  glm::pi<float>()) yaw -= glm::two_pi<float>();
        if (yaw < -glm::pi<float>()) yaw += glm::two_pi<float>();
    }

    // ── weapon sway animation ─────────────────────────────────────────────────
    glm::mat4 weaponViewMatrix(float swayT) const {
        // Weapon rendered in its own coordinate space offset from view origin
        glm::mat4 base = glm::mat4(1.0f);
        // Bob
        float bobX = sinf(swayT * 2.0f) * 0.008f;
        float bobY = cosf(swayT * 4.0f) * 0.004f;
        base = glm::translate(base, glm::vec3(bobX, bobY, 0));
        return base;
    }
};

} // namespace CS
