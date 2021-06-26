#include <texture_asset.h>

#include <string>
#include <json.hpp>
#include <lz4.h>

assets::TextureFormat assets::parse_format(const char* f)
{
  if (strcmp(f, "RGBA8") == 0)
    return assets::TextureFormat::RGBA8;

  else
    return assets::TextureFormat::Unknown;
}

assets::AssetFile assets::pack_texture(assets::TextureInfo* info, void* pixelData)
{
  AssetFile file;

  nlohmann::json texture_metadata;
  texture_metadata["format"] = "RGBA8";
  texture_metadata["width"] = info->pixelsize[0];
  texture_metadata["height"] = info->pixelsize[1];
  texture_metadata["buffer_size"] = info->textureSize;
  texture_metadata["original_file"] = info->originalFile;
  texture_metadata["compression"] = "";
  std::string stringified = texture_metadata.dump();
	file.json = stringified;

  // int compressStaging = LZ4_compressBound(info->textureSize);
	file.binaryBlob.resize(info->textureSize);
  memcpy(file.binaryBlob.data(), pixelData, info->textureSize);
	// int compressedSize = LZ4_compress_default((const char*)pixelData, file.binaryBlob.data(), info->textureSize, compressStaging);
	// file.binaryBlob.resize(compressedSize);

  return file;
}

assets::TextureInfo assets::read_texture_info(assets::AssetFile* file)
{
  TextureInfo info;

  nlohmann::json texture_metadata = nlohmann::json::parse(file->json);

  std::string formatString = texture_metadata["format"];
  info.textureFormat = parse_format(formatString.c_str());

  std::string compressionString = texture_metadata["compression"];

  info.textureSize = texture_metadata["buffer_size"];

  info.pixelsize[0] = texture_metadata["width"];
  info.pixelsize[1] = texture_metadata["height"];

  info.originalFile = texture_metadata["original_file"];

	return info;
}

void assets::unpack_texture(TextureInfo* info, const char* sourcebuffer, size_t sourceSize, char* destination)
{
  // if (info->compressionMode == CompressionMode::LZ4)
	// 	LZ4_decompress_safe(sourcebuffer, destination, sourceSize, info->textureSize);
  //
	// else
    memcpy(destination, sourcebuffer, sourceSize);

}
