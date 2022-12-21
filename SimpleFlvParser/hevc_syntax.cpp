#include "hevc_syntax.h"
#include "utils.h"
#include <math.h>

#define MIN(a,b) ((a)<(b)?(a):(b))

int hevc_more_rbsp_data(BitReader& b) {
	if (b.Eof())
		return 0;
	if (b.PeekU1() == 1) // if next bit is 1, we've reached the stop bit
		return 0;
	return 1;
}

void read_hevc_rbsp_trailing_bits(BitReader& b)
{
	b.ReadU1(); // rbsp_stop_one_bit, equal to 1
	while (!b.IsByteAligned())
		b.ReadU1(); // rbsp_alignment_zero_bit, equal to 0
}

int read_hevc_ff_coded_number(BitReader& b)
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

//7.3.3 Profile, tier and level syntax
void parse_profile_tier_level(profile_tier_level_t* s, BitReader& b, int maxNumSubLayersMinus1) 
{
	int i, j;
	s->general_profile_space = b.ReadU(2); 
	s->general_tier_flag = b.ReadU(1); 
	s->general_profile_idc = b.ReadU(5); 
	for (j = 0; j < 32; j++)
		s->general_profile_compatibility_flag[j] = b.ReadU(1); 
	s->general_progressive_source_flag = b.ReadU(1); 
	s->general_interlaced_source_flag = b.ReadU(1); 
	s->general_non_packed_constraint_flag = b.ReadU(1); 
	s->general_frame_only_constraint_flag = b.ReadU(1); 
	b.SkipU(44); //general_reserved_zero_44bits
	s->general_level_idc = b.ReadU(8);
	for (i = 0; i < maxNumSubLayersMinus1; i++) {
		s->sub_layer_profile_present_flag[i] = b.ReadU(1); 
		s->sub_layer_level_present_flag[i] = b.ReadU(1); 
	} 
	if (maxNumSubLayersMinus1 > 0) 
		for (i = maxNumSubLayersMinus1; i < 8; i++) 
			b.SkipU(2); //reserved_zero_2bits[i]
	for (i = 0; i < maxNumSubLayersMinus1; i++) { 
		if (s->sub_layer_profile_present_flag[i]) {
			s->sub_layer_profile_space[i] = b.ReadU(2); 
			s->sub_layer_tier_flag[i] = b.ReadU(1); 
			s->sub_layer_profile_idc[i] = b.ReadU(5); 
			for (j = 0; j < 32; j++)
				s->sub_layer_profile_compatibility_flag[i][j] = b.ReadU(1); 
			s->sub_layer_progressive_source_flag[i] = b.ReadU(1); 
			s->sub_layer_interlaced_source_flag[i] = b.ReadU(1); 
			s->sub_layer_non_packed_constraint_flag[i] = b.ReadU(1); 
			s->sub_layer_frame_only_constraint_flag[i] = b.ReadU(1); 
			b.SkipU(44); //sub_layer_reserved_zero_44bits[i]
		}
		if (s->sub_layer_level_present_flag[i])
			s->sub_layer_level_idc[i] = b.ReadU(8); 
	}
}

//E.2.3 Sub-layer HRD parameters syntax
void parse_sub_layer_hrd_parameters(sub_layer_hrd_parameters_t* s, BitReader& b, int subLayerId, int CpbCnt, int sub_pic_hrd_params_present_flag) 
{
	int i = 0;
	for (i = 0; i <= CpbCnt; i++) { 
		s->bit_rate_value_minus1[i] = b.ReadUE(); 
		s->cpb_size_value_minus1[i] = b.ReadUE(); 
		if (sub_pic_hrd_params_present_flag) {
		s->cpb_size_du_value_minus1[i] = b.ReadUE(); 
		s->bit_rate_du_value_minus1[i] = b.ReadUE(); 
		} 
		s->cbr_flag[i] = b.ReadU(1); 
	}
}

//E.2.2 HRD parameters syntax
void parse_hrd_parameters(hrd_parameters_t* s, BitReader& b, int commonInfPresentFlag, int maxNumSubLayersMinus1)
{
	int i;
	if (commonInfPresentFlag) { 
		s->nal_hrd_parameters_present_flag = b.ReadU(1); 
		s->vcl_hrd_parameters_present_flag = b.ReadU(1); 
		if (s->nal_hrd_parameters_present_flag || s->vcl_hrd_parameters_present_flag){
			s->sub_pic_hrd_params_present_flag = b.ReadU(1); 
			if (s->sub_pic_hrd_params_present_flag) {
				s->tick_divisor_minus2 = b.ReadU(8); 
				s->du_cpb_removal_delay_increment_length_minus1 = b.ReadU(5); 
				s->sub_pic_cpb_params_in_pic_timing_sei_flag = b.ReadU(1); 
				s->dpb_output_delay_du_length_minus1 = b.ReadU(5); 
			}
			s->bit_rate_scale = b.ReadU(4); 
			s->cpb_size_scale = b.ReadU(4); 
			if (s->sub_pic_hrd_params_present_flag) 
				s->cpb_size_du_scale = b.ReadU(4); 
			s->initial_cpb_removal_delay_length_minus1 = b.ReadU(5); 
			s->au_cpb_removal_delay_length_minus1 = b.ReadU(5); 
			s->dpb_output_delay_length_minus1 = b.ReadU(5); 
		}
	} 
	for (i = 0; i <= maxNumSubLayersMinus1; i++) { 
		s->fixed_pic_rate_general_flag[i] = b.ReadU(1); 
		if (!s->fixed_pic_rate_general_flag[i])
			s->fixed_pic_rate_within_cvs_flag[i] = b.ReadU(1); 
		if (s->fixed_pic_rate_within_cvs_flag[i]) 
			s->elemental_duration_in_tc_minus1[i] = b.ReadUE(); 
		else 
			s->low_delay_hrd_flag[i] = b.ReadU(1); 
		if (!s->low_delay_hrd_flag[i]) 
			s->cpb_cnt_minus1[i] = b.ReadUE(); 
		if (s->nal_hrd_parameters_present_flag) 
			parse_sub_layer_hrd_parameters(&s->nal_hrd[i], b, i, s->cpb_cnt_minus1[i], s->sub_pic_hrd_params_present_flag);
		if (s->vcl_hrd_parameters_present_flag) 
			parse_sub_layer_hrd_parameters(&s->vcl_hrd[i], b, i, s->cpb_cnt_minus1[i], s->sub_pic_hrd_params_present_flag);
	}
}

void parse_scaling_list_data(scaling_list_data_t* s, BitReader& b) {
	int sizeId, matrixId, i;
	for (sizeId = 0; sizeId < 4; sizeId++) {
		for (matrixId = 0; matrixId < ((sizeId == 3) ? 2 : 6); matrixId++) {
			s->scaling_list_pred_mode_flag[sizeId][matrixId] = b.ReadU(1); 
			if (!s->scaling_list_pred_mode_flag[sizeId][matrixId])
				s->scaling_list_pred_matrix_id_delta[sizeId][matrixId] = b.ReadUE(); 
			else {
				int nextCoef = 8;
				int coefNum = MIN(64, (1 << (4 + (sizeId << 1))));
				if (sizeId > 1) {
					s->scaling_list_dc_coef_minus8[sizeId - 2][matrixId] = b.ReadSE();
					nextCoef = s->scaling_list_dc_coef_minus8[sizeId - 2][matrixId] + 8;
				}
				for (i = 0; i < coefNum; i++) {
					s->scaling_list_delta_coef[i] = b.ReadSE(); 
					nextCoef = (nextCoef + s->scaling_list_delta_coef[i] + 256) % 256;
					//ScalingList[sizeId][matrixId][i] = nextCoef
				}
			} 
		}
	}
}

