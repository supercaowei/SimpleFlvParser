#ifndef _SFP_FLV_FILE_H_
#define _SFP_FLV_FILE_H_

#include "input_interface.h"
#include "demux_interface.h"

#include <memory>
#include <string>
#include <list>
#include <functional>

typedef std::function<void(const std::shared_ptr<FlvHeaderInterface>&)> FlvHeaderCallback;
typedef std::function<void(const std::shared_ptr<FlvTagInterface>&)> FlvTagCallback;
typedef std::function<void(const std::shared_ptr<NaluInterface>&)> NaluCallback;

class FlvHeader;
class FlvTag;
class NaluBase;
class HevcNaluBase;

class FlvFile
{
public:
	FlvFile(const std::string& flv_path, const std::shared_ptr<DemuxInterface>& demux_output);
	~FlvFile();
	bool IsGood() { return is_good_; }

public:
	void Output(const FlvHeaderCallback& header_cb, const FlvTagCallback& tag_cb, const NaluCallback& nalu_cb);

private:
	bool is_good_ = false;
	std::shared_ptr<FlvHeader> flv_header_;
	std::list<std::shared_ptr<FlvTag> > flv_data_;
};

class H264File
{
public:
	H264File(const std::string& h264_path);
	~H264File();
	bool IsGood() { return is_good_; }

public:
	void Output(const NaluCallback& nalu_cb);

private:
	bool is_good_ = false;
	std::list<std::shared_ptr<NaluBase> > nalu_list_;
};

class H265File
{
public:
	H265File(const std::string& h265_path);
	~H265File();
	bool IsGood() { return is_good_; }

public:
	void Output(const NaluCallback& nalu_cb);

private:
	bool is_good_ = false;
	std::list<std::shared_ptr<HevcNaluBase> > nalu_list_;
};

#endif
