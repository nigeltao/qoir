# QOIR File Format

A QOIR file encodes a BGRA image with 8 bits per channel.


## Overview

A QOIR file consists of an unpadded sequence of chunks:

- 1 QOIR chunk.
- 1 or more other (non-QOIR, non-QEND) chunks. These other chunks must contain
  exactly 1 QPIX chunk.
- 1 QEND chunk.

Each chunk has a 12 byte header and then a variable length payload. The header:

- 4 byte ChunkType. Examples include but are not limited to "CICP", "EXIF",
  "ICCP", "QEND", "QOIR", "QPIX" and "XMP ". By convention, these consist only
  of ASCII letters, numbers and underscores, and are right-padded with spaces.
- 8 byte PayloadLength. All QOIR integers are stored unsigned and little
  endian. For PayloadLength, values above `0x7FFF_FFFF_FFFF_FFFF` are invalid.


## QOIR Chunk

The minimum PayloadLength is 8. The PayloadLength also serves as a file format
version number, as future versions may use a larger value. The payload is:

- 3 byte width in pixels.
- ½ byte (low  4 bits) PixelFormat.
- ½ byte (high 4 bits) reserved.
- 3 byte height in pixels.
- ⅜ byte (low  3 bits) Lossiness.
- ⅝ byte (high 5 bits) reserved.
- The remainder is ignored, for forward compatibility.

The maximum (inclusive) image width and height is `0xFF_FFFF = 16777215`
pixels. A zero width (or zero height) QOIR image is valid, just as an empty
string is a valid string. Given a 0 width, a height of 0, 1 or 999 are all
valid ways of representing an empty image - one with no pixels.


### Pixel Format

Every QOIR file conceptually encodes 4 bytes per pixel, even if the image is
completely opaque. The first three are B (Blue), G (Green) and R (Red) values.
The last one is either an A (Alpha) value or X (ignored).

There are only 3 valid values of the QOIR chunk's payload's 4-bit PixelFormat
field, although future versions of the QOIR format may extend this.

- `0x01` means BGRX. This pixel format implies that the image is completely
  opaque, although the converse is not necessarily true: encoding a completely
  opaque image does not require using the BGRX pixel format.
- `0x02` means BGRA and that the color channels use nonpremultiplied alpha.
- `0x03` means BGRA and that the color channels use premultiplied alpha.

Decoding implementations can choose to decode in RGBX or RGBA order instead of
BGRX or BGRA. Similarly, for completely opaque images, they can choose to
decode to BGR or RGB without loss - 3 instead of 4 bytes per pixel.


### Lossiness

The QOIR chunk's payload's 3-bit lossiness value ranges from 0 to 7
(inclusive). Zero means that the image is lossless (for a source image that is
at most 8 bits per channel). A positive `lossiness` value means that only the
low `(8 - lossiness)` bits of the encoded channel's values are meaningful.

After processing the QPIX chunk (see below), each channel's value `v` is
replaced by `look_up_table[lossiness][v & mask]`, where the look-up tables are
listed in the appendix below and `(v & mask)` produces the low `(8 -
lossiness)` bits of `v`.

When decoding to a pixel format that differs from the QOIR image's native pixel
format discussed above, the look-up table transformation should occur prior to
any conversion between nonpremultiplied and premultiplied alpha.


## QEND Chunk

The PayloadLength must be 0.


## QPIX Chunk

The minimum PayloadLength is 0. The payload contains an unpadded sequence of
encoded tiles. Each tile can be decoded independently, measures 64 × 64 pixels
(although the right and bottom tiles might be smaller) and are presented in the
natural order (the same as pixels: left-to-right and top-to-bottom). No tiles
are empty: the minimum tile width and tile height is 1 pixel, not 0. A QOIR
image whose overall width or height is 0 pixels simply has 0 tiles.

A tile's encoding consists of a 4 byte prefix:

- 3 byte EncodedTileLength. Values above `0x4000 = 16384` are invalid unless
  the high bit of the EncodedTileFormat is set. None of the currently supported
  EncodedTileFormat values have that high bit set, but future versions might
  use this.
- 1 byte EncodedTileFormat

After the prefix are EncodedTileLength bytes whose interpretation depends on
the EncodedTileFormat:

- 0x00 "Literals Tile Format" means that the encoded tile bytes are literally
  BGRX / BGRA values (with no compression). There are 4 bytes per pixel, even
  for the BGRX pixel format.
- 0x01 "Ops Tile Format" means that the encoded tile bytes are virtual machine
  ops (see below).
- 0x02 "LZ4-Literals Tile Format" means that the encoded tile bytes are LZ4
  compressed. The decompressed bytes are like the "Literals Tile Format".
