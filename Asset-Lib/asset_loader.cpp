#include <asset_loader.h>

#include <fstream>

assets::CompressionMode assets::parse_compression(const char* f)
{
	if (strcmp(f, "LZ4") == 0)
		return assets::CompressionMode::LZ4;

	else
		return assets::CompressionMode::None;
}

bool assets::save_binaryfile(const char* path, const AssetFile& file)
{
  std::ofstream outfile;
  outfile.open(path, std::ios::binary | std::ios::out);

  //type
  outfile.write(file.type, 4);

  //version
	uint32_t version = file.version;
	outfile.write((const char*)&version, sizeof(uint32_t));

	//json lenght
	uint32_t jsonlength = file.json.size();
	outfile.write((const char*)&jsonlength, sizeof(uint32_t));

	//blob lenght
	uint32_t bloblength = file.binaryBlob.size();
	outfile.write((const char*)&bloblength, sizeof(uint32_t));

	//json stream
	outfile.write(file.json.data(), jsonlength);

	//blob data
	outfile.write(file.binaryBlob.data(), bloblength);

	outfile.close();

  return true;
}

bool assets::load_binaryfile(const char* path, AssetFile& outputFile)
{
  std::ifstream infile;
  infile.open(path, std::ios::binary | std::ios::in);

  if (!infile.is_open())
    return false;

  infile.seekg(0);

  //type
  infile.read(outputFile.type, 4);

  //version
  infile.read((char*) &outputFile.version, sizeof(uint32_t));

  //json lenght
	uint32_t jsonlength = 0;
  infile.read((char*) &jsonlength, sizeof(uint32_t));
  outputFile.json.resize(jsonlength);

	//blob lenght
	uint32_t bloblength = 0;
	infile.read((char*) &bloblength, sizeof(uint32_t));
  outputFile.binaryBlob.resize(bloblength);

  //json stream
  infile.read(outputFile.json.data(), jsonlength);

  //blob data
  infile.read(outputFile.binaryBlob.data(), bloblength);

  infile.close();

  return true;
}
