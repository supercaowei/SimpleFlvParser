#include "db_output.h"
#include "sqlite3.h"
#include <assert.h>

#ifdef _WIN32
#include <io.h>
#include <process.h>
#pragma warning(disable: 4996)
#define sprintf sprintf_s
#define access _access
#else
#include <unistd.h>
#endif

static const char *SQL_STAT_CREATE_FLV_HEADER_TABLE = \
	"CREATE TABLE IF NOT EXISTS flv_header (\
		have_video BOOLEAN,\
		have_audio BOOLEAN,\
		version INTEGER,\
		header_size INTEGER)";

static const char *SQL_STAT_INSERT_FLV_HEADER_FMT = \
	"INSERT INTO flv_header(have_video, have_audio, version, header_size) \
	VALUES( %d , %d , %d , %d )\0";

static const char *SQL_STAT_CREATE_FLV_TAGS_TABLE = \
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

static const char *SQL_STAT_INSERT_FLV_TAG_FMT = \
	"INSERT INTO flv_tags(serial, previous_tag_size, tag_type, stream_id, tag_size, \
	pts, dts, sub_type, format, extra_info) \
	VALUES( %d , %d , '%s' , %d , %d , %d , %d , '%s' , '%s' , '%s' )\0";

static const char *SQL_STAT_CREATE_NALU_TABLE = \
	"CREATE TABLE IF NOT EXISTS nal_units (\
		serial INTEGER PRIMARY KEY,\
		tag_serial_belong INTEGER,\
		nalu_size INTEGER,\
		nal_ref_idc INTEGER,\
		nal_unit_type TEXT,\
		first_mb_in_slice INTEGER,\
		slice_type TEXT,\
		pic_parameter_set_id INTEGER,\
		frame_num INTEGER,\
		field_pic_flag INTEGER,\
		pic_order_cnt_lsb INTEGER,\
		slice_qp_delta INTEGER,\
		extra_info VARCHAR(2000))";

static const char *SQL_STAT_INSERT_NALU_FMT = \
	"INSERT INTO nal_units(serial, tag_serial_belong, nalu_size, nal_ref_idc, nal_unit_type, \
	first_mb_in_slice, slice_type, pic_parameter_set_id, frame_num, field_pic_flag, pic_order_cnt_lsb, slice_qp_delta, extra_info) \
	VALUES( %d , %d , %d , %d , '%s' , %d , '%s' , %d , %d , %d , %d , %d , '%s' )\0";

DBOutput::DBOutput(const std::string& db_path)
{
	if (access(db_path.c_str(), 0) == 0) //file already exists
		remove(db_path.c_str()); //delete the file
	if (access(db_path.c_str(), 0) == 0)
	{
		printf("Database file %s already exists and is occupied now.", db_path.c_str());
		return;
	}

	int ret = sqlite3_open_v2(db_path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, NULL);
	if (ret != SQLITE_OK)
	{
		db_ = NULL;
		return;
	}

	const char *create_table_sql[] = { SQL_STAT_CREATE_FLV_HEADER_TABLE, SQL_STAT_CREATE_FLV_TAGS_TABLE, SQL_STAT_CREATE_NALU_TABLE };
	for (int i = 0; i < 3; i++)
	{
		ret = sqlite3_exec(db_, create_table_sql[i], NULL, NULL, NULL);
		if (ret != SQLITE_OK)
		{
			sqlite3_close(db_);
			db_ = NULL;
			return;
		}
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
	static char buff[100] = { 0 };
	sprintf(
		buff, SQL_STAT_INSERT_FLV_HEADER_FMT,
		header->HaveVideo(),
		header->HaveAudio(),
		header->Version(),
		header->HeaderSize());

	int ret = sqlite3_exec(db_, buff, NULL, NULL, NULL);
	if (ret != SQLITE_OK)
	{
		assert(false);
	}
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
	sprintf(buff, SQL_STAT_INSERT_FLV_TAG_FMT, 
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
	"INSERT INTO nal_units(serial, tag_serial_belong, nalu_size, nal_ref_idc, nal_unit_type, \
				first_mb_in_slice, slice_type, pic_parameter_set_id, frame_num, field_pic_flag, pic_order_cnt_lsb, slice_qp_delta, extra_info) \
									VALUES( %d , %d , %d , %d , '%s' , %d , '%s' , %d , %d , %d , %d , %d , '%s' )\0";

	static char buff[10240] = { 0 };
	sprintf(
		buff, SQL_STAT_INSERT_NALU_FMT,
		++nalu_serial,
		nalu->TagSerialBelong(),
		nalu->NaluSize(),
		nalu->NalRefIdc(),
		nalu->NalUnitType().c_str(),
		nalu->FirstMbInSlice(),
		nalu->SliceType().c_str(),
		nalu->PicParameterSetId(),
		nalu->FrameNum(),
		nalu->FieldPicFlag(),
		nalu->PicOrderCntLsb(),
		nalu->SliceQpDelta(),
		nalu->ExtraInfo().c_str());

	int ret = sqlite3_exec(db_, buff, NULL, NULL, NULL);
	if (ret != SQLITE_OK)
	{
		assert(false);
	}
}
