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

#include <string.h>

#include "rom.h"
#include "gpios.h"

#define USE_TEST_IMAGE
#ifdef USE_TEST_IMAGE
/*
 * A test program, z80 machine code, expected to be ORGed at 0x8000.
 * Use xxd to create the header file, for example:
 *   > zcc +zx -vn -startup=5 -clib=sdcc_iy z80_image.c -o z80_image
 *   > xxd -i -c 16 z80_image_CODE.bin > ../../../firmware/z80_image.h
 */
#include "z80_image.h"
#endif

/* Using this messes up the DMA timings */
//#define OVERCLOCK 270000

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
 * Local copy of the ZX memory, 64K
 */
#define ZX_MEMORY_SIZE           ((uint32_t)65536)
static uint8_t zx_memory_mirror[ZX_MEMORY_SIZE];

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

/* Need to gate this so DMA doesn't happen if we're supplying/mirroring RAM? */
static uint32_t dma_gate = 0;
void dma_memory_block( uint8_t *src, uint32_t zx_ram_location, uint32_t length ) 
{
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

  /* Blipper goes high while DMA process is active */
  gpio_put( GPIO_BLIPPER1, 1 );

  const uint32_t write_address = zx_ram_location;

  uint32_t byte_counter;
  for( byte_counter=0; byte_counter < length; byte_counter++ )
  {
    /* Set address of ZX byte to write to */
    gpio_put_masked( GPIO_ABUS_BITMASK, (write_address+byte_counter)<<GPIO_ABUS_A0 );

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
     * Spectrum RAM is rated 150ns which is 1.5e-07. RP2350 clock speed is
     * 150,000,000Hz, so one clock cycle is 6.66666666667e-09. So that's 22.5
     * RP2350 clock cycles in one DRAM transaction time. NOP is T1, so it
     * takes one clock cycle, so 23 NOPs should guarantee a pause long
     * enough for the 4116s to respond.
     */
#define USING_STATIC_RAM_MODULE 0
#if USING_STATIC_RAM_MODULE
  /*
   * This was developed on a Spectrum containing a static RAM-based lower memory
   * module. I thought it would be faster than the 4116s, so should work with
   * fewer than 23 NOPs. Turns out it doesn't. Empirical testing shows it needs 29,
   * which is 1.93e-07 seconds, or about 19 microseconds. It don't know why.
   * 
   * Update: it turns out that sometimes 29 is too few and the DMA doesn't work.
   * This appears to be related to the Spectrum's temperature. The cooler the
   * machine is the longer this delay needs to be - but again, this is with a
   * static RAM module.
   * 
   * I've currently got this at 35/150,000,000ths of a second. This seems
   * reliable. A transfer of 6,912 bytes at this speed takes 2.37ms.
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
   * on with the timings. Empirical testing shows it needs 37 RP2350 cycles,
   * which is 24.6 microseconds at 150MHz.
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

  /* Update local mirror to match the ZX RAM */
  zx_memory_mirror[write_address+byte_counter] = *(src+byte_counter);

    /* Remove write and memory request */
    gpio_put( GPIO_Z80_WR,   1 );
    gpio_put( GPIO_Z80_MREQ, 1 ); 
  }

  /* DMA complete - put the address, data and control buses back to hi-Z */
  gpio_set_dir_in_masked( GPIO_ABUS_BITMASK );
  gpio_set_dir_in_masked( GPIO_DBUS_BITMASK );

  gpio_set_dir( GPIO_Z80_MREQ, GPIO_IN );
  gpio_set_dir( GPIO_Z80_WR,   GPIO_IN );
  gpio_set_dir( GPIO_Z80_IORQ, GPIO_IN );
  gpio_set_dir( GPIO_Z80_RD,   GPIO_IN );

  /* Release bus request */
  gpio_put( GPIO_Z80_BUSREQ, 1 );

  /* Indicate DMA process complete */
  gpio_put( GPIO_BLIPPER1, 0 );

  return;
}


/* Test routine, called on alarm a few secs after the zx has booted up */
int64_t copy_test_program( alarm_id_t id, void *user_data )
{
#ifdef USE_TEST_IMAGE
  add_dma_to_queue( z80_image_CODE_bin, 0x8000, z80_image_CODE_bin_len );
#endif

  return 0;
}


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
  gpio_init( GPIO_BLIPPER1 ); gpio_set_dir( GPIO_BLIPPER1, GPIO_OUT ); gpio_put( GPIO_BLIPPER1, 0 );
  gpio_init( GPIO_BLIPPER2 ); gpio_set_dir( GPIO_BLIPPER2, GPIO_OUT ); gpio_put( GPIO_BLIPPER2, 0 );
 
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

  /* Zero mirror memory */
  for( uint32_t i=0; i < ZX_MEMORY_SIZE; i++ )
    zx_memory_mirror[i]=0;

  /* Copy the original ROM image into the Z80 memory mirror */
  memcpy( zx_memory_mirror, _48_original_rom, _48_original_rom_len );

  /* Take over the ZX ROM */
