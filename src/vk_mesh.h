#pragma once

#include <vk_types.h>
#include <vector>
#include <glm/glm.hpp>

#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/DefaultLogger.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

struct VertexInputDescription {
  std::vector<VkVertexInputBindingDescription>   bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;

  VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex {
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec3 color;
  glm::vec2 uv;

  static VertexInputDescription get_vertex_description();
};

struct Mesh {
  std::vector<Vertex> vertices;

  Buffer verticesBuffer;

  bool load_from_obj(const char* filename);

  bool assimp_load(const char* filename);
};