//7.4.8 Short-term reference picture set semantics
void short_term_ref_fill_varibles(short_term_ref_pic_set_t* s, int stRpsIdx, hevc_global_variables_t* v) {
	int i, j, dPoc;
	int RefRpsIdx = stRpsIdx - (s->delta_idx_minus1 + 1);
	int deltaRps = (1 - 2 * s->delta_rps_sign) * (s->abs_delta_rps_minus1 + 1);
	if (s->inter_ref_pic_set_prediction_flag) {
		i = 0;
		for (j = v->NumPositivePics[RefRpsIdx] - 1; j >= 0; j--) { 
			dPoc = v->DeltaPocS1[RefRpsIdx][j] + deltaRps;
			if(dPoc < 0 && s->use_delta_flag[v->NumNegativePics[RefRpsIdx] + j]) { 
				v->DeltaPocS0[stRpsIdx][i] = dPoc;
				v->UsedByCurrPicS0[stRpsIdx][i++] = s->used_by_curr_pic_flag[v->NumNegativePics[RefRpsIdx] + j];
			} 
		}
		if (deltaRps < 0 && s->use_delta_flag[v->NumDeltaPocs[RefRpsIdx]]) {
			v->DeltaPocS0[stRpsIdx][i] = deltaRps;
			v->UsedByCurrPicS0[stRpsIdx][i++] = s->used_by_curr_pic_flag[v->NumDeltaPocs[RefRpsIdx]];
		}
		for (j = 0; j < v->NumNegativePics[RefRpsIdx]; j++) { 
			dPoc = v->DeltaPocS0[RefRpsIdx][j] + deltaRps;
			if (dPoc < 0 && s->use_delta_flag[j]) { 
				v->DeltaPocS0[stRpsIdx][i] = dPoc;
				v->UsedByCurrPicS0[stRpsIdx][i++] = s->used_by_curr_pic_flag[j];
			} 
		} 
		v->NumNegativePics[stRpsIdx] = i;

		i = 0;
		for(j = v->NumNegativePics[RefRpsIdx] - 1; j >= 0; j--) { 
			dPoc = v->DeltaPocS0[RefRpsIdx][j] + deltaRps;
			if (dPoc > 0 && s->use_delta_flag[j]) { 
				v->DeltaPocS1[stRpsIdx][i] = dPoc;
				v->UsedByCurrPicS1[stRpsIdx][i++] = s->used_by_curr_pic_flag[j];
			} 
		}
		if (deltaRps > 0 && s->use_delta_flag[v->NumDeltaPocs[RefRpsIdx]]) {
			v->DeltaPocS1[stRpsIdx][i] = deltaRps;
			v->UsedByCurrPicS1[stRpsIdx][i++] = s->used_by_curr_pic_flag[v->NumDeltaPocs[RefRpsIdx]];
		} 
		for (j = 0; j < v->NumPositivePics[RefRpsIdx]; j++) { 
			dPoc = v->DeltaPocS1[RefRpsIdx][j] + deltaRps;
			if (dPoc > 0 && s->use_delta_flag[v->NumNegativePics[RefRpsIdx] + j]) { 
				v->DeltaPocS1[stRpsIdx][i] = dPoc;
				v->UsedByCurrPicS1[stRpsIdx][i++] = s->used_by_curr_pic_flag[v->NumNegativePics[RefRpsIdx] + j]; 
			} 
		} 
		v->NumPositivePics[stRpsIdx] = i;
	} else {
		v->NumNegativePics[stRpsIdx] = s->num_negative_pics;
		v->NumPositivePics[stRpsIdx] = s->num_positive_pics;
		for (i = 0; i < s->num_negative_pics; i++) {
			v->UsedByCurrPicS0[stRpsIdx][i] = s->used_by_curr_pic_s0_flag[i];
			v->DeltaPocS0[stRpsIdx][i] = (i == 0 ? 0 : v->DeltaPocS0[stRpsIdx][i - 1]) - (s->delta_poc_s0_minus1[i] + 1);
		}
		for (i = 0; i < s->num_positive_pics; i++) {
			v->UsedByCurrPicS1[stRpsIdx][i] = s->used_by_curr_pic_s1_flag[i];
			v->DeltaPocS1[stRpsIdx][i] = (i == 0 ? 0 : v->DeltaPocS1[stRpsIdx][i - 1]) + (s->delta_poc_s1_minus1[i] + 1);
		}
	}
	v->NumDeltaPocs[stRpsIdx] = v->NumNegativePics[stRpsIdx] + v->NumPositivePics[stRpsIdx];
}

//7.3.7 Short-term reference picture set syntax
//TODO 这里可能理解不对
void parse_short_term_ref_pic_set(short_term_ref_pic_set_t* s, BitReader& b, int stRpsIdx, hevc_sps_t* sps) 
{
	int i, j;
	memset(s, 0, sizeof(short_term_ref_pic_set_t));
	hevc_global_variables_t* v = &sps->hevc_global_variables;
	if (stRpsIdx != 0) 
		s->inter_ref_pic_set_prediction_flag = b.ReadU(1); 
	if (s->inter_ref_pic_set_prediction_flag) {
		if (stRpsIdx == sps->num_short_term_ref_pic_sets) 
			s->delta_idx_minus1 = b.ReadUE(); 
		s->delta_rps_sign = b.ReadU(1); 
		s->abs_delta_rps_minus1 = b.ReadUE();
		int RefRpsIdx = stRpsIdx - (s->delta_idx_minus1 + 1);
		for (j = 0; j <= v->NumDeltaPocs[RefRpsIdx]; j++) { 
			s->used_by_curr_pic_flag[j] = b.ReadU(1); 
			if (!s->used_by_curr_pic_flag[j]) 
				s->use_delta_flag[j] = b.ReadU(1); 
		}
	} else { 
		s->num_negative_pics = b.ReadUE(); 
		s->num_positive_pics = b.ReadUE(); 
		for (i = 0; i < s->num_negative_pics; i++) { 
			s->delta_poc_s0_minus1[i] = b.ReadUE(); 
			s->used_by_curr_pic_s0_flag[i] = b.ReadU(1); 
		} 
		for (i = 0; i < s->num_positive_pics; i++) { 
			s->delta_poc_s1_minus1[i] = b.ReadUE(); 
			s->used_by_curr_pic_s1_flag[i] = b.ReadU(1); 
		} 
	}
	short_term_ref_fill_varibles(s, stRpsIdx, v);
}

int derive_UsedByCurrPicLt(hevc_sps_t* sps, hevc_slice_header_t* sh, int i) 
{
	if (i < sh->num_long_term_sps)
		return sps->used_by_curr_pic_lt_sps_flag[sh->lt_idx_sps[i]];
	else
		return sh->used_by_curr_pic_lt_flag[i];
}

int derive_CurrRpsIdx(hevc_sps_t* sps, hevc_slice_header_t* sh) 
{
	if (sh->short_term_ref_pic_set_sps_flag == 1) {
		return sh->short_term_ref_pic_set_idx;
	} else {
		return sps->num_short_term_ref_pic_sets;
	}
}

int derive_NumPocTotalCurr(hevc_sps_t* sps, hevc_slice_header_t* sh) 
{
	int NumPocTotalCurr = 0, i;
	hevc_global_variables_t* v = &sps->hevc_global_variables;
	int CurrRpsIdx = derive_CurrRpsIdx(sps, sh);
	for (i = 0; i < v->NumNegativePics[CurrRpsIdx]; i++) 
		if (v->UsedByCurrPicS0[CurrRpsIdx][i]) 
			NumPocTotalCurr++; 
	for (i = 0; i < v->NumPositivePics[CurrRpsIdx]; i++)
		if (v->UsedByCurrPicS1[CurrRpsIdx][i]) 
			NumPocTotalCurr++;
	for (i = 0; i < sh->num_long_term_sps + sh->num_long_term_pics; i++) 
		if (derive_UsedByCurrPicLt(sps, sh, i)) 
			NumPocTotalCurr++;
	return NumPocTotalCurr;
}

void parse_ref_pic_lists_modification(ref_pic_lists_modification_t* s, BitReader& b, hevc_sps_t* sps, hevc_slice_header_t* sh) 
{
	int i = 0;
	int NumPocTotalCurr = derive_NumPocTotalCurr(sps, sh);
	s->ref_pic_list_modification_flag_l0 = b.ReadU(1); 
	if (s->ref_pic_list_modification_flag_l0) 
		for (i = 0; i <= sh->num_ref_idx_l0_active_minus1; i++) 
			s->list_entry_l0[i] = b.ReadUV(NumPocTotalCurr); //u(v) 7.4.7.2
	if (sh->slice_type == HEVC_SLICE_TYPE_B) {
		s->ref_pic_list_modification_flag_l1 = b.ReadU(1); 
		if (s->ref_pic_list_modification_flag_l1) 
			for (i = 0; i <= sh->num_ref_idx_l1_active_minus1; i++) 
				s->list_entry_l1[i] = b.ReadUV(NumPocTotalCurr); //u(v) 7.4.7.2
	}
}

