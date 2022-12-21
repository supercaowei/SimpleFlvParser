#ifndef SFP_HEVC_SYNTAX_H
#define SFP_HEVC_SYNTAX_H

#include "bytes.h"
#include <string>
#include "json/value.h"

#define HEVC_MAX_ARRAY_SIZE  64
#define HEVC_MAX_ARRAY_SIZE_LARGE 256

//Table 7-7 – Name association to slice_type
#define HEVC_SLICE_TYPE_B 0
#define HEVC_SLICE_TYPE_P 1
#define HEVC_SLICE_TYPE_I 2

//see Table 7-1 – NAL unit type codes and NAL unit type classes
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
	HevcNaluTypeCodedSliceBLANT, 		// 17 // Current name in the spec: BLA_W_DLP or BLA_W_RADL
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
	HevcNaluTypeFillerData,				// 38
	HevcNaluTypeSEI,					// 39 Prefix SEI
	HevcNaluTypeSEISuffix,				// 40 Suffix SEI
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

//7.3.4 Scaling list data syntax
typedef struct scaling_list_data_t
{
	int scaling_list_pred_mode_flag[4][6];
	int scaling_list_pred_matrix_id_delta[4][6];
	int scaling_list_dc_coef_minus8[2][6];
	int scaling_list_delta_coef[64];
} scaling_list_data_t;

//7.3.3 Profile, tier and level syntax
typedef struct profile_tier_level_t
{
	int maxNumSubLayersMinus1;
	int general_profile_space;
	int general_tier_flag;
	int general_profile_idc;
	int general_profile_compatibility_flag[32];
	int general_progressive_source_flag;
	int general_interlaced_source_flag;
	int general_non_packed_constraint_flag;
	int general_frame_only_constraint_flag;
	//44bits general_reserved_zero_44bits;

	int general_level_idc;
	int sub_layer_profile_present_flag[HEVC_MAX_ARRAY_SIZE];
	int sub_layer_level_present_flag[HEVC_MAX_ARRAY_SIZE];
	//for(for(int reserved_zero_2bits));

	//for(i = 0; i < maxNumSubLayersMinus1; i++)
	//if(sub_layer_profile_present_flag[i])
	int sub_layer_profile_space[HEVC_MAX_ARRAY_SIZE];
	int sub_layer_tier_flag[HEVC_MAX_ARRAY_SIZE];
	int sub_layer_profile_idc[HEVC_MAX_ARRAY_SIZE];
	int sub_layer_profile_compatibility_flag[HEVC_MAX_ARRAY_SIZE][32];
	int sub_layer_progressive_source_flag[HEVC_MAX_ARRAY_SIZE];
	int sub_layer_interlaced_source_flag[HEVC_MAX_ARRAY_SIZE];
	int sub_layer_non_packed_constraint_flag[HEVC_MAX_ARRAY_SIZE];
	int sub_layer_frame_only_constraint_flag[HEVC_MAX_ARRAY_SIZE];
	//if(sub_layer_level_present_flag[i])
	int sub_layer_level_idc[HEVC_MAX_ARRAY_SIZE];
} profile_tier_level_t;

//E.2.3 Sub-layer HRD parameters syntax
typedef struct sub_layer_hrd_parameters_t {
	int subLayerId;
	int CpbCnt;
	int sub_pic_hrd_params_present_flag;
	int bit_rate_value_minus1[HEVC_MAX_ARRAY_SIZE];
	int cpb_size_value_minus1[HEVC_MAX_ARRAY_SIZE];
	int cpb_size_du_value_minus1[HEVC_MAX_ARRAY_SIZE];
	int bit_rate_du_value_minus1[HEVC_MAX_ARRAY_SIZE];
	int cbr_flag[HEVC_MAX_ARRAY_SIZE];
} sub_layer_hrd_parameters_t;

