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

#include <errno.h>
#include <stdio.h>

#define QOIR_IMPLEMENTATION
#include "../src/qoir.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#include "../third_party/stb/stb_image.h"
#include "../third_party/stb/stb_image_write.h"

// Utilities should be #include'd after qoir.h
#include "../util/load_file.c"

// ----

static uint32_t  //
crc32(uint8_t* p, size_t n) {
  // These numbers (derived from the 0xEDB88320 IEEE polynomial) is the result
  // of Go's "crc32.MakeTable(crc32.IEEE)" in C syntax.
  static const uint32_t table[256] = {
      0x00000000u, 0x77073096u, 0xEE0E612Cu, 0x990951BAu, 0x076DC419u,
      0x706AF48Fu, 0xE963A535u, 0x9E6495A3u, 0x0EDB8832u, 0x79DCB8A4u,
      0xE0D5E91Eu, 0x97D2D988u, 0x09B64C2Bu, 0x7EB17CBDu, 0xE7B82D07u,
      0x90BF1D91u, 0x1DB71064u, 0x6AB020F2u, 0xF3B97148u, 0x84BE41DEu,
      0x1ADAD47Du, 0x6DDDE4EBu, 0xF4D4B551u, 0x83D385C7u, 0x136C9856u,
      0x646BA8C0u, 0xFD62F97Au, 0x8A65C9ECu, 0x14015C4Fu, 0x63066CD9u,
      0xFA0F3D63u, 0x8D080DF5u, 0x3B6E20C8u, 0x4C69105Eu, 0xD56041E4u,
      0xA2677172u, 0x3C03E4D1u, 0x4B04D447u, 0xD20D85FDu, 0xA50AB56Bu,
      0x35B5A8FAu, 0x42B2986Cu, 0xDBBBC9D6u, 0xACBCF940u, 0x32D86CE3u,
      0x45DF5C75u, 0xDCD60DCFu, 0xABD13D59u, 0x26D930ACu, 0x51DE003Au,
      0xC8D75180u, 0xBFD06116u, 0x21B4F4B5u, 0x56B3C423u, 0xCFBA9599u,
      0xB8BDA50Fu, 0x2802B89Eu, 0x5F058808u, 0xC60CD9B2u, 0xB10BE924u,
      0x2F6F7C87u, 0x58684C11u, 0xC1611DABu, 0xB6662D3Du, 0x76DC4190u,
      0x01DB7106u, 0x98D220BCu, 0xEFD5102Au, 0x71B18589u, 0x06B6B51Fu,
      0x9FBFE4A5u, 0xE8B8D433u, 0x7807C9A2u, 0x0F00F934u, 0x9609A88Eu,
      0xE10E9818u, 0x7F6A0DBBu, 0x086D3D2Du, 0x91646C97u, 0xE6635C01u,
      0x6B6B51F4u, 0x1C6C6162u, 0x856530D8u, 0xF262004Eu, 0x6C0695EDu,
      0x1B01A57Bu, 0x8208F4C1u, 0xF50FC457u, 0x65B0D9C6u, 0x12B7E950u,
      0x8BBEB8EAu, 0xFCB9887Cu, 0x62DD1DDFu, 0x15DA2D49u, 0x8CD37CF3u,
      0xFBD44C65u, 0x4DB26158u, 0x3AB551CEu, 0xA3BC0074u, 0xD4BB30E2u,
      0x4ADFA541u, 0x3DD895D7u, 0xA4D1C46Du, 0xD3D6F4FBu, 0x4369E96Au,
      0x346ED9FCu, 0xAD678846u, 0xDA60B8D0u, 0x44042D73u, 0x33031DE5u,
      0xAA0A4C5Fu, 0xDD0D7CC9u, 0x5005713Cu, 0x270241AAu, 0xBE0B1010u,
      0xC90C2086u, 0x5768B525u, 0x206F85B3u, 0xB966D409u, 0xCE61E49Fu,
      0x5EDEF90Eu, 0x29D9C998u, 0xB0D09822u, 0xC7D7A8B4u, 0x59B33D17u,
      0x2EB40D81u, 0xB7BD5C3Bu, 0xC0BA6CADu, 0xEDB88320u, 0x9ABFB3B6u,
      0x03B6E20Cu, 0x74B1D29Au, 0xEAD54739u, 0x9DD277AFu, 0x04DB2615u,
      0x73DC1683u, 0xE3630B12u, 0x94643B84u, 0x0D6D6A3Eu, 0x7A6A5AA8u,
      0xE40ECF0Bu, 0x9309FF9Du, 0x0A00AE27u, 0x7D079EB1u, 0xF00F9344u,
      0x8708A3D2u, 0x1E01F268u, 0x6906C2FEu, 0xF762575Du, 0x806567CBu,
      0x196C3671u, 0x6E6B06E7u, 0xFED41B76u, 0x89D32BE0u, 0x10DA7A5Au,
      0x67DD4ACCu, 0xF9B9DF6Fu, 0x8EBEEFF9u, 0x17B7BE43u, 0x60B08ED5u,
      0xD6D6A3E8u, 0xA1D1937Eu, 0x38D8C2C4u, 0x4FDFF252u, 0xD1BB67F1u,
      0xA6BC5767u, 0x3FB506DDu, 0x48B2364Bu, 0xD80D2BDAu, 0xAF0A1B4Cu,
      0x36034AF6u, 0x41047A60u, 0xDF60EFC3u, 0xA867DF55u, 0x316E8EEFu,
      0x4669BE79u, 0xCB61B38Cu, 0xBC66831Au, 0x256FD2A0u, 0x5268E236u,
      0xCC0C7795u, 0xBB0B4703u, 0x220216B9u, 0x5505262Fu, 0xC5BA3BBEu,
      0xB2BD0B28u, 0x2BB45A92u, 0x5CB36A04u, 0xC2D7FFA7u, 0xB5D0CF31u,
      0x2CD99E8Bu, 0x5BDEAE1Du, 0x9B64C2B0u, 0xEC63F226u, 0x756AA39Cu,
      0x026D930Au, 0x9C0906A9u, 0xEB0E363Fu, 0x72076785u, 0x05005713u,
      0x95BF4A82u, 0xE2B87A14u, 0x7BB12BAEu, 0x0CB61B38u, 0x92D28E9Bu,
      0xE5D5BE0Du, 0x7CDCEFB7u, 0x0BDBDF21u, 0x86D3D2D4u, 0xF1D4E242u,
      0x68DDB3F8u, 0x1FDA836Eu, 0x81BE16CDu, 0xF6B9265Bu, 0x6FB077E1u,
      0x18B74777u, 0x88085AE6u, 0xFF0F6A70u, 0x66063BCAu, 0x11010B5Cu,
      0x8F659EFFu, 0xF862AE69u, 0x616BFFD3u, 0x166CCF45u, 0xA00AE278u,
      0xD70DD2EEu, 0x4E048354u, 0x3903B3C2u, 0xA7672661u, 0xD06016F7u,
      0x4969474Du, 0x3E6E77DBu, 0xAED16A4Au, 0xD9D65ADCu, 0x40DF0B66u,
      0x37D83BF0u, 0xA9BCAE53u, 0xDEBB9EC5u, 0x47B2CF7Fu, 0x30B5FFE9u,
      0xBDBDF21Cu, 0xCABAC28Au, 0x53B39330u, 0x24B4A3A6u, 0xBAD03605u,
      0xCDD70693u, 0x54DE5729u, 0x23D967BFu, 0xB3667A2Eu, 0xC4614AB8u,
      0x5D681B02u, 0x2A6F2B94u, 0xB40BBE37u, 0xC30C8EA1u, 0x5A05DF1Bu,
      0x2D02EF8Du,
  };

  uint32_t ret = 0xFFFFFFFFu;
  for (; n > 0; n--) {
    ret = table[(ret & 0xFF) ^ *p] ^ (ret >> 8);
    p++;
  }
  return ret ^ 0xFFFFFFFFu;
}

