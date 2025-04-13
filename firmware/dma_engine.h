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

void init_dma_engine( void );
void init_interrupt_protection( void );

void add_dma_to_queue( uint8_t *src, uint32_t zx_ram_location, uint32_t length );
uint32_t is_dma_queue_full( void );
void activate_dma_queue_entry( void );

void dma_memory_block( const uint8_t *src,    const uint32_t zx_ram_location,
                       const uint32_t length, const uint32_t incr,
                       const uint32_t int_protection );

#endif
