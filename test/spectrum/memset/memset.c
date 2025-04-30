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
    uint16_t length = (rand() & 0x7FE)+1;

//  uint16_t dest_addr = 0x5BC0;            // Lower RAM, contended, printer buffer, doesn't work
    uint16_t dest_addr = 0xC000;            // Upper RAM, not contended, above code, below stack

    CMD_CLEAR_STATUS(memset_cmd);
    CMD_CLEAR_ERROR(memset_cmd);

    MEMSET_SET_DEST(memset_cmd,dest_addr);
    MEMSET_SET_C(memset_cmd,c_value);
    MEMSET_SET_LENGTH(memset_cmd,length);

    printf("Check %ld, %02X times %d bytes?\n", check_counter, c_value, length);

    printf("%02X %02X %02X %02X  %02X %02X  %02X  %02X %02X\n",
	   memset_cmd[0],memset_cmd[1],memset_cmd[2],memset_cmd[3],
	   memset_cmd[4],memset_cmd[5],
	   memset_cmd[6],
	   memset_cmd[7],memset_cmd[8]);

    CMD_TRIGGER_IMMEDIATE_CMD( memset_cmd );
    CMD_SPIN_ON_STATUS( memset_cmd );

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

    printf("Check %ld, %02X times %d bytes OK!\n", ++check_counter, c_value++, length);
  }
}
