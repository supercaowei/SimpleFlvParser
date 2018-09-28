#ifndef _SFP_UTILS_H_
#define _SFP_UTILS_H_

#include "bytes.h"
#include <string>

std::string  UnicodeToUTF8(const std::wstring& unicode_in);
std::wstring UTF8ToUnicode(const std::string& utf8_in);
std::string  UnicodeToGBK(const std::wstring& unicode_in);
std::wstring GBKToUnicode(const std::string& gbk_in);
template<typename T> T ToUpperString(T& str);
template<typename T> T ToLowerString(T& str);

bool         FilePathIsExist(const std::string& filepath_in, bool is_directory);
bool         CreateDirectoryRecursively(const std::string& full_dir);
std::string  DirectoryFromFilePath(const std::string& path);
std::string  FileNameFromFilePath(const std::string& path);

int BytesToInt(uint8_t* pData, int iCount);
std::string BytesToStr(uint8_t * pData, int iCount);

class ByteReader
{
public:
	ByteReader(uint8_t* start, uint32_t len);
	ByteReader(uint8_t* start, uint8_t* end);

	uint8_t* CurrentPos() const;
	uint8_t* ReadBytes(uint32_t need_bytes, bool move_pos = true);
	uint32_t RemainingSize() const;

private:
	uint8_t* start_ = NULL;
	uint8_t* end_ = NULL;
};

class BitReader
{
public:
	BitReader(uint8_t* byte_start, uint32_t byte_len);
	BitReader(uint8_t* byte_start, uint8_t* byte_end);

	bool IsByteAligned() { return bits_left_ == 8; }
	bool Eof() { return p_ >= end_; }
	bool Overrun() { return p_ > end_; }
	uint32_t Pos() { return ((p_ > end_) ? (end_ - start_) : (p_ - start_)); }
	uint32_t BytesLeft() { return ((p_ >= end_) ? 0 : (end_ - p_)); }

	uint32_t ReadU1();
	void SkipU1();
	uint32_t PeekU1();
	uint32_t ReadU(int nbits);
	void SkipU(int nbis);
	uint32_t ReadF(int nbits);
	uint32_t ReadU8();
	uint32_t ReadUE();
	int32_t ReadSE();
	int ReadBytes(uint8_t* buf, int len);
	int SkipBytes(int len);
	uint32_t NextBits(int nbits);

private:
	uint8_t* start_ = NULL;
	uint8_t* p_ = NULL;
	uint8_t* end_ = NULL;
	int bits_left_ = 0; //how many bits left in the byte pointed by p_
};

#endif
