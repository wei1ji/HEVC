/*
 * HEVC parameter set parsing
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_HEVC_PS_H
#define AVCODEC_HEVC_PS_H

#include <stdint.h>
#include "config.h"
#include "libavutil/buffer.h"
#include "libavutil/pixfmt.h"
#include "libavutil/rational.h"

#include "avcodec.h"
#include "get_bits.h"
#include "hevc.h"

#ifdef SVC_EXTENSION
enum {
    DEFAULT=0,
    X2,
    X1_5,
    SNR,
};

enum ChannelType {
    CHANNEL_TYPE_LUMA    = 0,
    CHANNEL_TYPE_CHROMA  = 1,
    MAX_NUM_CHANNEL_TYPE = 2
};

typedef struct UpsamplInf {
    int addXLum;
    int addYLum;
    int scaleXLum;
    int scaleYLum;
    int addXCr;
    int addYCr;
    int scaleXCr;
    int scaleYCr;
    int idx;
    int shift[MAX_NUM_CHANNEL_TYPE];
    int shift_up[MAX_NUM_CHANNEL_TYPE];
    int mv_scale_x;
    int mv_scale_y;
} UpsamplInf;
#endif

typedef struct ShortTermRPS {
    unsigned int num_negative_pics;
    int num_delta_pocs;
    int rps_idx_num_delta_pocs;
    int32_t delta_poc[32];
    uint8_t used_by_curr_pic_flag[32];
} ShortTermRPS;

typedef struct LongTermRPS {
    int     poc[32];
    uint8_t used[32];
    uint8_t nb_refs;
} LongTermRPS;

typedef struct SliceHeader {


    ///< address (in raster order) of the first block in the current slice segment

    ///< address (in raster order) of the first block in the current slice
    unsigned int   slice_addr; // Diff with slice_segment_address ??

    //Begin here

    uint8_t first_slice_in_pic_flag;
    uint8_t no_output_of_prior_pics_flag;
    uint8_t slice_pps_id;
    uint8_t dependent_slice_segment_flag;
    unsigned int   slice_segment_address;//TODO check if uint8_t is enough

    uint8_t discardable_flag;
    uint8_t cross_layer_bla_flag;


    enum HEVCSliceType slice_type;

    uint8_t pic_output_flag;
    uint8_t colour_plane_id;
//Missing slice_reserved_flag ???

    int slice_pic_order_cnt_lsb; //(unsigned ??)

    ///< RPS coded in the slice header itself is stored here
    int short_term_ref_pic_set_sps_flag;

    int short_term_ref_pic_set_size;//(num_short_terme_ref_pic_sets ???)

    //Begin parsing the short term ref pic set
    //short_term_ref_pic_idx
    //num_long_term_sps
    //num_long_term_pics
    //lt_idx_sps[]
    //poc_lsb_lt[]
    //used_by_curr_pic_lt_flag[]
    //delta_poc_msb_present_flag[i]
    //delta_poc_msb_cycle[i]


    //TODO sort Short and long term ref pic etc.
    ShortTermRPS slice_rps;
    const ShortTermRPS *short_term_rps;
    int long_term_ref_pic_set_size;
    LongTermRPS long_term_rps;
    unsigned int list_entry_lx[2][32];

    uint8_t rpl_modification_flag[2];

    //back here after rps

    uint8_t slice_temporal_mvp_enabled_flag;

    int inter_layer_pred_enabled_flag;

    //num_inter_layer_ref_pics///< num_inter_layer_ref_pics_minus1 + 1
    //inter_layer_pred_layer_idc[i]

    uint8_t slice_sample_adaptive_offset_flag[3];
    //slice_sao_luma && slice_sao_chroma

    //num_ref_idx_active_override_flag
    //num_ref_idx_l0_active_minus1
    //num_ref_idx_l1_active_minus1

    //ref_pic_list_modification()

    uint8_t mvd_l1_zero_flag;

    uint8_t cabac_init_flag;

//    uint8_t collocated_from_l0_flag;


    unsigned int collocated_ref_idx;

    //five_minus_max_num_merge_cand

    int slice_qp_delta; //(int8_t???)
    int slice_cb_qp_offset;
    int slice_cr_qp_offset;

    uint8_t cu_chroma_qp_offset_enabled_flag;

    //deblocking_filter_override_flag ??
    //slice_deblocking_filter_disabled_flag ??

    int8_t slice_beta_offset;    ///< beta_offset_div2 * 2
    int8_t slice_tc_offset;      ///< tc_offset_div2 * 2

    uint8_t slice_loop_filter_across_slices_enabled_flag;

    int num_entry_point_offsets;
    //offset_len_minus1??
    int *entry_point_offset; ///<entry_point_offset_minus1 + 1

    unsigned int slice_segment_header_extension_length;
    uint8_t poc_reset_idc;
    uint8_t poc_reset_period_id;
    uint8_t full_poc_reset_flag;
    unsigned int poc_lsb_val;
    uint8_t poc_msb_cycle_val_present_flag;
    unsigned int poc_msb_cycle_val;
    uint8_t slice_segment_header_extension_data_bit;

    //TODO All of the following could be stored in a slice or a local ctx

    int * offset;
    int * size;

    uint8_t collocated_list; //???

    unsigned int nb_refs[2];

    uint8_t disable_deblocking_filter_flag; ///< slice_header_disable_deblocking_filter_flag



    unsigned int max_num_merge_cand; ///< 5 - 5_minus_max_num_merge_cand



    int8_t slice_qp;

    uint8_t luma_log2_weight_denom;
    int16_t chroma_log2_weight_denom;

    int16_t luma_weight_l0[16];
    int16_t chroma_weight_l0[16][2];
    int16_t chroma_weight_l1[16][2];
    int16_t luma_weight_l1[16];

    int16_t luma_offset_l0[16];
    int16_t chroma_offset_l0[16][2];

    int16_t luma_offset_l1[16];
    int16_t chroma_offset_l1[16][2];


    int     active_num_ILR_ref_idx;        //< Active inter-layer reference pictures
    int     inter_layer_pred_layer_idc[MAX_LAYERS];

#ifdef SVC_EXTENSION
    //int ScalingFactor[MAX_LAYERS][2];
    //int MvScalingFactor[MAX_LAYERS][2];
    int Bit_Depth[MAX_LAYERS][MAX_NUM_CHANNEL_TYPE];

    uint8_t m_bPocResetFlag;
    //uint8_t cross_layer_bla_flag;
#endif
    int slice_ctb_addr_rs;
} SliceHeader;

typedef struct HEVCWindow {
    int left_offset;
    int right_offset;
    int top_offset;
    int bottom_offset;
} HEVCWindow;

typedef struct PTLCommon {
    uint8_t profile_space;
    uint8_t tier_flag;
    uint8_t profile_idc;
    uint8_t profile_compatibility_flag[32];
    uint8_t level_idc;
    uint8_t progressive_source_flag;
    uint8_t interlaced_source_flag;
    uint8_t non_packed_constraint_flag;
    uint8_t frame_only_constraint_flag;
    //PTL Ptl_general;
    uint8_t setProfileIdc;
    uint8_t general_inbld_flag; 
} PTLCommon;

typedef struct PTL {
    PTLCommon general_ptl;
    PTLCommon sub_layer_ptl[HEVC_MAX_SUB_LAYERS];

    uint8_t sub_layer_profile_present_flag[HEVC_MAX_SUB_LAYERS];
    uint8_t sub_layer_level_present_flag[HEVC_MAX_SUB_LAYERS];
    //PTL Ptl_sublayer[16];

    int sub_layer_profile_space[HEVC_MAX_SUB_LAYERS];
    uint8_t sub_layer_tier_flag[HEVC_MAX_SUB_LAYERS];
    int sub_layer_profile_idc[HEVC_MAX_SUB_LAYERS];
    uint8_t sub_layer_profile_compatibility_flags[HEVC_MAX_SUB_LAYERS][32];
    int sub_layer_level_idc[HEVC_MAX_SUB_LAYERS];
} PTL;

enum ChromaFormat
{
    CHROMA_400  = 0,
    CHROMA_420  = 1,
    CHROMA_422  = 2,
    CHROMA_444  = 3
#if AUXILIARY_PICTURES
    ,NUM_CHROMA_FORMAT = 4
#endif
};

#define MULTIPLE_PTL_SUPPORT 1
enum Profiles   {
    NONE = 0,
    MAIN = 1,
    MAIN10 = 2,
    MAINSTILLPICTURE = 3,
#if MULTIPLE_PTL_SUPPORT
    RANGEEXTENSION = 4,
    RANGEEXTENSIONHIGH = 5,
    MULTIVIEWMAIN = 6,
    SCALABLEMAIN = 7,
    SCALABLEMAIN10 = 8,
#endif
};

enum Level   {
    L_NONE     = 0,
    LEVEL1        = 30,
    LEVEL2        = 60,
    LEVEL2_1      = 63,
    LEVEL3        = 90,
    LEVEL3_1      = 93,
    LEVEL4        = 120,
    LEVEL4_1      = 123,
    LEVEL5        = 150,
    LEVEL5_1      = 153,
    LEVEL5_2      = 156,
    LEVEL6        = 180,
    LEVEL6_1      = 183,
    LEVEL6_2      = 186,
};

enum Tier   {
    T_MAIN = 0,
    T_HIGH = 1,
};

typedef struct RepFormat
{
    enum ChromaFormat chroma_format_vps_idc;
    int16_t   pic_width_vps_in_luma_samples;
    int16_t   pic_height_vps_in_luma_samples;
    uint8_t   chroma_and_bit_depth_vps_present_flag;
    uint8_t   separate_colour_plane_vps_flag; 
    uint8_t   bit_depth_vps[MAX_NUM_CHANNEL_TYPE]; // coded as minus8


    uint8_t conformance_window_vps_flag;
    int     conf_win_vps_left_offset;
    int     conf_win_vps_right_offset;
    int     conf_win_vps_top_offset;
    int     conf_win_vps_bottom_offset;
} RepFormat;

typedef struct SAOParams {
    uint8_t offset_abs[3][4];   ///< sao_offset_abs
    uint8_t offset_sign[3][4];  ///< sao_offset_sign

    uint8_t band_position[3];   ///< sao_band_position
    int16_t offset_val[3][5];   ///<SaoOffsetVal

    uint8_t eo_class[3];        ///< sao_eo_class
    uint8_t type_idx[3];        ///< sao_type_idx
} SAOParams;

typedef struct SubLayerHRDParams {
    int bit_rate_value_minus1[16];
    int cpb_size_value_minus1[16];
    int cpb_size_du_value_minus1[16];
    int bit_rate_du_value_minus1[16];
    int cbr_flag[16];
} SubLayerHRDParams;

typedef struct HRDParameters {
    uint8_t     nal_hrd_parameters_present_flag;
    uint8_t     vcl_hrd_parameters_present_flag;
    uint8_t     sub_pic_hrd_params_present_flag;
    struct {
        uint8_t     tick_divisor_minus2;
        uint8_t     du_cpb_removal_delay_increment_length_minus1;
        uint8_t     sub_pic_cpb_params_in_pic_timing_sei_flag;
        uint8_t     dpb_output_delay_du_length_minus1;
    } sub_pic_hrd_params;
    uint8_t     bit_rate_scale;
    uint8_t     cpb_size_scale;
    uint8_t     cpb_size_du_scale;
    uint8_t     initial_cpb_removal_delay_length_minus1;
    uint8_t     au_cpb_removal_delay_length_minus1;
    uint8_t     dpb_output_delay_length_minus1;

    //TODO : put these in a struct
    uint8_t     fixed_pic_rate_general_flag[16];
    uint8_t     fixed_pic_rate_within_cvs_flag[16];
    int         elemental_duration_in_tc_minus1[16];
    uint8_t     low_delay_hrd_flag[16];
    uint8_t     cpb_cnt_minus1[16];
    
    SubLayerHRDParams sub_layer_hrd_params[16];
} HRDParameters;

typedef struct HEVCVUI {
    uint8_t aspect_ratio_info_present_flag;
    uint8_t aspect_ratio_idc;
    //would be clearer with width and height instead of num den
    AVRational sar;

    uint8_t overscan_info_present_flag;
    uint8_t overscan_appropriate_flag;

    uint8_t video_signal_type_present_flag;
    uint8_t video_format;
    uint8_t video_full_range_flag;

    uint8_t colour_description_present_flag;
    uint8_t colour_primaries;
    uint8_t transfer_characteristic;
    uint8_t matrix_coeffs;

    uint8_t chroma_loc_info_present_flag;
    unsigned int chroma_sample_loc_type_top_field;
    unsigned int chroma_sample_loc_type_bottom_field;

    uint8_t neutral_chroma_indication_flag;
    uint8_t frame_field_info_present_flag;
    uint8_t field_seq_flag;

    uint8_t default_display_window_flag;
    HEVCWindow def_disp_win;

    uint8_t vui_timing_info_present_flag;
    struct {
        uint32_t vui_num_units_in_tick;
        uint32_t vui_time_scale;
        uint8_t  vui_poc_proportional_to_timing_flag;
        unsigned int vui_num_ticks_poc_diff_one_minus1;// TODO minus1 +1
        uint8_t  vui_hrd_parameters_present_flag;
        //TODO add HRDParams
        HRDParameters HrdParam;
    }vui_timing_info;


    uint8_t bitstream_restriction_flag;
    //TODO add bitstream restriction structure
    struct {
        uint8_t tiles_fixed_structure_flag;
        uint8_t motion_vectors_over_pic_boundaries_flag;
        uint8_t restricted_ref_pic_lists_flag;
        unsigned int min_spatial_segmentation_idc;
        unsigned int max_bytes_per_pic_denom;
        unsigned int max_bits_per_min_cu_denom;
        unsigned int log2_max_mv_length_horizontal;
        unsigned int log2_max_mv_length_vertical;
    } bitstream_restriction;
} HEVCVUI;

typedef struct VideoSignalInfo {
    uint8_t video_vps_format;                
	uint8_t video_full_range_vps_flag;
	uint8_t color_primaries_vps;
	uint8_t transfer_characteristics_vps;
	uint8_t matrix_coeffs_vps;
} VideoSignalInfo;

typedef struct BspHrdParams {
    uint8_t vps_num_add_hrd_params;
    uint8_t cprms_add_present_flag[16];
    uint8_t num_sub_layer_hrd_minus1[16];
    HRDParameters HrdParam[16];
    uint8_t num_signalled_partitioning_schemes[16];
    uint8_t layer_included_in_partition_flag[16][16][16][16];
    uint8_t num_partitions_in_scheme_minus1[16][16];
    uint8_t num_bsp_schedules_minus1[16][16][16];
    uint8_t bsp_hrd_idx[16][16][16][16][16];
    uint8_t bsp_sched_idx[16][16][16][16][16];

} BspHrdParams;

typedef struct VPSVUIParameters {
    uint8_t cross_layer_pic_type_aligned_flag;
    uint8_t cross_layer_irap_aligned_flag;
    uint8_t all_layers_idr_aligned_flag;
    uint8_t bit_rate_present_vps_flag;
    uint8_t pic_rate_present_vps_flag;
    uint8_t bit_rate_present_flag[16][16];
    uint8_t pic_rate_present_flag[16][16];

    uint16_t avg_bit_rate[16][16];
    uint16_t max_bit_rate[16][16];

    uint8_t  constant_pic_rate_idc[16][16];
    uint16_t avg_pic_rate[16][16];

    uint8_t video_signal_info_idx_present_flag;
    uint8_t vps_num_video_signal_info_minus1;
    VideoSignalInfo video_signal_info[16];
    uint8_t vps_video_signal_info_idx[16];
    uint8_t tiles_not_in_use_flag;
    uint8_t tiles_in_use_flag[16];
    uint8_t loop_filter_not_across_tiles_flag[16];
    uint8_t tile_boundaries_aligned_flag[16][16];

    uint8_t wpp_not_in_use_flag; 
    uint8_t wpp_in_use_flag[16];

    uint8_t single_layer_for_non_irap_flag;
    uint8_t higher_layer_irap_skip_flag;    
    uint8_t ilp_restricted_ref_layers_flag;

    uint8_t min_spatial_segment_offset_plus1[16][16];
    uint8_t ctu_based_offset_enabled_flag[16][16];
    uint8_t min_horizontal_ctu_offset_plus1[16][16];

    uint8_t vps_vui_bsp_hrd_present_flag;
    BspHrdParams bsp_hrd_params;
    uint8_t base_layer_parameter_set_compatibility_flag[16]; 
} VPSVUIParameters;

typedef struct DPBSize {
    uint8_t sub_layer_flag_info_present_flag[16];
    uint8_t sub_layer_dpb_info_present_flag[16][16];
    uint16_t max_vps_dec_pic_buffering_minus1[16][16][16];
    uint16_t max_vps_num_reorder_pics[16][16];
    uint16_t max_vps_latency_increase_plus1[16][16];
} DPBSize;

typedef struct HEVCVPSExt {
    PTL   ptl[16];

    uint8_t splitting_flag;
    uint8_t scalability_mask_flag[16];
    uint8_t dimension_id_len[16];
    uint8_t vps_nuh_layer_id_present_flag;
    uint8_t layer_id_in_nuh[64];
    uint8_t layer_id_in_vps[16];//TODO remove from structure
    uint8_t dimension_id[16][16];
    uint8_t view_id_len;
    uint8_t view_id_val[16];
    uint8_t direct_dependency_flag[64][64];
    uint16_t num_add_layer_sets;
    uint8_t highest_layer_idx[16][16];
    uint8_t vps_sub_layers_max_minus1_present_flag;
    uint8_t sub_layers_vps_max_minus1[16];
    uint8_t max_tid_ref_present_flag;
    uint8_t max_tid_il_ref_pics_plus1[16][16];
    uint8_t default_ref_layers_active_flag;
    uint8_t vps_num_profile_tier_level_minus1;
    uint8_t vps_profile_present_flag[16];
    uint8_t num_add_olss;
    uint8_t default_output_layer_idc;
    uint8_t layer_set_idx_for_ols[16];
    uint8_t output_layer_flag[16][16];
    uint8_t profile_tier_level_idx[16][16];
    uint8_t alt_output_layer_flag[16];
    uint8_t vps_num_rep_formats_minus1;
    uint8_t rep_format_idx_present_flag;
    uint8_t vps_rep_format_idx[16];
    uint8_t max_one_active_ref_layer_flag;
    uint8_t vps_poc_lsb_aligned_flag;
    uint8_t poc_lsb_not_present_flag[16];
    uint8_t direct_dep_type_len_minus2;
    uint8_t direct_dependency_all_layers_flag;
    uint8_t direct_dependency_all_layers_type;
    uint8_t direct_dependency_type[16][16];

    uint8_t vps_non_vui_extension_length;
    uint8_t vps_non_vui_extension_data_byte;
    uint8_t vps_vui_present_flag;
    uint8_t vps_vui_alignment_bit_equal_to_one;

    //TODO remove those 3 and use appropriate and add appropriate structure into
    //HEVCMultiLayerContext
    uint8_t     ref_layer_id[16][16];
    uint8_t     num_direct_ref_layers[16];
    uint8_t     number_ref_layers[16][16];

    RepFormat         rep_format[16];
    DPBSize           dpb_size;
    VPSVUIParameters  vui_parameters;
} HEVCVPSExt;

typedef struct HEVCVPS {
    uint8_t  vps_id;
    uint8_t  vps_base_layer_internal_flag;
    uint8_t  vps_base_layer_available_flag;
    uint8_t  vps_nonHEVCBaseLayerFlag; ///< not standard but helpfull
    uint8_t  vps_max_layers;     ///< vps_max_layers_minus1 + 1
    uint8_t  vps_max_sub_layers; ///< vps_max_temporal_layers_minus1 + 1
    uint8_t  vps_temporal_id_nesting_flag;
    uint16_t vps_reserved_0xffff_16bits;

    PTL ptl; //TODO profile their level could be a pointer to a PTL to reduce VPS
    // footprint however we have to be carefull while comparing VPS NAL for
    // removal from vps list

    uint8_t  vps_sub_layer_ordering_info_present_flag;
    //FIXME structure + dyn malloc for sub_layers ordering info ???
    uint8_t    vps_max_dec_pic_buffering[HEVC_MAX_SUB_LAYERS]; ///< vps_max_dec_pic_buffering_minus1 + 1
    uint8_t    vps_max_num_reorder_pics [HEVC_MAX_SUB_LAYERS];
    int32_t    vps_max_latency_increase [HEVC_MAX_SUB_LAYERS]; ///< vps_max_dec_pic_buffering_plus1 - 1

    uint8_t  vps_max_layer_id;
    int16_t  vps_num_layer_sets; ///< vps_num_layer_sets_minus1 + 1
    uint8_t    layer_id_included_flag[16][16];

    uint8_t  vps_timing_info_present_flag;
    //FIXME structure + dyn malloc for vps timing info ???
    uint32_t   vps_num_units_in_tick;
    uint32_t   vps_time_scale;
    uint8_t    vps_poc_proportional_to_timing_flag;
    int          vps_num_ticks_poc_diff_one; ///< vps_num_ticks_poc_diff_one_minus1 + 1
    int16_t    vps_num_hrd_parameters;
    unsigned int hrd_layer_set_idx[16];
    //FIXME keep cprms_present_flag table ???
    HRDParameters HrdParam;

    //FIXME vps_extension2_flag etc..
    int         vps_extension_flag;
    HEVCVPSExt  vps_ext;
    //AVBufferRef *nal;
} HEVCVPS;

typedef struct ScalingList {
    /* This is a little wasteful, since sizeID 0 only needs 8 coeffs,
     * and size ID 3 only has 2 arrays, not 6. */
    uint8_t sl[4][6][64];
    uint8_t sl_dc[2][6];
} ScalingList;

