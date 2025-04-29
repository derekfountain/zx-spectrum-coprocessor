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

#ifndef __ZX_COPRO_H
#define __ZX_COPRO_H

#include <stdint.h>

/*
 * Status status from the coprocessor back into the Spectrum's memory.
 * These are for the highest level structure. Individual processes and commands
 * have result values of their own.
 */
typedef enum
{
  ZXCOPRO_NONE        = 0,
  ZXCOPRO_OK          = 1,
  ZXCOPRO_ERROR       = 2,

  ZXCOPRO_UNABLE_TO_RETURN_RESPONSE,
  ZXCOPRO_UNKNOWN
}
ZXCOPRO_STATUS;

/*
 * An address in the Z80's memory space is 0x0000 to 0xFFFF inclusive.
 */
typedef uint16_t ZX_ADDR;

typedef uint8_t  ZX_BYTE;
typedef uint16_t ZX_WORD;

#endif
