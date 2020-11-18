#include "flv_file_internal.h"
#include "utils.h"
#include "json/reader.h"

#define FLV_HEADER_SIZE           9
#define PREVIOUS_TAG_SIZE_SIZE    4
#define FLV_TAG_HEADER_SIZE       11
#define FLV_VIDEO_TAG_HEADER_SIZE 5
#define FLV_AUDIO_TAG_HEADER_SIZE 1
#define STRING_UNKNOWN "Unknown"

extern bool print_sei;

FlvHeader::FlvHeader(ByteReader& data)
{
	if (data.RemainingSize() < FLV_HEADER_SIZE)
		return;

	uint8_t* header_pos = data.ReadBytes(3);
	if (strncmp((char *)header_pos, "FLV", 3)) //not begin with "FLV"
	{
		printf("This is not an FLV file.\n");
		return;
	}
	if (*(header_pos = data.ReadBytes(1)) != 0x01)
		return;
	if ((*(header_pos = data.ReadBytes(1)) & 0xfa) != 0)
		return;

	has_audio_ = (*header_pos & 0x04) != 0;
	has_video_ = (*header_pos & 0x01) != 0;
	version_ = 0x01;
	header_size_ = (uint32_t)BytesToInt(data.ReadBytes(4), 4);
	is_good_ = true;

	if (header_size_ > FLV_HEADER_SIZE)
		data.ReadBytes(header_size_ - FLV_HEADER_SIZE);
}

bool FlvHeader::HaveVideo()
{
	return has_video_;
}

bool FlvHeader::HaveAudio()
{
	return has_audio_;
}

uint8_t FlvHeader::Version()
{
	return version_;
}

uint8_t FlvHeader::HeaderSize()
{
	return header_size_;
}

std::string GetFlvTagTypeString(FlvTagType type)
{
	switch (type)
	{
	case FlvTagTypeAudio:
		return "AudioTag";
	case FlvTagTypeVideo:
		return "VideoTag";
	case FlvTagTypeScriptData:
		return "Script";
	default:
		return "UnknownTagType";
	}
}

uint32_t FlvTagHeader::LastTagTimestamp = 0;

FlvTagHeader::FlvTagHeader(ByteReader& data)
{
	memset(this, 0, sizeof(FlvTagHeader));

	if (data.RemainingSize() < FLV_TAG_HEADER_SIZE)
		return;

	tag_type_ = (FlvTagType)*data.ReadBytes(1);
	tag_data_size_ = (uint32_t)BytesToInt(data.ReadBytes(3), 3);
	timestamp_ = (uint32_t)BytesToInt(data.ReadBytes(3), 3);
	uint32_t timestamp_extend = (uint32_t)*data.ReadBytes(1);
	if (timestamp_extend)
		timestamp_ |= (timestamp_extend << 24);
	stream_id_ = (uint32_t)BytesToInt(data.ReadBytes(3), 3);
	if (stream_id_ != 0)
		return;

	switch (tag_type_)
	{
	case FlvTagTypeAudio:
	case FlvTagTypeVideo:
	case FlvTagTypeScriptData:
		break;
	default:
		printf("Unknown tag type %d\n", tag_type_);
		return;
	}

	is_good_ = true;

	if (timestamp_ < LastTagTimestamp)
		printf("Warning: current timestamp %u is smaller than last timestamp %u.\n", timestamp_, LastTagTimestamp);
	LastTagTimestamp = timestamp_;
}

uint32_t FlvTag::LastVideoDts = 0;
uint32_t FlvTag::LastAudioDts = 0;

FlvTag::FlvTag(ByteReader& data, int tag_serial, const std::shared_ptr<DemuxInterface>& demux_output)
{
	if (data.RemainingSize() < PREVIOUS_TAG_SIZE_SIZE + FLV_TAG_HEADER_SIZE)
	{
		data.ReadBytes(data.RemainingSize());
		return;
	}

	tag_serial_ = tag_serial;
	previous_tag_size_ = (uint32_t)BytesToInt(data.ReadBytes(PREVIOUS_TAG_SIZE_SIZE), PREVIOUS_TAG_SIZE_SIZE);
	tag_header_ = std::make_shared<FlvTagHeader>(data);
	if (!tag_header_ || !tag_header_->is_good_)
	{
		if (tag_header_->tag_data_size_ > 0)
			data.ReadBytes(tag_header_->tag_data_size_ < data.RemainingSize() ? tag_header_->tag_data_size_ : data.RemainingSize());
		else //error
			data.ReadBytes(data.RemainingSize());
		return;
	}
	tag_data_ = FlvTagData::Create(data, tag_header_->tag_data_size_, tag_header_->tag_type_, demux_output);
	if (!tag_data_ || !tag_data_->IsGood())
		return;
	tag_data_->SetTagSerial(tag_serial_);

	is_good_ = true;
}

NaluList FlvTag::EnumNalus()
{
	if (tag_data_)
		return tag_data_->EnumNalus();
	return NaluList();
}

int FlvTag::Serial()
{
	return tag_serial_;
}

uint32_t FlvTag::PreviousTagSize()
{
	return previous_tag_size_;
}

std::string FlvTag::TagType()
{
	if (tag_header_)
		return GetFlvTagTypeString(tag_header_->tag_type_);
	return "";
}

uint32_t FlvTag::StreamId()
{
	if (tag_header_)
		return tag_header_->stream_id_;
	return 0;
}

uint32_t FlvTag::TagSize()
{
	if (tag_header_)
		return FLV_TAG_HEADER_SIZE + tag_header_->tag_data_size_;
	return 0;
}

uint32_t FlvTag::Pts()
{
	if (tag_header_ && tag_data_)
		return tag_header_->timestamp_ + tag_data_->GetCts();
	return 0;
}

uint32_t FlvTag::Dts()
{
	if (tag_header_)
		return tag_header_->timestamp_;
	return 0;
}

int FlvTag::DtsDiff()
{
	int32_t diff = 0;
	if (tag_header_)
	{
		if (tag_header_->tag_type_ == FlvTagTypeVideo)
		{
			diff = tag_header_->timestamp_ - LastVideoDts;
			LastVideoDts = tag_header_->timestamp_;
		}
		else if (tag_header_->tag_type_ == FlvTagTypeAudio)
		{
			diff = tag_header_->timestamp_ - LastAudioDts;
			LastAudioDts = tag_header_->timestamp_;
		}
	}
	return diff;
}

std::string FlvTag::SubType()
{
	if (tag_data_)
		return tag_data_->GetSubTypeString();
	return "";
}

std::string FlvTag::Format()
{
	if (tag_data_)
		return tag_data_->GetFormatString();
	return "";
}

std::string FlvTag::ExtraInfo()
{
	if (tag_data_)
		return tag_data_->GetExtraInfo();
	return "";
}

std::shared_ptr<FlvTagData> FlvTagData::Create(ByteReader& data, uint32_t tag_data_size, FlvTagType tag_type, const std::shared_ptr<DemuxInterface>& demux_output)
{
	if (data.RemainingSize() < tag_data_size)
	{
		data.ReadBytes(data.RemainingSize());
		return std::shared_ptr<FlvTagData>(nullptr);
	}

	ByteReader tag_data(data.ReadBytes(tag_data_size), tag_data_size);

	switch (tag_type)
	{
	case FlvTagTypeAudio:
		return std::make_shared<FlvTagDataAudio>(tag_data, demux_output);
	case FlvTagTypeVideo:
		return std::make_shared<FlvTagDataVideo>(tag_data, demux_output);
	case FlvTagTypeScriptData:
		return std::make_shared<FlvTagDataScript>(tag_data);
	default:
		return std::shared_ptr<FlvTagData>(nullptr);
	}
}

FlvTagDataScript::FlvTagDataScript(ByteReader& data)
{
	if (!data.RemainingSize())
		return;

	AMFObject amf = {0, 0};
	int decoded_size = AMF_Decode(&amf, (const char *)data.CurrentPos(), data.RemainingSize(), 0);
	if (decoded_size <= 0)
	{
		AMF_Reset(&amf);
		return;
	}
	data.ReadBytes(decoded_size);

	DecodeAMF(&amf, script_data_);
	if (script_data_.isNull() || (!script_data_.isObject() && !script_data_.isArray()))
		return;

	is_good_ = true;
}

std::string FlvTagDataScript::GetExtraInfo()
{
	return script_data_.toStyledString();
}

void FlvTagDataScript::DecodeAMF(const AMFObject* amf, Json::Value& json)
{
	for (int i = 0; i < amf->o_num; i++)
	{
		std::string key;
		Json::Value value;
		AMFObjectProperty* amf_item = &amf->o_props[i];
		if (amf_item->p_name.av_val && amf_item->p_name.av_len)
			key = std::string(amf_item->p_name.av_val, 0, amf_item->p_name.av_len);
		switch (amf_item->p_type)
		{
		case AMF_NUMBER:
		case AMF_BOOLEAN:
		case AMF_DATE:
			value = amf_item->p_vu.p_number;
			break;
		case AMF_STRING:
		case AMF_LONG_STRING:
		case AMF_XML_DOC:
			if (amf_item->p_vu.p_aval.av_val && amf_item->p_vu.p_aval.av_len)
				value = std::string(amf_item->p_vu.p_aval.av_val, 0, amf_item->p_vu.p_aval.av_len);
			else
				value = "";
			break;
		case AMF_OBJECT:
		case AMF_ECMA_ARRAY:
		case AMF_STRICT_ARRAY:
		case AMF_AVMPLUS:
			DecodeAMF(&amf_item->p_vu.p_object, value);
			break;
		default:
			break;
		}

		if (!value.isNull())
		{
			if (key.empty())
				json.append(value);
			else
				json[key] = value;
		}
	}
}

