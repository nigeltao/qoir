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
//
// This file also contains a stand-alone implementation of LZ4 block
// compression, a general format that is not limited to compressing images. The
// qoir_lz4_block_decode and qoir_lz4_block_encode functions also read from and
// write to a contiguous block of memory.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

// -------- Other Macros

// Clang also #define's "__GNUC__".
#if defined(__GNUC__) || defined(_MSC_VER)
#define QOIR_RESTRICT __restrict
#else
#define QOIR_RESTRICT
#endif

// -------- Basic Result Types

typedef struct qoir_size_result_struct {
  const char* status_message;
  size_t value;
} qoir_size_result;

// -------- Status Messages

extern const char qoir_lz4_status_message__error_dst_is_too_short[];
extern const char qoir_lz4_status_message__error_invalid_data[];
extern const char qoir_lz4_status_message__error_src_is_too_long[];

extern const char qoir_status_message__error_invalid_argument[];
extern const char qoir_status_message__error_invalid_data[];
extern const char qoir_status_message__error_out_of_memory[];
extern const char qoir_status_message__error_unsupported_pixbuf[];
extern const char qoir_status_message__error_unsupported_pixbuf_dimensions[];
extern const char qoir_status_message__error_unsupported_pixfmt[];
extern const char qoir_status_message__error_unsupported_tile_format[];

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

// -------- File Format Constants

#define QOIR_TILE_MASK 0x7F
#define QOIR_TILE_SIZE 0x80
#define QOIR_TILE_SHIFT 7

// QOIR_LITERALS_PRE_PADDING is large enough to hold one row of a tile's
// pixels, at 4 bytes per pixel.
#define QOIR_LITERALS_PRE_PADDING (4 * QOIR_TILE_SIZE)

// QOIR_TS2 is the maximum (inclusive) number of pixels in a tile.
#define QOIR_TS2 (QOIR_TILE_SIZE * QOIR_TILE_SIZE)

// -------- LZ4 Decode

// QOIR_LZ4_BLOCK_DECODE_MAX_INCL_SRC_LEN is the maximum (inclusive) supported
// input length for this file's LZ4 decode functions. The LZ4 block format can
// generally support longer inputs, but this implementation specifically is
// more limited, to simplify overflow checking.
//
// With sufficiently large input, qoir_lz4_block_encode (note that that's
// encode, not decode) may very well produce output that is longer than this.
// That output is valid (in terms of the LZ4 file format) but isn't decodable
// by qoir_lz4_block_decode.
//
// 0x00FFFFFF = 16777215, which is over 16 million bytes.
#define QOIR_LZ4_BLOCK_DECODE_MAX_INCL_SRC_LEN 0x00FFFFFF

// qoir_lz4_block_decode writes to dst the LZ4 block decompressed form of src,
// returning the number of bytes written.
//
// It fails with qoir_lz4_status_message__error_dst_is_too_short if dst_len is
// not long enough to hold the decompressed form.
QOIR_MAYBE_STATIC qoir_size_result         //
qoir_lz4_block_decode(                     //
    uint8_t* QOIR_RESTRICT dst_ptr,        //
    size_t dst_len,                        //
    const uint8_t* QOIR_RESTRICT src_ptr,  //
    size_t src_len);

// -------- LZ4 Encode

// QOIR_LZ4_BLOCK_ENCODE_MAX_INCL_SRC_LEN is the maximum (inclusive) supported
// input length for this file's LZ4 encode functions. The LZ4 block format can
// generally support longer inputs, but this implementation specifically is
// more limited, to simplify overflow checking.
//
// 0x7E000000 = 2113929216, which is over 2 billion bytes.
#define QOIR_LZ4_BLOCK_ENCODE_MAX_INCL_SRC_LEN 0x7E000000

// qoir_lz4_block_encode_worst_case_dst_len returns the maximum (inclusive)
// number of bytes required to LZ4 block compress src_len input bytes.
QOIR_MAYBE_STATIC qoir_size_result         //
qoir_lz4_block_encode_worst_case_dst_len(  //
    size_t src_len);

// qoir_lz4_block_encode writes to dst the LZ4 block compressed form of src,
// returning the number of bytes written.
//
// Unlike the LZ4_compress_default function from the official implementation
// (https://github.com/lz4/lz4), it fails immediately with
// qoir_lz4_status_message__error_dst_is_too_short if dst_len is less than
// qoir_lz4_block_encode_worst_case_dst_len(src_len), even if the worst case is
// unrealized and the compressed form would actually fit.
QOIR_MAYBE_STATIC qoir_size_result         //
qoir_lz4_block_encode(                     //
    uint8_t* QOIR_RESTRICT dst_ptr,        //
    size_t dst_len,                        //
    const uint8_t* QOIR_RESTRICT src_ptr,  //
    size_t src_len);

// -------- QOIR Decode

typedef struct qoir_decode_pixel_configuration_result_struct {
  const char* status_message;
  qoir_pixel_configuration dst_pixcfg;
} qoir_decode_pixel_configuration_result;

QOIR_MAYBE_STATIC qoir_decode_pixel_configuration_result  //
qoir_decode_pixel_configuration(                          //
    uint8_t* src_ptr,                                     //
    size_t src_len);

typedef struct qoir_decode_buffer_struct {
  struct {
    // opcodes has to be before literals, so that (worst case) we can read (and
    // ignore) 8 bytes past the end of the opcodes array. See §
    uint8_t opcodes[4 * QOIR_TS2];
    uint8_t literals[QOIR_LITERALS_PRE_PADDING + (4 * QOIR_TS2)];
  } private_impl;
} qoir_decode_buffer;

typedef struct qoir_decode_result_struct {
  const char* status_message;
  void* owned_memory;
  qoir_pixel_buffer dst_pixbuf;
} qoir_decode_result;

typedef struct qoir_decode_options_struct {
  // Custom malloc/free implementations. NULL etc_func pointers means to use
  // the standard malloc and free functions. Non-NULL etc_func pointers will be
  // passed the memory_func_context.
  void* (*contextual_malloc_func)(void* memory_func_context, size_t len);
  void (*contextual_free_func)(void* memory_func_context, void* ptr);
  void* memory_func_context;

  qoir_decode_buffer* decbuf;
  qoir_pixel_format pixfmt;
} qoir_decode_options;

QOIR_MAYBE_STATIC qoir_decode_result  //
qoir_decode(                          //
    const uint8_t* src_ptr,           //
    size_t src_len,                   //
    qoir_decode_options* options);

// -------- QOIR Encode

typedef struct qoir_encode_buffer_struct {
  struct {
    // opcodes' size is ((5 * QOIR_TS2) + 64), not (4 * QOIR_TS2), because in
    // the worst case (during encoding, before discarding the too-long opcodes
    // in favor of literals), each pixel uses QOIR_OP_RGBA8, 5 bytes each. The
    // +64 is for the same reason as §, but the +8 is rounded up to a multiple
    // of a typical cache line size.
    uint8_t opcodes[(5 * QOIR_TS2) + 64];
    uint8_t literals[QOIR_LITERALS_PRE_PADDING + (4 * QOIR_TS2)];
  } private_impl;
} qoir_encode_buffer;

typedef struct qoir_encode_result_struct {
  const char* status_message;
  void* owned_memory;
  uint8_t* dst_ptr;
  size_t dst_len;
} qoir_encode_result;

typedef struct qoir_encode_options_struct {
  // Custom malloc/free implementations. NULL etc_func pointers means to use
  // the standard malloc and free functions. Non-NULL etc_func pointers will be
  // passed the memory_func_context.
  void* (*contextual_malloc_func)(void* memory_func_context, size_t len);
  void (*contextual_free_func)(void* memory_func_context, void* ptr);
  void* memory_func_context;

  qoir_encode_buffer* encbuf;
} qoir_encode_options;

QOIR_MAYBE_STATIC qoir_encode_result  //
qoir_encode(                          //
    qoir_pixel_buffer* src_pixbuf,    //
    qoir_encode_options* options);

// ================================ -Public Interface

#ifdef QOIR_IMPLEMENTATION

// ================================ +Private Implementation

// This implementation assumes that:
//  - converting a uint32_t to a size_t will never overflow.
//  - converting a size_t to a uint64_t will never overflow.
#if defined(__WORDSIZE)
#if (__WORDSIZE != 32) && (__WORDSIZE != 64)
#error "qoir.h requires a word size of either 32 or 64 bits"
#endif
#endif

