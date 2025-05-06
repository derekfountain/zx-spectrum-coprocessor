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

#include "trace_table.h"
#include "dma_engine.h"
#include "cmd.h"

#define NUM_TRACE_TABLE_ENTRIES   1024

typedef enum
{
  TRACE_TABLE_INVALID_ENTRY      = 0,
  TRACE_TABLE_INITIALISED_ENTRY  = 0x01,
  TRACE_TABLE_CMD_SET            = 0x02,
  TRACE_TABLE_DMA_SET            = 0x04,
  
  TRACE_TABLE_ZXCOPRO_STATUS_SET = 0x08,
  TRACE_TABLE_ZXCOPRO_ERROR_SET  = 0x10,
}
TRACE_TABLE_ENTRY_STATUS;

/* Trace table */
typedef struct _TRACE_TABLE_ENTRY
{
  TRACE_TABLE_ENTRY_STATUS entry_status;

  ZXCOPRO_CMD    cmd;
  uint8_t        flags;

  uint8_t       *src;
  ZX_ADDR        zx_ram_location;
  uint32_t       length;

  ZXCOPRO_STATUS zxcopro_status;
  ZXCOPRO_STATUS zxcopro_error;

  DMA_STATUS     dma_status;
  CMD_ERROR      error;
}
TRACE_TABLE_ENTRY;

static TRACE_TABLE_ENTRY trace_table[NUM_TRACE_TABLE_ENTRIES];

static int32_t current_entry_index = -1;

void trace_table_new_entry( void )
{
  if( current_entry_index == NUM_TRACE_TABLE_ENTRIES )
    current_entry_index = -1;

  current_entry_index++;

  trace_table[current_entry_index].entry_status |= TRACE_TABLE_INITIALISED_ENTRY;
}

void trace_table_set_cmd_args( const ZXCOPRO_CMD cmd, const uint8_t flags )
{
  trace_table[current_entry_index].cmd             = cmd;
  trace_table[current_entry_index].flags           = flags;

  trace_table[current_entry_index].entry_status    |= TRACE_TABLE_CMD_SET;
}

void trace_table_set_dma_args( const uint8_t *src, const ZX_ADDR zx_ram_location, const uint32_t length )
{
  trace_table[current_entry_index].src             = (uint8_t*)src;
  trace_table[current_entry_index].zx_ram_location = zx_ram_location;
  trace_table[current_entry_index].length          = length;

  trace_table[current_entry_index].entry_status    |= TRACE_TABLE_DMA_SET;
}

void trace_table_set_status( const ZXCOPRO_STATUS status )
{
  trace_table[current_entry_index].zxcopro_status   = status;

  trace_table[current_entry_index].entry_status    |= TRACE_TABLE_ZXCOPRO_STATUS_SET;
}

void trace_table_set_error( const ZXCOPRO_STATUS error )
{
  trace_table[current_entry_index].zxcopro_error   = error;

  trace_table[current_entry_index].entry_status    |= TRACE_TABLE_ZXCOPRO_ERROR_SET;
}