FlvTagDataAudio::FlvTagDataAudio(ByteReader& data, const std::shared_ptr<DemuxInterface>& demux_output)
{
	audio_tag_header_ = std::make_shared<AudioTagHeader>(data);
	if (!audio_tag_header_ || !audio_tag_header_->is_good_)
		return;

	audio_tag_body_ = AudioTagBody::Create(data, audio_tag_header_->audio_format_, demux_output);
	if (!audio_tag_body_ || !audio_tag_body_->IsGood())
		return;

	is_good_ = true;
}

std::string FlvTagDataAudio::GetSubTypeString()
{
	if (audio_tag_body_)
		return GetAudioTagTypeString(audio_tag_body_->GetAudioTagType());
	return FlvTagData::GetSubTypeString();
}

std::string FlvTagDataAudio::GetFormatString()
{
	if (audio_tag_header_)
		return GetAudioFormatString(audio_tag_header_->audio_format_) + " | "
		+ GetAudioSamplerateString(audio_tag_header_->samplerate_) + " | "
		+ GetAudioSampleWidthString(audio_tag_header_->sample_width_) + " | "
		+ GetAudioChannelNumString(audio_tag_header_->channel_num_);

	return "";
}

std::string FlvTagDataAudio::GetExtraInfo()
{
	if (audio_tag_body_)
		return audio_tag_body_->GetExtraInfo();
	return "";
}

AudioTagHeader::AudioTagHeader(ByteReader& data)
{
	if (data.RemainingSize() < FLV_AUDIO_TAG_HEADER_SIZE)
		return;

	memset(this, 0, sizeof(AudioTagHeader));
	uint8_t b = *data.ReadBytes(1);

	audio_format_ = (AudioFormat)(b >> 4);
	switch (audio_format_)
	{
	case AudioFormatLinearPCMPE:
	case AudioFormatADPCM:
	case AudioFormatMP3:
	case AudioFormatLinearPCMLE:
	case AudioFormatNellymoser16kHz:
	case AudioFormatNellymoser8kHz:
	case AudioFormatNellymoser:
	case AudioFormatG711ALawPCM:
	case AudioFormatG711MuLawPCM:
	case AudioFormatReserved:
	case AudioFormatAAC:
	case AudioFormatSpeex:
	case AudioFormatMP38kHZ:
	case AudioFormatDevideSpecific:
		break;
	default:
		return;
	}

	samplerate_ = (AudioSamplerate)((b >> 2) & 0x03);
	switch (samplerate_)
	{
	case AudioSamplerate5p5K:
	case AudioSamplerate11K:
	case AudioSamplerate22K:
	case AudioSamplerate44K:
		break;
	default:
		return;
	}

	sample_width_ = (AudioSampleWidth)((b >> 1) & 0x01);
	switch (sample_width_)
	{
	case AudioSampleWidth8Bit:
	case AudioSampleWidth16Bit:
		break;
	default:
		return;
	}

	channel_num_ = (AudioChannelNum)(b & 0x01);
	switch (channel_num_)
	{
	case AudioChannelMono:
	case AudioChannelStereo:
		break;
	default:
		return;
	}

	is_good_ = true;
}

std::string GetAudioFormatString(AudioFormat fmt)
{
	switch (fmt)
	{
	case AudioFormatLinearPCMPE:
		return "Linear PCM PE";
	case AudioFormatADPCM:
		return "ADPCM";
	case AudioFormatMP3:
		return "MP3";
	case AudioFormatLinearPCMLE:
		return "Linear PCM LE";
	case AudioFormatNellymoser16kHz:
		return "Nellymoser 16kHz";
	case AudioFormatNellymoser8kHz:
		return "Nellymoser 8kHz";
	case AudioFormatNellymoser:
		return "Nellymoser";
	case AudioFormatG711ALawPCM:
		return "G711 ALaw PCM";
	case AudioFormatG711MuLawPCM:
		return "G711 MuLaw PCM";
	case AudioFormatReserved:
		return "Reserved";
	case AudioFormatAAC:
		return "AAC";
	case AudioFormatSpeex:
		return "Speex";
	case AudioFormatMP38kHZ:
		return "MP3 8kHZ";
	case AudioFormatDevideSpecific:
		return "Devide Specific";
	default:
		return STRING_UNKNOWN;
	}
}

std::string GetAudioSamplerateString(AudioSamplerate samplerate)
{
	switch (samplerate)
	{
	case AudioSamplerate5p5K:
		return "5.5kHz";
	case AudioSamplerate11K:
		return "11kHz";
	case AudioSamplerate22K:
		return "22kHz";
	case AudioSamplerate44K:
		return "44kHz";
	default:
		return STRING_UNKNOWN;
	}
}

std::string GetAudioSampleWidthString(AudioSampleWidth sample_width)
{
	switch (sample_width)
	{
	case AudioSampleWidth8Bit:
		return "8bit";
	case AudioSampleWidth16Bit:
		return "16bit";
	default:
		return STRING_UNKNOWN;
	}
}

std::string GetAudioChannelNumString(AudioChannelNum channel_num)
{
	switch (channel_num)
	{
	case AudioChannelMono:
		return "Mono";
	case AudioChannelStereo:
		return "Stereo";
	default:
		return STRING_UNKNOWN;
	}
}

std::string GetAudioTagTypeString(AudioTagType type)
{
	switch (type)
	{
	case AudioTagTypeAACConfig:
		return "AAC Config Frame";
	case AudioTagTypeAACData:
		return "AAC Data Frame";
	default:
		return STRING_UNKNOWN;
	}
}

std::shared_ptr<AudioSpecificConfig> AudioTagBody::CurrentAudioConfig = NULL;

std::shared_ptr<AudioTagBody> AudioTagBody::Create(ByteReader& data, AudioFormat audio_format, const std::shared_ptr<DemuxInterface>& demux_output)
{
	switch (audio_format)
	{
	case AudioFormatAAC:
	{
		uint8_t b = *data.ReadBytes(1);
		if (b == AudioTagTypeAACConfig)
			return std::make_shared<AudioTagBodyAACConfig>(data, demux_output);
		else if (b == AudioTagTypeAACData)
			return std::make_shared<AudioTagBodyAACData>(data, demux_output);
		else
			return std::shared_ptr<AudioTagBody>(nullptr);
	}
	default:
		return std::make_shared<AudioTagBodyNonAAC>(data);
	}
}

