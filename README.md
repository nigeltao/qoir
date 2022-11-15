# QOIR: a Simple, Lossless Image File Format

QOIR (pronounced like "choir") is simple, lossless image file format that is
very fast to encode and decode while achieving compression ratios roughly
comparable to PNG.

It was inspired by the [QOI image file format](https://qoiformat.org/),
building on it in a number of ways:

- It uses a RIFF-like file structure, allowing future extensions.
- That structure means that QOIR files can include metadata like color profiles
  (QOI is limited to two choices, "linear" or "sRGB") and EXIF information.
- It integrates LZ4 compression to [produce smaller
  files](https://github.com/nigeltao/qoir/commit/9be1f2fdcfac3289804b80d4b0673dde499a9c1c).
- It can represent premultiplied alpha (as well as PNG's non-premultiplied
  alpha), which can avoid a post-processing step if your game engine or GUI
  toolkit works with [premultiplied
  alpha](https://iquilezles.org/articles/premultipliedalpha/).
- It has an optional lossy mode.
- It partitions the image into independent tiles, allowing for multi-threaded
  codec implementations.
- As an implementation concern (not a file format concern), it can decode into
  a pre-existing pixel buffer (in a variety of pixel formats), not just always
  returning a newly allocated pixel buffer. This also supports multi-threaded
  decoding, where the entire pixel buffer is allocated first and N separate
  threads then decode into their own separate portion of that pixel buffer.


## Building

[`src/qoir.h`](src/qoir.h) is a single file C library, so there's no separate
configure or build steps. Just `#define QOIR_IMPLEMENTATION` before you
`#include` it.


## Benchmarks

The numbers below are the top-level summary of the full benchmarks, normalized
so that QOIR is 1.000 (the [full benchmarks](doc/full_benchmarks.txt) page has
raw, non-normalized numbers and links to the benchmark suite of images). For
example, **comparing PNG/libpng with QOIR, libpng compresses a little smaller
(0.960 versus 1.000) but QOIR encodes 30x faster (0.033 versus 1.000) and
decodes 4.9x faster (0.203 versus 1.000)**.

For example, PNG/fpng encodes faster (1.138x) than QOIR but produces larger
(1.234x) files and decodes slower (0.536x) than QOIR.

For example, JPEG-XL Lossless at its default encoder options produces smaller
files (0.613x) but encodes slower (0.003x) and decodes slower (0.017x) than
QOIR. Inverting those last two numbers give QOIR encoding 296x faster and
decoding 60x faster than JPEG-XL Lossless (using its "effort = 7" option).

The conclusion isn't that QOIR is always better or worse than any other format.
It's all trade-offs. However, **QOIR has the fastest decode speed listed** and
achieves reasonable (roughly comparable to PNG) compression ratios. JXL and
WebP, lossless or lossy, have better compression ratios but also slower encode
and decode speeds.

Even though PNG/stb and QOI are worse than QOIR in all three columns
(compression ratio, encoding speed and decoding speed), they still have their
own advantages. PNG/stb, like any PNG implementation, has unrivalled
compatibility with other software (and the stb library is easy to integrate,
being a single file C library). QOI is the simplest (easiest to understand and
easiest to customize) format, weighing under 700 lines of C code (QOIR is
around 3000 lines of C code plus 4000 lines of data tables).

To repeat, it's all trade-offs.


### Lossless Benchmarks

```
Rel = Relative to QOIR_Lossless (which is normalized to 1.000)

RelCmpRatio  = Relative CompressedSize / DecompressedSize. Lower is better.
RelEncSpeed  = Relative Encode MegaPixels per second.     Higher is better.
RelDecSpeed  = Relative Decode MegaPixels per second.     Higher is better.

QOIR_Lossless    1.000 RelCmpRatio    1.000 RelEncSpeed    1.000 RelDecSpeed   (1)
JXL_Lossless/f   0.860 RelCmpRatio    0.630 RelEncSpeed    0.120 RelDecSpeed   (2)
JXL_Lossless/l3  0.725 RelCmpRatio    0.032 RelEncSpeed    0.022 RelDecSpeed
JXL_Lossless/l7  0.613 RelCmpRatio    0.003 RelEncSpeed    0.017 RelDecSpeed
PNG/fpng         1.234 RelCmpRatio    1.138 RelEncSpeed    0.536 RelDecSpeed   (1)
PNG/fpnge        1.108 RelCmpRatio    1.851 RelEncSpeed      n/a RelDecSpeed   (1)
PNG/libpng       0.960 RelCmpRatio    0.033 RelEncSpeed    0.203 RelDecSpeed
PNG/stb          1.354 RelCmpRatio    0.045 RelEncSpeed    0.186 RelDecSpeed   (1)
PNG/wuffs        0.946 RelCmpRatio      n/a RelEncSpeed    0.509 RelDecSpeed   (1), (3)
QOI              1.118 RelCmpRatio    0.870 RelEncSpeed    0.700 RelDecSpeed   (1)
WebP_Lossless    0.654 RelCmpRatio    0.015 RelEncSpeed    0.325 RelDecSpeed
```

(1) means that the codec implementation is available as a [single file C
library](https://github.com/nothings/stb/blob/master/docs/stb_howto.txt).

(2) means that the fjxl encoder is a single file C library but there is no fjxl
decoder. There's also the j40 single file JXL decoder, in a separate
repository, but I couldn't get it to work. Passing it something produced by the
cjxl reference encoder produced `Error: Decoding failed (rnge) during
j40_next_frame`.

(3) means that Wuffs' standard library has a PNG decoder but not a PNG encoder.
The "compression ratio" numbers simply take the benchmark suite PNG images "as
is" without re-encoding.


### Lossy Benchmarks

QOIR is first and foremost a lossless format (for 24-bit RGB or 32-bit RGBA
images) but it also has a trivial lossy mode (reducing each pixel from 8 to 6
bits per channel). Here are some comparisons to other lossy formats. Once
again, there are trade-offs.

```
QOIR_Lossy       0.641 RelCmpRatio    0.903 RelEncSpeed    0.731 RelDecSpeed   (1)
JXL_Lossy/l3     0.440 RelCmpRatio    0.051 RelEncSpeed    0.091 RelDecSpeed
JXL_Lossy/l7     0.305 RelCmpRatio    0.013 RelEncSpeed    0.070 RelDecSpeed
WebP_Lossy       0.084 RelCmpRatio    0.065 RelEncSpeed    0.453 RelDecSpeed
```

Lossy encoders (other than QOIR) use the respective libraries' default options.
Different size/speed/quality trade-offs may be achievable with other options.


### Multi-Threading

The benchmark numbers above are all single-threaded. Other codec
implementations can be sped up (in terms of wall clock time) by using multiple
threads. QOIR is no different: multi-threaded decoding can be [over 3x
faster](https://github.com/nigeltao/qoir/commit/913011d51da68c4f9c3e7c9d98aa4f9a04ac8d8e),
depending on your input image size.


### Other Libraries

These libraries are only used by the benchmark program. The QOIR codec
implementation has no dependencies (and brings its own LZ4 implementation).

JXL ([libjxl/libjxl](https://github.com/libjxl/libjxl.git)) is the official
JPEG-XL library. The /l suffix denotes the regular libjxl implementation and
the /f suffix denotes the `experimental/fast_lossless` encoder (also known as
fjxl) in that repository (but still using the regular libjxl decoder). The
final 3 or 7 denotes libjxl's "effort" encoding option, which defaults to 7.

PNG/fpng ([richgel999/fpng](https://github.com/richgel999/fpng.git)) is a fast
PNG encoder and decoder. The encoded output are valid PNG images but the fpng
decoder only accepts fpng-encoded PNG images. It is not a general PNG decoder.
It's also currently only SIMD-optimized for the x86 CPU family, not ARM.

PNG/fpnge ([veluca93/fpnge](https://github.com/veluca93/fpnge.git)) is a very
fast PNG encoder. The repository only contains an encoder, not a decoder. It's
also currently only SIMD-optimized for the x86 CPU family, not ARM.

PNG/libpng is the official libpng library as built on Debian Bullseye.

PNG/stb ([nothings/stb](https://github.com/nothings/stb.git)) is one of the
best known "single file C library" PNG implementations.

PNG/wuffs ([google/wuffs](https://github.com/google/wuffs.git)) is the PNG
decoder from the Wuffs repository. There is no encoder but the decoder is
discussed in ["The Fastest, Safest PNG Decoder in the
World"](https://nigeltao.github.io/blog/2021/fastest-safest-png-decoder.html).
While its reported decode speed here is not as fast as PNG/fpng, PNG/wuffs is a
general PNG decoder and isn't limited to only those PNG images produced by the
PNG/fpng encoder.

QOI ([phoboslab/qoi](https://github.com/phoboslab/qoi.git)) is a recent
(released in 2021), simple image file format that is remarkably competitive.

QOIR is this repository. Lossless is the default option. Lossy means using the
lossiness=2 encoding option, reducing each pixel from 8 to 6 bits per channel.

WebP is the official libwebp library as built on Debian Bullseye. The WebP file
format cannot represent an image dimension greater than 16383 pixels, such as
the 1313 x 20667 `screenshot_web/phoboslab.org.png` image from the benchmark
suite. For these large images, we use PNG/libpng instead.


### Excluded Libraries

AVIF wasn't measured. I gave up after hitting [Debian bug
976349](https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=976349) and the
combination of (1) lossless is the primary focus of QOIR but (2) the [avif
examples](https://github.com/AOMediaCodec/libavif/tree/b3e0f31/examples) or
avif.h file not obviously showing how to encode *losslessly* from the library
(not the command line tools). Maybe I'll try again later.

JPEG wasn't measured. Around 45% of the [benchmark suite
images](https://qoiformat.org/benchmark/) have an alpha channel, which JPEG
cannot represent.

PNG/libspng and PNG/lodepng weren't measured. They are presumably roughly
comparable, [within a factor of
2](https://nigeltao.github.io/blog/2021/fastest-safest-png-decoder.html#appendix-benchmark-numbers),
to PNG/libpng and PNG/stb.


## LZ4

The LZ4 block compression implementation in this repository is available as a
stand-alone [Single File LZ4 C Library](https://github.com/nigeltao/sflz4).


## License

Apache 2. See the [LICENSE](LICENSE) file for details.


---

Updated on November 2022.
