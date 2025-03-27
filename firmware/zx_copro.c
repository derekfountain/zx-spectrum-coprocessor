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

#include "rom_emulation.h"
#include "zx_mirror.h"
#include "z80_test_image.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "int_counter.pio.h"
#include "gpios.h"

/*
 * Flag set by a PIO routine which monitors the Spectrum's /INT line.
 * This is set true when it's unsafe to do a DMA into the Spectrum 
 * due to the risk of the Spectrum missing the next /INT
 */
volatile uint32_t interrupt_unsafe = 0;

static void test_blipper( void )
{
  gpio_put( GPIO_BLIPPER1, 1 );
  __asm volatile ("nop");
  __asm volatile ("nop");
  __asm volatile ("nop");
  __asm volatile ("nop");
  gpio_put( GPIO_BLIPPER1, 0 );
}

/* DMA queue */
typedef struct _DMA_QUEUE_ENTRY
{
  uint8_t  *src;
  uint32_t  zx_ram_location;
  uint32_t  length;
}
DMA_QUEUE_ENTRY;

/* Not sure if this queue idea is going anywhere yet */
static DMA_QUEUE_ENTRY dma_queue[1] = {0};
static void add_dma_to_queue( uint8_t *src, uint32_t zx_ram_location, uint32_t length )
{
  dma_queue[0].src             = src;
  dma_queue[0].zx_ram_location = zx_ram_location;
  dma_queue[0].length          = length;
}