std::string GetMPEG4AudioObjectTypeString(MPEG4AudioObjectType type)
{
	switch (type)
	{
	case MPEG4AudioObjectTypeNull:
		return "Null";
	case MPEG4AudioObjectTypeAACMain:
		return "AAC Main";
	case MPEG4AudioObjectTypeAACLC:
		return "AAC LC (Low Complexity)";
	case MPEG4AudioObjectTypeAACSSR:
		return "AAC SSR (Scalable Sample Rate)";
	case MPEG4AudioObjectTypeAACLTP:
		return "AAC LTP (Long Term Prediction)";
	case MPEG4AudioObjectTypeSBR:
		return "SBR (Spectral Band Replication)";
	case MPEG4AudioObjectTypeAACScalable:
		return "AAC Scalable";
	case MPEG4AudioObjectTypeTwinVQ:
		return "TwinVQ";
	case MPEG4AudioObjectTypeCELP:
		return "CELP (Code Excited Linear Prediction)";
	case MPEG4AudioObjectTypeHXVC:
		return "HXVC (Harmonic Vector eXcitation Coding)";
	case MPEG4AudioObjectTypeReserved1:
		return "Reserved";
	case MPEG4AudioObjectTypeReserved2:
		return "Reserved";
	case MPEG4AudioObjectTypeTTSI:
		return "TTSI (Text-To-Speech Interface)";
	case MPEG4AudioObjectTypeMainSynthesis:
		return "Main Synthesis";
	case MPEG4AudioObjectTypeWavetableSynthesis:
		return "Wavetable Synthesis";
	case MPEG4AudioObjectTypeGeneralMIDI:
		return "General MIDI";
	case MPEG4AudioObjectTypeASAE:
		return "Algorithmic Synthesis and Audio Effects";
	case MPEG4AudioObjectTypeERAACLC:
		return "ER (Error Resilient) AAC LC";
	case MPEG4AudioObjectTypeReserved3:
		return "Reserved";
	case MPEG4AudioObjectTypeERAACLTP:
		return "ER (Error Resilient) AAC LTP";
	case MPEG4AudioObjectTypeERAACScalable:
		return "ER (Error Resilient) AAC Scalable";
	case MPEG4AudioObjectTypeERTwinVQ:
		return "ER (Error Resilient) TwinVQ";
	case MPEG4AudioObjectTypeERBSAC:
		return "ER (Error Resilient) BSAC (Bit-Sliced Arithmetic Coding)";
	case MPEG4AudioObjectTypeERAACLD:
		return "ER (Error Resilient) AAC LD (Low Delay)";
	case MPEG4AudioObjectTypeERCELP:
		return "ER (Error Resilient) CELP (Code Excited Linear Prediction)";
	case MPEG4AudioObjectTypeERHVXC:
		return "ER (Error Resilient) HVXC (Harmonic Vector eXcitation Coding)";
	case MPEG4AudioObjectTypeERHILN:
		return "ER (Error Resilient) HILN (Harmonic and Individual Lines plus Noise)";
	case MPEG4AudioObjectTypeERParametric:
		return "ER (Error Resilient) Parametric";
	case MPEG4AudioObjectTypeSSC:
		return "SSC (SinuSoidal Coding)";
	case MPEG4AudioObjectTypePS:
		return "PS (Parametric Stereo)";
	case MPEG4AudioObjectTypeMPEGSurround:
		return "MPEG Surround";
	case MPEG4AudioObjectTypeEscapeValue:
		return "Escape value";
	case MPEG4AudioObjectTypeLayer1:
		return "Layer - 1";
	case MPEG4AudioObjectTypeLayer2:
		return "Layer - 2";
	case MPEG4AudioObjectTypeLayer3:
		return "Layer - 3";
	case MPEG4AudioObjectTypeDST:
		return "DST (Direct Stream Transfer)";
	case MPEG4AudioObjectTypeALS:
		return "ALS (Audio Lossless)";
	case MPEG4AudioObjectTypeSLS:
		return "SLS (Scalable LosslesS)";
	case MPEG4AudioObjectTypeSLSNonCore:
		return "SLS (Scalable LosslesS) non-core";
	case MPEG4AudioObjectTypeERAACELD:
		return "ER (Error Resilient) AAC ELD (Enhanced Low Delay)";
	case MPEG4AudioObjectTypeSMRSimple:
		return "SMR (Symbolic Music Representation) Simple";
	case MPEG4AudioObjectTypeSMRMain:
		return "SMR (Symbolic Music Representation) Main";
	case MPEG4AudioObjectTypeUSACNoSBR:
		return "USAC (Unified Speech and Audio Coding) (no SBR)";
	case MPEG4AudioObjectTypeSAOC:
		return "SAOC (Spatial Audio Object Coding)";
	case MPEG4AudioObjectTypeLDMPEGSurround:
		return "LD MPEG Surround";
	case MPEG4AudioObjectTypeUSAC:
		return "USAC (Unified Speech and Audio Coding)";
	default:
		return "Null";
	}
}

std::string GetMPEG4AudioSamplerateString(MPEG4AudioSamplerate samplerate)
{
	switch (samplerate)
	{
	case MPEG4AudioSamplerate96000Hz:
		return "96000 Hz";
	case MPEG4AudioSamplerate88200Hz:
		return "88200 Hz";
	case MPEG4AudioSamplerate64000Hz:
		return "64000 Hz";
	case MPEG4AudioSamplerate48000Hz:
		return "48000 Hz";
	case MPEG4AudioSamplerate44100Hz:
		return "44100 Hz";
	case MPEG4AudioSamplerate32000Hz:
		return "32000 Hz";
	case MPEG4AudioSamplerate24000Hz:
		return "24000 Hz";
	case MPEG4AudioSamplerate22050Hz:
		return "22050 Hz";
	case MPEG4AudioSamplerate16000Hz:
		return "16000 Hz";
	case MPEG4AudioSamplerate12000Hz:
		return "12000 Hz";
	case MPEG4AudioSamplerate11025Hz:
		return "11025 Hz";
	case MPEG4AudioSamplerate8000Hz:
		return "8000 Hz";
	case MPEG4AudioSamplerate7350Hz:
		return "7350 Hz";
	case MPEG4AudioSamplerateReserved1:
		return "Reserved";
	case MPEG4AudioSamplerateReserved2:
		return "Reserved";
	case MPEG4AudioSamplerateWrittenExplictly:
		return "frequency is written explictly";
	default:
		return STRING_UNKNOWN;
	}
}

AudioSpecificConfig::AudioSpecificConfig(ByteReader& data)
{
	memset(this, 0, sizeof(AudioSpecificConfig));
	if (data.RemainingSize() < 2)
		return;

	uint16_t u16 = (*data.ReadBytes(1) << 8) | (*data.ReadBytes(1));
	audioObjectType = (uint8_t)(u16 >> 11); //high 5 bits
	if (audioObjectType >= MPEG4AudioObjectTypeEscapeValue)
	{
		printf("Audio object type is escape value.\n");
		return;
	}
	samplingFrequencyIndex = (uint8_t)((u16 >> 7) & 0x000f); //next 4 bits
	if (samplingFrequencyIndex >= MPEG4AudioSamplerateWrittenExplictly)
	{
		printf("Sampling frequency index is written explictly.\n");
		return;
	}
	channelConfiguration = (uint8_t)((u16 >> 3) & 0x000f); //next 4 bits
	frameLengthFlag = (uint8_t)((u16 >> 2) & 0x0001); //next 1 bit
	dependsOnCoreCoder = (uint8_t)((u16 >> 1) & 0x0001); //next 1 bit
	extensionFlag = (uint8_t)(u16 & 0x0001); //last bit

	is_good_ = true;
}

std::string AudioSpecificConfig::Serialize()
{
	Json::Value root, aac_config;
	aac_config["object_type"] = audioObjectType;
	aac_config["object_type_name"] = GetMPEG4AudioObjectTypeString((MPEG4AudioObjectType)audioObjectType);
	aac_config["frequency_index"] = samplingFrequencyIndex;
	aac_config["frequency"] = GetMPEG4AudioSamplerateString((MPEG4AudioSamplerate)samplingFrequencyIndex);
	aac_config["channel_configuration"] = channelConfiguration;
	aac_config["frame_length_flag"] = frameLengthFlag;
	aac_config["depends_on_core_coder"] = dependsOnCoreCoder;
	aac_config["extension_flag"] = extensionFlag;
	root["aac_config"] = aac_config;
	return root.toStyledString();
}

AudioTagBodyAACConfig::AudioTagBodyAACConfig(ByteReader& data, const std::shared_ptr<DemuxInterface>& demux_output)
{
	aac_config_ = std::make_shared<AudioSpecificConfig>(data);
	if (!aac_config_ || !aac_config_->is_good_)
		return;

	CurrentAudioConfig = aac_config_;

	audio_tag_type_ = AudioTagTypeAACConfig;
	is_good_ = true;
}

std::string AudioTagBodyAACConfig::GetExtraInfo()
{
	if (aac_config_)
		return aac_config_->Serialize();
	return "";
}

static int GetADTSHeader(uint8_t* buffer, uint16_t aac_size, const std::shared_ptr<AudioSpecificConfig>& audio_config)
{
	/*
	http://wiki.multimedia.cx/index.php?title=ADTS
	ADTS
	Audio Data Transport Stream (ADTS) is a format, used by MPEG TS or Shoutcast to stream audio, usually AAC.

	Structure
	AAAAAAAA AAAABCCD EEFFFFGH HHIJKLMM MMMMMMMM MMMOOOOO OOOOOOPP (QQQQQQQQ QQQQQQQQ)

	Header consists of 7 or 9 bytes (without or with CRC).

	Letter	Length (bits)	Description
	A	12	syncword 0xFFF, all bits must be 1
	B	1	MPEG Version: 0 for MPEG-4, 1 for MPEG-2
	C	2	Layer: always 0
	D	1	protection absent, Warning, set to 1 if there is no CRC and 0 if there is CRC
	E	2	profile, the MPEG-4 Audio Object Type minus 1
	F	4	MPEG-4 Sampling Frequency Index (15 is forbidden)
	G	1	private bit, guaranteed never to be used by MPEG, set to 0 when encoding, ignore when decoding
	H	3	MPEG-4 Channel Configuration (in the case of 0, the channel configuration is sent via an inband PCE)
	I	1	originality, set to 0 when encoding, ignore when decoding
	J	1	home, set to 0 when encoding, ignore when decoding
	K	1	copyrighted id bit, the next bit of a centrally registered copyright identifier, set to 0 when encoding, ignore when decoding
	L	1	copyright id start, signals that this frame's copyright id bit is the first bit of the copyright id, set to 0 when encoding, ignore when decoding
	M	13	frame length, this value must include 7 or 9 bytes of header length: FrameLength = (ProtectionAbsent == 1 ? 7 : 9) + size(AACFrame)
	O	11	Buffer fullness
	P	2	Number of AAC frames (RDBs) in ADTS frame minus 1, for maximum compatibility always use 1 AAC frame per ADTS frame
	Q	16	CRC if protection absent is 0

	Usage in MPEG-TS
	ADTS packet must be a content of PES packet. Pack AAC data inside ADTS frame, than pack inside PES packet, then mux by TS packetizer.

	Usage in Shoutcast
	ADTS frames goes one by one in TCP stream. Look for syncword, parse header and look for next syncword after.
	*/

	uint8_t profile = audio_config->audioObjectType - 1;
	uint8_t sample_freq_index = audio_config->samplingFrequencyIndex;
	uint8_t channel_cfg = audio_config->channelConfiguration;
	uint16_t adts_frame_len = 7 + aac_size;

	uint8_t pos = 0;
	buffer[pos++] = 0xff; //syncword(high 8 bits, 1111 1111)
	buffer[pos++] = 0xf1; //syncword(low 4 bits, 1111); ID(1 bit, 0); Layer(2 bit, 00); protection absent(1 bit, 0)
	buffer[pos++] = ((profile & 0x03) << 6) | ((sample_freq_index & 0x0F) << 2) | ((channel_cfg & 0x07) >> 2); //profile, Sampling Frequency Index, first bit of channel configuration
	buffer[pos++] = ((channel_cfg & 0x03) << 6) | ((adts_frame_len & 0x1FFF) >> 11); //last 2 bits of channel configuration, first 2 bits of frame length
	buffer[pos++] = (adts_frame_len & 0x07FF) >> 3; //middle 8 bits of frame length
	buffer[pos++] = ((adts_frame_len & 0x0007) << 5) | 0x1F; //last 3 bits of frame length, first 5 bits of buffer fullness
	buffer[pos++] = 0x3F << 2; //last 6 bits of buffer fullness
	
	return pos;
}