#define EMULATE_ROM 0
#if EMULATE_ROM  
  /* We're emulating ROM, hold ROMCS permanently high */
  gpio_init( GPIO_ROMCS ); gpio_set_dir( GPIO_ROMCS, GPIO_OUT ); gpio_put( GPIO_ROMCS, 1 );
#else
  /* Not emulating ROM, let the Spectrum's ROM chip do its normal thing */
  gpio_init( GPIO_ROMCS ); gpio_set_dir( GPIO_ROMCS, GPIO_IN ); // gpio_pull_up( GPIO_ROMCS );
#endif

#ifdef USE_TEST_IMAGE
  dma_queue[0].src = NULL;

  /* The DMA stuff starts in a few seconds */
  add_alarm_in_ms( 3000, copy_test_program, NULL, 0 );

#define RESET_INSTRUCTION_COUNTER 200
  uint32_t start_test_image = 0;
#endif

  /* Let the Spectrum run */
  gpio_put( GPIO_RESET_Z80, 0 );

  /*
   * The IRQ handler stuff is nowhere near fast enough to handle this. The Z80's
   * write is finished long before the RP2350 even gets to call the handler function.
   * So, tight loop in the main core for now.
   */
  while( 1 )
  {
    register uint64_t gpios = gpio_get_all64();

    /* A memory write is when mem-request and write are both low */
    const uint64_t WR_MREQ_MASK = (0x01 << GPIO_Z80_MREQ) | (0x01 << GPIO_Z80_WR);

    /* A memory read is when mem-request and read are both low */
    const uint64_t RD_MREQ_MASK = (0x01 << GPIO_Z80_MREQ) | (0x01 << GPIO_Z80_RD);

    /* A memory read is when mem-request and read are both low */
    const uint64_t Z80_IN_RESET_MASK = ((uint64_t)0x01 << GPIO_Z80_RESET);

    /* Pick up the address being accessed */
    register uint64_t address = (gpios & GPIO_ABUS_BITMASK) >> GPIO_ABUS_A0;

    /* Note this will trigger if another core is doing DMA */
    if( (gpios & WR_MREQ_MASK) == 0 )
    {
      /* Ignore writes to ROM */
      if( address >= 0x4000 )
      {
        /* Pick the value being written from the data bus and mirror it */
        uint8_t data = (gpios & GPIO_DBUS_BITMASK) & 0xFF;
        zx_memory_mirror[address] = data;
      }

      /* Wait for the Z80 write to finish */
      while( (gpio_get_all64() & WR_MREQ_MASK) == 0 );
    }
    else if( (gpios & RD_MREQ_MASK) == 0 )
    {
#if EMULATE_ROM    
      /* Ignore reads from anywhere other than ROM, the Spectrum still reads its own RAM */
      if( address <= 0x3FFF )
      {
        /* Pick up ROM byte from local mirror */
        uint8_t data = zx_memory_mirror[address];

#ifdef USE_TEST_IMAGE
        /* Inject JP 0x8000 into bytes 0, 1 and 2 to force a jump to the test code */
        if( start_test_image )
        {
          if(      address == 0 ) data = 0xc3;
          else if( address == 1 ) data = 0x00;
          else if( address == 2 ) data = 0x80;
        }
#endif

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
#ifdef USE_TEST_IMAGE
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
        if( start_test_image )
        {
          /* Not sure what to do here. As some point I want to say start_test_image=0; */
        }
#endif
        /*
         * It's a read from RAM, the Spectrum's RAM chips will field it.
         * Just wait for the read to finish we don't loop continuously
         * while this read is on the Z80 control bus
         */
        while( (gpio_get_all64() & RD_MREQ_MASK) == 0 );        
      }
#else
      /*
       * We're not emulating ROM, and the Spectrum's RAM chips will field
       * all RAM accesses, so there's nothing to do.
       * Just wait for the read to finish we don't loop continuously
       * while this read is on the Z80 control bus
       */
      while( (gpio_get_all64() & RD_MREQ_MASK) == 0 );        
#endif        
    }

    /*
     * If there's something in the DMA queue, activate it while we're
     * between ROM reads. I think this might need to go onto the other
     * core at some point.
     */
    if( dma_queue[0].src != NULL )
    {
      dma_memory_block( dma_queue[0].src, dma_queue[0].zx_ram_location, dma_queue[0].length );
      dma_queue[0].src = NULL;

#ifdef USE_TEST_IMAGE
#if EMULATE_ROM
      /*
       * If we're using the Z80 test code and emulating the ROM, reset the Z80
       * and set the flag which causes the JP 0x8000 at the start of the
       * emulated ROM.
       * This isn't really correct, the memory blocked DMAed above might not be
       * the test code, it might be something else. This all needs working out.
       */
      start_test_image = 1;
      
      gpio_put( GPIO_RESET_Z80, 1 );
      busy_wait_us_32( 100 );
      gpio_put( GPIO_RESET_Z80, 0 );
#endif
#endif
    }

  }

}
 