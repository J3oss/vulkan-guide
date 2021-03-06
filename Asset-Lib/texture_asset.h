#pragma once

#include "asset_loader.h"

namespace assets
{
	enum class TextureFormat
	{
		Unknown = 0,
		RGBA8
	};

	struct TextureInfo
  {
		uint64_t textureSize;
		TextureFormat textureFormat;
		uint32_t pixelsize[3];
		std::string originalFile;
	};

	//parses the texture metadata from an asset file
	TextureInfo read_texture_info(AssetFile* file);

	void unpack_texture(TextureInfo* info, const char* sourcebuffer, size_t sourceSize, char* destination);

	AssetFile pack_texture(TextureInfo* info, void* pixelData);

	TextureFormat parse_format(const char* f);
}
