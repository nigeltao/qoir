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

//go:build ignore

package main

// This program prints qoir_private_encode_tile_opcodes's dists table.

import (
	"fmt"
)

func main() {
	dists := [256]int{}
	for n := 8; n >= 0; n-- {
		width := 1 << n
		start := -(width / 2)
		for i := 0; i < width; i++ {
			dists[uint8(start+i)] = width / 2
		}
	}
	for k, v := range dists {
		if (k & 7) == 7 {
			fmt.Printf("0x%02X,  //\n", v)
		} else {
			fmt.Printf("0x%02X, ", v)
		}
	}
}
