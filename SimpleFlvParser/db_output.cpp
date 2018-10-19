#include "db_output.h"
#include "sqlite3.h"
#include <assert.h>

static const char *SQL_STAT_CREATE_TABLE = \
	"CREATE TABLE IF NOT EXISTS flv_tags(\
		serial INTEGER PRIMARY KEY, \
		previous_tag_size INTEGER, \
		tag_type TEXT, \
		stream_id INTEGER, \
		tag_size INTEGER, \
		pts INTEGER, \
		dts INTEGER, \
		sub_type TEXT, \
		format TEXT, \
		extra_info VARCHAR(2000))";

static const char *SQL_STAT_INSERT = \
	"INSERT INTO flv_tags(serial, previous_tag_size, tag_type, stream_id, tag_size, \
	pts, dts, sub_type, format, extra_info) \
	VALUES( ? , ? , ? , ? , ? , ? , ? , ? , ? , ? )";

static const char *SQL_STAT_INSERT_FMT = \
"INSERT INTO flv_tags(serial, previous_tag_size, tag_type, stream_id, tag_size, \
	pts, dts, sub_type, format, extra_info) \
	VALUES( %d , %d , '%s' , %d , %d , %d , %d , '%s' , '%s' , '%s' )\0";

DBOutput::DBOutput(const std::string& db_path)
{
	int ret = sqlite3_open_v2(db_path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
	if (ret != SQLITE_OK)
	{
		db_ = NULL;
		return;
	}

	ret = sqlite3_exec(db_, SQL_STAT_CREATE_TABLE, NULL, NULL, NULL);
	if (ret != SQLITE_OK)
	{
		sqlite3_close(db_);
		db_ = NULL;
		return;
	}
}

DBOutput::~DBOutput()
{
	if (db_)
	{
		sqlite3_close(db_);
		db_ = NULL;
	}
}

void DBOutput::FlvHeaderOutput(const std::shared_ptr<FlvHeaderInterface>& header)
{
}

void DBOutput::FlvTagOutput(const std::shared_ptr<FlvTagInterface>& tag)
{
	if (tag == NULL)
		return;

	//sqlite3_stmt* stmt = NULL;
	//int length = strlen(SQL_STAT_INSERT) + 1;
	//std::string str;
	//sqlite3_prepare_v2(db_, SQL_STAT_INSERT, length, &stmt, NULL);
	//sqlite3_bind_int(stmt, 1, tag->Serial());
	//sqlite3_bind_int(stmt, 2, tag->PreviousTagSize());
	//str = tag->TagType();
	//int ret1 = sqlite3_bind_text(stmt, 3, str.c_str(), str.size(), SQLITE_STATIC);
	//sqlite3_bind_int(stmt, 4, tag->StreamId());
	//sqlite3_bind_int(stmt, 5, tag->TagSize());
	//sqlite3_bind_int(stmt, 6, tag->Pts());
	//sqlite3_bind_int(stmt, 7, tag->Dts());
	//str = tag->SubType();
	//sqlite3_bind_text(stmt, 8, str.c_str(), str.size(), SQLITE_STATIC);
	//str = tag->Format();
	//sqlite3_bind_text(stmt, 9, str.c_str(), str.size(), SQLITE_STATIC);
	//str = tag->ExtraInfo();
	//sqlite3_bind_text(stmt, 10, str.c_str(), str.size(), SQLITE_STATIC);

	//int ret = sqlite3_step(stmt);
	//if (!(ret == SQLITE_OK || ret == SQLITE_ROW || ret == SQLITE_DONE))
	//	assert(false);

	//sqlite3_finalize(stmt);

	static char buff[10240] = {0};
	sprintf(buff, SQL_STAT_INSERT_FMT, 
		tag->Serial(),
		tag->PreviousTagSize(),
		tag->TagType().c_str(),
		tag->StreamId(),
		tag->TagSize(),
		tag->Pts(),
		tag->Dts(),
		tag->SubType().c_str(),
		tag->Format().c_str(),
		tag->ExtraInfo().c_str());

	int ret = sqlite3_exec(db_, buff, NULL, NULL, NULL);
	if (ret != SQLITE_OK)
	{
		assert(false);
	}
}

void DBOutput::NaluOutput(const std::shared_ptr<NaluInterface>& nalu)
{
}