#if defined(__GNUC__)
#define QOIR_ALWAYS_INLINE inline __attribute__((__always_inline__))
#elif defined(_MSC_VER)
#define QOIR_ALWAYS_INLINE __forceinline
#else
#define QOIR_ALWAYS_INLINE inline
#endif

#if !defined(QOIR_CONFIG__DISABLE_SIMD)
#if (defined(__GNUC__) && defined(__x86_64__)) || \
    (defined(_MSC_VER) && defined(_M_X64))
#define QOIR_USE_SIMD_SSE2
#include <emmintrin.h>
#endif
#endif

// Normally, the qoir_private_peek_etc and qoir_private_poke_etc
// implementations are both (1) correct regardless of CPU endianness and (2)
// very fast (e.g. an inlined qoir_private_peek_u32le call, in an optimized
// clang or gcc build, is a single MOV instruction on x86_64).
//
// However, the endian-agnostic implementations are slow on Microsoft's C
// compiler (MSC). Alternative memcpy-based implementations restore speed, but
// they are only correct on little-endian CPU architectures. Defining
// QOIR_USE_MEMCPY_LE_PEEK_POKE opts in to these implementations.
//
// https://godbolt.org/z/q4MfjzTPh
#if defined(_MSC_VER) && !defined(__clang__) && \
    (defined(_M_ARM64) || defined(_M_X64))
#define QOIR_USE_MEMCPY_LE_PEEK_POKE
#endif

static inline uint32_t  //
qoir_private_peek_u32le(const uint8_t* p) {
#if defined(QOIR_USE_MEMCPY_LE_PEEK_POKE)
  uint32_t x;
  memcpy(&x, p, 4);
  return x;
#else
  return ((uint32_t)(p[0]) << 0) | ((uint32_t)(p[1]) << 8) |
         ((uint32_t)(p[2]) << 16) | ((uint32_t)(p[3]) << 24);
#endif
}

static inline uint64_t  //
qoir_private_peek_u64le(const uint8_t* p) {
#if defined(QOIR_USE_MEMCPY_LE_PEEK_POKE)
  uint64_t x;
  memcpy(&x, p, 8);
  return x;
#else
  return ((uint64_t)(p[0]) << 0) | ((uint64_t)(p[1]) << 8) |
         ((uint64_t)(p[2]) << 16) | ((uint64_t)(p[3]) << 24) |
         ((uint64_t)(p[4]) << 32) | ((uint64_t)(p[5]) << 40) |
         ((uint64_t)(p[6]) << 48) | ((uint64_t)(p[7]) << 56);
#endif
}

static inline void  //
qoir_private_poke_u32le(uint8_t* p, uint32_t x) {
#if defined(QOIR_USE_MEMCPY_LE_PEEK_POKE)
  memcpy(p, &x, 4);
#else
  p[0] = (uint8_t)(x >> 0);
  p[1] = (uint8_t)(x >> 8);
  p[2] = (uint8_t)(x >> 16);
  p[3] = (uint8_t)(x >> 24);
#endif
}

static inline void  //
qoir_private_poke_u64le(uint8_t* p, uint64_t x) {
#if defined(QOIR_USE_MEMCPY_LE_PEEK_POKE)
  memcpy(p, &x, 8);
#else
  p[0] = (uint8_t)(x >> 0);
  p[1] = (uint8_t)(x >> 8);
  p[2] = (uint8_t)(x >> 16);
  p[3] = (uint8_t)(x >> 24);
  p[4] = (uint8_t)(x >> 32);
  p[5] = (uint8_t)(x >> 40);
  p[6] = (uint8_t)(x >> 48);
  p[7] = (uint8_t)(x >> 56);
#endif
}

static inline uint32_t  //
qoir_private_hash(const uint8_t* p) {
  // 2654435761u is Knuth's magic constant. 6 is log2(color_cache_size).
  return (qoir_private_peek_u32le(p) * 2654435761u) >> (32 - 6);
}

static inline uint32_t  //
qoir_private_tile_dimension(bool interior, uint32_t pixel_dimension) {
  return interior ? QOIR_TILE_SIZE
                  : (((pixel_dimension - 1) & QOIR_TILE_MASK) + 1);
}

// QOIR_TILE_LZ4_COMPRESSION_WORST_CASE equals
// qoir_lz4_block_encode_worst_case_dst_len(4 * QTS * QTS).
#define QOIR_TILE_LZ4_COMPRESSION_WORST_CASE \
  ((4 * QOIR_TILE_SIZE * QOIR_TILE_SIZE) +   \
   ((4 * QOIR_TILE_SIZE * QOIR_TILE_SIZE) / 255) + 16)

// -------- SWAR (SIMD Within a Register)

// These treat a u32 as a 4xu8 vector.

// QOIR_SWAR_PAVGB returns ((a + b + 1) / 2).
#define QOIR_SWAR_PAVGB(a, b) ((a | b) - (((a ^ b) >> 1) & 0x7F7F7F7F))

// QOIR_SWAR_PSUBB returns (a - b).
#define QOIR_SWAR_PSUBB(a, b) \
  ((a | 0x80808080) - (b & 0x7F7F7F7F)) ^ ((a ^ ~b) & 0x80808080)

// -------- Memory Management

#define QOIR_MALLOC(len)                                                \
  qoir_private_malloc(options ? options->contextual_malloc_func : NULL, \
                      options ? options->memory_func_context : NULL, len)

#define QOIR_FREE(ptr)                                              \
  qoir_private_free(options ? options->contextual_free_func : NULL, \
                    options ? options->memory_func_context : NULL, ptr)

static inline void*                                                  //
qoir_private_malloc(void* (*contextual_malloc_func)(void*, size_t),  //
                    void* memory_func_context,                       //
                    size_t len) {
  if (contextual_malloc_func) {
    return (*contextual_malloc_func)(memory_func_context, len);
  }
  return malloc(len);
}

static inline void                                             //
qoir_private_free(void (*contextual_free_func)(void*, void*),  //
                  void* memory_func_context,                   //
                  void* ptr) {
  if (contextual_free_func) {
    (*contextual_free_func)(memory_func_context, ptr);
    return;
  }
  free(ptr);
}

// -------- Status Messages

const char qoir_lz4_status_message__error_dst_is_too_short[] =  //
    "#qoir/lz4: dst is too short";
const char qoir_lz4_status_message__error_invalid_data[] =  //
    "#qoir/lz4: invalid data";
const char qoir_lz4_status_message__error_src_is_too_long[] =  //
    "#qoir/lz4: src is too long";

const char qoir_status_message__error_invalid_argument[] =  //
    "#qoir: invalid argument";
const char qoir_status_message__error_invalid_data[] =  //
    "#qoir: invalid data";
const char qoir_status_message__error_out_of_memory[] =  //
    "#qoir: out of memory";
const char qoir_status_message__error_unsupported_pixbuf[] =  //
    "#qoir: unsupported pixbuf";
const char qoir_status_message__error_unsupported_pixbuf_dimensions[] =  //
    "#qoir: unsupported pixbuf dimensions";
const char qoir_status_message__error_unsupported_pixfmt[] =  //
    "#qoir: unsupported pixfmt";
const char qoir_status_message__error_unsupported_tile_format[] =  //
    "#qoir: unsupported tile format";

// -------- Pixel Swizzlers

typedef void (*qoir_private_swizzle_func)(  //
    uint8_t* QOIR_RESTRICT dst_ptr,         //
    size_t dst_stride_in_bytes,             //
    const uint8_t* QOIR_RESTRICT src_ptr,   //
    size_t src_stride_in_bytes,             //
    size_t width_in_pixels,                 //
    size_t height_in_pixels);

static void                                //
qoir_private_swizzle__copy_4(              //
    uint8_t* QOIR_RESTRICT dst_ptr,        //
    size_t dst_stride_in_bytes,            //
    const uint8_t* QOIR_RESTRICT src_ptr,  //
    size_t src_stride_in_bytes,            //
    size_t width_in_pixels,                //
    size_t height_in_pixels) {
  size_t n = 4 * width_in_pixels;
  for (; height_in_pixels > 0; height_in_pixels--) {
    memcpy(dst_ptr, src_ptr, n);
    dst_ptr += dst_stride_in_bytes;
    src_ptr += src_stride_in_bytes;
  }
}

