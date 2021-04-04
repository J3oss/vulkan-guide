#include "camera.h"

#include <glm/gtx/transform.hpp>

glm::mat4 Camera::get_view()
{
  view = glm::translate(glm::mat4(1.f), pos);

  return view;
}

glm::mat4 Camera::get_projection()
{
	projection = glm::perspective(glm::radians(fovy), aspect, near, far);
	projection[1][1] *= -1;

  return projection;
}
