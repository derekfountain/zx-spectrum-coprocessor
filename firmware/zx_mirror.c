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

#include <string.h>

#include "zx_mirror.h"

/*
 * Local copy of the ZX memory, 64K including the ROM. The ROM area in this
 * image is unused. The ROM emulation code uses its own buffer for that. 
 * The $0000 to $3FFF area here will be updated if anything writes to ROM
 * (which some code in the Spectrum ROM does do). But it will be ignored.
 */
static uint8_t zx_memory_mirror[ZX_MEMORY_SIZE];

inline uint8_t get_zx_mirror_byte( uint32_t offset )
{
  return zx_memory_mirror[offset];
}

inline void put_zx_mirror_byte( uint32_t offset, uint8_t value )
{
  zx_memory_mirror[offset] = value;
}

const void *query_zx_mirror_ptr( uint16_t offset )
{
  return &zx_memory_mirror[offset];
}

void initialise_zx_mirror( void )
{
  for( uint32_t i=0; i < ZX_MEMORY_SIZE; i++ )
  {
    zx_memory_mirror[i]=0;
  }

  return;
}