static void                                //
qoir_private_swizzle__rgb__rgba(           //
    uint8_t* QOIR_RESTRICT dst_ptr,        //
    size_t dst_stride_in_bytes,            //
    const uint8_t* QOIR_RESTRICT src_ptr,  //
    size_t src_stride_in_bytes,            //
    size_t width_in_pixels,                //
    size_t height_in_pixels) {
  for (; height_in_pixels > 0; height_in_pixels--) {
    for (size_t n = width_in_pixels; n > 0; n--) {
      *dst_ptr++ = *src_ptr++;
      *dst_ptr++ = *src_ptr++;
      *dst_ptr++ = *src_ptr++;
      src_ptr++;
    }
    dst_ptr += dst_stride_in_bytes - (3 * width_in_pixels);
    src_ptr += src_stride_in_bytes - (4 * width_in_pixels);
  }
}

static void                                //
qoir_private_swizzle__rgba__rgb(           //
    uint8_t* QOIR_RESTRICT dst_ptr,        //
    size_t dst_stride_in_bytes,            //
    const uint8_t* QOIR_RESTRICT src_ptr,  //
    size_t src_stride_in_bytes,            //
    size_t width_in_pixels,                //
    size_t height_in_pixels) {
  for (; height_in_pixels > 0; height_in_pixels--) {
    for (size_t n = width_in_pixels; n > 0; n--) {
      *dst_ptr++ = *src_ptr++;
      *dst_ptr++ = *src_ptr++;
      *dst_ptr++ = *src_ptr++;
      *dst_ptr++ = 0xFF;
    }
    dst_ptr += dst_stride_in_bytes - (4 * width_in_pixels);
    src_ptr += src_stride_in_bytes - (3 * width_in_pixels);
  }
}

// -------- LZ4 Decode

QOIR_MAYBE_STATIC qoir_size_result         //
qoir_lz4_block_decode(                     //
    uint8_t* QOIR_RESTRICT dst_ptr,        //
    size_t dst_len,                        //
    const uint8_t* QOIR_RESTRICT src_ptr,  //
    size_t src_len) {
  qoir_size_result result = {0};

  if (src_len > QOIR_LZ4_BLOCK_DECODE_MAX_INCL_SRC_LEN) {
    result.status_message = qoir_lz4_status_message__error_src_is_too_long;
    return result;
  }

  uint8_t* const original_dst_ptr = dst_ptr;

  // See https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md for file
  // format details, such as the LZ4 token's bit patterns.
  while (src_len > 0) {
    uint32_t token = *src_ptr++;
    src_len--;

    uint32_t literal_len = token >> 4;
    if (literal_len > 0) {
      if (literal_len == 15) {
        while (1) {
          if (src_len == 0) {
            goto fail_invalid_data;
          }
          uint32_t s = *src_ptr++;
          src_len--;
          literal_len += s;
          if (s != 255) {
            break;
          }
        }
      }

      if (literal_len > src_len) {
        goto fail_invalid_data;
      } else if (literal_len > dst_len) {
        result.status_message = qoir_lz4_status_message__error_dst_is_too_short;
        return result;
      }
      memcpy(dst_ptr, src_ptr, literal_len);
      dst_ptr += literal_len;
      dst_len -= literal_len;
      src_ptr += literal_len;
      src_len -= literal_len;
      if (src_len == 0) {
        result.value = ((size_t)(dst_ptr - original_dst_ptr));
        return result;
      }
    }

    if (src_len < 2) {
      goto fail_invalid_data;
    }
    uint32_t copy_off = ((uint32_t)src_ptr[0]) | (((uint32_t)src_ptr[1]) << 8);
    src_ptr += 2;
    src_len -= 2;
    if ((copy_off == 0) ||  //
        (copy_off > ((size_t)(dst_ptr - original_dst_ptr)))) {
      goto fail_invalid_data;
    }

    uint32_t copy_len = (token & 15) + 4;
    if (copy_len == 19) {
      while (1) {
        if (src_len == 0) {
          goto fail_invalid_data;
        }
        uint32_t s = *src_ptr++;
        src_len--;
        copy_len += s;
        if (s != 255) {
          break;
        }
      }
    }

    if (dst_len < copy_len) {
      result.status_message = qoir_lz4_status_message__error_dst_is_too_short;
      return result;
    }
    dst_len -= copy_len;
    for (const uint8_t* from = dst_ptr - copy_off; copy_len > 0; copy_len--) {
      *dst_ptr++ = *from++;
    }
  }

fail_invalid_data:
  result.status_message = qoir_lz4_status_message__error_invalid_data;
  return result;
}

// -------- LZ4 Encode

#define QOIR_LZ4_HASH_TABLE_SIZE 12

static inline uint32_t  //
qoir_lz4_private_hash(uint32_t x) {
  // 2654435761u is Knuth's magic constant.
  return (x * 2654435761u) >> (32 - QOIR_LZ4_HASH_TABLE_SIZE);
}

static inline size_t  //
qoir_lz4_private_longest_common_prefix(const uint8_t* p,
                                       const uint8_t* q,
                                       const uint8_t* p_limit) {
  const uint8_t* const original_p = p;
  size_t n = p_limit - p;
  while ((n >= 4) &&
         (qoir_private_peek_u32le(p) == qoir_private_peek_u32le(q))) {
    p += 4;
    q += 4;
    n -= 4;
  }
  while ((n > 0) && (*p == *q)) {
    p += 1;
    q += 1;
    n -= 1;
  }
  return (size_t)(p - original_p);
}

QOIR_MAYBE_STATIC qoir_size_result         //
qoir_lz4_block_encode_worst_case_dst_len(  //
    size_t src_len) {
  qoir_size_result result = {0};

  if (src_len > QOIR_LZ4_BLOCK_ENCODE_MAX_INCL_SRC_LEN) {
    result.status_message = qoir_lz4_status_message__error_src_is_too_long;
    return result;
  }
  // For the curious, if (src_len <= 0x7E000000) then (value <= 0x7E7E7E8E).
  result.value = src_len + (src_len / 255) + 16;
  return result;
}

