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

// This program prints the qoir_private_table_dists values.

import (
	"fmt"
)

func main() {
	table := [256]int{}
	for lossiness := 0; lossiness < 8; lossiness++ {
		if lossiness == 0 {
			for n := 8; n >= 0; n-- {
				width := 1 << n
				start := -(width / 2)
				for i := 0; i < width; i++ {
					table[uint8(start+i)] = width / 2
				}
			}
		} else {
			max := 1 << (8 - lossiness)
			for k, v := range table {
				if v == max {
					table[k] = table[0xFF&(k+max)]
				}
			}
		}

		for k, v := range table {
			if (k & 15) == 0 {
				fmt.Printf("  0x%02X,", v)
			} else if (k & 15) == 15 {
				fmt.Printf("0x%02X,\n", v)
			} else {
				fmt.Printf("0x%02X,", v)
			}
		}
		if lossiness != 7 {
			fmt.Printf("}, {\n")
		}
	}
}
