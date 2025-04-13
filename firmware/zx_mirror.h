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

#ifndef __ZX_MIRROR_H
#define __ZX_MIRROR_H

#include <stdint.h>

#define ZX_MEMORY_SIZE           ((uint32_t)65536)

void initialise_zx_mirror( void );
uint8_t get_zx_mirror_byte( uint32_t offset );
void put_zx_mirror_byte( uint32_t offset, uint8_t value );

const void *query_zx_mirror_ptr( uint16_t offset );

#endif