static inline uint32_t  //
peek_u32be(const uint8_t* p) {
  return ((uint32_t)(p[0]) << 24) | ((uint32_t)(p[1]) << 16) |
         ((uint32_t)(p[2]) << 8) | ((uint32_t)(p[3]) << 0);
}

static inline void  //
poke_u32be(uint8_t* p, uint32_t x) {
  p[0] = (uint8_t)(x >> 24);
  p[1] = (uint8_t)(x >> 16);
  p[2] = (uint8_t)(x >> 8);
  p[3] = (uint8_t)(x >> 0);
}

static inline bool  //
u64_overflow_add(uint64_t* x, uint64_t y) {
  uint64_t old = *x;
  *x += y;
  return *x < old;
}

// XMP (XML) is stored in PNG as an iTXt chunk with this magic prefix.
//
// There are five NUL bytes at the end. The first terminates the keyword. The
// second and third are compression flag and method. The last two terminate the
// empty language tag and translated keyword. See
// https://www.w3.org/TR/2003/REC-PNG-20031110/#11iTXt
static const char* xmp_magic_ptr = "XML:com.adobe.xmp\x00\x00\x00\x00\x00";
static const size_t xmp_magic_len = 22;

// ----

typedef struct extract_png_metadata_result_struct {
  const uint8_t* cicp_ptr;
  size_t cicp_len;

  const uint8_t* exif_ptr;
  size_t exif_len;

  const uint8_t* iccp_ptr;
  size_t iccp_len;

  const uint8_t* xmp_ptr;
  size_t xmp_len;
} extract_png_metadata_result;