typedef struct HEVCSPS {
    uint8_t  vps_id;

    uint8_t  sps_max_sub_layers; ///<sps_max_sub_layers_minus1 + 1
    uint8_t  sps_ext_or_max_sub_layers; ///< sps_ext_or_max_sub_layers_minus1 + 1


    // This is added to SPS in order to know if
    // wether or not we are in a multi_layer context
    //value is set to (nuh_layer_id && max_sub_layers == 7)
    uint8_t is_multi_layer_ext_sps;

    uint8_t sps_temporal_id_nesting_flag;
    uint8_t sps_id;


    PTL       ptl;

    uint8_t chroma_format_idc;
    uint8_t separate_colour_plane_flag;

    //uint16_t width_in_luma_samples == width
    //uint16_t height_in_luma_samples == height

    uint8_t conformance_window_flag;
    HEVCWindow conf_win;

    //FIXME the use of table is sometimes troubling into the code
    // we should use bitdepth and bit depth_c as specified by the standard
    int bit_depth[MAX_NUM_CHANNEL_TYPE];
    int pixel_shift[MAX_NUM_CHANNEL_TYPE]; // Not in the standard but usefull

    unsigned int log2_max_poc_lsb; ///< log2_max_pic_order_cnt_lsb_minus4 + 4

    uint8_t sps_sub_layer_ordering_info_present_flag;

    struct {
        uint16_t max_dec_pic_buffering;
        uint16_t num_reorder_pics;
        uint16_t max_latency_increase;
    } temporal_layer[HEVC_MAX_SUB_LAYERS];

    // multi_layer_ext_flag == 1
    uint8_t update_rep_format_flag;
    uint8_t sps_rep_format_idx;
    uint8_t sps_infer_scaling_list_flag;
    uint8_t sps_scaling_list_ref_layer_id;

    unsigned int log2_min_cb_size;
    unsigned int log2_diff_max_min_cb_size;
    unsigned int log2_min_tb_size;
    unsigned int log2_diff_max_min_tb_size;

    int max_transform_hierarchy_depth_inter;
    int max_transform_hierarchy_depth_intra;

    uint8_t scaling_list_enabled_flag;

    //TODO  rename those
    uint8_t sps_scaling_list_data_present_flag;
    ScalingList scaling_list;

    uint8_t amp_enabled_flag;
    uint8_t sao_enabled_flag;
    uint8_t pcm_enabled_flag;

    struct {
        uint8_t bit_depth;
        uint8_t bit_depth_chroma;
        unsigned int log2_min_pcm_cb_size;
        unsigned int log2_max_pcm_cb_size;
        uint8_t loop_filter_disable_flag;
    } pcm;

    unsigned int num_short_term_rps;
    ShortTermRPS st_rps[HEVC_MAX_SHORT_TERM_RPS_COUNT];

    uint8_t  long_term_ref_pics_present_flag;
    uint8_t  num_long_term_ref_pics_sps;
    uint16_t lt_ref_pic_poc_lsb_sps      [32];
    uint8_t  used_by_curr_pic_lt_sps_flag[32];

    uint8_t  sps_temporal_mvp_enabled_flag;
    uint8_t  sps_strong_intra_smoothing_enable_flag;

    uint8_t vui_parameters_present_flag;
    HEVCVUI vui;

    uint8_t  sps_extension_present_flag;
    uint8_t  sps_range_extension_flag;
    uint8_t  sps_multilayer_extension_flag;
    uint8_t  sps_3d_extension_flag;
    uint8_t  sps_extension_5bits;

    //TODO HEVCSPSExt struct;

    // Range extensions
    uint8_t transform_skip_rotation_enabled_flag;
    uint8_t transform_skip_context_enabled_flag;
    uint8_t implicit_rdpcm_enabled_flag;
    uint8_t explicit_rdpcm_enabled_flag;
    uint8_t extended_precision_processing_flag;
    uint8_t intra_smoothing_disabled_flag;
    uint8_t high_precision_offsets_enabled_flag; 
    uint8_t persistent_rice_adaptation_enabled_flag;
    uint8_t cabac_bypass_alignment_enabled_flag;

    // Multi-layer extensions
    uint8_t inter_view_mv_vert_constraint_flag;

    //Those are some useful computed values which could be added to the HEVCContext
    //instead
    ///< output (i.e. cropped) values
    int output_width, output_height;
    HEVCWindow output_window;

    enum AVPixelFormat pix_fmt;

    unsigned int log2_ctb_size;
    unsigned int log2_min_pu_size;
    unsigned int log2_max_trafo_size;

    ///< coded frame dimension in various units
    int width;
    int height;
    int ctb_width;
    int ctb_height;
    int ctb_size;
    int min_cb_width;
    int min_cb_height;
    int min_tb_width;
    int min_tb_height;
    int min_pu_width;
    int min_pu_height;
    int tb_mask;

    int hshift[3];
    int vshift[3];

    int qp_bd_offset;



    //FIXME scaled_ref_layer is used but not set ???
    HEVCWindow scaled_ref_layer_window[MAX_LAYERS];
    //FIXME: This flag could probably be removed without any harm
    uint8_t    set_mfm_enabled_flag;
    uint8_t    v1_compatible;

#if OHCONFIG_AMT
    uint8_t use_intra_emt;
    uint8_t use_inter_emt;
#endif
} HEVCSPS;

