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

// Package gammaawaredither dithers images (quantizing the colors while
// avoiding large sections of flat color) in a gamma-correction aware manner so
// that the input and output images have the same perceived brightness.
package gammaawaredither

import (
	"errors"
	"image"
	"image/color"
	"math"
)

// These errors can be returned by Dither.
var (
	ErrNilSrcImage         = errors.New("gammaawaredither: nil src image")
	ErrUnsupportedSrcImage = errors.New("gammaawaredither: unsupported src image")
)

// DitherOptions holds the optional arguments to the Dither function.
type DitherOptions struct {
	// Gamma is the gamma correction value for the source image. The default
	// (zero) value is equivalent to 2.2. Use 1.0 for no gamma correction.
	Gamma float64
}

func (o *DitherOptions) gamma() float64 {
	if (o != nil) && (o.Gamma > 0) {
		return o.Gamma
	}
	return 2.2
}

// Dither returns a dithered version of the source image. lossiness ranges from
// 0 (lossless for 8-bit color source images, lossy for higher bit depths) to 7
// (very lossy, unconditionally), inclusive. opts may be nil, equivalent to a
// pointer to a zero-valued DitherOptions struct.
func Dither(src image.Image, lossiness int, opts *DitherOptions) (image.Image, error) {
	if src == nil {
		return nil, ErrNilSrcImage
	}
	src64, ok := src.(image.RGBA64Image)
	if !ok {
		return nil, ErrUnsupportedSrcImage
	}
	if lossiness < 0 {
		lossiness = 0
	} else if lossiness > 7 {
		lossiness = 7
	}
	lossiness32 := uint32(lossiness)
	gamma := opts.gamma()

	bounds := src64.Bounds()
	dst := image.NewRGBA(bounds)
	for y := bounds.Min.Y; y < bounds.Max.Y; y++ {
		for x := bounds.Min.X; x < bounds.Max.X; x++ {
			pixel := src64.RGBA64At(x, y)
			noise := noiseTable[y&15][x&15]
			dst.SetRGBA(x, y, color.RGBA{
				// The ">>8" converts from 16-bit to 8-bit color.
				R: dither(lossiness32, uint8(pixel.R>>8), noise, gamma),
				G: dither(lossiness32, uint8(pixel.G>>8), noise, gamma),
				B: dither(lossiness32, uint8(pixel.B>>8), noise, gamma),
				A: dither(lossiness32, uint8(pixel.A>>8), noise, gamma),
			})
		}
	}
	return dst, nil
}

func dither(lossiness uint32, pixel uint8, noise float64, gamma float64) uint8 {
	// The maximum pixel value remains unchanged after dithering.
	if pixel >= 0xFF {
		return 0xFF
	}

	// Find the lower (inclusive) and upper (exclusive) quantized values that
	// bracket the pixel value.
	lowShifted := uint8(0)
	if shifted := pixel >> lossiness; shifted > 0 {
		lowShifted = shifted - 1
	}
	unshift := &unshiftTables[lossiness]
	lower := unshift[lowShifted+0]
	upper := unshift[lowShifted+1]
	if pixel >= upper {
		lower = unshift[lowShifted+1]
		upper = unshift[lowShifted+2]
	}

	// Compare the pixel's position in that bracket to the noise.
	p := math.Pow(float64(pixel)/255.0, gamma)
	l := math.Pow(float64(lower)/255.0, gamma)
	u := math.Pow(float64(upper)/255.0, gamma)
	if (p - l) > (noise * (u - l)) {
		return uint8(upper)
	}
	return uint8(lower)
}

