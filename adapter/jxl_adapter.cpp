// Copyright 2022 Nigel Tao.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <vector>

#include "../src/qoir.h"
#include "jxl/decode.h"
#include "jxl/decode_cxx.h"
#include "jxl/encode.h"
#include "jxl/encode_cxx.h"

// ----

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif

// Both fjxl and fpnge declare "struct BitWriter" in the top-level namespace,
// with the same name (and both have a Write(uint32_t, uint64_t) method) but
// different field layout. Without this #define hack, the two can (silently)
// conflict, leading to a crash.
#define BitWriter FJXL_BitWriter
#include "../../libjxl/experimental/fast_lossless/fast_lossless.cc"
#undef BitWriter

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// ----

#ifdef __cplusplus
extern "C" {
#endif

qoir_decode_result           //
my_decode_jxl_lib(           //
    const uint8_t* src_ptr,  //
    const size_t src_len) {
  auto dec = JxlDecoderMake(nullptr);
  if (JXL_DEC_SUCCESS !=
      JxlDecoderSubscribeEvents(dec.get(),
                                JXL_DEC_BASIC_INFO | JXL_DEC_FULL_IMAGE)) {
    qoir_decode_result fail_result = {0};
    fail_result.status_message =
        "#my_decode_jxl_lib: JxlDecoderSubscribeEvents failed";
    return fail_result;
  }

  JxlBasicInfo info;
  JxlDecoderSetInput(dec.get(), src_ptr, src_len);
  JxlDecoderCloseInput(dec.get());

  uint8_t* dst_ptr = nullptr;
  while (true) {
    JxlDecoderStatus status = JxlDecoderProcessInput(dec.get());
    if (status == JXL_DEC_BASIC_INFO) {
      if (JXL_DEC_SUCCESS != JxlDecoderGetBasicInfo(dec.get(), &info)) {
        free(dst_ptr);
        qoir_decode_result fail_result = {0};
        fail_result.status_message =
            "#my_decode_jxl_lib: JxlDecoderGetBasicInfo failed";
        return fail_result;
      }
      continue;

    } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
      uint32_t num_channels = (info.alpha_bits == 0) ? 4 : 3;
      JxlPixelFormat pixel_format = {num_channels, JXL_TYPE_UINT8,
                                     JXL_NATIVE_ENDIAN, 0};
      size_t dst_len;
      if (JXL_DEC_SUCCESS !=
          JxlDecoderImageOutBufferSize(dec.get(), &pixel_format, &dst_len)) {
        free(dst_ptr);
        qoir_decode_result fail_result = {0};
        fail_result.status_message =
            "#my_decode_jxl_lib: JxlDecoderImageOutBufferSize failed";
        return fail_result;
      }
      if (dst_len != num_channels * info.xsize * info.ysize) {
        free(dst_ptr);
        qoir_decode_result fail_result = {0};
        fail_result.status_message =
            "#my_decode_jxl_lib: inconsistent buffer size";
        return fail_result;
      }
      if (dst_ptr) {
        free(dst_ptr);
        dst_ptr = nullptr;
      }
      dst_ptr = (uint8_t*)malloc(dst_len);
      if (!dst_ptr) {
        qoir_decode_result fail_result = {0};
        fail_result.status_message = "#my_decode_jxl_lib: out of memory";
        return fail_result;
      }
      if (JXL_DEC_SUCCESS != JxlDecoderSetImageOutBuffer(
                                 dec.get(), &pixel_format, dst_ptr, dst_len)) {
        free(dst_ptr);
        qoir_decode_result fail_result = {0};
        fail_result.status_message =
            "#my_decode_jxl_lib: JxlDecoderSetImageOutBuffer failed";
        return fail_result;
      }
      continue;

    } else if ((status == JXL_DEC_FULL_IMAGE) || (status == JXL_DEC_SUCCESS)) {
      break;
    }

    free(dst_ptr);
    qoir_decode_result fail_result = {0};
    fail_result.status_message =
        "#my_decode_jxl_lib: JxlDecoderProcessInput failed";
    return fail_result;
  }

  uint32_t num_channels = (info.alpha_bits == 0) ? 4 : 3;
  qoir_decode_result result = {0};
  result.owned_memory = dst_ptr;
  result.dst_pixbuf.pixcfg.pixfmt = (info.alpha_bits == 0)
                                        ? QOIR_PIXEL_FORMAT__RGB
                                        : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  result.dst_pixbuf.pixcfg.width_in_pixels = info.xsize;
  result.dst_pixbuf.pixcfg.height_in_pixels = info.ysize;
  result.dst_pixbuf.data = dst_ptr;
  result.dst_pixbuf.stride_in_bytes = (size_t)num_channels * (size_t)info.xsize;
  return result;
}