AudioTagBodyAACData::AudioTagBodyAACData(ByteReader& data, const std::shared_ptr<DemuxInterface>& demux_output)
{
	if (demux_output)
	{
		uint8_t adts_header[20] = {0};
		int adts_header_len = GetADTSHeader(adts_header, data.RemainingSize(), CurrentAudioConfig);
		demux_output->OnAudioAACData(adts_header, adts_header_len);
		demux_output->OnAudioAACData(data.CurrentPos(), data.RemainingSize());
	}

	audio_tag_type_ = AudioTagTypeAACData;
	is_good_ = true;
}

AudioTagBodyNonAAC::AudioTagBodyNonAAC(ByteReader& data)
{
	audio_tag_type_ = AudioTagTypeNonAAC;
	is_good_ = true;
}

FlvTagDataVideo::FlvTagDataVideo(ByteReader& data, const std::shared_ptr<DemuxInterface>& demux_output)
{
	video_tag_header_ = std::make_shared<VideoTagHeader>(data);
	if (!video_tag_header_ || !video_tag_header_->is_good_)
		return;

	video_tag_body_ = VideoTagBody::Create(data, video_tag_header_->codec_id_, demux_output);
	if (!video_tag_body_ || !video_tag_body_->IsGood())
		return;

	is_good_ = true;
}

void FlvTagDataVideo::SetTagSerial(int tag_serial)
{
	if (video_tag_body_)
		video_tag_body_->SetTagSerial(tag_serial);
}

uint32_t FlvTagDataVideo::GetCts()
{
	if (video_tag_body_)
		return video_tag_body_->GetCts();
	return FlvTagData::GetCts();
}

std::string FlvTagDataVideo::GetSubTypeString()
{
	if (video_tag_body_)
		return GetVideoTagTypeString(video_tag_body_->GetVideoTagType(), video_tag_header_->codec_id_);
	return FlvTagData::GetSubTypeString();
}

std::string FlvTagDataVideo::GetFormatString()
{
	if (video_tag_header_)
		return GetFlvVideoFrameTypeString(video_tag_header_->frame_type_) + " | "
		+ GetFlvVideoCodecIDString(video_tag_header_->codec_id_);
	return "";
}

std::string FlvTagDataVideo::GetExtraInfo()
{
	if (video_tag_body_)
		return video_tag_body_->GetExtraInfo();
	return "";
}

NaluList FlvTagDataVideo::EnumNalus()
{
	if (video_tag_body_)
		return video_tag_body_->EnumNalus();
	return NaluList();
}

std::string GetFlvVideoFrameTypeString(FlvVideoFrameType type)
{
	switch (type)
	{
	case FlvVideoFrameTypeKeyFrame:
		return "Key Frame";
	case FlvVideoFrameTypeInterFrame:
		return "Inter Frame";
	case FlvVideoFrameTypeDisposableInterFrame:
		return "Disposable Inter Frame";
	case FlvVideoFrameTypeGeneratedKeyFrame:
		return "Generated Key Frame";
	case FlvVideoFrameTypeVideoInfo:
		return "Video Info";
	default:
		return STRING_UNKNOWN;
	}
}

std::string GetFlvVideoCodecIDString(FlvVideoCodecID id)
{
	switch (id)
	{
	case FlvVideoCodeIDJPEG:
		return "JPEG";
	case FlvVideoCodeIDSorensonH263:
		return "Sorenson H.263";
	case FlvVideoCodeIDScreenVideo:
		return "Screen Video";
	case FlvVideoCodeIDOn2VP6:
		return "On2 VP6";
	case FlvVideoCodeIDOn2VP6Alpha:
		return "On2 VP6 Alpha";
	case FlvVideoCodeIDScreenVideoV2:
		return "Screen Video V2";
	case FlvVideoCodeIDAVC:
		return "AVC";
	case FlvVideoCodeIDHEVC:
		return "HEVC";
	default:
		return STRING_UNKNOWN;
	}
}

VideoTagHeader::VideoTagHeader(ByteReader& data)
{
	memset(this, 0, sizeof(VideoTagHeader));

	uint8_t b = *data.ReadBytes(1); //high 4 bits
	frame_type_ = (FlvVideoFrameType)(b >> 4);
	switch (frame_type_)
	{
	case FlvVideoFrameTypeKeyFrame:
	case FlvVideoFrameTypeInterFrame:
	case FlvVideoFrameTypeDisposableInterFrame:
	case FlvVideoFrameTypeGeneratedKeyFrame:
	case FlvVideoFrameTypeVideoInfo:
		break;
	default:
		return;
	}

	codec_id_ = (FlvVideoCodecID)(b & 0x0f); //low 4 bits
	switch (codec_id_)
	{
	case FlvVideoCodeIDJPEG:
	case FlvVideoCodeIDSorensonH263:
	case FlvVideoCodeIDScreenVideo:
	case FlvVideoCodeIDOn2VP6:
	case FlvVideoCodeIDOn2VP6Alpha:
	case FlvVideoCodeIDScreenVideoV2:
	case FlvVideoCodeIDAVC:
	case FlvVideoCodeIDHEVC:
		break;
	default:
		return;
	}

	is_good_ = true;
}

std::string GetVideoTagTypeString(VideoTagType type, FlvVideoCodecID codec_id)
{
	if (codec_id != FlvVideoCodeIDAVC && codec_id != FlvVideoCodeIDHEVC)
		return STRING_UNKNOWN;

	bool is264 = codec_id == FlvVideoCodeIDAVC;
	switch (type)
	{
	case VideoTagTypeAVCSequenceHeader:
		return is264 ? "H.264 SPS PPS" : "H.265 VPS SPS PPS";
	case VideoTagTypeAVCNalu:
		return is264 ? "H.264 NAL Units" : "H.265 NAL Units";
	case VideoTagTypeAVCSequenceEnd:
		return is264 ? "H.264 Sequence End" : "H.265 Sequence End";
	default:
		return STRING_UNKNOWN;
	}
}

std::shared_ptr<VideoTagBody> VideoTagBody::Create(ByteReader& data, FlvVideoCodecID codec_id, const std::shared_ptr<DemuxInterface>& demux_output)
{
	switch (codec_id)
	{
	case FlvVideoCodeIDAVC:
	{
		uint8_t b = *data.ReadBytes(1);
		switch (b)
		{
		case VideoTagTypeAVCSequenceHeader:
			return std::make_shared<VideoTagBodySpsPps>(data, demux_output);
		case VideoTagTypeAVCNalu:
			return std::make_shared<VideoTagBodyAVCNalu>(data, demux_output);
		case VideoTagTypeAVCSequenceEnd:
			return std::make_shared<VideoTagBodySequenceEnd>(data);
		default:
			return std::shared_ptr<VideoTagBody>(nullptr);
		}
	}
	case FlvVideoCodeIDHEVC:
	{
		uint8_t b = *data.ReadBytes(1);
		switch (b)
		{
		case VideoTagTypeAVCSequenceHeader:
			return std::make_shared<VideoTagBodyVpsSpsPps>(data, demux_output);
		case VideoTagTypeAVCNalu:
			return std::make_shared<VideoTagBodyHEVCNalu>(data, demux_output);
		case VideoTagTypeAVCSequenceEnd:
			return std::make_shared<VideoTagBodySequenceEnd>(data);
		default:
			return std::shared_ptr<VideoTagBody>(nullptr);
		}
	}
	default:
		return std::make_shared<VideoTagBodyNonAVC>(data);
	}
}

