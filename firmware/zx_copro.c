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

/*
 * ZX Spectrum Coprocessor.
 *
 * cmake -DCMAKE_BUILD_TYPE=Debug ..
 * make -j10
 * sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg -c "program ./zx_copro.elf verify reset exit"
 * sudo openocd -f interface/cmsis-dap.cfg -f target/rp2350.cfg
 * gdb-multiarch ./zx_copro.elf
 *  target remote localhost:3333
 *  load
 *  monitor reset init
 *  continue
 */

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/clocks.h"
#include "pico/multicore.h"

#include "dma_engine.h"
#include "rom_emulation.h"
#include "zx_mirror.h"
#include "z80_test_image.h"
#include "cmd_immediate.h"

#include "gpios.h"

static void test_blipper( void )
{
  gpio_put( GPIO_BLIPPER1, 1 );
  __asm volatile ("nop");
  __asm volatile ("nop");
  __asm volatile ("nop");
  __asm volatile ("nop");
  gpio_put( GPIO_BLIPPER1, 0 );
}


/*
 * Test routine, called on alarm a few secs after the zx has booted up.
 * This isn't called if we're not using the test program
 */
static int64_t load_test_program( alarm_id_t id, void *user_data )
{
  // CONF alarm is goin goff
  z80_test_image_set_pending();

  return 0;
}

#define OVERCLOCK 200000

