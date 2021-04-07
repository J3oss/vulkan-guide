#include <vk_mesh.h>

#include <iostream>
#include <tiny_obj_loader.h>

#include <asset_loader.h>
#include <mesh_asset.h>

#include <cstdint>

VertexInputDescription Vertex::get_vertex_description()
{
	VertexInputDescription description;

	VkVertexInputBindingDescription mainBinding = {};
	mainBinding.binding = 0;
	mainBinding.stride = sizeof(Vertex);
	mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	description.bindings.push_back(mainBinding);

	VkVertexInputAttributeDescription positionAttribute = {};
	positionAttribute.binding = 0;
	positionAttribute.location = 0;
	positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	positionAttribute.offset = offsetof(Vertex, position);

	VkVertexInputAttributeDescription normalAttribute = {};
	normalAttribute.binding = 0;
	normalAttribute.location = 1;
	normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	normalAttribute.offset = offsetof(Vertex, normal);

	VkVertexInputAttributeDescription colorAttribute = {};
	colorAttribute.binding = 0;
	colorAttribute.location = 2;
	colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	colorAttribute.offset = offsetof(Vertex, color);

	VkVertexInputAttributeDescription uvAttribute = {};
	uvAttribute.binding = 0;
	uvAttribute.location = 3;
	uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
	uvAttribute.offset = offsetof(Vertex, uv);

	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(colorAttribute);
	description.attributes.push_back(uvAttribute);

	return description;
}

bool Mesh::load_mesh(const char* filename)
{
	assets::AssetFile file;
	assets::MeshInfo info;

	bool loaded = assets::load_binaryfile(filename, file);
	if (!loaded)
	{
		std::cout << "Error when loading mesh " << filename << std::endl;;
		return false;
	}

	info = read_mesh_info(&file);

	//TODO when using index drawing change this
	std::vector<char> vertexBuffer;
	std::vector<char> indexBuffer;

	vertexBuffer.resize(info.vertexBuferSize);
	indexBuffer.resize(info.indexBuferSize);

	assets::unpack_mesh(&info, file.binaryBlob.data(), file.binaryBlob.size(), vertexBuffer.data(), indexBuffer.data());

	uint32_t indexCount  = indexBuffer.size() / sizeof(uint32_t);
	uint32_t vertexCount = vertexBuffer.size() / sizeof(assets::Vertex_f32_PNCV);

	uint32_t* unpacked_indices = (uint32_t*)indexBuffer.data();
	vertices.resize(indexCount);
	assets::Vertex_f32_PNCV* unpackedVertices = (assets::Vertex_f32_PNCV*)vertexBuffer.data();

	for (int i = 0; i < indexCount; i++)
	{
		Vertex new_vert;
		new_vert.position.x = unpackedVertices[ unpacked_indices[i] ].position[0];
		new_vert.position.y = unpackedVertices[ unpacked_indices[i] ].position[1];
		new_vert.position.z = unpackedVertices[ unpacked_indices[i] ].position[2];

		new_vert.normal.x = unpackedVertices[ unpacked_indices[i] ].normal[0];
		new_vert.normal.y = unpackedVertices[ unpacked_indices[i] ].normal[1];
		new_vert.normal.z = unpackedVertices[ unpacked_indices[i] ].normal[2];

		new_vert.color.x = unpackedVertices[ unpacked_indices[i] ].color[0];
		new_vert.color.y = unpackedVertices[ unpacked_indices[i] ].color[1];
		new_vert.color.z = unpackedVertices[ unpacked_indices[i] ].color[2];

		new_vert.uv.x = unpackedVertices[ unpacked_indices[i] ].uv[0];
		new_vert.uv.y = unpackedVertices[ unpacked_indices[i] ].uv[1];

		vertices.push_back(new_vert);
	}

	return true;
}
