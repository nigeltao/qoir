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

// This program prints the qoir_private_table_unlossify values.

import (
	"fmt"
)

func main() {
	for lossiness := 1; lossiness < 8; lossiness++ {
		table := [256]int{}
		for k := range table {
			x := (k << lossiness) & 0xFF
			v := 0
			for ; x > 0; x >>= (8 - lossiness) {
				v |= x
			}
			table[k] = v
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
