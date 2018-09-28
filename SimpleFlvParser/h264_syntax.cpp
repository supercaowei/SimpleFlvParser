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
	int i;
	int j = 0;
	int count = 0;

	for (i = 1; i < *nal_size; i++)
	{
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

	*nal_size = i;
	*rbsp_size = j;
	return j;
}

//Appendix E.1.2 HRD parameters syntax
void read_hrd_parameters(sps_t* sps, BitReader& b)
{
	int SchedSelIdx;

	sps->hrd.cpb_cnt_minus1 = b.ReadUE();
	sps->hrd.bit_rate_scale = b.ReadU(4);
	sps->hrd.cpb_size_scale = b.ReadU(4);
	for (SchedSelIdx = 0; SchedSelIdx <= sps->hrd.cpb_cnt_minus1; SchedSelIdx++)
	{
		sps->hrd.bit_rate_value_minus1[SchedSelIdx] = b.ReadUE();
		sps->hrd.cpb_size_value_minus1[SchedSelIdx] = b.ReadUE();
		sps->hrd.cbr_flag[SchedSelIdx] = b.ReadU1();
	}
	sps->hrd.initial_cpb_removal_delay_length_minus1 = b.ReadU(5);
	sps->hrd.cpb_removal_delay_length_minus1 = b.ReadU(5);
	sps->hrd.dpb_output_delay_length_minus1 = b.ReadU(5);
	sps->hrd.time_offset_length = b.ReadU(5);
}

#define SAR_Extended      255        // Extended_SAR

//Appendix E.1.1 VUI parameters syntax
void read_vui_parameters(/*h264_stream_t* h, */sps_t* sps, BitReader& b)
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
		read_hrd_parameters(sps, b);
	}
	sps->vui.vcl_hrd_parameters_present_flag = b.ReadU1();
	if (sps->vui.vcl_hrd_parameters_present_flag)
	{
		read_hrd_parameters(sps, b);
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

void read_scaling_list(BitReader& b, uint8_t* scalingList, int sizeOfScalingList, int useDefaultScalingMatrixFlag)
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
	int rbsp_stop_one_bit = b.ReadU1(); // equal to 1
	while (!b.IsByteAligned())
		int rbsp_alignment_zero_bit = b.ReadU1(); // equal to 0
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
	s->payload = (uint8_t*)malloc(payloadSize);
	int i;
	for (i = 0; i < payloadSize; i++)
		s->payload[i] = b.ReadU(8);
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
