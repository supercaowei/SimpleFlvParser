#ifndef _SFP_TEXT_OUTPUT_H_
#define _SFP_TEXT_OUTPUT_H_

#include "output_interface.h"
#include "input_interface.h"
#include <string>

struct sqlite3;

class TextOutput : public FlvOutputInterface
{
public:
	TextOutput(const std::string& txt_path);
	~TextOutput();

	virtual void FlvHeaderOutput(const std::shared_ptr<FlvHeaderInterface>& header) override;
	virtual void FlvTagOutput(const std::shared_ptr<FlvTagInterface>& tag) override;
	virtual void NaluOutput(const std::shared_ptr<NaluInterface>& nalu) override;
	virtual bool IsGood() override { return txt_file_ != NULL; }

private:
	FILE* txt_file_ = NULL;
	int nalu_serial = 0;
	bool header_title_printed_ = false;
	bool tags_title_printed_ = false;
	bool nalu_title_printed_ = false;
};


#endif