std::string GetNaluTypeString(NaluType type)
{
	switch (type)
	{
	case NaluTypeUnused:
		return "Unused";
	case NaluTypeNonIDR:
		return "Non-IDR Slice";
	case NaluTypeSliceDPA:
		return "Data Partition A";
	case NaluTypeSliceDPB:
		return "Data Partition B";
	case NaluTypeSliceDPC:
		return "Data Partition C";
	case NaluTypeIDR:
		return "IDR Slice";
	case NaluTypeSEI:
		return "SEI";
	case NaluTypeSPS:
		return "SPS";
	case NaluTypePPS:
		return "PPS";
	case NaluTypeAUD:
		return "Access Unit Delimiter";
	case NaluTypeSeqEnd:
		return "End of Sequence";
	case NaluTypeStreamEnd:
		return "End of Stream";
	case NaluTypeFillerData:
		return "Filler Data";
	case NaluTypeSPSExt:
		return "SPS Ext";
	case NaluTypeSliceAux:
		return "Aux Slice";
	default:
		return STRING_UNKNOWN;
	}
}

NaluHeader::NaluHeader(uint8_t b)
{
	nal_unit_type_ = (NaluType)(b & 0x1f);
	nal_ref_idc_ = (b >> 5) & 0x03;
}

std::shared_ptr<NaluBase> NaluBase::CurrentSps = std::shared_ptr<NaluBase>(nullptr);
std::shared_ptr<NaluBase> NaluBase::CurrentPps = std::shared_ptr<NaluBase>(nullptr);
std::shared_ptr<NaluBase> NaluBase::Create(ByteReader& data, uint32_t nalu_size, const std::shared_ptr<DemuxInterface>& demux_output)
{
	if (data.RemainingSize() < nalu_size || nalu_size <= 1) {
		printf("data remaining size %d, nalu size %d\n", data.RemainingSize(), nalu_size);
		return NULL;
	}

	std::shared_ptr<NaluBase> nalu;
	uint8_t nalu_header = *data.ReadBytes(1, false); //just peek 1 byte
	NaluType nalu_type = (NaluType)(nalu_header & 0x1f);
	switch (nalu_type)
	{
	case NaluTypeNonIDR:
	case NaluTypeIDR:
	case NaluTypeSliceAux:
		nalu = std::make_shared<NaluSlice>(data, nalu_size, demux_output);
		break;
	case NaluTypeSEI:
		nalu = std::make_shared<NaluSEI>(data, nalu_size, demux_output);
		break;
	case NaluTypeSPS:
		CurrentSps = std::make_shared<NaluSps>(data, nalu_size, demux_output);
		nalu = CurrentSps;
		break;
	case NaluTypePPS:
		CurrentPps = std::make_shared<NaluPps>(data, nalu_size, demux_output);
		nalu = CurrentPps;
		break;
	default:
		nalu = std::make_shared<NaluBase>(data, nalu_size, demux_output);
		break;
	}

	if (nalu && !nalu->IsGood()) {
		std::string nalu_type_string = GetNaluTypeString(nalu->GetNaluHeader()->nal_unit_type_);
		printf("nalu 0x%p (type: %s, size: %d) is not good\n", nalu.get(), nalu_type_string.c_str(), nalu->NaluSize());
		nalu = NULL;
	}

	return nalu;
}

NaluBase::NaluBase(ByteReader& data, uint32_t nalu_size, const std::shared_ptr<DemuxInterface>& demux_output)
{
	nalu_size_ = nalu_size;
	if (data.RemainingSize() < nalu_size_)
	{
		data.ReadBytes(data.RemainingSize());
		return;
	}

	if (demux_output)
	{
		static const uint8_t start_code[] = { 0x00, 0x00, 0x00, 0x01 };
		demux_output->OnVideoNaluData(start_code, 4);
		demux_output->OnVideoNaluData(data.CurrentPos(), nalu_size_);
	}

	//parse nalu header
	nalu_header_ = std::make_shared<NaluHeader>(*data.ReadBytes(1, false)); //just peek

	//allocate memory and transfer nal to rbsp
	rbsp_size_ = nalu_size_;
	rbsp_ = (uint8_t *)malloc(rbsp_size_);
	int nalu_size_tmp = (int)nalu_size_;
	uint8_t * nalu_buffer = data.ReadBytes(nalu_size_);
	no_bother = true;
	int ret = nal_to_rbsp(nalu_buffer, &nalu_size_tmp, rbsp_, (int *)&rbsp_size_);
	if (ret < 0 || !rbsp_ || !rbsp_size_)
	{
		ReleaseRbsp();
		return;
	}

	is_good_ = true;
}

NaluBase::~NaluBase()
{
	ReleaseRbsp();
}

void NaluBase::ReleaseRbsp()
{
	if (rbsp_)
	{
		free(rbsp_);
		rbsp_ = NULL;
		rbsp_size_ = 0;
	}
}

std::string NaluBase::CompleteInfo()
{
	Json::Value json_nalu;

	json_nalu["nal_unit_size"] = nalu_size_;
	if (nalu_header_)
	{
		json_nalu["forbidden_zero_bit"] = 0;
		json_nalu["nal_ref_idc"] = nalu_header_->nal_ref_idc_;
		json_nalu["nal_unit_type"] = GetNaluTypeString(nalu_header_->nal_unit_type_);
	}

	return json_nalu.toStyledString();
}

int NaluBase::TagSerialBelong()
{
	return tag_serial_belong_;
}

uint32_t NaluBase::NaluSize()
{
	return nalu_size_;
}

uint8_t NaluBase::NalRefIdc()
{
	if (nalu_header_)
		return nalu_header_->nal_ref_idc_;
	return 0;
}

std::string NaluBase::NalUnitType()
{
	if (nalu_header_)
		return GetNaluTypeString(nalu_header_->nal_unit_type_);
	return "";
}

int8_t NaluBase::FirstMbInSlice()
{
	return -1;
}

std::string NaluBase::SliceType()
{
	return "";
}

int NaluBase::PicParameterSetId()
{
	return -1;
}

int NaluBase::FrameNum()
{
	return -1;
}

int NaluBase::FieldPicFlag()
{
	return -1;
}

int NaluBase::PicOrderCntLsb()
{
	return -1;
}

int NaluBase::SliceQpDelta()
{
	return -1;
}

std::string NaluBase::ExtraInfo()
{
	return "";
}

NaluSps::NaluSps(ByteReader& data, uint32_t nalu_len_size, const std::shared_ptr<DemuxInterface>& demux_output)
	: NaluBase(data, nalu_len_size, demux_output)
{
	if (!is_good_) //NaluBase parse error
		return;
	is_good_ = false;
	if (nalu_header_->nal_unit_type_ != NaluTypeSPS)
		return;

	sps_ = std::make_shared<sps_t>();
	memset(sps_.get(), 0, sizeof(sps_t));
	BitReader rbsp_data(rbsp_, rbsp_size_);
	read_seq_parameter_set_rbsp(sps_.get(), rbsp_data);
	ReleaseRbsp();
	is_good_ = true;
}

std::string NaluSps::CompleteInfo()
{
	Json::Value json_nalu;
	Json::Reader reader;
	reader.parse(NaluBase::CompleteInfo(), json_nalu);
	if (sps_)
		json_nalu["sps"] = sps_to_json(sps_.get());
	return json_nalu.toStyledString();
}

std::string NaluSps::ExtraInfo()
{
	if (sps_)
		return sps_to_json(sps_.get()).toStyledString();
	return "";
}

NaluPps::NaluPps(ByteReader& data, uint32_t nalu_size, const std::shared_ptr<DemuxInterface>& demux_output)
	: NaluBase(data, nalu_size, demux_output)
{
	if (!is_good_) //NaluBase parse error
		return;
	is_good_ = false;
	if (nalu_header_->nal_unit_type_ != NaluTypePPS)
		return;

	pps_ = std::make_shared<pps_t>();
	memset(pps_.get(), 0, sizeof(pps_t));
	BitReader rbsp_data(rbsp_, rbsp_size_);
	read_pic_parameter_set_rbsp(pps_.get(), rbsp_data);
	ReleaseRbsp();
	is_good_ = true;
}

std::string NaluPps::CompleteInfo()
{
	Json::Value json_nalu;
	Json::Reader reader;
	reader.parse(NaluBase::CompleteInfo(), json_nalu);
	if (pps_)
		json_nalu["pps"] = pps_to_json(pps_.get());
	return json_nalu.toStyledString();
}

std::string NaluPps::ExtraInfo()
{
	if (pps_)
		return pps_to_json(pps_.get()).toStyledString();
	return "";
}

NaluSlice::NaluSlice(ByteReader& data, uint32_t nalu_size, const std::shared_ptr<DemuxInterface>& demux_output)
	: NaluBase(data, nalu_size, demux_output)
{
	if (!is_good_) //NaluBase parse error
		return;
	is_good_ = false;
	if (nalu_header_->nal_unit_type_ != NaluTypeNonIDR 
		&& nalu_header_->nal_unit_type_ != NaluTypeIDR
		&& nalu_header_->nal_unit_type_ != NaluTypeSliceAux)
		return;

	slice_header_ = std::make_shared<slice_header_t>();
	memset(slice_header_.get(), 0, sizeof(slice_header_t));
	BitReader rbsp_data(rbsp_, rbsp_size_);
	NaluSps* current_sps = (NaluSps*)NaluBase::CurrentSps.get();
	NaluPps* current_pps = (NaluPps*)NaluBase::CurrentPps.get();
	if (!current_sps || !current_pps)
		return;
	read_slice_header_rbsp(slice_header_.get(), rbsp_data, nalu_header_->nal_unit_type_, nalu_header_->nal_ref_idc_, current_sps->sps_.get(), current_pps->pps_.get());
	ReleaseRbsp();
	is_good_ = true;
}