- 0x03 "LZ4-Ops Tile Format" means that the encoded tile bytes are LZ4
  compressed. The decompressed bytes are like the "Ops Tile Format".
- Other values are valid (for forward compatibility) but decoders should reject
  them as unsupported.

LZ4 specifically means [LZ4 block
compression](https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md). When
used in QOIR encoded tiles, a decompressed size above `0x1_0000 = 65536` is
invalid.

Regardless of the EncodedTileFormat, decoding a tile must consume exactly
EncodedTileLength bytes and produce exactly `(tile_width × tile_height)` pixels
of data. Pixels within a tile are, once again, presented in the natural order
and are packed tight, with no slack between rows, even when `(tile_width <
64)`.


### Ops Tile Format

In this case, the tile's pixels are reconstructed by executing a sequence of
virtual machine operations (also called ops). Each op occupies one or more
bytes and produces one or more pixels.

Virtual machine state consists of:

- A `color_cache` 64-element array of 4 byte tuples (BGRX or BGRA values).
  These are initialized at the start of every tile to opaque black: `{0x00,
  0x00, 0x00, 0xFF}`.
- A `next_color_index` integer, ranging from 0 to 63 inclusive, initialized to
  zero.

Some ops refer to "the previous pixel": the 4-tuple produced by the most
recently executed op. For the first op, the previous pixel is opaque black.

Every op produces pixels and therefore sets "the previous pixel". Every delta
op (i.e. every op other than `QOIR_OP_INDEX`, `QOIR_OP_RUNS` and
`QOIR_OP_RUNL`; see below) also updates the `color_cache`: setting the
`next_color_index`'th element to the produced pixel's BGRX / BGRA value and
then incrementing `next_color_index` by 1, modulo 64.

There are three broad categories of ops:

- Index ops produce a pixel from the `color_cache`.
- Delta ops produce a pixel that is similar to the previous pixel, adjusting
  its BGRX / BGRA values by some amount (modulo 256).
- Run ops produce the previous pixel's BGRX / BGRA values N+1 times, for some
  value of N. To be clear, that's "the same 4-tuple, N+1 times", not "the N+1
  previous 4-tuples".


### Opcodes

The low 2 to 8 bits of the op's first byte determine the opcode and therefore
(1) how many bytes the op occupies and (2) how to interpret the remaining op's
bits. When those bits cross byte boundaries, fields are reconstructed in
little-endian order.

    i=index, b=blue, g=green, r=red, a=alpha, c=b-g, s=r-g, n=run_length
    QOIR_OP_INDEX  iiiiii00
    QOIR_OP_BGR2   rrggbb01
    QOIR_OP_LUMA   gggggg10 sssscccc
    QOIR_OP_BGR7   bbbbb011 ggggggbb rrrrrrrg
    QOIR_OP_RUNS   nnnnn111
    QOIR_OP_RUNL   11010111 nnnnnnnn
    QOIR_OP_BGRA2  11011111 aarrggbb
    QOIR_OP_BGRA4  11100111 ggggbbbb aaaarrrr
    QOIR_OP_BGRA8  11101111 bbbbbbbb gggggggg rrrrrrrr aaaaaaaa
    QOIR_OP_BGR8   11110111 bbbbbbbb gggggggg rrrrrrrr
    QOIR_OP_A8     11111111 aaaaaaaa

A `QOIR_OP_INDEX` op produces one pixel: the `iiiiii`th element of the
`color_cache`.

A `QOIR_OP_BGR2` op produces one pixel: the previous pixel's BGRX / BGRA value
adjusted by `(bb - 2)`, `(gg - 2)` and `(rr - 2)`. The X / A channel is
unchanged.

A `QOIR_OP_LUMA` op produces one pixel: the previous pixel's BGRX / BGRA value
adjusted by `(cccc - 8 + gggggg - 32)`, `(gggggg - 32)` and `(ssss - 8 +
gggggg - 32)`. The X / A channel is unchanged.

A `QOIR_OP_BGR7` op produces one pixel: the previous pixel's BGRX / BGRA value
adjusted by `(bbbbbbb - 64)`, `(ggggggg - 64)` and `(rrrrrrr - 64)`. The X / A
channel is unchanged.

The `QOIR_OP_RUNS` and `QOIR_OP_RUNL` ops produce the previous pixel `(nnnnn +
1)` or `(nnnnnnnn + 1)` times. For a short run, `nnnnn` ranges from 0 to 25
inclusive. For a long run, `nnnnnnnn` ranges from 0 to 255 inclusive.