typedef struct SYUVP {
    int16_t Y;
    int16_t U;
    int16_t V;
} SYUVP;

typedef struct SCuboid {
  SYUVP P[4];
} SCuboid;

typedef struct TCom3DAsymLUT {
    uint16_t  num_cm_ref_layers_minus1;
    uint8_t   uiRefLayerId[16];
    uint8_t   cm_octant_depth;              // m_nMaxOctantDepth = nMaxOctantDepth;
    uint8_t   cm_y_part_num_log2;           //m_nMaxYPartNumLog2 = nMaxYPartNumLog2;
    uint16_t  cm_input_luma_bit_depth;     // m_nInputBitDepthY = nInputBitDepth;
    uint16_t  cm_input_chroma_bit_depth;   // m_nInputBitDepthC = nInputBitDepthC;
    uint16_t  cm_output_luma_bit_depth;
    uint16_t  cm_output_chroma_bit_depth;  // m_nOutputBitDepthC = nOutputBitDepthC;
    uint8_t   cm_res_quant_bit;
    uint8_t   cm_flc_bits;
    int  cm_adapt_threshold_u_delta;
    int  cm_adapt_threshold_v_delta;
    int  nAdaptCThresholdU;
    int  nAdaptCThresholdV;

    int16_t  delta_bit_depth;   //    m_nDeltaBitDepthC = m_nOutputBitDepthC - m_nInputBitDepthC;
    int16_t  delta_bit_depth_C; //    m_nDeltaBitDepth = m_nOutputBitDepthY - m_nInputBitDepthY;
    uint16_t  max_part_num_log2; //3 * m_nMaxOctantDepth + m_nMaxYPartNumLog2;

    int16_t YShift2Idx;
    int16_t UShift2Idx;
    int16_t VShift2Idx;
    int16_t nMappingShift;
    int16_t nMappingOffset;
    SCuboid ***S_Cuboid;

} TCom3DAsymLUT;