// noiseTable is a 16x16 blue noise texture, based on "LDR_LLL1_42.png" from
// the zip file linked to from http://momentsingraphics.de/BlueNoise.html and
// whose LICENSE.txt says "To the extent possible under law, Christoph Peters
// has waived all copyright and related or neighboring rights".
var noiseTable = [16][16]float64{
	{0xAE / 256.0, 0xE7 / 256.0, 0x89 / 256.0, 0xF2 / 256.0, 0x26 / 256.0, 0x1A / 256.0, 0x8C / 256.0, 0xF7 / 256.0,
		0xC1 / 256.0, 0x69 / 256.0, 0xA4 / 256.0, 0x30 / 256.0, 0x91 / 256.0, 0xCC / 256.0, 0x64 / 256.0, 0x52 / 256.0},
	{0x71 / 256.0, 0x1C / 256.0, 0x5E / 256.0, 0x7B / 256.0, 0xA9 / 256.0, 0xD5 / 256.0, 0x62 / 256.0, 0x08 / 256.0,
		0xCE / 256.0, 0x49 / 256.0, 0x0E / 256.0, 0x55 / 256.0, 0x7A / 256.0, 0x18 / 256.0, 0xA6 / 256.0, 0x05 / 256.0},
	{0xD8 / 256.0, 0xBF / 256.0, 0x99 / 256.0, 0x01 / 256.0, 0x56 / 256.0, 0xE4 / 256.0, 0x72 / 256.0, 0xB2 / 256.0,
		0x36 / 256.0, 0x97 / 256.0, 0xD9 / 256.0, 0xED / 256.0, 0xBC / 256.0, 0x25 / 256.0, 0xF8 / 256.0, 0x96 / 256.0},
	{0x2B / 256.0, 0x3B / 256.0, 0xFE / 256.0, 0xB6 / 256.0, 0x41 / 256.0, 0x2F / 256.0, 0x9F / 256.0, 0xE9 / 256.0,
		0x1D / 256.0, 0x85 / 256.0, 0xAC / 256.0, 0x6D / 256.0, 0x3E / 256.0, 0xD1 / 256.0, 0x47 / 256.0, 0x80 / 256.0},
	{0x66 / 256.0, 0x4F / 256.0, 0x13 / 256.0, 0xD2 / 256.0, 0x83 / 256.0, 0xC2 / 256.0, 0x0F / 256.0, 0x51 / 256.0,
		0xFB / 256.0, 0x60 / 256.0, 0x2A / 256.0, 0x00 / 256.0, 0x9D / 256.0, 0x5B / 256.0, 0xB5 / 256.0, 0xE2 / 256.0},
	{0xC8 / 256.0, 0xAA / 256.0, 0x90 / 256.0, 0xEB / 256.0, 0x6B / 256.0, 0x23 / 256.0, 0x94 / 256.0, 0x79 / 256.0,
		0x40 / 256.0, 0xCA / 256.0, 0xDE / 256.0, 0x76 / 256.0, 0xF5 / 256.0, 0x16 / 256.0, 0x8B / 256.0, 0x0B / 256.0},
	{0xF3 / 256.0, 0x20 / 256.0, 0x77 / 256.0, 0x33 / 256.0, 0x5C / 256.0, 0xCB / 256.0, 0xE1 / 256.0, 0xA8 / 256.0,
		0xBB / 256.0, 0x14 / 256.0, 0x8E / 256.0, 0xB0 / 256.0, 0x4D / 256.0, 0xC3 / 256.0, 0x34 / 256.0, 0x70 / 256.0},
	{0x58 / 256.0, 0x45 / 256.0, 0xDB / 256.0, 0xA5 / 256.0, 0x0D / 256.0, 0x4A / 256.0, 0xF4 / 256.0, 0x04 / 256.0,
		0x68 / 256.0, 0x57 / 256.0, 0x31 / 256.0, 0xE5 / 256.0, 0x1E / 256.0, 0x82 / 256.0, 0xE8 / 256.0, 0xA2 / 256.0},
	{0x95 / 256.0, 0x03 / 256.0, 0xBD / 256.0, 0xFA / 256.0, 0x8A / 256.0, 0xB3 / 256.0, 0x28 / 256.0, 0x3A / 256.0,
		0x86 / 256.0, 0xA1 / 256.0, 0xD3 / 256.0, 0x43 / 256.0, 0x9A / 256.0, 0x63 / 256.0, 0xCD / 256.0, 0x27 / 256.0},
	{0xD7 / 256.0, 0x65 / 256.0, 0x81 / 256.0, 0x1B / 256.0, 0x54 / 256.0, 0x6E / 256.0, 0x98 / 256.0, 0xDA / 256.0,
		0xC5 / 256.0, 0xEE / 256.0, 0x0C / 256.0, 0x6F / 256.0, 0xFF / 256.0, 0x06 / 256.0, 0x3D / 256.0, 0xB9 / 256.0},
	{0xF1 / 256.0, 0xAD / 256.0, 0x2E / 256.0, 0x3F / 256.0, 0xCF / 256.0, 0xEC / 256.0, 0x7C / 256.0, 0x19 / 256.0,
		0x5F / 256.0, 0x22 / 256.0, 0x7F / 256.0, 0xAF / 256.0, 0xBE / 256.0, 0x53 / 256.0, 0x7D / 256.0, 0x17 / 256.0},
	{0x48 / 256.0, 0x73 / 256.0, 0xE6 / 256.0, 0xC4 / 256.0, 0xA0 / 256.0, 0x07 / 256.0, 0x46 / 256.0, 0xB7 / 256.0,
		0xF9 / 256.0, 0x4E / 256.0, 0x37 / 256.0, 0x92 / 256.0, 0x2C / 256.0, 0xDC / 256.0, 0xA7 / 256.0, 0x8D / 256.0},
	{0x5A / 256.0, 0x09 / 256.0, 0x93 / 256.0, 0x24 / 256.0, 0x61 / 256.0, 0xD6 / 256.0, 0x32 / 256.0, 0x8F / 256.0,
		0x6C / 256.0, 0xA3 / 256.0, 0xD0 / 256.0, 0xEA / 256.0, 0x15 / 256.0, 0x67 / 256.0, 0xF6 / 256.0, 0x35 / 256.0},
	{0xD4 / 256.0, 0xB4 / 256.0, 0xFC / 256.0, 0x50 / 256.0, 0x84 / 256.0, 0xAB / 256.0, 0x11 / 256.0, 0xC6 / 256.0,
		0xE3 / 256.0, 0x02 / 256.0, 0x42 / 256.0, 0x75 / 256.0, 0xC7 / 256.0, 0x9E / 256.0, 0x21 / 256.0, 0xC0 / 256.0},
	{0x10 / 256.0, 0x9B / 256.0, 0x6A / 256.0, 0x12 / 256.0, 0xDF / 256.0, 0x74 / 256.0, 0xF0 / 256.0, 0x59 / 256.0,
		0x29 / 256.0, 0xB1 / 256.0, 0x88 / 256.0, 0x5D / 256.0, 0x0A / 256.0, 0x4C / 256.0, 0x87 / 256.0, 0x78 / 256.0},
	{0x2D / 256.0, 0x44 / 256.0, 0xC9 / 256.0, 0x38 / 256.0, 0xBA / 256.0, 0x4B / 256.0, 0x9C / 256.0, 0x3C / 256.0,
		0x7E / 256.0, 0x1F / 256.0, 0xE0 / 256.0, 0xFD / 256.0, 0xB8 / 256.0, 0x39 / 256.0, 0xEF / 256.0, 0xDD / 256.0},
}