void parse_pred_weight_table(pred_weight_table_t* s, BitReader& b, hevc_sps_t* sps, hevc_slice_header_t* sh) 
{
	int i, j;
	s->luma_log2_weight_denom = b.ReadUE(); 
	if (sps->chroma_format_idc != 0) 
		s->delta_chroma_log2_weight_denom = b.ReadSE(); 
	for (i = 0; i <= sh->num_ref_idx_l0_active_minus1; i++)
		s->luma_weight_l0_flag[i] = b.ReadU(1); 
	if (sps->chroma_format_idc != 0) 
		for (i = 0; i <= sh->num_ref_idx_l0_active_minus1; i++) 
			s->chroma_weight_l0_flag[i] = b.ReadU(1); 
	for (i = 0; i <= sh->num_ref_idx_l0_active_minus1; i++) {
		if (s->luma_weight_l0_flag[i]) {
			s->delta_luma_weight_l0[i] = b.ReadSE(); 
			s->luma_offset_l0[i] = b.ReadSE(); 
		}
		if (s->chroma_weight_l0_flag[i])
			for (j = 0; j < 2; j++) {
				s->delta_chroma_weight_l0[i][j] = b.ReadSE(); 
				s->delta_chroma_offset_l0[i][j] = b.ReadSE(); 
			} 
	} 
	if (sh->slice_type == HEVC_SLICE_TYPE_B) { 
		for (i = 0; i <= sh->num_ref_idx_l1_active_minus1; i++) 
			s->luma_weight_l1_flag[i] = b.ReadU(1); 
		if (sps->chroma_format_idc != 0)
			for (i = 0; i <= sh->num_ref_idx_l1_active_minus1; i++) 
				s->chroma_weight_l1_flag[i] = b.ReadU(1); 
		for (i = 0; i <= sh->num_ref_idx_l1_active_minus1; i++) { 
			if (s->luma_weight_l1_flag[i]) { 
				s->delta_luma_weight_l1[i] = b.ReadSE(); 
				s->luma_offset_l1[i] = b.ReadSE(); 
			} 
			if (s->chroma_weight_l1_flag[i]) 
				for (j = 0; j < 2; j++) { 
					s->delta_chroma_weight_l1[i][j] = b.ReadSE(); 
					s->delta_chroma_offset_l1[i][j] = b.ReadSE(); 
				} 
		} 
	}
}

#define EXTENDED_SAR 255 //Table E.1 – Interpretation of sample aspect ratio indicator

void parse_vui_parameters(vui_parameters_t* s, BitReader& b, hevc_sps_t* sps)
{
	s->aspect_ratio_info_present_flag = b.ReadU(1); 
	if (s->aspect_ratio_info_present_flag) {
		s->aspect_ratio_idc = b.ReadU(8); 
		if (s->aspect_ratio_idc == EXTENDED_SAR) {
			s->sar_width = b.ReadU(16); 
			s->sar_height = b.ReadU(16); 
		}
	} 
	s->overscan_info_present_flag = b.ReadU(1); 
	if (s->overscan_info_present_flag) 
		s->overscan_appropriate_flag = b.ReadU(1); 
	s->video_signal_type_present_flag = b.ReadU(1); 
	if (s->video_signal_type_present_flag) {
		s->video_format = b.ReadU(3); 
		s->video_full_range_flag = b.ReadU(1); 
		s->colour_description_present_flag = b.ReadU(1); 
		if (s->colour_description_present_flag) { 
			s->colour_primaries = b.ReadU(8); 
			s->transfer_characteristics = b.ReadU(8); 
			s->matrix_coeffs = b.ReadU(8); 
		} 
	}
	s->chroma_loc_info_present_flag = b.ReadU(1); 
	if (s->chroma_loc_info_present_flag) {
		s->chroma_sample_loc_type_top_field = b.ReadUE(); 
		s->chroma_sample_loc_type_bottom_field = b.ReadUE(); 
	}
	s->neutral_chroma_indication_flag = b.ReadU(1); 
	s->field_seq_flag = b.ReadU(1); 
	s->frame_field_info_present_flag = b.ReadU(1); 
	s->default_display_window_flag = b.ReadU(1); 
	if (s->default_display_window_flag) { 
		s->def_disp_win_left_offset = b.ReadUE(); 
		s->def_disp_win_right_offset = b.ReadUE(); 
		s->def_disp_win_top_offset = b.ReadUE(); 
		s->def_disp_win_bottom_offset = b.ReadUE(); 
	} 
	s->vui_timing_info_present_flag = b.ReadU(1); 
	if (s->vui_timing_info_present_flag) { 
		s->vui_num_units_in_tick = b.ReadU(32); 
		s->vui_time_scale = b.ReadU(32); 
		s->vui_poc_proportional_to_timing_flag = b.ReadU(1); 
		if (s->vui_poc_proportional_to_timing_flag) 
			s->vui_num_ticks_poc_diff_one_minus1 = b.ReadUE();
		s->vui_hrd_parameters_present_flag = b.ReadU(1); 
		if (s->vui_hrd_parameters_present_flag)
			parse_hrd_parameters(&s->hrd, b, 1, sps->sps_max_sub_layers_minus1);
	}
	s->bitstream_restriction_flag = b.ReadU(1); 
	if (s->bitstream_restriction_flag) {
		s->tiles_fixed_structure_flag = b.ReadU(1); 
		s->motion_vectors_over_pic_boundaries_flag = b.ReadU(1); 
		s->restricted_ref_pic_lists_flag = b.ReadU(1); 
		s->min_spatial_segmentation_idc = b.ReadUE(); 
		s->max_bytes_per_pic_denom = b.ReadUE(); 
		s->max_bits_per_min_cu_denom = b.ReadUE(); 
		s->log2_max_mv_length_horizontal = b.ReadUE(); 
		s->log2_max_mv_length_vertical = b.ReadUE(); 
	}
}

//Table E.2 – Meaning of video_format
std::string hevc_vui_video_format_string(int video_format) {
	static const char* video_format_strs[] = {"Component", "PAL", "NTSC", "SECAM", "MAC", "Unspecified video format"};
	if (video_format >= 0 && video_format <= 5) {
		return video_format_strs[video_format];
	}
	return std::string("Unknown_") + std::to_string(video_format);
}

