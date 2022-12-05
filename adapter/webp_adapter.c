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
#include "webp/decode.h"
#include "webp/encode.h"

#ifdef __cplusplus
extern "C" {
#endif

qoir_decode_result           //
my_decode_png_libpng(        //
    const uint8_t* src_ptr,  //
    const size_t src_len);

qoir_encode_result           //
my_encode_png_libpng(        //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

// ----

qoir_decode_result           //
my_decode_webp(              //
    const uint8_t* src_ptr,  //
    const size_t src_len) {
  // The WebP file format cannot represent an image dimension greater than ((1
  // << 14) - 1) = 16383, or 16384 for lossless. Use PNG/libpng instead.
  if ((src_len > 0) && (src_ptr[0] == 0x89)) {
    return my_decode_png_libpng(src_ptr, src_len);
  }

  int width;
  int height;
  uint8_t* tmp_ptr = WebPDecodeRGBA(src_ptr, src_len, &width, &height);
  if (!tmp_ptr) {
    qoir_decode_result result = {0};
    result.status_message = "#my_decode_webp: WebPDecodeRGBA failed";
    return result;
  }
  // TODO: use 3 (not 4) bytes per pixel if opaque, consistent with the other
  // adapters. The one-shot WebPDecodeEtc functions don't allow that, though.
  uint64_t dst_len = 4 * (uint64_t)width * (uint64_t)height;
  if (dst_len > SIZE_MAX) {
    WebPFree(tmp_ptr);
    qoir_decode_result fail_result = {0};
    fail_result.status_message = "#my_decode_webp: image is too large";
    return fail_result;
  }

  uint8_t* dst_ptr = (uint8_t*)malloc(dst_len);
  if (!dst_ptr) {
    WebPFree(tmp_ptr);
    qoir_decode_result fail_result = {0};
    fail_result.status_message = "#my_decode_webp: out of memory";
    return fail_result;
  }
  memcpy(dst_ptr, tmp_ptr, dst_len);
  WebPFree(tmp_ptr);

  qoir_decode_result result = {0};
  result.owned_memory = dst_ptr;
  result.dst_pixbuf.pixcfg.pixfmt = QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  result.dst_pixbuf.pixcfg.width_in_pixels = width;
  result.dst_pixbuf.pixcfg.height_in_pixels = height;
  result.dst_pixbuf.data = dst_ptr;
  result.dst_pixbuf.stride_in_bytes = 4 * (size_t)width;
  return result;
}

typedef size_t               //
    (*webp_encode_func)(     //
        const uint8_t* rgb,  //
        int width,           //
        int height,          //
        int stride,          //
        uint8_t** output);

static qoir_encode_result               //
my_encode_webp(                         //
    const uint8_t* png_ptr,             //
    const size_t png_len,               //
    qoir_pixel_buffer* src_pixbuf,      //
    webp_encode_func encode_rgb_func,   //
    webp_encode_func encode_rgba_func,  //
    bool lossy2) {
  // The WebP file format cannot represent an image dimension greater than ((1
  // << 14) - 1) = 16383, or 16384 for lossless. Use PNG/libpng instead.
  const uint32_t w = src_pixbuf->pixcfg.width_in_pixels;
  const uint32_t h = src_pixbuf->pixcfg.height_in_pixels;
  if ((w > 16383) || (h > 16383)) {
    return my_encode_png_libpng(png_ptr, png_len, src_pixbuf);
  }

  void* to_free = NULL;
  const uint8_t* src_ptr = src_pixbuf->data;
  size_t src_stride = src_pixbuf->stride_in_bytes;
  if (lossy2) {
    uint32_t bytes_per_pixel =
        qoir_pixel_format__bytes_per_pixel(src_pixbuf->pixcfg.pixfmt);
    uint64_t len = w * h * (uint64_t)bytes_per_pixel;
    uint8_t* ptr = (len <= SIZE_MAX) ? (uint8_t*)malloc(len) : NULL;
    if (!ptr) {
      qoir_encode_result fail_result = {0};
      fail_result.status_message = "#my_encode_webp: out of memory";
      return fail_result;
    }
    size_t stride = w * bytes_per_pixel;
    for (uint32_t y = 0; y < h; y++) {
      const uint8_t* s = src_ptr + (y * src_stride);
      uint8_t* d = ptr + (y * stride);
      for (uint32_t i = 0; i < (w * bytes_per_pixel); i++) {
        uint8_t value = (*s++) >> 2;
        *d++ = (value << 2) | (value >> 4);
      }
    }
    to_free = ptr;
    src_ptr = ptr;
    src_stride = w * bytes_per_pixel;
  }

  uint8_t* tmp_ptr = NULL;
  size_t dst_len = 0;
  switch (src_pixbuf->pixcfg.pixfmt) {
    case QOIR_PIXEL_FORMAT__RGB:
      dst_len = (*encode_rgb_func)(src_ptr, w, h, src_stride, &tmp_ptr);
      break;
    case QOIR_PIXEL_FORMAT__RGBA_NONPREMUL:
      dst_len = (*encode_rgba_func)(src_ptr, w, h, src_stride, &tmp_ptr);
      break;
    default: {
      qoir_encode_result fail_result = {0};
      fail_result.status_message = "#my_encode_webp: unsupported pixel format";
      return fail_result;
    }
  }
  if (!dst_len) {
    free(to_free);
    qoir_encode_result fail_result = {0};
    fail_result.status_message =
        "#my_encode_webp: WebPEncodeLosslessEtc failed";
    return fail_result;
  }

  uint8_t* dst_ptr = (uint8_t*)malloc(dst_len);
  if (!dst_ptr) {
    WebPFree(tmp_ptr);
    free(to_free);
    qoir_encode_result fail_result = {0};
    fail_result.status_message = "#my_encode_webp: out of memory";
    return fail_result;
  }
  memcpy(dst_ptr, tmp_ptr, dst_len);
  WebPFree(tmp_ptr);
  free(to_free);

  qoir_encode_result result = {0};
  result.owned_memory = dst_ptr;
  result.dst_ptr = dst_ptr;
  result.dst_len = dst_len;
  return result;
}

static size_t            //
my_WebPEncodeRGB(        //
    const uint8_t* rgb,  //
    int width,           //
    int height,          //
    int stride,          //
    uint8_t** output) {
  const float default_quality_factor = 75.0f;
  return WebPEncodeRGB(rgb, width, height, stride, default_quality_factor,
                       output);
}

static size_t             //
my_WebPEncodeRGBA(        //
    const uint8_t* rgba,  //
    int width,            //
    int height,           //
    int stride,           //
    uint8_t** output) {
  const float default_quality_factor = 75.0f;
  return WebPEncodeRGBA(rgba, width, height, stride, default_quality_factor,
                        output);
}

qoir_encode_result           //
my_encode_webp_lossless(     //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  return my_encode_webp(png_ptr, png_len, src_pixbuf, WebPEncodeLosslessRGB,
                        WebPEncodeLosslessRGBA, false);
}

qoir_encode_result           //
my_encode_webp_lossy(        //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  return my_encode_webp(png_ptr, png_len, src_pixbuf, my_WebPEncodeRGB,
                        my_WebPEncodeRGBA, false);
}

qoir_encode_result           //
my_encode_webp_lossy2(       //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  return my_encode_webp(png_ptr, png_len, src_pixbuf, WebPEncodeLosslessRGB,
                        WebPEncodeLosslessRGBA, true);
}

#ifdef __cplusplus
}  // extern "C"
#endif
