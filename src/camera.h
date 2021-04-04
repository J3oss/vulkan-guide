#pragma once

#include <glm/glm.hpp>

class Camera
{
public:
  	glm::vec3 pos = { 0.f,-6.f,-10.f };

    float fovy = 70.0f;
    float aspect = 1700.f / 900.f;
    float near = 0.1f;
    float far = 200.0f;

    glm::mat4 get_view();
    glm::mat4 get_projection();

private:
  glm::mat4 view;
  glm::mat4 projection;
};