Json::Value hevc_vui_to_json(vui_parameters_t* s)
{
	Json::Value json_vui;
	json_vui["aspect_ratio_info_present_flag"] = s->aspect_ratio_info_present_flag;
	if (s->aspect_ratio_info_present_flag) {
		json_vui["aspect_ratio_idc"] = s->aspect_ratio_idc;
		if (s->aspect_ratio_idc == EXTENDED_SAR) {
			json_vui["sar_width"] = s->sar_width;
			json_vui["sar_height"] = s->sar_height;
		}
	}
	json_vui["overscan_info_present_flag"] = s->overscan_info_present_flag;
	if (s->overscan_info_present_flag) 
		json_vui["overscan_appropriate_flag"] = s->overscan_appropriate_flag;
	json_vui["video_signal_type_present_flag"] = s->video_signal_type_present_flag;
	if (s->video_signal_type_present_flag) {
		json_vui["video_format"] = hevc_vui_video_format_string(s->video_format);
		json_vui["video_full_range_flag"] = s->video_full_range_flag;
		json_vui["colour_description_present_flag"] = s->colour_description_present_flag;
		if (s->colour_description_present_flag) { 
			json_vui["colour_primaries"] = s->colour_primaries;
			json_vui["transfer_characteristics"] = s->transfer_characteristics;
			json_vui["matrix_coeffs"] = s->matrix_coeffs;
		} 
	}
	json_vui["chroma_loc_info_present_flag"] = s->chroma_loc_info_present_flag;
	if (s->chroma_loc_info_present_flag) {
		json_vui["chroma_sample_loc_type_top_field"] = s->chroma_sample_loc_type_top_field;
		json_vui["chroma_sample_loc_type_bottom_field"] = s->chroma_sample_loc_type_bottom_field;
	}
	json_vui["neutral_chroma_indication_flag"] = s->neutral_chroma_indication_flag;
	json_vui["field_seq_flag"] = s->field_seq_flag;
	json_vui["frame_field_info_present_flag"] = s->frame_field_info_present_flag;
	json_vui["default_display_window_flag"] = s->default_display_window_flag;
	if (s->default_display_window_flag) { 
		json_vui["def_disp_win_left_offset"] = s->def_disp_win_left_offset;
		json_vui["def_disp_win_right_offset"] = s->def_disp_win_right_offset;
		json_vui["def_disp_win_top_offset"] = s->def_disp_win_top_offset;
		json_vui["def_disp_win_bottom_offset"] = s->def_disp_win_bottom_offset;
	} 
	json_vui["vui_timing_info_present_flag"] = s->vui_timing_info_present_flag;
	if (s->vui_timing_info_present_flag) { 
		json_vui["vui_num_units_in_tick"] = s->vui_num_units_in_tick;
		json_vui["vui_time_scale"] = s->vui_time_scale;
		json_vui["vui_poc_proportional_to_timing_flag"] = s->vui_poc_proportional_to_timing_flag;
		if (s->vui_poc_proportional_to_timing_flag) 
			json_vui["vui_num_ticks_poc_diff_one_minus1"] = s->vui_num_ticks_poc_diff_one_minus1;
		json_vui["vui_hrd_parameters_present_flag"] = s->vui_hrd_parameters_present_flag;
		// if (s->vui_hrd_parameters_present_flag)
		// 	parse_hrd_parameters(&s->hrd, b, 1, sps->sps_max_sub_layers_minus1);
	}
	json_vui["bitstream_restriction_flag"] = s->bitstream_restriction_flag;
	if (s->bitstream_restriction_flag) {
		json_vui["tiles_fixed_structure_flag"] = s->tiles_fixed_structure_flag;
		json_vui["motion_vectors_over_pic_boundaries_flag"] = s->motion_vectors_over_pic_boundaries_flag;
		json_vui["restricted_ref_pic_lists_flag"] = s->restricted_ref_pic_lists_flag;
		json_vui["min_spatial_segmentation_idc"] = s->min_spatial_segmentation_idc;
		json_vui["max_bytes_per_pic_denom"] = s->max_bytes_per_pic_denom;
		json_vui["max_bits_per_min_cu_denom"] = s->max_bits_per_min_cu_denom;
		json_vui["log2_max_mv_length_horizontal"] = s->log2_max_mv_length_horizontal;
		json_vui["log2_max_mv_length_vertical"] = s->log2_max_mv_length_vertical;
	}

	return json_vui;
}

//7.3.2.1 Video parameter set RBSP syntax
void read_hevc_video_parameter_set_rbsp(hevc_vps_t* s, BitReader& b) 
{
	int i, j;
	memset(s, 0, sizeof(hevc_vps_t));
	s->vps_video_parameter_set_id = b.ReadU(4); 
	s->vps_reserved_three_2bits = b.ReadU(2); 
	s->vps_max_layers_minus1 = b.ReadU(6); 
	s->vps_max_sub_layers_minus1 = b.ReadU(3); 
	s->vps_temporal_id_nesting_flag = b.ReadU(1); 
	s->vps_reserved_0xffff_16bits = b.ReadU(16); 
	parse_profile_tier_level(&s->profile_tier_level, b, s->vps_max_sub_layers_minus1);
	s->vps_sub_layer_ordering_info_present_flag = b.ReadU(1); 
	for (i = (s->vps_sub_layer_ordering_info_present_flag ? 0 : s->vps_max_sub_layers_minus1); 
			i <= s->vps_max_sub_layers_minus1; i++) {
		s->vps_max_dec_pic_buffering_minus1[i] = b.ReadUE(); 
		s->vps_max_num_reorder_pics[i] = b.ReadUE(); 
		s->vps_max_latency_increase_plus1[i] = b.ReadUE(); 
	}
	s->vps_max_layer_id = b.ReadU(6); 
	s->vps_num_layer_sets_minus1 = b.ReadUE(); 
	for (i = 1; i <= s->vps_num_layer_sets_minus1; i++) 
		for (j = 0; j <= s->vps_max_layer_id; j++) 
			s->layer_id_included_flag[i][j] = b.ReadU(1); 
	s->vps_timing_info_present_flag = b.ReadU(1); 
	if (s->vps_timing_info_present_flag) {
		s->vps_num_units_in_tick = b.ReadU(32); 
		s->vps_time_scale = b.ReadU(32); 
		s->vps_poc_proportional_to_timing_flag = b.ReadU(1); 
		if (s->vps_poc_proportional_to_timing_flag)
			s->vps_num_ticks_poc_diff_one_minus1 = b.ReadUE(); 
		s->vps_num_hrd_parameters = b.ReadUE(); 
		for (i = 0; i < s->vps_num_hrd_parameters; i++) { 
			s->hrd_layer_set_idx[i] = b.ReadUE(); 
			if (i > 0) 
				s->cprms_present_flag[i] = b.ReadU(1); 
			parse_hrd_parameters(&s->hrd[i], b, s->cprms_present_flag[i], s->vps_max_sub_layers_minus1);
		} 
	} 
	int vps_extension_flag = b.ReadU(1); 
	if (vps_extension_flag)
		while(hevc_more_rbsp_data(b)) 
			b.SkipU(1); //vps_extension_data_flag
	read_hevc_rbsp_trailing_bits(b);
}

Json::Value hevc_vps_to_json(hevc_vps_t* vps)
{
	Json::Value json_vps;
	json_vps["vps_video_parameter_set_id"] = vps->vps_video_parameter_set_id;
	json_vps["vps_max_layers_minus1"] = vps->vps_max_layers_minus1;
	json_vps["vps_max_sub_layers_minus1"] = vps->vps_max_sub_layers_minus1;
	json_vps["vps_temporal_id_nesting_flag"] = vps->vps_temporal_id_nesting_flag;
	//parse_profile_tier_level(&s->profile_tier_level, b, s->vps_max_sub_layers_minus1);
	json_vps["vps_sub_layer_ordering_info_present_flag"] = vps->vps_sub_layer_ordering_info_present_flag;
	json_vps["vps_max_layer_id"] = vps->vps_max_layer_id;
	json_vps["vps_num_layer_sets_minus1"] = vps->vps_num_layer_sets_minus1;
	json_vps["vps_timing_info_present_flag"] = vps->vps_timing_info_present_flag;
	if (vps->vps_timing_info_present_flag) {
		json_vps["vps_num_units_in_tick"] = vps->vps_num_units_in_tick;
		json_vps["vps_time_scale"] = vps->vps_time_scale;
		json_vps["vps_poc_proportional_to_timing_flag"] = vps->vps_poc_proportional_to_timing_flag;
		json_vps["vps_num_ticks_poc_diff_one_minus1"] = vps->vps_num_ticks_poc_diff_one_minus1;
		json_vps["vps_num_hrd_parameters"] = vps->vps_num_hrd_parameters;
		//parse_hrd_parameters(&s->hrd[i], b, s->cprms_present_flag[i], s->vps_max_sub_layers_minus1);
	}
	return json_vps;
}

//7.4.3.2 Sequence parameter set RBSP semantics
void derive_sps_varibles(hevc_sps_t* s) 
{
	hevc_global_variables_t* v = &s->hevc_global_variables;
	v->MinCbLog2SizeY = s->log2_min_luma_coding_block_size_minus3 + 3;
	v->CtbLog2SizeY = v->MinCbLog2SizeY + s->log2_diff_max_min_luma_coding_block_size;
	v->MinCbSizeY = 1 << v->MinCbLog2SizeY;
	v->CtbSizeY = 1 << v->CtbLog2SizeY;
	v->PicWidthInMinCbsY = s->pic_width_in_luma_samples / v->MinCbSizeY;
	v->PicWidthInCtbsY = (int)ceil(s->pic_width_in_luma_samples * 1.0 / v->CtbSizeY);
	v->PicHeightInMinCbsY = s->pic_height_in_luma_samples / v->MinCbSizeY;
	v->PicHeightInCtbsY = (int)ceil(s->pic_height_in_luma_samples * 1.0 / v->CtbSizeY);
	v->PicSizeInMinCbsY = v->PicWidthInMinCbsY * v->PicHeightInMinCbsY;
	v->PicSizeInCtbsY = v->PicWidthInCtbsY * v->PicHeightInCtbsY;
	v->PicSizeInSamplesY = s->pic_width_in_luma_samples * s->pic_height_in_luma_samples;	
}

