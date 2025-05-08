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

#ifndef __TRACE_TABLE_H
#define __TRACE_TABLE_H

#include <stdint.h>
#include "zx_copro.h"
#include "cmd.h"
#include "dma_engine.h"

void trace_table_new_entry(  void );
void trace_table_set_cmd_args(  const ZXCOPRO_CMD cmd, const uint8_t flags );
void trace_table_set_dma_args(  const uint8_t *src, const ZX_ADDR zx_ram_location, const uint32_t length );
void trace_table_set_dma_mode(  const DMA_MODE dma_mode );
void trace_table_set_status(  const ZXCOPRO_STATUS status );
void trace_table_set_error(  const ZXCOPRO_STATUS error );


#endif
