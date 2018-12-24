#ifndef _SFP_DEMUX_TO_FILE_H_
#define _SFP_DEMUX_TO_FILE_H_

#include "demux_interface.h"
#include <string>

class DemuxToFile : public DemuxInterface
{
public:
	DemuxToFile(const std::string& video_file_path, const std::string& audio_file_path = "");
	~DemuxToFile();

	virtual void OnVideoNaluData(const uint8_t* data, uint32_t size) override;
	virtual void OnAudioAACData(const uint8_t* data, uint32_t size) override;

private:
	FILE* video_file_ = NULL;
	FILE* audio_file_ = NULL;
};

#endif //_SFP_DEMUX_TO_FILE_H_
