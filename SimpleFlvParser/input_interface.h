#ifndef _SFP_INPUT_INTERFACE_H_
#define _SFP_INPUT_INTERFACE_H_

#include "bytes.h"
#include <string>

class FlvHeaderInterface
{
public:
	virtual ~FlvHeaderInterface() {}
	virtual bool    HaveVideo() = 0;
	virtual bool    HaveAudio() = 0;
	virtual uint8_t Version() = 0;
	virtual uint8_t HeaderSize() = 0;
};

class FlvTagInterface
{
public:
	virtual ~FlvTagInterface() {}
	virtual int         Serial() = 0;
	virtual uint32_t    PreviousTagSize() = 0;
	virtual std::string TagType() = 0;
	virtual uint32_t    StreamId() = 0;
	virtual uint32_t    TagSize() = 0;
	virtual uint32_t    Pts() = 0;
	virtual uint32_t    Dts() = 0;
	virtual std::string SubType() = 0;
	virtual std::string Format() = 0;
	virtual std::string ExtraInfo() = 0;
};

class NaluInterface
{
public:
	virtual ~NaluInterface() {}
	virtual int         TagSerialBelong() = 0;
	virtual uint32_t    NaluSize() = 0;
	virtual uint8_t     NalRefIdc() = 0;
	virtual std::string NalUnitType() = 0;
	virtual int8_t      FirstMbInSlice() = 0;
	virtual std::string SliceType() = 0;
	virtual int         PicParameterSetId() = 0;
	virtual int         FrameNum() = 0;
	virtual int         FieldPicFlag() = 0;
	virtual int         PicOrderCntLsb() = 0;
	virtual int         SliceQpDelta() = 0;
	virtual std::string ExtraInfo() = 0;
};


#endif