typedef struct HEVCPPS {
    uint8_t pps_id;
    uint8_t sps_id; ///< seq_parameter_set_id

    uint8_t dependent_slice_segments_enabled_flag;
    uint8_t output_flag_present_flag;

    int     num_extra_slice_header_bits;

    uint8_t sign_data_hiding_flag;
    uint8_t cabac_init_present_flag;

    int num_ref_idx_l0_default_active; ///< num_ref_idx_l0_default_active_minus1 + 1
    int num_ref_idx_l1_default_active; ///< num_ref_idx_l1_default_active_minus1 + 1

    int init_qp_minus26;

    uint8_t constrained_intra_pred_flag;
    uint8_t transform_skip_enabled_flag;

    uint8_t cu_qp_delta_enabled_flag;
    int     diff_cu_qp_delta_depth;

    int pps_cb_qp_offset;
    int pps_cr_qp_offset;

    uint8_t pps_slice_chroma_qp_offsets_present_flag;

    uint8_t weighted_pred_flag;
    uint8_t weighted_bipred_flag;

    uint8_t transquant_bypass_enable_flag;
    uint8_t tiles_enabled_flag;
    uint8_t entropy_coding_sync_enabled_flag;

    int num_tile_columns;   ///< num_tile_columns_minus1 + 1
    int num_tile_rows;      ///< num_tile_rows_minus1 + 1

    uint8_t uniform_spacing_flag;
    unsigned int *column_width;  ///< ColumnWidth
    unsigned int *row_height;    ///< RowHeight

    uint8_t loop_filter_across_tiles_enabled_flag;
    uint8_t pps_loop_filter_across_slices_enabled_flag;

    uint8_t deblocking_filter_control_present_flag;
    uint8_t deblocking_filter_override_enabled_flag;
    uint8_t pps_deblocking_filter_disabled_flag;

    int8_t pps_beta_offset;    ///< beta_offset_div2 * 2
    int8_t pps_tc_offset;      ///< tc_offset_div2 * 2

    uint8_t pps_scaling_list_data_present_flag;
    ScalingList scaling_list;

    uint8_t lists_modification_present_flag;
    int     log2_parallel_merge_level; ///< log2_parallel_merge_level_minus2 + 2

    uint8_t slice_segment_header_extension_present_flag;

    uint8_t pps_extension_present_flag;
    uint8_t pps_range_extension_flag;
    uint8_t pps_multilayer_extension_flag;
    uint8_t pps_3d_extension_flag;
    uint8_t pps_extension_5bits;


    //Range extensions
    uint8_t log2_max_transform_skip_block_size;
    uint8_t cross_component_prediction_enabled_flag;
    uint8_t chroma_qp_offset_list_enabled_flag;
    uint8_t diff_cu_chroma_qp_offset_depth;

    uint8_t chroma_qp_offset_list_len_minus1;
    int8_t  cb_qp_offset_list[5];
    int8_t  cr_qp_offset_list[5];

    uint8_t log2_sao_offset_scale_luma;
    uint8_t log2_sao_offset_scale_chroma;

    // Multilayer_extensions
    uint8_t poc_reset_info_present_flag;
    uint8_t pps_infer_scaling_list_flag;
    uint8_t pps_scaling_list_ref_layer_id;

    uint16_t num_ref_loc_offsets;
    //those should be tables are in tables
    uint8_t ref_loc_offset_layer_id;
    uint8_t scaled_ref_layer_offset_present_flag;
    HEVCWindow scaled_ref_window[16];

    uint8_t ref_region_offset_present_flag;
    HEVCWindow ref_window[16];

    uint8_t resample_phase_set_present_flag;

    uint16_t phase_hor_luma[16];
    uint16_t phase_ver_luma[16];
    uint16_t phase_hor_chroma[16];
    uint16_t phase_ver_chroma[16];

    uint8_t colour_mapping_enabled_flag;



    TCom3DAsymLUT pc3DAsymLUT;
    uint8_t m_nCGSOutputBitDepth[MAX_NUM_CHANNEL_TYPE];

    // Derived parameters

    unsigned int *col_bd;        ///< ColBd
    unsigned int *row_bd;        ///< RowBd

    int *col_idxX;
    int *ctb_addr_rs_to_ts; ///< CtbAddrRSToTS
    int *ctb_addr_ts_to_rs; ///< CtbAddrTSToRS
    int *tile_id;           ///< TileId
    int *tile_width;           ///< TileWidth
    int *tile_pos_rs;       ///< TilePosRS
    int *wpp_pos_ts;
    int *min_tb_addr_zs;    ///< MinTbAddrZS
    int *min_tb_addr_zs_tab;///< MinTbAddrZS

    uint8_t is_setup;

} HEVCPPS;

