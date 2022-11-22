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

// ----

#include "lz4.h"
#include "zstd.h"

// The third_party/zpng/zpng.cpp code expects to call ZSTD_foo functions from
// the zstd library. Use a C preprocessor hack to make it call our
// lz4hack_ZSTD_foo functions instead, which delegate to the LZ4 library.
#define ZSTD_compress lz4hack_ZSTD_compress
#define ZSTD_compressBound lz4hack_ZSTD_compressBound
#define ZSTD_decompress lz4hack_ZSTD_decompress

ZSTDLIB_API size_t           //
lz4hack_ZSTD_compressBound(  //
    size_t srcSize) {
  if (srcSize > INT_MAX) {
    return 0;
  }
  return LZ4_compressBound(static_cast<int>(srcSize));
}

ZSTDLIB_API size_t       //
lz4hack_ZSTD_compress(   //
    void* dst,           //
    size_t dstCapacity,  //
    const void* src,     //
    size_t srcSize,      //
    int compressionLevel) {
  static const size_t error_generic = -1;  // Equals (-ZSTD_error_GENERIC);
  if ((dstCapacity > INT_MAX) || ZSTD_isError(dstCapacity) ||
      (srcSize > INT_MAX)) {
    return error_generic;
  }
  int n = LZ4_compress_default(       //
      static_cast<const char*>(src),  //
      static_cast<char*>(dst),        //
      static_cast<int>(srcSize),      //
      static_cast<int>(dstCapacity));
  return (n > 0) ? n : error_generic;
}

ZSTDLIB_API size_t        //
lz4hack_ZSTD_decompress(  //
    void* dst,            //
    size_t dstCapacity,   //
    const void* src,      //
    size_t srcSize) {
  static const size_t error_generic = -1;  // Equals (-ZSTD_error_GENERIC);
  if ((dstCapacity > INT_MAX) || ZSTD_isError(dstCapacity) ||
      (srcSize > INT_MAX)) {
    return error_generic;
  }
  int n = LZ4_decompress_safe(        //
      static_cast<const char*>(src),  //
      static_cast<char*>(dst),        //
      static_cast<int>(srcSize),      //
      static_cast<int>(dstCapacity));
  return (n >= 0) ? n : error_generic;
}

// ----

// Use the C preprocessor again to be able to link both of the original and the
// hacked third_party/zpng/zpng.cpp code.
#define ZPNG_Compress lz4hack_ZPNG_Compress
#define ZPNG_Decompress lz4hack_ZPNG_Decompress
#define ZPNG_Free lz4hack_ZPNG_Free

// The remainder of this file copy/pastes zpng_adapter.cpp, replacing
// "foo_zpng" function names with "foo_lz4png".

#include "../src/qoir.h"
#include "../third_party/zpng/zpng.cpp"