void read_hevc_seq_parameter_set_rbsp(hevc_sps_t* s, BitReader& b)
{
	int i;
	memset(s, 0, sizeof(hevc_sps_t));
	s->sps_video_parameter_set_id = b.ReadU(4); 
	s->sps_max_sub_layers_minus1 = b.ReadU(3); 
	s->sps_temporal_id_nesting_flag = b.ReadU(1); 
	parse_profile_tier_level(&s->profile_tier_level, b, s->sps_max_sub_layers_minus1);
	s->sps_seq_parameter_set_id = b.ReadUE(); 
	s->chroma_format_idc = b.ReadUE(); 
	if (s->chroma_format_idc == 3) 
		s->separate_colour_plane_flag = b.ReadU(1); 
	s->pic_width_in_luma_samples = b.ReadUE(); 
	s->pic_height_in_luma_samples = b.ReadUE(); 
	s->conformance_window_flag = b.ReadU(1); 
	if (s->conformance_window_flag) { 
		s->conf_win_left_offset = b.ReadUE(); 
		s->conf_win_right_offset = b.ReadUE(); 
		s->conf_win_top_offset = b.ReadUE(); 
		s->conf_win_bottom_offset = b.ReadUE(); 
	} 
	s->bit_depth_luma_minus8 = b.ReadUE(); 
	s->bit_depth_chroma_minus8 = b.ReadUE(); 
	s->log2_max_pic_order_cnt_lsb_minus4 = b.ReadUE(); 
	s->sps_sub_layer_ordering_info_present_flag = b.ReadU(1); 
	for (i = (s->sps_sub_layer_ordering_info_present_flag ? 0 : s->sps_max_sub_layers_minus1); 
			i <= s->sps_max_sub_layers_minus1; i++) {
		s->sps_max_dec_pic_buffering_minus1[i] = b.ReadUE(); 
		s->sps_max_num_reorder_pics[i] = b.ReadUE(); 
		s->sps_max_latency_increase_plus1[i] = b.ReadUE(); 
	} 
	s->log2_min_luma_coding_block_size_minus3 = b.ReadUE(); 
	s->log2_diff_max_min_luma_coding_block_size = b.ReadUE(); 
	s->log2_min_transform_block_size_minus2 = b.ReadUE(); 
	s->log2_diff_max_min_transform_block_size = b.ReadUE(); 
	s->max_transform_hierarchy_depth_inter = b.ReadUE(); 
	s->max_transform_hierarchy_depth_intra = b.ReadUE(); 
	s->scaling_list_enabled_flag = b.ReadU(1); 
	if (s->scaling_list_enabled_flag) { 
		s->sps_scaling_list_data_present_flag = b.ReadU(1); 
		if (s->sps_scaling_list_data_present_flag)
			parse_scaling_list_data(&s->scaling_list_data, b);
	} 
	s->amp_enabled_flag = b.ReadU(1); 
	s->sample_adaptive_offset_enabled_flag = b.ReadU(1); 
	s->pcm_enabled_flag = b.ReadU(1); 
	if (s->pcm_enabled_flag) { 
		s->pcm_sample_bit_depth_luma_minus1 = b.ReadU(4); 
		s->pcm_sample_bit_depth_chroma_minus1 = b.ReadU(4);
		s->log2_min_pcm_luma_coding_block_size_minus3 = b.ReadUE(); 
		s->log2_diff_max_min_pcm_luma_coding_block_size = b.ReadUE(); 
		s->pcm_loop_filter_disabled_flag = b.ReadU(1); 
	} 
	s->num_short_term_ref_pic_sets = b.ReadUE(); 
	for (i = 0; i < s->num_short_term_ref_pic_sets; i++)
		parse_short_term_ref_pic_set(&s->short_term_ref_pic_set[i], b, i, s);
	s->long_term_ref_pics_present_flag = b.ReadU(1); 
	if (s->long_term_ref_pics_present_flag) { 
		s->num_long_term_ref_pics_sps = b.ReadUE(); 
		for (i = 0; i < s->num_long_term_ref_pics_sps; i++) { 
			s->lt_ref_pic_poc_lsb_sps[i] = b.ReadU(s->log2_max_pic_order_cnt_lsb_minus4 + 4); //u(v) 7.4.3.2
			s->used_by_curr_pic_lt_sps_flag[i] = b.ReadU(1); 
		} 
	} 
	s->sps_temporal_mvp_enabled_flag = b.ReadU(1); 
	s->strong_intra_smoothing_enabled_flag = b.ReadU(1); 
	s->vui_parameters_present_flag = b.ReadU(1); 
	if (s->vui_parameters_present_flag) 
		parse_vui_parameters(&s->vui, b, s);
	derive_sps_varibles(s);
	int sps_extension_flag = b.ReadU(1); 
	if (sps_extension_flag) 
		while(hevc_more_rbsp_data(b))
			b.SkipU(1); //sps_extension_data_flag
	read_hevc_rbsp_trailing_bits(b);
}

Json::Value hevc_sps_to_json(hevc_sps_t* sps)
{
	Json::Value json_sps;
	json_sps["sps_video_parameter_set_id"] = sps->sps_video_parameter_set_id;
	json_sps["sps_max_sub_layers_minus1"] = sps->sps_max_sub_layers_minus1;
	json_sps["sps_temporal_id_nesting_flag"] = sps->sps_temporal_id_nesting_flag;
	//parse_profile_tier_level(&s->profile_tier_level, b, s->sps_max_sub_layers_minus1);
	json_sps["sps_seq_parameter_set_id"] = sps->sps_seq_parameter_set_id;
	json_sps["chroma_format_idc"] = sps->chroma_format_idc;
	json_sps["separate_colour_plane_flag"] = sps->separate_colour_plane_flag;
	json_sps["pic_width_in_luma_samples"] = sps->pic_width_in_luma_samples;
	json_sps["pic_height_in_luma_samples"] = sps->pic_height_in_luma_samples;
	if (sps->conformance_window_flag) { 
		json_sps["conf_win_left_offset"] = sps->conf_win_left_offset;
		json_sps["conf_win_right_offset"] = sps->conf_win_right_offset;
		json_sps["conf_win_top_offset"] = sps->conf_win_top_offset;
		json_sps["conf_win_bottom_offset"] = sps->conf_win_bottom_offset;
	}
	json_sps["bit_depth_luma_minus8"] = sps->bit_depth_luma_minus8;
	json_sps["bit_depth_chroma_minus8"] = sps->bit_depth_chroma_minus8;
	//s->log2_max_pic_order_cnt_lsb_minus4 = b.ReadUE(); 
	json_sps["sps_sub_layer_ordering_info_present_flag"] = sps->sps_sub_layer_ordering_info_present_flag;
	// s->log2_min_luma_coding_block_size_minus3 = b.ReadUE(); 
	// s->log2_diff_max_min_luma_coding_block_size = b.ReadUE(); 
	// s->log2_min_transform_block_size_minus2 = b.ReadUE(); 
	// s->log2_diff_max_min_transform_block_size = b.ReadUE(); 
	json_sps["max_transform_hierarchy_depth_inter"] = sps->max_transform_hierarchy_depth_inter;
	json_sps["max_transform_hierarchy_depth_intra"] = sps->max_transform_hierarchy_depth_intra;
	json_sps["scaling_list_enabled_flag"] = sps->scaling_list_enabled_flag;
	json_sps["sps_scaling_list_data_present_flag"] = sps->sps_scaling_list_data_present_flag;
	json_sps["amp_enabled_flag"] = sps->amp_enabled_flag;
	json_sps["sample_adaptive_offset_enabled_flag"] = sps->sample_adaptive_offset_enabled_flag;
	json_sps["pcm_enabled_flag"] = sps->pcm_enabled_flag;
	if (sps->pcm_enabled_flag) { 
		json_sps["pcm_sample_bit_depth_luma_minus1"] = sps->pcm_sample_bit_depth_luma_minus1;
		json_sps["pcm_sample_bit_depth_chroma_minus1"] = sps->pcm_sample_bit_depth_chroma_minus1;
		// s->log2_min_pcm_luma_coding_block_size_minus3 = b.ReadUE(); 
		// s->log2_diff_max_min_pcm_luma_coding_block_size = b.ReadUE(); 
		json_sps["pcm_loop_filter_disabled_flag"] = sps->pcm_loop_filter_disabled_flag;
	} 
	json_sps["num_short_term_ref_pic_sets"] = sps->num_short_term_ref_pic_sets;
	json_sps["long_term_ref_pics_present_flag"] = sps->long_term_ref_pics_present_flag;
	json_sps["num_long_term_ref_pics_sps"] = sps->num_long_term_ref_pics_sps;
	json_sps["sps_temporal_mvp_enabled_flag"] = sps->sps_temporal_mvp_enabled_flag;
	json_sps["strong_intra_smoothing_enabled_flag"] = sps->strong_intra_smoothing_enabled_flag;
	json_sps["vui_parameters_present_flag"] = sps->vui_parameters_present_flag;
	if (sps->vui_parameters_present_flag) 
		json_sps["vui"] = hevc_vui_to_json(&sps->vui);

	return json_sps;
}

