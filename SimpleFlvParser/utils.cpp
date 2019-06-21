#include "utils.h"

template<typename T>
inline T ToUpperString(T& str)
{
	T str_copy = str;
	for (size_t i = 0; i < str_copy.length(); i++)
	{
		auto c = str_copy[i];
		if (islower(c))
			str_copy[i] = (decltype(c))toupper(c);
	}
	return str_copy;
}

template<typename T>
inline T ToLowerString(T& str)
{
	T str_copy = str;
	for (size_t i = 0; i < str_copy.length(); i++)
	{
		auto c = str_copy[i];
		if (isupper(c))
			str_copy[i] = (decltype(c))tolower(c);
	}
	return str_copy;
}

#ifdef WIN32

#include <shlobj.h> 
#include <shlwapi.h>
#pragma  comment(lib, "Version.lib")
#pragma  comment(lib, "Shlwapi.lib")

#pragma  warning(disable: 4996)

std::string UnicodeToUTF8(const std::wstring& unicode_in)
{
	std::string utf8_out;
	int len = WideCharToMultiByte(CP_UTF8, 0, unicode_in.c_str(), -1, NULL, 0, NULL, NULL);
	utf8_out.resize(len + 1);
	WideCharToMultiByte(CP_UTF8, 0, unicode_in.c_str(), -1, (LPSTR)utf8_out.c_str(), len, NULL, NULL);
	return utf8_out;
}

std::wstring UTF8ToUnicode(const std::string & utf8_in)
{
	std::wstring unicode_out;
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8_in.c_str(), -1, NULL, 0);
	unicode_out.resize(len + 1);
	MultiByteToWideChar(CP_UTF8, 0, utf8_in.c_str(), -1, (LPWSTR)unicode_out.c_str(), len);
	return unicode_out;
}

std::string UnicodeToGBK(const std::wstring& unicode_in)
{
	std::string gbk_out;
	int len = WideCharToMultiByte(CP_ACP, 0, unicode_in.c_str(), -1, NULL, 0, NULL, NULL);
	gbk_out.resize(len + 1);
	WideCharToMultiByte(CP_ACP, 0, unicode_in.c_str(), -1, (LPSTR)gbk_out.c_str(), len, NULL, NULL);
	return gbk_out;
}

std::wstring GBKToUnicode(const std::string & gbk_in)
{
	std::wstring unicode_out;
	int len = MultiByteToWideChar(CP_ACP, 0, gbk_in.c_str(), -1, NULL, 0);
	unicode_out.resize(len + 1);
	MultiByteToWideChar(CP_ACP, 0, gbk_in.c_str(), -1, (LPWSTR)unicode_out.c_str(), len);
	return unicode_out;
}

