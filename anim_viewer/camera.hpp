#pragma once
#include "structs.hpp"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <algorithm>

// Przechowywanie stanu myszy dla sterowania kamerą
struct CameraController {
    bool is_dragging = false;
    double last_x = 0.0;
    double last_y = 0.0;
};

static CameraController g_cam_control;
static AppState* g_app_ptr = nullptr; // Wskaźnik pomocniczy dla callbacku scrolla

// Callback dla kółka myszy (Scroll)
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    // Jeśli interakcja odbywa się w okienku ImGui, ignorujemy scroll w scenie 3D
    if (ImGui::GetIO().WantCaptureMouse) return;

    if (g_app_ptr)
    {
        // Czułość przybliżania/oddalania
        float sensitivity = 20.0f;
        g_app_ptr->camera_dist -= static_cast<float>(yoffset) * sensitivity;
        g_app_ptr->camera_dist = std::clamp(g_app_ptr->camera_dist, 10.0f, 5000.0f);
    }
}

// Funkcja obsługująca interakcję myszy z kamerą (PPM)
void handle_camera_input(GLFWwindow* window, AppState& app)
{
    if (ImGui::GetIO().WantCaptureMouse)
    {
        g_cam_control.is_dragging = false;
        return;
    }

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)
    {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        if (!g_cam_control.is_dragging)
        {
            g_cam_control.is_dragging = true;
            g_cam_control.last_x = xpos;
            g_cam_control.last_y = ypos;
        }
        else
        {
            float dx = static_cast<float>(xpos - g_cam_control.last_x);
            float dy = static_cast<float>(ypos - g_cam_control.last_y);

            g_cam_control.last_x = xpos;
            g_cam_control.last_y = ypos;

            float sensitivity = 0.3f;

            app.camera_yaw += dx * sensitivity;
            app.camera_pitch -= dy * sensitivity;

            app.camera_pitch = std::clamp(app.camera_pitch, -89.0f, 89.0f);

            if (app.camera_yaw >= 360.0f) app.camera_yaw -= 360.0f;
            if (app.camera_yaw < 0.0f)   app.camera_yaw += 360.0f;
        }
    }
    else
    {
        g_cam_control.is_dragging = false;
    }
}