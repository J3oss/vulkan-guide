#version 460

struct ObjectData {
  mat4 model;
};

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

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 color;
layout (location = 3) in vec2 texCoord;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outTexCoord;

layout (set = 0, binding = 0) uniform GlobalData {
  CameraData camera;
  SceneData  scene;
} globalData;

layout(std140,set = 1, binding = 0) readonly buffer ObjectBuffer {
	ObjectData objects[];
} objectBuffer;

void main()
{
  mat4 modelMatrix = objectBuffer.objects[gl_BaseInstance].model;
  mat4 transformMatrix = globalData.camera.viewproj * modelMatrix;
  gl_Position = transformMatrix * vec4(position,1.0f);
  outColor = color;
  outTexCoord = texCoord;
}
