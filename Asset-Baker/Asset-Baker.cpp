#include <asset_loader.h>
#include <texture_asset.h>
#include <mesh_asset.h>

#include <iostream>
#include <fstream>
#include <filesystem>

#include <glm/glm.hpp>
#include<glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

namespace fs = std::filesystem;

#define OUTDIR "assets_export"

std::string calculate_assimp_mesh_name(const aiScene* scene, int meshIndex)
{
	char buffer[50];

	snprintf(buffer, 50, "%d", meshIndex);
	std::string matname = "MESH_" + std::string{ buffer } + "_"+ std::string{ scene->mMeshes[meshIndex]->mName.C_Str()};
	return matname;
}

bool convert_image(const fs::path& input, const fs::path& output)
{
	int texWidth, texHeight, texChannels;

	stbi_uc* pixels = stbi_load(input.u8string().c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	if (!pixels)
  {
		std::cout << "Failed to load texture file " << input << std::endl;
		return false;
	}

	int texture_size = texWidth * texHeight * 4;

	assets::TextureInfo texinfo;
	texinfo.textureSize = texture_size;
	texinfo.pixelsize[0] = texWidth;
	texinfo.pixelsize[1] = texHeight;
	texinfo.textureFormat = assets::TextureFormat::RGBA8;
	texinfo.originalFile = input.string();
	assets::AssetFile newImage = assets::pack_texture(&texinfo, pixels);

	stbi_image_free(pixels);

	save_binaryfile(output.string().c_str(), newImage);

	return true;
}

void extract_assimp_meshes(const aiScene* scene, const fs::path& input, const fs::path& outputFolder)
{
	if(!scene)
	{
		std::cout << "invalid file for assimp"<< std::endl;
		return;
	}

	using VertexFormat = assets::Vertex_f32_PNCV;
	auto VertexFormatEnum = assets::VertexFormat::PNCV_F32;

	for (size_t meshindex = 0; meshindex < scene->mNumMeshes; meshindex++)
	{
		const aiMesh* mesh = scene->mMeshes[meshindex];

		std::vector<VertexFormat> _vertices;
		std::vector<uint32_t> _indices;

		_vertices.resize(mesh->mNumVertices);
		for (size_t v = 0; v < mesh->mNumVertices; v++)
		{
			VertexFormat vert;

			vert.position[0] = mesh->mVertices[v][0];
			vert.position[1] = mesh->mVertices[v][1];
			vert.position[2] = mesh->mVertices[v][2];
			vert.position[3] = 0;

			vert.normal[0] = mesh->mNormals[v].x;
			vert.normal[1] = mesh->mNormals[v].y;
			vert.normal[2] = mesh->mNormals[v].z;
			vert.normal[3] = 0;

			if (mesh->GetNumUVChannels() >= 1)
			{
				vert.uv[0] = mesh->mTextureCoords[0][v].x;
				vert.uv[1] = mesh->mTextureCoords[0][v].y;
				vert.uv[2] = 0;
				vert.uv[3] = 0;
			}
			else
			{
				vert.uv[0] = 0;
				vert.uv[1] = 0;
				vert.uv[2] = 0;
				vert.uv[3] = 0;
			}

			if (mesh->HasVertexColors(0))
			{
				vert.color[0] = mesh->mColors[0][v].r;
				vert.color[1] = mesh->mColors[0][v].g;
				vert.color[2] = mesh->mColors[0][v].b;
				vert.color[3] = 0;
			}
			else
			{
				vert.color[0] = 1;
				vert.color[1] = 1;
				vert.color[2] = 1;
				vert.color[3] = 0;
			}

			_vertices[v] = vert;
			printf("vert: %f %f %f %f\n", vert.position[0], vert.position[1], vert.position[2], vert.position[3]);
		}

		_indices.resize(mesh->mNumFaces * 3);
		for (size_t f = 0; f < mesh->mNumFaces; f++)
		{
			_indices[f * 3 + 0] = mesh->mFaces[f].mIndices[0];
			_indices[f * 3 + 1] = mesh->mFaces[f].mIndices[1];
			_indices[f * 3 + 2] = mesh->mFaces[f].mIndices[2];

			//TODO check if this still valid
			//assimp fbx creates bad normals, just regen them
			if (true)
			{
				int v0 = _indices[f * 3 + 0];
				int v1 = _indices[f * 3 + 1];
				int v2 = _indices[f * 3 + 2];
				glm::vec3 p0{ _vertices[v0].position[0],
					 _vertices[v0].position[1],
					 _vertices[v0].position[2]
				};
				glm::vec3 p1{ _vertices[v1].position[0],
					 _vertices[v1].position[1],
					 _vertices[v1].position[2]
				};
				glm::vec3 p2{ _vertices[v2].position[0],
					 _vertices[v2].position[1],
					 _vertices[v2].position[2]
				};

				glm::vec3 normal =  glm::normalize(glm::cross(p2 - p0, p1 - p0));

				memcpy(_vertices[v0].normal, &normal, sizeof(float) * 3);
				memcpy(_vertices[v1].normal, &normal, sizeof(float) * 3);
				memcpy(_vertices[v2].normal, &normal, sizeof(float) * 3);
			}
		}

		assets::MeshInfo info;
		info.vertexBuferSize = _vertices.size() * sizeof(VertexFormat);
		info.vertexCount = mesh->mNumVertices;
		info.faceCount = mesh->mNumFaces;

		info.indexBuferSize =  _indices.size() * sizeof(uint32_t);
		info.indexCount = mesh->mNumFaces * 3;

		info.bounds = assets::calculateBounds(_vertices.data(), _vertices.size());
		info.vertexFormat = VertexFormatEnum;
		info.indexSize = sizeof(uint32_t);
		info.originalFile = input.string();

		assets::AssetFile newFile = assets::pack_mesh(&info, (char*)_vertices.data(), (char*)_indices.data());

		std::string meshname = calculate_assimp_mesh_name(scene, meshindex);
		fs::path meshpath = outputFolder.parent_path() / (meshname + ".mesh");
		std::cout << "/* message */" <<meshpath.string().c_str()<< '\n';
		save_binaryfile(meshpath.string().c_str(), newFile);
	}
}

int main(int argc, char const *argv[])
{
  if (argc < 2)
	{
		std::cout << "You need to put the path to the info file"<< std::endl;
		return -1;
	}

  fs::path path{ argv[1] };
  fs::path input_directory = path;
	fs::path output_directory = path / OUTDIR;

	fs::create_directory(output_directory);

  std::cout << "loading asset directory at " << input_directory << std::endl;

  for (auto& p : fs::directory_iterator(input_directory))
  {
  	std::cout << "File: " << p << std::endl;

  	if (p.path().extension() == ".png")
    {
			std::cout << "found a texture" << std::endl;

			fs::path newpath = output_directory / p.path().filename();
			newpath.replace_extension(".tx");
			convert_image(p.path(), newpath);
  	}

  	if (p.path().extension() == ".obj" || p.path().extension() == ".glb")
    {
			std::cout << "found a mesh" << std::endl;

			Assimp::Importer importer;
			const aiScene* scene = importer.ReadFile( p.path(),
					aiProcess_CalcTangentSpace       |
				 	aiProcess_Triangulate            |
				 	aiProcess_JoinIdenticalVertices  |
				 	aiProcess_FlipUVs								 |
				 	aiProcess_SortByPType);

			fs::path newpath = output_directory / p.path().filename();
    	newpath.replace_extension(".mesh");
			extract_assimp_meshes(scene, p.path(), newpath);
		}
	}

  return 0;
}