A `QOIR_OP_BGRA2` op produces one pixel: the previous pixel's BGRX / BGRA value
adjusted by `(bb - 2)`, `(gg - 2)`, `(rr - 2)` and `(aa - 2)`.

A `QOIR_OP_BGRA4` op produces one pixel: the previous pixel's BGRX / BGRA value
adjusted by `(bbbb - 8)`, `(gggg - 8)`, `(rrrr - 8)` and `(aaaa - 8)`.

A `QOIR_OP_BGRA8` op produces one pixel: the previous pixel's BGRX / BGRA value
adjusted by `bbbbbbbb`, `gggggggg`, `rrrrrrrr` and `aaaaaaaa`.

A `QOIR_OP_BGR8` op produces one pixel: the previous pixel's BGRX / BGRA value
adjusted by `bbbbbbbb`, `gggggggg` and `rrrrrrrr`. The X / A channel is
unchanged.

A `QOIR_OP_A8` op produces one pixel: the previous pixel's BGRX / BGRA value
adjusted by `aaaaaaaa`. The B, G and R channels are unchanged.

There is some redundancy here. There are multiple ways to encode "produce a
pixel that's the same as the previous pixel". Similarly, there's multiple ways
to encode "a run of 5 identical pixels".


## Ancillary Chunks

The "QOIR", "QPIX" or "QEND" ChunkTypes (and their corresponding chunks) are
called critical. All other ChunkTypes and chunks are called ancillary and
decoders are free to ignore them. This document defines 4 ancillary ChunkTypes.

