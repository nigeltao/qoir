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
	lossiness = flag.Int("lossiness", 4, "From 0 (lossless for 8-bit color) to 7 (very lossy)")
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
	gamma, _ := pnggamma.DecodeGamma(src)
	dst, err := gammaawaredither.Dither(img, *lossiness, &gammaawaredither.DitherOptions{
		Gamma: gamma,
	})
	if err != nil {
		return err
	}
	return png.Encode(os.Stdout, dst)
}
