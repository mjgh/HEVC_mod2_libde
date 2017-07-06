/*
 * H.265 video codec.
 * Copyright (c) 2013 StrukturAG, Dirk Farin, <farin@struktur.de>
 *
 * This file is part of libde265.
 *
 * libde265 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libde265 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libde265.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "sps.h"
#include "sps_func.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>


static int SubWidthC[]  = { -1,2,2,1 };
static int SubHeightC[] = { -1,2,1,1 };


// TODO: should be in some header-file of refpic.c
extern bool read_short_term_ref_pic_set(decoder_context* ctx, bitreader* br, ref_pic_set* sets,
                                        int idxRps, int num_short_term_ref_pic_sets);


de265_error read_sps(decoder_context* ctx, bitreader* br,
                     seq_parameter_set* sps, ref_pic_set** ref_pic_sets)
{
  sps->video_parameter_set_id = get_bits(br,4);
  sps->sps_max_sub_layers     = get_bits(br,3) +1;
  if (sps->sps_max_sub_layers>7) {
    return DE265_ERROR_CODED_PARAMETER_OUT_OF_RANGE;
  }

  sps->sps_temporal_id_nesting_flag = get_bits(br,1);

  read_profile_tier_level(br,&sps->profile_tier_level, sps->sps_max_sub_layers);

  sps->seq_parameter_set_id = get_uvlc(br);


  // --- decode chroma type ---

  sps->chroma_format_idc = get_uvlc(br);

  if (sps->chroma_format_idc == 3) {
    sps->separate_colour_plane_flag = get_bits(br,1);
  }
  else {
    sps->separate_colour_plane_flag = 0;
  }

  if (sps->separate_colour_plane_flag) {
    sps->ChromaArrayType = 0;
  }
  else {
    sps->ChromaArrayType = sps->chroma_format_idc;
  }

  if (sps->chroma_format_idc<0 ||
      sps->chroma_format_idc>3) {
    add_warning(ctx, DE265_WARNING_INVALID_CHROMA_FORMAT, false);
    return DE265_ERROR_CODED_PARAMETER_OUT_OF_RANGE;
  }

  sps->SubWidthC  = SubWidthC [sps->chroma_format_idc];
  sps->SubHeightC = SubHeightC[sps->chroma_format_idc];


  // --- picture size ---

  sps->pic_width_in_luma_samples = get_uvlc(br);
  sps->pic_height_in_luma_samples = get_uvlc(br);

  sps->conformance_window_flag = get_bits(br,1);

  if (sps->conformance_window_flag) {
    sps->conf_win_left_offset  = get_uvlc(br);
    sps->conf_win_right_offset = get_uvlc(br);
    sps->conf_win_top_offset   = get_uvlc(br);
    sps->conf_win_bottom_offset= get_uvlc(br);
  }
  else {
    sps->conf_win_left_offset  = 0;
    sps->conf_win_right_offset = 0;
    sps->conf_win_top_offset   = 0;
    sps->conf_win_bottom_offset= 0;
  }

  if (sps->ChromaArrayType==0) {
    sps->WinUnitX = 1;
    sps->WinUnitY = 1;
  }
  else {
    sps->WinUnitX = SubWidthC[sps->chroma_format_idc];
    sps->WinUnitY = SubHeightC[sps->chroma_format_idc];
  }


  sps->bit_depth_luma   = get_uvlc(br) +8;
  sps->bit_depth_chroma = get_uvlc(br) +8;

  sps->log2_max_pic_order_cnt_lsb = get_uvlc(br) +4;
  sps->MaxPicOrderCntLsb = 1<<(sps->log2_max_pic_order_cnt_lsb);


  // --- sub_layer_ordering_info ---

  sps->sps_sub_layer_ordering_info_present_flag = get_bits(br,1);

  int firstLayer = (sps->sps_sub_layer_ordering_info_present_flag ?
                    0 : sps->sps_max_sub_layers-1 );

  for (int i=firstLayer ; i <= sps->sps_max_sub_layers-1; i++ ) {
    sps->sps_max_dec_pic_buffering[i] = get_uvlc(br);
    sps->sps_max_num_reorder_pics[i]  = get_uvlc(br);
    sps->sps_max_latency_increase[i]  = get_uvlc(br);
  }

  // copy info to all layers if only specified once

  if (sps->sps_sub_layer_ordering_info_present_flag) {
    int ref = sps->sps_max_sub_layers-1;
    assert(ref<7);

    for (int i=0 ; i < sps->sps_max_sub_layers-1; i++ ) {
      sps->sps_max_dec_pic_buffering[i] = sps->sps_max_dec_pic_buffering[ref];
      sps->sps_max_num_reorder_pics[i]  = sps->sps_max_num_reorder_pics[ref];
      sps->sps_max_latency_increase[i]  = sps->sps_max_latency_increase[ref];
    }
  }


  sps->log2_min_luma_coding_block_size = get_uvlc(br)+3;
  sps->log2_diff_max_min_luma_coding_block_size = get_uvlc(br);
  sps->log2_min_transform_block_size = get_uvlc(br)+2;
  sps->log2_diff_max_min_transform_block_size = get_uvlc(br);
  sps->max_transform_hierarchy_depth_inter = get_uvlc(br);
  sps->max_transform_hierarchy_depth_intra = get_uvlc(br);
  sps->scaling_list_enable_flag = get_bits(br,1);

  if (sps->scaling_list_enable_flag) {

    sps->sps_scaling_list_data_present_flag = get_bits(br,1);
    if (sps->sps_scaling_list_data_present_flag) {

      return DE265_ERROR_SCALING_LIST_NOT_IMPLEMENTED;
    }
  }

  sps->amp_enabled_flag = get_bits(br,1);
  sps->sample_adaptive_offset_enabled_flag = get_bits(br,1);
  sps->pcm_enabled_flag = get_bits(br,1);
  if (sps->pcm_enabled_flag) {
    sps->pcm_sample_bit_depth_luma = get_bits(br,4)+1;
    sps->pcm_sample_bit_depth_chroma = get_bits(br,4)+1;
    sps->log2_min_pcm_luma_coding_block_size = get_uvlc(br)+3;
    sps->log2_diff_max_min_pcm_luma_coding_block_size = get_uvlc(br);
    sps->pcm_loop_filter_disable_flag = get_bits(br,1);
  }
  else {
    sps->pcm_sample_bit_depth_luma = 0;
    sps->pcm_sample_bit_depth_chroma = 0;
    sps->log2_min_pcm_luma_coding_block_size = 0;
    sps->log2_diff_max_min_pcm_luma_coding_block_size = 0;
    sps->pcm_loop_filter_disable_flag = 0;
  }

  sps->num_short_term_ref_pic_sets = get_uvlc(br);
  if (sps->num_short_term_ref_pic_sets < 0 ||
      sps->num_short_term_ref_pic_sets > 64) {
    add_warning(ctx, DE265_WARNING_NUMBER_OF_SHORT_TERM_REF_PIC_SETS_OUT_OF_RANGE, false);
    return DE265_ERROR_CODED_PARAMETER_OUT_OF_RANGE;
  }

  // --- allocate reference pic set ---

  // allocate one more for the ref-pic-set that may be sent in the slice header
  // TODO: should use "realloc" and only reallocate if necessary
  free(*ref_pic_sets);
  *ref_pic_sets = (ref_pic_set *)calloc(sizeof(ref_pic_set), sps->num_short_term_ref_pic_sets + 1);

  for (int i = 0; i < sps->num_short_term_ref_pic_sets; i++) {

    //alloc_ref_pic_set(&(*ref_pic_sets)[i],
    //sps->sps_max_dec_pic_buffering[sps->sps_max_sub_layers-1]);

    bool success = read_short_term_ref_pic_set(ctx,br,*ref_pic_sets, i,
                                               sps->num_short_term_ref_pic_sets);
    if (!success) {
      return DE265_WARNING_SPS_HEADER_INVALID;
    }

    // dump_short_term_ref_pic_set(&(*ref_pic_sets)[i], fh);
  }

  sps->long_term_ref_pics_present_flag = get_bits(br,1);

  if (sps->long_term_ref_pics_present_flag) {

    sps->num_long_term_ref_pics_sps = get_uvlc(br);
    if (sps->num_long_term_ref_pics_sps > 32) {
      return DE265_ERROR_CODED_PARAMETER_OUT_OF_RANGE;
    }

    for (int i = 0; i < sps->num_long_term_ref_pics_sps; i++ ) {
      sps->lt_ref_pic_poc_lsb_sps[i] = get_bits(br, sps->log2_max_pic_order_cnt_lsb);
      sps->used_by_curr_pic_lt_sps_flag[i] = get_bits(br,1);
    }
  }
  else {
    sps->num_long_term_ref_pics_sps = 0; // NOTE: missing definition in standard !
  }

  sps->sps_temporal_mvp_enabled_flag = get_bits(br,1);
  sps->strong_intra_smoothing_enable_flag = get_bits(br,1);
  sps->vui_parameters_present_flag = get_bits(br,1);

#if 0
  if (sps->vui_parameters_present_flag) {
    assert(false);
    /*
      vui_parameters()

        sps_extension_flag
        u(1)
        if( sps_extension_flag )

          while( more_rbsp_data() )

            sps_extension_data_flag
              u(1)
              rbsp_trailing_bits()
    */
  }

  sps->sps_extension_flag = get_bits(br,1);
  if (sps->sps_extension_flag) {
    assert(false);
  }

  check_rbsp_trailing_bits(br);