QOIR_MAYBE_STATIC qoir_size_result         //
qoir_lz4_block_encode(                     //
    uint8_t* QOIR_RESTRICT dst_ptr,        //
    size_t dst_len,                        //
    const uint8_t* QOIR_RESTRICT src_ptr,  //
    size_t src_len) {
  qoir_size_result result = qoir_lz4_block_encode_worst_case_dst_len(src_len);
  if (result.status_message) {
    return result;
  } else if (result.value > dst_len) {
    result.status_message = qoir_lz4_status_message__error_dst_is_too_short;
    result.value = 0;
    return result;
  }
  result.value = 0;

  uint8_t* dp = dst_ptr;
  const uint8_t* sp = src_ptr;
  const uint8_t* literal_start = src_ptr;

  // See https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md for "The
  // last match must start at least 12 bytes before the end of block" and other
  // file format details, such as the LZ4 token's bit patterns.
  if (src_len > 12) {
    const uint8_t* const match_limit = src_ptr + src_len - 5;
    const size_t final_literals_limit = src_len - 11;

    // hash_table maps from QOIR_LZ4_HASH_TABLE_SIZE-bit keys to 32-bit values.
    // Each value is an offset o, relative to src_ptr, initialized to zero.
    // Each key, when set, is a hash of 4 bytes src_ptr[o .. o+4].
    uint32_t hash_table[1 << QOIR_LZ4_HASH_TABLE_SIZE] = {0};

    while (1) {
      // Start with 1-byte steps, accelerating when not finding any matches
      // (e.g. when compressing binary data, not text data).
      size_t step = 1;
      size_t step_counter = 1 << 6;

      // Start with a non-empty literal.
      const uint8_t* next_sp = sp + 1;
      uint32_t next_hash =
          qoir_lz4_private_hash(qoir_private_peek_u32le(next_sp));

      // Find a match or goto final_literals.
      const uint8_t* match = NULL;
      do {
        sp = next_sp;
        next_sp += step;
        step = step_counter++ >> 6;
        if ((next_sp - src_ptr) > final_literals_limit) {
          goto final_literals;
        }
        uint32_t* hash_table_entry = &hash_table[next_hash];
        match = src_ptr + *hash_table_entry;
        next_hash = qoir_lz4_private_hash(qoir_private_peek_u32le(next_sp));
        *hash_table_entry = (uint32_t)(sp - src_ptr);
      } while (((sp - match) > 0xFFFF) ||
               (qoir_private_peek_u32le(sp) != qoir_private_peek_u32le(match)));

      // Extend the match backwards.
      while ((sp > literal_start) && (match > src_ptr) &&
             (sp[-1] == match[-1])) {
        sp--;
        match--;
      }

      // Emit half of the LZ4 token, encoding the literal length. We'll fix up
      // the other half later.
      uint8_t* token = dp;
      size_t literal_len = (size_t)(sp - literal_start);
      if (literal_len < 15) {
        *dp++ = (uint8_t)(literal_len << 4);
      } else {
        size_t n = literal_len - 15;
        *dp++ = 0xF0;
        for (; n >= 255; n -= 255) {
          *dp++ = 0xFF;
        }
        *dp++ = (uint8_t)n;
      }
      memcpy(dp, literal_start, literal_len);
      dp += literal_len;

      while (1) {
        // At this point:
        //  - sp    points to the start of the match's later   copy.
        //  - match points to the start of the match's earlier copy.
        //  - token points to the LZ4 token.

        // Calculate the match length and update the token's other half.
        size_t copy_off = (size_t)(sp - match);
        *dp++ = (uint8_t)(copy_off >> 0);
        *dp++ = (uint8_t)(copy_off >> 8);
        size_t adj_copy_len = qoir_lz4_private_longest_common_prefix(
            4 + sp, 4 + match, match_limit);
        if (adj_copy_len < 15) {
          *token |= (uint8_t)adj_copy_len;
        } else {
          size_t n = adj_copy_len - 15;
          *token |= 0x0F;
          for (; n >= 255; n -= 255) {
            *dp++ = 0xFF;
          }
          *dp++ = (uint8_t)n;
        }
        sp += 4 + adj_copy_len;

        // Update the literal_start and check the final_literals_limit.
        literal_start = sp;
        if ((sp - src_ptr) >= final_literals_limit) {
          goto final_literals;
        }

        // We've skipped over hashing everything within the match. Also, the
        // minimum match length is 4. Update the hash table for one of those
        // skipped positions.
        hash_table[qoir_lz4_private_hash(qoir_private_peek_u32le(sp - 2))] =
            (uint32_t)(sp - 2 - src_ptr);

        // Check if this match can be followed immediately by another match.
        // If so, continue the loop. Otherwise, break.
        uint32_t* hash_table_entry =
            &hash_table[qoir_lz4_private_hash(qoir_private_peek_u32le(sp))];
        uint32_t old_offset = *hash_table_entry;
        uint32_t new_offset = (uint32_t)(sp - src_ptr);
        *hash_table_entry = new_offset;
        match = src_ptr + old_offset;
        if (((new_offset - old_offset) > 0xFFFF) ||
            (qoir_private_peek_u32le(sp) != qoir_private_peek_u32le(match))) {
          break;
        }
        token = dp++;
        *token = 0;
      }
    }
  }

final_literals:
  do {
    size_t final_literal_len = src_len - (size_t)(literal_start - src_ptr);
    if (final_literal_len < 15) {
      *dp++ = (uint8_t)(final_literal_len << 4);
    } else {
      size_t n = final_literal_len - 15;
      *dp++ = 0xF0;
      for (; n >= 255; n -= 255) {
        *dp++ = 0xFF;
      }
      *dp++ = (uint8_t)n;
    }
    memcpy(dp, literal_start, final_literal_len);
    dp += final_literal_len;
  } while (0);

  result.value = (size_t)(dp - dst_ptr);
  return result;
}

// -------- QOIR Decode

QOIR_MAYBE_STATIC qoir_decode_pixel_configuration_result  //
qoir_decode_pixel_configuration(                          //
    uint8_t* src_ptr,                                     //
    size_t src_len) {
  qoir_decode_pixel_configuration_result result = {0};

  if ((src_len < 20) ||
      (qoir_private_peek_u32le(src_ptr) != 0x52494F51)) {  // "QOIR"le.
    result.status_message = qoir_status_message__error_invalid_data;
    return result;
  }
  uint64_t qoir_chunk_payload_len = qoir_private_peek_u64le(src_ptr + 4);
  if ((qoir_chunk_payload_len < 8) ||
      (qoir_chunk_payload_len > 0x7FFFFFFFFFFFFFFFull)) {
    result.status_message = qoir_status_message__error_invalid_data;
    return result;
  }

  uint32_t header0 = qoir_private_peek_u32le(src_ptr + 12);
  uint32_t width_in_pixels = 0xFFFFFF & header0;
  qoir_pixel_format src_pixfmt = 0x0F & (header0 >> 24);
  switch (src_pixfmt) {
    case QOIR_PIXEL_FORMAT__BGRX:
    case QOIR_PIXEL_FORMAT__BGRA_NONPREMUL:
    case QOIR_PIXEL_FORMAT__BGRA_PREMUL:
      break;
    default:
      result.status_message = qoir_status_message__error_invalid_data;
      return result;
  }
  uint32_t header1 = qoir_private_peek_u32le(src_ptr + 16);
  uint32_t height_in_pixels = 0xFFFFFF & header1;

  result.dst_pixcfg.pixfmt = src_pixfmt;
  result.dst_pixcfg.width_in_pixels = width_in_pixels;
  result.dst_pixcfg.height_in_pixels = height_in_pixels;
  return result;
}

