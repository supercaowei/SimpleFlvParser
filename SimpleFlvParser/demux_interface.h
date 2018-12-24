#ifndef _SFP_DEMUX_INTERFACE_H_
#define _SFP_DEMUX_INTERFACE_H_

#include "bytes.h"

class DemuxInterface
{
public:
	virtual ~DemuxInterface() {}
	virtual void OnVideoNaluData(const uint8_t* data, uint32_t size) = 0;
	virtual void OnAudioAACData(const uint8_t* data, uint32_t size) = 0;
};

#endif //_SFP_DEMUX_INTERFACE_H_
