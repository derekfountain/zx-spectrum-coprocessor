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

#ifndef __DMA_ENGINE_H
#define __DMA_ENGINE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * This data structure defines a DMA block to write to the Spectrum.
 */
typedef struct _dma_block
{
  uint8_t  *src;               // Location in RP memory to read from
  uint32_t  zx_ram_location;   // Location in ZX memory to write into
  uint32_t  length;            // Number of bytes to write
  uint32_t  incr;              // Number of bytes to increment src by

  struct _dma_block* next_ptr; // Pointer to next one of these
} DMA_BLOCK;

void init_dma_engine( void );
void init_interrupt_protection( void );

void add_dma_to_queue( uint8_t *src, uint32_t zx_ram_location, uint32_t length );
uint32_t is_dma_queue_full( void );
void activate_dma_queue_entry( void );

void dma_memory_block( const DMA_BLOCK *data_block,
                       const bool int_protection );

#endif