// Callers should pass (QOIR_LITERALS_PRE_PADDING + (4 * tw * th)) for dst_len,
// so that the decode loop can always refer (by a simple negative offset) to
// the pixel above the current pixel.
//
// Callers should pass (opcode_stream_length + 8) for src_len so that the
// decode loop can always peek for 8 bytes, even at the end of the stream.
// Reference: §
static qoir_size_result            //
qoir_private_decode_tile_opcodes(  //
    uint8_t* dst_ptr,              //
    size_t dst_len,                //
    const uint8_t* src_ptr,        //
    size_t src_len) {
  qoir_size_result result = {0};

  if ((dst_len < QOIR_LITERALS_PRE_PADDING) || (src_len < 8)) {
    result.status_message = qoir_status_message__error_invalid_argument;
    return result;
  }

  // The array-of-four-uint8_t elements are in R, G, B, A order.
  uint8_t color_cache[64][4];
  for (int i = 0; i < 64; i++) {
    color_cache[i][0] = 0x00;
    color_cache[i][1] = 0x00;
    color_cache[i][2] = 0x00;
    color_cache[i][3] = 0xFF;
  }

  uint8_t* dp = dst_ptr + QOIR_LITERALS_PRE_PADDING;
  uint8_t* dq = dst_ptr + dst_len;
  const uint8_t* sp = src_ptr;
  const uint8_t* sq = src_ptr + src_len - 8;
  while (dp < dq) {
    if (sp >= sq) {
      result.status_message = qoir_status_message__error_invalid_data;
      return result;
    }

    // Either code path is equivalent to (but faster than):
    //   pixel[i] = (1 + dp[-4 + i] + dp[(-4 * QOIR_TILE_SIZE) + i]) / 2;
    // for i in 0..4.
    uint8_t pixel[4];
#if defined(QOIR_USE_SIMD_SSE2)
    int pred32l;  // Pixel left.
    int pred32a;  // Pixel above.
    memcpy(&pred32l, dp - 4, 4);
    memcpy(&pred32a, dp - (4 * QOIR_TILE_SIZE), 4);
    int pred32avg = _mm_cvtsi128_si32(
        _mm_avg_epu8(_mm_cvtsi32_si128(pred32l), _mm_cvtsi32_si128(pred32a)));
    memcpy(pixel, &pred32avg, 4);
#else
    uint32_t pred32l = qoir_private_peek_u32le(dp - 4);
    uint32_t pred32a = qoir_private_peek_u32le(dp - (4 * QOIR_TILE_SIZE));
    qoir_private_poke_u32le(pixel, QOIR_SWAR_PAVGB(pred32l, pred32a));
#endif

    uint64_t s64 = qoir_private_peek_u64le(sp);
    if ((s64 & 0xFF) == 0xF7) {  // QOIR_OP_RGB8
      pixel[0] += (uint8_t)(s64 >> 0x08);
      pixel[1] += (uint8_t)(s64 >> 0x10);
      pixel[2] += (uint8_t)(s64 >> 0x18);
      sp += 4;
      memcpy(color_cache[qoir_private_hash(pixel)], pixel, 4);
      memcpy(dp, pixel, 4);
      dp += 4;

    } else if ((s64 & 0x03) == 0) {  // QOIR_OP_INDEX
      sp += 1;
      memcpy(pixel, color_cache[(uint8_t)s64 >> 0x02], 4);
      memcpy(dp, pixel, 4);
      dp += 4;

    } else if ((s64 & 0x03) == 1) {  // QOIR_OP_RGB2
      pixel[0] += ((s64 >> 0x02) & 0x03) - 2;
      pixel[1] += ((s64 >> 0x04) & 0x03) - 2;
      pixel[2] += ((s64 >> 0x06) & 0x03) - 2;
      sp += 1;
      memcpy(color_cache[qoir_private_hash(pixel)], pixel, 4);
      memcpy(dp, pixel, 4);
      dp += 4;

    } else if ((s64 & 0x03) == 2) {  // QOIR_OP_LUMA
      uint8_t delta_g = ((uint8_t)s64 >> 0x02) - 32;
      pixel[0] += delta_g - 8 + ((s64 >> 0x08) & 0x0F);
      pixel[1] += delta_g;
      pixel[2] += delta_g - 8 + ((s64 >> 0x0C) & 0x0F);
      sp += 2;
      memcpy(color_cache[qoir_private_hash(pixel)], pixel, 4);
      memcpy(dp, pixel, 4);
      dp += 4;

    } else if ((s64 & 0x07) == 3) {  // QOIR_OP_RGB7
      pixel[0] += ((s64 >> 0x03) & 0x7F) - 0x40;
      pixel[1] += ((s64 >> 0x0A) & 0x7F) - 0x40;
      pixel[2] += ((s64 >> 0x11) & 0x7F) - 0x40;
      sp += 3;
      memcpy(color_cache[qoir_private_hash(pixel)], pixel, 4);
      memcpy(dp, pixel, 4);
      dp += 4;

    } else if ((s64 & 0xFF) < 0xD7) {  // QOIR_OP_RUNS
      size_t run_length = (s64 & 0xFF) >> 0x03;
      if ((dq - dp) < (4 * (run_length + 1))) {
        result.status_message = qoir_status_message__error_invalid_data;
        return result;
      }
      memcpy(pixel, dp - 4, 4);
      do {
        memcpy(dp, pixel, 4);
        dp += 4;
      } while (run_length--);
      sp += 1;

    } else if ((s64 & 0xFF) == 0xD7) {  // QOIR_OP_RUNL
      size_t run_length = (s64 >> 0x08) & 0xFF;
      if ((dq - dp) < (4 * (run_length + 1))) {
        result.status_message = qoir_status_message__error_invalid_data;
        return result;
      }
      memcpy(pixel, dp - 4, 4);
      do {
        memcpy(dp, pixel, 4);
        dp += 4;
      } while (run_length--);
      sp += 2;

    } else if ((s64 & 0xFF) == 0xDF) {  // QOIR_OP_RGBA2
      pixel[0] += ((s64 >> 0x08) & 0x03) - 2;
      pixel[1] += ((s64 >> 0x0A) & 0x03) - 2;
      pixel[2] += ((s64 >> 0x0C) & 0x03) - 2;
      pixel[3] += ((s64 >> 0x0E) & 0x03) - 2;
      sp += 2;
      memcpy(color_cache[qoir_private_hash(pixel)], pixel, 4);
      memcpy(dp, pixel, 4);
      dp += 4;

    } else if ((s64 & 0xFF) == 0xE7) {  // QOIR_OP_RGBA4
      pixel[0] += ((s64 >> 0x08) & 0x0F) - 8;
      pixel[1] += ((s64 >> 0x0C) & 0x0F) - 8;
      pixel[2] += ((s64 >> 0x10) & 0x0F) - 8;
      pixel[3] += ((s64 >> 0x14) & 0x0F) - 8;
      sp += 3;
      memcpy(color_cache[qoir_private_hash(pixel)], pixel, 4);
      memcpy(dp, pixel, 4);
      dp += 4;

    } else if ((s64 & 0xFF) == 0xEF) {  // QOIR_OP_RGBA8
      pixel[0] += (uint8_t)(s64 >> 0x08);
      pixel[1] += (uint8_t)(s64 >> 0x10);
      pixel[2] += (uint8_t)(s64 >> 0x18);
      pixel[3] += (uint8_t)(s64 >> 0x20);
      sp += 5;
      memcpy(color_cache[qoir_private_hash(pixel)], pixel, 4);
      memcpy(dp, pixel, 4);
      dp += 4;

    } else {  // QOIR_OP_A8
      pixel[3] += (uint8_t)(s64 >> 0x08);
      sp += 2;
      memcpy(color_cache[qoir_private_hash(pixel)], pixel, 4);
      memcpy(dp, pixel, 4);
      dp += 4;
    }
  }

  if (sp != sq) {
    result.status_message = qoir_status_message__error_invalid_data;
    return result;
  }
  result.value = (size_t)(dp - dst_ptr);
  return result;
}

static const char*                  //
qoir_private_decode_qpix_payload(   //
    qoir_decode_buffer* decbuf,     //
    qoir_pixel_format dst_pixfmt,   //
    uint32_t dst_width_in_pixels,   //
    uint32_t dst_height_in_pixels,  //
    uint8_t* dst_data,              //
    size_t dst_stride_in_bytes,     //
    qoir_pixel_format src_pixfmt,   //
    const uint8_t* src_ptr,         //
    size_t src_len) {
  size_t height_in_tiles =
      (dst_height_in_pixels + QOIR_TILE_MASK) >> QOIR_TILE_SHIFT;
  size_t width_in_tiles =
      (dst_width_in_pixels + QOIR_TILE_MASK) >> QOIR_TILE_SHIFT;
  if ((height_in_tiles == 0) || (width_in_tiles == 0)) {
    goto done;
  }
  size_t ty1 = (height_in_tiles - 1) << QOIR_TILE_SHIFT;
  size_t tx1 = (width_in_tiles - 1) << QOIR_TILE_SHIFT;

  qoir_private_swizzle_func swizzle = NULL;
  switch (dst_pixfmt) {
    case QOIR_PIXEL_FORMAT__RGB:
      swizzle = qoir_private_swizzle__rgb__rgba;
      break;
    case QOIR_PIXEL_FORMAT__RGBA_NONPREMUL:
      swizzle = qoir_private_swizzle__copy_4;
      break;
    default:
      return qoir_status_message__error_unsupported_pixfmt;
  }
  size_t num_dst_channels = qoir_pixel_format__bytes_per_pixel(dst_pixfmt);

  uint8_t* literals_pre_padding = decbuf->private_impl.literals;
  for (int i = 0; i < QOIR_LITERALS_PRE_PADDING; i += 4) {
    literals_pre_padding[i + 0] = 0x00;
    literals_pre_padding[i + 1] = 0x00;
    literals_pre_padding[i + 2] = 0x00;
    literals_pre_padding[i + 3] = 0xFF;
  }

  // ty, tx, tw and th are the tile's top-left offset, width and height, all
  // measured in pixels.
  for (size_t ty = 0; ty <= ty1; ty += QOIR_TILE_SIZE) {
    for (size_t tx = 0; tx <= tx1; tx += QOIR_TILE_SIZE) {
      size_t tw = qoir_private_tile_dimension(tx < tx1, dst_width_in_pixels);
      size_t th = qoir_private_tile_dimension(ty < ty1, dst_height_in_pixels);

      if (src_len < 4) {
        return qoir_status_message__error_invalid_data;
      }
      uint32_t prefix = qoir_private_peek_u32le(src_ptr);
      src_ptr += 4;
      src_len -= 4;
      size_t tile_len = prefix & 0xFFFFFF;
      if ((src_len < (tile_len + 8)) ||  //
          (((4 * QOIR_TS2) < tile_len) && ((prefix >> 31) != 0))) {
        return qoir_status_message__error_invalid_data;
      }

      const uint8_t* literals = NULL;
      switch (prefix >> 24) {
        case 0: {  // Literals tile format.
          if (tile_len != (4 * tw * th)) {
            return qoir_status_message__error_invalid_data;
          }
          literals = src_ptr;
          break;
        }
        case 1: {  // Opcodes tile format.
          qoir_size_result r = qoir_private_decode_tile_opcodes(
              decbuf->private_impl.literals,              //
              QOIR_LITERALS_PRE_PADDING + (4 * tw * th),  //
              src_ptr, tile_len + 8);                     // See § for +8.
          if (r.status_message) {
            return r.status_message;
          } else if (r.value != (QOIR_LITERALS_PRE_PADDING + (4 * tw * th))) {
            return qoir_status_message__error_invalid_data;
          }
          literals = decbuf->private_impl.literals + QOIR_LITERALS_PRE_PADDING;
          break;
        }
        case 2: {  // LZ4-Literals tile format.
          qoir_size_result r = qoir_lz4_block_decode(
              decbuf->private_impl.literals + QOIR_LITERALS_PRE_PADDING,
              sizeof(decbuf->private_impl.literals) - QOIR_LITERALS_PRE_PADDING,
              src_ptr, tile_len);
          if (r.status_message) {
            return qoir_status_message__error_invalid_data;
          } else if (r.value != (4 * tw * th)) {
            return qoir_status_message__error_invalid_data;
          }
          literals = decbuf->private_impl.literals + QOIR_LITERALS_PRE_PADDING;
          break;
        }
        case 3: {  // LZ4-Opcodes tile format.
          qoir_size_result r0 = qoir_lz4_block_decode(
              decbuf->private_impl.opcodes,
              sizeof(decbuf->private_impl.opcodes), src_ptr, tile_len);
          if (r0.status_message) {
            return qoir_status_message__error_invalid_data;
          }
          qoir_size_result r1 = qoir_private_decode_tile_opcodes(
              decbuf->private_impl.literals,                //
              QOIR_LITERALS_PRE_PADDING + (4 * tw * th),    //
              decbuf->private_impl.opcodes, r0.value + 8);  // See § for +8.
          if (r1.status_message) {
            return r1.status_message;
          } else if (r1.value != (QOIR_LITERALS_PRE_PADDING + (4 * tw * th))) {
            return qoir_status_message__error_invalid_data;
          }
          literals = decbuf->private_impl.literals + QOIR_LITERALS_PRE_PADDING;
          break;
        }
        default:
          return qoir_status_message__error_unsupported_tile_format;
      }

      src_ptr += tile_len;
      src_len -= tile_len;

      uint8_t* dp =
          dst_data + (dst_stride_in_bytes * ty) + (num_dst_channels * tx);
      (*swizzle)(dp, dst_stride_in_bytes, literals, 4 * tw, tw, th);
    }
  }

done:
  if (src_len != 8) {
    return qoir_status_message__error_invalid_data;
  }
  return NULL;
}

