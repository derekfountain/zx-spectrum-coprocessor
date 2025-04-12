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

#include <string.h>

#include "hardware/gpio.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

#include "rom_emulation.h"
#include "rom.h"
#include "zx_mirror.h"
#include "z80_test_image.h"

#include "gpios.h"

/*
 * Timings here are very tight. At 200MHz a single RP2350 instruction takes 5ns. 
 * Each cycle of the 3.5MHz Z80 is about 286ns, so there's time for 57 RP2350
 * instructions to execute in one Z80 cycle.
 * The Z80's M1 instruction fetch has MREQ low for one-and-a-half Z80 cycles. 
 * That's 85 RP2350 cycles to spot the MREQ, read the address bus, find the
 * data value and get it on the data bus.
 * I don't want to use a ridiculous overclock, 200MHz is fine, maybe a bit
 * faster if I really have to.
 * 
 * Testing shows 200MHz is fast enough for the time being. Comments in this code
 * are based on a 200MHz overclock.
 */
static uint16_t initial_jp_destination = 0;

void set_initial_jp( uint16_t dest )
{
  initial_jp_destination = dest;
}

static void core1_rom_emulation( void )
{
  irq_set_mask_enabled( 0xFFFFFFFF, 0 );

  while( 1 )
  {
    const uint64_t mreq_mask = MREQ_MASK;
    const uint64_t rd_mask   = RD_MASK;
    const uint64_t wr_mask   = WR_MASK;

    uint64_t gpios;
    
    /* Spin, waiting for a memory request. (Approx 90ns to 100ns)  */
    while( ((gpios = gpio_get_all64()) & mreq_mask) );

    /* Pick up the address being accessed (approx 20ns) */
    uint64_t address = (gpios & GPIO_ABUS_BITMASK) >> GPIO_ABUS_A0;

    /* Is it a read that's happening? (Approx 35ns) */
    if( (gpios & rd_mask) == 0 )
    {
      /* Ignore reads from anywhere other than ROM, the Spectrum still reads its own RAM (Approx 15ns) */
      if( using_z80_test_image() && (initial_jp_destination != 0) && (address <= 0x0002) )
      {
        uint8_t data;
//this isn't going to work because the multiple restarts of the z80
//it passes through address 0 multiple times
        /* Inject JP to the z80 test code into bytes 0, 1 and 2 */
        if(      address == 0x0000 )
        {
//  gpio_put( GPIO_BLIPPER2, 0 );
          gpio_set_dir( GPIO_ROMCS, GPIO_OUT ); gpio_put( GPIO_ROMCS, 1 );
          data = 0xc3;
        }
        else if( address == 0x0001 )
        {
          data = (uint8_t)(initial_jp_destination & 0xFF);
        }
        else if( address == 0x0002 )
        {
          data = (uint8_t)((initial_jp_destination >> 8) & 0xFF);
          initial_jp_destination = 0;
        }

        /* Set the data bus to outputs */
        gpio_set_dir_out_masked64( GPIO_DBUS_BITMASK );

        /* Write the value out to the Z80 */
        gpio_put_masked64( GPIO_DBUS_BITMASK, (data & 0xFF) << GPIO_DBUS_D0 );

        /*
         * As of this point the data is on the bus ready for the CPU to read it.
         * The read happens 428ns after MREQ goes low. This point is reached
         * after approx 300ns to 375ns. With a 270MHz overclock the data is
         * consistently ready about 200ns to 220ns after MREQ goes low.
         */
      
        /* Wait for the Z80's read to finish */
        while( (gpio_get_all64() & mreq_mask) == 0 );

        /* Z80 has picked up the byte, put data bus back to inputs */
        gpio_set_dir_in_masked64( GPIO_DBUS_BITMASK );

        if( initial_jp_destination == 0 )
        {
          gpio_set_dir( GPIO_ROMCS, GPIO_IN );
//  gpio_put( GPIO_BLIPPER2, 1 );
        }

      }

    }
    else if( (gpios & wr_mask) == 0 )
    {
      /*
       * Pick the value being written from the data bus and mirror it.
       * Writes to ROM will be written into the mirror, but they're not
       * accessed from there. The code above uses its own ROM image.
       * I did it this way for speed. There's not enough time here to
       * check if address is < 0x4000.
       */
      uint8_t data = (gpios & GPIO_DBUS_BITMASK) & 0xFF;
      put_zx_mirror_byte( address, data );

      /*
       * I don't attempt to wait for the MREQ to finish. The timing is too
       * tight, it's quicker just to let it go back to the top of the loop
       * and drop through here several times while the write completes.
       */
    }
    else
    {
      /*
       * MREQ is low, but it's not a read and it's not a write. It must be
       * a refresh. I don't attempt to wait for the MREQ to finish. The
       * timing is too tight, it's quicker just to let it go back to the
       * top of the loop and drop through here several times while the
       * refresh completes.
       */
    }
  } /* End infinite loop */
}


void init_rom_emulation( void )
{
  initial_jp_destination = 0;
  multicore_launch_core1( core1_rom_emulation );
}
