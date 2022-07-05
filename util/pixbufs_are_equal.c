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

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static bool  //
pixbufs_are_equal(const qoir_pixel_buffer* pb0, const qoir_pixel_buffer* pb1) {
  if ((pb0 == NULL) || (pb1 == NULL)) {
    return pb0 == pb1;
  } else if ((pb0->pixcfg.pixfmt != pb1->pixcfg.pixfmt) ||
             (pb0->pixcfg.width_in_pixels != pb1->pixcfg.width_in_pixels) ||
             (pb0->pixcfg.height_in_pixels != pb1->pixcfg.height_in_pixels)) {
    return false;
  }
  uint64_t width_in_bytes =
      ((uint64_t)(pb0->pixcfg.width_in_pixels)) *
      ((uint64_t)(qoir_pixel_format__bytes_per_pixel(pb0->pixcfg.pixfmt)));
  uint8_t* ptr0 = pb0->data;
  uint8_t* ptr1 = pb1->data;
  for (uint32_t n = pb0->pixcfg.height_in_pixels; n > 0; n--) {
    if (memcmp(ptr0, ptr1, width_in_bytes)) {
      return false;
    }
    ptr0 += pb0->stride_in_bytes;
    ptr1 += pb1->stride_in_bytes;
  }
  return true;
}
