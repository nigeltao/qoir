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
#include "png.h"

#ifdef __cplusplus
extern "C" {
#endif

qoir_decode_result           //
my_decode_png_libpng(        //
    const uint8_t* src_ptr,  //
    const size_t src_len) {
  // Use the STB library just to get the num_channels. There doesn't seem to be
  // an easy way to do this with libpng's "simplified API".
  int width = 0;
  int height = 0;
  int num_channels = 0;
  if (!stbi_info_from_memory(src_ptr, src_len, &width, &height,
                             &num_channels)) {
    qoir_decode_result fail_result = {0};
    fail_result.status_message =
        "#my_decode_png_libpng: stbi_info_from_memory failed";
    return fail_result;
  } else if (num_channels != 3) {
    num_channels = 4;
  }

  png_image image = {0};
  image.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_memory(&image, src_ptr, src_len)) {
    qoir_decode_result fail_result = {0};
    fail_result.status_message =
        "#my_decode_png_libpng: png_image_begin_read_from_memory failed";
    return fail_result;
  }
  image.format = (num_channels == 3) ? PNG_FORMAT_RGB : PNG_FORMAT_RGBA;

  size_t dst_len = PNG_IMAGE_SIZE(image);
  uint8_t* dst_ptr = (uint8_t*)malloc(dst_len);
  if (!dst_ptr) {
    png_image_free(&image);
    qoir_decode_result fail_result = {0};
    fail_result.status_message = "#my_decode_png_libpng: out of memory";
    return fail_result;
  }

  if (!png_image_finish_read(&image, NULL, dst_ptr, 0, NULL)) {
    png_image_free(&image);
    free(dst_ptr);
    qoir_decode_result fail_result = {0};
    fail_result.status_message =
        "#my_decode_png_libpng: png_image_finish_read failed";
    return fail_result;
  }

  qoir_decode_result result = {0};
  result.owned_memory = dst_ptr;
  result.dst_pixbuf.pixcfg.pixfmt = (num_channels == 3)
                                        ? QOIR_PIXEL_FORMAT__RGB
                                        : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  result.dst_pixbuf.pixcfg.width_in_pixels = image.width;
  result.dst_pixbuf.pixcfg.height_in_pixels = image.height;
  result.dst_pixbuf.data = dst_ptr;
  result.dst_pixbuf.stride_in_bytes =
      (size_t)num_channels * (size_t)image.width;
  return result;
}

qoir_encode_result           //
my_encode_png_libpng(        //
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
          "#my_encode_png_libpng: unsupported pixel format";
      return fail_result;
    }
  }

  static const size_t max_dst_len = 67108864;  // 64 MiB.
  uint8_t* dst_ptr = (uint8_t*)malloc(max_dst_len);
  if (!dst_ptr) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message = "#my_encode_png_libpng: out of memory";
    return fail_result;
  }

  png_image image = {0};
  image.version = PNG_IMAGE_VERSION;
  image.width = src_pixbuf->pixcfg.width_in_pixels;
  image.height = src_pixbuf->pixcfg.height_in_pixels;
  image.format = (num_channels == 3) ? PNG_FORMAT_RGB : PNG_FORMAT_RGBA;

  size_t dst_len = max_dst_len;
  if (!png_image_write_to_memory(&image, dst_ptr, &dst_len, 0, src_pixbuf->data,
                                 src_pixbuf->stride_in_bytes, NULL)) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message =
        "#my_encode_png_libpng: png_image_write_to_memory failed";
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
