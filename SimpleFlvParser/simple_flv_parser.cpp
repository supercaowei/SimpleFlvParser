// SimpleFlvParser.cpp : 定义控制台应用程序的入口点。
//

#include "simple_flv_parser.h"
#include "flv_file.h"
#include "db_output.h"
#include "text_output.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <string>

std::string flv_file;
std::string db_file;
std::string txt_file;

int main(int argc, char* argv[])
{
	if (parse_args(argc, argv) < 0)
		return -1;

	std::shared_ptr<FlvFile> flv;

	//begin parsing flv file
	flv = std::make_shared<FlvFile>(flv_file);

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
			flv->Output(header_cb, tag_cb, nalu_cb);
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
			flv->Output(header_cb, tag_cb, nalu_cb);
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
				flv_file = argv[++i];
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
		else
		{
			if (argv[i][0] == '-')
				printf("Unknown option: %s\n", argv[i]);
			else
				printf("Ignored parameter: %s\n", argv[i]);
			goto help;
		}
	}

	if (flv_file.empty() || (db_file.empty() && txt_file.empty()))
		goto help;

	if (!FilePathIsExist(flv_file, false))
	{
		printf("Flv file %s doesn't exist.\n", flv_file.c_str());
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
	printf("\tSimpleFlvParser -i <input flv file> [-db <output db file>] [-txt <output text file>]\n");
}