void dma_memory_block( const uint8_t *src,    const uint32_t zx_ram_location,
                       const uint32_t length, const uint32_t int_protection ) 
{
  /*
   * If the Z80 is doing a read, potentially from ROM, wait for it to end.
   * The ROM emulation code wouldn't like to be interrupted.
   */
  if( using_rom_emulation() )
  {
    while( (gpio_get_all64() & RD_MREQ_MASK) == 0 );
  }

  /*
   * The Spectrum can't afford to miss an interrupt, so if one is approaching spin
   * while it passes
   */
  if( int_protection )
  {
    // Not sure about this bit
    if( interrupt_unsafe )
     gpio_put( GPIO_BLIPPER2, 1 );
    else
      gpio_put( GPIO_BLIPPER2, 0 );

  }

  /*
   * Empirical testing shows the DMA initialisaiton setup takes at most 8.5us.
   * That's with a 200MHz overclock, but I'm not sure that makes much difference
   */

  /* Assert bus request */
  gpio_put( GPIO_Z80_BUSREQ, 0 );

  /*
   * Spin waiting for Z80 to acknowledge. BUSACK goes active (low) on the 
   * rising edge of the clock - see fig8 in the Z80 manual
   */
  while( gpio_get( GPIO_Z80_BUSACK ) == 1 );

  /* OK, we have the Z80's bus */

  /* RD and IORQ lines are unused by this DMA process and stay inactive */
  gpio_set_dir( GPIO_Z80_RD,   GPIO_OUT ); gpio_put( GPIO_Z80_RD,   1 );
  gpio_set_dir( GPIO_Z80_IORQ, GPIO_OUT ); gpio_put( GPIO_Z80_IORQ, 1 );

  /* Reset Z80 address and data bus GPIOs as outputs */
  gpio_set_dir_out_masked( GPIO_ABUS_BITMASK );
  gpio_set_dir_out_masked( GPIO_DBUS_BITMASK );

  /* Set directions of control signals to outputs */
  gpio_set_dir( GPIO_Z80_MREQ, GPIO_OUT ); gpio_put( GPIO_Z80_MREQ, 1 );
  gpio_set_dir( GPIO_Z80_WR,   GPIO_OUT ); gpio_put( GPIO_Z80_WR,   1 );

  /* Blipper goes low while DMA process is active */
  gpio_put( GPIO_BLIPPER1, 0 );

  /*
   * At 200MHz this loop takes:
   *  16 bytes takes 10.5us
   *  32 bytes takes 15.5us
   *  64 bytes takes 25us
   * Crudely speaking, the 10us difference between the time it takes to DMA 32 bytes
   * and the time it takes to DMA 63 bytes is about 35 Z80 T-states, or about 9
   * of the Z80's fastest instructions. So for now I'm going to limit the on-the-fly
   * DMA transfer to 64 bytes and hard code the INT approaching signal. That seems a
   * fair tradeoff between functionality and not losing too much time waiting for an
   * approaching /INT.
   */

  for( uint32_t byte_counter=0; byte_counter < length; byte_counter++ )
  {
    /* Contents of this loop takes 435ns */
    
    /* Set up of buses takes ~150ns */

    /* Set address of ZX byte to write to */
    gpio_put_masked( GPIO_ABUS_BITMASK, (zx_ram_location+byte_counter)<<GPIO_ABUS_A0 );

    /* Assert memory request */
    gpio_put( GPIO_Z80_MREQ, 0 );

    /* Put value on the data bus */
    gpio_put_masked( GPIO_DBUS_BITMASK, *(src+byte_counter) );

    /*
     * Assert the write line to write it, the ULA responds to this and does
     * the write into the Spectrum's memory. i.e. the RAS/CAS stuff.
     */
    gpio_put( GPIO_Z80_WR, 0 );

    /*
     * The timing theory:
     * Spectrum RAM is rated 150ns which is 1.5e-07. RP2350 clock speed is
     * 200,000,000Hz (overclocked), so one clock cycle is 5ns. So that's 30
     * RP2350 clock cycles in one DRAM transaction time. NOP is T1, so it
     * takes one clock cycle, so 30 NOPs should guarantee a pause long
     * enough for the 4116s to respond.
     * 
     * I need to support both the original 4116s and the modern static RAM
     * memory module boards. Whichever is slower is the minimum speed I
     * run at.
     */
#define USING_STATIC_RAM_MODULE 0
#if USING_STATIC_RAM_MODULE
  /*
   * This was developed on a Spectrum containing a static RAM-based lower memory
   * module. I thought it would be faster than the 4116s, so should work with
   * fewer than 23 NOPs. Turns out it doesn't. Empirical testing shows at 150MHz
   * shows it needs 29, which is 1.93e-07 seconds, or about 190 nanooseconds.
   * 
   * Update: it turns out that sometimes 29 is too few and the DMA doesn't work.
   * This appears to be related to the Spectrum's temperature. The cooler the
   * machine is the longer this delay needs to be - but again, this is with a
   * static RAM module.
   */
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
#else
  /*
   * This was developed on a Spectrum containing the original 4116 RAM.
   * It turns out these need more time than the static memory module. As of
   * this writing I've given up trying to predict or interpret what's going
   * on with the timings. Empirical testing shows it needs 37 RP2350 cycles
   * at 150MHz. But I'm not sure that's right because when I overclocked to
   * 200MHz to get the ROM emulation working this continued to work.
   * At 200MHz each NOP takes 5ns; 37 of them take 185ns.
   */
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");
    __asm volatile ("nop");

    __asm volatile ("nop");
    __asm volatile ("nop");
#endif

    /* Mirror and buses reset takes ~100ns */

    /* Update local mirror to match the ZX RAM */
    put_zx_mirror_byte( zx_ram_location+byte_counter, *(src+byte_counter) );

    /* Remove write and memory request */
    gpio_put( GPIO_Z80_WR,   1 );
    gpio_put( GPIO_Z80_MREQ, 1 ); 
  }

  /*
   * Empirical testing shows this DMA teardown takes at most 1.6us.
   */

  /* DMA complete - put the address, data and control buses back to hi-Z */
  gpio_set_dir_in_masked( GPIO_ABUS_BITMASK );
  gpio_set_dir_in_masked( GPIO_DBUS_BITMASK );

  gpio_set_dir( GPIO_Z80_MREQ, GPIO_IN );
  gpio_set_dir( GPIO_Z80_WR,   GPIO_IN );
  gpio_set_dir( GPIO_Z80_IORQ, GPIO_IN );
  gpio_set_dir( GPIO_Z80_RD,   GPIO_IN );

  /* Release bus request */
  gpio_put( GPIO_Z80_BUSREQ, 1 );

  /* Indicate DMA process complete, inactive */
  gpio_put( GPIO_BLIPPER1, 1 );

  return;
}


/*
 * Test routine, called on alarm a few secs after the zx has booted up.
 * This isn't called if we're not using the test program
 */
int64_t copy_test_program( alarm_id_t id, void *user_data )
{
  add_dma_to_queue( get_z80_test_image_src(), get_z80_test_image_dest(), get_z80_test_image_length() );

  return 0;
}


