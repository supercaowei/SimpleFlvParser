#ifndef _SFP_FLV_FILE_INTERNAL_H_
#define _SFP_FLV_FILE_INTERNAL_H_

#include "bytes.h"
#include "amf.h"
#include "utils.h"
#include "h264_syntax.h"
#include "json/value.h"
#include <memory>
#include <list>

//////////////////////////////////////////////////////////////////////////
// Flv Header

struct FlvHeader
{
	FlvHeader(ByteReader& data);
	~FlvHeader() = default;

	uint8_t  version_;
	bool     has_video_;
	bool     has_audio_;
	uint32_t header_size_;
	bool     is_good_;
};

//////////////////////////////////////////////////////////////////////////
//Flv Tag

enum FlvTagType
{
	FlvTagTypeUnknown = 0,
	FlvTagTypeAudio = 8,
	FlvTagTypeVideo = 9,
	FlvTagTypeScriptData = 18,
};

struct FlvTagHeader
{
	FlvTagHeader(ByteReader& data);
	~FlvTagHeader() = default;

	FlvTagType tag_type_;
	uint32_t   tag_data_size_; //not including tag header size
	uint32_t   timestamp_;
	uint32_t   stream_id_;
	bool       is_good_;
};

class FlvTagData
{
public:
	static std::shared_ptr<FlvTagData> Create(ByteReader& data, uint32_t tag_data_size, FlvTagType tag_type);
	virtual ~FlvTagData() {}
	virtual bool IsGood() { return is_good_; }

protected:
	FlvTagData() {}

protected:
	bool is_good_ = false;
};

class FlvTag
{
public:
	FlvTag(ByteReader& data, int tag_serial);
	bool IsGood() { return is_good_; }

private:
	int tag_serial_ = -1;
	uint32_t previous_tag_size_ = 0;
	std::shared_ptr<FlvTagHeader> tag_header_;
	std::shared_ptr<FlvTagData> tag_data_;
	bool is_good_ = false;
};


//////////////////////////////////////////////////////////////////////////
// 3 types of tag only differ in tag data
// Script data tag

class FlvTagDataScript : public FlvTagData
{
public:
	FlvTagDataScript(ByteReader& data);
	~FlvTagDataScript() {}

private:
	static void DecodeAMF(const AMFObject* amf, Json::Value& json);

private:
	Json::Value script_data_;
};

//////////////////////////////////////////////////////////////////////////
// Audio Tag

enum AudioFormat
{
	AudioFormatLinearPCMPE     = 0,  //0 -- Linear PCM, platform endian
	AudioFormatADPCM           = 1,  //1 -- ADPCM
	AudioFormatMP3             = 2,  //2 -- MP3
	AudioFormatLinearPCMLE     = 3,  //3 -- Linear PCM, little endian
	AudioFormatNellymoser16kHz = 4,  //4 -- Nellymoser 16kHz mono
	AudioFormatNellymoser8kHz  = 5,  //5 -- Nellymoser 8kHz mono
	AudioFormatNellymoser      = 6,  //6 -- Nellymoser
	AudioFormatG711ALawPCM     = 7,  //7 -- G.711 A-law logarithmic PCM
	AudioFormatG711MuLawPCM    = 8,  //8 -- G.711 mu-law logarithmic PCM
	AudioFormatReserved        = 9,  //9 -- reserved
	AudioFormatAAC             = 10, //10 �C AAC
	AudioFormatSpeex           = 11, //11 - Speex
	AudioFormatMP38kHZ         = 14, //14 - MP3 8-kHz
	AudioFormatDevideSpecific  = 15, //15 - Device-specific sound
};

enum AudioSamplerate
{
	AudioSamplerate5p5K      = 0, //0 -- 5.5KHz
	AudioSamplerate11K       = 1, //1 -- 11kHz
	AudioSamplerate22K       = 2, //2 -- 22kHz
	AudioSamplerate44K       = 3, //3 -- 44kHz
};

enum AudioSampleWidth
{
	AudioSampleWidth8Bit  = 0,  //0 -- snd8Bit
	AudioSampleWidth16Bit = 1,  //1 -- snd16Bit
};

enum AudioChannelNum
{
	AudioChannelMono    = 0, //0 -- sndMomo
	AudioChannelStereo  = 1, //1 -- sndStereo
};

