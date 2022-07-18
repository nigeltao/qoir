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

// This program prints the qoir_private_table_lumaN values.

import (
	"fmt"
)

func main() {
	for k := uint32(0); k < 256; k += 2 {
		deltaG := ((k >> 0x01) & 0x07) - 4
		deltaB := deltaG - 2 + ((k >> 0x04) & 0x03)
		deltaR := deltaG - 2 + ((k >> 0x06) & 0x03)
		if (k & 6) == 0 {
			fmt.Printf("    ")
		}
		fmt.Printf("0x%02X,0x%02X,0x%02X,0,", uint8(deltaB), uint8(deltaG), uint8(deltaR))
		if (k & 6) == 6 {
			fmt.Println()
		}
	}

	fmt.Println()

	for k := uint32(0); k < 65536; k += 4 {
		deltaG := ((k >> 0x02) & 0x3F) - 32
		deltaB := deltaG - 8 + ((k >> 0x08) & 0x0F)
		deltaR := deltaG - 8 + ((k >> 0x0C) & 0x0F)
		if (k & 12) == 0 {
			fmt.Printf("    ")
		}
		fmt.Printf("0x%02X,0x%02X,0x%02X,0,", uint8(deltaB), uint8(deltaG), uint8(deltaR))
		if (k & 12) == 12 {
			fmt.Println()
		}
	}
}
