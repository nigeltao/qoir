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

#ifndef INCLUDE_GUARD_ADAPTER_ALL_ADAPTERS
#define INCLUDE_GUARD_ADAPTER_ALL_ADAPTERS

#ifdef __cplusplus
extern "C" {
#endif

// ----

qoir_decode_result           //
my_decode_jxl_lib(           //
    const uint8_t* src_ptr,  //
    const size_t src_len);

qoir_decode_result           //
my_decode_lz4png(            //
    const uint8_t* src_ptr,  //
    const size_t src_len);

qoir_decode_result           //
my_decode_png_fpng(          //
    const uint8_t* src_ptr,  //
    const size_t src_len);

qoir_decode_result           //
my_decode_png_fpnge(         //
    const uint8_t* src_ptr,  //
    const size_t src_len);

qoir_decode_result           //
my_decode_png_libpng(        //
    const uint8_t* src_ptr,  //
    const size_t src_len);

qoir_decode_result           //
my_decode_png_stb(           //
    const uint8_t* src_ptr,  //
    const size_t src_len);

qoir_decode_result           //
my_decode_png_wuffs(         //
    const uint8_t* src_ptr,  //
    const size_t src_len);

qoir_decode_result           //
my_decode_qoi(               //
    const uint8_t* src_ptr,  //
    const size_t src_len);

qoir_decode_result           //
my_decode_webp(              //
    const uint8_t* src_ptr,  //
    const size_t src_len);

qoir_decode_result           //
my_decode_zpng(              //
    const uint8_t* src_ptr,  //
    const size_t src_len);

// ----

qoir_encode_result           //
my_encode_jxl_lossless_fst(  //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result            //
my_encode_jxl_lossless_lib3(  //
    const uint8_t* png_ptr,   //
    const size_t png_len,     //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result            //
my_encode_jxl_lossless_lib7(  //
    const uint8_t* png_ptr,   //
    const size_t png_len,     //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_jxl_lossy_lib3(    //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_lz4png_lossless(   //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_lz4png_lossy(      //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result                   //
my_encode_lz4png_nofilter_lossless(  //
    const uint8_t* png_ptr,          //
    const size_t png_len,            //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_jxl_lossy_lib7(    //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_png_fpng(          //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_png_fpnge(         //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_png_libpng(        //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_png_stb(           //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_png_wuffs(         //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_qoi(               //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_webp_lossless(     //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_webp_lossy(        //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_zpng_lossless(     //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result           //
my_encode_zpng_lossy(        //
    const uint8_t* png_ptr,  //
    const size_t png_len,    //
    qoir_pixel_buffer* src_pixbuf);

qoir_encode_result                 //
my_encode_zpng_nofilter_lossless(  //
    const uint8_t* png_ptr,        //
    const size_t png_len,          //
    qoir_pixel_buffer* src_pixbuf);

// ----

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // INCLUDE_GUARD_ETC
