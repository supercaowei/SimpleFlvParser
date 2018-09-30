#ifndef _SFP_FLV_FILE_H_
#define _SFP_FLV_FILE_H_

#include "input_interface.h"

#include <memory>
#include <string>
#include <list>

struct FlvHeader;
class FlvTag;

class FlvFile
{
public:
	FlvFile(const std::string& flv_path);
	~FlvFile();
	bool IsGood() { return is_good_; }

public:
	std::shared_ptr<FlvHeaderInterface> GetFlvHeader();
	std::shared_ptr<FlvTagInterface> EnumFlvTag();
	std::shared_ptr<NaluInterface> EnumNalu();

private:
	bool is_good_ = false;
	std::shared_ptr<FlvHeader> flv_header_;
	std::list<std::shared_ptr<FlvTag> > flv_data_;
};

#endif
