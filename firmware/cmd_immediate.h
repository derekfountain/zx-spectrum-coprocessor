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

/* This is an unused location in the ROM, unlikely to be written to accidently */
#define IMMEDIATE_CMD_TRIGGER_REG     ((uint64_t)0x386E)

/* I'm looking for a memory (MREQ) read (RD) from the address bus. This masks out all other GPIOs */
#define IMMEDIATE_CMD_TRIGGER_MASK    (GPIO_ABUS_BITMASK | WR_MREQ_MASK)

/* So mask in the above GPIOs and see if the result is one of these. (RD and MREQ need to be 0) */
#define IMMEDIATE_CMD_TRIGGER_PATTERN_LO (IMMEDIATE_CMD_TRIGGER_REG<<GPIO_ABUS_A0)
#define IMMEDIATE_CMD_TRIGGER_PATTERN_HI ((IMMEDIATE_CMD_TRIGGER_REG+1)<<GPIO_ABUS_A0)

uint32_t is_immediate_cmd_pending( void );
void     service_immediate_cmd( void );
void     cache_immediate_cmd_address_lo( uint8_t data );
void     cache_immediate_cmd_address_hi( uint8_t data );
uint16_t query_immediate_cmd_address( void );

#endif