QOIR_MAYBE_STATIC qoir_decode_result  //
qoir_decode(                          //
    const uint8_t* src_ptr,           //
    size_t src_len,                   //
    qoir_decode_options* options) {
  qoir_decode_result result = {0};
  if ((src_len < 44) ||
      (qoir_private_peek_u32le(src_ptr) != 0x52494F51)) {  // "QOIR"le.
    result.status_message = qoir_status_message__error_invalid_data;
    return result;
  }
  uint64_t qoir_chunk_payload_len = qoir_private_peek_u64le(src_ptr + 4);
  if ((qoir_chunk_payload_len < 8) ||
      (qoir_chunk_payload_len > 0x7FFFFFFFFFFFFFFFull) ||
      (qoir_chunk_payload_len > (src_len - 44))) {
    result.status_message = qoir_status_message__error_invalid_data;
    return result;
  }
  uint8_t* pixbuf_data = NULL;

  uint32_t header0 = qoir_private_peek_u32le(src_ptr + 12);
  uint32_t width_in_pixels = 0xFFFFFF & header0;
  qoir_pixel_format src_pixfmt = 0x0F & (header0 >> 24);
  switch (src_pixfmt) {
    case QOIR_PIXEL_FORMAT__BGRX:
    case QOIR_PIXEL_FORMAT__BGRA_NONPREMUL:
    case QOIR_PIXEL_FORMAT__BGRA_PREMUL:
      break;
    default:
      goto fail_invalid_data;
  }
  uint32_t header1 = qoir_private_peek_u32le(src_ptr + 16);
  uint32_t height_in_pixels = 0xFFFFFF & header1;

  qoir_pixel_format dst_pixfmt = (options && options->pixfmt)
                                     ? options->pixfmt
                                     : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  uint64_t dst_width_in_bytes =
      width_in_pixels * qoir_pixel_format__bytes_per_pixel(dst_pixfmt);

  bool seen_qpix = false;
  const uint8_t* sp = src_ptr + (12 + qoir_chunk_payload_len);
  size_t sn = src_len - (12 + qoir_chunk_payload_len);
  while (1) {
    if (sn < 12) {
      goto fail_invalid_data;
    }
    uint32_t chunk_type = qoir_private_peek_u32le(sp + 0);
    uint64_t payload_len = qoir_private_peek_u64le(sp + 4);
    if (payload_len > 0x7FFFFFFFFFFFFFFFull) {
      goto fail_invalid_data;
    }
    sp += 12;
    sn -= 12;

    if (chunk_type == 0x52494F51) {  // "QOIR"le.
      goto fail_invalid_data;
    } else if (chunk_type == 0x444E4551) {  // "QEND"le.
      if ((payload_len != 0) || (sn != 0)) {
        goto fail_invalid_data;
      }
      break;
    }

    // This chunk must be followed by at least the QEND chunk (12 bytes).
    if ((sn < payload_len) || ((sn - payload_len) < 12)) {
      goto fail_invalid_data;
    }

    if (chunk_type == 0x58495051) {  // "QPIX"le.
      if (seen_qpix) {
        goto fail_invalid_data;
      }
      seen_qpix = true;

      uint64_t pixbuf_len = dst_width_in_bytes * (uint64_t)height_in_pixels;
      if (pixbuf_len > SIZE_MAX) {
        result.status_message =
            qoir_status_message__error_unsupported_pixbuf_dimensions;
        return result;

      } else if (pixbuf_len > 0) {
        pixbuf_data = QOIR_MALLOC((size_t)pixbuf_len);
        if (!pixbuf_data) {
          result.status_message = qoir_status_message__error_out_of_memory;
          return result;
        }
        qoir_decode_buffer* decbuf = options ? options->decbuf : NULL;
        bool free_decbuf = false;
        if (!decbuf) {
          decbuf = (qoir_decode_buffer*)QOIR_MALLOC(sizeof(qoir_decode_buffer));
          if (!decbuf) {
            result.status_message = qoir_status_message__error_out_of_memory;
            QOIR_FREE(pixbuf_data);
            return result;
          }
          free_decbuf = true;
        }
        const char* status_message = qoir_private_decode_qpix_payload(
            decbuf, dst_pixfmt, width_in_pixels, height_in_pixels, pixbuf_data,
            dst_width_in_bytes, src_pixfmt, sp,
            payload_len + 8);  // See § for +8.
        if (free_decbuf) {
          QOIR_FREE(decbuf);
        }
        if (status_message) {
          result.status_message = status_message;
          QOIR_FREE(pixbuf_data);
          return result;
        }

      } else if (payload_len != 0) {
        goto fail_invalid_data;
      }
    }

    sp += payload_len;
    sn -= payload_len;
  }

  if (!seen_qpix) {
    goto fail_invalid_data;
  }

  result.owned_memory = pixbuf_data;
  result.dst_pixbuf.pixcfg.pixfmt = dst_pixfmt;
  result.dst_pixbuf.pixcfg.width_in_pixels = width_in_pixels;
  result.dst_pixbuf.pixcfg.height_in_pixels = height_in_pixels;
  result.dst_pixbuf.data = pixbuf_data;
  result.dst_pixbuf.stride_in_bytes = dst_width_in_bytes;
  return result;

fail_invalid_data:
  result.status_message = qoir_status_message__error_invalid_data;
  QOIR_FREE(pixbuf_data);
  return result;
}

// -------- QOIR Encode

