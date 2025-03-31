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
#include "hardware/timer.h"

#include "dma_engine.h"

#include "gpios.h"
#include "rom_emulation.h"
#include "zx_mirror.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "int_unsafe.pio.h"

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
void add_dma_to_queue( uint8_t *src, uint32_t zx_ram_location, uint32_t length )
{
  dma_queue[0].src             = src;
  dma_queue[0].zx_ram_location = zx_ram_location;
  dma_queue[0].length          = length;
}

uint32_t is_dma_queue_full( void )
{
  return (dma_queue[0].src != NULL);
}

#include "z80_test_image.h"
void activate_dma_queue_entry( void )
{
  if( dma_queue[0].src != NULL )
  {
    dma_memory_block( dma_queue[0].src, dma_queue[0].zx_ram_location, dma_queue[0].length, true );

    dma_queue[0].src = NULL;

    // FIXME This is wrong, test image shouldn't be exposed here
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

/*
 * Flag set by a PIO routine which monitors the Spectrum's /INT line.
 * This is set true when it's unsafe to do a DMA into the Spectrum 
 * due to the risk of the Spectrum missing the next /INT
 */
static volatile uint32_t interrupt_unsafe = 0;

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
   * The Spectrum can't afford to miss an interrupt, so if one is approaching, spin
   * while it passes
   */
  if( int_protection )
  {
    /*
     * A combination of the int_unsafe PIO program and
     * RP2350 DMA keep this global variable updated
     */
    while( interrupt_unsafe );
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

void init_interrupt_protection( void )
{
  /*
   * Use PIO to time DMA missing the /INT signal. The INT signal is in the lower range
   * of GPIOs, but a test signal will need to be in the upper range. So set the base
   * to 16.
   */
  PIO pio                = pio0;
  pio_set_gpio_base( pio, 16 );
  uint sm_int_unsafe     = pio_claim_unused_sm( pio, true );
  uint offset_int_unsafe = pio_add_program( pio, &int_unsafe_program );
  int_unsafe_program_init( pio, sm_int_unsafe, offset_int_unsafe, GPIO_Z80_INT, GPIO_INT_UNSAFE );
  pio_sm_set_enabled( pio, sm_int_unsafe, true);

    /*
   * Set up the DMA channel which transfers a flag value from the PIO which indicates
   * it's unsafe for a Z80 DMA to proceed due to the risk of the Z80 missing the next
   * interrupt. The value is a 0 if there's no risk, and 1 if there is risk. It arrives
   * in the RX FIFO, this DMA moves it into a local variable.
   * I'm not sure this is strictly necessary, I think I could just access the flag as the
   * FIFO entry directly with pio0_hw->rxf[0]. But this seems the right way to do it.
   * The DREQ (data request) is set to make the DMA respond when the PIO
   * makes a value available on the RX FIFO.
   */
  int int_unsafe_dma_channel               = dma_claim_unused_channel( true );
  dma_channel_config int_unsafe_dma_config = dma_channel_get_default_config( int_unsafe_dma_channel );
  channel_config_set_transfer_data_size( &int_unsafe_dma_config, DMA_SIZE_32 );
  channel_config_set_read_increment( &int_unsafe_dma_config, false );
  channel_config_set_dreq( &int_unsafe_dma_config, DREQ_PIO0_RX0 );

  dma_channel_configure( int_unsafe_dma_channel,
                         &int_unsafe_dma_config,
                         &interrupt_unsafe,            // Write address, the local variable
                         &pio0_hw->rxf[0],             // Read address, the FIFO register
                         0xFFFFFFFF,                   // 0x0FFFFFFF plus 0xF0000000, ENDLESS, Spec 12.6.2.2.1
                         true                          // Start immediately
                       );

  /*
   * Set up the DMA channel which provides the countdown value for the PIO which handles
   * the "interrupt unsafe" timer. This DMA just sends the same 32 bit value to the PIO
   * each time the PIO asks for it.
   */
  int int_interval_dma_channel               = dma_claim_unused_channel( true );
  dma_channel_config int_interval_dma_config = dma_channel_get_default_config( int_interval_dma_channel );
  channel_config_set_transfer_data_size( &int_interval_dma_config, DMA_SIZE_32 );
  channel_config_set_write_increment( &int_interval_dma_config, false );
  channel_config_set_read_increment( &int_interval_dma_config, false );
  channel_config_set_dreq( &int_interval_dma_config, DREQ_PIO0_TX0 );

  /*
   * Interval countdown. The ULA generates /INTs at 19.97ms intervals (close to 20ms
   * but not quite). That's 19,970,000ns, at 5ns per cycle (200MHz) that's 3,994,000
   * cycles from /INT to /INT. I want a 30us pause to come in just before the /INT.
   * That's 30,000ns, or 6,000 cycles. So I want the countdown to run from /INT to 6,000
   * cycles before the next /INT, which is 3,994,000-6,000. But that gives a pre-INT
   * pause of about 38us, not 30us. I don't know why.
   * I discovered empirically that a countdown of 3,994,000-4,000 gives a pre-INT pause
   * of about 29us, which will do the job. My best guess is that the 19.97ms time between
   * INTs, which came from Smith's ULA book is not quite as precise as I'm assuming it
   * is. Or maybe the crystal in the 40 year old Spectrum I'm testing with has wandered
   * a bit. I could reimplement with some sort of dynamic measuring, but I think this
   * is good enough.
   */
  const uint32_t interval_countdown = 3994000-4000;
  dma_channel_configure( int_interval_dma_channel,
                         &int_interval_dma_config,
                         &pio0_hw->txf[0],             // Write address, PIO's FIFO
                         &interval_countdown,          // Read address, value to send
                         0xF0000001,                   // 1 transfer, plus 0xF0000000, ENDLESS, Spec 12.6.2.2.1
                         true                          // Start immediately
                        );

  return;
}

void init_dma_engine( void )
{
  dma_queue[0].src = NULL;
  return;
}