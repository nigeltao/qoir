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
#include "../third_party/fpng/fpng.cpp"

#ifdef __cplusplus
extern "C" {
#endif

static void  //
my_fpng_init() {
  static int initialized = 0;
  if (!initialized) {
    initialized = 1;
    fpng::fpng_init();
  }
}

qoir_decode_result           //
my_decode_png_fpng(          //
    const uint8_t* src_ptr,  //
    const size_t src_len) {
  my_fpng_init();

  uint32_t width;
  uint32_t height;
  uint32_t num_channels;

  // Call fpng::fpng_get_info just to load the num_channels variable. We can't
  // call fpng::fpng_decode_memory with the final desired_channels arg being 0.
  if (fpng::fpng_get_info(src_ptr, src_len, width, height, num_channels) !=
      fpng::FPNG_DECODE_SUCCESS) {
    qoir_decode_result fail_result = {0};
    fail_result.status_message = "#my_decode_png_fpng: fpng_get_info failed";
    return fail_result;
  }

  std::vector<uint8_t> dst_vec;
  if (fpng::fpng_decode_memory(src_ptr, src_len, dst_vec, width, height,
                               num_channels,
                               num_channels) != fpng::FPNG_DECODE_SUCCESS) {
    qoir_decode_result fail_result = {0};
    fail_result.status_message =
        "#my_decode_png_fpng: fpng_decode_memory failed";
    return fail_result;
  }

  size_t dst_len = dst_vec.size();
  uint8_t* dst_ptr = (uint8_t*)(malloc(dst_len));
  if (!dst_ptr) {
    qoir_decode_result fail_result = {0};
    fail_result.status_message = "#my_decode_png_fpng: out of memory";
    return fail_result;
  }
  memcpy(dst_ptr, dst_vec.data(), dst_len);

  qoir_decode_result result = {0};
  result.owned_memory = dst_ptr;
  result.dst_pixbuf.pixcfg.pixfmt = (num_channels == 3)
                                        ? QOIR_PIXEL_FORMAT__RGB
                                        : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  result.dst_pixbuf.pixcfg.width_in_pixels = width;
  result.dst_pixbuf.pixcfg.height_in_pixels = height;
  result.dst_pixbuf.data = dst_ptr;
  result.dst_pixbuf.stride_in_bytes = (size_t)num_channels * (size_t)width;
  return result;
}

qoir_encode_result           //
my_encode_png_fpng(          //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  my_fpng_init();

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
          "#my_encode_png_fpng: unsupported pixel format";
      return fail_result;
    }
  }

  std::vector<uint8_t> dst_vec;
  if (!fpng::fpng_encode_image_to_memory(
          src_pixbuf->data, src_pixbuf->pixcfg.width_in_pixels,
          src_pixbuf->pixcfg.height_in_pixels, num_channels, dst_vec)) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message =
        "#my_encode_png_fpng: fpng_encode_image_to_memory failed";
    return fail_result;
  }

  size_t dst_len = dst_vec.size();
  uint8_t* dst_ptr = (uint8_t*)(malloc(dst_len));
  if (!dst_ptr) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message = "#my_encode_png_fpng: out of memory";
    return fail_result;
  }
  memcpy(dst_ptr, dst_vec.data(), dst_len);

  qoir_encode_result result = {0};
  result.owned_memory = dst_ptr;
  result.dst_ptr = dst_ptr;
  result.dst_len = dst_len;
  return result;
}

#ifdef __cplusplus
}  // extern "C"
#endif