//E.2.2 HRD parameters syntax
typedef struct hrd_parameters_t {
	int commonInfPresentFlag;
	int maxNumSubLayersMinus1;
	//if(commonInfPresentFlag)
	int nal_hrd_parameters_present_flag;
	int vcl_hrd_parameters_present_flag;
	//if(nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag)
	int sub_pic_hrd_params_present_flag;
	//if(sub_pic_hrd_params_present_flag)
	int tick_divisor_minus2;
	int du_cpb_removal_delay_increment_length_minus1;
	int sub_pic_cpb_params_in_pic_timing_sei_flag;
	int dpb_output_delay_du_length_minus1;

	int bit_rate_scale;
	int cpb_size_scale;
	//if(sub_pic_hrd_params_present_flag)
	int cpb_size_du_scale;
	int initial_cpb_removal_delay_length_minus1;
	int au_cpb_removal_delay_length_minus1;
	int dpb_output_delay_length_minus1;

	//for(i = 0; i <= maxNumSubLayersMinus1; i++)
	int fixed_pic_rate_general_flag[HEVC_MAX_ARRAY_SIZE];
	//if(!fixed_pic_rate_general_flag[i])
	int fixed_pic_rate_within_cvs_flag[HEVC_MAX_ARRAY_SIZE];
	//if(fixed_pic_rate_within_cvs_flag[i]) 
	int elemental_duration_in_tc_minus1[HEVC_MAX_ARRAY_SIZE];
	int low_delay_hrd_flag[HEVC_MAX_ARRAY_SIZE];
	//if(!low_delay_hrd_flag[i]) 
	int cpb_cnt_minus1[HEVC_MAX_ARRAY_SIZE];
	//if(nal_hrd_parameters_present_flag)
	struct sub_layer_hrd_parameters_t nal_hrd[HEVC_MAX_ARRAY_SIZE], vcl_hrd[HEVC_MAX_ARRAY_SIZE];
} hrd_parameters_t;

typedef struct hevc_global_variables_t {
	int NumNegativePics[HEVC_MAX_ARRAY_SIZE];
	int NumPositivePics[HEVC_MAX_ARRAY_SIZE];
	int NumDeltaPocs[HEVC_MAX_ARRAY_SIZE];
	int DeltaPocS0[HEVC_MAX_ARRAY_SIZE][HEVC_MAX_ARRAY_SIZE];
	int DeltaPocS1[HEVC_MAX_ARRAY_SIZE][HEVC_MAX_ARRAY_SIZE]; 
	int UsedByCurrPicS0[HEVC_MAX_ARRAY_SIZE][HEVC_MAX_ARRAY_SIZE];
	int UsedByCurrPicS1[HEVC_MAX_ARRAY_SIZE][HEVC_MAX_ARRAY_SIZE];

	int MinCbLog2SizeY;
	int CtbLog2SizeY;
	int MinCbSizeY;
	int CtbSizeY;
	int PicWidthInMinCbsY;
	int PicWidthInCtbsY;
	int PicHeightInMinCbsY;
	int PicHeightInCtbsY;
	int PicSizeInMinCbsY;
	int PicSizeInCtbsY;
	int PicSizeInSamplesY;
	int PicWidthInSamplesC;
	int PicHeightInSamplesC;
} hevc_global_variables_t;

//7.3.7 Short-term reference picture set syntax
typedef struct short_term_ref_pic_set_t {
	int stRpsIdx;
	int RefRpsIdx;
	int inter_ref_pic_set_prediction_flag;
	int delta_idx_minus1;
	int delta_rps_sign;
	int abs_delta_rps_minus1;
	int used_by_curr_pic_flag[HEVC_MAX_ARRAY_SIZE];
	int use_delta_flag[HEVC_MAX_ARRAY_SIZE];

	int num_negative_pics;
	int num_positive_pics;
	int delta_poc_s0_minus1[HEVC_MAX_ARRAY_SIZE];
	int used_by_curr_pic_s0_flag[HEVC_MAX_ARRAY_SIZE];
	int delta_poc_s1_minus1[HEVC_MAX_ARRAY_SIZE];
	int used_by_curr_pic_s1_flag[HEVC_MAX_ARRAY_SIZE];
} short_term_ref_pic_set_t;

//7.3.6.2 Reference picture list modification syntax
typedef struct ref_pic_lists_modification_t {
	int ref_pic_list_modification_flag_l0;
	int list_entry_l0[HEVC_MAX_ARRAY_SIZE];
	int ref_pic_list_modification_flag_l1;
	int list_entry_l1[HEVC_MAX_ARRAY_SIZE];
} ref_pic_lists_modification_t;

