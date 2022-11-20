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

#include "../src/qoir.h"
#include "../third_party/fpnge/fpnge.cc"

#ifdef __cplusplus
extern "C" {
#endif

extern const char error_not_implemented[];

qoir_decode_result           //
my_decode_png_fpnge(         //
    const uint8_t* src_ptr,  //
    const size_t src_len) {
  qoir_decode_result result = {0};
  result.status_message = error_not_implemented;
  return result;
}

qoir_encode_result           //
my_encode_png_fpnge(         //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
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
          "#my_encode_png_fpnge: unsupported pixel format";
      return fail_result;
    }
  }
  const size_t bytes_per_channel = 1;

  size_t max_dst_len = FPNGEOutputAllocSize(
      bytes_per_channel, num_channels, src_pixbuf->pixcfg.width_in_pixels,
      src_pixbuf->pixcfg.height_in_pixels);
  uint8_t* dst_ptr = (uint8_t*)malloc(max_dst_len);
  if (!dst_ptr) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message = "#my_encode_png_fpnge: out of memory";
    return fail_result;
  }

  size_t dst_len = FPNGEEncode(
      bytes_per_channel, num_channels, src_pixbuf->data,
      src_pixbuf->pixcfg.width_in_pixels, src_pixbuf->stride_in_bytes,
      src_pixbuf->pixcfg.height_in_pixels, dst_ptr, nullptr);

  qoir_encode_result result = {0};
  result.owned_memory = dst_ptr;
  result.dst_ptr = dst_ptr;
  result.dst_len = dst_len;
  return result;
}

#ifdef __cplusplus
}  // extern "C"
#endif