static qoir_encode_result           //
my_encode_jxl_lib(                  //
    const uint8_t* png_ptr,         //
    const size_t png_len,           //
    qoir_pixel_buffer* src_pixbuf,  //
    bool lossless) {
  uint32_t num_channels;
  switch (src_pixbuf->pixcfg.pixfmt) {
    case QOIR_PIXEL_FORMAT__RGB:
      num_channels = 3;
      break;
    case QOIR_PIXEL_FORMAT__RGBA_NONPREMUL:
      num_channels = 4;
      break;
    default: {
      qoir_encode_result fail_result = {0};
      fail_result.status_message =
          "#my_encode_jxl_lib: unsupported pixel format";
      return fail_result;
    }
  }
  uint32_t w = src_pixbuf->pixcfg.width_in_pixels;
  uint32_t h = src_pixbuf->pixcfg.height_in_pixels;

  auto enc = JxlEncoderMake(nullptr);
  JxlBasicInfo basic_info;
  JxlEncoderInitBasicInfo(&basic_info);
  basic_info.xsize = w;
  basic_info.ysize = h;
  basic_info.bits_per_sample = 8;
  basic_info.uses_original_profile = JXL_TRUE;
  basic_info.num_color_channels = 3;
  basic_info.num_extra_channels = (num_channels == 4) ? 1 : 0;
  basic_info.alpha_bits = (num_channels == 4) ? 8 : 0;
  if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(enc.get(), &basic_info)) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message =
        "#my_encode_jxl_lib: JxlEncoderSetBasicInfo failed";
    return fail_result;
  }

  JxlColorEncoding color_encoding = {};
  JxlColorEncodingSetToSRGB(&color_encoding, false);
  if (JXL_ENC_SUCCESS !=
      JxlEncoderSetColorEncoding(enc.get(), &color_encoding)) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message =
        "#my_encode_jxl_lib: JxlEncoderSetColorEncoding failed";
    return fail_result;
  }

  JxlEncoderFrameSettings* frame_settings =
      JxlEncoderFrameSettingsCreate(enc.get(), nullptr);
  if (JXL_ENC_SUCCESS != JxlEncoderSetFrameLossless(frame_settings, lossless)) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message =
        "#my_encode_jxl_lib: JxlEncoderSetFrameLossless failed";
    return fail_result;
  }

  JxlPixelFormat pixel_format = {num_channels, JXL_TYPE_UINT8,
                                 JXL_NATIVE_ENDIAN, 0};
  if (JXL_ENC_SUCCESS != JxlEncoderAddImageFrame(frame_settings, &pixel_format,
                                                 src_pixbuf->data,
                                                 num_channels * w * h)) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message =
        "#my_encode_jxl_lib: JxlEncoderAddImageFrame failed";
    return fail_result;
  }
  JxlEncoderCloseInput(enc.get());

  std::vector<uint8_t> dst_vec;
  {
    dst_vec.resize(64);
    uint8_t* next_out = dst_vec.data();
    size_t avail_out = dst_vec.size() - (next_out - dst_vec.data());
    JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;
    while (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
      process_result =
          JxlEncoderProcessOutput(enc.get(), &next_out, &avail_out);
      if (process_result == JXL_ENC_NEED_MORE_OUTPUT) {
        size_t offset = next_out - dst_vec.data();
        dst_vec.resize(dst_vec.size() * 2);
        next_out = dst_vec.data() + offset;
        avail_out = dst_vec.size() - offset;
      }
    }
    dst_vec.resize(next_out - dst_vec.data());
    if (JXL_ENC_SUCCESS != process_result) {
      qoir_encode_result fail_result = {0};
      fail_result.status_message =
          "#my_encode_jxl_lib: JxlEncoderProcessOutput failed";
      return fail_result;
    }
  }

  size_t dst_len = dst_vec.size();
  uint8_t* dst_ptr = (uint8_t*)(malloc(dst_len));
  if (!dst_ptr) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message = "#my_encode_jxl_lib: out of memory";
    return fail_result;
  }
  memcpy(dst_ptr, dst_vec.data(), dst_len);

  qoir_encode_result result = {0};
  result.owned_memory = dst_ptr;
  result.dst_ptr = dst_ptr;
  result.dst_len = dst_len;
  return result;
}

qoir_encode_result           //
my_encode_jxl_lossless_lib(  //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  return my_encode_jxl_lib(png_ptr, png_len, src_pixbuf, true);
}

qoir_encode_result           //
my_encode_jxl_lossy_lib(     //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  return my_encode_jxl_lib(png_ptr, png_len, src_pixbuf, false);
}

qoir_encode_result           //
my_encode_jxl_lossless_fst(  //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  size_t num_channels;
  switch (src_pixbuf->pixcfg.pixfmt) {
    case QOIR_PIXEL_FORMAT__RGB:
      num_channels = 3;
      break;
    case QOIR_PIXEL_FORMAT__RGBA_NONPREMUL:
      num_channels = 4;
      break;
    default: {
      qoir_encode_result fail_result = {0};
      fail_result.status_message =
          "#my_encode_jxl_lossless_fst: unsupported pixel format";
      return fail_result;
    }
  }

  // experimental/fast_lossless/fast_lossless_main.cc says
  // "Effort should be between 0 and 127 (default is 2, more is slower)".
  int default_effort = 2;

  unsigned char* dst_ptr = nullptr;
  size_t dst_len = FastLosslessEncode(
      src_pixbuf->data, src_pixbuf->pixcfg.width_in_pixels,
      src_pixbuf->stride_in_bytes, src_pixbuf->pixcfg.height_in_pixels,
      num_channels, 8, default_effort, &dst_ptr);
  if (dst_len == 0) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message =
        "#my_encode_jxl_lossless_fst: FastLosslessEncode failed";
    return fail_result;
  }
  qoir_encode_result result = {0};
  result.owned_memory = dst_ptr;
  result.dst_ptr = dst_ptr;
  result.dst_len = dst_len;
  return result;
}

#ifdef __cplusplus
}  // extern "C"
#endif