//7.3.6.3 Weighted prediction parameters syntax
typedef struct pred_weight_table_t {
	int luma_log2_weight_denom;
	int delta_chroma_log2_weight_denom;
	int luma_weight_l0_flag[HEVC_MAX_ARRAY_SIZE];
	int chroma_weight_l0_flag[HEVC_MAX_ARRAY_SIZE];
	int delta_luma_weight_l0[HEVC_MAX_ARRAY_SIZE];
	int luma_offset_l0[HEVC_MAX_ARRAY_SIZE];
	int delta_chroma_weight_l0[HEVC_MAX_ARRAY_SIZE][2];
	int delta_chroma_offset_l0[HEVC_MAX_ARRAY_SIZE][2];
	int luma_weight_l1_flag[HEVC_MAX_ARRAY_SIZE];
	int chroma_weight_l1_flag[HEVC_MAX_ARRAY_SIZE];
	int delta_luma_weight_l1[HEVC_MAX_ARRAY_SIZE];
	int luma_offset_l1[HEVC_MAX_ARRAY_SIZE];
	int delta_chroma_weight_l1[HEVC_MAX_ARRAY_SIZE][2];
	int delta_chroma_offset_l1[HEVC_MAX_ARRAY_SIZE][2];
} pred_weight_table_t;

//7.3.2.1 Video parameter set RBSP syntax
typedef struct hevc_vps_t 
{
	int vps_video_parameter_set_id;
	int vps_reserved_three_2bits;
	int vps_max_layers_minus1;
	int vps_max_sub_layers_minus1;
	int vps_temporal_id_nesting_flag;
	int vps_reserved_0xffff_16bits;

	struct profile_tier_level_t profile_tier_level;

	int vps_sub_layer_ordering_info_present_flag;
	//for(i = (vps_sub_layer_ordering_info_present_flag ? 0 : vps_max_sub_layers_minus1); 
	//	i <= vps_max_sub_layers_minus1; i++)
	int vps_max_dec_pic_buffering_minus1[HEVC_MAX_ARRAY_SIZE];
	int vps_max_num_reorder_pics[HEVC_MAX_ARRAY_SIZE];
	int vps_max_latency_increase_plus1[HEVC_MAX_ARRAY_SIZE];

	int vps_max_layer_id;
	int vps_num_layer_sets_minus1;
	//for(i = 1; i <= vps_num_layer_sets_minus1; i++) 
	//	for(j = 0; j <= vps_max_layer_id; j++) 
	int layer_id_included_flag[HEVC_MAX_ARRAY_SIZE_LARGE * HEVC_MAX_ARRAY_SIZE][4];

	int vps_timing_info_present_flag;
	//if(vps_timing_info_present_flag)
	int vps_num_units_in_tick;
	int vps_time_scale;
	int vps_poc_proportional_to_timing_flag;
	//if(vps_poc_proportional_to_timing_flag)
	int vps_num_ticks_poc_diff_one_minus1;
	int vps_num_hrd_parameters;
	//for(i = 0; i < vps_num_hrd_parameters; i++)
	int hrd_layer_set_idx[HEVC_MAX_ARRAY_SIZE];
	int cprms_present_flag[HEVC_MAX_ARRAY_SIZE];
	struct hrd_parameters_t hrd[HEVC_MAX_ARRAY_SIZE];
} hevc_vps_t;

