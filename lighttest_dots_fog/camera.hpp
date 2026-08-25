#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>


// ---------------------------------------------------------------------
// Kamera
// ---------------------------------------------------------------------
struct Camera {
  glm::vec3 pos   = {0.f, 100.f, 500.f};
  float     yaw   = -90.f;
  float     pitch = 0.f;
  float     speed = 300.f;

  glm::vec3 front() const {
    return glm::normalize(glm::vec3(
      cos(glm::radians(yaw))*cos(glm::radians(pitch)),
      sin(glm::radians(pitch)),
      sin(glm::radians(yaw))*cos(glm::radians(pitch))));
    }
  glm::vec3 right() const { return glm::normalize(glm::cross(front(), {0,1,0})); }
  glm::mat4 view() const { return glm::lookAt(pos, pos+front(), glm::vec3(0,1,0)); }
  };