
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
static ZX_ADDR query_immediate_cmd_address( void )
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


static void immediate_cmd_memset( ZX_ADDR cmd_zx_addr, ZX_ADDR response_zx_addr )
{
  /*
   * Pick up the address in RP memory of the command structure. This returns a
   * pointer into RP memory.
   */
  const CMD_STRUCT *cmd_ptr = query_zx_mirror_ptr( cmd_zx_addr );

  /* The memset command structure immediately follows the command structure */
  MEMSET_CMD *memset_cmd_ptr = (MEMSET_CMD*)((uint8_t*)cmd_ptr + sizeof( CMD_STRUCT ));

  /* memset_cmd_ptr is pointing into RP memory which contains the mirror of the Spectrum's RAM */
  const ZX_BYTE  *src    = &(memset_cmd_ptr->c);
  const ZX_ADDR   zx_addr = memset_cmd_ptr->zx_addr[0] + memset_cmd_ptr->zx_addr[1]*256;
  const ZX_WORD   n       = memset_cmd_ptr->n[0] + memset_cmd_ptr->n[1]*256;

  DMA_BLOCK block = { (uint8_t*)src, zx_addr, n, 0 };
  dma_memory_block( &block, true );

  /*
   * FIXME This needs to be chained from the previous block, but
   * it would only save about 1us, maybe a little less, so it can
   * go on the TODO list for now
   */
  ZXCOPRO_RESPONSE response_ok = ZXCOPRO_OK;
  DMA_BLOCK response = { (uint8_t*)&response_ok, response_zx_addr, 1, 0 };
  dma_memory_block( &response, true );
}

/*
 * This is the entry point for all coprocessor commands which are executed immediately.
 * The address of the command structure is expected to have been written into the
 * IMMEDIATE_CMD_TRIGGER_REG register. 
 */
void service_immediate_cmd( void )
{
  /*
   * Find the address in ZX memory previously written by the Z80 into IMMEDIATE_CMD_TRIGGER_REG.
   * That's the start of the CMD_STRUCT in the Z80 address space. The response to the ZX from
   * the copro goes back in a member of that structure.
   */
  const ZX_ADDR cmd_zx_addr      = query_immediate_cmd_address();
  const ZX_ADDR response_zx_addr = cmd_zx_addr + offsetof( CMD_STRUCT, response );

  /*
   * Pick up the address in RP memory of the command structure and fetch the
   * command type from it
   */
  const CMD_STRUCT *cmd_ptr = query_zx_mirror_ptr( cmd_zx_addr );
  const ZXCOPRO_CMD cmd_type = cmd_ptr->type;

  /* OK, whatever the Z80 program has requested, that's what we want to do */
  switch( cmd_type )
  {
    case ZXCOPRO_MEMSET_SMALL:
    {
      immediate_cmd_memset( cmd_zx_addr, response_zx_addr );
    }
    break;

    default:
      // Need to the set the CMD's error to unknown
      break;
  }

  cmd_pending = 0; 
}