std::string NaluSlice::CompleteInfo()
{
	Json::Value json_nalu;
	Json::Reader reader;
	reader.parse(NaluBase::CompleteInfo(), json_nalu);
	if (slice_header_)
		json_nalu["slice_header"] = slice_header_to_json(slice_header_.get(), nalu_header_->nal_unit_type_, nalu_header_->nal_ref_idc_);
	return json_nalu.toStyledString();
}

int8_t NaluSlice::FirstMbInSlice()
{
	if (slice_header_)
		return slice_header_->first_mb_in_slice;
	return NaluBase::FirstMbInSlice();
}

std::string NaluSlice::SliceType()
{
	if (slice_header_)
		return GetSliceTypeString(slice_header_->slice_type);
	return NaluBase::SliceType();
}

int NaluSlice::PicParameterSetId()
{
	if (slice_header_)
		return slice_header_->pic_parameter_set_id;
	return NaluBase::PicParameterSetId();
}

int NaluSlice::FrameNum()
{
	if (slice_header_)
		return slice_header_->frame_num;
	return NaluBase::FrameNum();
}

int NaluSlice::FieldPicFlag()
{
	if (slice_header_)
		return slice_header_->field_pic_flag;
	return NaluBase::FieldPicFlag();
}

int NaluSlice::PicOrderCntLsb()
{
	if (slice_header_)
		return slice_header_->pic_order_cnt_lsb;
	return NaluBase::PicOrderCntLsb();
}

int NaluSlice::SliceQpDelta()
{
	if (slice_header_)
		return slice_header_->slice_qp_delta;
	return NaluBase::SliceQpDelta();
}

std::string NaluSlice::ExtraInfo()
{
	return "";
}

NaluSEI::NaluSEI(ByteReader& data, uint32_t nalu_size, const std::shared_ptr<DemuxInterface>& demux_output)
	: NaluBase(data, nalu_size, demux_output)
{
	if (!is_good_) //NaluBase parse error
		return;
	is_good_ = false;
	if (nalu_header_->nal_unit_type_ != NaluTypeSEI)
		return;

	BitReader rbsp_data(rbsp_, rbsp_size_);
	seis_ = read_sei_rbsp(&sei_num_, rbsp_data);
	ReleaseRbsp();
	if (!seis_ || !sei_num_)
		return;

	if (print_sei) 
	{
		for (int i = 0; i < sei_num_; i++) {
			if (seis_[i] && seis_[i]->payloadSize > 0 && seis_[i]->payload)
				printf("Sei %d payload type: %d, payload size: %d, content: %s\n", i, seis_[i]->payloadType, seis_[i]->payloadSize, seis_[i]->payload);
			else if (!seis_[i])
				printf("Sei %d is NULL\n", i);
			else 
				printf("Sei %d payload type: %d, payload size: %d, payload: %p\n", i, seis_[i]->payloadType, seis_[i]->payloadSize, seis_[i]->payload);
		}
	}

	is_good_ = true;
}

NaluSEI::~NaluSEI()
{
	if (seis_ || sei_num_)
	{
		release_seis(seis_, sei_num_);
		seis_ = NULL;
		sei_num_ = 0;
	}
}

std::string NaluSEI::CompleteInfo()
{
	return ExtraInfo();
}

std::string NaluSEI::ExtraInfo()
{
	Json::Value extra_info;
	extra_info["seis"] = seis_to_json(seis_, sei_num_);
	return extra_info.toStyledString();
}

VideoTagBodyAVCNalu::VideoTagBodyAVCNalu(ByteReader& data, const std::shared_ptr<DemuxInterface>& demux_output)
{
	if (data.RemainingSize() < 3)
		return;
	cts_ = (uint32_t)BytesToInt(data.ReadBytes(3), 3);

	while (data.RemainingSize() > 4)
	{
		uint32_t nalu_size = (uint32_t)BytesToInt(data.ReadBytes(4), 4);
		std::shared_ptr<NaluBase> nalu = NaluBase::Create(data, nalu_size, demux_output);
		if (!nalu || !nalu->IsNoBother())
			break;
		else if (!nalu->IsGood())
			continue;
		nalu_list_.push_back(nalu);
	}
	if (nalu_list_.empty())
		return;

	video_tag_type_ = VideoTagTypeAVCNalu;
	is_good_ = true;
}

void VideoTagBodyAVCNalu::SetTagSerial(int tag_serial)
{
	for (const auto& item : nalu_list_)
		item->SetTagSerialBelong(tag_serial);
}

NaluList VideoTagBodyAVCNalu::EnumNalus()
{
	NaluList list(nalu_list_.begin(), nalu_list_.end());
	return list;
}

std::string VideoTagBodyAVCNalu::GetExtraInfo()
{
	Json::Value root, nalus, nalu;
	Json::Reader reader;

	for (const auto& item : nalu_list_)
	{
		nalu.clear();
		if (reader.parse(item->CompleteInfo(), nalu))
			nalus.append(nalu);
	}

	root["nalu_list"] = nalus;
	return root.toStyledString();
}

AVCDecoderConfigurationRecord::AVCDecoderConfigurationRecord(ByteReader& data, const std::shared_ptr<DemuxInterface>& demux_output)
{
	memset(this, 0, sizeof(AVCDecoderConfigurationRecord));
	if (data.RemainingSize() < 8)
		return;

	configurationVersion = *data.ReadBytes(1);
	AVCProfileIndication = *data.ReadBytes(1);
	profile_compatibility = *data.ReadBytes(1);
	AVCLevelIndication = *data.ReadBytes(1);
	lengthSizeMinusOne = *data.ReadBytes(1);

	//SPS
	numOfSequenceParameterSets = *data.ReadBytes(1);
	if ((numOfSequenceParameterSets & 0x1F) != 1) //Only 1 sps, says Adobe
		return;
	uint32_t nalu_size = (uint32_t)BytesToInt(data.ReadBytes(2), 2);
	sps_nal_ = NaluBase::Create(data, nalu_size, demux_output);
	if (!sps_nal_ || !sps_nal_->IsGood())
		return;

	//PPS
	if (data.RemainingSize() < 1)
		return;
	numOfPictureParameterSets = *data.ReadBytes(1);
	if (numOfPictureParameterSets != 1) //Only 1 pps, says Adobe
		return;
	nalu_size = (uint32_t)BytesToInt(data.ReadBytes(2), 2);
	pps_nal_ = NaluBase::Create(data, nalu_size, demux_output);
	if (!pps_nal_ || !pps_nal_->IsGood())
		return;

	is_good_ = true;
}

VideoTagBodySpsPps::VideoTagBodySpsPps(ByteReader& data, const std::shared_ptr<DemuxInterface>& demux_output)
{
	if (data.RemainingSize() < 3)
		return;
	cts_ = (uint32_t)BytesToInt(data.ReadBytes(3), 3);
	avc_config_ = std::make_shared<AVCDecoderConfigurationRecord>(data, demux_output);
	if (!avc_config_ || !avc_config_->is_good_)
		return;

	video_tag_type_ = VideoTagTypeAVCSequenceHeader;
	is_good_ = true;
}

void VideoTagBodySpsPps::SetTagSerial(int tag_serial)
{
	if (avc_config_)
	{
		if (avc_config_->sps_nal_)
			avc_config_->sps_nal_->SetTagSerialBelong(tag_serial);
		if (avc_config_->pps_nal_)
			avc_config_->pps_nal_->SetTagSerialBelong(tag_serial);
	}
}

NaluList VideoTagBodySpsPps::EnumNalus()
{
	NaluList list;
	if (avc_config_)
	{
		if (avc_config_->sps_nal_)
			list.push_back(avc_config_->sps_nal_);
		if (avc_config_->pps_nal_)
			list.push_back(avc_config_->pps_nal_);
	}
	return list;
}

std::string VideoTagBodySpsPps::GetExtraInfo()
{
	Json::Value extra_info, json_sps, json_pps;
	Json::Reader reader;

	extra_info["configurationVersion"] = avc_config_->configurationVersion;
	extra_info["AVCProfileIndication"] = avc_config_->AVCProfileIndication;
	extra_info["profile_compatibility"] = avc_config_->profile_compatibility;
	extra_info["AVCLevelIndication"] = avc_config_->AVCLevelIndication;
	extra_info["lengthSizeMinusOne"] = avc_config_->lengthSizeMinusOne;
	extra_info["numOfSequenceParameterSets"] = avc_config_->numOfSequenceParameterSets & 0x1F;
	if (reader.parse(avc_config_->sps_nal_->CompleteInfo(), json_sps))
		extra_info["nalu_list"].append(json_sps);
	extra_info["numOfPictureParameterSets"] = avc_config_->numOfPictureParameterSets;
	if (reader.parse(avc_config_->pps_nal_->CompleteInfo(), json_pps))
		extra_info["nalu_list"].append(json_pps);
	
	return extra_info.toStyledString();
}

