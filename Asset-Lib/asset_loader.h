#pragma once

#include <string>
#include <cstring>
#include <vector>

namespace assets
{
	struct AssetFile
  {
		std::string json;
		std::vector<char> binaryBlob;
	};


	bool save_binaryfile(const char* path, const AssetFile& file);

	bool load_binaryfile(const char* path, AssetFile& outputFile);
}