void main( void )
{
  bi_decl(bi_program_description("ZX Spectrum Coprocessor Board Binary."));

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

  /*
   * Use PIO to time DMA missing the /INT signal. The INT signal is in the lower range
   * of GPIOs, but a test signal will need to be in the upper range. So set the base
   * to 16.
   */
  PIO pio                 = pio0;
  pio_set_gpio_base( pio, 16 );
  uint sm_int_counter     = pio_claim_unused_sm( pio, true );
  uint offset_int_counter = pio_add_program( pio, &int_counter_program );
  // 37 is output so i can see it's running, remove that in due course
  int_counter_program_init( pio, sm_int_counter, offset_int_counter, GPIO_Z80_INT, 37 );
  pio_sm_set_enabled( pio, sm_int_counter, true);

  /* Zero mirror memory */
  initialise_zx_mirror();

  /* Take over the ZX ROM */
  if( using_rom_emulation() )
  {
    /* We're emulating ROM, hold ROMCS permanently high and start the emulation running */
    gpio_init( GPIO_ROMCS ); gpio_set_dir( GPIO_ROMCS, GPIO_OUT ); gpio_put( GPIO_ROMCS, 1 );
    start_rom_emulation();

    /* Give the other core a moment to initialise */
    sleep_ms( 100 );
  }
  else
  {
    /* Not emulating ROM, let the Spectrum's ROM chip do its normal thing */
    gpio_init( GPIO_ROMCS ); gpio_set_dir( GPIO_ROMCS, GPIO_IN );
  }

  dma_queue[0].src = NULL;
  if( using_z80_test_image() )
  {
    /* The DMA stuff starts in a few seconds */
    add_alarm_in_ms( 3000, copy_test_program, NULL, 0 );
  }

  /* Let the Spectrum run */
  gpio_put( GPIO_RESET_Z80, 0 );

  /*
   * Set up the DMA channel which transfers the data value from the PIO
   * which counts from the /INT signal to the point where a Z80 DMA isn't
   * safe to perform due to the risk of losing the next /INT.
   * The value arrives in the RX FIFO, this DMA moves it into a local variable.
   * I'm not sure this is strictly necessary, I think I could just access the
   * FIFO entry directly with pio0_hw->rxf[0]. But this seems the right way to
   * do it.
   * The DREQ (data request) is set to make the DMA respond when the PIO
   * makes a value available on the RX FIFO.
   */
  int int_counter_dma_channel               = dma_claim_unused_channel( true );
  dma_channel_config int_counter_dma_config = dma_channel_get_default_config( int_counter_dma_channel );
  channel_config_set_transfer_data_size( &int_counter_dma_config, DMA_SIZE_32 );
  channel_config_set_read_increment( &int_counter_dma_config, false );
  channel_config_set_dreq( &int_counter_dma_config, DREQ_PIO0_RX0 );

  dma_channel_configure( int_counter_dma_channel,
                         &int_counter_dma_config,
                         &interrupt_unsafe,            // Write address, the local variable
                         &pio0_hw->rxf[0],             // Read address, the FIFO register
                         0xFFFFFFFF,                   // 0x0FFFFFFF plus 0xF0000000, ENDLESS, Spec 12.6.2.2.1
                         true                          // Start immediately
                       );

  /*
   * The IRQ handler stuff is nowhere near fast enough to handle this. The Z80's
   * write is finished long before the RP2350 even gets to call the handler function.
   * So, tight loop in the main core for now.
   */
gpio_put( GPIO_BLIPPER2, 1 );
  while( 1 )
  {
    /*
     * If there's something in the DMA queue, activate it while we're
     * between ROM reads. I think this might need to go onto the other
     * core at some point.
     */
    if( dma_queue[0].src != NULL )
    {
      dma_memory_block( dma_queue[0].src, dma_queue[0].zx_ram_location, dma_queue[0].length, true );

      dma_queue[0].src = NULL;

      if( using_z80_test_image() && using_rom_emulation() )
      {
        /*
         * If we're using the Z80 test code and emulating the ROM, reset the Z80
         * and set the flag which causes the JP 0x8000 at the start of the
         * emulated ROM.
         * This isn't really correct, the memory blocked DMAed above might not be
         * the test code, it might be something else. This all needs working out.
         */
        set_initial_jp( 0x8000 );
      
        gpio_put( GPIO_RESET_Z80, 1 );
        busy_wait_us_32( 100 );
        gpio_put( GPIO_RESET_Z80, 0 );
      }
    }
  }

}
 