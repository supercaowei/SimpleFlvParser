#ifndef _SFP_FLV_FILE_INTERNAL_H_
#define _SFP_FLV_FILE_INTERNAL_H_

#include "input_interface.h"
#include "bytes.h"
#include "amf.h"
#include "utils.h"
#include "h264_syntax.h"
#include "json/value.h"
#include <memory>
#include <list>

typedef std::list<std::shared_ptr<NaluInterface> > NaluList;

//////////////////////////////////////////////////////////////////////////
// Flv Header

struct FlvHeader : public FlvHeaderInterface
{
	FlvHeader(ByteReader& data);
	~FlvHeader() = default;

	//implement FlvHeaderInterface
	virtual bool HaveVideo() override;
	virtual bool HaveAudio() override;
	virtual uint8_t Version() override;

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

std::string GetFlvTagTypeString(FlvTagType type);

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
	virtual uint32_t GetCts() { return 0; }
	virtual std::string GetSubTypeString() { return ""; }
	virtual std::string GetFormatString() { return ""; }
	virtual std::string GetExtraInfo() { return ""; }
	virtual NaluList EnumNalus() { return NaluList(); }

protected:
	FlvTagData() {}

protected:
	bool is_good_ = false;
};

class FlvTag : public FlvTagInterface
{
public:
	FlvTag(ByteReader& data, int tag_serial);
	bool IsGood() { return is_good_; }
	NaluList EnumNalus();

	//implement FlvTagInterface
	virtual int Serial() override;
	virtual uint32_t PreviousTagSize() override;
	virtual std::string TagType() override;
	virtual uint32_t StreamId() override;
	virtual uint32_t TagSize() override;
	virtual uint32_t Pts() override;
	virtual uint32_t Dts() override;
	virtual std::string SubType() override;
	virtual std::string Format() override;
	virtual std::string ExtraInfo() override;

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

std::string GetAudioTagTypeString(AudioTagType type);

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
	AudioTagType GetAudioTagType() { return audio_tag_type_; }

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

	virtual std::string GetSubTypeString() override;
	virtual std::string GetFormatString() override;
	virtual std::string GetExtraInfo() override;

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

enum FlvVideoCodecID
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
	FlvVideoCodecID codec_id_;
	bool is_good_;

	VideoTagHeader(ByteReader& data);
};

enum VideoTagType
{
	VideoTagTypeNonAVC              = -1, //Non AVC
	VideoTagTypeAVCSequenceHeader   = 0,  //AVC sequence header
	VideoTagTypeAVCNalu             = 1,  //AVC Nalu
	VideoTagTypeAVCSequenceEnd      = 2,  //AVC end of sequence (lower level Nalu sequence ender is not required or supported)
};

std::string GetVideoTagTypeString(VideoTagType type);

class VideoTagBody
{
public:
	static std::shared_ptr<VideoTagBody> Create(ByteReader& data, FlvVideoCodecID codec_id);
	virtual ~VideoTagBody() {}
	virtual bool IsGood() { return is_good_; }
	virtual uint32_t GetCts() { return 0; }
	VideoTagType GetVideoTagType() { return video_tag_type_; }
	virtual NaluList EnumNalus() { return NaluList(); }

protected:
	VideoTagBody() = default;

protected:
	bool is_good_ = false;
	VideoTagType video_tag_type_;
};

enum EnNaluType
{
	NaluTypeUnknown    = -1,
	NaluTypeUnused     = 0,
	NaluTypeNonIDR     = 1,
	NaluTypeSliceDPA   = 2,
	NaluTypeSliceDPB   = 3,
	NaluTypeSliceDPC   = 4,
	NaluTypeIDR        = 5,
	NaluTypeSEI        = 6,
	NaluTypeSPS        = 7,
	NaluTypePPS        = 8,
	NaluTypeAUD        = 9,
	NaluTypeSeqEnd     = 10,
	NaluTypeStreamEnd  = 11,
	NaluTypeFillerData = 12,
	NaluTypeSPSExt     = 13,
	NaluTypeSliceAux   = 19,
};

std::string GetNaluTypeString(EnNaluType type);

struct NaluHeader
{
	uint8_t nal_ref_idc_;
	EnNaluType nal_unit_type_;

