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
#include "trace_table.h"

/*
 * Return a status to the Spectrum in the original DMA command structure. The
 * Z80 is expecting to be watching for this. The error code in the DMA command
 * structure isn't changed, consider it undefined.
 * 
 * If this DMA fails then an error code is DMAed back instead, for whatever good
 * that might do.
 */
void dma_status_to_zx( ZXCOPRO_STATUS status, ZX_ADDR status_zx_addr, ZX_ADDR error_zx_addr )
{
  trace_table_new_entry();
  trace_table_set_dma_args( (uint8_t*)&status, status_zx_addr, 1 );
  
  /* @FIXME What if the original DMA was top border time? */
  DMA_BLOCK block = { (uint8_t*)&status, status_zx_addr, 1, 0 };
  if( dma_memory_block( &block, true ) != DMA_STATUS_OK )
  {
    dma_error_to_zx( ZXCOPRO_UNABLE_TO_RETURN_RESPONSE, status_zx_addr, error_zx_addr );
  }

  return;
}

/*
 * Return an error to the Spectrum in the original DMA command structure. The
 * error code given is passed back in original command structure's error value,
 * then the status value (which the Z80 program is expected to be watching) is
 * returned as ZXCOPRO_ERROR.
 * 
 * This is the end of the line. If this DMA fails there's nothing more I can do.
 * I don't check the return values.
 */
void dma_error_to_zx( ZXCOPRO_STATUS error_code, ZX_ADDR status_zx_addr, ZX_ADDR error_zx_addr )
{
  trace_table_new_entry();
  trace_table_set_dma_args( (uint8_t*)&error_code, error_zx_addr, 1 );
  trace_table_set_error( error_code );

  /* @FIXME What if the original DMA was top border time? */
  DMA_BLOCK err_block = { (uint8_t*)&error_code, error_zx_addr, 1, 0 };
  (void)dma_memory_block( &err_block, true );


  const ZXCOPRO_STATUS error_status = ZXCOPRO_ERROR;
  trace_table_new_entry();
  trace_table_set_dma_args( (uint8_t*)&error_status, status_zx_addr, 1 );
  trace_table_set_status( ZXCOPRO_ERROR );

  DMA_BLOCK status_block = { (uint8_t*)&error_status, status_zx_addr, 1, 0 };
  (void)dma_memory_block( &status_block, true );

  return;
}

