
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

/*
 * The address of a command structure is a Spectrum address. (The
 * structure is built up in Spectrum memory by the Z80 program.)
 * The address is passed to the RP by the Z80 writing the address
 * into an unused Spectrum ROM location. This doesn't actually
 * write anything, of course, but the bus scanning code spots it
 * and caches away the low and high bytes of the 16 bit address
 * value. The writes are expected in low-then-high byte order,
 * which is how the low endian Z80 naturally does it.
 */
static uint8_t cmd_address_lo;
static uint8_t cmd_address_hi;
void cache_immediate_cmd_address_lo( uint8_t data )
{
  cmd_address_lo = data;

  /* Assume this is the first half of a 16-bit write */
  cmd_pending = 0;
}
void cache_immediate_cmd_address_hi( uint8_t data )
{
  cmd_address_hi = data;

  /*
   * Assume this is the second half of a 16-bit write and that the
   * value is now available
   */
  cmd_pending = 1;
}

/*
 * Pick up the address in Spectrum memory of the command structure which
 * defines the coprocessor command to run.
 */
static uint16_t query_immediate_cmd_address( void )
{
  return (cmd_address_hi << 8) + cmd_address_lo;
}

/*
 * Answers true if both low and high bytes of a command buffer
 * address have been written by the Z80 into the trigger register
 * IMMEDIATE_CMD_TRIGGER_REG.
 */
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
  DMA_BLOCK block = { (uint8_t*)src, zx_addr, n, 0 };
  dma_memory_block( &block, false );
}

/*
 * This is the entry point for all coprocessor commands which are executed immediately.
 * The address of the command structure is expected to have been written into the
 * IMMEDIATE_CMD_TRIGGER_REG register. 
 */
void service_immediate_cmd( void )
{
  /* Find the address previously written by the Z80 into IMMEDIATE_CMD_TRIGGER_REG */
  uint16_t cmd_zx_addr = query_immediate_cmd_address();

  /*
   * Pick up the address in the ZX memory of the command structure. This returns a
   * pointer into RP memory which contains the ZX memory image.
   */
  const CMD_STRUCT *cmd_ptr = query_zx_mirror_ptr( cmd_zx_addr );

  /* OK, whatever the Z80 program has requested, that's what we want to do */
  switch( cmd_ptr->type )
  {
    case ZXCOPRO_MEMSET_SMALL:
    {
      immediate_cmd_memset( (MEMSET_CMD*)((uint8_t*)cmd_ptr + sizeof( CMD_STRUCT )) );
    }
    break;

    default:
      // Need to the set the CMD's error to unknown
      break;
  }

  cmd_pending = 0; 
}
