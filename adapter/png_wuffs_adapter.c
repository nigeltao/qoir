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

#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__STATIC_FUNCTIONS
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__ZLIB
#include "../third_party/wuffs/wuffs-v0.3.c"

#ifdef __cplusplus
extern "C" {
#endif

extern const char error_not_implemented[];

qoir_decode_result           //
my_decode_png_wuffs(         //
    const uint8_t* src_ptr,  //
    const size_t src_len) {
  uint8_t* pixbuf_ptr = NULL;
  uint8_t* workbuf_ptr = NULL;
  wuffs_base__status status = {0};

  do {
    // static avoids an allocation every time this function is called, but it
    // means that this function is not thread-safe.
    static wuffs_png__decoder dec;

    // Initialize.
    status = wuffs_png__decoder__initialize(&dec, sizeof(dec), WUFFS_VERSION,
                                            WUFFS_INITIALIZE__DEFAULT_OPTIONS);
    if (status.repr) {
      goto fail;
    }
    wuffs_png__decoder__set_quirk_enabled(
        &dec, WUFFS_BASE__QUIRK_IGNORE_CHECKSUM, true);
    wuffs_base__io_buffer src =
        wuffs_base__ptr_u8__reader((uint8_t*)src_ptr, src_len, true);

    // Decode the image configuration.
    wuffs_base__image_config imgcfg;
    status = wuffs_png__decoder__decode_image_config(&dec, &imgcfg, &src);
    if (status.repr) {
      goto fail;
    }
    uint32_t w = wuffs_base__pixel_config__width(&imgcfg.pixcfg);
    uint32_t h = wuffs_base__pixel_config__height(&imgcfg.pixcfg);
    if ((w > 0xFFFFFF) || (h > 0xFFFFFF)) {
      status.repr = "#my_decode_png_wuffs: image is too large";
      goto fail;
    }
    uint32_t num_channels;
    switch (wuffs_base__pixel_config__pixel_format(&imgcfg.pixcfg).repr) {
      case WUFFS_BASE__PIXEL_FORMAT__BGR:
      case WUFFS_BASE__PIXEL_FORMAT__RGB:
        num_channels = 3;
        break;
      default:
        num_channels = 4;
        break;
    }

    // Configure the pixel buffer.
    uint64_t pixbuf_len = (uint64_t)w * (uint64_t)h * num_channels;
    if (pixbuf_len > SIZE_MAX) {
      status.repr = "#my_decode_png_wuffs: image is too large";
      goto fail;
    }
    pixbuf_ptr = (uint8_t*)malloc(pixbuf_len);
    if (!pixbuf_ptr) {
      status.repr = "#my_decode_png_wuffs: out of memory";
      goto fail;
    }

    wuffs_base__pixel_config pixcfg;
    wuffs_base__pixel_config__set(
        &pixcfg,
        (num_channels == 3) ? WUFFS_BASE__PIXEL_FORMAT__BGR
                            : WUFFS_BASE__PIXEL_FORMAT__BGRA_NONPREMUL,
        WUFFS_BASE__PIXEL_SUBSAMPLING__NONE, w, h);
    wuffs_base__pixel_buffer pixbuf;
    status = wuffs_base__pixel_buffer__set_from_slice(
        &pixbuf, &pixcfg, wuffs_base__make_slice_u8(pixbuf_ptr, pixbuf_len));
    if (status.repr) {
      goto fail;
    }

    // Configure the work buffer.
    uint64_t workbuf_len = wuffs_png__decoder__workbuf_len(&dec).max_incl;
    if (workbuf_len > SIZE_MAX) {
      status.repr = "#my_decode_png_wuffs: image is too large";
      goto fail;
    }
    workbuf_ptr = (uint8_t*)malloc(workbuf_len);
    if (!workbuf_ptr) {
      status.repr = "#my_decode_png_wuffs: out of memory";
      goto fail;
    }

    // Decode the image.
    status = wuffs_png__decoder__decode_frame(
        &dec, &pixbuf, &src, WUFFS_BASE__PIXEL_BLEND__SRC,
        wuffs_base__make_slice_u8(workbuf_ptr, workbuf_len), NULL);
    if (status.repr) {
      goto fail;
    }
    if (workbuf_ptr) {
      free(workbuf_ptr);
    }
    qoir_decode_result result = {0};
    result.owned_memory = pixbuf_ptr;
    result.dst_pixbuf.pixcfg.pixfmt = (num_channels == 3)
                                          ? QOIR_PIXEL_FORMAT__BGR
                                          : QOIR_PIXEL_FORMAT__BGRA_NONPREMUL;
    result.dst_pixbuf.pixcfg.width_in_pixels = w;
    result.dst_pixbuf.pixcfg.height_in_pixels = h;
    result.dst_pixbuf.data = pixbuf_ptr;
    result.dst_pixbuf.stride_in_bytes = w * num_channels;
    return result;
  } while (false);

fail:
  if (workbuf_ptr) {
    free(workbuf_ptr);
  }
  if (pixbuf_ptr) {
    free(pixbuf_ptr);
  }
  qoir_decode_result fail_result = {0};
  fail_result.status_message = status.repr;
  return fail_result;
}

qoir_encode_result           //
my_encode_png_wuffs(         //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf) {
  qoir_encode_result result = {0};
  result.status_message = error_not_implemented;
  return result;
}

#ifdef __cplusplus
}  // extern "C"
#endif
