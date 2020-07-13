#include "flv_file.h"
#include "flv_file_internal.h"
#include "utils.h"

#ifdef _WIN32
#pragma  warning(disable: 4996)
#endif

FlvFile::FlvFile(const std::string& flv_path, const std::shared_ptr<DemuxInterface>& demux_output)
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
	if (!flv_header_ || !flv_header_->IsGood())
	{
		delete[] flv_data;
		return;
	}

	int tag_count = 1;
	while (reader.RemainingSize())
	{
		std::shared_ptr<FlvTag> tag = std::make_shared<FlvTag>(reader, tag_count, demux_output);
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

void FlvFile::Output(const FlvHeaderCallback& header_cb, const FlvTagCallback& tag_cb, const NaluCallback& nalu_cb)
{
	if (header_cb)
		header_cb(flv_header_);

	if (tag_cb)
	{
		for (auto iter = flv_data_.cbegin(); iter != flv_data_.cend(); iter++)
		{
			std::shared_ptr<FlvTag> tag = *iter;
			if (!tag || !tag->IsGood())
				continue;
			if (tag_cb)
				tag_cb(tag);
		}
	}

	if (nalu_cb)
	{
		for (auto iter = flv_data_.cbegin(); iter != flv_data_.cend(); iter++)
		{
			std::shared_ptr<FlvTag> tag = *iter;
			if (!tag || !tag->IsGood())
				continue;
			NaluList nalu_list = tag->EnumNalus();
			for (const auto& nalu : nalu_list)
				nalu_cb(nalu);
		}
	}
}

int isNaluStartCode(uint8_t* data, uint32_t size) {
	if (!data || size < 3 || data[0] != 0 || data[1] != 0) {
		return 0;
	}
    return size >= 4 && data[2] == 0 && data[3] == 1 ? 4 : ((data[2] == 1) ? 3 : 0);
}

//从reader当前位置寻找start code（0x000001或0x00000001），找到就返回start code，否则就返回负值
bool findNalu(ByteReader& reader, uint8_t* &nalu, uint32_t &naluSize) {
	//必须以一个start code开头
	int startCodeSize = isNaluStartCode(reader.ReadBytes(4, false), 4);
	if (startCodeSize) {
		reader.ReadBytes(startCodeSize); //跳过start code
	} else {
		return false; //不是以start code开头，或者已经到达末尾
	}

	nalu = reader.CurrentPos();
	while (reader.RemainingSize() > 0) {
		if (isNaluStartCode(reader.ReadBytes(4, false), 4)) {
			break; //接下来3个字节或4个字节是start code
		}
		reader.ReadBytes(1); //移动1个字节
	}
	naluSize = reader.CurrentPos() - nalu;
	return (int)naluSize > 0;
}

H264File::H264File(const std::string& h264_path) {
	FILE* h264_file = fopen(h264_path.c_str(), "rb");
	if (!h264_file)
	{
		printf("fopen failed. \n");
		return;
	}
	fseek(h264_file, 0, SEEK_END);
	uint32_t h264_size = ftell(h264_file);
	uint8_t* h264_data = new uint8_t[h264_size];
	fseek(h264_file, 0, SEEK_SET);
	if (h264_size != fread(h264_data, sizeof(uint8_t), h264_size, h264_file))
	{
		printf("read h264 file failed. \n");
		delete[] h264_data;
		fclose(h264_file);
		return;
	}
	fclose(h264_file);

	uint8_t* nalu_data = NULL;
	uint32_t nalu_size = 0;
	ByteReader reader(h264_data, h264_size);
	std::shared_ptr<DemuxInterface> demux_output;
	while (findNalu(reader, nalu_data, nalu_size)) {
		ByteReader naluReader(nalu_data, nalu_size);
		auto nalu = NaluBase::Create(naluReader, nalu_size, demux_output);
		if (nalu) {
			nalu_list_.push_back(nalu);
		}
	}

	is_good_ = true;
	printf("nalu count: %lu\n", nalu_list_.size());
	delete[] h264_data;
}

H264File::~H264File() {

}

void H264File::Output(const NaluCallback& nalu_cb) {
	if (nalu_cb)
	{
		for (auto iter = nalu_list_.cbegin(); iter != nalu_list_.cend(); iter++)
		{
			auto nalu = *iter;
			if (!nalu || !nalu->IsGood())
				continue;
			nalu_cb(nalu);
		}
	}
}


H265File::H265File(const std::string& h265_path) {
	FILE* h265_file = fopen(h265_path.c_str(), "rb");
	if (!h265_file)
	{
		printf("fopen failed. \n");
		return;
	}
	fseek(h265_file, 0, SEEK_END);
	uint32_t h265_size = ftell(h265_file);
	uint8_t* h265_data = new uint8_t[h265_size];
	fseek(h265_file, 0, SEEK_SET);
	if (h265_size != fread(h265_data, sizeof(uint8_t), h265_size, h265_file))
	{
		printf("read h265 file failed. \n");
		delete[] h265_data;
		fclose(h265_file);
		return;
	}
	fclose(h265_file);

	uint8_t* nalu_data = NULL;
	uint32_t nalu_size = 0;
	ByteReader reader(h265_data, h265_size);
	std::shared_ptr<DemuxInterface> demux_output;
	while (findNalu(reader, nalu_data, nalu_size)) {
		ByteReader naluReader(nalu_data, nalu_size);
		auto nalu = HevcNaluBase::Create(naluReader, nalu_size, demux_output);
		if (nalu) {
			nalu_list_.push_back(nalu);
		}
	}

	is_good_ = true;
	printf("nalu count: %lu\n", nalu_list_.size());
	delete[] h265_data;
}

H265File::~H265File() {

}

void H265File::Output(const NaluCallback& nalu_cb) {
	if (nalu_cb)
	{
		for (auto iter = nalu_list_.cbegin(); iter != nalu_list_.cend(); iter++)
		{
			auto nalu = *iter;
			if (!nalu || !nalu->IsGood())
				continue;
			nalu_cb(nalu);
		}
	}
}
