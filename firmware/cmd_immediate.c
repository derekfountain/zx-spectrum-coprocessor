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

/*
 * If the result of the DMA back to the Spectrum was an error, translate that
 * DMA level error to one of the general ZX-coprocessor results. i.e. convert
 * from internal value to a value the Spectrum expects to receive.
 * 
 * @TODO Not sure about error handling yet, this masks what's
 * happened from the Z80 program which is not desirable. I think a separate
 * error handling module of some sort is required.
 */
static ZXCOPRO_RESPONSE dma_result_to_response( DMA_STATUS status )
{
  struct
  {
    DMA_STATUS status;
    ZXCOPRO_RESPONSE response;
  } lookup_table[] =
  {
    { DMA_STATUS_OK, ZXCOPRO_OK },
  };

  for( uint32_t i=0; i<sizeof(lookup_table)/sizeof(lookup_table[0]); i++ )
  {
    if( lookup_table[i].status == status )
      return lookup_table[i].response;
  }

  return ZXCOPRO_UNKNOWN_ERR;
}

static void immediate_cmd_memset( ZX_ADDR cmd_zx_addr, ZX_ADDR response_zx_addr, ZX_ADDR error_zx_addr )
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

  /* DMA the values to set, no increment on the block src pointer */
  DMA_BLOCK block = { (uint8_t*)src, zx_addr, n, 0 };
  DMA_STATUS status;
  if( (status=dma_memory_block( &block, true )) == DMA_STATUS_OK )
  {
    /* DMA the response into the ZX memory */
    dma_response_to_zx( ZXCOPRO_OK, response_zx_addr, error_zx_addr );
  }
  else
  {
    /* DMA the error into the ZX memory */
    dma_error_to_zx( dma_result_to_response(status), error_zx_addr );
  }
}

