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

#include "rom_emulation.h"
#include "zx_mirror.h"
#include "z80_test_image.h"

#if EMULATE_ROM

#include "gpios.h"

static uint16_t initial_jp_destination;

void set_initial_jp( uint16_t dest )
{
  initial_jp_destination = dest;
}

void reset_initial_jp( void )
{
  initial_jp_destination = 0;
}

static void core1_rom_emulation( void )
{
  while( 1 )
  {
    register uint64_t gpios = gpio_get_all64();

    /* Pick up the address being accessed */
    register uint64_t address = (gpios & GPIO_ABUS_BITMASK) >> GPIO_ABUS_A0;

    if( (gpios & RD_MREQ_MASK) == 0 )
    {
      /* Ignore reads from anywhere other than ROM, the Spectrum still reads its own RAM */
      if( address <= 0x3FFF )
      {
        /* Pick up ROM byte from local mirror */
        uint8_t data = get_zx_mirror_byte( address );

        if( using_z80_test_image() )
        {
          /* Inject JP 0x8000 into bytes 0, 1 and 2 to force a jump to the test code */
          if( initial_jp_destination != 0 )
          {
            if(      address == 0 ) data = 0xc3;
            else if( address == 1 ) data = (uint8_t)(initial_jp_destination & 0xFF);
            else if( address == 2 ) data = (uint8_t)(((initial_jp_destination & 0xFF00) >> 8) & 0xFF);
          }
        }

        /* Set the data bus to outputs */
        gpio_set_dir_out_masked( GPIO_DBUS_BITMASK );

        /* Write the value out to the Z80 */
        gpio_put_masked64( GPIO_DBUS_BITMASK, (data & 0xFF) << GPIO_DBUS_D0 );
      
        /* Wait for the Z80's read to finish */
        while( (gpio_get_all64() & RD_MREQ_MASK) == 0 );

        /* Z80 has picked up the byte, put data bus back to inputs */
        gpio_set_dir_in_masked( GPIO_DBUS_BITMASK );
      }
      else
      {
        /*
         * Bit of a problem here. If the test image is in use, it will have
         * been DMAed to 0x8000 and the Z80 reset. The first 3 bytes of the
         * ROM will have been tweaked to make a JP 0x8000 to start the test
         * program. Now I want to remove the tweaked JP instruction.
         * The issue is that when the Spectrum resets it actually resets
         * several times. It jumps to 0x0000, runs a few instructions, then
         * jumps back to 0x0000 and does it again. It's arbitrary, but I think
         * it's caused by jitter on the reset line as C27 charges up.
         * But that leaves the question of how I know when it's safe to put
         * the original ROM bytes back. If I just look for address 0x8000 on
         * the address bus, that works, but if there's then another jittery
         * reset and the JP 0x8000 has been removed the normal boot will
         * happen. So how do I know when the jitters have finished and the
         * test code is really running?
         * The answer might be to count instructions executed or something,
         * but for now I'm just going to ignore the problem and leave the
         * JP 0x8000 in place. If this proves to be a problem I'll come back
         * to it. It's only a test convenience after all.
         */
        if( using_z80_test_image() && initial_jp_destination != 0 )
        {
          /* Not sure what to do here. As some point I want to say initial_jp_destination=0; */
        }

        /*
         * It's a read from RAM, the Spectrum's RAM chips will field it.
         * Just wait for the read to finish we don't loop continuously
         * while this read is on the Z80 control bus
         */
        while( (gpio_get_all64() & RD_MREQ_MASK) == 0 );        
      }
    }
    else
    {
      /* Whatever state is on the Z80 control bus, it's not a read. Ignore. */
    }
  }
}


void start_rom_emulation( void )
{
  multicore_launch_core1( core1_rom_emulation );
}

#endif
