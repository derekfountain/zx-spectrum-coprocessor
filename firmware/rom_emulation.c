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

#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

#include "rom_emulation.h"
#include "zx_mirror.h"
#include "z80_test_image.h"

#include "gpios.h"

//overclock appears to be needed, but why? am i really that close to the speed limit?
//#define OVERCLOCK 270000

static uint16_t initial_jp_destination = 0;

void set_initial_jp( uint16_t dest )
{
  initial_jp_destination = dest;
}

void reset_initial_jp( void )
{
  initial_jp_destination = 0;
}

uint32_t using_rom_emulation( void )
{
#define EMULATE_ROM 1
  return EMULATE_ROM;
}

static void __time_critical_func(core1_rom_emulation)( void )
{
  irq_set_mask_enabled( 0xFFFFFFFF, 0 );

#ifdef OVERCLOCK
  set_sys_clock_khz( OVERCLOCK, 1 );
#endif

  initial_jp_destination = 0;

  while( 1 )
  {
    const uint64_t mreq_mask = MREQ_MASK;
    const uint64_t rd_mask   = RD_MASK;
    const uint64_t wr_mask   = WR_MASK;

    register uint64_t gpios;
    
    /*
     * Ths fastest Z80 memory read is the M1 instruction fetch. It takes 1.5 
     * Z80 clock cycles from MREQ going low to the CPU reading the data byte
     * from the data bus. That's about 428ns.
     */

    /* Spin, waiting for a memory request. (Approx 90ns to 100ns)  */
//    while( (((gpios = gpio_get_all64()) & RD_MREQ_MASK)) && (gpios & WR_MREQ_MASK) );
//    gpios = gpio_get_all64();
  gpio_put( GPIO_BLIPPER1, 0 );
    while( ((gpios = gpio_get_all64()) & mreq_mask) );
  gpio_put( GPIO_BLIPPER1, 1 );

    /* Pick up the address being accessed (approx 20ns) */
    register uint64_t address = (gpios & GPIO_ABUS_BITMASK) >> GPIO_ABUS_A0;

    /* Is it a read that's happening? (Approx 35ns) */
    if( (gpios & rd_mask) == 0 )
    {
      /* Ignore reads from anywhere other than ROM, the Spectrum still reads its own RAM (Approx 15ns) */
      if( address <= 0x3FFF )
      {
        /* Pick up ROM byte from local mirror (Approx 100ns) */
        uint8_t data = get_zx_mirror_byte( address );
#if 0
        if( using_z80_test_image() )
        {
          /* Inject JP to the z80 test code into bytes 0, 1 and 2 */
          if( initial_jp_destination != 0 )
          {
            if(      address == 0x0000 ) data = 0xc3;
            else if( address == 0x0001 ) data = (uint8_t)(initial_jp_destination & 0xFF);
            else if( address == 0x0002 ) data = (uint8_t)((initial_jp_destination >> 8) & 0xFF);
          }
        }
#endif

        /* Set the data bus to outputs */
        gpio_set_dir_out_masked( GPIO_DBUS_BITMASK );

        /* Write the value out to the Z80 */
        gpio_put_masked64( GPIO_DBUS_BITMASK, (data & 0xFF) << GPIO_DBUS_D0 );

        /*
         * As of this point the data is on the bus ready for the CPU to read it.
         * The read happens 428ns after MREQ goes low. This point is reached
         * after approx 300ns to 375ns. With a 270MHz overclock the data is
         * consistently ready about 200ns to 220ns after MREQ goes low.
         */
  gpio_put( GPIO_BLIPPER2, 0 );
      
        /* Wait for the Z80's read to finish */
        while( (gpio_get_all64() & mreq_mask) == 0 );
  gpio_put( GPIO_BLIPPER2, 1 );

        /* Z80 has picked up the byte, put data bus back to inputs */
        gpio_set_dir_in_masked( GPIO_DBUS_BITMASK );
      }
      else
      {
        /*
          * It's a read from RAM, the Spectrum's RAM chips will field it.
          * Just wait for the read to finish we don't loop continuously
          * while this read is on the Z80 control bus
          */
        while( (gpio_get_all64() & mreq_mask) == 0 );        
      }
    }
#if 0
    else if( (gpios & WR_MREQ_MASK) == 0 )
    {
      /* Ignore writes to ROM */
      if( address >= 0x4000 )
      {
        /* Pick the value being written from the data bus and mirror it */
        uint8_t data = (gpios & GPIO_DBUS_BITMASK) & 0xFF;
        put_zx_mirror_byte( address, data );
      }

      /* Wait for the Z80 write to finish. MREQ stays low for around 285ns */
      while( (gpio_get_all64() & mreq_mask) == 0 );
    }
#endif
  } /* End infinite loop */
}


void start_rom_emulation( void )
{
  multicore_launch_core1( core1_rom_emulation );
}
