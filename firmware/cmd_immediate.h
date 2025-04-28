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

#ifndef __CMD_IMMEDIATE_H
#define __CMD_IMMEDIATE_H

#include <stdint.h>
#include "cmd.h"
#include "gpios.h"

/* This is an unused location in the ROM, unlikely to be written to accidently */
#define IMMEDIATE_CMD_TRIGGER_REG     ((uint64_t)0x386E)

/* I'm looking for a memory (MREQ) write (WR) from the address bus. This masks out all other GPIOs */
#define IMMEDIATE_CMD_TRIGGER_MASK    (GPIO_ABUS_BITMASK | WR_MREQ_MASK)

/* Mask in the above GPIOs and see if the result is one of these. (WR and MREQ need to be 0) */
#define IMMEDIATE_CMD_TRIGGER_PATTERN_LO (IMMEDIATE_CMD_TRIGGER_REG<<GPIO_ABUS_A0)
#define IMMEDIATE_CMD_TRIGGER_PATTERN_HI ((IMMEDIATE_CMD_TRIGGER_REG+1)<<GPIO_ABUS_A0)

void    service_immediate_cmd( ZX_ADDR zx_addr );

/*
 * memset, memory set coprocessor command
 *
 * This is for testing more than anything. I can set screen values, or small areas
 * of the upper RAM with it. It's faster than an LDIR memset.
 */
typedef struct _memset_cmd
{
  uint8_t zx_addr[2];   /* Z80 16 bit, low endian address in ZX memory */
  uint8_t c;            /* The constant to set */
  uint8_t n[2];         /* Z80 16 bit, low endian count of bytes to set */
} MEMSET_CMD;

/*
 * pxy2saddr, pixel x,y to screen address coprocessor command
 *
 * As per the same function in z88dk.
 */
typedef struct _pxy2saddr_cmd
{
  uint8_t x;            /* X pixel, 0 to 255 */
  uint8_t y;            /* Y pixel, 0 to 191 */
  ZX_ADDR result;       /* 2 byte place to put the result (16 bit address) */
} PXY2SADDR_CMD;

#endif
