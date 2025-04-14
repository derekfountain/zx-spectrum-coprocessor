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

#ifndef __ZX_MEMORY_MANAGEMENT_H
#define __ZX_MEMORY_MANAGEMENT_H

#include <stdint.h>

#include "pico/sync.h"

typedef enum
{
  FULL_ROM_EMULATION,
  RAM_MIRROR_ONLY
}
EMULATION_MODE;

uint32_t using_rom_emulation( void );

void start_rom_emulation( EMULATION_MODE );
void set_initial_jp( uint16_t dest );
void reset_initial_jp( void );

#endif