bool FilePathIsExist(const std::string & filepath_in, bool is_directory)
{
	const DWORD file_attr = ::GetFileAttributesW(GBKToUnicode(filepath_in).c_str());
	if (file_attr != INVALID_FILE_ATTRIBUTES)
	{
		if (is_directory)
			return (file_attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
		else
			return true;
	}
	return false;
}

bool CreateDirectoryRecursively(const std::string& full_dir)
{
	HRESULT result = ::SHCreateDirectory(NULL, GBKToUnicode(full_dir).c_str());
	if (result == ERROR_SUCCESS || result == ERROR_FILE_EXISTS || result == ERROR_ALREADY_EXISTS)
		return true;
	return false;
}

#elif __APPLE__ || __ANDROID__ || __linux__ || __unix__

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

bool FilePathIsExist(const std::string & filepath_in, bool is_directory)
{
	struct stat info;
	int r = stat(filepath_in.c_str(), &info);
	if (r == 0)
	{
		if (is_directory && S_ISDIR(info.st_mode))
			return true;
		else if (!is_directory)
			return true;
		else
		{
			printf("%s exists as a %s.\n", filepath_in.c_str(), is_directory ? "file" : "directory");
			return false;
		}
	}
	else if (errno == ENOENT)
		return false;
	else
		printf("Check file path existance error.\n");
	return false;
}

bool CreateDirectoryRecursively(const std::string& full_dir)
{
	if (FilePathIsExist(full_dir, true))
		return true;
	else if (FilePathIsExist(full_dir, false))
		return false;

	char buff[1024] = {0};
	sprintf(buff, "mkdir -p \"%s\"", full_dir.c_str());
	int ret = system(buff);
	return (ret == 0);
}

#endif

std::string DirectoryFromFilePath(const std::string& path)
{
	std::string path_copy = path;
	std::string::size_type pos = path_copy.find_last_of("\\/");
	if (pos == std::string::npos)
		return "";
	path_copy = path_copy.substr(0, pos);
	pos = path_copy.find_last_not_of("\\/");
	if (pos == std::string::npos)
		return "";
	return path_copy.substr(0, pos + 1);
}

std::string FileNameFromFilePath(const std::string& path)
{
	std::string::size_type pos = path.find_last_of("\\/");
	if (pos == std::string::npos)
		return path;
	else if (pos == path.length() - 1)
		return "";
	return path.substr(pos + 1, path.length() - pos - 1);
}

int BytesToInt(uint8_t * pData, int iCount)
{
	int iResult = 0;
	for (int i = 0; i < iCount; i++)
	{
		iResult += pData[i] << ((iCount - 1 - i) * 8);
	}
	return iResult;
}

std::string BytesToStr(uint8_t * pData, int iCount)
{
	std::string strResult((char*)pData, iCount);
	return strResult;
}

ByteReader::ByteReader(uint8_t* start, uint32_t len)
{
	start_ = start;
	end_ = start + len;
}

ByteReader::ByteReader(uint8_t* start, uint8_t* end)
{
	start_ = start;
	end_ = end;
	if (end_ < start_)
		end_ = start_;
}

uint8_t* ByteReader::CurrentPos() const
{
	return start_;
}

uint8_t* ByteReader::ReadBytes(uint32_t need_bytes, bool move_pos)
{
	uint8_t* ret = NULL;
	if (start_ + need_bytes <= end_)
	{
		ret = start_;
		if (move_pos)
			start_ += need_bytes;
	}
	return ret;
}

uint32_t ByteReader::RemainingSize() const
{
	return ((end_ > start_) ? (end_ - start_) : 0);
}

//////////////////////////////////////////////////////////////////////////
// BitReader

BitReader::BitReader(uint8_t* byte_start, uint32_t byte_len, uint8_t bits_left)
{
	if (!byte_start || !byte_len || bits_left < 1 || bits_left > 8)
	    return;
		
	start_ = byte_start;
	p_ = byte_start;
	end_ = byte_start + byte_len;
	bits_left_ = bits_left;
}

uint32_t BitReader::ReadU1()
{
	uint32_t r = 0;
	if (Eof())
		return r;

	bits_left_--;
	r = (*p_ >> bits_left_) & 0x01;

	if (bits_left_ == 0)
	{
		p_++;
		bits_left_ = 8;
	}

	return r;
}

void BitReader::SkipU1()
{
	if (Eof())
		return;

	bits_left_--;
	if (bits_left_ == 0)
	{
		p_++;
		bits_left_ = 8;
	}
}

uint32_t BitReader::PeekU1()
{
	uint32_t r = 0;

	if (!Eof())
		r = (*p_ >> (bits_left_ - 1)) & 0x01;

	return r;
}

uint32_t BitReader::ReadU(int nbits)
{
	uint32_t r = 0;

	for (int i = 0; i < nbits; i++)
		r |= (ReadU1() << (nbits - i - 1));

	return r;
}

void BitReader::SkipU(int nbis)
{
	if (nbis < bits_left_)
	{
		bits_left_ -= nbis;
		return;
	}

	p_ -= 1 + (nbis - bits_left_) / 8;
	bits_left_ = 8 - ((nbis - bits_left_) % 8);

	if (Eof())
	{
		p_ = end_;
		bits_left_ = 8;
	}
}

uint32_t BitReader::ReadF(int nbits) 
{
	return ReadU(nbits); 
}


uint32_t BitReader::ReadU8()
{
	if (bits_left_ == 8 && !Eof()) // can do fast read
	{
		uint32_t r = *p_;
		p_++;
		return r;
	}
	return ReadU(8);
}

uint32_t BitReader::ReadUE()
{
	int32_t r = 0;
	int i = 0;

	while (ReadU1() == 0 && i < 32 && !Eof())
		i++;

	r = ReadU(i);
	r += (1 << i) - 1;
	return r;
}

int32_t BitReader::ReadSE()
{
	int32_t r = ReadUE();

	if (r & 0x01)
		r = (r + 1) / 2;
	else
		r = -(r / 2);

	return r;
}

int BitReader::ReadBytes(uint8_t* buf, int len)
{
	int actual_len = len;
	if (actual_len > 0 && end_ - p_ < actual_len)
		actual_len = end_ - p_;
	if (actual_len <= 0)
		return 0;

	if (IsByteAligned()) 
	{
		memcpy(buf, p_, actual_len);
		p_ += actual_len;
	}
	else
	{
		for (int i = 0; i < actual_len; i++)
			buf[i] = (uint8_t)ReadU(8);
	}
	
	return actual_len;
}


int BitReader::SkipBytes(int len)
{
	int actual_len = len;
	if (actual_len > 0 && end_ - p_ < actual_len)
		actual_len = end_ - p_;
	if (actual_len <= 0)
		actual_len = 0;
	p_ += actual_len;
	return actual_len;
}

uint32_t BitReader::NextBits(int nbits)
{
	BitReader b(*this);
	return b.ReadU(nbits);
}


