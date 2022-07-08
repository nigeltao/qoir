# QOIR File Format

QOIR is an unpadded sequence of chunks:

- 1 QOIR chunk.
- 1 or more other (non-QOIR, non-QEND) chunks. These other chunks must contain
  exactly 1 QPIX chunk.
- 1 QEND chunk.

Each chunk has a 12 byte header and a variable length payload. The header:

- 4 byte ChunkType (e.g. "EXIF", "ICCP", "QEND", "QOIR", "QPIX", "XMP ").
- 8 byte PayloadLength. All QOIR integers are stored unsigned and little
  endian. For PayloadLength, values above `0x7FFF_FFFF_FFFF_FFFF` are invalid.


## QOIR Chunk

The minimum PayloadLength is 8. The PayloadLength also serves as a file format
version number, as future versions may use a larger value. The payload is:

- 3 byte width in pixels.
- ½ byte pixel format (as per the `qoir_pixel_format` C type).
- ½ byte reserved.
- 3 byte height in pixels.
- 1 byte reserved.
- The remainder is ignored, for forward compatibility.


## QEND Chunk

The PayloadLength must be 0.


## QPIX chunk.

The minimum PayloadLength is 0. The payload contains an unpadded sequence of
encoded tiles. Each tile can be decoded independently, measures 128 x 128
pixels (although the right and bottom tiles might be smaller) and are presented
in the natural order (the same as pixels: left-to-right and top-to-bottom).

A tile's encoding consists of a 4 byte prefix:

- 3 byte EncodedTileLength. Values above 0x10000 = 65536 are invalid unless the
  high bit of the EncodedTileFormat is set. None of the currently supported
  EncodedTileFormat values have that high bit set, but future versions might
  use this.
- 1 byte EncodedTileFormat

After the prefix are EncodedTileLength bytes whose interpretation depends on
the EncodedTileFormat:

- 0x00 "Literals tile format" means that the encoded tile bytes are literally
  RGBA values (with no compression).
- 0x01 "Opcodes tile format" means that the encoded tile bytes are pixel
  opcodes (see below).
- 0x02 "LZ4-Literals tile format" means that the encoded tile bytes are LZ4
  compressed. The decompressed bytes are like the "Literals tile format".
- 0x03 "LZ4-Opcodes tile format" means that the encoded tile bytes are LZ4
  compressed. The decompressed bytes are like the "Opcodes tile format".
- Other values are valid (for forward compatibility) but decoders should reject
  them as unsupported.

LZ4 specifically means [LZ4 block
compression](https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md). When
used in QOIR encoded tiles, a decompressed size above 65536 is invalid.


## Pixel Opcodes

TODO: add more details.