enum AudioTagType
{
	AudioTagTypeNonAAC    = -1,
	AudioTagTypeAACConfig = 0,
	AudioTagTypeAACData   = 1
};

struct AudioTagHeader
{
	AudioFormat audio_format_;
	AudioSamplerate samplerate_;
	AudioSampleWidth sample_width_;
	AudioChannelNum channel_num_;
	bool is_good_ = false;

	AudioTagHeader(ByteReader& data);
};

class AudioTagBody
{
public:
	static std::shared_ptr<AudioTagBody> Create(ByteReader& data, AudioFormat audio_format);
	virtual ~AudioTagBody() {}
	virtual bool IsGood() { return is_good_; }

protected:
	AudioTagBody() = default;

protected:
	AudioTagType audio_tag_type_;
	bool is_good_ = false;
};

struct AudioSpecificConfig
{
	uint8_t audioObjectType;
	uint8_t samplingFrequencyIndex;
	uint8_t channelConfiguration;
	uint8_t frameLengthFlag;
	uint8_t dependsOnCoreCoder;
	uint8_t extensionFlag;

	bool is_good_;

	AudioSpecificConfig(ByteReader& data);
};

class AudioTagBodyAACConfig : public AudioTagBody
{
public:
	AudioTagBodyAACConfig(ByteReader& data);

private:
	std::shared_ptr<AudioSpecificConfig> aac_config_;
};

class AudioTagBodyAACData : public AudioTagBody
{
public:
	AudioTagBodyAACData(ByteReader& data);
};

class AudioTagBodyNonAAC : public AudioTagBody
{
public:
	AudioTagBodyNonAAC(ByteReader& data);
};

class FlvTagDataAudio : public FlvTagData
{
public:
	FlvTagDataAudio(ByteReader& data);

private:
	std::shared_ptr<AudioTagHeader> audio_tag_header_;
	std::shared_ptr<AudioTagBody> audio_tag_body_;
};

//////////////////////////////////////////////////////////////////////////
// Video Tag

enum FlvVideoFrameType
{
	FlvVideoFrameTypeKeyFrame              = 1, //keyframe (for AVC, a seekable frame)
	FlvVideoFrameTypeInterFrame            = 2, //inter frame (for AVC, a non-seekable frame)
	FlvVideoFrameTypeDisposableInterFrame  = 3, //disposable inter frame (H.263 only)
	FlvVideoFrameTypeGeneratedKeyFrame     = 4, //generated keyframe (reserved for server use only)
	FlvVideoFrameTypeVideoInfo             = 5, //video info/command frame
};

enum FlvVideoCodeID
{
	FlvVideoCodeIDJPEG              = 1, //JPEG (currently unused)
	FlvVideoCodeIDSorensonH263      = 2, //Sorenson H.263
	FlvVideoCodeIDScreenVideo       = 3, //Screen video
	FlvVideoCodeIDOn2VP6            = 4, //On2 VP6
	FlvVideoCodeIDOn2VP6Alpha       = 5, //On2 VP6 with alpha channel
	FlvVideoCodeIDScreenVideoV2     = 6, //Screen video version 2
	FlvVideoCodeIDAVC               = 7, //AVC
};

struct VideoTagHeader
{
	FlvVideoFrameType frame_type_;
	FlvVideoCodeID codec_id_;
	bool is_good_;

	VideoTagHeader(ByteReader& data);
};

enum VideoTagType
{
	VideoTagTypeNonAVC              = -1, //Non AVC
	VideoTagTypeAVCSequenceHeader   = 0,  //AVC sequence header
	VideoTagTypeAVCNALU             = 1,  //AVC NALU
	VideoTagTypeAVCSequenceEnd      = 2,  //AVC end of sequence (lower level NALU sequence ender is not required or supported)
};

class VideoTagBody
{
public:
	static std::shared_ptr<VideoTagBody> Create(ByteReader& data, FlvVideoCodeID codec_id);
	virtual ~VideoTagBody() {}
	virtual bool IsGood() { return is_good_; }

protected:
	VideoTagBody() = default;

protected:
	bool is_good_ = false;
	VideoTagType video_tag_type_;
};

