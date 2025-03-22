/*
 * ZX Coprocessor Firmware, a Raspberry Pi RP2350b based Spectrum device
 * Copyright (C) 2025 Derek Fountain
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "z80_test_image.h"

/*
 * A test program, z80 machine code, expected to be ORGed at 0x8000.
 * Use xxd to create the header file, for example:
 *   > zcc +zx -vn -startup=5 -clib=sdcc_iy z80_image.c -o z80_image
 *   > xxd -i -c 16 z80_image_CODE.bin > ../../../firmware/z80_image.h
 */
#include "z80_image.h"

uint8_t *get_z80_test_image_src( void )
{
  return z80_image_CODE_bin;
}

uint16_t get_z80_test_image_dest( void )
{
  return 0x8000;
}

uint16_t get_z80_test_image_length( void )
{
  return z80_image_CODE_bin_len;
}

uint32_t using_z80_test_image( void )
{
#define USE_Z80_TEST_IMAGE 0
  return USE_Z80_TEST_IMAGE;
}

