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
 * This structure defines a coprocessor request. It starts on the Spectrum
 * from where the coprocessor reads its contents.
 */
typedef struct _cmd_struct
{
  ZXCOPRO_CMD type;
  uint8_t     result;
  uint8_t     error; 
}
CMD_STRUCT;

#endif