static extract_png_metadata_result  //
extract_png_metadata(const uint8_t* src_ptr, size_t src_len) {
  extract_png_metadata_result result = {0};
  if ((src_len < 8) || (peek_u32be(src_ptr + 0) != 0x89504E47) ||
      (peek_u32be(src_ptr + 4) != 0x0D0A1A0A)) {
    return result;
  }
  src_ptr += 8;
  src_len -= 8;
  while (src_len >= 8) {
    uint32_t chunk_len = peek_u32be(src_ptr + 0);
    uint32_t chunk_tag = peek_u32be(src_ptr + 4);
    src_ptr += 8;
    src_len -= 8;
    if (src_len < chunk_len) {
      return result;
    }
    switch (chunk_tag) {
      case 0x63494350: {  // 'cICP'be
        result.cicp_ptr = src_ptr;
        result.cicp_len = chunk_len;
        break;
      }
      case 0x65584966: {  // 'eXIf'be
        result.exif_ptr = src_ptr;
        result.exif_len = chunk_len;
        break;
      }
      case 0x69434350: {  // 'iCCP'be
        result.iccp_ptr = src_ptr;
        result.iccp_len = chunk_len;
        break;
      }
      case 0x69545874: {  // 'iTXt'be
        if ((chunk_len >= xmp_magic_len) &&
            (memcmp(xmp_magic_ptr, src_ptr, xmp_magic_len) == 0)) {
          result.xmp_ptr = src_ptr + xmp_magic_len;
          result.xmp_len = chunk_len - xmp_magic_len;
        }
        break;
      }
    }
    src_ptr += chunk_len;
    src_len -= chunk_len;
    if (src_len < 4) {
      return result;
    }
    src_ptr += 4;
    src_len -= 4;
  }
  return result;
}