//E.2.1 VUI parameters syntax
typedef struct vui_parameters_t 
{
	int aspect_ratio_info_present_flag;
	//if(aspect_ratio_info_present_flag)
	int aspect_ratio_idc;
	//if(aspect_ratio_idc == EXTENDED_SAR)
	int sar_width;
	int sar_height;

	int overscan_info_present_flag;
	//if(overscan_info_present_flag)
	int overscan_appropriate_flag;
	int video_signal_type_present_flag;
	//if(video_signal_type_present_flag)
	int video_format;
	int video_full_range_flag;
	int colour_description_present_flag;
	//if(colour_description_present_flag)
	int colour_primaries;
	int transfer_characteristics;
	int matrix_coeffs;

	int chroma_loc_info_present_flag;
	//if(chroma_loc_info_present_flag)
	int chroma_sample_loc_type_top_field;
	int chroma_sample_loc_type_bottom_field;

	int neutral_chroma_indication_flag;
	int field_seq_flag;
	int frame_field_info_present_flag;
	int default_display_window_flag;
	//if(default_display_window_flag)
	int def_disp_win_left_offset;
	int def_disp_win_right_offset;
	int def_disp_win_top_offset;
	int def_disp_win_bottom_offset;

	int vui_timing_info_present_flag;
	//if(vui_timing_info_present_flag)
	int vui_num_units_in_tick;
	int vui_time_scale;
	int vui_poc_proportional_to_timing_flag;
	//if(vui_poc_proportional_to_timing_flag)
	int vui_num_ticks_poc_diff_one_minus1;
	int vui_hrd_parameters_present_flag;
	//if(vui_hrd_parameters_present_flag)
	struct hrd_parameters_t hrd;

	int bitstream_restriction_flag;
	//if(bitstream_restriction_flag)
	int tiles_fixed_structure_flag;
	int motion_vectors_over_pic_boundaries_flag;
	int restricted_ref_pic_lists_flag;
	int min_spatial_segmentation_idc;
	int max_bytes_per_pic_denom;
	int max_bits_per_min_cu_denom;
	int log2_max_mv_length_horizontal;
	int log2_max_mv_length_vertical;
} vui_parameters_t;

//7.3.2.2 Sequence parameter set RBSP syntax
typedef struct hevc_sps_t 
{
	int sps_video_parameter_set_id;
	int sps_max_sub_layers_minus1;
	int sps_temporal_id_nesting_flag;

	struct profile_tier_level_t profile_tier_level;

	int sps_seq_parameter_set_id;
	int chroma_format_idc;
	int separate_colour_plane_flag;
	int pic_width_in_luma_samples;
	int pic_height_in_luma_samples;
	int conformance_window_flag;
	//if(conformance_window_flag)
	int conf_win_left_offset;
	int conf_win_right_offset;
	int conf_win_top_offset;
	int conf_win_bottom_offset;
 
	int bit_depth_luma_minus8;
	int bit_depth_chroma_minus8;
	int log2_max_pic_order_cnt_lsb_minus4;

	int sps_sub_layer_ordering_info_present_flag;
	//for(i = (sps_sub_layer_ordering_info_present_flag ? 0 : sps_max_sub_layers_minus1); 
	//	i <= sps_max_sub_layers_minus1; i++)
	int sps_max_dec_pic_buffering_minus1[HEVC_MAX_ARRAY_SIZE];
	int sps_max_num_reorder_pics[HEVC_MAX_ARRAY_SIZE];
	int sps_max_latency_increase_plus1[HEVC_MAX_ARRAY_SIZE];

	int log2_min_luma_coding_block_size_minus3;
	int log2_diff_max_min_luma_coding_block_size;
	int log2_min_transform_block_size_minus2;
	int log2_diff_max_min_transform_block_size;
	int max_transform_hierarchy_depth_inter;
	int max_transform_hierarchy_depth_intra;
	int scaling_list_enabled_flag;
	//if(scaling_list_enabled_flag)
	int sps_scaling_list_data_present_flag;
	//if(sps_scaling_list_data_present_flag)
	struct scaling_list_data_t scaling_list_data;
 
	int amp_enabled_flag;
	int sample_adaptive_offset_enabled_flag;
	int pcm_enabled_flag;
	//if(pcm_enabled_flag)
	int pcm_sample_bit_depth_luma_minus1;
	int pcm_sample_bit_depth_chroma_minus1;
	int log2_min_pcm_luma_coding_block_size_minus3;
	int log2_diff_max_min_pcm_luma_coding_block_size;
	int pcm_loop_filter_disabled_flag;

	int num_short_term_ref_pic_sets;
	struct short_term_ref_pic_set_t short_term_ref_pic_set[HEVC_MAX_ARRAY_SIZE];
	struct hevc_global_variables_t hevc_global_variables; //我理解这是一个跟sps共生的结构

	int long_term_ref_pics_present_flag;
	//if(long_term_ref_pics_present_flag)
	int num_long_term_ref_pics_sps;
	int lt_ref_pic_poc_lsb_sps[HEVC_MAX_ARRAY_SIZE];
	int used_by_curr_pic_lt_sps_flag[HEVC_MAX_ARRAY_SIZE];

	int sps_temporal_mvp_enabled_flag;
	int strong_intra_smoothing_enabled_flag;
	int vui_parameters_present_flag;
	//if(vui_parameters_present_flag)
	struct vui_parameters_t vui;
} hevc_sps_t;

