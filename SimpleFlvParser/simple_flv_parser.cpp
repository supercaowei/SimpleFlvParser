// SimpleFlvParser.cpp
//

#include "simple_flv_parser.h"
#include "flv_file.h"
#include "db_output.h"
#include "text_output.h"
#include "demux_to_file.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <string>
#include <functional>

std::string iuput_file;
std::string input_type = "flv";
std::string db_file;
std::string txt_file;
std::string h26x_file;
std::string aac_file;
bool print_sei = false;

int main(int argc, char* argv[])
{
	if (parse_args(argc, argv) < 0)
		return -1;

	std::shared_ptr<FlvFile> flv;
	std::shared_ptr<H264File> h264;
	std::shared_ptr<H265File> h265;
	std::shared_ptr<DemuxToFile> demux_to_file;

	if (!h26x_file.empty() || !aac_file.empty())
		demux_to_file = std::make_shared<DemuxToFile>(h26x_file, aac_file);

	if (input_type == "flv") {
		flv = std::make_shared<FlvFile>(iuput_file, demux_to_file);
	} else if (input_type == "h264") {
		h264 = std::make_shared<H264File>(iuput_file);
	} else if (input_type == "h265") {
		h265 = std::make_shared<H265File>(iuput_file);
	}

	//open an output
	if (!db_file.empty())
	{
		std::shared_ptr<FlvOutputInterface> output = std::make_shared<DBOutput>(db_file);
		if (output && output->IsGood())
		{
			//get the output callbacks
			FlvHeaderCallback header_cb = std::bind(&FlvOutputInterface::FlvHeaderOutput, output, std::placeholders::_1);
			FlvTagCallback tag_cb = std::bind(&FlvOutputInterface::FlvTagOutput, output, std::placeholders::_1);
			NaluCallback nalu_cb = std::bind(&FlvOutputInterface::NaluOutput, output, std::placeholders::_1);

			//do output
			if (flv)
				flv->Output(header_cb, tag_cb, nalu_cb);
			if (h264)
				h264->Output(nalu_cb);
			if (h265)
				h265->Output(nalu_cb);
		}
	}

	if (!txt_file.empty())
	{
		std::shared_ptr<FlvOutputInterface> output = std::make_shared<TextOutput>(txt_file);
		if (output && output->IsGood())
		{
			//get the output callbacks
			FlvHeaderCallback header_cb = std::bind(&FlvOutputInterface::FlvHeaderOutput, output, std::placeholders::_1);
			FlvTagCallback tag_cb = std::bind(&FlvOutputInterface::FlvTagOutput, output, std::placeholders::_1);
			NaluCallback nalu_cb = std::bind(&FlvOutputInterface::NaluOutput, output, std::placeholders::_1);

			//do output
			if (flv)
				flv->Output(header_cb, tag_cb, nalu_cb);
			if (h264)
				h264->Output(nalu_cb);
			if (h265)
				h265->Output(nalu_cb);
		}
	}

	return 0;
}

int parse_args(int argc, char* argv[])
{
	for (int i = 0; i < argc; i++)
		printf("%s ", argv[i]);
	printf("\n");

	if (argc < 2)
		goto help;
	
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
			goto help;
		else if (strcmp(argv[i], "-i") == 0)
		{
			if (i + 1 >= argc || argv[i + 1][0] == '-')
				goto help;
			else
				iuput_file = argv[++i];
		}
		else if (strcmp(argv[i], "-type") == 0) 
		{
			if (i + 1 >= argc || (strcmp(argv[i + 1], "flv") && strcmp(argv[i + 1], "h264") && strcmp(argv[i + 1], "h265")))
				goto help;
			else
				input_type = argv[++i];
		}
		else if (strcmp(argv[i], "-db") == 0)
		{
			if (i + 1 >= argc || argv[i + 1][0] == '-')
				goto help;
			else
				db_file = argv[++i];
		}
		else if (strcmp(argv[i], "-txt") == 0)
		{
			if (i + 1 >= argc || argv[i + 1][0] == '-')
				goto help;
			else
				txt_file = argv[++i];
		}
		else if (strcmp(argv[i], "-printsei") == 0)
		{
			print_sei = true;
		}
		else if (strcmp(argv[i], "-vcopy") == 0)
		{
			if (i + 1 >= argc || argv[i + 1][0] == '-')
				goto help;
			else
				h26x_file = argv[++i];
		}
		else if (strcmp(argv[i], "-acopy") == 0)
		{
			if (i + 1 >= argc || argv[i + 1][0] == '-')
				goto help;
			else
				aac_file = argv[++i];
		}
		else
		{
			if (argv[i][0] == '-')
				printf("Unknown option: %s\n", argv[i]);
			else
				printf("Ignored parameter: %s\n", argv[i]);
			goto help;
		}
	}

	if (iuput_file.empty() || (db_file.empty() && txt_file.empty() && h26x_file.empty() && aac_file.empty() && !print_sei))
		goto help;

	if (!FilePathIsExist(iuput_file, false))
	{
		printf("Flv file %s doesn't exist.\n", iuput_file.c_str());
		return -2;
	}

	return 0;

help:
	print_help();
	return -1;
}

void print_help()
{
	printf("SimpleFlvParser usage: \n");
	printf("\tSimpleFlvParser -i <input flv file> [-type flv|h264|h265] [-db <output db file>] [-txt <output text file>] [-printsei] [-vcopy <output h264/h265 file>] [-acopy <output aac file>]\n");
	printf("\t-i <input flv file>: 输入的被解析文件路径\n");
	printf("\t-type flv|h264|h265: 输入的文件类型，支持flv文件和annex-b格式的H264/H265文件\n");
	printf("\t-db <output db file>: 输出db文件路径\n");
	printf("\t-txt <output text file>: 输出文本文件路径\n");
	printf("\t-printsei: 打印SEI内容\n");
	printf("\t-vcopy <output h264/h265 file>: 从flv中demux输出h264或h265文件的路径\n");
	printf("\t-acopy <output aac file>: 从flv中demux输出aac文件的路径\n");
}