#endif

  // --- compute derived values ---

  sps->BitDepth_Y   = sps->bit_depth_luma;
  sps->QpBdOffset_Y = 6*(sps->bit_depth_luma-8);
  sps->BitDepth_C   = sps->bit_depth_chroma;
  sps->QpBdOffset_C = 6*(sps->bit_depth_chroma-8);

  sps->Log2MinCbSizeY = sps->log2_min_luma_coding_block_size;
  sps->Log2CtbSizeY = sps->Log2MinCbSizeY + sps->log2_diff_max_min_luma_coding_block_size;
  sps->MinCbSizeY = 1 << sps->Log2MinCbSizeY;
  sps->CtbSizeY = 1 << sps->Log2CtbSizeY;
  sps->PicWidthInMinCbsY = sps->pic_width_in_luma_samples / sps->MinCbSizeY;
  sps->PicWidthInCtbsY   = ceil_div(sps->pic_width_in_luma_samples, sps->CtbSizeY);
  sps->PicHeightInMinCbsY = sps->pic_height_in_luma_samples / sps->MinCbSizeY;
  sps->PicHeightInCtbsY   = ceil_div(sps->pic_height_in_luma_samples,sps->CtbSizeY);
  sps->PicSizeInMinCbsY   = sps->PicWidthInMinCbsY * sps->PicHeightInMinCbsY;
  sps->PicSizeInCtbsY = sps->PicWidthInCtbsY * sps->PicHeightInCtbsY;
  sps->PicSizeInSamplesY = sps->pic_width_in_luma_samples * sps->pic_height_in_luma_samples;

  if (sps->chroma_format_idc==0 || sps->separate_colour_plane_flag) {
    sps->CtbWidthC  = 0;
    sps->CtbHeightC = 0;
  }
  else {
    sps->CtbWidthC  = sps->CtbSizeY / sps->SubWidthC;
    sps->CtbHeightC = sps->CtbSizeY / sps->SubHeightC;
  }

  sps->Log2MinTrafoSize = sps->log2_min_transform_block_size;
  sps->Log2MaxTrafoSize = sps->log2_min_transform_block_size + sps->log2_diff_max_min_transform_block_size;

  sps->Log2MinPUSize = sps->Log2MinCbSizeY-1;
  sps->PicWidthInMinPUs  = sps->PicWidthInCtbsY  << (sps->Log2CtbSizeY - sps->Log2MinPUSize);
  sps->PicHeightInMinPUs = sps->PicHeightInCtbsY << (sps->Log2CtbSizeY - sps->Log2MinPUSize);

  sps->Log2MinIpcmCbSizeY = sps->log2_min_pcm_luma_coding_block_size;
  sps->Log2MaxIpcmCbSizeY = (sps->log2_min_pcm_luma_coding_block_size +
                             sps->log2_diff_max_min_pcm_luma_coding_block_size);

  // the following are not in the standard
  sps->PicWidthInTbsY  = sps->PicWidthInCtbsY  << (sps->Log2CtbSizeY - sps->Log2MinTrafoSize);
  sps->PicHeightInTbsY = sps->PicHeightInCtbsY << (sps->Log2CtbSizeY - sps->Log2MinTrafoSize);
  sps->PicSizeInTbsY = sps->PicWidthInTbsY * sps->PicHeightInTbsY;

  sps->sps_read = true;

  return DE265_OK;
}



