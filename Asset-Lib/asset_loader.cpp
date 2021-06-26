#include <asset_loader.h>

#include <fstream>

bool assets::save_binaryfile(const char* path, const AssetFile& file)
{
  std::ofstream outfile;
  outfile.open(path, std::ios::binary | std::ios::out);

	//json lenght
	uint32_t jsonlength = file.json.size() + 1 ;
	outfile.write((const char*)&jsonlength, sizeof(uint32_t));
  printf("%d\n", jsonlength);
	//blob lenght
	uint32_t bloblength = file.binaryBlob.size();
	outfile.write((const char*)&bloblength, sizeof(uint32_t));

	//json stream
	outfile.write(file.json.c_str(), jsonlength);

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
