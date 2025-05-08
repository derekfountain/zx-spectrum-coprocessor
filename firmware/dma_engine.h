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
#include "zx_copro.h"       /* For ZX_ADDR etc */

/*
 * Error codes for the low level DMA
 */
typedef enum
{
  DMA_STATUS_OK = 0,

  DMA_STATUS_BAD_STRUCT,
  DMA_STATUS_TOO_BIG,                    // Number of bytes to DMA is too large
  DMA_STATUS_TOO_SMALL,                  // Number of bytes to DMA is too small (zero)
  DMA_STATUS_TOP_BORDER_TOO_BIG,         // Number of bytes to DMA in top border time is too large
  DMA_STATUS_BAD_INCR,                   // An increment value is way out
  DMA_STATUS_CONTENTION_FAIL,            // DMA would clash with ULA's contention

  DMA_STATUS_LAST
}
DMA_STATUS;

/*
 * This data structure defines a DMA block to write to the Spectrum.
 */
typedef struct _dma_block
{
  uint8_t  *src;               // Location in RP memory to read from
  ZX_ADDR   zx_ram_location;   // Location in ZX memory to write into
  uint32_t  length;            // Number of bytes to write
  uint32_t  incr;              // Number of bytes to increment src by
  bool      ignore_interrupt;  // True if the Z80 is OK to ignore interrupt protection
  bool      top_border_time;   // True if the Z80 has set the DMA to run in top border time

  struct _dma_block* next_ptr; // Pointer to next one of these
} DMA_BLOCK;

  /*
   * Modes: (visible externally so the tracing can track it)
   *
   * contended means 0x4000 to 0x7FFF on the fly. The DMA can happen at the speed the
   * ULA can drive RAS/CAS, but the transfer needs to respect memory contention. That
   * means mimicking the Z80 exactly (i.e. stopping when the Z80 clock stops, etc).
   * This mode doesn't work. 
   * 
   * top border means 0x4000 to 0x7FFF, but the Z80 program guarantees the DMA is
   * happening in top border time. That means there can't be contention and the code
   * here doesn't need to use Z80 timings. This is the fastest mode, it runs as fast
   * as the ULA can drive RAS/CAS.
   * 
   * uncontended means 0x8000 to 0xFFFF. This is simple and reliable, no contention
   * to worry about. But the DMA needs to run at the speed the 74-series logic chips
   * can drive the 4164s at, which is slower than the ULA drives the 4116s.
   */
  typedef enum
  {
    DMA_MODE_CONTENDED,
    DMA_MODE_TOP_BORDER,
    DMA_MODE_UNCONTENDED
  }
  DMA_MODE;

/*
 * In theory a DMA could fill the Z80 memory space. Not sure why
 * anyone would want to.
 */
#define MAX_DMA_LENGTH ((uint32_t)65536)

/*
 * The increment is to allow DMAing a chunk of data which is spanned across
 * memory. Anything greater than maybe 32 or 64 bytes is unlikely to make
 * sense, but you never know.
 */
#define MAX_INCR ((uint32_t)4096)

/*
 * This defines the largest DMA it's possible to do inside the time the ULA
 * takes to draw the top border. This refers to contended memory DMAs, typically
 * into ZX screen memory, using ULA DMA timings.
 */
#define TOP_BORDER_MAX_LENGTH  ((uint32_t)8192)

void init_dma_engine( void );
void init_interrupt_protection( void );

void add_dma_to_queue( uint8_t *src, ZX_ADDR zx_ram_location, uint32_t length );
uint32_t is_dma_queue_full( void );
void activate_dma_queue_entry( void );

DMA_STATUS dma_memory_block( const DMA_BLOCK *data_block,
                             const bool int_protection );

#endif
