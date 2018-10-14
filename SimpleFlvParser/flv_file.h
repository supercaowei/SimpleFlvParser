#ifndef _SFP_FLV_FILE_H_
#define _SFP_FLV_FILE_H_

#include "input_interface.h"

#include <memory>
#include <string>
#include <list>
#include <functional>

typedef std::function<void(const std::shared_ptr<FlvHeaderInterface>&)> FlvHeaderCallback;
typedef std::function<void(const std::shared_ptr<FlvTagInterface>&)> FlvTagCallback;
typedef std::function<void(const std::shared_ptr<NaluInterface>&)> NaluCallback;

class FlvHeader;
class FlvTag;

class FlvFile
{
public:
	FlvFile(const std::string& flv_path);
	~FlvFile();
	bool IsGood() { return is_good_; }

public:
	void Output(const FlvHeaderCallback& header_cb, const FlvTagCallback& tag_cb, const NaluCallback& nalu_cb);

private:
	bool is_good_ = false;
	std::shared_ptr<FlvHeader> flv_header_;
	std::list<std::shared_ptr<FlvTag> > flv_data_;
};

#endif
