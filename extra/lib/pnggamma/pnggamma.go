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

// ----------------

// Package pnggamma parses the gamma correction value from a PNG image.
//
// The image/png package in Go's standard library, as of Go 1.19 (August 2022),
// does not parse gamma. See https://github.com/golang/go/issues/20613
package pnggamma

import (
	"errors"
)

// DefaultGamma can be returned by DecodeGamma.
const DefaultGamma = 2.2

// These errors can be returned by DecodeGamma.
var (
	ErrNoExplicitGamma = errors.New("pnggamma: no explicit gamma")
	ErrNotAPNG         = errors.New("pnggamma: not a PNG")
)

// DecodeGamma returns the gamma correction value from a PNG-encoded image.
// These are optional. It is valid for PNG images to not contain a gamma.
//
// This function returns:
//   - (aPositiveNumber, nil) if the input is a valid PNG image that explicitly
//     states its gamma.
//   - (DefaultGamma, ErrNoExplicitGamma) if the input is a valid PNG image
//     that does not explicitly state its gamma.
//   - (DefaultGamma, ErrNotAPNG) if the input is not a valid PNG image.
//
// The float64 returned is always positive (and DefaultGamma, 2.2, is a
// reasonable value to use) regardless of the error returned's nil-ness.
//
// "A valid PNG image" is not comprehensively checked, as e.g. zlib
// decompression validity checking is much more computationally expensive than
// simply parsing a gamma correction value. There may be false positives.
func DecodeGamma(encodedPNG []byte) (float64, error) {
	if (len(encodedPNG) < 8) || (string(encodedPNG[:8]) != "\x89PNG\r\n\x1a\n") {
		return DefaultGamma, ErrNotAPNG
	}

	gamaChunkGamma := 0.0
	srgbChunkGamma := 0.0

	for src := encodedPNG[8:]; len(src) >= 12; {
		chunkLen := u32be(src)
		if uint64(len(src)) < (12 + uint64(chunkLen)) {
			return DefaultGamma, ErrNotAPNG
		}
		src = src[4:]

		chunkTag := u32be(src)
		src = src[4:]
		switch chunkTag {
		case 0x49444154: // 'IDAT'be
			// In theory, we'd want an iccpChunkGamma value too, that trumps
			// srgbChunkGamma and gamaChunkGamma, but we don't in practice. See
			// the other "In theory..." comment below.
			if srgbChunkGamma > 0 {
				return srgbChunkGamma, nil
			} else if gamaChunkGamma > 0 {
				return gamaChunkGamma, nil
			}
			return DefaultGamma, ErrNoExplicitGamma

		case 0x67414D41: // 'gAMA'be
			if chunkLen != 4 {
				break
			}
			inverseGamma := u32be(src)
			// Special-case some common inverseGamma values to be more precise.
			if inverseGamma == 45455 {
				gamaChunkGamma = 2.2 // Instead of 2.199978000219998.
			} else if inverseGamma == 55556 {
				gamaChunkGamma = 1.8 // Instead of 1.799985600115199.
			} else if inverseGamma != 0 {
				gamaChunkGamma = 100000 / float64(inverseGamma)
			}

		case 0x69434350: // 'iCCP'be
			// In theory, we should parse the ICC profile and compute the gamma
			// value from that. However, not all PNG decoders handle iCCP
			// chunks, so the PNG specification says: "A PNG encoder that
			// writes the iCCP chunk is encouraged to also write gAMA and cHRM
			// chunks that approximate the ICC profile, to provide
			// compatibility with applications that do not use the iCCP chunk."
			//
			// This case here can be often a no-op, in practice, so it's
			// probably not worth the complexity to implement.

		case 0x73524742: // 'sRGB'be
			srgbChunkGamma = 2.2
		}

		// Skip the payload and checksum.
		src = src[4+uint64(chunkLen):]
	}
	return DefaultGamma, ErrNotAPNG
}

func u32be(b []byte) uint32 {
	return (uint32(b[0]) << 24) |
		(uint32(b[1]) << 16) |
		(uint32(b[2]) << 8) |
		(uint32(b[3]) << 0)
}
