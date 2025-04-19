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
 * Valid responses from the coprocessor back into the Spectrum's memory.
 * These are for the high level command structure. Individual commands
 * will have error values of their own.
 */
typedef enum
{
  ZXCOPRO_OK = 1,             /* Assume the Z80 starts with it as 0, so non-zero starting point here */
  ZXCOPRO_CMD_ERROR,
}
ZXCOPRO_RESPONSE;

/*
 * Error codes for the command structure.
 */
typedef enum
{
  ZXCOPRO_CMD_ERR_BAD_STRUCT = 1,
  ZXCOPRO_CMD_ERR_UNKNOWN_CMD,

  ZXCOPRO_CMD_ERR_LAST
}
ZXCOPRO_ERROR;

/*
 * This structure defines a coprocessor request. It starts on the Spectrum
 * from where the coprocessor reads its contents.
 */
typedef struct _cmd_struct
{
  ZXCOPRO_CMD      type;
  ZXCOPRO_RESPONSE response;
  ZXCOPRO_ERROR    error; 
}
CMD_STRUCT;

#endif