//7.3.2.3 Picture parameter set RBSP syntax
typedef struct hevc_pps_t
{
	int pps_pic_parameter_set_id;
	int pps_seq_parameter_set_id;
	int dependent_slice_segments_enabled_flag;
	int output_flag_present_flag;
	int num_extra_slice_header_bits;
	int sign_data_hiding_enabled_flag;
	int cabac_init_present_flag;
	int num_ref_idx_l0_default_active_minus1;
	int num_ref_idx_l1_default_active_minus1;
	int init_qp_minus26;
	int constrained_intra_pred_flag;
	int transform_skip_enabled_flag;
	int cu_qp_delta_enabled_flag;
	int diff_cu_qp_delta_depth;
	int pps_cb_qp_offset;
	int pps_cr_qp_offset;
	int pps_slice_chroma_qp_offsets_present_flag;
	int weighted_pred_flag;
	int weighted_bipred_flag;
	int transquant_bypass_enabled_flag;
	int tiles_enabled_flag;
	int entropy_coding_sync_enabled_flag;

	//if(tiles_enabled_flag)
	int num_tile_columns_minus1;
	int num_tile_rows_minus1;
	int uniform_spacing_flag;
	int column_width_minus1[HEVC_MAX_ARRAY_SIZE];
	int row_height_minus1[265];
	int loop_filter_across_tiles_enabled_flag;

	int pps_loop_filter_across_slices_enabled_flag;
	int deblocking_filter_control_present_flag;
	//if(deblocking_filter_control_present_flag)
	int deblocking_filter_override_enabled_flag;
	int pps_deblocking_filter_disabled_flag;
	int pps_beta_offset_div2;
	int pps_tc_offset_div2;

	int pps_scaling_list_data_present_flag;
	//if(pps_scaling_list_data_present_flag)
	struct scaling_list_data_t scaling_list_data;

	int lists_modification_present_flag;
	int log2_parallel_merge_level_minus2;
	int slice_segment_header_extension_present_flag;
} hevc_pps_t;