typedef struct HEVCParamSets {
    AVBufferRef *vps_list[HEVC_MAX_VPS_COUNT];
    AVBufferRef *sps_list[HEVC_MAX_SPS_COUNT];
    AVBufferRef *pps_list[HEVC_MAX_PPS_COUNT];

    /* currently active parameter sets */
    const HEVCVPS *vps;
    const HEVCSPS *sps;
    const HEVCPPS *pps;
} HEVCParamSets;



typedef struct CodingTree {
    int depth; ///< ctDepth
} CodingTree;


int ff_hevc_decode_short_term_rps(GetBitContext *gb, AVCodecContext *avctx,
                                  ShortTermRPS *rps, const HEVCSPS *sps, int is_slice_header);

/**
 * Parse the SPS from the bitstream into the provided HEVCSPS struct.
 *
 * @param sps_id the SPS id will be written here
 * @param apply_defdispwin if set 1, the default display window from the VUI
 *                         will be applied to the video dimensions
 * @param vps_list if non-NULL, this function will validate that the SPS refers
 *                 to an existing VPS
 */
int ff_hevc_parse_sps(HEVCSPS *sps, GetBitContext *gb, unsigned int *sps_id,
                      int apply_defdispwin, AVBufferRef **vps_list, AVCodecContext *avctx, int nuh_layer_id);

int ff_hevc_decode_nal_vps(GetBitContext *gb, AVCodecContext *avctx,
                           HEVCParamSets *ps);
int ff_hevc_decode_nal_sps(GetBitContext *gb, AVCodecContext *avctx,
                           HEVCParamSets *ps, int apply_defdispwin, int nuh_layer_id);
int ff_hevc_decode_nal_pps(GetBitContext *gb, AVCodecContext *avctx,
                           HEVCParamSets *ps);

int ff_hevc_encode_nal_vps(HEVCVPS *vps, unsigned int id,
                           uint8_t *buf, int buf_size);

 int setup_pps(AVCodecContext *avctx,
                            HEVCPPS *pps, HEVCSPS *sps);






void Free3DArray(HEVCPPS * pc3DAsymLUT);

#endif /* AVCODEC_HEVC_PS_H */