#ifdef __cplusplus
extern "C" {
#endif

qoir_decode_result           //
my_decode_lz4png(            //
    const uint8_t* src_ptr,  //
    const size_t src_len) {
  ZPNG_Buffer buffer = {0};
  buffer.Data = const_cast<uint8_t*>(src_ptr);
  buffer.Bytes = src_len;
  ZPNG_ImageData image = ZPNG_Decompress(buffer);
  if (!image.Buffer.Data) {
    qoir_decode_result fail_result = {0};
    fail_result.status_message = "#my_decode_lz4png: ZPNG_Decompress failed";
    return fail_result;
  }

  size_t len = image.Buffer.Bytes;
  void* ptr = malloc(len);
  if (!ptr) {
    ZPNG_Free(&image.Buffer);
    qoir_decode_result fail_result = {0};
    fail_result.status_message = "#my_decode_lz4png: out of memory";
    return fail_result;
  }
  memcpy(ptr, image.Buffer.Data, len);
  ZPNG_Free(&image.Buffer);

  qoir_decode_result result = {0};
  result.owned_memory = ptr;
  result.dst_pixbuf.pixcfg.pixfmt = (image.Channels == 3)
                                        ? QOIR_PIXEL_FORMAT__RGB
                                        : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  result.dst_pixbuf.pixcfg.width_in_pixels = image.WidthPixels;
  result.dst_pixbuf.pixcfg.height_in_pixels = image.HeightPixels;
  result.dst_pixbuf.data = static_cast<uint8_t*>(ptr);
  result.dst_pixbuf.stride_in_bytes = image.StrideBytes;
  return result;
}

qoir_encode_result                  //
my_encode_lz4png(                   //
    const uint8_t* png_ptr,         //
    const size_t png_len,           //
    qoir_pixel_buffer* src_pixbuf,  //
    bool lossy,                     //
    bool no_filter) {
  unsigned int num_channels;
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
          "#my_encode_lz4png: unsupported pixel format";
      return fail_result;
    }
  }
  uint32_t w = src_pixbuf->pixcfg.width_in_pixels;
  uint32_t h = src_pixbuf->pixcfg.height_in_pixels;
  uint64_t num_pixels = w * h * static_cast<uint64_t>(num_channels);
  if ((num_pixels > SIZE_MAX) || (w > 0xFFFF) || (h > 0xFFFF) ||
      (src_pixbuf->stride_in_bytes !=
       (w * static_cast<uint64_t>(num_channels)))) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message = "#my_encode_lz4png: unsupported pixel buffer";
    return fail_result;
  }
  const size_t n = static_cast<size_t>(num_pixels);

  uint8_t* lossy_data = nullptr;
  if (lossy) {
    lossy_data = static_cast<uint8_t*>(malloc(n));
    if (!lossy_data) {
      qoir_encode_result fail_result = {0};
      fail_result.status_message = "#my_encode_lz4png: out of memory";
      return fail_result;
    }
    for (size_t i = 0; i < n; i++) {
      const uint8_t d = src_pixbuf->data[i] >> 2;
      lossy_data[i] = (d << 2) | (d >> 4);
    }
  }

  ZPNG_ImageData image = {0};
  image.Buffer.Data = lossy_data ? lossy_data : src_pixbuf->data;
  image.Buffer.Bytes = n;
  image.BytesPerChannel = 1;
  image.Channels = num_channels;
  image.WidthPixels = w;
  image.HeightPixels = h;
  image.StrideBytes = w * num_channels;

  ZPNG_Buffer buffer = ZPNG_Compress(&image, no_filter);
  free(lossy_data);
  if (!buffer.Data) {
    qoir_encode_result fail_result = {0};
    fail_result.status_message = "#my_encode_lz4png: ZPNG_Compress failed";
    return fail_result;
  }

  size_t len = buffer.Bytes;
  void* ptr = malloc(len);
  if (!ptr) {
    ZPNG_Free(&buffer);
    qoir_encode_result fail_result = {0};
    fail_result.status_message = "#my_encode_lz4png: out of memory";
    return fail_result;
  }
  memcpy(ptr, buffer.Data, len);
  ZPNG_Free(&buffer);

  qoir_encode_result result = {0};
  result.owned_memory = ptr;
  result.dst_ptr = static_cast<uint8_t*>(ptr);
  result.dst_len = len;
  return result;
}

qoir_encode_result           //
my_encode_lz4png_lossless(   //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  return my_encode_lz4png(png_ptr, png_len, src_pixbuf, false, false);
}

qoir_encode_result           //
my_encode_lz4png_lossy(      //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  return my_encode_lz4png(png_ptr, png_len, src_pixbuf, true, false);
}

qoir_encode_result                   //
my_encode_lz4png_nofilter_lossless(  //
    const uint8_t* png_ptr,          //
    const size_t png_len,            //
    qoir_pixel_buffer* src_pixbuf) {
  return my_encode_lz4png(png_ptr, png_len, src_pixbuf, false, true);
}

#ifdef __cplusplus
}  // extern "C"
#endif