void read_hevc_pic_parameter_set_rbsp(hevc_pps_t* s, BitReader& b)
{
	int i;
	memset(s, 0, sizeof(hevc_pps_t));
	s->pps_pic_parameter_set_id = b.ReadUE(); 
	s->pps_seq_parameter_set_id = b.ReadUE(); 
	s->dependent_slice_segments_enabled_flag = b.ReadU(1); 
	s->output_flag_present_flag = b.ReadU(1); 
	s->num_extra_slice_header_bits = b.ReadU(3); 
	s->sign_data_hiding_enabled_flag = b.ReadU(1); 
	s->cabac_init_present_flag = b.ReadU(1); 
	s->num_ref_idx_l0_default_active_minus1 = b.ReadUE(); 
	s->num_ref_idx_l1_default_active_minus1 = b.ReadUE(); 
	s->init_qp_minus26 = b.ReadSE(); 
	s->constrained_intra_pred_flag = b.ReadU(1); 
	s->transform_skip_enabled_flag = b.ReadU(1); 
	s->cu_qp_delta_enabled_flag = b.ReadU(1); 
	if (s->cu_qp_delta_enabled_flag)
		s->diff_cu_qp_delta_depth = b.ReadUE(); 
	s->pps_cb_qp_offset = b.ReadSE(); 
	s->pps_cr_qp_offset = b.ReadSE(); 
	s->pps_slice_chroma_qp_offsets_present_flag = b.ReadU(1); 
	s->weighted_pred_flag = b.ReadU(1); 
	s->weighted_bipred_flag = b.ReadU(1); 
	s->transquant_bypass_enabled_flag = b.ReadU(1); 
	s->tiles_enabled_flag = b.ReadU(1); 
	s->entropy_coding_sync_enabled_flag = b.ReadU(1); 
	if (s->tiles_enabled_flag) {
		s->num_tile_columns_minus1 = b.ReadUE(); 
		s->num_tile_rows_minus1 = b.ReadUE(); 
		s->uniform_spacing_flag = b.ReadU(1); 
		if (!s->uniform_spacing_flag) {
			for (i = 0; i < s->num_tile_columns_minus1; i++)
				s->column_width_minus1[i] = b.ReadUE(); 
			for (i = 0; i < s->num_tile_rows_minus1; i++)
				s->row_height_minus1[i] = b.ReadUE(); 
		}
		s->loop_filter_across_tiles_enabled_flag = b.ReadU(1); 
	} 
	s->pps_loop_filter_across_slices_enabled_flag = b.ReadU(1); 
	s->deblocking_filter_control_present_flag = b.ReadU(1); 
	if (s->deblocking_filter_control_present_flag) { 
		s->deblocking_filter_override_enabled_flag = b.ReadU(1); 
		s->pps_deblocking_filter_disabled_flag = b.ReadU(1); 
		if (!s->pps_deblocking_filter_disabled_flag) { 
			s->pps_beta_offset_div2 = b.ReadSE(); 
			s->pps_tc_offset_div2 = b.ReadSE(); 
		} 
	}
	s->pps_scaling_list_data_present_flag = b.ReadU(1); 
	if (s->pps_scaling_list_data_present_flag) 
		parse_scaling_list_data(&s->scaling_list_data, b); 
	s->lists_modification_present_flag = b.ReadU(1); 
	s->log2_parallel_merge_level_minus2 = b.ReadUE(); 
	s->slice_segment_header_extension_present_flag = b.ReadU(1); 
	int pps_extension_flag = b.ReadU(1); 
	if (pps_extension_flag) 
		while(hevc_more_rbsp_data(b)) 
			b.SkipU(1); //pps_extension_data_flag
	read_hevc_rbsp_trailing_bits(b);
}

Json::Value hevc_pps_to_json(hevc_pps_t* s)
{
	Json::Value json_pps;
	json_pps["pps_pic_parameter_set_id"] = s->pps_pic_parameter_set_id;
	json_pps["pps_seq_parameter_set_id"] = s->pps_seq_parameter_set_id;
	json_pps["dependent_slice_segments_enabled_flag"] = s->dependent_slice_segments_enabled_flag;
	json_pps["output_flag_present_flag"] = s->output_flag_present_flag;
	json_pps["num_extra_slice_header_bits"] = s->num_extra_slice_header_bits;
	json_pps["sign_data_hiding_enabled_flag"] = s->sign_data_hiding_enabled_flag;
	json_pps["cabac_init_present_flag"] = s->cabac_init_present_flag;
	json_pps["num_ref_idx_l0_default_active_minus1"] = s->num_ref_idx_l0_default_active_minus1;
	json_pps["num_ref_idx_l1_default_active_minus1"] = s->num_ref_idx_l1_default_active_minus1;
	json_pps["init_qp_minus26"] = s->init_qp_minus26;
	json_pps["constrained_intra_pred_flag"] = s->constrained_intra_pred_flag;
	json_pps["transform_skip_enabled_flag"] = s->transform_skip_enabled_flag;
	json_pps["cu_qp_delta_enabled_flag"] = s->cu_qp_delta_enabled_flag;
	if (s->cu_qp_delta_enabled_flag)
		json_pps["diff_cu_qp_delta_depth"] = s->diff_cu_qp_delta_depth;
	json_pps["pps_cb_qp_offset"] = s->pps_cb_qp_offset;
	json_pps["pps_cr_qp_offset"] = s->pps_cr_qp_offset;
	json_pps["pps_slice_chroma_qp_offsets_present_flag"] = s->pps_slice_chroma_qp_offsets_present_flag;
	json_pps["weighted_pred_flag"] = s->weighted_pred_flag;
	json_pps["weighted_bipred_flag"] = s->weighted_bipred_flag;
	json_pps["transquant_bypass_enabled_flag"] = s->transquant_bypass_enabled_flag;
	json_pps["tiles_enabled_flag"] = s->tiles_enabled_flag;
	json_pps["entropy_coding_sync_enabled_flag"] = s->entropy_coding_sync_enabled_flag;
	if (s->tiles_enabled_flag) {
		json_pps["num_tile_columns_minus1"] = s->num_tile_columns_minus1;
		json_pps["num_tile_rows_minus1"] = s->num_tile_rows_minus1;
		json_pps["uniform_spacing_flag"] = s->uniform_spacing_flag;
		if (!s->uniform_spacing_flag) {
			Json::Value column_width_minus1, row_height_minus1;
			for (int i = 0; i < s->num_tile_columns_minus1; i++)
				column_width_minus1.append(s->column_width_minus1[i]);
			for (int i = 0; i < s->num_tile_rows_minus1; i++)
				row_height_minus1.append(s->row_height_minus1[i]);
			json_pps["column_width_minus1"] = column_width_minus1;
			json_pps["row_height_minus1"] = row_height_minus1;
		}
		json_pps["loop_filter_across_tiles_enabled_flag"] = s->loop_filter_across_tiles_enabled_flag;
	} 
	json_pps["pps_loop_filter_across_slices_enabled_flag"] = s->pps_loop_filter_across_slices_enabled_flag;
	json_pps["deblocking_filter_control_present_flag"] = s->deblocking_filter_control_present_flag;
	if (s->deblocking_filter_control_present_flag) { 
		json_pps["deblocking_filter_override_enabled_flag"] = s->deblocking_filter_override_enabled_flag;
		json_pps["pps_deblocking_filter_disabled_flag"] = s->pps_deblocking_filter_disabled_flag;
		if (!s->pps_deblocking_filter_disabled_flag) { 
			json_pps["pps_beta_offset_div2"] = s->pps_beta_offset_div2;
			json_pps["pps_tc_offset_div2"] = s->pps_tc_offset_div2;
		} 
	}
	json_pps["pps_scaling_list_data_present_flag"] = s->pps_scaling_list_data_present_flag;
	// if (s->pps_scaling_list_data_present_flag) 
	// 	parse_scaling_list_data(&s->scaling_list_data, b); 
	json_pps["lists_modification_present_flag"] = s->lists_modification_present_flag;
	json_pps["log2_parallel_merge_level_minus2"] = s->log2_parallel_merge_level_minus2;
	json_pps["slice_segment_header_extension_present_flag"] = s->slice_segment_header_extension_present_flag;

	return json_pps;
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
		//D.2.7 User data unregistered SEI message syntax
		if (payloadType == 5)
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
			//reserved_payload_extension_data = 8 * payloadSize - nEarlierBits - nPayloadZeroBits - 1
			int nEarlierBits = (end - start + 1) * 8 - b.BitsLeft();
			int reserved_payload_extension_data = (8 * payloadSize - nEarlierBits) / 2;
			b.SkipU(reserved_payload_extension_data); //u(v)
		}
		read_sei_end_bits(b);
	}*/
}

