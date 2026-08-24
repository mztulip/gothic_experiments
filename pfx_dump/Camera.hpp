#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include "imgui.h"
#include "PfxParams.hpp"

struct Camera {
    glm::vec3 targetPos = {0.f, 0.f, 0.f};
    float distance  = 300.f;
    float yaw       = -90.f;
    float pitch     = 15.f;
    float speed     = 300.f;

    glm::vec3 getPosition() const {
        float radYaw   = glm::radians(yaw);
        float radPitch = glm::radians(pitch);

        glm::vec3 dir;
        dir.x = std::cos(radYaw) * std::cos(radPitch);
        dir.y = std::sin(radPitch);
        dir.z = std::sin(radYaw) * std::cos(radPitch);

        return targetPos - dir * distance;
    }

    glm::vec3 front() const { return glm::normalize(targetPos - getPosition()); }
    glm::vec3 right() const { return glm::normalize(glm::cross(front(), glm::vec3(0, 1, 0))); }
    glm::mat4 view() const  { return glm::lookAt(getPosition(), targetPos, glm::vec3(0, 1, 0)); }
};

namespace CameraController {
    inline Camera g_cam;
    inline bool g_keys[512] = {};
    inline double g_lastX = 400, g_lastY = 300;
    inline bool g_firstMouse = true;
    inline bool g_lookActive = false;
    inline bool g_resetRequested = false;

    inline void scrollCallback(GLFWwindow*, double, double yoffset) {
        if (ImGui::GetIO().WantCaptureMouse) return;
        float zoomFactor = 1.15f;
        if (yoffset > 0) g_cam.distance /= zoomFactor;
        else if (yoffset < 0) g_cam.distance *= zoomFactor;
        g_cam.distance = std::clamp(g_cam.distance, 5.f, 50000.f);
    }

    inline void keyCallback(GLFWwindow* w, int key, int, int action, int) {
        if (action == GLFW_PRESS || action == GLFW_RELEASE) g_keys[key] = (action == GLFW_PRESS);
        if (action != GLFW_PRESS) return;
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(w, GLFW_TRUE);
        if (key == GLFW_KEY_R) g_resetRequested = true;
    }

    inline void mouseButtonCallback(GLFWwindow* w, int button, int action, int) {
        if (button != GLFW_MOUSE_BUTTON_LEFT) return;
        if (action == GLFW_PRESS && !ImGui::GetIO().WantCaptureMouse) {
            g_lookActive = true;
            g_firstMouse = true;
            glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else if (action == GLFW_RELEASE) {
            g_lookActive = false;
            glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    inline void cursorCallback(GLFWwindow*, double x, double y) {
        if (!g_lookActive) { g_lastX = x; g_lastY = y; return; }
        if (g_firstMouse) { g_lastX = x; g_lastY = y; g_firstMouse = false; }
        double dx = x - g_lastX, dy = g_lastY - y;
        g_lastX = x; g_lastY = y;
        const float sens = 0.15f;
        g_cam.yaw   += float(dx) * sens;
        g_cam.pitch += float(dy) * sens;
        g_cam.pitch  = std::clamp(g_cam.pitch, -89.f, 89.f);
    }

    inline void updateMovement(float dt) {
        if (ImGui::GetIO().WantCaptureKeyboard) return;
        float speed = g_cam.speed * dt * (g_keys[GLFW_KEY_LEFT_SHIFT] ? 3.f : 1.f);
        if (g_keys[GLFW_KEY_W]) g_cam.targetPos += g_cam.front() * speed;
        if (g_keys[GLFW_KEY_S]) g_cam.targetPos -= g_cam.front() * speed;
        if (g_keys[GLFW_KEY_A]) g_cam.targetPos -= g_cam.right() * speed;
        if (g_keys[GLFW_KEY_D]) g_cam.targetPos += g_cam.right() * speed;
        if (g_keys[GLFW_KEY_SPACE])        g_cam.targetPos.y += speed;
        if (g_keys[GLFW_KEY_LEFT_CONTROL]) g_cam.targetPos.y -= speed;
    }
}