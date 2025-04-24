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

#ifndef __CMD_H
#define __CMD_H

#include <stdint.h>
#include <stdbool.h>
#include "zx_copro.h"
#include "dma_engine.h"

/*
 * These are the commands the coprocessor is able to fulfill.
 */
typedef enum
{
  ZXCOPRO_MEMSET_SMALL  = 128,
  ZXCOPRO_MEMSET_LARGE,          // FIXME Still not sure if commands which run on /int should be separate or flagged
}
ZXCOPRO_CMD;


/*
 * Error codes for the command structure.
 */
typedef enum
{
  CMD_ERR_BAD_STRUCT = DMA_RESULT_LAST,
  CMD_ERR_UNKNOWN_CMD,

  CMD_ERR_BAD_ARG,         // Arguments make no sense
  CMD_ERR_TOO_BIG,         // Number of bytes to DMA is too large
  CMD_ERR_BAD_INCR,        // An increment value is way out

  CMD_ERR_LAST
}
CMD_ERROR;

/*
 * This structure defines a coprocessor request. It's created on the Spectrum
 * from where the coprocessor reads its contents (e.g. type) and writes back the
 * response and error codes as appropriate.
 * The bytes in the Spectrum memory which immediately follow this structure are
 * the arguments to the command, and are defined by 'type'.
 */
typedef struct _cmd_struct
{
  ZXCOPRO_CMD      type;
  ZXCOPRO_RESPONSE response;
  CMD_ERROR        error; 
}
CMD_STRUCT;

void dma_response_to_zx( ZXCOPRO_RESPONSE response, ZX_ADDR response_zx_addr, ZX_ADDR error_zx_addr );
void dma_error_to_zx( ZXCOPRO_RESPONSE response, ZX_ADDR error_zx_addr );

#endif
