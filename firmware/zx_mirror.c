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
#include "rom_emulation.h"
#include "rom.h"

/*
 * Local copy of the ZX memory, 64K including the ROM
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

void initialise_zx_mirror( void )
{
  for( uint32_t i=0; i < ZX_MEMORY_SIZE; i++ )
  {
    zx_memory_mirror[i]=0;
  }

  if( using_rom_emulation() )
  {  
    /* Copy the original ROM image into the Z80 memory mirror */
    memcpy( zx_memory_mirror, _48_original_rom, _48_original_rom_len );
  }

  return;
}
