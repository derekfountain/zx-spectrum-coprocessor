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

#ifndef __Z80_TEST_IMAGE_H
#define __Z80_TEST_IMAGE_H

#include <stdint.h>

uint32_t  using_z80_test_image( void );

/* This sets up the pointers and size, etc */
void     init_z80_test_image( void );

/* This marks the test image ready to load into the ZX at the next opportunity */
void z80_test_image_set_pending( void );

uint32_t is_z80_test_ready( void );


#endif