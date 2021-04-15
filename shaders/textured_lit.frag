#version 450

struct CameraData {
  mat4 view;
	mat4 proj;
	mat4 viewproj;
};

struct SceneData{
  vec4 fogColor;
	vec4 fogDistances;
	vec4 ambientColor;
	vec4 sunlightDirection;
	vec4 sunlightColor;
};

layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec4 outFragColor;

layout(set = 2, binding = 0) uniform sampler2D tex1;

void main()
{
  vec3 color = texture(tex1,inTexCoord).xyz;
  outFragColor = vec4(color,1.0f);
}
