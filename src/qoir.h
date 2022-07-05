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

#ifndef QOIR_INCLUDE_GUARD
#define QOIR_INCLUDE_GUARD

// QOIR is a fast, simple image file format.
//
// Most users will want the qoir_decode and qoir_encode functions, which read
// from and write to a contiguous block of memory.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// TODO: remove the dependency.
#ifdef QOIR_IMPLEMENTATION
#define QOI_IMPLEMENTATION
#endif
#include "../third_party/qoi/qoi.h"

#ifdef __cplusplus
extern "C" {
#endif

// ================================ +Public Interface

// QOIR ships as a "single file C library" or "header file library" as per
// https://github.com/nothings/stb/blob/master/docs/stb_howto.txt
//
// To use that single file as a "foo.c"-like implementation, instead of a
// "foo.h"-like header, #define QOIR_IMPLEMENTATION before #include'ing or
// compiling it.

// -------- Compile-time Configuration

// Define QOIR_CONFIG__STATIC_FUNCTIONS (combined with QOIR_IMPLEMENTATION) to
// make all of QOIR's functions have static storage.
//
// This can help the compiler ignore or discard unused code, which can produce
// faster compiles and smaller binaries. Other motivations are discussed in the
// "ALLOW STATIC IMPLEMENTATION" section of
// https://raw.githubusercontent.com/nothings/stb/master/docs/stb_howto.txt
#if defined(QOIR_CONFIG__STATIC_FUNCTIONS)
#define QOIR_MAYBE_STATIC static
#else
#define QOIR_MAYBE_STATIC
#endif  // defined(QOIR_CONFIG__STATIC_FUNCTIONS)

// -------- Status Messages

extern const char qoir_status_message__error_invalid_argument[];
extern const char qoir_status_message__error_invalid_data[];
extern const char qoir_status_message__error_unsupported_pixbuf[];
extern const char qoir_status_message__error_unsupported_pixfmt[];

// -------- Pixel Buffers

// A pixel format combines an alpha transparency choice, a color model choice
// and other configuration (such as pixel byte order).
//
// Values less than 0x10 are directly representable by the file format (and by
// this implementation's API), using the same bit pattern.
//
// Values greater than or equal to 0x10 are representable by the API but not by
// the file format:
//  - the 0x10 bit means 3 (not 4) bytes per (fully opaque) pixel.
//  - the 0x20 bit means RGBA (not BGRA) byte order.

typedef uint32_t qoir_pixel_alpha_transparency;
typedef uint32_t qoir_pixel_color_model;
typedef uint32_t qoir_pixel_format;

// clang-format off

#define QOIR_PIXEL_ALPHA_TRANSPARENCY__OPAQUE                  0x01
#define QOIR_PIXEL_ALPHA_TRANSPARENCY__NONPREMULTIPLIED_ALPHA  0x02
#define QOIR_PIXEL_ALPHA_TRANSPARENCY__PREMULTIPLIED_ALPHA     0x03

#define QOIR_PIXEL_COLOR_MODEL__BGRA  0x00

#define QOIR_PIXEL_FORMAT__MASK_FOR_ALPHA_TRANSPARENCY  0x03
#define QOIR_PIXEL_FORMAT__MASK_FOR_COLOR_MODEL         0x0C

#define QOIR_PIXEL_FORMAT__INVALID         0x00
#define QOIR_PIXEL_FORMAT__BGRX            0x01
#define QOIR_PIXEL_FORMAT__BGRA_NONPREMUL  0x02
#define QOIR_PIXEL_FORMAT__BGRA_PREMUL     0x03
#define QOIR_PIXEL_FORMAT__BGR             0x11
#define QOIR_PIXEL_FORMAT__RGBX            0x21
#define QOIR_PIXEL_FORMAT__RGBA_NONPREMUL  0x22
#define QOIR_PIXEL_FORMAT__RGBA_PREMUL     0x23
#define QOIR_PIXEL_FORMAT__RGB             0x31

// clang-format on

typedef struct qoir_pixel_configuration_struct {
  qoir_pixel_format pixfmt;
  uint32_t width_in_pixels;
  uint32_t height_in_pixels;
} qoir_pixel_configuration;

typedef struct qoir_pixel_buffer_struct {
  qoir_pixel_configuration pixcfg;
  uint8_t* data;
  size_t stride_in_bytes;
} qoir_pixel_buffer;

static inline uint32_t  //
qoir_pixel_format__bytes_per_pixel(qoir_pixel_format pixfmt) {
  return (pixfmt & 0x10) ? 3 : 4;
}

// -------- QOIR Decode / Encode

typedef struct qoir_decode_pixel_configuration_result_struct {
  const char* status_message;
  qoir_pixel_configuration dst_pixcfg;
} qoir_decode_pixel_configuration_result;

QOIR_MAYBE_STATIC qoir_decode_pixel_configuration_result  //
qoir_decode_pixel_configuration(                          //
    uint8_t* src_ptr,                                     //
    size_t src_len);

typedef struct qoir_decode_result_struct {
  const char* status_message;
  void* owned_memory;
  qoir_pixel_buffer dst_pixbuf;
} qoir_decode_result;

typedef struct qoir_decode_options_struct {
  qoir_pixel_format pixfmt;
} qoir_decode_options;

QOIR_MAYBE_STATIC qoir_decode_result  //
qoir_decode(                          //
    const uint8_t* src_ptr,           //
    size_t src_len,                   //
    qoir_decode_options* options);

typedef struct qoir_encode_result_struct {
  const char* status_message;
  void* owned_memory;
  uint8_t* dst_ptr;
  size_t dst_len;
} qoir_encode_result;

typedef struct qoir_encode_options_struct {
  uint32_t todo;
} qoir_encode_options;

QOIR_MAYBE_STATIC qoir_encode_result  //
qoir_encode(                          //
    qoir_pixel_buffer* src_pixbuf,    //
    qoir_encode_options* options);

// ================================ -Public Interface

#ifdef QOIR_IMPLEMENTATION

// ================================ +Private Implementation

// -------- Status Messages

const char qoir_status_message__error_invalid_argument[] =  //
    "#qoir: invalid argument";
const char qoir_status_message__error_invalid_data[] =  //
    "#qoir: invalid data";
const char qoir_status_message__error_unsupported_pixbuf[] =  //
    "#qoir: unsupported pixbuf";
const char qoir_status_message__error_unsupported_pixfmt[] =  //
    "#qoir: unsupported pixfmt";

// -------- QOIR Decode / Encode

QOIR_MAYBE_STATIC qoir_decode_pixel_configuration_result  //
qoir_decode_pixel_configuration(                          //
    uint8_t* src_ptr,                                     //
    size_t src_len) {
  qoir_decode_pixel_configuration_result result = {0};
  result.status_message = "#TODO";
  return result;
}

QOIR_MAYBE_STATIC qoir_decode_result  //
qoir_decode(                          //
    const uint8_t* src_ptr,           //
    size_t src_len,                   //
    qoir_decode_options* options) {
  qoir_decode_result result = {0};
  qoir_pixel_format pixfmt = (options && options->pixfmt)
                                 ? options->pixfmt
                                 : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;

  qoi_desc desc;
  size_t bpp = qoir_pixel_format__bytes_per_pixel(pixfmt);
  void* pixbuf_data = qoi_decode(src_ptr, src_len, &desc, bpp);
  if (!pixbuf_data) {
    result.status_message = qoir_status_message__error_invalid_data;
    return result;
  }

  result.owned_memory = pixbuf_data;
  result.dst_pixbuf.pixcfg.pixfmt = pixfmt;
  result.dst_pixbuf.pixcfg.width_in_pixels = desc.width;
  result.dst_pixbuf.pixcfg.height_in_pixels = desc.height;
  result.dst_pixbuf.data = pixbuf_data;
  result.dst_pixbuf.stride_in_bytes = bpp * (size_t)desc.width;
  return result;
}

QOIR_MAYBE_STATIC qoir_encode_result  //
qoir_encode(                          //
    qoir_pixel_buffer* src_pixbuf,    //
    qoir_encode_options* options) {
  qoir_encode_result result = {0};
  if (!src_pixbuf) {
    result.status_message = qoir_status_message__error_invalid_argument;
    return result;
  }
  qoi_desc desc;
  desc.width = src_pixbuf->pixcfg.width_in_pixels;
  desc.height = src_pixbuf->pixcfg.height_in_pixels;
  desc.channels = 0;
  desc.colorspace = QOI_SRGB;

  switch (src_pixbuf->pixcfg.pixfmt) {
    case QOIR_PIXEL_FORMAT__RGB:
      desc.channels = 3;
      break;
    case QOIR_PIXEL_FORMAT__RGBA_NONPREMUL:
      desc.channels = 4;
      break;
    default:
      result.status_message = qoir_status_message__error_unsupported_pixfmt;
      return result;
  }

  if (src_pixbuf->stride_in_bytes !=
      ((size_t)desc.channels * src_pixbuf->pixcfg.width_in_pixels)) {
    result.status_message = qoir_status_message__error_unsupported_pixbuf;
    return result;
  }
  int encoded_qoi_len = 0;
  void* encoded_qoi_ptr = qoi_encode(src_pixbuf->data, &desc, &encoded_qoi_len);

  result.owned_memory = encoded_qoi_ptr;
  result.dst_ptr = encoded_qoi_ptr;
  result.dst_len = encoded_qoi_len;
  return result;
}

// ================================ -Private Implementation

#endif  // QOIR_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // QOIR_INCLUDE_GUARD
