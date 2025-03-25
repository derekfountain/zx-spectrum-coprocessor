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

/*
 * Timings here are very tight. At 200MHz a single RP2350 instruction takes 5ns. 
 * Each cycle of the 3.5MHz Z80 is about 286ns, so there's time for 57 RP2350
 * instructions to execute in one Z80 cycle.
 * The Z80's M1 instruction fetch has MREQ low for one-and-a-half Z80 cycles. 
 * That's 85 RP2350 cycles to spot the MREQ, read the address bus, find the
 * data value and get it on the data bus.
 * I don't want to use a ridiculous overclock, 200MHz is fine, maybe a bit
 * faster if I really have to.
 */
#define OVERCLOCK 200000

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
     * A refresh has MREQ low for one Z80 clock cycle, which is 285ns. I 
     * can ignore those.
     */

    /* Spin, waiting for a memory request. (Approx 90ns to 100ns)  */
    while( ((gpios = gpio_get_all64()) & mreq_mask) );

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

#if 1
gpio_put( GPIO_BLIPPER1, 0 );
        if( using_z80_test_image() )
        {

        }
gpio_put( GPIO_BLIPPER1, 1 );
#endif

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
    else if( (gpios & wr_mask) == 0 )
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
    else
    {
#if 0
      /*
       * I don't understand why this doesn't work. Leaving it in causes the
       * Spectrum to crash. In fact, it doesn't actually start up. If I leave
       * this out I see a series of blips from the main filter loop at the
       * top as it repeatedly decides MREQ is active and the code comes here
       * and drops out again. That seems harmless, but it's not really right.
       * If I leave this code in, it waits for the MREQ to end before going
       * back to the top. That should also work, and it looks the same on the
       * scope, but the ZX then won't start.
       */  
      /*
       * MREQ is low, but it's not a read and it's not a write. It must be
       * a refresh. Let it finish, then go back for the next one.
       * On a refresh MREQ stays low for one Z80 cycle, so around 285ns.
       */
      gpio_put( GPIO_BLIPPER2, 0 );
      while( (gpio_get_all64() & mreq_mask) == 0 );
      gpio_put( GPIO_BLIPPER2, 1 );
#endif
    }
  } /* End infinite loop */
}


void start_rom_emulation( void )
{
  multicore_launch_core1( core1_rom_emulation );
}
