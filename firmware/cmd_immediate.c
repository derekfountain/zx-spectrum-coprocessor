
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

#include "gpios.h"
#include "hardware/gpio.h"
#include "cmd_immediate.h"
#include "dma_engine.h"
#include "zx_mirror.h"

static uint32_t cmd_pending = 0;

static uint8_t cmd_address_lo;
void cache_immediate_cmd_address_lo( uint8_t data )
{
  cmd_address_lo = data;

  /* For now, assume this is the first half of a 16-bit write */
  cmd_pending = 0;
}

static uint8_t cmd_address_hi;
void cache_immediate_cmd_address_hi( uint8_t data )
{
  cmd_address_hi = data;

  /*
   * For now, assume this is the second half of a 16-bit write and that the
   * value is now available
   */
  cmd_pending = 1;
}

uint16_t query_immediate_cmd_address( void )
{
  return (cmd_address_hi << 8) + cmd_address_lo;
}

inline uint32_t is_immediate_cmd_pending( void )
{
  return cmd_pending;
}



static void immediate_cmd_memset( MEMSET_CMD *memset_cmd_ptr )
{
  const uint8_t  *src    = &(memset_cmd_ptr->c);
  const uint16_t  zx_addr = memset_cmd_ptr->zx_addr[0] + memset_cmd_ptr->zx_addr[1]*256;
  const uint16_t  n       = memset_cmd_ptr->n[0] + memset_cmd_ptr->n[1]*256;

  gpio_put( GPIO_BLIPPER1, 0 );
  dma_memory_block( src, zx_addr, n, 0, true );
}

void service_immediate_cmd( void )
{
  uint16_t cmd_zx_addr = query_immediate_cmd_address();
  const CMD *cmd_ptr = query_zx_mirror_ptr( cmd_zx_addr );

  switch( cmd_ptr->type )
  {
    case CMD_MEMSET_TYPE:
    {
      immediate_cmd_memset( (MEMSET_CMD*)((uint8_t*)cmd_ptr + sizeof( CMD )) );
    }
    break;

    default:
      // Need to the set the CMD's error to unknown
      break;
  }

  cmd_pending = 0; 
}