static QOIR_ALWAYS_INLINE qoir_size_result  //
qoir_private_encode_tile_opcodes(           //
    uint8_t* dst_ptr,                       //
    const uint8_t* src_ptr,                 //
    uint32_t tw,                            //
    uint32_t th,                            //
    bool has_alpha) {
  // dists holds the log2 distance from zero (with modular arithmetic).
  //  - There is    1 element  such that (dists[i] <   1).
  //  - There are   2 elements such that (dists[i] <   2).
  //  - There are   4 elements such that (dists[i] <   4).
  //  - There are   8 elements such that (dists[i] <   8).
  //  - etc.
  //  - There are 128 elements such that (dists[i] < 128).
  //  - There are 256 elements such that (dists[i] < 256).
  static const uint8_t dists[256] = {
      0x00, 0x02, 0x04, 0x04, 0x08, 0x08, 0x08, 0x08,  //
      0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,  //
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,  //
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,  //
      0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  //
      0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  //
      0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  //
      0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  //
      0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  //
      0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  //
      0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  //
      0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  //
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,  //
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,  //
      0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,  //
      0x08, 0x08, 0x08, 0x08, 0x04, 0x04, 0x02, 0x01,  //
  };

  qoir_size_result result = {0};

  uint32_t run_length = 0;
  // The array-of-four-uint8_t elements are in R, G, B, A order.
  uint8_t color_cache[64][4];
  for (int i = 0; i < 64; i++) {
    color_cache[i][0] = 0x00;
    color_cache[i][1] = 0x00;
    color_cache[i][2] = 0x00;
    color_cache[i][3] = 0xFF;
  }

  uint8_t* dp = dst_ptr;
  const uint8_t* sp = src_ptr + QOIR_LITERALS_PRE_PADDING;
  const uint8_t* sq =
      src_ptr + QOIR_LITERALS_PRE_PADDING + (4 * (size_t)tw * (size_t)th);
  for (; sp < sq; sp += 4) {
    if (!memcmp(sp, sp - 4, 4)) {
      run_length++;
      if (run_length == 256) {
        *dp++ = 0xD7;  // QOIR_OP_RUNL
        *dp++ = 0xFF;
        run_length = 0;
      }
      continue;
    }

    if (run_length > 0) {
      if (run_length <= 26) {
        *dp++ = (uint8_t)(0x07 | ((run_length - 1) << 0x03));  // QOIR_OP_RUNS
        run_length = 0;
      } else {
        *dp++ = 0xD7;  // QOIR_OP_RUNL
        *dp++ = (uint8_t)(run_length - 1);
        run_length = 0;
      }
    }

    uint32_t hash = qoir_private_hash(sp);
    if (!memcmp(color_cache[hash], sp, 4)) {
      *dp++ = (uint8_t)(0x00 | (hash << 0x02));  // QOIR_OP_INDEX
      continue;
    }

    memcpy(color_cache[hash], sp, 4);

    // Either code path is equivalent to (but faster than):
    //   delta[i] = sp[i] - ((1 + sp[-4+i] + sp[(-4*QOIR_TILE_SIZE)+i]) / 2);
    // for i in 0..4.
    uint8_t delta[4];
#if defined(QOIR_USE_SIMD_SSE2)
    int curr32;   // Current pixel.
    int pred32l;  // Pixel left.
    int pred32a;  // Pixel above.
    memcpy(&curr32, sp, 4);
    memcpy(&pred32l, sp - 4, 4);
    memcpy(&pred32a, sp - (4 * QOIR_TILE_SIZE), 4);
    int delta32 = _mm_cvtsi128_si32(_mm_sub_epi8(
        _mm_cvtsi32_si128(curr32),
        _mm_avg_epu8(_mm_cvtsi32_si128(pred32l), _mm_cvtsi32_si128(pred32a))));
    memcpy(delta, &delta32, 4);
#else
    uint32_t curr32 = qoir_private_peek_u32le(sp);
    uint32_t pred32l = qoir_private_peek_u32le(sp - 4);
    uint32_t pred32a = qoir_private_peek_u32le(sp - (4 * QOIR_TILE_SIZE));
    qoir_private_poke_u32le(
        delta, QOIR_SWAR_PSUBB(curr32, QOIR_SWAR_PAVGB(pred32l, pred32a)));
#endif

    if (!has_alpha || (delta[3] == 0)) {
      uint8_t dist02 = dists[delta[0]] | dists[delta[2]];
      uint8_t dist1 = dists[delta[1]];
      uint8_t dist = dist02 | dist1;

      uint8_t d0d1 = delta[0] - delta[1];
      uint8_t d2d1 = delta[2] - delta[1];

      if (dist < 0x04) {
        *dp++ = 0x01 |                      // QOIR_OP_RGB2
                ((delta[0] + 2) << 0x02) |  //
                ((delta[1] + 2) << 0x04) |  //
                ((delta[2] + 2) << 0x06);

      } else if ((dist1 < 0x40) && ((dists[d0d1] | dists[d2d1]) < 0x10)) {
        *dp++ = 0x02 |                          // QOIR_OP_LUMA
                (((delta[1] + 0x20)) << 0x02);  //
        *dp++ = (((d0d1 + 0x08)) << 0x00) |     //
                (((d2d1 + 0x08)) << 0x04);

      } else if (dist < 0x80) {
        qoir_private_poke_u32le(
            dp,
            0x03 |  // QOIR_OP_RGB7
                ((uint32_t)(uint8_t)(delta[0] + 0x40) << 0x03) |
                ((uint32_t)(uint8_t)(delta[1] + 0x40) << 0x0A) |
                ((uint32_t)(uint8_t)(delta[2] + 0x40) << 0x11));
        dp += 3;

      } else {
        *dp++ = 0xF7;  // QOIR_OP_RGB8
        *dp++ = delta[0];
        *dp++ = delta[1];
        *dp++ = delta[2];
      }

    } else if ((delta[0] | delta[1] | delta[2]) == 0) {
      *dp++ = 0xFF;  // QOIR_OP_A8
      *dp++ = delta[3];

    } else {
      uint8_t dist =
          dists[delta[0]] | dists[delta[1]] | dists[delta[2]] | dists[delta[3]];
      if (dist < 0x04) {
        *dp++ = 0xDF;                       // QOIR_OP_RGBA2
        *dp++ = ((delta[0] + 2) << 0x00) |  //
                ((delta[1] + 2) << 0x02) |  //
                ((delta[2] + 2) << 0x04) |  //
                ((delta[3] + 2) << 0x06);
      } else if (dist < 0x10) {
        *dp++ = 0xE7;                       // QOIR_OP_RGBA4
        *dp++ = ((delta[0] + 8) << 0x00) |  //
                ((delta[1] + 8) << 0x04);   //
        *dp++ = ((delta[2] + 8) << 0x00) |  //
                ((delta[3] + 8) << 0x04);
      } else {
        *dp++ = 0xEF;  // QOIR_OP_RGBA8
        *dp++ = delta[0];
        *dp++ = delta[1];
        *dp++ = delta[2];
        *dp++ = delta[3];
      }
    }
  }

  if (run_length > 0) {
    if (run_length <= 26) {
      *dp++ = (uint8_t)(0x07 | ((run_length - 1) << 0x03));  // QOIR_OP_RUNS
      run_length = 0;
    } else {
      *dp++ = 0xD7;  // QOIR_OP_RUNL
      *dp++ = (uint8_t)(run_length - 1);
      run_length = 0;
    }
  }

  result.value = (size_t)(dp - dst_ptr);
  return result;
}

static qoir_size_result                       //
qoir_private_encode_tile_opcodes_sans_alpha(  //
    uint8_t* dst_ptr,                         //
    const uint8_t* src_ptr,                   //
    uint32_t tw,                              //
    uint32_t th) {
  return qoir_private_encode_tile_opcodes(dst_ptr, src_ptr, tw, th, false);
}

static qoir_size_result                       //
qoir_private_encode_tile_opcodes_with_alpha(  //
    uint8_t* dst_ptr,                         //
    const uint8_t* src_ptr,                   //
    uint32_t tw,                              //
    uint32_t th) {
  return qoir_private_encode_tile_opcodes(dst_ptr, src_ptr, tw, th, true);
}

