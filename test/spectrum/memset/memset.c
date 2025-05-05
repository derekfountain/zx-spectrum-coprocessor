/*
 * zcc +zx -vn -startup=4 -clib=sdcc_iy memset.c ../common/cmd.c -o z80_image
 * xxd -i -c 16 z80_image_CODE.bin > ../../../firmware/z80_image.h
 */

#include <input.h>
#include <input/input_zx.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <z80.h>
#include <intrinsic.h>
#include <arch/zx.h>

#include "../common/cmd.h"

void main(void)
{
  ioctl(1, IOCTL_OTERM_PAUSE, 0);

  MEMSET_INIT(memset_cmd);

  uint32_t check_counter = 0;
  uint8_t  c_value = 0;
  srand( 1000 );
  while(1)
  {
//  uint16_t length = (rand() & 0x07FE)+1;  // 2K
    uint16_t length = (rand() & 0x00FF)+1;  // 256 bytes

//  uint16_t dest_addr = 0x4000;            // Lower RAM, contended, screen
//  uint16_t dest_addr = 0x5A00;            // Lower RAM, contended, attributes
//  uint16_t dest_addr = 0x5B00;            // Lower RAM, contended, printer buffer, max length of 256 bytes
                                            // Doesn't work, not sure why
    uint16_t dest_addr = 0xC000;            // Upper RAM, not contended, above code, below stack

    CMD_CLEAR_STATUS(memset_cmd);
    CMD_CLEAR_ERROR(memset_cmd);

    MEMSET_SET_DEST(memset_cmd,dest_addr);
    MEMSET_SET_C(memset_cmd,c_value);
    MEMSET_SET_LENGTH(memset_cmd,length);

#if 0
    printf("\x16\x01\x01" "Check %ld, %02X times %d bytes?\n", check_counter, c_value, length);

    printf("%02X %02X %02X %02X  %02X %02X  %02X  %02X %02X\n",
	   memset_cmd[0],memset_cmd[1],memset_cmd[2],memset_cmd[3],
	   memset_cmd[4],memset_cmd[5],
	   memset_cmd[6],
	   memset_cmd[7],memset_cmd[8]);
#endif

    CMD_TRIGGER_IMMEDIATE_CMD( memset_cmd );
static uint8_t border = 0;
    CMD_SPIN_ON_STATUS( memset_cmd )
    {
      zx_border(border++);
    };

    if( CMD_IS_COPRO_ERROR( memset_cmd ) )
    {
      printf("Error is %d\n", CMD_QUERY_COPRO_ERROR( memset_cmd ) );
      while(1);
    }

    uint16_t counter;
    for( counter=0; counter < length; counter++ )
    {
      uint8_t *check_addr = (uint8_t*)(dest_addr+counter);
      if( *check_addr != c_value )
      {
         printf("Fail at %p, expected %02X, found %02X\n",
	              check_addr, c_value, *check_addr);
	       while(1);
      }
    }
#if 0
    printf("Check %ld, %02X times %d bytes OK!\n", ++check_counter, c_value++, length);
#endif
    z80_delay_ms(1000);
  }
}