load_file_result  //
convert_from_png_to_qoir(const uint8_t* src_ptr,
                         size_t src_len,
                         const qoir_encode_options* enc_opts) {
  load_file_result result = {0};

  int width = 0;
  int height = 0;
  int channels = 0;
  if (!stbi_info_from_memory(src_ptr, src_len, &width, &height, &channels)) {
    result.status_message = "#main: stbi_info_from_memory failed";
    return result;
  } else if (channels != 3) {
    channels = 4;
  }
  int ignored = 0;
  unsigned char* pixbuf_data = stbi_load_from_memory(
      src_ptr, src_len, &ignored, &ignored, &ignored, channels);
  if (!pixbuf_data) {
    result.status_message = "#main: stbi_load_from_memory failed";
    return result;
  } else if ((width > 0xFFFFFF) || (height > 0xFFFFFF)) {
    stbi_image_free(pixbuf_data);
    result.status_message = "#main: image is too large";
    return result;
  }

  qoir_encode_options local_enc_opts = {0};
  if (enc_opts) {
    memcpy(&local_enc_opts, enc_opts, sizeof(qoir_encode_options));
  }
  extract_png_metadata_result png_metadata =
      extract_png_metadata(src_ptr, src_len);
  if (local_enc_opts.metadata_cicp_len == 0) {
    local_enc_opts.metadata_cicp_ptr = png_metadata.cicp_ptr;
    local_enc_opts.metadata_cicp_len = png_metadata.cicp_len;
  }
  if (local_enc_opts.metadata_iccp_len == 0) {
    local_enc_opts.metadata_iccp_ptr = png_metadata.iccp_ptr;
    local_enc_opts.metadata_iccp_len = png_metadata.iccp_len;
  }
  if (local_enc_opts.metadata_exif_len == 0) {
    local_enc_opts.metadata_exif_ptr = png_metadata.exif_ptr;
    local_enc_opts.metadata_exif_len = png_metadata.exif_len;
  }
  if (local_enc_opts.metadata_xmp_len == 0) {
    local_enc_opts.metadata_xmp_ptr = png_metadata.xmp_ptr;
    local_enc_opts.metadata_xmp_len = png_metadata.xmp_len;
  }

  qoir_pixel_buffer pixbuf;
  pixbuf.pixcfg.pixfmt = (channels == 3) ? QOIR_PIXEL_FORMAT__RGB
                                         : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  pixbuf.pixcfg.width_in_pixels = width;
  pixbuf.pixcfg.height_in_pixels = height;
  pixbuf.data = (uint8_t*)pixbuf_data;
  pixbuf.stride_in_bytes = (size_t)channels * (size_t)width;
  qoir_encode_result enc = qoir_encode(&pixbuf, &local_enc_opts);
  stbi_image_free(pixbuf_data);
  if (enc.status_message) {
    free(enc.owned_memory);
    result.status_message = enc.status_message;
    return result;
  }

  result.owned_memory = enc.owned_memory;
  result.dst_ptr = enc.dst_ptr;
  result.dst_len = enc.dst_len;
  return result;
}

// ----

typedef struct my_stbi_write_func_context_struct {
  load_file_result* lfr;
  qoir_decode_result* qdr;
} my_stbi_write_func_context;