static void immediate_cmd_pxy2saddr( ZX_ADDR cmd_zx_addr, ZX_ADDR response_zx_addr, ZX_ADDR error_zx_addr )
{
  ZX_ADDR saddr_lut[] = { 0x4000, 0x4100, 0x4200, 0x4300, 0x4400, 0x4500, 0x4600, 0x4700,
                          0x4020, 0x4120, 0x4220, 0x4320, 0x4420, 0x4520, 0x4620, 0x4720,
                          0x4040, 0x4140, 0x4240, 0x4340, 0x4440, 0x4540, 0x4640, 0x4740,
                          0x4060, 0x4160, 0x4260, 0x4360, 0x4460, 0x4560, 0x4660, 0x4760,
                          0x4080, 0x4180, 0x4280, 0x4380, 0x4480, 0x4580, 0x4680, 0x4780,
                          0x40A0, 0x41A0, 0x42A0, 0x43A0, 0x44A0, 0x45A0, 0x46A0, 0x47A0,
                          0x40C0, 0x41C0, 0x42C0, 0x43C0, 0x44C0, 0x45C0, 0x46C0, 0x47C0,
                          0x40E0, 0x41E0, 0x42E0, 0x43E0, 0x44E0, 0x45E0, 0x46E0, 0x47E0,

                          0x4800, 0x4900, 0x4A00, 0x4B00, 0x4C00, 0x4D00, 0x4E00, 0x4F00,
                          0x4820, 0x4920, 0x4A20, 0x4B20, 0x4C20, 0x4D20, 0x4E20, 0x4F20,
                          0x4840, 0x4940, 0x4A40, 0x4B40, 0x4C40, 0x4D40, 0x4E40, 0x4F40,
                          0x4860, 0x4960, 0x4A60, 0x4B60, 0x4C60, 0x4D60, 0x4E60, 0x4F60,
                          0x4880, 0x4980, 0x4A80, 0x4B80, 0x4C80, 0x4D80, 0x4E80, 0x4F80,
                          0x48A0, 0x49A0, 0x4AA0, 0x4BA0, 0x4CA0, 0x4DA0, 0x4EA0, 0x4FA0,
                          0x48C0, 0x49C0, 0x4AC0, 0x4BC0, 0x4CC0, 0x4DC0, 0x4EC0, 0x4FC0,
                          0x48E0, 0x49E0, 0x4AE0, 0x4BE0, 0x4CE0, 0x4DE0, 0x4EE0, 0x4FE0,

                          0x5000, 0x5100, 0x5200, 0x5300, 0x5400, 0x5500, 0x5600, 0x5700,
                          0x5020, 0x5120, 0x5220, 0x5320, 0x5420, 0x5520, 0x5620, 0x5720,
                          0x5040, 0x5140, 0x5240, 0x5340, 0x5440, 0x5540, 0x5640, 0x5740,
                          0x5060, 0x5160, 0x5260, 0x5360, 0x5460, 0x5560, 0x5660, 0x5760,
                          0x5080, 0x5180, 0x5280, 0x5380, 0x5480, 0x5580, 0x5680, 0x5780,
                          0x50A0, 0x51A0, 0x52A0, 0x53A0, 0x54A0, 0x55A0, 0x56A0, 0x57A0,
                          0x50C0, 0x51C0, 0x52C0, 0x53C0, 0x54C0, 0x55C0, 0x56C0, 0x57C0,
                          0x50E0, 0x51E0, 0x52E0, 0x53E0, 0x54E0, 0x55E0, 0x56E0, 0x57E0
  };

  /*
   * Pick up the address in RP memory of the command structure. This returns a
   * pointer into RP memory.
   */
  const CMD_STRUCT *cmd_ptr = query_zx_mirror_ptr( cmd_zx_addr );

  /* The memset command structure immediately follows the command structure */
  PXY2SADDR_CMD *pxy2saddr_ptr = (PXY2SADDR_CMD*)((uint8_t*)cmd_ptr + sizeof( CMD_STRUCT ));

  /* pxy2saddr_ptr is pointing into RP memory which contains the mirror of the Spectrum's RAM */
  const ZX_BYTE  x          = pxy2saddr_ptr->x;
  const ZX_BYTE  y          = pxy2saddr_ptr->y;
  const ZX_ADDR result_addr = cmd_zx_addr + sizeof( CMD_STRUCT ) + offsetof( PXY2SADDR_CMD, result );

  /* x and result address are limited to valid numbers by data size */
  if( y <= 191 )
  {
    /*
    * Calculate the screen address of the pixel, based on look up table.
    * ARM is little endian, so I can pass this straight back to Z80 land
    */
    ZX_ADDR answer = saddr_lut[y]+(x/8);

    /* DMA the values to set */
    DMA_BLOCK block = { (uint8_t*)&answer, result_addr, 2, 1 };
    DMA_STATUS status;
    if( (status=dma_memory_block( &block, true )) == DMA_STATUS_OK )
    {
      /* DMA the response into the ZX memory */
      dma_response_to_zx( ZXCOPRO_OK, response_zx_addr, error_zx_addr );
    }
    else
    {
      /* DMA the error into the ZX memory */
      dma_error_to_zx( dma_result_to_response(status), error_zx_addr );
    }
  }
  else
  {
    dma_error_to_zx( CMD_ERR_BAD_ARG, error_zx_addr );
  }
}

/*
 * This is the entry point for all coprocessor commands which are executed immediately.
 * The address of the command structure is expected to have been written into the
 * IMMEDIATE_CMD_TRIGGER_REG register and is passed in here.
 */
void service_immediate_cmd( ZX_ADDR cmd_zx_addr )
{
  /*
   * We have the start of the CMD_STRUCT in the Z80 address space. The response to the ZX from
   * the copro goes back in a member of that structure, or maybe an error.
   */
  const ZX_ADDR response_zx_addr = cmd_zx_addr + offsetof( CMD_STRUCT, response );
  const ZX_ADDR error_zx_addr    = cmd_zx_addr + offsetof( CMD_STRUCT, error );

  /*
   * Pick up the address in RP memory of the command structure and fetch the
   * command type from it
   */
  const CMD_STRUCT *cmd_ptr = query_zx_mirror_ptr( cmd_zx_addr );
  const ZXCOPRO_CMD cmd_type = cmd_ptr->type;

  /*
   * OK, whatever the Z80 program has requested, that's what we want to do.
   * Probably need a table here, let's see how big it grows
   */
  switch( cmd_type )
  {
    case ZXCOPRO_MEMSET_SMALL:
    {
      immediate_cmd_memset( cmd_zx_addr, response_zx_addr, error_zx_addr );
    }
    break;
    case ZXCOPRO_PXY2SADDR:
    {
      immediate_cmd_pxy2saddr( cmd_zx_addr, response_zx_addr, error_zx_addr );
    }
    break;

    default:
    {
      /* @FIXME Error handling is totally wrong, not sure what value this sends! */
      dma_error_to_zx( CMD_ERR_UNKNOWN_CMD, error_zx_addr );
    }
    break;
  }
}