void read_hevc_sei_message(hevc_sei_t* sei, BitReader& b, int nal_unit_type)
{
	sei->payloadType = read_hevc_ff_coded_number(b);
	sei->payloadSize = read_hevc_ff_coded_number(b);
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
	} while (hevc_more_rbsp_data(b));
	read_hevc_rbsp_trailing_bits(b);
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

//see 7.3.2.9 Slice segment layer RBSP syntax
//and 7.3.6.1 General slice segment header syntax
void hevc_slice_segment_header(hevc_slice_header_t* s, BitReader& b, uint8_t nal_unit_type, hevc_sps_t* sps, hevc_pps_t* pps) 
{
	//see H.265 Table 7-1 – NAL unit type codes and NAL unit type classes
	if(!((nal_unit_type >= HevcNaluTypeCodedSliceTrailN && nal_unit_type <= HevcNaluTypeCodedSliceTFD) || 
			(nal_unit_type >= HevcNaluTypeCodedSliceBLA && nal_unit_type <= HevcNaluTypeCodedSliceCRA)))
		return;

	int i;
	memset(s, 0, sizeof(hevc_slice_header_t));
	s->first_slice_segment_in_pic_flag = b.ReadU(1); 
	if (nal_unit_type >= HevcNaluTypeCodedSliceBLA && nal_unit_type <= HevcNaluTypeReserved23) 
		s->no_output_of_prior_pics_flag = b.ReadU(1); 
	s->slice_pic_parameter_set_id = b.ReadUE(); 
	if (!s->first_slice_segment_in_pic_flag) { 
		if (pps->dependent_slice_segments_enabled_flag) 
			s->dependent_slice_segment_flag = b.ReadU(1); 
		s->slice_segment_address = b.ReadUV(sps->hevc_global_variables.PicSizeInCtbsY); //u(v) 7.4.7.1
	} 
	if (!s->dependent_slice_segment_flag) { 
		for (i = 0; i < pps->num_extra_slice_header_bits; i++) 
			s->slice_reserved_flag[i] = b.ReadU(1); 
		s->slice_type = b.ReadUE(); 
		if (pps->output_flag_present_flag) 
			s->pic_output_flag = b.ReadU(1); 
		if (sps->separate_colour_plane_flag == 1) 
			s->colour_plane_id = b.ReadU(2); 
		if (nal_unit_type != HevcNaluTypeCodedSliceIDR && nal_unit_type != HevcNaluTypeCodedSliceIDRNLP) { 
			s->slice_pic_order_cnt_lsb = b.ReadU(sps->log2_max_pic_order_cnt_lsb_minus4 + 4); //u(v) 7.4.7.1
			s->short_term_ref_pic_set_sps_flag = b.ReadU(1); 
			if (!s->short_term_ref_pic_set_sps_flag) 
				parse_short_term_ref_pic_set(&s->short_term_ref_pic_set, b, sps->num_short_term_ref_pic_sets, sps);
			else if (sps->num_short_term_ref_pic_sets > 1) 
				s->short_term_ref_pic_set_idx = b.ReadUV(sps->num_short_term_ref_pic_sets); //u(v) 7.4.7.1
			if (sps->long_term_ref_pics_present_flag) { 
				if (sps->num_long_term_ref_pics_sps > 0) 
					s->num_long_term_sps = b.ReadUE(); 
				s->num_long_term_pics = b.ReadUE(); 
				for (i = 0; i < s->num_long_term_sps + s->num_long_term_pics; i++) { 
					if (i < s->num_long_term_sps) { 
						if (sps->num_long_term_ref_pics_sps > 1) 
							s->lt_idx_sps[i] = b.ReadUV(sps->num_long_term_ref_pics_sps); //u(v) 7.4.7.1
					} else { 
						s->poc_lsb_lt[i] = b.ReadU(sps->log2_max_pic_order_cnt_lsb_minus4 + 4); //u(v) 7.4.7.1
						s->used_by_curr_pic_lt_flag[i] = b.ReadU(1); 
					} 
					s->delta_poc_msb_present_flag[i] = b.ReadU(1); 
					if (s->delta_poc_msb_present_flag[i]) 
						s->delta_poc_msb_cycle_lt[i] = b.ReadUE(); 
				} 
			} 
			if (sps->sps_temporal_mvp_enabled_flag) 
				s->slice_temporal_mvp_enabled_flag = b.ReadU(1); 
		}
		if (sps->sample_adaptive_offset_enabled_flag) { 
			s->slice_sao_luma_flag = b.ReadU(1); 
			s->slice_sao_chroma_flag = b.ReadU(1); 
		} 
		if (s->slice_type == HEVC_SLICE_TYPE_P || s->slice_type == HEVC_SLICE_TYPE_B) { 
			s->num_ref_idx_active_override_flag = b.ReadU(1); 
			if (s->num_ref_idx_active_override_flag) { 
				s->num_ref_idx_l0_active_minus1 = b.ReadUE(); 
				if (s->slice_type == HEVC_SLICE_TYPE_B) 
					s->num_ref_idx_l1_active_minus1 = b.ReadUE(); 
			} 
			if (pps->lists_modification_present_flag && derive_NumPocTotalCurr(sps, s) > 1) 
				parse_ref_pic_lists_modification(&s->ref_pic_lists_modification, b, sps, s); 
			if (s->slice_type == HEVC_SLICE_TYPE_B) 
				s->mvd_l1_zero_flag = b.ReadU(1); 
			if (pps->cabac_init_present_flag) 
				s->cabac_init_flag = b.ReadU(1); 
			if (s->slice_temporal_mvp_enabled_flag) { 
				if (s->slice_type == HEVC_SLICE_TYPE_B) 
					s->collocated_from_l0_flag = b.ReadU(1); 
				if ((s->collocated_from_l0_flag && s->num_ref_idx_l0_active_minus1 > 0) || 
						(!s->collocated_from_l0_flag && s->num_ref_idx_l1_active_minus1 > 0)) 
					s->collocated_ref_idx = b.ReadUE(); 
			} 
			if ((pps->weighted_pred_flag && s->slice_type == HEVC_SLICE_TYPE_P) || 
					(pps->weighted_bipred_flag && s->slice_type == HEVC_SLICE_TYPE_B)) 
				parse_pred_weight_table(&s->pred_weight_table, b, sps, s); 
			s->five_minus_max_num_merge_cand = b.ReadUE(); 
		} 
		s->slice_qp_delta = b.ReadSE(); 
		if (pps->pps_slice_chroma_qp_offsets_present_flag) { 
			s->slice_cb_qp_offset = b.ReadSE(); 
			s->slice_cr_qp_offset = b.ReadSE(); 
		} 
		if (pps->deblocking_filter_override_enabled_flag) 
			s->deblocking_filter_override_flag = b.ReadU(1); 
		if (s->deblocking_filter_override_flag) { 
			s->slice_deblocking_filter_disabled_flag = b.ReadU(1); 
			if (!s->slice_deblocking_filter_disabled_flag) { 
				s->slice_beta_offset_div2 = b.ReadSE(); 
				s->slice_tc_offset_div2 = b.ReadSE(); 
			} 
		} 
		if (pps->pps_loop_filter_across_slices_enabled_flag && (s->slice_sao_luma_flag || 
				s->slice_sao_chroma_flag || !s->slice_deblocking_filter_disabled_flag)) 
			s->slice_loop_filter_across_slices_enabled_flag = b.ReadU(1); 
	} 
	if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag) { 
		s->num_entry_point_offsets = b.ReadUE();
		if (s->num_entry_point_offsets > 0) { 
			s->offset_len_minus1 = b.ReadUE(); 
			for (i = 0; i < s->num_entry_point_offsets; i++) 
				s->entry_point_offset_minus1[i] = b.ReadU(s->offset_len_minus1 + 1); //u(v) 
		} 
	} 
	if (pps->slice_segment_header_extension_present_flag) { 
		s->slice_segment_header_extension_length = b.ReadUE(); 
		for(i = 0; i < s->slice_segment_header_extension_length; i++) 
			s->slice_segment_header_extension_data_byte[i] = b.ReadU(8); 
	} 
	read_hevc_rbsp_trailing_bits(b);
}