void  //
my_stbi_write_func(void* context, void* data, int size) {
  my_stbi_write_func_context* ctx = (my_stbi_write_func_context*)context;
  if (!ctx || !ctx->lfr || !ctx->qdr) {
    return;
  } else if (size < (33 + 12)) {
    // Every valid PNG file must be at least:
    //  - an 8 byte magic identifier + a 25 byte IHDR chunk
    //  - a 12 byte IEND chunk.
    ctx->lfr->status_message = "#main: invalid write-function size";
    return;
  }

  // Set n equal to the original PNG size (as encoded by stb_image_write) plus
  // additional space to hold metadata chunks. PNG chunks are framed by 4 bytes
  // chunk length, 4 bytes chunk tag and 4 bytes CRC-32 checksum.
  uint64_t n = (uint64_t)size;
  if ((ctx->qdr->metadata_cicp_len > 0x7FFFFFFF) ||
      (ctx->qdr->metadata_iccp_len > 0x7FFFFFFF) ||
      (ctx->qdr->metadata_exif_len > 0x7FFFFFFF) ||
      (ctx->qdr->metadata_xmp_len > 0x7FFFFFFF)) {
    ctx->lfr->status_message = "#main: unsupported metadata length";
    return;
  }
  bool overflow = false;
  if (ctx->qdr->metadata_cicp_len) {
    overflow = overflow || u64_overflow_add(&n, 12) ||
               u64_overflow_add(&n, ctx->qdr->metadata_cicp_len);
  } else if (ctx->qdr->metadata_iccp_len) {
    overflow = overflow || u64_overflow_add(&n, 12) ||
               u64_overflow_add(&n, ctx->qdr->metadata_iccp_len);
  }
  if (ctx->qdr->metadata_exif_len) {
    overflow = overflow || u64_overflow_add(&n, 12) ||
               u64_overflow_add(&n, ctx->qdr->metadata_exif_len);
  }
  if (ctx->qdr->metadata_xmp_len) {
    overflow = overflow || u64_overflow_add(&n, 12 + xmp_magic_len) ||
               u64_overflow_add(&n, ctx->qdr->metadata_xmp_len);
  }
  if (overflow || (n > SIZE_MAX)) {
    ctx->lfr->status_message = "#main: unsupported metadata length";
    return;
  }

  // Allocate memory.
  uint8_t* p = malloc(n);
  if (!p) {
    ctx->lfr->status_message = "#main: out of memory";
    return;
  }
  ctx->lfr->owned_memory = p;
  ctx->lfr->dst_ptr = p;
  ctx->lfr->dst_len = n;

  // Interleave metadata chunks between the IHDR / IDAT / IEND chunks encoded
  // by stb_image_write.

  // Magic identifier (8 bytes) and IHDR chunk (25 bytes).
  memcpy(p, data, 33);
  p += 33;
  n -= 33;
  data += 33;
  size -= 33;

  // Insert color space related metadata before the IDAT chunks.
  //
  // This *is* if-else-if. We write only one (the 'best') color space chunk.
  if (ctx->qdr->metadata_cicp_len) {
    uint32_t len = (uint32_t)ctx->qdr->metadata_cicp_len;
    poke_u32be(p + 0, len);
    poke_u32be(p + 4, 0x63494350);  // "cICP"be.
    memcpy(p + 8, ctx->qdr->metadata_cicp_ptr, len);
    poke_u32be(p + 8 + len, crc32(p + 4, 4 + len));
    p += 12 + len;
    n -= 12 + len;
  } else if (ctx->qdr->metadata_iccp_len) {
    uint32_t len = (uint32_t)ctx->qdr->metadata_iccp_len;
    poke_u32be(p + 0, len);
    poke_u32be(p + 4, 0x69434350);  // "iCCP"be.
    memcpy(p + 8, ctx->qdr->metadata_iccp_ptr, len);
    poke_u32be(p + 8 + len, crc32(p + 4, 4 + len));
    p += 12 + len;
    n -= 12 + len;
  }

  // IDAT chunks.
  memcpy(p, data, size - 12);
  p += size - 12;
  n -= size - 12;
  data += size - 12;
  size = 12;

  // Insert other metadata after the IDAT chunks.
  //
  // This *is not* if-else-if. The chunks are independent.
  if (ctx->qdr->metadata_exif_len) {
    uint32_t len = (uint32_t)ctx->qdr->metadata_exif_len;
    poke_u32be(p + 0, len);
    poke_u32be(p + 4, 0x65584966);  // "eXIf"be.
    memcpy(p + 8, ctx->qdr->metadata_exif_ptr, len);
    poke_u32be(p + 8 + len, crc32(p + 4, 4 + len));
    p += 12 + len;
    n -= 12 + len;
  }
  if (ctx->qdr->metadata_xmp_len) {
    uint32_t len = (uint32_t)ctx->qdr->metadata_xmp_len;
    poke_u32be(p + 0, len);
    poke_u32be(p + 4, 0x69545874);  // "iTXt"be.
    memcpy(p + 8, xmp_magic_ptr, xmp_magic_len);
    memcpy(p + 8 + xmp_magic_len, ctx->qdr->metadata_xmp_ptr, len);
    poke_u32be(p + 8 + xmp_magic_len + len,
               crc32(p + 4, 4 + xmp_magic_len + len));
    p += 12 + xmp_magic_len + len;
    n -= 12 + xmp_magic_len + len;
  }

  // IEND chunk.
  memcpy(p, data, 12);
}

