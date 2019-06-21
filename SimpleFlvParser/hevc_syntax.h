#ifndef SFP_HEVC_SYNTAX_H
#define SFP_HEVC_SYNTAX_H

#include "bytes.h"
#include <string>
#include "json/value.h"

enum HevcNaluType
{
	HevcNaluTypeUnknown = -1,
	HevcNaluTypeCodedSliceTrailN = 0,	// 0
	HevcNaluTypeCodedSliceTrailR, 		// 1
	HevcNaluTypeCodedSliceTSAN,			// 2
	HevcNaluTypeCodedSliceTLA, 			// 3 // Current name in the spec: TSA_R
	HevcNaluTypeCodedSliceSTSAN, 		// 4
	HevcNaluTypeCodedSliceSTSAR, 		// 5
	HevcNaluTypeCodedSliceRADLN, 		// 6
	HevcNaluTypeCodedSliceDLP, 			// 7 // Current name in the spec: RADL_R
	HevcNaluTypeCodedSliceRASLN, 		// 8
	HevcNaluTypeCodedSliceTFD,			// 9 // Current name in the spec: RASL_R

	HevcNaluTypeReserved10,
	HevcNaluTypeReserved11,
	HevcNaluTypeReserved12,
	HevcNaluTypeReserved13,
	HevcNaluTypeReserved14,
	HevcNaluTypeReserved15,

	HevcNaluTypeCodedSliceBLA, 			// 16 // Current name in the spec: BLA_W_LP
	HevcNaluTypeCodedSliceBLANT, 		// 17 // Current name in the spec: BLA_W_DLP
	HevcNaluTypeCodedSliceBLANLP,  		// 18
	HevcNaluTypeCodedSliceIDR,	  		// 19 // Current name in the spec: IDR_W_DLP
	HevcNaluTypeCodedSliceIDRNLP, 		// 20
	HevcNaluTypeCodedSliceCRA, 			// 21

	HevcNaluTypeReserved22,
	HevcNaluTypeReserved23,
	HevcNaluTypeReserved24,
	HevcNaluTypeReserved25,
	HevcNaluTypeReserved26,
	HevcNaluTypeReserved27,
	HevcNaluTypeReserved28,
	HevcNaluTypeReserved29,
	HevcNaluTypeReserved30,
	HevcNaluTypeReserved31,

	HevcNaluTypeVPS,					// 32
	HevcNaluTypeSPS,					// 33
	HevcNaluTypePPS,					// 34
	HevcNaluTypeAccessUnitDelimiter, 	// 35
	HevcNaluTypeEOS,					// 36
	HevcNaluTypeEOB,					// 37
	HevcNaluTypeFillerData,			    // 38
	HevcNaluTypeSEI,					// 39 Prefix SEI
	HevcNaluTypeSEISuffix,			    // 40 Suffix SEI
	HevcNaluTypeReserved41,
	HevcNaluTypeReserved42,
	HevcNaluTypeReserved43,
	HevcNaluTypeReserved44,
	HevcNaluTypeReserved45,
	HevcNaluTypeReserved46,
	HevcNaluTypeReserved47,
	HevcNaluTypeUnspecified48,
	HevcNaluTypeUnspecified49,
	HevcNaluTypeUnspecified50,
	HevcNaluTypeUnspecified51,
	HevcNaluTypeUnspecified52,
	HevcNaluTypeUnspecified53,
	HevcNaluTypeUnspecified54,
	HevcNaluTypeUnspecified55,
	HevcNaluTypeUnspecified56,
	HevcNaluTypeUnspecified57,
	HevcNaluTypeUnspecified58,
	HevcNaluTypeUnspecified59,
	HevcNaluTypeUnspecified60,
	HevcNaluTypeUnspecified61,
	HevcNaluTypeUnspecified62,
	HevcNaluTypeUnspecified63,
	HevcNaluTypeInvalid,
};

typedef struct
{
	int payloadType;
	int payloadSize;
	uint8_t* payload;
} hevc_sei_t;

class BitReader;

int  hevc_nal_to_rbsp(const uint8_t* nal_buf, int* nal_size, uint8_t* rbsp_buf, int* rbsp_size);

hevc_sei_t** read_hevc_sei_rbsp(uint32_t *num_seis, BitReader& b, int nal_unit_type);

void release_hevc_seis(hevc_sei_t** seis, uint32_t num_seis);

Json::Value hevc_seis_to_json(hevc_sei_t** seis, uint32_t num_seis);

#endif
