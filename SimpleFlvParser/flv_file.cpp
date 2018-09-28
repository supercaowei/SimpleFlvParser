#include "flv_file.h"
#include "flv_file_internal.h"
#include "utils.h"

#ifdef _WIN32
#pragma  warning(disable: 4996)
#endif

FlvFile::FlvFile(const std::string& flv_path)
{
	FILE* flv_file = fopen(flv_path.c_str(), "rb");
	if (!flv_file)
	{
		printf("fopen is failed. \n");
		return;
	}
	fseek(flv_file, 0, SEEK_END);
	uint32_t file_size = ftell(flv_file);
	uint8_t* flv_data = new uint8_t[file_size];
	fseek(flv_file, 0, SEEK_SET);
	if (file_size != fread(flv_data, sizeof(uint8_t), file_size, flv_file))
	{
		printf("read file is failed. \n");
		delete[] flv_data;
		fclose(flv_file);
		return;
	}
	fclose(flv_file);

	ByteReader reader(flv_data, file_size);
	flv_header_ = std::make_shared<FlvHeader>(reader);
	if (!flv_header_ || !flv_header_->is_good_)
	{
		delete[] flv_data;
		return;
	}

	int tag_count = 1;
	while (reader.RemainingSize())
	{
		std::shared_ptr<FlvTag> tag = std::make_shared<FlvTag>(reader, tag_count);
		if (!tag || !tag->IsGood())
			continue;
		flv_data_.push_back(tag);
		tag_count++;
	}

	printf("tag count: %lu\n", flv_data_.size());
	delete[] flv_data;
}

FlvFile::~FlvFile()
{

}

