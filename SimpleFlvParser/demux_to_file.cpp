#include "demux_to_file.h"

DemuxToFile::DemuxToFile(const std::string& video_file_path, const std::string& audio_file_path)
{
	if (!video_file_path.empty())
		video_file_ = fopen(video_file_path.c_str(), "w");
	if (!audio_file_path.empty())
		audio_file_ = fopen(audio_file_path.c_str(), "w");
}

DemuxToFile::~DemuxToFile()
{
	if (video_file_)
	{
		fclose(video_file_);
		video_file_ = NULL;
	}
	if (audio_file_)
	{
		fclose(audio_file_);
		audio_file_ = NULL;
	}
}

void DemuxToFile::OnVideoNaluData(const uint8_t* data, uint32_t size)
{
	if (!video_file_)
		return;
	fwrite(data, size, 1, video_file_);
}

void DemuxToFile::OnAudioAACData(const uint8_t* data, uint32_t size)
{
	if (!audio_file_)
		return;
	fwrite(data, size, 1, audio_file_);
}

