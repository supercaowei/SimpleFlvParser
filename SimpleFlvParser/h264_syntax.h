#ifndef SFP_H264_SYNTAX_H
#define SFP_H264_SYNTAX_H

#include "bytes.h"
#include <string>

//7.4.3 Table 7-6. Name association to slice_type
#define SLICE_TYPE_P        0        // P (P slice)
#define SLICE_TYPE_B        1        // B (B slice)
#define SLICE_TYPE_I        2        // I (I slice)
#define SLICE_TYPE_SP       3        // SP (SP slice)
#define SLICE_TYPE_SI       4        // SI (SI slice)
//as per footnote to Table 7-6, the *_ONLY slice types indicate that all other slices in that picture are of the same type
#define SLICE_TYPE_P_ONLY    5        // P (P slice)
#define SLICE_TYPE_B_ONLY    6        // B (B slice)
#define SLICE_TYPE_I_ONLY    7        // I (I slice)
#define SLICE_TYPE_SP_ONLY   8        // SP (SP slice)
#define SLICE_TYPE_SI_ONLY   9        // SI (SI slice)

/**
Sequence Parameter Set
@see 7.3.2.1 Sequence parameter set RBSP syntax
@see read_seq_parameter_set_rbsp
@see write_seq_parameter_set_rbsp
@see debug_sps
*/
typedef struct 
{
	uint8_t profile_idc;
	uint8_t constraint_set0_flag;
	uint8_t constraint_set1_flag;
	uint8_t constraint_set2_flag;
	uint8_t constraint_set3_flag;
	uint8_t constraint_set4_flag;
	uint8_t constraint_set5_flag;
	uint8_t reserved_zero_2bits;
	uint8_t level_idc;
	uint8_t seq_parameter_set_id;
	uint8_t chroma_format_idc;
	uint8_t residual_colour_transform_flag;
	uint8_t bit_depth_luma_minus8;
	uint8_t bit_depth_chroma_minus8;
	uint8_t qpprime_y_zero_transform_bypass_flag;
	uint8_t seq_scaling_matrix_present_flag;
	uint8_t seq_scaling_list_present_flag[8];
	uint8_t* ScalingList4x4[6];
	uint8_t UseDefaultScalingMatrix4x4Flag[6];
	uint8_t* ScalingList8x8[2];
	uint8_t UseDefaultScalingMatrix8x8Flag[2];
	uint8_t log2_max_frame_num_minus4;
	uint8_t pic_order_cnt_type;
	uint8_t log2_max_pic_order_cnt_lsb_minus4;
	uint8_t delta_pic_order_always_zero_flag;
	uint8_t offset_for_non_ref_pic;
	uint8_t offset_for_top_to_bottom_field;
	uint8_t num_ref_frames_in_pic_order_cnt_cycle;
	uint8_t offset_for_ref_frame[256];
	uint8_t num_ref_frames;
	uint8_t gaps_in_frame_num_value_allowed_flag;
	uint8_t pic_width_in_mbs_minus1;
	uint8_t pic_height_in_map_units_minus1;
	uint8_t frame_mbs_only_flag;
	uint8_t mb_adaptive_frame_field_flag;
	uint8_t direct_8x8_inference_flag;
	uint8_t frame_cropping_flag;
	uint8_t frame_crop_left_offset;
	uint8_t frame_crop_right_offset;
	uint8_t frame_crop_top_offset;
	uint8_t frame_crop_bottom_offset;
	uint8_t vui_parameters_present_flag;

	struct
	{
		uint8_t aspect_ratio_info_present_flag;
		uint8_t aspect_ratio_idc;
		uint8_t sar_width;
		uint8_t sar_height;
		uint8_t overscan_info_present_flag;
		uint8_t overscan_appropriate_flag;
		uint8_t video_signal_type_present_flag;
		uint8_t video_format;
		uint8_t video_full_range_flag;
		uint8_t colour_description_present_flag;
		uint8_t colour_primaries;
		uint8_t transfer_characteristics;
		uint8_t matrix_coefficients;
		uint8_t chroma_loc_info_present_flag;
		uint8_t chroma_sample_loc_type_top_field;
		uint8_t chroma_sample_loc_type_bottom_field;
		uint8_t timing_info_present_flag;
		uint8_t num_units_in_tick;
		uint8_t time_scale;
		uint8_t fixed_frame_rate_flag;
		uint8_t nal_hrd_parameters_present_flag;
		uint8_t vcl_hrd_parameters_present_flag;
		uint8_t low_delay_hrd_flag;
		uint8_t pic_struct_present_flag;
		uint8_t bitstream_restriction_flag;
		uint8_t motion_vectors_over_pic_boundaries_flag;
		uint8_t max_bytes_per_pic_denom;
		uint8_t max_bits_per_mb_denom;
		uint8_t log2_max_mv_length_horizontal;
		uint8_t log2_max_mv_length_vertical;
		uint8_t num_reorder_frames;
		uint8_t max_dec_frame_buffering;
	} vui;

	struct
	{
		uint8_t cpb_cnt_minus1;
		uint8_t bit_rate_scale;
		uint8_t cpb_size_scale;
		uint8_t bit_rate_value_minus1[32]; // up to cpb_cnt_minus1, which is <= 31
		uint8_t cpb_size_value_minus1[32];
		uint8_t cbr_flag[32];
		uint8_t initial_cpb_removal_delay_length_minus1;
		uint8_t cpb_removal_delay_length_minus1;
		uint8_t dpb_output_delay_length_minus1;
		uint8_t time_offset_length;
	} hrd;
} sps_t;

