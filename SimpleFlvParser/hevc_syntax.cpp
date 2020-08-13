#include "hevc_syntax.h"
#include "h264_syntax.h"
#include "utils.h"

int hevc_nal_to_rbsp(const uint8_t* nal_buf, int* nal_size, uint8_t* rbsp_buf, int* rbsp_size)
{
	int i, k;
	int j = 0;
	int count = 0;
	bool trailing_zero = false;

	//i starts with 2, because the first 2 bytes are nal unit header
	for (i = 2; i < *nal_size; i++)
	{
		for (k = i; k < *nal_size; k++)
		{
			if (nal_buf[k] != 0x00)
				break;
		}
		if (k >= *nal_size)
		{
			trailing_zero = true;
			break;
		}

		// in NAL unit, 0x000000, 0x000001 or 0x000002 shall not occur at any byte-aligned position
		if ((count == 2) && (nal_buf[i] < 0x03))
		{
			return -1;
		}

		if ((count == 2) && (nal_buf[i] == 0x03))
		{
			// check the 4th byte after 0x000003, except when cabac_zero_word is used, in which case the last three bytes of this NAL unit must be 0x000003
			if ((i < *nal_size - 1) && (nal_buf[i + 1] > 0x03))
			{
				return -2;
			}

			// if cabac_zero_word is used, the final byte of this NAL unit(0x03) is discarded, and the last two bytes of RBSP must be 0x0000
			if (i == *nal_size - 1)
			{
				break;
			}

			i++;
			count = 0;
		}

		if (j >= *rbsp_size)
		{
			// error, not enough space
			return -3;
		}

		rbsp_buf[j] = nal_buf[i];
		if (nal_buf[i] == 0x00)
		{
			count++;
		}
		else
		{
			count = 0;
		}
		j++;
	}

	if (trailing_zero && i < *nal_size)
	{
		memcpy(rbsp_buf + j, nal_buf + i, *nal_size - i);
		j += *nal_size - i;
		i = *nal_size;
	}

	*nal_size = i;
	*rbsp_size = j;
	return j;
}

hevc_sei_t* hevc_sei_new()
{
	hevc_sei_t* s = (hevc_sei_t*)malloc(sizeof(hevc_sei_t));
	memset(s, 0, sizeof(hevc_sei_t));
	s->payload = NULL;
	return s;
}

void hevc_sei_free(hevc_sei_t* s)
{
	if (s->payload != NULL)
		free(s->payload);
	free(s);
}

int hevc_payload_extension_present(BitReader& b, uint8_t* payloadEnd)
{
	int bytesLeft = payloadEnd - b.BytePos();
	if (bytesLeft > 1) //rbsp trailing bits have at most 8 bits
		return 1;
	else if (bytesLeft < 1) // reached payload end
		return 0;

	BitReader leftPayload(b.BytePos(), bytesLeft, b.BitsLeft());
	if (!leftPayload.ReadU1()) //current bit is not bit_equal_to_one
		return 1;
	while (!leftPayload.IsByteAligned())
	{
		if (leftPayload.ReadU1()) //current bit is not bit_equal_to_zero
			return 1;
	}

	return 0;
}

//see D.2.1 General SEI message syntax in ITU-T H.265
void read_hevc_sei_payload(hevc_sei_t* s, BitReader& b, int payloadType, int payloadSize, int nal_unit_type)
{
	uint8_t* start = b.BytePos();

	if (nal_unit_type == HevcNaluTypeSEI || nal_unit_type == HevcNaluTypeSEISuffix) 
	{
		if (payloadType == 5) //we only want "User data unregistered SEI"
		{
			b.SkipU(128); //uuid_iso_iec_11578
			s->payload = (uint8_t*)malloc(payloadSize - 16 + 1);
			for (int i = 0; i < payloadSize - 16; i++)
				s->payload[i] = b.ReadU8();
			s->payload[payloadSize - 16] = 0; //'\0'
			s->payloadSize = payloadSize - 16;
		}
		else if (payloadType == 100)
		{
			s->payload = (uint8_t*)malloc(payloadSize + 1);
			for (int i = 0; i < payloadSize; i++)
				s->payload[i] = b.ReadU8();
			s->payload[payloadSize] = 0; //'\0'
			s->payloadSize = payloadSize;
		}
		else
		{
			printf("Warning: Skipped SEI payload type %d, payload size %d.\n", payloadType, payloadSize);
			b.SkipBytes(payloadSize);
		}
	}

	/* //ffmpeg都没管这一段
	uint8_t* end = b.BytePos();
	if (!b.IsByteAligned() || start + payloadSize > end) //There is more data in payload
	{
		if (hevc_payload_extension_present(b, start + payloadSize))
		{
			printf("caution: reserved_payload_extension_data size may be error.\n");
			//see D.3.1 General SEI payload semantics
			//reserved_payload_extension_data = 8 * payloadSize − nEarlierBits − nPayloadZeroBits − 1
			int nEarlierBits = (end - start + 1) * 8 - b.BitsLeft();
			int reserved_payload_extension_data = (8 * payloadSize - nEarlierBits) / 2;
			b.SkipU(reserved_payload_extension_data); //u(v)
		}
		read_sei_end_bits(b);
	}*/
}

void read_hevc_sei_message(hevc_sei_t* sei, BitReader& b, int nal_unit_type)
{
	sei->payloadType = _read_ff_coded_number(b);
	sei->payloadSize = _read_ff_coded_number(b);
	read_hevc_sei_payload(sei, b, sei->payloadType, sei->payloadSize, nal_unit_type);
}

hevc_sei_t** read_hevc_sei_rbsp(uint32_t *num_seis, BitReader& b, int nal_unit_type)
{
	if (!num_seis || (nal_unit_type != HevcNaluTypeSEI && nal_unit_type != HevcNaluTypeSEISuffix))
		return NULL;

	hevc_sei_t **seis = NULL;
	*num_seis = 0;
	do {
		hevc_sei_t* sei = hevc_sei_new();
		read_hevc_sei_message(sei, b, nal_unit_type);
		if (!sei->payload || sei->payloadSize <= 0) {
			hevc_sei_free(sei);
			continue;
		}
		(*num_seis)++;
		seis = (hevc_sei_t**)realloc(seis, (*num_seis) * sizeof(hevc_sei_t*));
		seis[*num_seis - 1] = sei;
	} while (more_rbsp_data(b));
	read_rbsp_trailing_bits(b);

	return seis;
}

void release_hevc_seis(hevc_sei_t** seis, uint32_t num_seis)
{
	for (uint32_t i = 0; i < num_seis; i++)
	{
		if (seis[i])
			hevc_sei_free(seis[i]);
	}
	free(seis);
}

Json::Value hevc_seis_to_json(hevc_sei_t** seis, uint32_t num_seis)
{
	Json::Value json_seis;

	if (!seis || !num_seis)
		return json_seis;

	for (uint32_t i = 0; i < num_seis; i++)
	{
		Json::Value json_sei;
		hevc_sei_t *sei = seis[i];
		if (!sei)
			continue;

		json_sei["payloadType"] = sei->payloadType;
		if ((sei->payloadType == 5 || sei->payloadType == 100) && sei->payload && sei->payloadSize)
		{
			std::string sei_content = std::string((char *)sei->payload, sei->payloadSize);
			json_sei["payload"] = sei_content;
			json_sei["payloadSize"] = sei->payloadSize;
		}
		json_seis.append(json_sei);
	}

	return json_seis;
}
