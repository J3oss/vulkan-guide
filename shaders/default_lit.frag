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

layout (location = 0) out vec4 outFragColor;

layout (set = 0, binding = 0) uniform GlobalData {
  CameraData camera;
  SceneData  scene;
} globalData;

void main()
{
	outFragColor = vec4(inColor + globalData.scene.ambientColor.xyz,1.0f);
}