VideoTagBodySequenceEnd::VideoTagBodySequenceEnd(ByteReader& data)
{
	if (data.RemainingSize() < 3)
		return;
	cts_ = (uint32_t)BytesToInt(data.ReadBytes(3), 3);
	video_tag_type_ = VideoTagTypeAVCSequenceEnd;
	is_good_ = true;
}

VideoTagBodyNonAVC::VideoTagBodyNonAVC(ByteReader& data)
{
	video_tag_type_ = VideoTagTypeNonAVC;
	is_good_ = true;
}


//////////////////////////////////////////////////////////////////////////
//HEVC

std::string GetHevcNaluTypeString(HevcNaluType type)
{
	switch (type)
	{
	case HevcNaluTypeCodedSliceTrailN:
		return "CodedSliceTrailN";
	case HevcNaluTypeCodedSliceTrailR:
		return "CodedSliceTrailR";
	case HevcNaluTypeCodedSliceTSAN:
		return "CodedSliceTSAN";
	case HevcNaluTypeCodedSliceTLA:
		return "CodedSliceTLA";
	case HevcNaluTypeCodedSliceSTSAN:
		return "CodedSliceSTSAN";
	case HevcNaluTypeCodedSliceSTSAR:
		return "CodedSliceSTSAR";
	case HevcNaluTypeCodedSliceRADLN:
		return "CodedSliceRADLN";
	case HevcNaluTypeCodedSliceDLP:
		return "CodedSliceDLP";
	case HevcNaluTypeCodedSliceRASLN:
		return "CodedSliceRASLN";
	case HevcNaluTypeCodedSliceTFD:
		return "CodedSliceTFD";
	case HevcNaluTypeReserved10:
		return "Reserved10";
	case HevcNaluTypeReserved11:
		return "Reserved11";
	case HevcNaluTypeReserved12:
		return "Reserved12";
	case HevcNaluTypeReserved13:
		return "Reserved13";
	case HevcNaluTypeReserved14:
		return "Reserved14";
	case HevcNaluTypeReserved15:
		return "Reserved15";
	case HevcNaluTypeCodedSliceBLA:
		return "CodedSliceBLA";
	case HevcNaluTypeCodedSliceBLANT:
		return "CodedSliceBLANT";
	case HevcNaluTypeCodedSliceBLANLP:
		return "CodedSliceBLANLP";
	case HevcNaluTypeCodedSliceIDR:
		return "CodedSliceIDR";
	case HevcNaluTypeCodedSliceIDRNLP:
		return "CodedSliceIDRNLP";
	case HevcNaluTypeCodedSliceCRA:
		return "CodedSliceCRA";
	case HevcNaluTypeReserved22:
		return "Reserved22";
	case HevcNaluTypeReserved23:
		return "Reserved23";
	case HevcNaluTypeReserved24:
		return "Reserved24";
	case HevcNaluTypeReserved25:
		return "Reserved25";
	case HevcNaluTypeReserved26:
		return "Reserved26";
	case HevcNaluTypeReserved27:
		return "Reserved27";
	case HevcNaluTypeReserved28:
		return "Reserved28";
	case HevcNaluTypeReserved29:
		return "Reserved29";
	case HevcNaluTypeReserved30:
		return "Reserved30";
	case HevcNaluTypeReserved31:
		return "Reserved31";
	case HevcNaluTypeVPS:
		return "VPS";
	case HevcNaluTypeSPS:
		return "SPS";
	case HevcNaluTypePPS:
		return "PPS";
	case HevcNaluTypeAccessUnitDelimiter:
		return "AccessUnitDelimiter";
	case HevcNaluTypeEOS:
		return "EOS";
	case HevcNaluTypeEOB:
		return "EOB";
	case HevcNaluTypeFillerData:
		return "FillerData";
	case HevcNaluTypeSEI:
		return "SEIPrefix";
	case HevcNaluTypeSEISuffix:
		return "SEISuffix";
	case HevcNaluTypeReserved41:
		return "Reserved41";
	case HevcNaluTypeReserved42:
		return "Reserved42";
	case HevcNaluTypeReserved43:
		return "Reserved43";
	case HevcNaluTypeReserved44:
		return "Reserved44";
	case HevcNaluTypeReserved45:
		return "Reserved45";
	case HevcNaluTypeReserved46:
		return "Reserved46";
	case HevcNaluTypeReserved47:
		return "Reserved47";
	case HevcNaluTypeUnspecified48:
		return "Unspecified48";
	case HevcNaluTypeUnspecified49:
		return "Unspecified49";
	case HevcNaluTypeUnspecified50:
		return "Unspecified50";
	case HevcNaluTypeUnspecified51:
		return "Unspecified51";
	case HevcNaluTypeUnspecified52:
		return "Unspecified52";
	case HevcNaluTypeUnspecified53:
		return "Unspecified53";
	case HevcNaluTypeUnspecified54:
		return "Unspecified54";
	case HevcNaluTypeUnspecified55:
		return "Unspecified55";
	case HevcNaluTypeUnspecified56:
		return "Unspecified56";
	case HevcNaluTypeUnspecified57:
		return "Unspecified57";
	case HevcNaluTypeUnspecified58:
		return "Unspecified58";
	case HevcNaluTypeUnspecified59:
		return "Unspecified59";
	case HevcNaluTypeUnspecified60:
		return "Unspecified60";
	case HevcNaluTypeUnspecified61:
		return "Unspecified61";
	case HevcNaluTypeUnspecified62:
		return "Unspecified62";
	case HevcNaluTypeUnspecified63:
		return "Unspecified63";
	case HevcNaluTypeInvalid:
		return "Invalid";
	default:
		return STRING_UNKNOWN;
	}
}

HevcNaluHeader::HevcNaluHeader(uint16_t b)
{
	nal_unit_type_ = (HevcNaluType)((b >> 9) & 0x3F);
}

std::shared_ptr<HevcNaluBase> HevcNaluBase::Create(ByteReader& data, uint32_t nalu_size, const std::shared_ptr<DemuxInterface>& demux_output)
{
	if (data.RemainingSize() < nalu_size || nalu_size <= 2) {
		printf("data remaining size %d, nalu size %d\n", data.RemainingSize(), nalu_size);
		return NULL;
	}

	std::shared_ptr<HevcNaluBase> nalu;
	HevcNaluHeader nalu_header((uint16_t)BytesToInt(data.CurrentPos(), 2)); //just peek 2 bytes
	switch (nalu_header.nal_unit_type_)
	{
	//see H.265 Table 7-1 – NAL unit type codes and NAL unit type classes
	case HevcNaluTypeCodedSliceTrailN:
	case HevcNaluTypeCodedSliceTrailR:
	case HevcNaluTypeCodedSliceTSAN:
	case HevcNaluTypeCodedSliceTLA:
	case HevcNaluTypeCodedSliceSTSAN:
	case HevcNaluTypeCodedSliceSTSAR:
	case HevcNaluTypeCodedSliceRADLN:
	case HevcNaluTypeCodedSliceDLP:
	case HevcNaluTypeCodedSliceRASLN:
	case HevcNaluTypeCodedSliceTFD:
	case HevcNaluTypeCodedSliceBLA:
	case HevcNaluTypeCodedSliceBLANT:
	case HevcNaluTypeCodedSliceBLANLP:
	case HevcNaluTypeCodedSliceIDR:
	case HevcNaluTypeCodedSliceIDRNLP:
	case HevcNaluTypeCodedSliceCRA:
		nalu = std::make_shared<HevcNaluSlice>(data, nalu_size, demux_output);
		break;
	case HevcNaluTypeSEI:
	case HevcNaluTypeSEISuffix:
		nalu = std::make_shared<HevcNaluSEI>(data, nalu_size, demux_output);
		break;
	default:
		nalu = std::make_shared<HevcNaluBase>(data, nalu_size, demux_output);
		break;
	}

	if (nalu && !nalu->IsGood()) {
		std::string nalu_type_string = GetHevcNaluTypeString(nalu_header.nal_unit_type_);
		printf("nalu 0x%p (type: %s, size: %d) is not good\n", nalu.get(), nalu_type_string.c_str(), nalu->NaluSize());
		nalu = NULL;
	}

	return nalu;
}