// unshift[n][x] expands the low (8 - n) bits of x to range from 0x00 to 0xFF
// inclusive, linearly.
var unshiftTables = [8][256]uint8{{
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
	0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
	0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
	0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
	0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
	0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
	0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
	0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
	0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
	0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
	0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
	0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
}, {
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E,
	0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E,
	0x40, 0x42, 0x44, 0x46, 0x48, 0x4A, 0x4C, 0x4E, 0x50, 0x52, 0x54, 0x56, 0x58, 0x5A, 0x5C, 0x5E,
	0x60, 0x62, 0x64, 0x66, 0x68, 0x6A, 0x6C, 0x6E, 0x70, 0x72, 0x74, 0x76, 0x78, 0x7A, 0x7C, 0x7E,
	0x81, 0x83, 0x85, 0x87, 0x89, 0x8B, 0x8D, 0x8F, 0x91, 0x93, 0x95, 0x97, 0x99, 0x9B, 0x9D, 0x9F,
	0xA1, 0xA3, 0xA5, 0xA7, 0xA9, 0xAB, 0xAD, 0xAF, 0xB1, 0xB3, 0xB5, 0xB7, 0xB9, 0xBB, 0xBD, 0xBF,
	0xC1, 0xC3, 0xC5, 0xC7, 0xC9, 0xCB, 0xCD, 0xCF, 0xD1, 0xD3, 0xD5, 0xD7, 0xD9, 0xDB, 0xDD, 0xDF,
	0xE1, 0xE3, 0xE5, 0xE7, 0xE9, 0xEB, 0xED, 0xEF, 0xF1, 0xF3, 0xF5, 0xF7, 0xF9, 0xFB, 0xFD, 0xFF,
	0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E,
	0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E,
	0x40, 0x42, 0x44, 0x46, 0x48, 0x4A, 0x4C, 0x4E, 0x50, 0x52, 0x54, 0x56, 0x58, 0x5A, 0x5C, 0x5E,
	0x60, 0x62, 0x64, 0x66, 0x68, 0x6A, 0x6C, 0x6E, 0x70, 0x72, 0x74, 0x76, 0x78, 0x7A, 0x7C, 0x7E,
	0x81, 0x83, 0x85, 0x87, 0x89, 0x8B, 0x8D, 0x8F, 0x91, 0x93, 0x95, 0x97, 0x99, 0x9B, 0x9D, 0x9F,
	0xA1, 0xA3, 0xA5, 0xA7, 0xA9, 0xAB, 0xAD, 0xAF, 0xB1, 0xB3, 0xB5, 0xB7, 0xB9, 0xBB, 0xBD, 0xBF,
	0xC1, 0xC3, 0xC5, 0xC7, 0xC9, 0xCB, 0xCD, 0xCF, 0xD1, 0xD3, 0xD5, 0xD7, 0xD9, 0xDB, 0xDD, 0xDF,
	0xE1, 0xE3, 0xE5, 0xE7, 0xE9, 0xEB, 0xED, 0xEF, 0xF1, 0xF3, 0xF5, 0xF7, 0xF9, 0xFB, 0xFD, 0xFF,
}, {
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
	0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D, 0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
	0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E, 0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
	0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF, 0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF,
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
	0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D, 0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
	0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E, 0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
	0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF, 0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF,
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
	0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D, 0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
	0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E, 0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
	0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF, 0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF,
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C, 0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
	0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D, 0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
	0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E, 0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
	0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF, 0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF,
}, {
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39, 0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD, 0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF,
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39, 0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD, 0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF,
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39, 0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD, 0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF,
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39, 0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD, 0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF,
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39, 0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD, 0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF,
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39, 0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD, 0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF,
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39, 0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD, 0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF,
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39, 0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD, 0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF,
}, {
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
	0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF,
}, {
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF, 0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF,
}, {
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
	0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF, 0x00, 0x55, 0xAA, 0xFF,
}, {
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
	0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
}}