std::string hevc_slice_type_string(int type)
{
	static const char* hevc_slice_type_strings[3] = { "B", "P", "I" };
	if (type >= 0 && type < 3)
		return hevc_slice_type_strings[type];
	return std::string("Unknown_") + std::to_string(type);
}

Json::Value hevc_slice_segment_header_to_json(hevc_slice_header_t* s, int nal_unit_type, hevc_sps_t* sps, hevc_pps_t* pps)
{
	int i;
	Json::Value json_slice_header;
	json_slice_header["first_slice_segment_in_pic_flag"] = s->first_slice_segment_in_pic_flag;
	if (nal_unit_type >= HevcNaluTypeCodedSliceBLA && nal_unit_type <= HevcNaluTypeReserved23) 
		json_slice_header["no_output_of_prior_pics_flag"] = s->no_output_of_prior_pics_flag;
	json_slice_header["slice_pic_parameter_set_id"] = s->slice_pic_parameter_set_id;
	if (!s->first_slice_segment_in_pic_flag) { 
		if (pps->dependent_slice_segments_enabled_flag) 
			json_slice_header["dependent_slice_segment_flag"] = s->dependent_slice_segment_flag;
		json_slice_header["slice_segment_address"] = s->slice_segment_address;
	} 
	if (!s->dependent_slice_segment_flag) {
		json_slice_header["slice_type"] = hevc_slice_type_string(s->slice_type);
		if (pps->output_flag_present_flag) 
			json_slice_header["pic_output_flag"] = s->pic_output_flag;
		if (sps->separate_colour_plane_flag == 1) 
			json_slice_header["colour_plane_id"] = s->colour_plane_id;
		if (nal_unit_type != HevcNaluTypeCodedSliceIDR && nal_unit_type != HevcNaluTypeCodedSliceIDRNLP) { 
			json_slice_header["slice_pic_order_cnt_lsb"] = s->slice_pic_order_cnt_lsb;
			json_slice_header["short_term_ref_pic_set_sps_flag"] = s->short_term_ref_pic_set_sps_flag;
			if (!s->short_term_ref_pic_set_sps_flag) 
				{}//parse_short_term_ref_pic_set(&s->short_term_ref_pic_set, b, sps->num_short_term_ref_pic_sets, sps);
			else if (sps->num_short_term_ref_pic_sets > 1) 
				json_slice_header["short_term_ref_pic_set_idx"] = s->short_term_ref_pic_set_idx;
			if (sps->long_term_ref_pics_present_flag) { 
				if (sps->num_long_term_ref_pics_sps > 0) 
					json_slice_header["num_long_term_sps"] = s->num_long_term_sps;
				json_slice_header["num_long_term_pics"] = s->num_long_term_pics;
			} 
			if (sps->sps_temporal_mvp_enabled_flag) 
				json_slice_header["slice_temporal_mvp_enabled_flag"] = s->slice_temporal_mvp_enabled_flag;
		}
		if (sps->sample_adaptive_offset_enabled_flag) { 
			json_slice_header["slice_sao_luma_flag"] = s->slice_sao_luma_flag;
			json_slice_header["slice_sao_chroma_flag"] = s->slice_sao_chroma_flag;
		} 
		if (s->slice_type == HEVC_SLICE_TYPE_P || s->slice_type == HEVC_SLICE_TYPE_B) { 
			json_slice_header["num_ref_idx_active_override_flag"] = s->num_ref_idx_active_override_flag;
			if (s->num_ref_idx_active_override_flag) { 
				json_slice_header["num_ref_idx_l0_active_minus1"] = s->num_ref_idx_l0_active_minus1;
				if (s->slice_type == HEVC_SLICE_TYPE_B) 
					json_slice_header["num_ref_idx_l1_active_minus1"] = s->num_ref_idx_l1_active_minus1;
			} 
			// if (pps->lists_modification_present_flag && derive_NumPocTotalCurr(sps, s) > 1) 
			// 	parse_ref_pic_lists_modification(&s->ref_pic_lists_modification, b, sps, s); 
			if (s->slice_type == HEVC_SLICE_TYPE_B) 
				json_slice_header["mvd_l1_zero_flag"] = s->mvd_l1_zero_flag;
			if (pps->cabac_init_present_flag) 
				json_slice_header["cabac_init_flag"] = s->cabac_init_flag;
			if (s->slice_temporal_mvp_enabled_flag) { 
				if (s->slice_type == HEVC_SLICE_TYPE_B) 
					json_slice_header["collocated_from_l0_flag"] = s->collocated_from_l0_flag;
				if ((s->collocated_from_l0_flag && s->num_ref_idx_l0_active_minus1 > 0) || 
						(!s->collocated_from_l0_flag && s->num_ref_idx_l1_active_minus1 > 0)) 
					json_slice_header["collocated_ref_idx"] = s->collocated_ref_idx;
			} 
			// if ((pps->weighted_pred_flag && s->slice_type == HEVC_SLICE_TYPE_P) || 
			// 		(pps->weighted_bipred_flag && s->slice_type == HEVC_SLICE_TYPE_B)) 
			// 	parse_pred_weight_table(&s->pred_weight_table, b, sps, s); 
			json_slice_header["five_minus_max_num_merge_cand"] = s->five_minus_max_num_merge_cand;
		} 
		json_slice_header["slice_qp_delta"] = s->slice_qp_delta;
		if (pps->pps_slice_chroma_qp_offsets_present_flag) { 
			json_slice_header["slice_cb_qp_offset"] = s->slice_cb_qp_offset;
			json_slice_header["slice_cr_qp_offset"] = s->slice_cr_qp_offset;
		} 
		if (pps->deblocking_filter_override_enabled_flag) 
			json_slice_header["deblocking_filter_override_flag"] = s->deblocking_filter_override_flag;
		if (s->deblocking_filter_override_flag) { 
			json_slice_header["slice_deblocking_filter_disabled_flag"] = s->slice_deblocking_filter_disabled_flag;
			if (!s->slice_deblocking_filter_disabled_flag) { 
				json_slice_header["slice_beta_offset_div2"] = s->slice_beta_offset_div2;
				json_slice_header["slice_tc_offset_div2"] = s->slice_tc_offset_div2;
			} 
		} 
		if (pps->pps_loop_filter_across_slices_enabled_flag && (s->slice_sao_luma_flag || 
				s->slice_sao_chroma_flag || !s->slice_deblocking_filter_disabled_flag)) 
			json_slice_header["slice_loop_filter_across_slices_enabled_flag"] = s->slice_loop_filter_across_slices_enabled_flag;
	} 
	if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag) { 
		json_slice_header["num_entry_point_offsets"] = s->num_entry_point_offsets;
		if (s->num_entry_point_offsets > 0) { 
			json_slice_header["offset_len_minus1"] = s->offset_len_minus1;
			// for (i = 0; i < s->num_entry_point_offsets; i++) 
			// 	s->entry_point_offset_minus1[i] = b.ReadU(s->offset_len_minus1 + 1); //u(v) 
		} 
	} 
	if (pps->slice_segment_header_extension_present_flag) { 
		json_slice_header["slice_segment_header_extension_length"] = s->slice_segment_header_extension_length;
		// for(i = 0; i < s->slice_segment_header_extension_length; i++) 
		// 	s->slice_segment_header_extension_data_byte[i] = b.ReadU(8); 
	} 

	return json_slice_header;
}
