#include "flv_file_internal.h"
#include "utils.h"

#define FLV_HEADER_SIZE           9
#define PREVIOUS_TAG_SIZE_SIZE    4
#define FLV_TAG_HEADER_SIZE       11
#define FLV_VIDEO_TAG_HEADER_SIZE 5
#define FLV_AUDIO_TAG_HEADER_SIZE 1

FlvHeader::FlvHeader(ByteReader& data)
{
	memset(this, 0, sizeof(FlvHeader));

	if (data.RemainingSize() < FLV_HEADER_SIZE)
		return;

	uint8_t* header_pos = data.ReadBytes(3);
	if (strncmp((char *)header_pos, "FLV", 3)) //not begin with "FLV"
		return;
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
		return;
	}

	is_good_ = true;
}

FlvTag::FlvTag(ByteReader& data, int tag_serial)
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
	tag_data_ = FlvTagData::Create(data, tag_header_->tag_data_size_, tag_header_->tag_type_);
	if (!tag_data_ || !tag_data_->IsGood())
		return;

	is_good_ = true;
}

std::shared_ptr<FlvTagData> FlvTagData::Create(ByteReader& data, uint32_t tag_data_size, FlvTagType tag_type)
{
	if (data.RemainingSize() < tag_data_size)
		return std::shared_ptr<FlvTagData>(nullptr);

	ByteReader tag_data(data.ReadBytes(tag_data_size), tag_data_size);

	switch (tag_type)
	{
	case FlvTagTypeAudio:
		return std::make_shared<FlvTagDataAudio>(tag_data);
	case FlvTagTypeVideo:
		return std::make_shared<FlvTagDataVideo>(tag_data);
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

	printf("%s\n", script_data_.toStyledString().c_str());

	is_good_ = true;
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

FlvTagDataAudio::FlvTagDataAudio(ByteReader& data)
{
	audio_tag_header_ = std::make_shared<AudioTagHeader>(data);
	if (!audio_tag_header_ || !audio_tag_header_->is_good_)
		return;

	audio_tag_body_ = AudioTagBody::Create(data, audio_tag_header_->audio_format_);
	if (!audio_tag_body_ || !audio_tag_body_->IsGood())
		return;

	is_good_ = true;
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

std::shared_ptr<AudioTagBody> AudioTagBody::Create(ByteReader& data, AudioFormat audio_format)
{
	switch (audio_format)
	{
	case AudioFormatAAC:
	{
		uint8_t b = *data.ReadBytes(1);
		if (b == AudioTagTypeAACConfig)
			return std::make_shared<AudioTagBodyAACConfig>(data);
		else if (b == AudioTagTypeAACData)
			return std::make_shared<AudioTagBodyAACData>(data);
		else
			return std::shared_ptr<AudioTagBody>(nullptr);
	}
	default:
		return std::make_shared<AudioTagBodyNonAAC>(data);
	}
}

AudioSpecificConfig::AudioSpecificConfig(ByteReader& data)
{
	memset(this, 0, sizeof(AudioSpecificConfig));
	if (data.RemainingSize() < 2)
		return;

	uint16_t u16 = (*data.ReadBytes(1) << 8) | (*data.ReadBytes(1));
	audioObjectType = (uint8_t)(u16 >> 11); //high 5 bits
	samplingFrequencyIndex = (uint8_t)((u16 >> 7) & 0x000f); //next 4 bits
	channelConfiguration = (uint8_t)((u16 >> 3) & 0x000f); //next 4 bits
	frameLengthFlag = (uint8_t)((u16 >> 2) & 0x0001); //next 1 bit
	dependsOnCoreCoder = (uint8_t)((u16 >> 1) & 0x0001); //next 1 bit
	extensionFlag = (uint8_t)(u16 & 0x0001); //last bit

	is_good_ = true;
}

AudioTagBodyAACConfig::AudioTagBodyAACConfig(ByteReader& data)
{
	aac_config_ = std::make_shared<AudioSpecificConfig>(data);
	if (!aac_config_ || !aac_config_->is_good_)
		return;

	audio_tag_type_ = AudioTagTypeAACConfig;
	is_good_ = true;
}

AudioTagBodyAACData::AudioTagBodyAACData(ByteReader& data)
{
	audio_tag_type_ = AudioTagTypeAACData;
	is_good_ = true;
}

AudioTagBodyNonAAC::AudioTagBodyNonAAC(ByteReader& data)
{
	audio_tag_type_ = AudioTagTypeNonAAC;
	is_good_ = true;
}

FlvTagDataVideo::FlvTagDataVideo(ByteReader& data)
{
	video_tag_header_ = std::make_shared<VideoTagHeader>(data);
	if (!video_tag_header_ || !video_tag_header_->is_good_)
		return;

	video_tag_body_ = VideoTagBody::Create(data, video_tag_header_->codec_id_);
	if (!video_tag_body_ || !video_tag_body_->IsGood())
		return;

	is_good_ = true;
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

	codec_id_ = (FlvVideoCodeID)(b & 0x0f); //low 4 bits
	switch (codec_id_)
	{
	case FlvVideoCodeIDJPEG:
	case FlvVideoCodeIDSorensonH263:
	case FlvVideoCodeIDScreenVideo:
	case FlvVideoCodeIDOn2VP6:
	case FlvVideoCodeIDOn2VP6Alpha:
	case FlvVideoCodeIDScreenVideoV2:
	case FlvVideoCodeIDAVC:
		break;
	default:
		return;
	}

	is_good_ = true;
}

std::shared_ptr<VideoTagBody> VideoTagBody::Create(ByteReader& data, FlvVideoCodeID codec_id)
{
	switch (codec_id)
	{
	case FlvVideoCodeIDAVC:
	{
		uint8_t b = *data.ReadBytes(1);
		switch (b)
		{
		case VideoTagTypeAVCSequenceHeader:
			return std::make_shared<VideoTagBodySPSPPS>(data);
		case VideoTagTypeAVCNALU:
			return std::make_shared<VideoTagBodyAVCNALU>(data);
		case VideoTagTypeAVCSequenceEnd:
			return std::make_shared<VideoTagBodySequenceEnd>(data);
		default:
			return std::shared_ptr<VideoTagBody>(nullptr);
		}
		break;
	}
	default:
		return std::make_shared<VideoTagBodyNonAVC>(data);
	}
}

NALUHeader::NALUHeader(uint8_t b)
{
	nal_unit_type_ = (NALUType)(b & 0x1f);
	nal_ref_idc_ = (b >> 5) & 0x03;
}

std::shared_ptr<NALUBase> NALUBase::CurrentSPS = std::shared_ptr<NALUBase>(nullptr);
std::shared_ptr<NALUBase> NALUBase::CurrentPPS = std::shared_ptr<NALUBase>(nullptr);
std::shared_ptr<NALUBase> NALUBase::Create(ByteReader& data, uint8_t nalu_len_size)
{
	NALUType nalu_type = GetNALUType(data, nalu_len_size);
	switch (nalu_type)
	{
	case NALUTypeNonIDR:
	case NALUTypeIDR:
	case NALUTypeSliceAux:
		return std::make_shared<NALUSlice>(data, nalu_len_size);
	case NALUTypeSEI:
		return std::make_shared<NALUSEI>(data, nalu_len_size);
	case NALUTypeSPS:
		CurrentSPS = std::make_shared<NALUSPS>(data, nalu_len_size);
		return CurrentSPS;
	case NALUTypePPS:
		CurrentPPS = std::make_shared<NALUPPS>(data, nalu_len_size);
		return CurrentPPS;
	default:
		return std::make_shared<NALUBase>(data, nalu_len_size);
	}
}

NALUBase::NALUBase(ByteReader& data, uint8_t nalu_len_size)
{
	//nalu_len_size indicates how many bytes at the ByteReader's start is the nalu length
	// In sps and pps, it's 2. In IDR, p, b frames and SEI nalu, it's 4.
	if (nalu_len_size > 4) 
		return;
	if (data.RemainingSize() < nalu_len_size)
		return;
	nalu_size_ = (uint32_t)BytesToInt(data.ReadBytes(nalu_len_size), nalu_len_size);
	if (data.RemainingSize() < nalu_size_)
	{
		data.ReadBytes(data.RemainingSize());
		return;
	}

	//parse NALU header
	nalu_header_ = std::make_shared<NALUHeader>(*data.ReadBytes(1, false)); //just peek

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

NALUBase::~NALUBase()
{
	ReleaseRbsp();
}

NALUType NALUBase::GetNALUType(const ByteReader& data, uint8_t nalu_len_size)
{
	if (nalu_len_size > 4)
		return NALUTypeUnknown;
	//The first nalu_len_size bytes of nalu_start are nalu length, The nalu header is after the nalu length
	if (data.RemainingSize() < (uint32_t)(nalu_len_size + 1))
		return NALUTypeUnknown;
	
	uint8_t nalu_header = *(data.CurrentPos() + nalu_len_size);
	return (NALUType)(nalu_header & 0x1f);
}

void NALUBase::ReleaseRbsp()
{
	if (rbsp_)
	{
		free(rbsp_);
		rbsp_ = NULL;
		rbsp_size_ = 0;
	}
}

NALUSPS::NALUSPS(ByteReader& data, uint8_t nalu_len_size)
	: NALUBase(data, nalu_len_size)
{
	if (!is_good_) //NALUBase parse error
		return;
	is_good_ = false;
	if (nalu_header_->nal_unit_type_ != NALUTypeSPS)
		return;

	sps_ = std::make_shared<sps_t>();
	memset(sps_.get(), 0, sizeof(sps_t));
	BitReader rbsp_data(rbsp_, rbsp_size_);
	read_seq_parameter_set_rbsp(sps_.get(), rbsp_data);
	ReleaseRbsp();
	is_good_ = true;
}

NALUPPS::NALUPPS(ByteReader& data, uint8_t nalu_len_size)
	: NALUBase(data, nalu_len_size)
{
	if (!is_good_) //NALUBase parse error
		return;
	is_good_ = false;
	if (nalu_header_->nal_unit_type_ != NALUTypePPS)
		return;

	pps_ = std::make_shared<pps_t>();
	memset(pps_.get(), 0, sizeof(pps_t));
	BitReader rbsp_data(rbsp_, rbsp_size_);
	read_pic_parameter_set_rbsp(pps_.get(), rbsp_data);
	ReleaseRbsp();
	is_good_ = true;
}

NALUSlice::NALUSlice(ByteReader& data, uint8_t nalu_len_size)
	: NALUBase(data, nalu_len_size)
{
	if (!is_good_) //NALUBase parse error
		return;
	is_good_ = false;
	if (nalu_header_->nal_unit_type_ != NALUTypeNonIDR 
		&& nalu_header_->nal_unit_type_ != NALUTypeIDR
		&& nalu_header_->nal_unit_type_ != NALUTypeSliceAux)
		return;

	slice_header_ = std::make_shared<slice_header_t>();
	memset(slice_header_.get(), 0, sizeof(slice_header_t));
	BitReader rbsp_data(rbsp_, rbsp_size_);
	NALUSPS* current_sps = (NALUSPS*)NALUBase::CurrentSPS.get();
	NALUPPS* current_pps = (NALUPPS*)NALUBase::CurrentPPS.get();
	if (!current_sps || !current_pps)
		return;
	read_slice_header_rbsp(slice_header_.get(), rbsp_data, nalu_header_->nal_unit_type_, nalu_header_->nal_ref_idc_, current_sps->sps_.get(), current_pps->pps_.get());
	ReleaseRbsp();
	is_good_ = true;
}

NALUSEI::NALUSEI(ByteReader& data, uint8_t nalu_len_size)
	: NALUBase(data, nalu_len_size)
{
	if (!is_good_) //NALUBase parse error
		return;
	is_good_ = false;
	if (nalu_header_->nal_unit_type_ != NALUTypeSEI)
		return;

	BitReader rbsp_data(rbsp_, rbsp_size_);
	seis_ = read_sei_rbsp(&sei_num_, rbsp_data);
	ReleaseRbsp();
	if (!seis_ || !sei_num_)
		return;
	is_good_ = true;
}

NALUSEI::~NALUSEI()
{
	if (seis_ || sei_num_)
	{
		release_seis(seis_, sei_num_);
		seis_ = NULL;
		sei_num_ = 0;
	}
}

VideoTagBodyAVCNALU::VideoTagBodyAVCNALU(ByteReader& data)
{
	if (data.RemainingSize() < 3)
		return;
	cts_ = (uint32_t)BytesToInt(data.ReadBytes(3), 3);

	while (data.RemainingSize())
	{
		std::shared_ptr<NALUBase> nalu = NALUBase::Create(data, 4);
		if (!nalu || !nalu->IsNoBother())
			break;
		else if (!nalu->IsGood())
			continue;
		nalu_list_.push_back(nalu);
	}
	if (nalu_list_.empty())
		return;

	video_tag_type_ = VideoTagTypeAVCNALU;
	is_good_ = true;
}

AVCDecoderConfigurationRecord::AVCDecoderConfigurationRecord(ByteReader& data)
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
	sps_nal_ = NALUBase::Create(data, 2);
	if (!sps_nal_ || !sps_nal_->IsGood())
		return;

	//PPS
	if (data.RemainingSize() < 1)
		return;
	numOfPictureParameterSets = *data.ReadBytes(1);
	if (numOfPictureParameterSets != 1) //Only 1 pps, says Adobe
		return;
	pps_nal_ = NALUBase::Create(data, 2);
	if (!pps_nal_ || !pps_nal_->IsGood())
		return;

	is_good_ = true;
}

VideoTagBodySPSPPS::VideoTagBodySPSPPS(ByteReader& data)
{
	if (data.RemainingSize() < 3)
		return;
	cts_ = (uint32_t)BytesToInt(data.ReadBytes(3), 3);
	avc_config_ = std::make_shared<AVCDecoderConfigurationRecord>(data);
	if (!avc_config_ || !avc_config_->is_good_)
		return;

	video_tag_type_ = VideoTagTypeAVCSequenceHeader;
	is_good_ = true;
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
