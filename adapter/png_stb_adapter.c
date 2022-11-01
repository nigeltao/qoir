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
#include "../third_party/stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb/stb_image_write.h"

#ifdef __cplusplus
extern "C" {
#endif

qoir_decode_result           //
my_decode_png_stb(           //
    const uint8_t* src_ptr,  //
    const size_t src_len) {
  int width = 0;
  int height = 0;
  int num_channels = 0;
  if (!stbi_info_from_memory(src_ptr, src_len, &width, &height,
                             &num_channels)) {
    qoir_decode_result fail_result = {0};
    fail_result.status_message =
        "#my_decode_png_stb: stbi_info_from_memory failed";
    return fail_result;
  } else if (num_channels != 3) {
    num_channels = 4;
  }

  int ignored = 0;
  unsigned char* pixbuf_data = stbi_load_from_memory(
      src_ptr, src_len, &ignored, &ignored, &ignored, num_channels);
  if (!pixbuf_data) {
    qoir_decode_result fail_result = {0};
    fail_result.status_message =
        "#my_decode_png_stb: stbi_load_from_memory failed";
    return fail_result;
  } else if ((width > 0xFFFFFF) || (height > 0xFFFFFF)) {
    stbi_image_free(pixbuf_data);
    qoir_decode_result fail_result = {0};
    fail_result.status_message = "#my_decode_png_stb: image is too large";
    return fail_result;
  }

  qoir_decode_result result = {0};
  result.owned_memory = pixbuf_data;
  result.dst_pixbuf.pixcfg.pixfmt = (num_channels == 3)
                                        ? QOIR_PIXEL_FORMAT__RGB
                                        : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  result.dst_pixbuf.pixcfg.width_in_pixels = width;
  result.dst_pixbuf.pixcfg.height_in_pixels = height;
  result.dst_pixbuf.data = (uint8_t*)pixbuf_data;
  result.dst_pixbuf.stride_in_bytes = (size_t)num_channels * (size_t)width;
  return result;
}

static void          //
my_stbi_write_func(  //
    void* context,   //
    void* src_ptr,   //
    int src_len) {
  qoir_encode_result* result = (qoir_encode_result*)context;
  uint8_t* dst_ptr = (uint8_t*)malloc(src_len);
  if (!dst_ptr) {
    result->status_message = "#my_stbi_write_func: out of memory";
    return;
  }
  memcpy(dst_ptr, src_ptr, src_len);
  result->owned_memory = dst_ptr;
  result->dst_ptr = dst_ptr;
  result->dst_len = src_len;
}

qoir_encode_result           //
my_encode_png_stb(           //
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
      fail_result.status_message =
          "#my_encode_png_stb: unsupported pixel format";
      return fail_result;
    }
  }

  qoir_encode_result result = {0};
  if (!stbi_write_png_to_func(&my_stbi_write_func, &result,
                              src_pixbuf->pixcfg.width_in_pixels,
                              src_pixbuf->pixcfg.height_in_pixels, num_channels,
                              src_pixbuf->data, src_pixbuf->stride_in_bytes)) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message = "#my_encode_png_stb: stbi_write_png failed";
    return fail_result;
  }
  return result;
}

#ifdef __cplusplus
}  // extern "C"
#endif