	NaluHeader(uint8_t b);
};

class NaluBase : public NaluInterface
{
public:
	static std::shared_ptr<NaluBase> Create(ByteReader& data, uint8_t nalu_len_size);
	NaluBase(ByteReader& data, uint8_t nalu_len_size);
	virtual ~NaluBase();
	bool IsGood() { return is_good_; }
	bool IsNoBother() { return no_bother; }
	void ReleaseRbsp();

	virtual uint8_t Importance() override;
	virtual std::string NaluType() override;
	virtual uint32_t NaluSize() override;
	virtual std::string SliceType() override;
	virtual std::string ExtraInfo() override;

private:
	//don't change ByteReader position, get the nalu type
	static EnNaluType GetNaluType(const ByteReader& data, uint8_t nalu_len_size);

public:
	static std::shared_ptr<NaluBase> CurrentSps;
	static std::shared_ptr<NaluBase> CurrentPps;

protected:
	uint32_t nalu_size_ = 0;
	std::shared_ptr<NaluHeader> nalu_header_;
	uint8_t *rbsp_ = NULL;
	uint32_t rbsp_size_ = 0;
	bool is_good_ = false;
	bool no_bother = false;
};

class NaluSps : public NaluBase
{
public:
	NaluSps(ByteReader& data, uint8_t nalu_len_size);
	std::shared_ptr<sps_t> sps_;

	virtual std::string ExtraInfo() override;
};

class NaluPps : public NaluBase
{
public:
	NaluPps(ByteReader& data, uint8_t nalu_len_size);
	std::shared_ptr<pps_t> pps_;

	virtual std::string ExtraInfo() override;
};

class NaluSlice : public NaluBase
{
public:
	NaluSlice(ByteReader& data, uint8_t nalu_len_size);

	virtual std::string SliceType() override;
	virtual std::string ExtraInfo() override;

private:
	std::shared_ptr<slice_header_t> slice_header_;
};

class NaluSEI : public NaluBase
{
public:
	NaluSEI(ByteReader& data, uint8_t nalu_len_size);
	~NaluSEI();

	virtual std::string ExtraInfo() override;

private:
	sei_t** seis_ = NULL;
	uint32_t sei_num_ = 0;
};

class VideoTagBodyAVCNalu : public VideoTagBody
{
public:
	VideoTagBodyAVCNalu(ByteReader& data);
	~VideoTagBodyAVCNalu() {}
	uint32_t GetCts() { return cts_; }
	virtual NaluList EnumNalus() override;

private:
	uint32_t cts_ = 0;
	std::list<std::shared_ptr<NaluBase> > nalu_list_;
};

struct AVCDecoderConfigurationRecord
{
	uint8_t  configurationVersion;
	uint8_t  AVCProfileIndication;
	uint8_t  profile_compatibility;
	uint8_t  AVCLevelIndication;
	uint8_t  lengthSizeMinusOne;
	uint8_t  numOfSequenceParameterSets;
	std::shared_ptr<NaluBase> sps_nal_;
	uint8_t  numOfPictureParameterSets;
	std::shared_ptr<NaluBase> pps_nal_;

	bool is_good_;

	AVCDecoderConfigurationRecord(ByteReader& data);
};

class VideoTagBodySpsPps : public VideoTagBody
{
public:
	VideoTagBodySpsPps(ByteReader& data);
	~VideoTagBodySpsPps() {}
	uint32_t GetCts() { return cts_; }

	virtual NaluList EnumNalus() override;

private:
	uint32_t cts_ = 0;
	std::shared_ptr<AVCDecoderConfigurationRecord> avc_config_;
};

class VideoTagBodySequenceEnd : public VideoTagBody
{
public:
	VideoTagBodySequenceEnd(ByteReader& data);
	uint32_t GetCts() { return cts_; }

private:
	uint32_t cts_ = 0;
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

	virtual uint32_t GetCts() override;
	virtual std::string GetSubTypeString() override;
	virtual std::string GetFormatString() override;
	virtual std::string GetExtraInfo() override;
	virtual NaluList EnumNalus() override;

private:
	std::shared_ptr<VideoTagHeader> video_tag_header_;
	std::shared_ptr<VideoTagBody> video_tag_body_;
};



#endif

