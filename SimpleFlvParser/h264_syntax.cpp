#include "h264_syntax.h"
#include "utils.h"

/**
Convert NAL data (Annex B format) to RBSP data.
The size of rbsp_buf must be the same as size of the nal_buf to guarantee the output will fit.
If that is not true, output may be truncated and an error will be returned.
Additionally, certain byte sequences in the input nal_buf are not allowed in the spec and also cause the conversion to fail and an error to be returned.
@param[in] nal_buf   the nal data
@param[in,out] nal_size  as input, pointer to the size of the nal data; as output, filled in with the actual size of the nal data
@param[in,out] rbsp_buf   allocated memory in which to put the rbsp data
@param[in,out] rbsp_size  as input, pointer to the maximum size of the rbsp data; as output, filled in with the actual size of rbsp data
@return  actual size of rbsp data, or -1 on error
*/
// 7.3.1 NAL unit syntax
// 7.4.1.1 Encapsulation of an SODB within an RBSP
int nal_to_rbsp(const uint8_t* nal_buf, int* nal_size, uint8_t* rbsp_buf, int* rbsp_size)
{
	int i, k;
	int j = 0;
	int count = 0;
	bool trailing_zero = false;

	for (i = 1; i < *nal_size; i++)
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
				return -1;
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
			return -1;
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

//Appendix E.1.2 HRD parameters syntax
void read_hrd_parameters(sps_t::hrd_t* hrd, BitReader& b)
{
	int SchedSelIdx;

	hrd->cpb_cnt_minus1 = b.ReadUE();
	hrd->bit_rate_scale = b.ReadU(4);
	hrd->cpb_size_scale = b.ReadU(4);
	for (SchedSelIdx = 0; SchedSelIdx <= hrd->cpb_cnt_minus1; SchedSelIdx++)
	{
		hrd->bit_rate_value_minus1[SchedSelIdx] = b.ReadUE();
		hrd->cpb_size_value_minus1[SchedSelIdx] = b.ReadUE();
		hrd->cbr_flag[SchedSelIdx] = b.ReadU1();
	}
	hrd->initial_cpb_removal_delay_length_minus1 = b.ReadU(5);
	hrd->cpb_removal_delay_length_minus1 = b.ReadU(5);
	hrd->dpb_output_delay_length_minus1 = b.ReadU(5);
	hrd->time_offset_length = b.ReadU(5);
}

Json::Value hrd_to_json(sps_t::hrd_t* hrd)
{
	Json::Value json_hrd;
	int SchedSelIdx;

	json_hrd["cpb_cnt_minus1"] = hrd->cpb_cnt_minus1;
	json_hrd["bit_rate_scale"] = hrd->bit_rate_scale;
	json_hrd["cpb_size_scale"] = hrd->cpb_size_scale;
	Json::Value json_cpb_list;
	for (SchedSelIdx = 0; SchedSelIdx <= hrd->cpb_cnt_minus1; SchedSelIdx++)
	{
		Json::Value json_cpb;
		json_cpb["bit_rate_value_minus1"] = hrd->bit_rate_value_minus1[SchedSelIdx];
		json_cpb["cpb_size_value_minus1"] = hrd->cpb_size_value_minus1[SchedSelIdx];
		json_cpb["cbr_flag"] = hrd->cbr_flag[SchedSelIdx];
		json_cpb_list[SchedSelIdx] = json_cpb;
	}
	json_hrd["cpb"] = json_cpb_list;
	json_hrd["initial_cpb_removal_delay_length_minus1"] = hrd->initial_cpb_removal_delay_length_minus1;
	json_hrd["cpb_removal_delay_length_minus1"] = hrd->cpb_removal_delay_length_minus1;
	json_hrd["dpb_output_delay_length_minus1"] = hrd->dpb_output_delay_length_minus1;
	json_hrd["time_offset_length"] = hrd->time_offset_length;

	return json_hrd;
}

#define SAR_Extended      255        // Extended_SAR

//Appendix E.1.1 VUI parameters syntax
void read_vui_parameters(sps_t* sps, BitReader& b)
{
	sps->vui.aspect_ratio_info_present_flag = b.ReadU1();
	if (sps->vui.aspect_ratio_info_present_flag)
	{
		sps->vui.aspect_ratio_idc = b.ReadU8();
		if (sps->vui.aspect_ratio_idc == SAR_Extended)
		{
			sps->vui.sar_width = b.ReadU(16);
			sps->vui.sar_height = b.ReadU(16);
		}
	}
	sps->vui.overscan_info_present_flag = b.ReadU1();
	if (sps->vui.overscan_info_present_flag)
	{
		sps->vui.overscan_appropriate_flag = b.ReadU1();
	}
	sps->vui.video_signal_type_present_flag = b.ReadU1();
	if (sps->vui.video_signal_type_present_flag)
	{
		sps->vui.video_format = b.ReadU(3);
		sps->vui.video_full_range_flag = b.ReadU1();
		sps->vui.colour_description_present_flag = b.ReadU1();
		if (sps->vui.colour_description_present_flag)
		{
			sps->vui.colour_primaries = b.ReadU8();
			sps->vui.transfer_characteristics = b.ReadU8();
			sps->vui.matrix_coefficients = b.ReadU8();
		}
	}
	sps->vui.chroma_loc_info_present_flag = b.ReadU1();
	if (sps->vui.chroma_loc_info_present_flag)
	{
		sps->vui.chroma_sample_loc_type_top_field = b.ReadUE();
		sps->vui.chroma_sample_loc_type_bottom_field = b.ReadUE();
	}
	sps->vui.timing_info_present_flag = b.ReadU1();
	if (sps->vui.timing_info_present_flag)
	{
		sps->vui.num_units_in_tick = b.ReadU(32);
		sps->vui.time_scale = b.ReadU(32);
		sps->vui.fixed_frame_rate_flag = b.ReadU1();
	}
	sps->vui.nal_hrd_parameters_present_flag = b.ReadU1();
	if (sps->vui.nal_hrd_parameters_present_flag)
	{
		read_hrd_parameters(&sps->nal_hrd, b);
	}
	sps->vui.vcl_hrd_parameters_present_flag = b.ReadU1();
	if (sps->vui.vcl_hrd_parameters_present_flag)
	{
		read_hrd_parameters(&sps->vcl_hrd, b);
	}
	if (sps->vui.nal_hrd_parameters_present_flag || sps->vui.vcl_hrd_parameters_present_flag)
	{
		sps->vui.low_delay_hrd_flag = b.ReadU1();
	}
	sps->vui.pic_struct_present_flag = b.ReadU1();
	sps->vui.bitstream_restriction_flag = b.ReadU1();
	if (sps->vui.bitstream_restriction_flag)
	{
		sps->vui.motion_vectors_over_pic_boundaries_flag = b.ReadU1();
		sps->vui.max_bytes_per_pic_denom = b.ReadUE();
		sps->vui.max_bits_per_mb_denom = b.ReadUE();
		sps->vui.log2_max_mv_length_horizontal = b.ReadUE();
		sps->vui.log2_max_mv_length_vertical = b.ReadUE();
		sps->vui.num_reorder_frames = b.ReadUE();
		sps->vui.max_dec_frame_buffering = b.ReadUE();
	}
}

Json::Value vui_to_json(sps_t* sps)
{
	Json::Value json_vui;

	json_vui["aspect_ratio_info_present_flag"] = sps->vui.aspect_ratio_info_present_flag;
	if (sps->vui.aspect_ratio_info_present_flag)
	{
		json_vui["aspect_ratio_idc"] = sps->vui.aspect_ratio_idc;
		if (sps->vui.aspect_ratio_idc == SAR_Extended)
		{
			json_vui["sar_width"] = sps->vui.sar_width;
			json_vui["sar_height"] = sps->vui.sar_height;
		}
	}
	json_vui["overscan_info_present_flag"] = sps->vui.overscan_info_present_flag;
	if (sps->vui.overscan_info_present_flag)
		json_vui["overscan_appropriate_flag"] = sps->vui.overscan_appropriate_flag;
	json_vui["video_signal_type_present_flag"] = sps->vui.video_signal_type_present_flag;
	if (sps->vui.video_signal_type_present_flag)
	{
		json_vui["video_format"] = sps->vui.video_format;
		json_vui["video_full_range_flag"] = sps->vui.video_full_range_flag;
		json_vui["colour_description_present_flag"] = sps->vui.colour_description_present_flag;
		if (sps->vui.colour_description_present_flag)
		{
			json_vui["colour_primaries"] = sps->vui.colour_primaries;
			json_vui["transfer_characteristics"] = sps->vui.transfer_characteristics;
			json_vui["matrix_coefficients"] = sps->vui.matrix_coefficients;
		}
	}
	json_vui["chroma_loc_info_present_flag"] = sps->vui.chroma_loc_info_present_flag;
	if (sps->vui.chroma_loc_info_present_flag)
	{
		json_vui["chroma_sample_loc_type_top_field"] = sps->vui.chroma_sample_loc_type_top_field;
		json_vui["chroma_sample_loc_type_bottom_field"] = sps->vui.chroma_sample_loc_type_bottom_field;
	}
	json_vui["timing_info_present_flag"] = sps->vui.timing_info_present_flag;
	if (sps->vui.timing_info_present_flag)
	{
		json_vui["num_units_in_tick"] = sps->vui.num_units_in_tick;
		json_vui["time_scale"] = sps->vui.time_scale;
		json_vui["fixed_frame_rate_flag"] = sps->vui.fixed_frame_rate_flag;
	}
	json_vui["nal_hrd_parameters_present_flag"] = sps->vui.nal_hrd_parameters_present_flag;
	if (sps->vui.nal_hrd_parameters_present_flag)
		json_vui["nal_hrd"] = hrd_to_json(&sps->nal_hrd);
	json_vui["vcl_hrd_parameters_present_flag"] = sps->vui.vcl_hrd_parameters_present_flag;
	if (sps->vui.vcl_hrd_parameters_present_flag)
		json_vui["vcl_hrd"] = hrd_to_json(&sps->vcl_hrd);
	if (sps->vui.nal_hrd_parameters_present_flag || sps->vui.vcl_hrd_parameters_present_flag)
		json_vui["low_delay_hrd_flag"] = sps->vui.low_delay_hrd_flag;
	json_vui["pic_struct_present_flag"] = sps->vui.pic_struct_present_flag;
	json_vui["bitstream_restriction_flag"] = sps->vui.bitstream_restriction_flag;
	if (sps->vui.bitstream_restriction_flag)
	{
		json_vui["motion_vectors_over_pic_boundaries_flag"] = sps->vui.motion_vectors_over_pic_boundaries_flag;
		json_vui["max_bytes_per_pic_denom"] = sps->vui.max_bytes_per_pic_denom;
		json_vui["max_bits_per_mb_denom"] = sps->vui.max_bits_per_mb_denom;
		json_vui["log2_max_mv_length_horizontal"] = sps->vui.log2_max_mv_length_horizontal;
		json_vui["log2_max_mv_length_vertical"] = sps->vui.log2_max_mv_length_vertical;
		json_vui["num_reorder_frames"] = sps->vui.num_reorder_frames;
		json_vui["max_dec_frame_buffering"] = sps->vui.max_dec_frame_buffering;
	}

	return json_vui;
}

void read_scaling_list(BitReader& b, int* scalingList, int sizeOfScalingList, int useDefaultScalingMatrixFlag)
{
	if (scalingList == NULL)
		return;

	uint8_t lastScale = 8;
	uint8_t nextScale = 8;
	for (int j = 0; j < sizeOfScalingList; j++)
	{
		if (nextScale != 0)
		{
			int delta_scale = b.ReadSE();
			nextScale = (lastScale + delta_scale + 256) % 256;
			useDefaultScalingMatrixFlag = (j == 0 && nextScale == 0);
		}
		scalingList[j] = (nextScale == 0) ? lastScale : nextScale;
		lastScale = scalingList[j];
	}
}

void read_rbsp_trailing_bits(BitReader& b)
{
	b.ReadU1(); // rbsp_stop_one_bit, equal to 1
	while (!b.IsByteAligned())
		b.ReadU1(); // rbsp_alignment_zero_bit, equal to 0
}

void read_seq_parameter_set_rbsp(sps_t* sps, BitReader& b)
{
	int i;

	memset(sps, 0, sizeof(sps_t));
	sps->profile_idc = b.ReadU8();
	sps->constraint_set0_flag = b.ReadU1();
	sps->constraint_set1_flag = b.ReadU1();
	sps->constraint_set2_flag = b.ReadU1();
	sps->constraint_set3_flag = b.ReadU1();
	sps->constraint_set4_flag = b.ReadU1();
	sps->constraint_set5_flag = b.ReadU1();
	sps->reserved_zero_2bits = b.ReadU(2);
	sps->level_idc = b.ReadU8();
	sps->seq_parameter_set_id = b.ReadUE();
	sps->chroma_format_idc = 1;
	if (sps->profile_idc == 100 || sps->profile_idc == 110 || sps->profile_idc == 122 || sps->profile_idc == 144)
	{
		sps->chroma_format_idc = b.ReadUE();
		if (sps->chroma_format_idc == 3)
			sps->residual_colour_transform_flag = b.ReadU1();
		sps->bit_depth_luma_minus8 = b.ReadUE();
		sps->bit_depth_chroma_minus8 = b.ReadUE();
		sps->qpprime_y_zero_transform_bypass_flag = b.ReadU1();
		sps->seq_scaling_matrix_present_flag = b.ReadU1();
		if (sps->seq_scaling_matrix_present_flag)
		{
			for (i = 0; i < 8; i++)
			{
				sps->seq_scaling_list_present_flag[i] = b.ReadU1();
				if (sps->seq_scaling_list_present_flag[i])
				{
					if (i < 6)
						read_scaling_list(b, sps->ScalingList4x4[i], 16, sps->UseDefaultScalingMatrix4x4Flag[i]);
					else
						read_scaling_list(b, sps->ScalingList8x8[i - 6], 64, sps->UseDefaultScalingMatrix8x8Flag[i - 6]);
				}
			}
		}
	}
	sps->log2_max_frame_num_minus4 = b.ReadUE();
	sps->pic_order_cnt_type = b.ReadUE();
	if (sps->pic_order_cnt_type == 0)
		sps->log2_max_pic_order_cnt_lsb_minus4 = b.ReadUE();
	else if (sps->pic_order_cnt_type == 1)
	{
		sps->delta_pic_order_always_zero_flag = b.ReadU1();
		sps->offset_for_non_ref_pic = b.ReadSE();
		sps->offset_for_top_to_bottom_field = b.ReadSE();
		sps->num_ref_frames_in_pic_order_cnt_cycle = b.ReadUE();
		for (i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
			sps->offset_for_ref_frame[i] = b.ReadSE();
	}
	sps->num_ref_frames = b.ReadUE();
	sps->gaps_in_frame_num_value_allowed_flag = b.ReadU1();
	sps->pic_width_in_mbs_minus1 = b.ReadUE();
	sps->pic_height_in_map_units_minus1 = b.ReadUE();
	sps->frame_mbs_only_flag = b.ReadU1();
	if (!sps->frame_mbs_only_flag)
		sps->mb_adaptive_frame_field_flag = b.ReadU1();
	sps->direct_8x8_inference_flag = b.ReadU1();
	sps->frame_cropping_flag = b.ReadU1();
	if (sps->frame_cropping_flag)
	{
		sps->frame_crop_left_offset = b.ReadUE();
		sps->frame_crop_right_offset = b.ReadUE();
		sps->frame_crop_top_offset = b.ReadUE();
		sps->frame_crop_bottom_offset = b.ReadUE();
	}
	sps->vui_parameters_present_flag = b.ReadU1();
	if (sps->vui_parameters_present_flag)
		read_vui_parameters(sps, b);
	read_rbsp_trailing_bits(b);
}

Json::Value sps_to_json(sps_t* sps)
{
	Json::Value json_sps;

	json_sps["profile_idc"] = sps->profile_idc;
	json_sps["constraint_set0_flag"] = sps->constraint_set0_flag;
	json_sps["constraint_set1_flag"] = sps->constraint_set1_flag;
	json_sps["constraint_set2_flag"] = sps->constraint_set2_flag;
	json_sps["constraint_set3_flag"] = sps->constraint_set3_flag;
	json_sps["constraint_set4_flag"] = sps->constraint_set4_flag;
	json_sps["constraint_set5_flag"] = sps->constraint_set5_flag;
	json_sps["reserved_zero_2bits"] = sps->reserved_zero_2bits;
	json_sps["level_idc"] = sps->level_idc;
	json_sps["seq_parameter_set_id"] = sps->seq_parameter_set_id;
	json_sps["chroma_format_idc"] = sps->chroma_format_idc;
	if (sps->profile_idc == 100 || sps->profile_idc == 110 || sps->profile_idc == 122 || sps->profile_idc == 144)
	{
		json_sps["chroma_format_idc"] = sps->chroma_format_idc;
		if (sps->chroma_format_idc == 3)
			json_sps["residual_colour_transform_flag"] = sps->residual_colour_transform_flag;
		json_sps["bit_depth_luma_minus8"] = sps->bit_depth_luma_minus8;
		json_sps["bit_depth_chroma_minus8"] = sps->bit_depth_chroma_minus8;
		json_sps["qpprime_y_zero_transform_bypass_flag"] = sps->qpprime_y_zero_transform_bypass_flag;
		json_sps["seq_scaling_matrix_present_flag"] = sps->seq_scaling_matrix_present_flag;
		if (sps->seq_scaling_matrix_present_flag)
		{
			Json::Value json_seq_scaling_matrix;
			for (int i = 0; i < 8; i++)
			{
				Json::Value json_seq_scaling_list;
				json_seq_scaling_list["seq_scaling_list_present_flag"] = sps->seq_scaling_list_present_flag[i];
				if (sps->seq_scaling_list_present_flag[i])
				{
					if (i < 6)
					{
						Json::Value jsonScalingList4x4;
						for (int j = 0; j < 16; j++)
							jsonScalingList4x4.append(sps->ScalingList4x4[i][j]);
						json_seq_scaling_list["ScalingList4x4"] = jsonScalingList4x4;
						json_seq_scaling_list["UseDefaultScalingMatrix4x4Flag"] = sps->UseDefaultScalingMatrix4x4Flag[i];
					}
					else
					{
						Json::Value jsonScalingList8x8;
						for (int j = 0; j < 64; j++)
							jsonScalingList8x8.append(sps->ScalingList8x8[i - 6][j]);
						json_seq_scaling_list["ScalingList8x8"] = jsonScalingList8x8;
						json_seq_scaling_list["UseDefaultScalingMatrix8x8Flag"] = sps->UseDefaultScalingMatrix8x8Flag[i - 6];
					}
				}
				json_seq_scaling_matrix[i] = json_seq_scaling_list;
			}
			json_sps["seq_scaling_matrix"] = json_seq_scaling_matrix;
		}
	}

	json_sps["log2_max_frame_num_minus4"] = sps->log2_max_frame_num_minus4;
	json_sps["pic_order_cnt_type"] = sps->pic_order_cnt_type;
	if (sps->pic_order_cnt_type == 0)
		json_sps["log2_max_pic_order_cnt_lsb_minus4"] = sps->log2_max_pic_order_cnt_lsb_minus4;
	else if (sps->pic_order_cnt_type == 1)
	{
		json_sps["delta_pic_order_always_zero_flag"] = sps->delta_pic_order_always_zero_flag;
		json_sps["offset_for_non_ref_pic"] = sps->offset_for_non_ref_pic;
		json_sps["offset_for_top_to_bottom_field"] = sps->offset_for_top_to_bottom_field;
		json_sps["num_ref_frames_in_pic_order_cnt_cycle"] = sps->num_ref_frames_in_pic_order_cnt_cycle;
		Json::Value json_offset_for_ref_frame;
		for (int i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
			json_offset_for_ref_frame[i] = sps->offset_for_ref_frame[i];
		json_sps["offset_for_ref_frame"] = json_offset_for_ref_frame;
	}
	json_sps["num_ref_frames"] = sps->num_ref_frames;
	json_sps["gaps_in_frame_num_value_allowed_flag"] = sps->gaps_in_frame_num_value_allowed_flag;
	json_sps["pic_width_in_mbs_minus1"] = sps->pic_width_in_mbs_minus1;
	json_sps["pic_height_in_map_units_minus1"] = sps->pic_height_in_map_units_minus1;
	json_sps["frame_mbs_only_flag"] = sps->frame_mbs_only_flag;
	if (!sps->frame_mbs_only_flag)
		json_sps["mb_adaptive_frame_field_flag"] = sps->mb_adaptive_frame_field_flag;
	json_sps["direct_8x8_inference_flag"] = sps->direct_8x8_inference_flag;
	json_sps["frame_cropping_flag"] = sps->frame_cropping_flag;
	if (sps->frame_cropping_flag)
	{
		json_sps["frame_crop_left_offset"] = sps->frame_crop_left_offset;
		json_sps["frame_crop_right_offset"] = sps->frame_crop_right_offset;
		json_sps["frame_crop_top_offset"] = sps->frame_crop_top_offset;
		json_sps["frame_crop_bottom_offset"] = sps->frame_crop_bottom_offset;
	}
	json_sps["vui_parameters_present_flag"] = sps->vui_parameters_present_flag;
	if (sps->vui_parameters_present_flag)
		json_sps["vui"] = vui_to_json(sps);

	return json_sps;
}

int more_rbsp_data(BitReader& b)
{
	if (b.Eof())
		return 0;
	if (b.PeekU1() == 1) // if next bit is 1, we've reached the stop bit
		return 0;
	return 1;
}

int intlog2(int x)
{
	int log = 0;
	if (x < 0) { x = 0; }
	while ((x >> log) > 0)
	{
		log++;
	}
	if (log > 0 && x == 1 << (log - 1)) { log--; }
	return log;
}

//7.3.2.2 Picture parameter set RBSP syntax
void read_pic_parameter_set_rbsp(pps_t* pps, BitReader& b)
{
	int i, i_group;

	memset(pps, 0, sizeof(pps_t));
	pps->pic_parameter_set_id = b.ReadUE();
	pps->seq_parameter_set_id = b.ReadUE();
	pps->entropy_coding_mode_flag = b.ReadU1();
	pps->pic_order_present_flag = b.ReadU1();
	pps->num_slice_groups_minus1 = b.ReadUE();

	if (pps->num_slice_groups_minus1 > 0)
	{
		pps->slice_group_map_type = b.ReadUE();
		if (pps->slice_group_map_type == 0)
		{
			for (i_group = 0; i_group <= pps->num_slice_groups_minus1; i_group++)
			{
				pps->run_length_minus1[i_group] = b.ReadUE();
			}
		}
		else if (pps->slice_group_map_type == 2)
		{
			for (i_group = 0; i_group < pps->num_slice_groups_minus1; i_group++)
			{
				pps->top_left[i_group] = b.ReadUE();
				pps->bottom_right[i_group] = b.ReadUE();
			}
		}
		else if (pps->slice_group_map_type == 3 ||
			pps->slice_group_map_type == 4 ||
			pps->slice_group_map_type == 5)
		{
			pps->slice_group_change_direction_flag = b.ReadU1();
			pps->slice_group_change_rate_minus1 = b.ReadUE();
		}
		else if (pps->slice_group_map_type == 6)
		{
			pps->pic_size_in_map_units_minus1 = b.ReadUE();
			for (i = 0; i <= pps->pic_size_in_map_units_minus1; i++)
			{
				pps->slice_group_id[i] = b.ReadU(intlog2(pps->num_slice_groups_minus1 + 1)); // was u(v)
			}
		}
	}
	pps->num_ref_idx_l0_active_minus1 = b.ReadUE();
	pps->num_ref_idx_l1_active_minus1 = b.ReadUE();
	pps->weighted_pred_flag = b.ReadU1();
	pps->weighted_bipred_idc = b.ReadU(2);
	pps->pic_init_qp_minus26 = b.ReadSE();
	pps->pic_init_qs_minus26 = b.ReadSE();
	pps->chroma_qp_index_offset = b.ReadSE();
	pps->deblocking_filter_control_present_flag = b.ReadU1();
	pps->constrained_intra_pred_flag = b.ReadU1();
	pps->redundant_pic_cnt_present_flag = b.ReadU1();

	pps->_more_rbsp_data_present = more_rbsp_data(b);
	if (pps->_more_rbsp_data_present)
	{
		pps->transform_8x8_mode_flag = b.ReadU1();
		pps->pic_scaling_matrix_present_flag = b.ReadU1();
		if (pps->pic_scaling_matrix_present_flag)
		{
			for (i = 0; i < 6 + 2 * pps->transform_8x8_mode_flag; i++)
			{
				pps->pic_scaling_list_present_flag[i] = b.ReadU1();
				if (pps->pic_scaling_list_present_flag[i])
				{
					if (i < 6)
					{
						read_scaling_list(b, pps->ScalingList4x4[i], 16,
							pps->UseDefaultScalingMatrix4x4Flag[i]);
					}
					else
					{
						read_scaling_list(b, pps->ScalingList8x8[i - 6], 64,
							pps->UseDefaultScalingMatrix8x8Flag[i - 6]);
					}
				}
			}
		}
		pps->second_chroma_qp_index_offset = b.ReadSE();
	}
	read_rbsp_trailing_bits(b);
}

Json::Value pps_to_json(pps_t* pps)
{
	int i, i_group;
	Json::Value json_pps;

	json_pps["pic_parameter_set_id"] = pps->pic_parameter_set_id;
	json_pps["seq_parameter_set_id"] = pps->seq_parameter_set_id;
	json_pps["entropy_coding_mode_flag"] = pps->entropy_coding_mode_flag;
	json_pps["pic_order_present_flag"] = pps->pic_order_present_flag;
	json_pps["num_slice_groups_minus1"] = pps->num_slice_groups_minus1;
	if (pps->num_slice_groups_minus1 > 0)
	{
		json_pps["slice_group_map_type"] = pps->slice_group_map_type;
		if (pps->slice_group_map_type == 0)
		{
			Json::Value json_run_length_minus1;
			for (i_group = 0; i_group <= pps->num_slice_groups_minus1; i_group++)
				json_run_length_minus1[i_group] = pps->run_length_minus1[i_group];
			json_pps["run_length_minus1"] = json_run_length_minus1;
		}
		else if (pps->slice_group_map_type == 2)
		{
			Json::Value json_top_left;
			Json::Value json_bottom_right;
			for (i_group = 0; i_group < pps->num_slice_groups_minus1; i_group++)
			{
				json_top_left[i_group] = pps->top_left[i_group];
				json_bottom_right[i_group] = pps->bottom_right[i_group];
			}
			json_pps["top_left"] = json_top_left;
			json_pps["bottom_right"] = json_bottom_right;
		}
		else if (pps->slice_group_map_type == 3 ||
			pps->slice_group_map_type == 4 ||
			pps->slice_group_map_type == 5)
		{
			json_pps["slice_group_change_direction_flag"] = pps->slice_group_change_direction_flag;
			json_pps["slice_group_change_rate_minus1"] = pps->slice_group_change_rate_minus1;
		}
		else if (pps->slice_group_map_type == 6)
		{
			json_pps["pic_size_in_map_units_minus1"] = pps->pic_size_in_map_units_minus1;
			Json::Value json_slice_group_id;
			for (i = 0; i <= pps->pic_size_in_map_units_minus1; i++)
				json_slice_group_id[i] = pps->slice_group_id[i];
			json_pps["slice_group_id"] = json_slice_group_id;
		}
	}
	json_pps["num_ref_idx_l0_active_minus1"] = pps->num_ref_idx_l0_active_minus1;
	json_pps["num_ref_idx_l1_active_minus1"] = pps->num_ref_idx_l1_active_minus1;
	json_pps["weighted_pred_flag"] = pps->weighted_pred_flag;
	json_pps["weighted_bipred_idc"] = pps->weighted_bipred_idc;
	json_pps["pic_init_qp_minus26"] = pps->pic_init_qp_minus26;
	json_pps["pic_init_qs_minus26"] = pps->pic_init_qs_minus26;
	json_pps["chroma_qp_index_offset"] = pps->chroma_qp_index_offset;
	json_pps["deblocking_filter_control_present_flag"] = pps->deblocking_filter_control_present_flag;
	json_pps["constrained_intra_pred_flag"] = pps->constrained_intra_pred_flag;
	json_pps["redundant_pic_cnt_present_flag"] = pps->redundant_pic_cnt_present_flag;
	json_pps["_more_rbsp_data_present"] = pps->_more_rbsp_data_present;
	if (pps->_more_rbsp_data_present)
	{
		json_pps["transform_8x8_mode_flag"] = pps->transform_8x8_mode_flag;
		json_pps["pic_scaling_matrix_present_flag"] = pps->pic_scaling_matrix_present_flag;
		if (pps->pic_scaling_matrix_present_flag)
		{
			Json::Value json_pic_scaling_matrix;
			for (i = 0; i < 6 + 2 * pps->transform_8x8_mode_flag; i++)
			{
				Json::Value json_pic_scaling_list;
				json_pic_scaling_list["pic_scaling_list_present_flag"] = pps->pic_scaling_list_present_flag[i];
				if (pps->pic_scaling_list_present_flag[i])
				{
					if (i < 6)
					{
						Json::Value jsonScalingList4x4;
						for (int j = 0; j < 16; j++)
							jsonScalingList4x4.append(pps->ScalingList4x4[i][j]);
						json_pic_scaling_list["ScalingList4x4"] = jsonScalingList4x4;
						json_pic_scaling_list["UseDefaultScalingMatrix4x4Flag"] = pps->UseDefaultScalingMatrix4x4Flag[i];
					}
					else
					{
						Json::Value jsonScalingList8x8;
						for (int j = 0; j < 64; j++)
							jsonScalingList8x8.append(pps->ScalingList8x8[i - 6][j]);
						json_pic_scaling_list["ScalingList8x8"] = jsonScalingList8x8;
						json_pic_scaling_list["UseDefaultScalingMatrix8x8Flag"] = pps->UseDefaultScalingMatrix8x8Flag[i - 6];
					}
				}
				json_pic_scaling_matrix[i] = json_pic_scaling_list;
			}
			json_pps["pic_scaling_matrix"] = json_pic_scaling_matrix;
		}
		json_pps["second_chroma_qp_index_offset"] = pps->second_chroma_qp_index_offset;
	}

	return json_pps;
}

int is_slice_type(int slice_type, int cmp_type)
{
	if (slice_type >= 5)
		slice_type -= 5;
	if (cmp_type >= 5)
		cmp_type -= 5;
	if (slice_type == cmp_type)
		return 1;
	else
		return 0;
}

bool check_zero(void* ptr, uint32_t size)
{
	static unsigned char zero[10240] = { 0 };
	if (size > 10240)
		return false;
	return (memcmp(ptr, zero, size) == 0);
}

//7.3.3.1 Reference picture list reordering syntax
void read_ref_pic_list_reordering(slice_header_t* sh, BitReader& b)
{
	if (!is_slice_type(sh->slice_type, SLICE_TYPE_I) && !is_slice_type(sh->slice_type, SLICE_TYPE_SI))
	{
		sh->rplr.ref_pic_list_reordering_flag_l0 = b.ReadU1();
		if (sh->rplr.ref_pic_list_reordering_flag_l0)
		{
			do
			{
				sh->rplr.reordering_of_pic_nums_idc = b.ReadUE();
				if (sh->rplr.reordering_of_pic_nums_idc == 0 ||
					sh->rplr.reordering_of_pic_nums_idc == 1)
				{
					sh->rplr.abs_diff_pic_num_minus1 = b.ReadUE();
				}
				else if (sh->rplr.reordering_of_pic_nums_idc == 2)
				{
					sh->rplr.long_term_pic_num = b.ReadUE();
				}
			} while (sh->rplr.reordering_of_pic_nums_idc != 3 && !b.Eof());
		}
	}
	if (is_slice_type(sh->slice_type, SLICE_TYPE_B))
	{
		sh->rplr.ref_pic_list_reordering_flag_l1 = b.ReadU1();
		if (sh->rplr.ref_pic_list_reordering_flag_l1)
		{
			do
			{
				sh->rplr.reordering_of_pic_nums_idc = b.ReadUE();
				if (sh->rplr.reordering_of_pic_nums_idc == 0 ||
					sh->rplr.reordering_of_pic_nums_idc == 1)
				{
					sh->rplr.abs_diff_pic_num_minus1 = b.ReadUE();
				}
				else if (sh->rplr.reordering_of_pic_nums_idc == 2)
				{
					sh->rplr.long_term_pic_num = b.ReadUE();
				}
			} while (sh->rplr.reordering_of_pic_nums_idc != 3 && !b.Eof());
		}
	}
}

Json::Value rplr_to_json(slice_header_t* sh)
{
	Json::Value json_rplr;

	if (!is_slice_type(sh->slice_type, SLICE_TYPE_I) && !is_slice_type(sh->slice_type, SLICE_TYPE_SI))
	{
		json_rplr["ref_pic_list_reordering_flag_l0"] = sh->rplr.ref_pic_list_reordering_flag_l0;
		if (sh->rplr.ref_pic_list_reordering_flag_l0)
		{
			json_rplr["reordering_of_pic_nums_idc"] = sh->rplr.reordering_of_pic_nums_idc;
			if (sh->rplr.reordering_of_pic_nums_idc == 0 ||
				sh->rplr.reordering_of_pic_nums_idc == 1)
				json_rplr["abs_diff_pic_num_minus1"] = sh->rplr.abs_diff_pic_num_minus1;
			else if (sh->rplr.reordering_of_pic_nums_idc == 2)
				json_rplr["long_term_pic_num"] = sh->rplr.long_term_pic_num;
		}
	}
	if (is_slice_type(sh->slice_type, SLICE_TYPE_B))
	{
		json_rplr["ref_pic_list_reordering_flag_l1"] = sh->rplr.ref_pic_list_reordering_flag_l1;
		if (sh->rplr.ref_pic_list_reordering_flag_l1)
		{
			json_rplr["reordering_of_pic_nums_idc"] = sh->rplr.reordering_of_pic_nums_idc;
			if (sh->rplr.reordering_of_pic_nums_idc == 0 ||
				sh->rplr.reordering_of_pic_nums_idc == 1)
				json_rplr["abs_diff_pic_num_minus1"] = sh->rplr.abs_diff_pic_num_minus1;
			else if (sh->rplr.reordering_of_pic_nums_idc == 2)
				json_rplr["long_term_pic_num"] = sh->rplr.long_term_pic_num;
		}
	}

	return json_rplr;
}

//7.3.3.2 Prediction weight table syntax
void read_pred_weight_table(slice_header_t* sh, BitReader& b, sps_t* sps, pps_t* pps)
{
	int i, j;

	sh->pwt.luma_log2_weight_denom = b.ReadUE();
	if (sps->chroma_format_idc != 0)
	{
		sh->pwt.chroma_log2_weight_denom = b.ReadUE();
	}
	for (i = 0; i <= pps->num_ref_idx_l0_active_minus1; i++)
	{
		sh->pwt.luma_weight_l0_flag[i] = b.ReadU1();
		if (sh->pwt.luma_weight_l0_flag[i])
		{
			sh->pwt.luma_weight_l0[i] = b.ReadSE();
			sh->pwt.luma_offset_l0[i] = b.ReadSE();
		}
		if (sps->chroma_format_idc != 0)
		{
			sh->pwt.chroma_weight_l0_flag[i] = b.ReadU1();
			if (sh->pwt.chroma_weight_l0_flag[i])
			{
				for (j = 0; j < 2; j++)
				{
					sh->pwt.chroma_weight_l0[i][j] = b.ReadSE();
					sh->pwt.chroma_offset_l0[i][j] = b.ReadSE();
				}
			}
		}
	}
	if (is_slice_type(sh->slice_type, SLICE_TYPE_B))
	{
		for (i = 0; i <= pps->num_ref_idx_l1_active_minus1; i++)
		{
			sh->pwt.luma_weight_l1_flag[i] = b.ReadU1();
			if (sh->pwt.luma_weight_l1_flag[i])
			{
				sh->pwt.luma_weight_l1[i] = b.ReadSE();
				sh->pwt.luma_offset_l1[i] = b.ReadSE();
			}
			if (sps->chroma_format_idc != 0)
			{
				sh->pwt.chroma_weight_l1_flag[i] = b.ReadU1();
				if (sh->pwt.chroma_weight_l1_flag[i])
				{
					for (j = 0; j < 2; j++)
					{
						sh->pwt.chroma_weight_l1[i][j] = b.ReadSE();
						sh->pwt.chroma_offset_l1[i][j] = b.ReadSE();
					}
				}
			}
		}
	}
}

Json::Value pwt_to_json(slice_header_t* sh)
{
	int i, j;
	Json::Value json_pwt;

	json_pwt["luma_log2_weight_denom"] = sh->pwt.luma_log2_weight_denom;
	json_pwt["chroma_log2_weight_denom"] = sh->pwt.chroma_log2_weight_denom;
	for (i = 0; i <= 64; i++)
	{
		if (check_zero(&sh->pwt.luma_weight_l0_flag[i], 64 - i))
			break;
		json_pwt["luma_weight_l0_flag"][i] = sh->pwt.luma_weight_l0_flag[i];
		if (sh->pwt.luma_weight_l0_flag[i])
		{
			json_pwt["luma_weight_l0"][i] = sh->pwt.luma_weight_l0[i];
			json_pwt["luma_offset_l0"][i] = sh->pwt.luma_offset_l0[i];
		}
		else
		{
			json_pwt["luma_weight_l0"][i] = 0;
			json_pwt["luma_offset_l0"][i] = 0;
		}
	}
	for (i = 0; i <= 64; i++)
	{
		if (check_zero(&sh->pwt.chroma_weight_l0_flag[i], 64 - i))
			break;
		json_pwt["chroma_weight_l0_flag"][i] = sh->pwt.chroma_weight_l0_flag[i];
		if (sh->pwt.chroma_weight_l0_flag[i])
		{
			for (j = 0; j < 2; j++)
			{
				json_pwt["chroma_weight_l0"][i][j] = sh->pwt.chroma_weight_l0[i][j];
				json_pwt["chroma_offset_l0"][i][j] = sh->pwt.chroma_offset_l0[i][j];
			}
		}
		else
		{
			for (j = 0; j < 2; j++)
			{
				json_pwt["chroma_weight_l0"][i][j] = 0;
				json_pwt["chroma_offset_l0"][i][j] = 0;
			}
		}
	}

	if (is_slice_type(sh->slice_type, SLICE_TYPE_B))
	{
		for (i = 0; i <= 64; i++)
		{
			if (check_zero(&sh->pwt.luma_weight_l1_flag[i], 64 - i))
				break;
			json_pwt["luma_weight_l1_flag"][i] = sh->pwt.luma_weight_l1_flag[i];
			if (sh->pwt.luma_weight_l1_flag[i])
			{
				json_pwt["luma_weight_l1"][i] = sh->pwt.luma_weight_l1[i];
				json_pwt["luma_offset_l1"][i] = sh->pwt.luma_offset_l1[i];
			}
			else
			{
				json_pwt["luma_weight_l1"][i] = 0;
				json_pwt["luma_offset_l1"][i] = 0;
			}
		}
		for (i = 0; i <= 64; i++)
		{
			if (check_zero(&sh->pwt.chroma_weight_l1_flag[i], 64 - i))
				break;
			json_pwt["chroma_weight_l1_flag"][i] = sh->pwt.chroma_weight_l1_flag[i];
			if (sh->pwt.chroma_weight_l1_flag[i])
			{
				for (j = 0; j < 2; j++)
				{
					json_pwt["chroma_weight_l1"][i][j] = sh->pwt.chroma_weight_l1[i][j];
					json_pwt["chroma_weight_l1"][i][j] = sh->pwt.chroma_weight_l1[i][j];
				}
			}
			else
			{
				for (j = 0; j < 2; j++)
				{
					json_pwt["chroma_weight_l1"][i][j] = 0;
					json_pwt["chroma_weight_l1"][i][j] = 0;
				}
			}
		}
	}

	return json_pwt;
}

//7.3.3.3 Decoded reference picture marking syntax
void read_dec_ref_pic_marking(slice_header_t* sh, BitReader& b, uint8_t nal_unit_type)
{
	if (nal_unit_type == 5)
	{
		sh->drpm.no_output_of_prior_pics_flag = b.ReadU1();
		sh->drpm.long_term_reference_flag = b.ReadU1();
	}
	else
	{
		sh->drpm.adaptive_ref_pic_marking_mode_flag = b.ReadU1();
		if (sh->drpm.adaptive_ref_pic_marking_mode_flag)
		{
			do
			{
				sh->drpm.memory_management_control_operation = b.ReadUE();
				if (sh->drpm.memory_management_control_operation == 1 ||
					sh->drpm.memory_management_control_operation == 3)
				{
					sh->drpm.difference_of_pic_nums_minus1 = b.ReadUE();
				}
				if (sh->drpm.memory_management_control_operation == 2)
				{
					sh->drpm.long_term_pic_num = b.ReadUE();
				}
				if (sh->drpm.memory_management_control_operation == 3 ||
					sh->drpm.memory_management_control_operation == 6)
				{
					sh->drpm.long_term_frame_idx = b.ReadUE();
				}
				if (sh->drpm.memory_management_control_operation == 4)
				{
					sh->drpm.max_long_term_frame_idx_plus1 = b.ReadUE();
				}
			} while (sh->drpm.memory_management_control_operation != 0 && !b.Eof());
		}
	}
}

Json::Value drpm_to_json(slice_header_t* sh, uint8_t nal_unit_type)
{
	Json::Value json_drpm;

	if (nal_unit_type == 5)
	{
		json_drpm["no_output_of_prior_pics_flag"] = sh->drpm.no_output_of_prior_pics_flag;
		json_drpm["long_term_reference_flag"] = sh->drpm.long_term_reference_flag;
	}
	else
	{
		json_drpm["adaptive_ref_pic_marking_mode_flag"] = sh->drpm.adaptive_ref_pic_marking_mode_flag;
		if (sh->drpm.adaptive_ref_pic_marking_mode_flag)
		{
			json_drpm["memory_management_control_operation"] = sh->drpm.memory_management_control_operation;
			if (sh->drpm.memory_management_control_operation == 1 ||
				sh->drpm.memory_management_control_operation == 3)
				json_drpm["difference_of_pic_nums_minus1"] = sh->drpm.difference_of_pic_nums_minus1;
			if (sh->drpm.memory_management_control_operation == 2)
				json_drpm["long_term_pic_num"] = sh->drpm.long_term_pic_num;
			if (sh->drpm.memory_management_control_operation == 3 ||
				sh->drpm.memory_management_control_operation == 6)
				json_drpm["long_term_frame_idx"] = sh->drpm.long_term_frame_idx;
			if (sh->drpm.memory_management_control_operation == 4)
				json_drpm["max_long_term_frame_idx_plus1"] = sh->drpm.max_long_term_frame_idx_plus1;
		}
	}

	return json_drpm;
}

//7.3.3 Slice header syntax
void read_slice_header_rbsp(slice_header_t* sh, BitReader& b, uint8_t nal_unit_type, uint8_t nal_ref_idc, sps_t* sps, pps_t* pps)
{
	memset(sh, 0, sizeof(slice_header_t));

	sh->first_mb_in_slice = b.ReadUE();
	sh->slice_type = b.ReadUE();
	sh->pic_parameter_set_id = b.ReadUE();

	sh->frame_num = b.ReadU(sps->log2_max_frame_num_minus4 + 4); // was u(v)
	if (!sps->frame_mbs_only_flag)
	{
		sh->field_pic_flag = b.ReadU1();
		if (sh->field_pic_flag)
		{
			sh->bottom_field_flag = b.ReadU1();
		}
	}
	if (nal_unit_type == 5)
	{
		sh->idr_pic_id = b.ReadUE();
	}
	if (sps->pic_order_cnt_type == 0)
	{
		sh->pic_order_cnt_lsb = b.ReadU(sps->log2_max_pic_order_cnt_lsb_minus4 + 4); // was u(v)
		if (pps->pic_order_present_flag && !sh->field_pic_flag)
		{
			sh->delta_pic_order_cnt_bottom = b.ReadSE();
		}
	}
	if (sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag)
	{
		sh->delta_pic_order_cnt[0] = b.ReadSE();
		if (pps->pic_order_present_flag && !sh->field_pic_flag)
		{
			sh->delta_pic_order_cnt[1] = b.ReadSE();
		}
	}
	if (pps->redundant_pic_cnt_present_flag)
	{
		sh->redundant_pic_cnt = b.ReadUE();
	}
	if (is_slice_type(sh->slice_type, SLICE_TYPE_B))
	{
		sh->direct_spatial_mv_pred_flag = b.ReadU1();
	}
	if (is_slice_type(sh->slice_type, SLICE_TYPE_P) || is_slice_type(sh->slice_type, SLICE_TYPE_SP) || is_slice_type(sh->slice_type, SLICE_TYPE_B))
	{
		sh->num_ref_idx_active_override_flag = b.ReadU1();
		if (sh->num_ref_idx_active_override_flag)
		{
			sh->num_ref_idx_l0_active_minus1 = b.ReadUE(); // FIXME does this modify the pps?
			if (is_slice_type(sh->slice_type, SLICE_TYPE_B))
			{
				sh->num_ref_idx_l1_active_minus1 = b.ReadUE();
			}
		}
	}
	read_ref_pic_list_reordering(sh, b);
	if ((pps->weighted_pred_flag && (is_slice_type(sh->slice_type, SLICE_TYPE_P) || is_slice_type(sh->slice_type, SLICE_TYPE_SP))) ||
		(pps->weighted_bipred_idc == 1 && is_slice_type(sh->slice_type, SLICE_TYPE_B)))
	{
		read_pred_weight_table(sh, b, sps, pps);
	}
	if (nal_ref_idc != 0)
	{
		read_dec_ref_pic_marking(sh, b, nal_unit_type);
	}
	if (pps->entropy_coding_mode_flag && !is_slice_type(sh->slice_type, SLICE_TYPE_I) && !is_slice_type(sh->slice_type, SLICE_TYPE_SI))
	{
		sh->cabac_init_idc = b.ReadUE();
	}
	sh->slice_qp_delta = b.ReadSE();
	if (is_slice_type(sh->slice_type, SLICE_TYPE_SP) || is_slice_type(sh->slice_type, SLICE_TYPE_SI))
	{
		if (is_slice_type(sh->slice_type, SLICE_TYPE_SP))
		{
			sh->sp_for_switch_flag = b.ReadU1();
		}
		sh->slice_qs_delta = b.ReadSE();
	}
	if (pps->deblocking_filter_control_present_flag)
	{
		sh->disable_deblocking_filter_idc = b.ReadUE();
		if (sh->disable_deblocking_filter_idc != 1)
		{
			sh->slice_alpha_c0_offset_div2 = b.ReadSE();
			sh->slice_beta_offset_div2 = b.ReadSE();
		}
	}
	if (pps->num_slice_groups_minus1 > 0 &&
		pps->slice_group_map_type >= 3 && pps->slice_group_map_type <= 5)
	{
		sh->slice_group_change_cycle =
			b.ReadU(intlog2(pps->pic_size_in_map_units_minus1 +
			pps->slice_group_change_rate_minus1 + 1)); // was u(v) // FIXME add 2?
	}
}

Json::Value slice_header_to_json(slice_header_t* sh, uint8_t nal_unit_type, uint8_t nal_ref_idc)
{
	Json::Value json_slice_header;

	json_slice_header["first_mb_in_slice"] = sh->first_mb_in_slice;
	json_slice_header["slice_type"] = GetSliceTypeString(sh->slice_type);
	json_slice_header["pic_parameter_set_id"] = sh->pic_parameter_set_id;
	json_slice_header["frame_num"] = sh->frame_num;
	json_slice_header["field_pic_flag"] = sh->field_pic_flag;
	if (sh->field_pic_flag && sh->bottom_field_flag)
		json_slice_header["bottom_field_flag"] = sh->bottom_field_flag;
	if (nal_unit_type == 5)
		json_slice_header["idr_pic_id"] = sh->idr_pic_id;
	json_slice_header["pic_order_cnt_lsb"] = sh->pic_order_cnt_lsb;

	if (!sh->field_pic_flag && sh->delta_pic_order_cnt_bottom)
		json_slice_header["delta_pic_order_cnt_bottom"] = sh->delta_pic_order_cnt_bottom;
	if (!check_zero(sh->delta_pic_order_cnt, 2))
	{
		json_slice_header["delta_pic_order_cnt"].append(sh->delta_pic_order_cnt[0]);
		if (!sh->field_pic_flag)
			json_slice_header["delta_pic_order_cnt"].append(sh->delta_pic_order_cnt[1]);
	}
	if (sh->redundant_pic_cnt)
		json_slice_header["redundant_pic_cnt"] = sh->redundant_pic_cnt;
	if (is_slice_type(sh->slice_type, SLICE_TYPE_B) && sh->direct_spatial_mv_pred_flag)
		json_slice_header["direct_spatial_mv_pred_flag"] = sh->direct_spatial_mv_pred_flag;
	if ((is_slice_type(sh->slice_type, SLICE_TYPE_P) || is_slice_type(sh->slice_type, SLICE_TYPE_SP) || is_slice_type(sh->slice_type, SLICE_TYPE_B))
		&& sh->num_ref_idx_active_override_flag)
	{
		json_slice_header["num_ref_idx_active_override_flag"] = sh->num_ref_idx_active_override_flag;
		if (sh->num_ref_idx_l0_active_minus1)
			json_slice_header["num_ref_idx_l0_active_minus1"] = sh->num_ref_idx_l0_active_minus1; // FIXME does this modify the pps?
		if (is_slice_type(sh->slice_type, SLICE_TYPE_B) && sh->num_ref_idx_l1_active_minus1)
			json_slice_header["num_ref_idx_l1_active_minus1"] = sh->num_ref_idx_l1_active_minus1;
	}

	if (!check_zero(&sh->rplr, sizeof(slice_header_t::rplr_t)))
		json_slice_header["ref_pic_list_reordering"] = rplr_to_json(sh);
	if (!check_zero(&sh->pwt, sizeof(slice_header_t::pwt_t)))
		json_slice_header["pred_weight_table"] = pwt_to_json(sh);
	if (!check_zero(&sh->drpm, sizeof(slice_header_t::drpm_t)))
		json_slice_header["dec_ref_pic_marking"] = drpm_to_json(sh, nal_unit_type);
	
	if (!is_slice_type(sh->slice_type, SLICE_TYPE_I) && !is_slice_type(sh->slice_type, SLICE_TYPE_SI) && sh->cabac_init_idc)
		json_slice_header["cabac_init_idc"] = sh->cabac_init_idc;
	json_slice_header["slice_qp_delta"] = sh->slice_qp_delta;
	if (is_slice_type(sh->slice_type, SLICE_TYPE_SP) || is_slice_type(sh->slice_type, SLICE_TYPE_SI))
	{
		if (is_slice_type(sh->slice_type, SLICE_TYPE_SP) && sh->sp_for_switch_flag)
			json_slice_header["sp_for_switch_flag"] = sh->sp_for_switch_flag;
		if (sh->slice_qs_delta)
			json_slice_header["slice_qs_delta"] = sh->slice_qs_delta;
	}

	if (sh->disable_deblocking_filter_idc)
		json_slice_header["disable_deblocking_filter_idc"] = sh->disable_deblocking_filter_idc;
	if (sh->disable_deblocking_filter_idc != 1 && (sh->slice_alpha_c0_offset_div2 || sh->slice_beta_offset_div2))
	{
		json_slice_header["slice_alpha_c0_offset_div2"] = sh->slice_alpha_c0_offset_div2;
		json_slice_header["slice_beta_offset_div2"] = sh->slice_beta_offset_div2;
	}
	if (sh->slice_group_change_cycle)
		json_slice_header["slice_group_change_cycle"] = sh->slice_group_change_cycle;

	return json_slice_header;
}

std::string GetSliceTypeString(uint8_t type)
{
	static const char* slice_type_strings[10] = { "P", "B", "I", "SP", "SI", "P_ONLY", "B_ONLY", "I_ONLY", "SP_ONLY", "SI_ONLY"};
	if (type < 10)
		return slice_type_strings[type];
	return "Unknown";
}

sei_t* sei_new()
{
	sei_t* s = (sei_t*)malloc(sizeof(sei_t));
	memset(s, 0, sizeof(sei_t));
	s->payload = NULL;
	return s;
}

void sei_free(sei_t* s)
{
	if (s->payload != NULL)
		free(s->payload);
	free(s);
}

int _read_ff_coded_number(BitReader& b)
{
	int n1 = 0;
	int n2;
	do
	{
		n2 = b.ReadU8();
		n1 += n2;
	} while (n2 == 0xff);
	return n1;
}

void read_sei_end_bits(BitReader& b)
{
	// if the message doesn't end at a byte border
	if (!b.IsByteAligned())
	{
		if (!b.ReadU1()) 
			fprintf(stderr, "WARNING: bit_equal_to_one is 0!!!!\n");
		while (!b.IsByteAligned())
		{
			if (b.ReadU1())
				fprintf(stderr, "WARNING: bit_equal_to_zero is 1!!!!\n");
		}
	}

	read_rbsp_trailing_bits(b);
}

// D.1 SEI payload syntax
void read_sei_payload(sei_t* s, BitReader& b, int payloadType, int payloadSize)
{
	if (payloadType == 5) //we only want "User data unregistered SEI"
	{
		b.SkipU(128); //uuid_iso_iec_11578
		s->payload = (uint8_t*)malloc(payloadSize - 16);
		for (int i = 0; i < payloadSize - 16; i++)
			s->payload[i] = b.ReadU(8);
		s->payloadSize = payloadSize - 16;
	}
	else if (payloadType == 100)
	{
		s->payload = (uint8_t*)malloc(payloadSize);
		for (int i = 0; i < payloadSize; i++)
			s->payload[i] = b.ReadU(8);
		s->payloadSize = payloadSize;
	}
	else
		b.SkipBytes(payloadSize);
	read_sei_end_bits(b);
}

//7.3.2.3.1 Supplemental enhancement information message syntax
void read_sei_message(sei_t* sei, BitReader& b)
{
	sei->payloadType = _read_ff_coded_number(b);
	sei->payloadSize = _read_ff_coded_number(b);
	read_sei_payload(sei, b, sei->payloadType, sei->payloadSize);
}

//7.3.2.3 Supplemental enhancement information RBSP syntax
sei_t** read_sei_rbsp(uint32_t *num_seis, BitReader& b)
{
	if (!num_seis)
		return NULL;

	sei_t **seis = NULL;
	*num_seis = 0;
	do {
		(*num_seis)++;
		seis = (sei_t**)realloc(seis, (*num_seis) * sizeof(sei_t*));
		seis[*num_seis - 1] = sei_new();
		sei_t* sei = seis[*num_seis - 1];
		read_sei_message(sei, b);
	} while (more_rbsp_data(b));
	read_rbsp_trailing_bits(b);

	return seis;
}

void release_seis(sei_t** seis, uint32_t num_seis)
{
	for (uint32_t i = 0; i < num_seis; i++)
	{
		if (seis[i])
			sei_free(seis[i]);
	}
	free(seis);
}

Json::Value seis_to_json(sei_t** seis, uint32_t num_seis)
{
	Json::Value json_seis;

	if (!seis || !num_seis)
		return json_seis;

	for (uint32_t i = 0; i < num_seis; i++)
	{
		Json::Value json_sei;
		sei_t *sei = seis[i];
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