//see 7.3.2.9 Slice segment layer RBSP syntax
//and 7.3.6.1 General slice segment header syntax
typedef struct hevc_slice_header_t
{
	int belongs_nal_unit_type;
	int first_slice_segment_in_pic_flag;
	//if(nal_unit_type >= BLA_W_LP && nal_unit_type <= RSV_IRAP_VCL23 )
	int no_output_of_prior_pics_flag;
	int slice_pic_parameter_set_id;
	//if(!first_slice_segment_in_pic_flag )
	//if(dependent_slice_segments_enabled_flag )
	int dependent_slice_segment_flag;
	int slice_segment_address;

	//if(!dependent_slice_segment_flag )
	//for(i = 0; i < num_extra_slice_header_bits; i++)
	int slice_reserved_flag[HEVC_MAX_ARRAY_SIZE];
	int slice_type;
	//if(output_flag_present_flag )
	int pic_output_flag;
	//if(separate_colour_plane_flag = = 1 )
	int colour_plane_id;

	//if(nal_unit_type != IDR_W_RADL && nal_unit_type != IDR_N_LP )
	int slice_pic_order_cnt_lsb;
	int short_term_ref_pic_set_sps_flag;
	//if(!short_term_ref_pic_set_sps_flag )
	struct short_term_ref_pic_set_t short_term_ref_pic_set;

	//else if(num_short_term_ref_pic_sets > 1) 
	int short_term_ref_pic_set_idx;
	//if(long_term_ref_pics_present_flag )
	//if(num_long_term_ref_pics_sps > 0 )
	int num_long_term_sps;
	int num_long_term_pics;
	//for(i = 0; i < num_long_term_sps + num_long_term_pics; i++)
	//if(i < num_long_term_sps )
	//if(num_long_term_ref_pics_sps > 1 )
	int lt_idx_sps[HEVC_MAX_ARRAY_SIZE];
	int poc_lsb_lt[HEVC_MAX_ARRAY_SIZE];
	int used_by_curr_pic_lt_flag[HEVC_MAX_ARRAY_SIZE];
	int delta_poc_msb_present_flag[HEVC_MAX_ARRAY_SIZE];
	//if(delta_poc_msb_present_flag[ i ] )
	int delta_poc_msb_cycle_lt[HEVC_MAX_ARRAY_SIZE];
	//if(sps_temporal_mvp_enabled_flag )
	int slice_temporal_mvp_enabled_flag;

	//if(sample_adaptive_offset_enabled_flag )
	int slice_sao_luma_flag;
	int slice_sao_chroma_flag;

	//if(slice_type = = P | | slice_type = = B )
	int num_ref_idx_active_override_flag;
	//if(num_ref_idx_active_override_flag )
	int num_ref_idx_l0_active_minus1;
	//if(slice_type = = B )
	int num_ref_idx_l1_active_minus1;

	//if(lists_modification_present_flag && NumPocTotalCurr > 1 )
	struct ref_pic_lists_modification_t ref_pic_lists_modification;

	//if(slice_type = = B )
	int mvd_l1_zero_flag;
	//if(cabac_init_present_flag )
	int cabac_init_flag;
	//if(slice_temporal_mvp_enabled_flag )
	//if(slice_type = = B )
	int collocated_from_l0_flag;
	int collocated_ref_idx;

	struct pred_weight_table_t pred_weight_table;
	int five_minus_max_num_merge_cand;

	int slice_qp_delta;
	//if(pps_slice_chroma_qp_offsets_present_flag)
	int slice_cb_qp_offset;
	int slice_cr_qp_offset;

	//if(deblocking_filter_override_enabled_flag )
	int deblocking_filter_override_flag;
	//if(deblocking_filter_override_flag )
	int slice_deblocking_filter_disabled_flag;
	//if(!slice_deblocking_filter_disabled_flag )
	int slice_beta_offset_div2;
	int slice_tc_offset_div2;
	int slice_loop_filter_across_slices_enabled_flag;

	//if(tiles_enabled_flag | | entropy_coding_sync_enabled_flag )
	int num_entry_point_offsets;
	//if(num_entry_point_offsets > 0 )
	int offset_len_minus1;
	//for(i = 0; i < num_entry_point_offsets; i++)
	int entry_point_offset_minus1[HEVC_MAX_ARRAY_SIZE];

	//if(slice_segment_header_extension_present_flag )
	int slice_segment_header_extension_length;
	//for(i = 0; i < slice_segment_header_extension_length; i++)
	int slice_segment_header_extension_data_byte[HEVC_MAX_ARRAY_SIZE];
} hevc_slice_header_t;

typedef struct hevc_sei_t
{
	int payloadType;
	int payloadSize;
	uint8_t* payload;
} hevc_sei_t;

class BitReader;

int  hevc_nal_to_rbsp(const uint8_t* nal_buf, int* nal_size, uint8_t* rbsp_buf, int* rbsp_size);

void read_hevc_video_parameter_set_rbsp(hevc_vps_t* vps, BitReader& b);

Json::Value hevc_vps_to_json(hevc_vps_t* vps);

void read_hevc_seq_parameter_set_rbsp(hevc_sps_t* sps, BitReader& b);

Json::Value hevc_sps_to_json(hevc_sps_t* sps);

void read_hevc_pic_parameter_set_rbsp(hevc_pps_t* pps, BitReader& b);

Json::Value hevc_pps_to_json(hevc_pps_t* pps);

void hevc_slice_segment_header(hevc_slice_header_t* sh, BitReader& b, uint8_t nal_unit_type, hevc_sps_t* sps, hevc_pps_t* pps);

Json::Value hevc_slice_segment_header_to_json(hevc_slice_header_t* sh, int nal_unit_type, hevc_sps_t* sps, hevc_pps_t* pps);

std::string hevc_slice_type_string(int type);

hevc_sei_t** read_hevc_sei_rbsp(uint32_t *num_seis, BitReader& b, int nal_unit_type);

void release_hevc_seis(hevc_sei_t** seis, uint32_t num_seis);

Json::Value hevc_seis_to_json(hevc_sei_t** seis, uint32_t num_seis);

#endif
