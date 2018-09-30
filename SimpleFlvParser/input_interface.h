#ifndef _SFP_INPUT_INTERFACE_H_
#define _SFP_INPUT_INTERFACE_H_

#include "bytes.h"
#include <string>

class FlvHeaderInterface
{
public:
	virtual ~FlvHeaderInterface() = 0;
	virtual bool HaveVideo() = 0;
	virtual bool HaveAudio() = 0;
	virtual uint8_t Version() = 0;
};

class FlvTagInterface
{
public:
	virtual ~FlvTagInterface() = 0;
	virtual int Serial() = 0;
	virtual uint32_t PreviousTagSize() = 0;
	virtual std::string TagType() = 0;
	virtual uint32_t StreamId() = 0;
	virtual uint32_t TagSize() = 0;
	virtual uint32_t Pts() = 0;
	virtual uint32_t Dts() = 0;
	virtual std::string FormatInfo() = 0;
	virtual std::string ExtraInfo() = 0;
};

class NaluInterface
{
public:
	virtual ~NaluInterface() = 0;
	virtual uint8_t Importance() = 0;
	virtual std::string NaluType() = 0;
	virtual uint32_t NaluSize() = 0;
	virtual std::string SliceType() = 0;
	virtual std::string ExtraInfo() = 0;
};


#endif
