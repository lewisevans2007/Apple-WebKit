/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef WEBRTC_USE_H265
#include "common_video/h265/h265_sps_parser.h"

#include <memory>
#include <vector>

#include "common_video/h265/h265_common.h"
#include "rtc_base/bit_buffer.h"
#include "rtc_base/logging.h"

namespace {
typedef absl::optional<webrtc::H265SpsParser::SpsState> OptionalSps;
typedef absl::optional<webrtc::H265SpsParser::ShortTermRefPicSet>
    OptionalShortTermRefPicSet;
}  // namespace

namespace webrtc {

#if WEBRTC_WEBKIT_BUILD
const uint32_t kMaxSPSLongTermRefPics = 32;
const uint32_t kMaxSPSPics = 16;
const uint32_t kMaxSPSShortTermRefPics = 64;
#endif

H265SpsParser::SpsState::SpsState() = default;

H265SpsParser::ShortTermRefPicSet::ShortTermRefPicSet() = default;

// General note: this is based off the 06/2019 version of the H.265 standard.
// You can find it on this page:
// http://www.itu.int/rec/T-REC-H.265

// Unpack RBSP and parse SPS state from the supplied buffer.
absl::optional<H265SpsParser::SpsState> H265SpsParser::ParseSps(
    const uint8_t* data,
    size_t length) {
  return ParseSpsInternal(H265::ParseRbsp(data, length));
}

bool H265SpsParser::ParseScalingListData(BitstreamReader& reader) {
  uint32_t scaling_list_pred_mode_flag[4][6];
  uint32_t scaling_list_pred_matrix_id_delta[4][6];
  int32_t scaling_list_dc_coef_minus8[4][6];
  int32_t scaling_list[4][6][64];
  for (int size_id = 0; size_id < 4; size_id++) {
    for (int matrix_id = 0; matrix_id < 6;
         matrix_id += (size_id == 3) ? 3 : 1) {
      // scaling_list_pred_mode_flag: u(1)
      scaling_list_pred_mode_flag[size_id][matrix_id] = reader.Read<bool>();
      if (!scaling_list_pred_mode_flag[size_id][matrix_id]) {
        // scaling_list_pred_matrix_id_delta: ue(v)
        scaling_list_pred_matrix_id_delta[size_id][matrix_id] =
            reader.ReadExponentialGolomb();
      } else {
        int32_t next_coef = 8;
        uint32_t coef_num = std::min(64, 1 << (4 + (size_id << 1)));
        if (size_id > 1) {
          // scaling_list_dc_coef_minus8: se(v)
          scaling_list_dc_coef_minus8[size_id - 2][matrix_id] =
              reader.ReadSignedExponentialGolomb();
          next_coef = scaling_list_dc_coef_minus8[size_id - 2][matrix_id];
        }
        for (uint32_t i = 0; i < coef_num; i++) {
          // scaling_list_delta_coef: se(v)
          int32_t scaling_list_delta_coef =
              reader.ReadSignedExponentialGolomb();
          next_coef = (next_coef + scaling_list_delta_coef + 256) % 256;
          scaling_list[size_id][matrix_id][i] = next_coef;
        }
      }
    }
  }

#if WEBRTC_WEBKIT_BUILD
  if (!reader.Ok()) {
    return false;
  }
#endif

  return true;
}

absl::optional<H265SpsParser::ShortTermRefPicSet>
H265SpsParser::ParseShortTermRefPicSet(
    uint32_t st_rps_idx,
    uint32_t num_short_term_ref_pic_sets,
    const std::vector<H265SpsParser::ShortTermRefPicSet>&
        short_term_ref_pic_set,
    H265SpsParser::SpsState& sps,
    BitstreamReader& reader) {
  H265SpsParser::ShortTermRefPicSet ref_pic_set;

  bool inter_ref_pic_set_prediction_flag = false;
  if (st_rps_idx != 0) {
    // inter_ref_pic_set_prediction_flag: u(1)
    inter_ref_pic_set_prediction_flag = reader.Read<bool>();
  }
  if (inter_ref_pic_set_prediction_flag) {
    uint32_t delta_idx_minus1 = 0;
    if (st_rps_idx == num_short_term_ref_pic_sets) {
      // delta_idx_minus1: ue(v)
      delta_idx_minus1 = reader.ReadExponentialGolomb();
    }
    // delta_rps_sign: u(1)
    reader.ConsumeBits(1);
    // abs_delta_rps_minus1: ue(v)
    reader.ReadExponentialGolomb();
    uint32_t ref_rps_idx = st_rps_idx - (delta_idx_minus1 + 1);
    uint32_t num_delta_pocs = 0;
    if (short_term_ref_pic_set[ref_rps_idx].inter_ref_pic_set_prediction_flag) {
      auto& used_by_curr_pic_flag =
          short_term_ref_pic_set[ref_rps_idx].used_by_curr_pic_flag;
      auto& use_delta_flag = short_term_ref_pic_set[ref_rps_idx].use_delta_flag;
      if (used_by_curr_pic_flag.size() != use_delta_flag.size()) {
        return OptionalShortTermRefPicSet();
      }
      for (uint32_t i = 0; i < used_by_curr_pic_flag.size(); i++) {
        if (used_by_curr_pic_flag[i] || use_delta_flag[i]) {
          num_delta_pocs++;
        }
      }
    } else {
      num_delta_pocs = short_term_ref_pic_set[ref_rps_idx].num_negative_pics +
                       short_term_ref_pic_set[ref_rps_idx].num_positive_pics;
    }
   ref_pic_set.used_by_curr_pic_flag.resize(num_delta_pocs + 1, 0);
    ref_pic_set.use_delta_flag.resize(num_delta_pocs + 1, 1);
    for (uint32_t j = 0; j <= num_delta_pocs; j++) {
      // used_by_curr_pic_flag: u(1)
      ref_pic_set.used_by_curr_pic_flag[j] = reader.Read<bool>();
      if (!ref_pic_set.used_by_curr_pic_flag[j]) {
        // use_delta_flag: u(1)
        ref_pic_set.use_delta_flag[j] = reader.Read<bool>();
      }
    }
  } else {
    // num_negative_pics: ue(v)
    ref_pic_set.num_negative_pics = reader.ReadExponentialGolomb();
    // num_positive_pics: ue(v)
    ref_pic_set.num_positive_pics = reader.ReadExponentialGolomb();
#if WEBRTC_WEBKIT_BUILD
    if (!reader.Ok() || ref_pic_set.num_negative_pics > kMaxSPSPics || ref_pic_set.num_positive_pics > kMaxSPSPics
        || (ref_pic_set.num_negative_pics + ref_pic_set.num_positive_pics) > kMaxSPSPics) {
      return absl::nullopt;
    }
#endif

    ref_pic_set.delta_poc_s0_minus1.resize(ref_pic_set.num_negative_pics, 0);
    ref_pic_set.used_by_curr_pic_s0_flag.resize(ref_pic_set.num_negative_pics,
                                                0);
    for (uint32_t i = 0; i < ref_pic_set.num_negative_pics; i++) {
      // delta_poc_s0_minus1: ue(v)
      ref_pic_set.delta_poc_s0_minus1[i] = reader.ReadExponentialGolomb();
      // used_by_curr_pic_s0_flag: u(1)
      ref_pic_set.used_by_curr_pic_s0_flag[i] = reader.Read<bool>();
    }
    ref_pic_set.delta_poc_s1_minus1.resize(ref_pic_set.num_positive_pics, 0);
    ref_pic_set.used_by_curr_pic_s1_flag.resize(ref_pic_set.num_positive_pics,
                                                0);
    for (uint32_t i = 0; i < ref_pic_set.num_positive_pics; i++) {
      // delta_poc_s1_minus1: ue(v)
      ref_pic_set.delta_poc_s1_minus1[i] = reader.ReadExponentialGolomb();
      // used_by_curr_pic_s1_flag: u(1)
      ref_pic_set.used_by_curr_pic_s1_flag[i] = reader.Read<bool>();
    }
  }

#if WEBRTC_WEBKIT_BUILD
  if (!reader.Ok()) {
    return absl::nullopt;
  }
#endif

  return OptionalShortTermRefPicSet(ref_pic_set);
}

absl::optional<H265SpsParser::SpsState> H265SpsParser::ParseSpsInternal(
    rtc::ArrayView<const uint8_t> buffer) {
  BitstreamReader reader(buffer);

  // Now, we need to use a bit buffer to parse through the actual HEVC SPS
  // format. See Section 7.3.2.2.1 ("General sequence parameter set data
  // syntax") of the H.265 standard for a complete description.
  // Since we only care about resolution, we ignore the majority of fields, but
  // we still have to actively parse through a lot of the data, since many of
  // the fields have variable size.
  // We're particularly interested in:
  // chroma_format_idc -> affects crop units
  // pic_{width,height}_* -> resolution of the frame in macroblocks (16x16).
  // frame_crop_*_offset -> crop information
  SpsState sps;

  // sps_video_parameter_set_id: u(4)
  uint32_t sps_video_parameter_set_id = 0;
  sps_video_parameter_set_id = reader.ReadBits(4);
  // sps_max_sub_layers_minus1: u(3)
  uint32_t sps_max_sub_layers_minus1 = 0;
  sps_max_sub_layers_minus1 = reader.ReadBits(3);
  sps.sps_max_sub_layers_minus1 = sps_max_sub_layers_minus1;
  sps.sps_max_dec_pic_buffering_minus1.resize(sps_max_sub_layers_minus1 + 1, 0);
  // sps_temporal_id_nesting_flag: u(1)
  reader.ConsumeBits(1);
  // profile_tier_level(1, sps_max_sub_layers_minus1). We are acutally not
  // using them, so read/skip over it.
  // general_profile_space+general_tier_flag+general_prfile_idc: u(8)
  reader.ConsumeBits(8);
  // general_profile_compatabilitiy_flag[32]
  reader.ConsumeBits(32);
  // general_progressive_source_flag + interlaced_source_flag+
  // non-packed_constraint flag + frame_only_constraint_flag: u(4)
  reader.ConsumeBits(4);
  // general_profile_idc decided flags or reserved.  u(43)
  reader.ConsumeBits(43);
  // general_inbld_flag or reserved 0: u(1)
  reader.ConsumeBits(1);
  // general_level_idc: u(8)
  reader.ConsumeBits(8);
  // if max_sub_layers_minus1 >=1, read the sublayer profile information
  std::vector<uint32_t> sub_layer_profile_present_flags;
  std::vector<uint32_t> sub_layer_level_present_flags;
  uint32_t sub_layer_profile_present = 0;
  uint32_t sub_layer_level_present = 0;
  for (uint32_t i = 0; i < sps_max_sub_layers_minus1; i++) {
    // sublayer_profile_present_flag and sublayer_level_presnet_flag:  u(2)
    sub_layer_profile_present = reader.Read<bool>();
    sub_layer_level_present = reader.Read<bool>();
    sub_layer_profile_present_flags.push_back(sub_layer_profile_present);
    sub_layer_level_present_flags.push_back(sub_layer_level_present);
  }
  if (sps_max_sub_layers_minus1 > 0) {
    for (uint32_t j = sps_max_sub_layers_minus1; j < 8; j++) {
      // reserved 2 bits: u(2)
      reader.ConsumeBits(2);
    }
  }
  for (uint32_t k = 0; k < sps_max_sub_layers_minus1; k++) {
    if (sub_layer_profile_present_flags[k]) {  //
      // sub_layer profile_space/tier_flag/profile_idc. ignored. u(8)
      reader.ConsumeBits(8);
      // profile_compatability_flag:  u(32)
      reader.ConsumeBits(32);
      // sub_layer progressive_source_flag/interlaced_source_flag/
      // non_packed_constraint_flag/frame_only_constraint_flag: u(4)
      reader.ConsumeBits(4);
      // following 43-bits are profile_idc specific. We simply read/skip it.
      // u(43)
      reader.ConsumeBits(43);
      // 1-bit profile_idc specific inbld flag.  We simply read/skip it. u(1)
      reader.ConsumeBits(1);
    }
    if (sub_layer_level_present_flags[k]) {
      // sub_layer_level_idc: u(8)
      reader.ConsumeBits(8);
    }
  }
  // sps_seq_parameter_set_id: ue(v)
  sps.id = reader.ReadExponentialGolomb();
  // chrome_format_idc: ue(v)
  sps.chroma_format_idc = reader.ReadExponentialGolomb();
  if (sps.chroma_format_idc == 3) {
    // seperate_colour_plane_flag: u(1)
    sps.separate_colour_plane_flag = reader.Read<bool>();
  }
  uint32_t pic_width_in_luma_samples = 0;
  uint32_t pic_height_in_luma_samples = 0;
  // pic_width_in_luma_samples: ue(v)
  pic_width_in_luma_samples = reader.ReadExponentialGolomb();
  // pic_height_in_luma_samples: ue(v)
  pic_height_in_luma_samples = reader.ReadExponentialGolomb();
  // conformance_window_flag: u(1)
  bool conformance_window_flag = reader.Read<bool>();

  uint32_t conf_win_left_offset = 0;
  uint32_t conf_win_right_offset = 0;
  uint32_t conf_win_top_offset = 0;
  uint32_t conf_win_bottom_offset = 0;
  if (conformance_window_flag) {
    // conf_win_left_offset: ue(v)
    conf_win_left_offset = reader.ReadExponentialGolomb();
    // conf_win_right_offset: ue(v)
    conf_win_right_offset = reader.ReadExponentialGolomb();
    // conf_win_top_offset: ue(v)
    conf_win_top_offset = reader.ReadExponentialGolomb();
    // conf_win_bottom_offset: ue(v)
    conf_win_bottom_offset = reader.ReadExponentialGolomb();
  }

#if WEBRTC_WEBKIT_BUILD
  // log2_max_pic_order_cnt_lsb_minus4 is used with
  // BitstreamReader::ConsumeBits, which can read at most INT_MAX bits at
  // a time. We also have to avoid overflow when adding 4 to the on-wire
  // golomb value, e.g., for evil input data.
  const uint32_t kMaxLog2LsbMinus4 = std::numeric_limits<int>::max() - 4;
#endif

  // bit_depth_luma_minus8: ue(v)
  reader.ReadExponentialGolomb();
  // bit_depth_chroma_minus8: ue(v)
  reader.ReadExponentialGolomb();
  // log2_max_pic_order_cnt_lsb_minus4: ue(v)
  sps.log2_max_pic_order_cnt_lsb_minus4 = reader.ReadExponentialGolomb();
#if WEBRTC_WEBKIT_BUILD
  if (!reader.Ok() || sps.log2_max_pic_order_cnt_lsb_minus4 > kMaxLog2LsbMinus4) {
    return absl::nullopt;
  }
#endif
  uint32_t sps_sub_layer_ordering_info_present_flag = 0;
  // sps_sub_layer_ordering_info_present_flag: u(1)
  sps_sub_layer_ordering_info_present_flag = reader.Read<bool>();
  for (uint32_t i = (sps_sub_layer_ordering_info_present_flag != 0)
                        ? 0
                        : sps_max_sub_layers_minus1;
       i <= sps_max_sub_layers_minus1; i++) {
    // sps_max_dec_pic_buffering_minus1: ue(v)
    sps.sps_max_dec_pic_buffering_minus1[i] = reader.ReadExponentialGolomb();
    // sps_max_num_reorder_pics: ue(v)
    reader.ReadExponentialGolomb();
    // sps_max_latency_increase_plus1: ue(v)
    reader.ReadExponentialGolomb();
  }
  // log2_min_luma_coding_block_size_minus3: ue(v)
  sps.log2_min_luma_coding_block_size_minus3 = reader.ReadExponentialGolomb();
  // log2_diff_max_min_luma_coding_block_size: ue(v)
  sps.log2_diff_max_min_luma_coding_block_size = reader.ReadExponentialGolomb();
  // log2_min_luma_transform_block_size_minus2: ue(v)
  reader.ReadExponentialGolomb();
  // log2_diff_max_min_luma_transform_block_size: ue(v)
  reader.ReadExponentialGolomb();
  // max_transform_hierarchy_depth_inter: ue(v)
  reader.ReadExponentialGolomb();
  // max_transform_hierarchy_depth_intra: ue(v)
  reader.ReadExponentialGolomb();
  // scaling_list_enabled_flag: u(1)
  bool scaling_list_enabled_flag = reader.Read<bool>();
  if (scaling_list_enabled_flag) {
    // sps_scaling_list_data_present_flag: u(1)
    bool sps_scaling_list_data_present_flag = reader.Read<bool>();
    if (sps_scaling_list_data_present_flag) {
      // scaling_list_data()
      if (!ParseScalingListData(reader)) {
        return OptionalSps();
      }
    }
  }

  // amp_enabled_flag: u(1)
  reader.ConsumeBits(1);
  // sample_adaptive_offset_enabled_flag: u(1)
  sps.sample_adaptive_offset_enabled_flag = reader.Read<bool>();
  // pcm_enabled_flag: u(1)
  bool pcm_enabled_flag = reader.Read<bool>();
  if (pcm_enabled_flag) {
    // pcm_sample_bit_depth_luma_minus1: u(4)
    reader.ConsumeBits(4);
    // pcm_sample_bit_depth_chroma_minus1: u(4)
    reader.ConsumeBits(4);
    // log2_min_pcm_luma_coding_block_size_minus3: ue(v)
    reader.ReadExponentialGolomb();
    // log2_diff_max_min_pcm_luma_coding_block_size: ue(v)
    reader.ReadExponentialGolomb();
    // pcm_loop_filter_disabled_flag: u(1)
    reader.ConsumeBits(1);
  }

  // num_short_term_ref_pic_sets: ue(v)
  sps.num_short_term_ref_pic_sets = reader.ReadExponentialGolomb();
#if WEBRTC_WEBKIT_BUILD
    if (!reader.Ok() || sps.num_short_term_ref_pic_sets > kMaxSPSShortTermRefPics) {
      return absl::nullopt;
    }
#endif
  sps.short_term_ref_pic_set.resize(sps.num_short_term_ref_pic_sets);
  for (uint32_t st_rps_idx = 0; st_rps_idx < sps.num_short_term_ref_pic_sets;
       st_rps_idx++) {
    // st_ref_pic_set()
    OptionalShortTermRefPicSet ref_pic_set =
        ParseShortTermRefPicSet(st_rps_idx, sps.num_short_term_ref_pic_sets,
                                sps.short_term_ref_pic_set, sps, reader);
    if (ref_pic_set) {
      sps.short_term_ref_pic_set[st_rps_idx] = *ref_pic_set;
    } else {
      return OptionalSps();
    }
  }

  // long_term_ref_pics_present_flag: u(1)
  sps.long_term_ref_pics_present_flag = reader.Read<bool>();
  if (sps.long_term_ref_pics_present_flag) {
    // num_long_term_ref_pics_sps: ue(v)
    sps.num_long_term_ref_pics_sps = reader.ReadExponentialGolomb();
#if WEBRTC_WEBKIT_BUILD
    if (!reader.Ok() || sps.num_long_term_ref_pics_sps > kMaxSPSLongTermRefPics) {
      return absl::nullopt;
    }
#endif
    sps.used_by_curr_pic_lt_sps_flag.resize(sps.num_long_term_ref_pics_sps, 0);
    for (uint32_t i = 0; i < sps.num_long_term_ref_pics_sps; i++) {
      // lt_ref_pic_poc_lsb_sps: u(v)
      uint32_t lt_ref_pic_poc_lsb_sps_bits =
          sps.log2_max_pic_order_cnt_lsb_minus4 + 4;
      reader.ConsumeBits(lt_ref_pic_poc_lsb_sps_bits);
      // used_by_curr_pic_lt_sps_flag: u(1)
      sps.used_by_curr_pic_lt_sps_flag[i] = reader.Read<bool>();
    }
  }

  // sps_temporal_mvp_enabled_flag: u(1)
  sps.sps_temporal_mvp_enabled_flag = reader.Read<bool>();

  // Far enough! We don't use the rest of the SPS.
#if WEBRTC_WEBKIT_BUILD
  if (!reader.Ok()) {
    return absl::nullopt;
  }
#endif

  sps.vps_id = sps_video_parameter_set_id;

  sps.pic_width_in_luma_samples = pic_width_in_luma_samples;
  sps.pic_height_in_luma_samples = pic_height_in_luma_samples;

  // Start with the resolution determined by the pic_width/pic_height fields.
  sps.width = pic_width_in_luma_samples;
  sps.height = pic_height_in_luma_samples;

  if (conformance_window_flag) {
   int sub_width_c =
        ((1 == sps.chroma_format_idc) || (2 == sps.chroma_format_idc)) &&
                (0 == sps.separate_colour_plane_flag)
            ? 2
            : 1;
    int sub_height_c =
        (1 == sps.chroma_format_idc) && (0 == sps.separate_colour_plane_flag)
            ? 2
            : 1;
    // the offset includes the pixel within conformance window. so don't need to
    // +1 as per spec
    sps.width -= sub_width_c * (conf_win_right_offset + conf_win_left_offset);
    sps.height -= sub_height_c * (conf_win_top_offset + conf_win_bottom_offset);
  }

#ifndef WEBRTC_WEBKIT_BUILD
  if (!reader.Ok()) {
    return absl::nullopt;
  }
#endif

  return OptionalSps(sps);
}

}  // namespace webrtc
#endif // WEBRTC_USE_H265