- A "CICP" or "ICCP" chunk's payload should be interpreted the same way as a
  PNG [cICP or iCCP](https://w3c.github.io/PNG-spec/#11addnlcolinfo) color
  space chunk. If both chunks are present, the "CICP" one takes precedence (and
  should occur before the "ICCP" chunk). If either or both chunks are present,
  they should all occur before the "QPIX" chunk. If neither chunk is present,
  the image is implicitly in the sRGB color space.
- An "EXIF" or "XMP " chunk's payload should be interpreted the same way as a
  WebP [Exif or
  XMP](https://developers.google.com/speed/webp/docs/riff_container#metadata)
  metadata chunk. If either or both chunks are present, they should all occur
  after the "QPIX" chunk.

Decoders may support all, none or any combination of these. For example, a
decoder may support "CICP, "ICCP" and "XMP " but not "EXIF".


### Unique Chunks

Chunks whose ChunkType starts with an upper-case ASCII letter are called
unique. Non-unique chunks are called repeatable. It is invalid for a QOIR file
to contain two or more unique chunks with the same ChunkType. Decoders are not
required to enforce the "ABCD" chunks' uniqueness (for whatever value of
"ABCD") if they ignore "ABCD" chunks.

This document only defines unique chunks: "CICP", "QEND", etc. Future
extensions to the format, official or not, may define repeatable chunks.


## Example QOIR File

Here's a simple QOIR image 3 pixels wide and 2 pixels high. It is a crude
approximation to the French flag, being three columns: blue, white and red.

    $ hd crude-flag.qoir
    00000000  51 4f 49 52 08 00 00 00  00 00 00 00 03 00 00 01  |QOIR............|
    00000010  02 00 00 00 51 50 49 58  0a 00 00 00 00 00 00 00  |....QPIX........|
    00000020  06 00 00 01 a5 59 bd 00  04 08 51 45 4e 44 00 00  |.....Y....QEND..|
    00000030  00 00 00 00 00 00                                 |......|

This breaks down as:

- A 20 byte QOIR chunk: 4 byte ChunkType, 8 byte PayloadLength (a little-endian
  uint64 whose value is 8). The payload states an image width of 3 pixels, a
  PixelFormat of BGRX, an image height of 2 pixels and 0 Lossiness.
- A 22 byte QPIX chunk: 4 byte ChunkType, 8 byte PayloadLength (a little-endian
  uint64 whose value is 10). Since the image dimensions are 3 × 2, there's only
  one tile in the payload. That tile's 3 byte EncodedTileLength has value 6 and
  its 1 byte EncodedTileFormat (0x01) means "Ops Tile Format".
- A 12 byte QEND chunk.

The 6 ops bytes:

    Initialize "the previous pixel" to opaque black................ {0x00, 0x00, 0x00, 0xFF}
    0xA5 = 0b1010_0101 = QOIR_OP_BGR2(ΔB=-1, ΔG=+0, ΔR=+0) produces {0xFF, 0x00, 0x00, 0xFF} and sets color_cache[0]
    0x59 = 0b0101_1001 = QOIR_OP_BGR2(ΔB=+0, ΔG=-1, ΔR=-1) produces {0xFF, 0xFF, 0xFF, 0xFF} and sets color_cache[1]
    0xBD = 0b1011_1101 = QOIR_OP_BGR2(ΔB=+1, ΔG=+1, ΔR=+0) produces {0x00, 0x00, 0xFF, 0xFF} and sets color_cache[2]
    0x00 = 0b0000_0000 = QOIR_OP_INDEX(0)                  produces {0xFF, 0x00, 0x00, 0xFF}
    0x04 = 0b0000_0100 = QOIR_OP_INDEX(1)                  produces {0xFF, 0xFF, 0xFF, 0xFF}
    0x08 = 0b0000_1000 = QOIR_OP_INDEX(2)                  produces {0x00, 0x00, 0xFF, 0xFF}

In this example, each pixel was encoded with a one byte op (and LZ4 compression
was not used) but recall that, in general:

- Ops can consume more than one byte.
- Ops can produce more than one pixel.

There are more example QOIR files (and their equivalent PNG forms) in the
`test/data` directory of this repository.


## Appendix - Lossiness Look-up Tables

See the "Lossiness" section above.

Given a `lossiness` value and `v`, a pre-transformed value, the
post-transformed value equals `(v << lossiness)` in the high `(8 - lossiness)`
bits and shifted repetitions of `v`'s bit pattern in the low `lossiness` bits.

For example, if `lossiness = 3` and `v = 0x14 = 0b1_0100` then the
post-transformed value is `0b1010_0101 = 0xA5`.

This transformation recovers the full `0x00 ..= 0xFF` range. The first and last
elements of each table is `0x00` and `0xFF`.


### Lossiness = 1

128 element look-up table.

    0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,0x12,0x14,0x16,0x18,0x1A,0x1C,0x1E,
    0x20,0x22,0x24,0x26,0x28,0x2A,0x2C,0x2E,0x30,0x32,0x34,0x36,0x38,0x3A,0x3C,0x3E,
    0x40,0x42,0x44,0x46,0x48,0x4A,0x4C,0x4E,0x50,0x52,0x54,0x56,0x58,0x5A,0x5C,0x5E,
    0x60,0x62,0x64,0x66,0x68,0x6A,0x6C,0x6E,0x70,0x72,0x74,0x76,0x78,0x7A,0x7C,0x7E,
    0x81,0x83,0x85,0x87,0x89,0x8B,0x8D,0x8F,0x91,0x93,0x95,0x97,0x99,0x9B,0x9D,0x9F,
    0xA1,0xA3,0xA5,0xA7,0xA9,0xAB,0xAD,0xAF,0xB1,0xB3,0xB5,0xB7,0xB9,0xBB,0xBD,0xBF,
    0xC1,0xC3,0xC5,0xC7,0xC9,0xCB,0xCD,0xCF,0xD1,0xD3,0xD5,0xD7,0xD9,0xDB,0xDD,0xDF,
    0xE1,0xE3,0xE5,0xE7,0xE9,0xEB,0xED,0xEF,0xF1,0xF3,0xF5,0xF7,0xF9,0xFB,0xFD,0xFF,


### Lossiness = 2

64 element look-up table.

    0x00,0x04,0x08,0x0C,0x10,0x14,0x18,0x1C,0x20,0x24,0x28,0x2C,0x30,0x34,0x38,0x3C,
    0x41,0x45,0x49,0x4D,0x51,0x55,0x59,0x5D,0x61,0x65,0x69,0x6D,0x71,0x75,0x79,0x7D,
    0x82,0x86,0x8A,0x8E,0x92,0x96,0x9A,0x9E,0xA2,0xA6,0xAA,0xAE,0xB2,0xB6,0xBA,0xBE,
    0xC3,0xC7,0xCB,0xCF,0xD3,0xD7,0xDB,0xDF,0xE3,0xE7,0xEB,0xEF,0xF3,0xF7,0xFB,0xFF,


### Lossiness = 3

32 element look-up table.

    0x00,0x08,0x10,0x18,0x21,0x29,0x31,0x39,0x42,0x4A,0x52,0x5A,0x63,0x6B,0x73,0x7B,
    0x84,0x8C,0x94,0x9C,0xA5,0xAD,0xB5,0xBD,0xC6,0xCE,0xD6,0xDE,0xE7,0xEF,0xF7,0xFF,


### Lossiness = 4

16 element look-up table.

    0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,


### Lossiness = 5

8 element look-up table.

    0x00,0x24,0x49,0x6D,0x92,0xB6,0xDB,0xFF,


### Lossiness = 6

4 element look-up table.

    0x00,0x55,0xAA,0xFF,


### Lossiness = 7

2 element look-up table.

    0x00,0xFF,
