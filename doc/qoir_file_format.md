# QOIR File Format

QOIR is a sequence of chunks:

- 1 QOIR chunk.
- 1 or more other (non-QOIR, non-QEND) chunks. These other chunks must contain
  exactly 1 QPIX chunk.
- 1 QEND chunk.

Each chunk has a 12 byte header and a variable length payload. There is no
padding between chunks. The header:

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

The minimum PayloadLength is 0. The payload contains pixel opcodes (similar to
QOI opcodes). This is the 'meat' of the format.

TODO: add more details.