enum NALUType
{
	NALUTypeUnknown    = -1,
	NALUTypeUnused     = 0,
	NALUTypeNonIDR     = 1,
	NALUTypeSliceDPA   = 2,
	NALUTypeSliceDPB   = 3,
	NALUTypeSliceDPC   = 4,
	NALUTypeIDR        = 5,
	NALUTypeSEI        = 6,
	NALUTypeSPS        = 7,
	NALUTypePPS        = 8,
	NALUTypeAUD        = 9,
	NALUTypeSeqEnd     = 10,
	NALUTypeStreamEnd  = 11,
	NALUTypeFillerData = 12,
	NALUTypeSPSExt     = 13,
	NALUTypeSliceAux   = 19,
};

struct NALUHeader
{
	uint8_t nal_ref_idc_;
	NALUType nal_unit_type_;

	NALUHeader(uint8_t b);
};

class NALUBase
{
public:
	static std::shared_ptr<NALUBase> Create(ByteReader& data, uint8_t nalu_len_size);
	NALUBase(ByteReader& data, uint8_t nalu_len_size);
	virtual ~NALUBase();
	bool IsGood() { return is_good_; }
	bool IsNoBother() { return no_bother; }
	void ReleaseRbsp();

private:
	//don't change ByteReader position, get the nalu type
	static NALUType GetNALUType(const ByteReader& data, uint8_t nalu_len_size);

public:
	static std::shared_ptr<NALUBase> CurrentSPS;
	static std::shared_ptr<NALUBase> CurrentPPS;

protected:
	uint32_t nalu_size_ = 0;
	std::shared_ptr<NALUHeader> nalu_header_;
	uint8_t *rbsp_ = NULL;
	uint32_t rbsp_size_ = 0;
	bool is_good_ = false;
	bool no_bother = false;
};

class NALUSPS : public NALUBase
{
public:
	NALUSPS(ByteReader& data, uint8_t nalu_len_size);
	std::shared_ptr<sps_t> sps_;
};

class NALUPPS : public NALUBase
{
public:
	NALUPPS(ByteReader& data, uint8_t nalu_len_size);
	std::shared_ptr<pps_t> pps_;
};

class NALUSlice : public NALUBase
{
public:
	NALUSlice(ByteReader& data, uint8_t nalu_len_size);

private:
	std::shared_ptr<slice_header_t> slice_header_;
};

class NALUSEI : public NALUBase
{
public:
	NALUSEI(ByteReader& data, uint8_t nalu_len_size);
	~NALUSEI();

private:
	sei_t** seis_ = NULL;
	uint32_t sei_num_ = 0;
};

class VideoTagBodyAVCNALU : public VideoTagBody
{
public:
	VideoTagBodyAVCNALU(ByteReader& data);
	~VideoTagBodyAVCNALU() {}

private:
	uint32_t cts_;
	std::list<std::shared_ptr<NALUBase> > nalu_list_;
};

struct AVCDecoderConfigurationRecord
{
	uint8_t  configurationVersion;
	uint8_t  AVCProfileIndication;
	uint8_t  profile_compatibility;
	uint8_t  AVCLevelIndication;
	uint8_t  lengthSizeMinusOne;
	uint8_t  numOfSequenceParameterSets;
	std::shared_ptr<NALUBase> sps_nal_;
	uint8_t  numOfPictureParameterSets;
	std::shared_ptr<NALUBase> pps_nal_;

	bool is_good_;

	AVCDecoderConfigurationRecord(ByteReader& data);
};

class VideoTagBodySPSPPS : public VideoTagBody
{
public:
	VideoTagBodySPSPPS(ByteReader& data);
	~VideoTagBodySPSPPS() {}

private:
	uint32_t cts_;
	std::shared_ptr<AVCDecoderConfigurationRecord> avc_config_;
};

class VideoTagBodySequenceEnd : public VideoTagBody
{
public:
	VideoTagBodySequenceEnd(ByteReader& data);

private:
	uint32_t cts_;
};

class VideoTagBodyNonAVC : public VideoTagBody
{
public:
	VideoTagBodyNonAVC(ByteReader& data);
};

class FlvTagDataVideo : public FlvTagData
{
public:
	FlvTagDataVideo(ByteReader& data);
	~FlvTagDataVideo() {}

private:
	std::shared_ptr<VideoTagHeader> video_tag_header_;
	std::shared_ptr<VideoTagBody> video_tag_body_;
};



#endif

