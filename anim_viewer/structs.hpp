#pragma once

#include <vector>

#include <epoxy/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>




struct RenderMesh 
{
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLsizei index_count = 0;
};

struct AppState 
{
    std::vector<RenderMesh> meshes;
    float camera_dist = 300.0f;
    float camera_pitch = 20.0f;
    float camera_yaw = 45.0f;
    glm::vec3 model_center{0.0f};
};