void main( void )
{
  bi_decl(bi_program_description("ZX Spectrum Coprocessor Board Binary."));

#ifdef OVERCLOCK
  set_sys_clock_khz( OVERCLOCK, 1 );
#endif

  /* All interrupts off except the timers */
//  irq_set_mask_enabled( 0xFFFFFFFF, 0 );
//  irq_set_mask_enabled( 0x0000000F, 1 );

  /*
   * This is the Z80's /RESET signal, it's an input to this code.
   *
   * The internal pull up is required, but I don't know why. Without it the /RESET
   * line is held low and the Spectrum doesn't start up. With it the /RESET line
   * rises normally and the Spectrum starts as usual. Cutting the track from 
   * /RESET to the GPIO_Z80_RESET GPIO makes the problem go away, so it appears
   * the RP2350 GPIO is where the problem is, it's holding the line low. But I
   * don't really know. I discovered by accident that setting the RP2350's internal
   * pull up makes the problem go away. OK, I'll take it.
   */
  gpio_init( GPIO_Z80_RESET );  gpio_set_dir( GPIO_Z80_RESET, GPIO_IN ); gpio_pull_up( GPIO_Z80_RESET );

  /* GPIO_RESET_Z80 is the output which drives a reset to the Z80. Hold the Z80 in reset until everything is set up */
  gpio_init( GPIO_RESET_Z80 ); gpio_set_dir( GPIO_RESET_Z80, GPIO_OUT ); gpio_put( GPIO_RESET_Z80, 1 );
 
  /* Blippers, for the scope */
  gpio_init( GPIO_BLIPPER1 ); gpio_set_dir( GPIO_BLIPPER1, GPIO_OUT ); gpio_put( GPIO_BLIPPER1, 1 );
  gpio_init( GPIO_BLIPPER2 ); gpio_set_dir( GPIO_BLIPPER2, GPIO_OUT ); gpio_put( GPIO_BLIPPER2, 1 );
gpio_init( 42 ); gpio_set_dir( 42, GPIO_OUT ); gpio_put( 42, 1 );
 
  /* Set up Z80 control bus */  
  gpio_init( GPIO_Z80_CLK  );   gpio_set_dir( GPIO_Z80_CLK,  GPIO_IN );
  gpio_init( GPIO_Z80_RD   );   gpio_set_dir( GPIO_Z80_RD,   GPIO_IN );
  gpio_init( GPIO_Z80_WR   );   gpio_set_dir( GPIO_Z80_WR,   GPIO_IN );
  gpio_init( GPIO_Z80_MREQ );   gpio_set_dir( GPIO_Z80_MREQ, GPIO_IN );
  gpio_init( GPIO_Z80_IORQ );   gpio_set_dir( GPIO_Z80_IORQ, GPIO_IN );
  gpio_init( GPIO_Z80_INT  );   gpio_set_dir( GPIO_Z80_INT,  GPIO_IN );
  gpio_init( GPIO_Z80_WAIT );   gpio_set_dir( GPIO_Z80_WAIT, GPIO_IN );
 
  gpio_init( GPIO_Z80_BUSREQ ); gpio_set_dir( GPIO_Z80_BUSREQ, GPIO_OUT ); gpio_put( GPIO_Z80_BUSREQ, 1 );
  gpio_init( GPIO_Z80_BUSACK ); gpio_set_dir( GPIO_Z80_BUSACK, GPIO_IN  );

  /* Initialise Z80 data bus GPIOs as inputs */
  gpio_init_mask( GPIO_DBUS_BITMASK );  gpio_set_dir_in_masked( GPIO_DBUS_BITMASK );

  /* Initialise Z80 address bus GPIOs as inputs */
  gpio_init_mask( GPIO_ABUS_BITMASK );  gpio_set_dir_in_masked( GPIO_ABUS_BITMASK );

  /* Let the Spectrum ROM do the ROM until we need to interfere */
  gpio_init( GPIO_ROMCS ); gpio_set_dir( GPIO_ROMCS, GPIO_IN );

  /* Initialise the interrupt protection PIO and DMA system */
  init_dma_engine();
  init_interrupt_protection();

  /* Zero mirror memory */
  initialise_zx_mirror();

  /* Take over the ZX ROM */
  if( using_rom_emulation() )
  {
    /* We're emulating ROM, hold ROMCS permanently high and start the emulation running */
    gpio_init( GPIO_ROMCS ); gpio_set_dir( GPIO_ROMCS, GPIO_OUT ); gpio_put( GPIO_ROMCS, 1 );
    start_rom_emulation( FULL_ROM_EMULATION );
  }
  else
  {
    /* Not emulating ROM, let the Spectrum's ROM chip do its normal thing */
    gpio_init( GPIO_ROMCS ); gpio_set_dir( GPIO_ROMCS, GPIO_IN );
    start_rom_emulation( RAM_MIRROR_ONLY );
  }

  /* Give the other core a moment to initialise */
  sleep_ms( 100 );

  /* Give the other core a moment to initialise */
  sleep_ms( 100 );

  if( using_z80_test_image() )
  {
    init_z80_test_image();

    /* The DMA stuff starts in a few seconds */
    add_alarm_in_ms( 3000, load_test_program, NULL, 0 );
  }

  /* Let the Spectrum run */
  gpio_put( GPIO_RESET_Z80, 0 );

  /*
   * The IRQ handler stuff is nowhere near fast enough to handle this. The Z80's
   * write is finished long before the RP2350 even gets to call the handler function.
   * So, tight loop in the main core for now.
   */
  while( 1 )
  {
    const uint64_t immediate_cmd_trigger_mask       = IMMEDIATE_CMD_TRIGGER_MASK;
    const uint64_t immediate_cmd_trigger_pattern_hi = IMMEDIATE_CMD_TRIGGER_PATTERN_HI;
    const uint64_t immediate_cmd_trigger_pattern_lo = IMMEDIATE_CMD_TRIGGER_PATTERN_LO;

    // FIXME PIO would be a lot more efficient than this
    //
    uint64_t gpios;
    uint8_t  data_bus;
    if( ((gpios=gpio_get_all64()) & immediate_cmd_trigger_mask) == immediate_cmd_trigger_pattern_lo )
    {
      /* Z80 is writing to the immediate command register low byte */
      cache_immediate_cmd_address_lo( (gpios>>GPIO_DBUS_D0) & GPIO_DBUS_BITMASK );
    }
    else if( (gpios & immediate_cmd_trigger_mask) == immediate_cmd_trigger_pattern_hi )
    {
      /* Z80 is writing to the immediate command register high byte */
      cache_immediate_cmd_address_hi( (gpios>>GPIO_DBUS_D0) & GPIO_DBUS_BITMASK );
    }


    if( is_immediate_cmd_pending() )
    {
      service_immediate_cmd();
    }


    /*
     * If there's something in the DMA queue, activate it.
     */
    if( is_dma_queue_full() )
    {
      activate_dma_queue_entry();
    }
  }

}
 