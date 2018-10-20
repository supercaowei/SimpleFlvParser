#ifndef _SFP_DB_OUTPUT_H_
#define _SFP_DB_OUTPUT_H_

#include "output_interface.h"
#include "input_interface.h"
#include <string>

struct sqlite3;

class DBOutput : public FlvOutputInterface
{
public:
	DBOutput(const std::string& db_path);
	~DBOutput();

	virtual void FlvHeaderOutput(const std::shared_ptr<FlvHeaderInterface>& header) override;
	virtual void FlvTagOutput(const std::shared_ptr<FlvTagInterface>& tag) override;
	virtual void NaluOutput(const std::shared_ptr<NaluInterface>& nalu) override;
	virtual bool IsGood() override { return db_ != NULL; }

private:
	sqlite3* db_ = NULL;
	int nalu_serial = 0;
};


#endif
