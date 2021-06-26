#pragma once

#include <asset_loader.h>

namespace assets
{
	struct Vertex_f32_PNCV
  {
		float position[4];
		float normal[4];
		float color[4];
		float uv[4];
	};

	struct Vertex_P32N8C8V16
  {
		float position[4];
		float normal[4];
		float color[4];
		float uv[4];
	};

	enum class VertexFormat : uint32_t
	{
		Unknown = 0,
		PNCV_F32, //everything at 32 bits
		P32N8C8V16 //position at 32 bits, normal at 8 bits, color at 8 bits, uvs at 16 bits float
	};

	struct MeshBounds
  {
		float origin[3];
		float radius;
		float extents[3];
	};

	struct MeshInfo
  {
		uint32_t vertexBuferSize;
		uint32_t vertexCount;
		uint32_t faceCount;

		uint32_t indexBuferSize;
		uint32_t indexCount;
		char indexSize;

		MeshBounds bounds;

		VertexFormat vertexFormat;

		std::string originalFile;
	};

	MeshInfo read_mesh_info(AssetFile* file);

	void unpack_mesh(MeshInfo* info, const char* sourcebuffer, size_t sourceSize, char* vertexBufer, char* indexBuffer);

	AssetFile pack_mesh(MeshInfo* info, char* vertexData, char* indexData);

	MeshBounds calculateBounds(Vertex_f32_PNCV* vertices, size_t count);
}
