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

package main

import (
	"bytes"
	"errors"
	"flag"
	"image"
	_ "image/gif"
	_ "image/jpeg"
	"image/png"
	"io"
	"os"

	"github.com/nigeltao/qoir/extra/lib/gammaawaredither"
	"github.com/nigeltao/qoir/extra/lib/pnggamma"
)

var (
	gammaFlag     = flag.Float64("gamma", 0, "Overrides the input's gamma if positive; 1.0 means naive dithering")
	lossinessFlag = flag.Int("lossiness", 4, "From 0 (lossless for 8-bit color) to 7 (very lossy)")
)

func main() {
	if err := main1(); err != nil {
		os.Stderr.WriteString(err.Error() + "\n")
		os.Exit(1)
	}
}

func main1() error {
	flag.Parse()
	if flag.NArg() != 0 {
		return errors.New("Usage: gammaawaredither -lossiness=4 < in.png > out.png")
	}
	src, err := io.ReadAll(os.Stdin)
	if err != nil {
		return err
	}
	img, _, err := image.Decode(bytes.NewReader(src))
	if err != nil {
		return err
	}
	dg, _ := pnggamma.DecodeGamma(src)
	if *gammaFlag > 0 {
		dg.Gamma = *gammaFlag
	}
	dstImage, err := gammaawaredither.Dither(img, *lossinessFlag, &gammaawaredither.DitherOptions{
		Gamma: dg.Gamma,
	})
	if err != nil {
		return err
	}
	dstBuffer := &bytes.Buffer{}
	err = png.Encode(dstBuffer, dstImage)
	if err != nil {
		return err
	}
	dstBytes := dstBuffer.Bytes()

	// Copy in the ICCP, SRGB and GAMA chunks if they were in the source PNG.
	// They're spliced after the magic identifier (8 bytes) and IHDR chunk (25
	// bytes) of the png.Encode result.
	ret := []byte(nil)
	ret = append(ret, dstBytes[:8+25]...)
	ret = append(ret, dg.ICCPChunk...)
	ret = append(ret, dg.SRGBChunk...)
	ret = append(ret, dg.GAMAChunk...)
	ret = append(ret, dstBytes[8+25:]...)
	_, err = os.Stdout.Write(ret)
	return err
}