void dump_sps(seq_parameter_set* sps, ref_pic_set* sets, int fd)
{
  //#if (_MSC_VER >= 1500)
  //#define LOG0(t) loginfo(LogHeaders, t)
  //#define LOG1(t,d) loginfo(LogHeaders, t,d)
  //#define LOG2(t,d1,d2) loginfo(LogHeaders, t,d1,d2)
  //#define LOG3(t,d1,d2,d3) loginfo(LogHeaders, t,d1,d2,d3)

  FILE* fh;
  if (fd==1) fh=stdout;
  else if (fd==2) fh=stderr;
  else { return; }

#define LOG0(t) log2fh(fh, t)
#define LOG1(t,d) log2fh(fh, t,d)
#define LOG2(t,d1,d2) log2fh(fh, t,d1,d2)
#define LOG3(t,d1,d2,d3) log2fh(fh, t,d1,d2,d3)
  

  LOG0("----------------- SPS -----------------\n");
  LOG1("video_parameter_set_id  : %d\n", sps->video_parameter_set_id);
  LOG1("sps_max_sub_layers      : %d\n", sps->sps_max_sub_layers);
  LOG1("sps_temporal_id_nesting_flag : %d\n", sps->sps_temporal_id_nesting_flag);

  dump_profile_tier_level(&sps->profile_tier_level, sps->sps_max_sub_layers, fh);

  LOG1("seq_parameter_set_id    : %d\n", sps->seq_parameter_set_id);
  LOG2("chroma_format_idc       : %d (%s)\n", sps->chroma_format_idc,
       sps->chroma_format_idc == 1 ? "4:2:0" :
       sps->chroma_format_idc == 2 ? "4:2:2" :
       sps->chroma_format_idc == 3 ? "4:4:4" : "unknown");

  if (sps->chroma_format_idc == 3) {
    LOG1("separate_colour_plane_flag : %d\n", sps->separate_colour_plane_flag);
  }

  LOG1("pic_width_in_luma_samples  : %d\n", sps->pic_width_in_luma_samples);
  LOG1("pic_height_in_luma_samples : %d\n", sps->pic_height_in_luma_samples);
  LOG1("conformance_window_flag    : %d\n", sps->conformance_window_flag);

  if (sps->conformance_window_flag) {
    LOG1("conf_win_left_offset  : %d\n", sps->conf_win_left_offset);
    LOG1("conf_win_right_offset : %d\n", sps->conf_win_right_offset);
    LOG1("conf_win_top_offset   : %d\n", sps->conf_win_top_offset);
    LOG1("conf_win_bottom_offset: %d\n", sps->conf_win_bottom_offset);
  }

  LOG1("bit_depth_luma   : %d\n", sps->bit_depth_luma);
  LOG1("bit_depth_chroma : %d\n", sps->bit_depth_chroma);

  LOG1("log2_max_pic_order_cnt_lsb : %d\n", sps->log2_max_pic_order_cnt_lsb);
  LOG1("sps_sub_layer_ordering_info_present_flag : %d\n", sps->sps_sub_layer_ordering_info_present_flag);

  int firstLayer = (sps->sps_sub_layer_ordering_info_present_flag ?
                    0 : sps->sps_max_sub_layers-1 );

  for (int i=firstLayer ; i <= sps->sps_max_sub_layers-1; i++ ) {
    LOG1("Layer %d\n",i);
    LOG1("  sps_max_dec_pic_buffering : %d\n", sps->sps_max_dec_pic_buffering[i]);
    LOG1("  sps_max_num_reorder_pics  : %d\n", sps->sps_max_num_reorder_pics[i]);
    LOG1("  sps_max_latency_increase  : %d\n", sps->sps_max_latency_increase[i]);
  }

  LOG1("log2_min_luma_coding_block_size : %d\n", sps->log2_min_luma_coding_block_size);
  LOG1("log2_diff_max_min_luma_coding_block_size : %d\n",sps->log2_diff_max_min_luma_coding_block_size);
  LOG1("log2_min_transform_block_size   : %d\n", sps->log2_min_transform_block_size);
  LOG1("log2_diff_max_min_transform_block_size : %d\n", sps->log2_diff_max_min_transform_block_size);
  LOG1("max_transform_hierarchy_depth_inter : %d\n", sps->max_transform_hierarchy_depth_inter);
  LOG1("max_transform_hierarchy_depth_intra : %d\n", sps->max_transform_hierarchy_depth_intra);
  LOG1("scaling_list_enable_flag : %d\n", sps->scaling_list_enable_flag);

  if (sps->scaling_list_enable_flag) {

    //sps->sps_scaling_list_data_present_flag = get_bits(br,1);
    if (sps->sps_scaling_list_data_present_flag) {

      LOG0("NOT IMPLEMENTED");
      //assert(0);
      //scaling_list_data()
    }
  }

  LOG1("amp_enabled_flag                    : %d\n", sps->amp_enabled_flag);
  LOG1("sample_adaptive_offset_enabled_flag : %d\n", sps->sample_adaptive_offset_enabled_flag);
  LOG1("pcm_enabled_flag                    : %d\n", sps->pcm_enabled_flag);

  if (sps->pcm_enabled_flag) {
    LOG1("pcm_sample_bit_depth_luma     : %d\n", sps->pcm_sample_bit_depth_luma);
    LOG1("pcm_sample_bit_depth_chroma   : %d\n", sps->pcm_sample_bit_depth_chroma);
    LOG1("log2_min_pcm_luma_coding_block_size : %d\n", sps->log2_min_pcm_luma_coding_block_size);
    LOG1("log2_diff_max_min_pcm_luma_coding_block_size : %d\n", sps->log2_diff_max_min_pcm_luma_coding_block_size);
    LOG1("pcm_loop_filter_disable_flag  : %d\n", sps->pcm_loop_filter_disable_flag);
  }

  LOG1("num_short_term_ref_pic_sets : %d\n", sps->num_short_term_ref_pic_sets);

  for (int i = 0; i < sps->num_short_term_ref_pic_sets; i++) {
    LOG1("ref_pic_set[ %2d ]: ",i);
    dump_compact_short_term_ref_pic_set(&sets[i], 16, fh);
  }

  LOG1("long_term_ref_pics_present_flag : %d\n", sps->long_term_ref_pics_present_flag);

  if (sps->long_term_ref_pics_present_flag) {

    LOG1("num_long_term_ref_pics_sps : %d\n", sps->num_long_term_ref_pics_sps);

    for (int i = 0; i < sps->num_long_term_ref_pics_sps; i++ ) {
      LOG3("lt_ref_pic_poc_lsb_sps[%d] : %d   (used_by_curr_pic_lt_sps_flag=%d)\n",
           i, sps->lt_ref_pic_poc_lsb_sps[i], sps->used_by_curr_pic_lt_sps_flag[i]);
    }
  }

  LOG1("sps_temporal_mvp_enabled_flag      : %d\n", sps->sps_temporal_mvp_enabled_flag);
  LOG1("strong_intra_smoothing_enable_flag : %d\n", sps->strong_intra_smoothing_enable_flag);
  LOG1("vui_parameters_present_flag        : %d\n", sps->vui_parameters_present_flag);

  LOG1("CtbSizeY     : %d\n", sps->CtbSizeY);
  LOG1("MinCbSizeY   : %d\n", sps->MinCbSizeY);
  LOG1("MaxCbSizeY   : %d\n", 1<<(sps->log2_min_luma_coding_block_size + sps->log2_diff_max_min_luma_coding_block_size));
  LOG1("MinTBSizeY   : %d\n", 1<<sps->log2_min_transform_block_size);
  LOG1("MaxTBSizeY   : %d\n", 1<<(sps->log2_min_transform_block_size + sps->log2_diff_max_min_transform_block_size));

  return;

  if (sps->vui_parameters_present_flag) {
    assert(false);
    /*
      vui_parameters()

        sps_extension_flag
        u(1)
        if( sps_extension_flag )

          while( more_rbsp_data() )

            sps_extension_data_flag
              u(1)
              rbsp_trailing_bits()
    */
  }
#undef LOG0
#undef LOG1
#undef LOG2
#undef LOG3
  //#endif
}


void free_ref_pic_sets(ref_pic_set** sets)
{
  free(*sets);
  *sets = NULL;
}