HevcNaluBase::HevcNaluBase(ByteReader& data, uint32_t nalu_size, const std::shared_ptr<DemuxInterface>& demux_output)
{
	nalu_size_ = nalu_size;
	if (data.RemainingSize() < nalu_size_)
	{
		data.ReadBytes(data.RemainingSize());
		return;
	}

	if (demux_output)
	{
		static const uint8_t start_code[] = { 0x00, 0x00, 0x00, 0x01 };
		demux_output->OnVideoNaluData(start_code, 4);
		demux_output->OnVideoNaluData(data.CurrentPos(), nalu_size_);
	}

	//parse nalu header
	nalu_header_ = std::make_shared<HevcNaluHeader>((uint16_t)BytesToInt(data.CurrentPos(), 2));

	//allocate memory and transfer nal to rbsp
	rbsp_size_ = nalu_size_;
	rbsp_ = (uint8_t *)malloc(rbsp_size_);
	int nalu_size_tmp = (int)nalu_size_;
	uint8_t * nalu_buffer = data.ReadBytes(nalu_size_);
	int ret = hevc_nal_to_rbsp(nalu_buffer, &nalu_size_tmp, rbsp_, (int *)&rbsp_size_);
	if (ret < 0 || !rbsp_ || !rbsp_size_)
	{
		ReleaseRbsp();
		return;
	}

	is_good_ = true;
}

HevcNaluType HevcNaluBase::GetHevcNaluType(const ByteReader& data, uint8_t nalu_len_size)
{
	if (data.RemainingSize() < (uint32_t)(nalu_len_size + 2))
		return HevcNaluTypeUnknown;
	HevcNaluHeader nalu_header((uint16_t)BytesToInt(data.CurrentPos() + nalu_len_size, 2));
	return nalu_header.nal_unit_type_;
}

void HevcNaluBase::ReleaseRbsp()
{
	if (rbsp_)
	{
		free(rbsp_);
		rbsp_ = NULL;
		rbsp_size_ = 0;
	}
}

std::string HevcNaluBase::CompleteInfo()
{
	return "";
}

int HevcNaluBase::TagSerialBelong()
{
	return tag_serial_belong_;
}

uint32_t HevcNaluBase::NaluSize()
{
	return nalu_size_;
}

uint8_t HevcNaluBase::NalRefIdc()
{
	return 0;
}

std::string HevcNaluBase::NalUnitType()
{
	if (nalu_header_)
		return GetHevcNaluTypeString(nalu_header_->nal_unit_type_);
	return STRING_UNKNOWN;
}

int8_t HevcNaluBase::FirstMbInSlice()
{
	return 0;
}

std::string HevcNaluBase::SliceType()
{
	return "";
}

int HevcNaluBase::PicParameterSetId()
{
	return 0;
}

int HevcNaluBase::FrameNum()
{
	return 0;
}

int HevcNaluBase::FieldPicFlag()
{
	return 0;
}

int HevcNaluBase::PicOrderCntLsb()
{
	return 0;
}

int HevcNaluBase::SliceQpDelta()
{
	return 0;
}

std::string HevcNaluBase::ExtraInfo()
{
	return "";
}

HevcNaluSEI::HevcNaluSEI(ByteReader& data, uint32_t nalu_size, const std::shared_ptr<DemuxInterface>& demux_output)
	: HevcNaluBase(data, nalu_size, demux_output)
{
	if (!is_good_) //NaluBase parse error
		return;
	is_good_ = false;
	if (nalu_header_->nal_unit_type_ != HevcNaluTypeSEI && nalu_header_->nal_unit_type_ != HevcNaluTypeSEISuffix)
		return;

	BitReader rbsp_data(rbsp_, rbsp_size_);
	seis_ = read_hevc_sei_rbsp(&sei_num_, rbsp_data, (int)nalu_header_->nal_unit_type_);
	ReleaseRbsp();
	if (!seis_ || !sei_num_)
		return;

	if (print_sei) 
	{
		for (int i = 0; i < sei_num_; i++) {
			if (seis_[i] && seis_[i]->payloadSize > 0 && seis_[i]->payload)
				printf("Sei %d payload type: %d, payload size: %d, content: %s\n", i, seis_[i]->payloadType, seis_[i]->payloadSize, seis_[i]->payload);
			else if (!seis_[i])
				printf("Sei %d is NULL\n", i);
			else 
				printf("Sei %d payload type: %d, payload size: %d, payload: %p\n", i, seis_[i]->payloadType, seis_[i]->payloadSize, seis_[i]->payload);
		}
	}

	is_good_ = true;
}

HevcNaluSEI::~HevcNaluSEI()
{
	if (seis_ || sei_num_)
	{
		release_hevc_seis(seis_, sei_num_);
		seis_ = NULL;
		sei_num_ = 0;
	}
}

std::string HevcNaluSEI::CompleteInfo()
{
	return ExtraInfo();
}

std::string HevcNaluSEI::ExtraInfo()
{
	Json::Value extra_info;
	extra_info["seis"] = hevc_seis_to_json(seis_, sei_num_);
	return extra_info.toStyledString();
}

HevcNaluSlice::HevcNaluSlice(ByteReader& data, uint32_t nalu_size, const std::shared_ptr<DemuxInterface>& demux_output)
	: HevcNaluBase(data, nalu_size, demux_output) 
{
	if (!is_good_) //NaluBase parse error
		return;
	is_good_ = false;

	slice_header_ = std::make_shared<hevc_slice_header_t>();
	memset(slice_header_.get(), 0, sizeof(hevc_slice_header_t));
	BitReader rbsp_data(rbsp_, rbsp_size_);
	hevc_slice_segment_header(slice_header_.get(), rbsp_data, nalu_header_->nal_unit_type_);
	if (slice_header_->first_slice_segment_in_pic_flag == 0)
	{
		printf("Warning: multi-slice!\n");
	}
	ReleaseRbsp();
	is_good_ = true;
}

std::string HevcNaluSlice::CompleteInfo()
{
	Json::Value json_nalu;
	Json::Reader reader;
	reader.parse(HevcNaluBase::CompleteInfo(), json_nalu);
	if (slice_header_)
		json_nalu["slice_header"] = hevc_slice_segment_header_to_json(slice_header_.get());
	return json_nalu.toStyledString();
}

VideoTagBodyHEVCNalu::VideoTagBodyHEVCNalu(ByteReader& data, const std::shared_ptr<DemuxInterface>& demux_output)
{
	if (data.RemainingSize() < 3)
		return;
	cts_ = (uint32_t)BytesToInt(data.ReadBytes(3), 3);

	while (data.RemainingSize() > 4)
	{
		uint32_t nalu_size = (uint32_t)BytesToInt(data.ReadBytes(4), 4);
		std::shared_ptr<HevcNaluBase> nalu = HevcNaluBase::Create(data, nalu_size, demux_output);
		if (!nalu)
			break;
		else if (!nalu->IsGood())
			continue;
		nalu_list_.push_back(nalu);
	}
	if (nalu_list_.empty())
		return;

	video_tag_type_ = VideoTagTypeAVCNalu;
	is_good_ = true;
}

void VideoTagBodyHEVCNalu::SetTagSerial(int tag_serial)
{
	for (const auto& item : nalu_list_)
		item->SetTagSerialBelong(tag_serial);
}

NaluList VideoTagBodyHEVCNalu::EnumNalus()
{
	NaluList list(nalu_list_.begin(), nalu_list_.end());
	return list;
}

std::string VideoTagBodyHEVCNalu::GetExtraInfo()
{
	return "";
}

HEVCDecoderConfigurationRecord::HEVCDecoderConfigurationRecord(ByteReader& data, const std::shared_ptr<DemuxInterface>& demux_output)
{
	if (data.RemainingSize() < 23)
		return;
	data.ReadBytes(22); //The first 22 bytes of HEVCDecoderConfigurationRecord are various flags and info

	int array_count = *data.ReadBytes(1); //The 23rd byte (index 22) is the number of nalu arrays
	for (int i = 0; i < array_count; i++)
	{
		data.ReadBytes(1);
		int nalu_count = (int)BytesToInt(data.ReadBytes(2), 2); //The number of nalus in single array
		for (int j = 0; j < nalu_count; j++)
		{
			uint32_t nalu_size = (uint32_t)BytesToInt(data.ReadBytes(2), 2);
			std::shared_ptr<HevcNaluBase> nalu = HevcNaluBase::Create(data, nalu_size, demux_output);
			if (nalu)
				nalu_list_.push_back(nalu);
		}
	}
	if (nalu_list_.empty())
		return;

	is_good_ = true;
}

VideoTagBodyVpsSpsPps::VideoTagBodyVpsSpsPps(ByteReader& data, const std::shared_ptr<DemuxInterface>& demux_output)
{
	if (data.RemainingSize() < 3)
		return;
	cts_ = (uint32_t)BytesToInt(data.ReadBytes(3), 3);
	hevc_config_ = std::make_shared<HEVCDecoderConfigurationRecord>(data, demux_output);
	if (!hevc_config_ || !hevc_config_->is_good_)
		return;

	video_tag_type_ = VideoTagTypeAVCSequenceHeader;
	is_good_ = true;
}

void VideoTagBodyVpsSpsPps::SetTagSerial(int tag_serial)
{
	if (hevc_config_)
	{
		for (const auto& item : hevc_config_->nalu_list_)
			item->SetTagSerialBelong(tag_serial);
	}
}

NaluList VideoTagBodyVpsSpsPps::EnumNalus()
{
	if (hevc_config_)
		return NaluList(hevc_config_->nalu_list_.begin(), hevc_config_->nalu_list_.end());
	return NaluList();
}
