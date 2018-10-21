#include "text_output.h"

#ifdef _WIN32
#include <io.h>
#include <process.h>
#pragma warning(disable: 4996)
#define sprintf sprintf_s
#define access _access
#else
#include <unistd.h>
#endif

TextOutput::TextOutput(const std::string& txt_path)
{
	if (access(txt_path.c_str(), 0) == 0) //file already exists
		remove(txt_path.c_str()); //delete the file
	if (access(txt_path.c_str(), 0) == 0)
	{
		printf("Text file %s already exists and is occupied now.\n", txt_path.c_str());
		return;
	}

	txt_file_ = fopen(txt_path.c_str(), "w");
	if (!txt_file_)
		printf("Create txt file %s error: %d.\n", txt_path.c_str(), errno);
}

TextOutput::~TextOutput()
{
	if (txt_file_)
	{
		fclose(txt_file_);
		txt_file_ = NULL;
	}
}

void TextOutput::FlvHeaderOutput(const std::shared_ptr<FlvHeaderInterface>& header)
{
	if (!header_title_printed_)
	{
		std::string splitter(15 + 1 + 15 + 1 + 15 + 1 + 15, '-');
		fprintf(txt_file_, "%s\n", splitter.c_str());
		fprintf(txt_file_, "%15s %15s %15s %15s\n", "have_video", "have_audio", "version", "header_size");
		header_title_printed_ = true;
	}
	fprintf(txt_file_, "%15d %15d %15d %15d\n", header->HaveVideo(), header->HaveAudio(), header->Version(), header->HeaderSize());
}

void TextOutput::FlvTagOutput(const std::shared_ptr<FlvTagInterface>& tag)
{
	if (!tags_title_printed_)
	{
		std::string splitter(10 + 1 + 15 + 1 + 12 + 1 + 10 + 1 + 10 + 1 + 10 + 1 + 20, '-');
		fprintf(txt_file_, "%s\n", splitter.c_str());
		fprintf(txt_file_, "%10s %15s %12s %10s %10s %10s %20s\n", "serial", "tag_type", "tag_size", "pts", "dts", "dts_diff", "sub_type");
		tags_title_printed_ = true;
	}
	fprintf(txt_file_, "%10d %15s %12d %10d %10d %10d %20s\n",
		tag->Serial(), 
		tag->TagType().c_str(), 
		tag->TagSize(), 
		tag->Pts(), 
		tag->Dts(), 
		tag->DtsDiff(), 
		tag->SubType().c_str());
}

void TextOutput::NaluOutput(const std::shared_ptr<NaluInterface>& nalu)
{
	if (!nalu_title_printed_)
	{
		std::string splitter(10 + 1 + 10 + 1 + 11 + 1 + 13 + 1 + 10, '-');
		fprintf(txt_file_, "%s\n", splitter.c_str());
		fprintf(txt_file_, "%10s %10s %11s %13s %10s\n", "tag_belong", "nalu_size", "nal_ref_idc", "nal_unit_type", "slice_type");
		nalu_title_printed_ = true;
	}

	fprintf(txt_file_, "%10d %10d %11d %13s %10s\n",
		nalu->TagSerialBelong(),
		nalu->NaluSize(),
		nalu->NalRefIdc(),
		nalu->NalUnitType().c_str(),
		nalu->SliceType().c_str());
}