/**
Picture Parameter Set
@see 7.3.2.2 Picture parameter set RBSP syntax
@see read_pic_parameter_set_rbsp
@see write_pic_parameter_set_rbsp
@see debug_pps
*/
typedef struct
{
	uint8_t pic_parameter_set_id;
	uint8_t seq_parameter_set_id;
	uint8_t entropy_coding_mode_flag;
	uint8_t pic_order_present_flag;
	uint8_t num_slice_groups_minus1;
	uint8_t slice_group_map_type;
	uint8_t run_length_minus1[8]; // up to num_slice_groups_minus1, which is <= 7 in Baseline and Extended, 0 otheriwse
	uint8_t top_left[8];
	uint8_t bottom_right[8];
	uint8_t slice_group_change_direction_flag;
	uint8_t slice_group_change_rate_minus1;
	uint8_t pic_size_in_map_units_minus1;
	uint8_t slice_group_id[256]; // FIXME what size?
	uint8_t num_ref_idx_l0_active_minus1;
	uint8_t num_ref_idx_l1_active_minus1;
	uint8_t weighted_pred_flag;
	uint8_t weighted_bipred_idc;
	uint8_t pic_init_qp_minus26;
	uint8_t pic_init_qs_minus26;
	uint8_t chroma_qp_index_offset;
	uint8_t deblocking_filter_control_present_flag;
	uint8_t constrained_intra_pred_flag;
	uint8_t redundant_pic_cnt_present_flag;

	// set iff we carry any of the optional headers
	uint8_t _more_rbsp_data_present;

	uint8_t transform_8x8_mode_flag;
	uint8_t pic_scaling_matrix_present_flag;
	uint8_t pic_scaling_list_present_flag[8];
	uint8_t* ScalingList4x4[6];
	uint8_t UseDefaultScalingMatrix4x4Flag[6];
	uint8_t* ScalingList8x8[2];
	uint8_t UseDefaultScalingMatrix8x8Flag[2];
	uint8_t second_chroma_qp_index_offset;
} pps_t;


/**
Slice Header
@see 7.3.3 Slice header syntax
@see read_slice_header_rbsp
@see write_slice_header_rbsp
@see debug_slice_header_rbsp
*/
typedef struct
{
	uint8_t first_mb_in_slice;
	uint8_t slice_type;
	uint8_t pic_parameter_set_id;
	uint8_t frame_num;
	uint8_t field_pic_flag;
	uint8_t bottom_field_flag;
	uint8_t idr_pic_id;
	uint8_t pic_order_cnt_lsb;
	uint8_t delta_pic_order_cnt_bottom;
	uint8_t delta_pic_order_cnt[2];
	uint8_t redundant_pic_cnt;
	uint8_t direct_spatial_mv_pred_flag;
	uint8_t num_ref_idx_active_override_flag;
	uint8_t num_ref_idx_l0_active_minus1;
	uint8_t num_ref_idx_l1_active_minus1;
	uint8_t cabac_init_idc;
	uint8_t slice_qp_delta;
	uint8_t sp_for_switch_flag;
	uint8_t slice_qs_delta;
	uint8_t disable_deblocking_filter_idc;
	uint8_t slice_alpha_c0_offset_div2;
	uint8_t slice_beta_offset_div2;
	uint8_t slice_group_change_cycle;


	struct
	{
		uint8_t luma_log2_weight_denom;
		uint8_t chroma_log2_weight_denom;
		uint8_t luma_weight_l0_flag[64];
		uint8_t luma_weight_l0[64];
		uint8_t luma_offset_l0[64];
		uint8_t chroma_weight_l0_flag[64];
		uint8_t chroma_weight_l0[64][2];
		uint8_t chroma_offset_l0[64][2];
		uint8_t luma_weight_l1_flag[64];
		uint8_t luma_weight_l1[64];
		uint8_t luma_offset_l1[64];
		uint8_t chroma_weight_l1_flag[64];
		uint8_t chroma_weight_l1[64][2];
		uint8_t chroma_offset_l1[64][2];
	} pwt; // predictive weight table

	struct // FIXME stack or array
	{
		uint8_t ref_pic_list_reordering_flag_l0;
		uint8_t ref_pic_list_reordering_flag_l1;
		uint8_t reordering_of_pic_nums_idc;
		uint8_t abs_diff_pic_num_minus1;
		uint8_t long_term_pic_num;
	} rplr; // ref pic list reorder

	struct // FIXME stack or array
	{
		uint8_t no_output_of_prior_pics_flag;
		uint8_t long_term_reference_flag;
		uint8_t adaptive_ref_pic_marking_mode_flag;
		uint8_t memory_management_control_operation;
		uint8_t difference_of_pic_nums_minus1;
		uint8_t long_term_pic_num;
		uint8_t long_term_frame_idx;
		uint8_t max_long_term_frame_idx_plus1;
	} drpm; // decoded ref pic marking
} slice_header_t;

typedef enum
{
	SliceTypeP = 0,
	SliceTypeB,
	SliceTypeI,
	SliceTypeSP,
	SliceTypeSI
} EnSliceType;

std::string GetSliceTypeString(EnSliceType type);

typedef struct
{
	int payloadType;
	int payloadSize;
	uint8_t* payload;
} sei_t;

class BitReader;

int  nal_to_rbsp(const uint8_t* nal_buf, int* nal_size, uint8_t* rbsp_buf, int* rbsp_size);

void read_seq_parameter_set_rbsp(sps_t* sps, BitReader& b);

void read_pic_parameter_set_rbsp(pps_t* pps, BitReader& b);

void read_slice_header_rbsp(slice_header_t* sh, BitReader& b, uint8_t nal_unit_type, uint8_t nal_ref_idc, sps_t* sps, pps_t* pps);

sei_t** read_sei_rbsp(uint32_t *num_seis, BitReader& b);

void release_seis(sei_t** seis, uint32_t num_seis);

#endif