static qoir_size_result            //
qoir_private_encode_qpix_payload(  //
    qoir_encode_buffer* encbuf,    //
    uint8_t* dst_ptr,              //
    qoir_pixel_buffer* src_pixbuf) {
  qoir_size_result result = {0};

  size_t height_in_tiles =
      (src_pixbuf->pixcfg.height_in_pixels + QOIR_TILE_MASK) >> QOIR_TILE_SHIFT;
  size_t width_in_tiles =
      (src_pixbuf->pixcfg.width_in_pixels + QOIR_TILE_MASK) >> QOIR_TILE_SHIFT;
  if ((height_in_tiles == 0) || (width_in_tiles == 0)) {
    return result;
  }
  size_t ty1 = (height_in_tiles - 1) << QOIR_TILE_SHIFT;
  size_t tx1 = (width_in_tiles - 1) << QOIR_TILE_SHIFT;

  qoir_private_swizzle_func swizzle = NULL;
  qoir_size_result (*encode_func)(uint8_t * dst_ptr,       //
                                  const uint8_t* src_ptr,  //
                                  uint32_t tw,             //
                                  uint32_t th) = NULL;
  switch (src_pixbuf->pixcfg.pixfmt) {
    case QOIR_PIXEL_FORMAT__RGB:
      swizzle = qoir_private_swizzle__rgba__rgb;
      encode_func = qoir_private_encode_tile_opcodes_sans_alpha;
      break;
    case QOIR_PIXEL_FORMAT__RGBA_NONPREMUL:
      swizzle = qoir_private_swizzle__copy_4;
      encode_func = qoir_private_encode_tile_opcodes_with_alpha;
      break;
    default:
      result.status_message = qoir_status_message__error_unsupported_pixfmt;
      return result;
  }
  size_t num_src_channels =
      qoir_pixel_format__bytes_per_pixel(src_pixbuf->pixcfg.pixfmt);
  uint8_t* dp = dst_ptr;

  uint8_t* literals_pre_padding = encbuf->private_impl.literals;
  for (int i = 0; i < QOIR_LITERALS_PRE_PADDING; i += 4) {
    literals_pre_padding[i + 0] = 0x00;
    literals_pre_padding[i + 1] = 0x00;
    literals_pre_padding[i + 2] = 0x00;
    literals_pre_padding[i + 3] = 0xFF;
  }

  // ty, tx, tw and th are the tile's top-left offset, width and height, all
  // measured in pixels.
  for (size_t ty = 0; ty <= ty1; ty += QOIR_TILE_SIZE) {
    for (size_t tx = 0; tx <= tx1; tx += QOIR_TILE_SIZE) {
      size_t tw = qoir_private_tile_dimension(
          tx < tx1, src_pixbuf->pixcfg.width_in_pixels);
      size_t th = qoir_private_tile_dimension(
          ty < ty1, src_pixbuf->pixcfg.height_in_pixels);

      const uint8_t* sp = src_pixbuf->data +
                          (src_pixbuf->stride_in_bytes * ty) +
                          (num_src_channels * tx);
      (*swizzle)(encbuf->private_impl.literals + QOIR_LITERALS_PRE_PADDING,
                 4 * tw,                           //
                 sp, src_pixbuf->stride_in_bytes,  //
                 tw, th);

      qoir_size_result r0 = (*encode_func)(
          encbuf->private_impl.opcodes, encbuf->private_impl.literals, tw, th);
      if (r0.status_message) {
        result.status_message = r0.status_message;
        return r0;
      }
      size_t literals_len = 4 * tw * th;
      if (r0.value >= literals_len) {
        // Use the Literals or LZ4-Literals tile format.
        qoir_size_result r1 = qoir_lz4_block_encode(
            dp + 4, QOIR_TILE_LZ4_COMPRESSION_WORST_CASE,
            encbuf->private_impl.literals + QOIR_LITERALS_PRE_PADDING,
            literals_len);
        if (!r1.status_message && (r1.value < r0.value)) {
          qoir_private_poke_u32le(dp, 0x02000000 | (uint32_t)r1.value);
          dp += 4 + r1.value;
        } else {
          memcpy(dp + 4,
                 encbuf->private_impl.literals + QOIR_LITERALS_PRE_PADDING,
                 literals_len);
          qoir_private_poke_u32le(dp, 0x00000000 | (uint32_t)literals_len);
          dp += 4 + literals_len;
        }

      } else {
        // Use the Opcodes or LZ4-Opcodes tile format.
        qoir_size_result r1 =
            qoir_lz4_block_encode(dp + 4, QOIR_TILE_LZ4_COMPRESSION_WORST_CASE,
                                  encbuf->private_impl.opcodes, r0.value);
        if (!r1.status_message && (r1.value < r0.value)) {
          qoir_private_poke_u32le(dp, 0x03000000 | (uint32_t)r1.value);
          dp += 4 + r1.value;
        } else {
          memcpy(dp + 4, encbuf->private_impl.opcodes, r0.value);
          qoir_private_poke_u32le(dp, 0x01000000 | (uint32_t)r0.value);
          dp += 4 + r0.value;
        }
      }
    }
  }

  result.value = (size_t)(dp - dst_ptr);
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
  } else if ((src_pixbuf->pixcfg.width_in_pixels > 0xFFFFFF) ||
             (src_pixbuf->pixcfg.height_in_pixels > 0xFFFFFF)) {
    result.status_message =
        qoir_status_message__error_unsupported_pixbuf_dimensions;
    return result;
  }

  uint32_t num_channels = 0;
  switch (src_pixbuf->pixcfg.pixfmt) {
    case QOIR_PIXEL_FORMAT__RGB:
      num_channels = 3;
      break;
    case QOIR_PIXEL_FORMAT__RGBA_NONPREMUL:
      num_channels = 4;
      break;
    default:
      result.status_message = qoir_status_message__error_unsupported_pixfmt;
      return result;
  }

  if (src_pixbuf->stride_in_bytes !=
      ((uint64_t)num_channels * src_pixbuf->pixcfg.width_in_pixels)) {
    result.status_message = qoir_status_message__error_unsupported_pixbuf;
    return result;
  }

  uint64_t width_in_tiles =
      (src_pixbuf->pixcfg.width_in_pixels + QOIR_TILE_MASK) >> QOIR_TILE_SHIFT;
  uint64_t height_in_tiles =
      (src_pixbuf->pixcfg.height_in_pixels + QOIR_TILE_MASK) >> QOIR_TILE_SHIFT;
  uint64_t tile_len_worst_case =
      4 + (4 * QOIR_TS2);  // Prefix + literal format.
  uint64_t dst_len_worst_case =
      (width_in_tiles * height_in_tiles * tile_len_worst_case) +
      44 +  // QOIR, QPIX and QEND chunk headers are 12 bytes each.
            // QOIR also has an 8 byte payload.
      (QOIR_TILE_LZ4_COMPRESSION_WORST_CASE -
       (4 * QOIR_TS2));  // We might temporarily write more than (4 * QOIR_TS2)
                         // bytes when LZ4 compressing each tile.
  if (dst_len_worst_case > SIZE_MAX) {
    result.status_message =
        qoir_status_message__error_unsupported_pixbuf_dimensions;
    return result;
  }
  uint8_t* dst_ptr = QOIR_MALLOC((size_t)dst_len_worst_case);
  if (!dst_ptr) {
    result.status_message = qoir_status_message__error_out_of_memory;
    return result;
  }

  // QOIR chunk.
  qoir_private_poke_u32le(dst_ptr + 0, 0x52494F51);  // "QOIR"le.
  qoir_private_poke_u64le(dst_ptr + 4, 8);
  qoir_private_poke_u32le(dst_ptr + 12, src_pixbuf->pixcfg.width_in_pixels);
  qoir_private_poke_u32le(dst_ptr + 16, src_pixbuf->pixcfg.height_in_pixels);
  dst_ptr[15] = (num_channels == 3) ? QOIR_PIXEL_FORMAT__BGRX
                                    : QOIR_PIXEL_FORMAT__BGRA_NONPREMUL;

  // QPIX chunk.
  qoir_private_poke_u32le(dst_ptr + 20, 0x58495051);  // "QPIX"le.
  qoir_encode_buffer* encbuf = options ? options->encbuf : NULL;
  bool free_encbuf = false;
  if (!encbuf) {
    encbuf = (qoir_encode_buffer*)QOIR_MALLOC(sizeof(qoir_encode_buffer));
    if (!encbuf) {
      result.status_message = qoir_status_message__error_out_of_memory;
      QOIR_FREE(dst_ptr);
      return result;
    }
    free_encbuf = true;
  }
  qoir_size_result r =
      qoir_private_encode_qpix_payload(encbuf, dst_ptr + 32, src_pixbuf);
  if (free_encbuf) {
    QOIR_FREE(encbuf);
  }
  if (r.status_message) {
    result.status_message = r.status_message;
    QOIR_FREE(dst_ptr);
    return result;
  } else if ((uint64_t)r.value > 0x7FFFFFFFFFFFFFFFull) {
    result.status_message =
        qoir_status_message__error_unsupported_pixbuf_dimensions;
    QOIR_FREE(dst_ptr);
    return result;
  }
  qoir_private_poke_u64le(dst_ptr + 24, r.value);

  // QEND chunk.
  qoir_private_poke_u32le(dst_ptr + 32 + r.value, 0x444E4551);  // "QEND"le.
  qoir_private_poke_u64le(dst_ptr + 36 + r.value, 0);

  result.owned_memory = dst_ptr;
  result.dst_ptr = dst_ptr;
  result.dst_len = 44 + r.value;
  return result;
}

// --------

#undef QOIR_ALWAYS_INLINE
#undef QOIR_FREE
#undef QOIR_LZ4_HASH_TABLE_SIZE
#undef QOIR_MALLOC
#undef QOIR_SWAR_PAVGB
#undef QOIR_SWAR_PSUBB
#undef QOIR_USE_MEMCPY_LE_PEEK_POKE
#undef QOIR_USE_SIMD_SSE2

// ================================ -Private Implementation

#endif  // QOIR_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // QOIR_INCLUDE_GUARD
