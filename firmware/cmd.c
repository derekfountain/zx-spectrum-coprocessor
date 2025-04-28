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

#include <stdint.h>

#include "zx_copro.h"
#include "cmd.h"
#include "dma_engine.h"

/*
 * Return a response to the Spectrum in the original DMA command structure. The
 * Z80 is expecting to be watching for this. 
 * 
 * If this DMA fails then an error code is DMAed back instead, for whatever good
 * that might do.
 */
void dma_response_to_zx( ZXCOPRO_RESPONSE response, ZX_ADDR response_zx_addr, ZX_ADDR error_zx_addr )
{
  DMA_BLOCK block = { (uint8_t*)&response, response_zx_addr, 1, 0 };
  if( dma_memory_block( &block, true ) != DMA_STATUS_OK )
  {
    dma_error_to_zx( ZXCOPRO_UNABLE_TO_RETURN_RESPONSE, error_zx_addr );
  }

  return;
}

/*
 * Return an error to the Spectrum in the original DMA command structure. The
 * Z80 program is expected to be watching for this, if it cares.
 * 
 * This is the end of the line. If this DMA fails there's nothing more I can do.
 */
void dma_error_to_zx( ZXCOPRO_RESPONSE error_code, ZX_ADDR error_zx_addr )
{
  DMA_BLOCK block = { (uint8_t*)&error_code, error_zx_addr, 1, 0 };
  (void)dma_memory_block( &block, true );

  return;
}