load_file_result  //
convert_from_qoir_to_png(const uint8_t* src_ptr, size_t src_len) {
  load_file_result result = {0};

  qoir_decode_pixel_configuration_result cfg =
      qoir_decode_pixel_configuration(src_ptr, src_len);
  if (cfg.status_message) {
    result.status_message = cfg.status_message;
    return result;
  }

  qoir_decode_options decopts = {0};
  decopts.pixfmt = (cfg.dst_pixcfg.pixfmt == QOIR_PIXEL_FORMAT__BGRX)
                       ? QOIR_PIXEL_FORMAT__RGB
                       : QOIR_PIXEL_FORMAT__RGBA_NONPREMUL;
  qoir_decode_result dec = qoir_decode(src_ptr, src_len, &decopts);
  if (dec.status_message) {
    free(dec.owned_memory);
    result.status_message = dec.status_message;
    return result;
  }

  my_stbi_write_func_context context;
  context.lfr = &result;
  context.qdr = &dec;
  if (!stbi_write_png_to_func(
          &my_stbi_write_func, &context, dec.dst_pixbuf.pixcfg.width_in_pixels,
          dec.dst_pixbuf.pixcfg.height_in_pixels,
          qoir_pixel_format__bytes_per_pixel(decopts.pixfmt),
          dec.dst_pixbuf.data, dec.dst_pixbuf.stride_in_bytes)) {
    free(dec.owned_memory);
    result.status_message = "#main: stbi_write_png failed";
    return result;
  }

  free(dec.owned_memory);
  return result;
}

// ----

int  //
usage() {
  fprintf(stderr,
          "Usage:\n"                                              //
          "  qoirconv --lossiness=L --dither foo.png foo.qoir\n"  //
          "  qoirconv foo.qoir foo.png\n"                         //
          "  L ranges in 0 ..= 7; the default (0) means lossless\n");
  return 1;
}

int  //
main(int argc, char** argv) {
  const char* arg_src = NULL;
  const char* arg_dst = NULL;
  qoir_encode_options encopts = {0};

  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];
    if (arg[0] != '-') {
      if (!arg_src) {
        arg_src = arg;
        continue;
      } else if (!arg_dst) {
        arg_dst = arg;
        continue;
      }
      return usage();
    }
    arg++;
    if (!strncmp(arg, "-dither", 7)) {
      encopts.dither = 1;
      continue;
    } else if (!strncmp(arg, "-lossiness=", 11)) {
      long int x = strtol(arg + 11, NULL, 10);
      if ((0 <= x) && (x < 8)) {
        encopts.lossiness = x;
        continue;
      }
    }
    return usage();
  }

  FILE* fin = stdin;
  if (arg_src) {
    fin = fopen(arg_src, "rb");
    if (!fin) {
      fprintf(stderr, "qoirconv: could not open %s: %s\n", arg_src,
              strerror(errno));
      return 1;
    }
  }

  load_file_result r0 = load_file(fin, UINT64_MAX);
  if (r0.status_message) {
    fprintf(stderr, "qoirconv: could not load %s: %s\n",
            arg_src ? arg_src : "<stdin>", r0.status_message);
    return 1;
  }

  // We would normally have to call free(r0.owned_memory) at some point, but
  // we're in the main function, so just exit the process (and 'leak' the
  // memory) when this function returns. Ditto for r1.owned_memory below.

  load_file_result r1 = {0};
  r1.status_message = "#main: unsupported file format";
  if (r0.dst_len > 0) {
    switch (r0.dst_ptr[0]) {
      case 0x51:
        r1 = convert_from_qoir_to_png(r0.dst_ptr, r0.dst_len);
        break;
      case 0x89:
        r1 = convert_from_png_to_qoir(r0.dst_ptr, r0.dst_len, &encopts);
        break;
    }
  }
  if (r1.status_message) {
    fprintf(stderr, "qoirconv: could not convert %s: %s\n",
            arg_src ? arg_src : "<stdin>", r1.status_message);
    return 1;
  }

  FILE* fout = stdout;
  if (arg_dst) {
    fout = fopen(arg_dst, "wb");
    if (!fout) {
      fprintf(stderr, "qoirconv: could not open %s: %s\n", arg_dst,
              strerror(errno));
      return 1;
    }
  }
  if (r1.dst_len != fwrite(r1.dst_ptr, 1, r1.dst_len, fout)) {
    fprintf(stderr, "qoirconv: could not save %s: %s\n",
            arg_dst ? arg_dst : "<stdout>", strerror(errno));
    return 1;
  }

  return 0;
}
