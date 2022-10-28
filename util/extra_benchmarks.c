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

#ifndef INCLUDE_GUARD_UTIL_EXTRA_BENCHMARKS
#define INCLUDE_GUARD_UTIL_EXTRA_BENCHMARKS

// ----

#define QOI_IMPLEMENTATION
#include "../third_party/qoi/qoi.h"

static qoir_decode_result    //
my_decode_qoi(               //
    const uint8_t* src_ptr,  //
    const size_t src_len) {
  qoi_desc desc;
  void* ptr = qoi_decode(src_ptr, src_len, &desc, 0);
  if (!ptr) {
    qoir_decode_result fail_result = {0};
    fail_result.status_message = "#my_decode_qoi: qoi_decode failed";
    return fail_result;
  }

  qoir_decode_result result = {0};
  result.owned_memory = ptr;
  result.dst_pixbuf.pixcfg.pixfmt = (desc.channels == 3)
                                        ? QOIR_PIXEL_FORMAT__RGB
                                        : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  result.dst_pixbuf.pixcfg.width_in_pixels = desc.width;
  result.dst_pixbuf.pixcfg.height_in_pixels = desc.height;
  result.dst_pixbuf.data = (uint8_t*)ptr;
  result.dst_pixbuf.stride_in_bytes =
      (size_t)desc.channels * (size_t)desc.width;
  return result;
}

static qoir_encode_result    //
my_encode_qoi(               //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  int num_channels;
  switch (src_pixbuf->pixcfg.pixfmt) {
    case QOIR_PIXEL_FORMAT__RGB:
      num_channels = 3;
      break;
    case QOIR_PIXEL_FORMAT__RGBA_NONPREMUL:
      num_channels = 4;
      break;
    default: {
      qoir_encode_result fail_result = {0};
      fail_result.status_message = "#my_encode_qoi: unsupported pixel format";
      return fail_result;
    }
  }

  qoi_desc desc;
  desc.width = src_pixbuf->pixcfg.width_in_pixels;
  desc.height = src_pixbuf->pixcfg.height_in_pixels;
  desc.channels = num_channels;
  desc.colorspace = QOI_SRGB;

  int len;
  void* ptr = qoi_encode(src_pixbuf->data, &desc, &len);
  if (!ptr) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message = "#my_encode_qoi: qoi_encode failed";
    return fail_result;
  }

  qoir_encode_result result = {0};
  result.owned_memory = ptr;
  result.dst_ptr = ptr;
  result.dst_len = len;
  return result;
}

#endif  // INCLUDE_GUARD_ETC
