/*
 * zcc +zx -vn -startup=0 -clib=sdcc_iy memset.c -o z80_image
 * xxd -i -c 16 z80_image_CODE.bin > ../../../firmware/z80_image.h
 */

#include <input.h>
#include <input/input_zx.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <z80.h>

void main(void)
{
  /* Compiler gives internal error if this isn't static */
  static uint8_t memset_cmd[] =
  {
    128, 0, 0,                 // CMD type, result and error

    0x00, 0x00,                // zx_addr to set memory at
    0x00,                      // c, constant value to set
    0, 0,                      // n, 16 bit count to set
  };

  ioctl(1, IOCTL_OTERM_PAUSE, 0);

  uint32_t check_counter = 0;
  uint8_t  c_value = 0;
  srand( 1000 );
  while(1)
  {
    uint8_t  length = (rand() & 0x1F)+1;

//    uint16_t dest_addr = 0x5B00;            // Lower RAM, contended, printer buffer
    uint16_t dest_addr = 0xC000;            // Upper RAM, not contended, above code, below stack

    memset_cmd[1] = 0;    // Response
    memset_cmd[2] = 0;    // Error

    memset_cmd[3] = dest_addr & 0xFF;
    memset_cmd[4] = (dest_addr>>8) & 0xFF;

    memset_cmd[5] = c_value;

    memset_cmd[6] = length;
    memset_cmd[7] = 0;

    uint16_t memset_cmd_addr = (uint16_t)(&memset_cmd[0]);

    printf("Check %ld, %02X times %d bytes?\n", check_counter, c_value, length);

    printf("%02X %02X %02X  %02X %02X  %02X  %02X %02X\n",
	   memset_cmd[0],memset_cmd[1],memset_cmd[2],
	   memset_cmd[3],memset_cmd[4],
	   memset_cmd[5],
	   memset_cmd[6],memset_cmd[7]);

    *((uint8_t*)14446) = (uint8_t)(memset_cmd_addr & 0xFF);
    *((uint8_t*)14447) = (uint8_t)((memset_cmd_addr >> 8) & 0xFF);
    
    while(1)
    {
      if( memset_cmd[1] == 0 && memset_cmd[2] == 0 )
      {
	printf("+\n");  // Spin on response going to 1
      }
      else
      {
	if( memset_cmd[1] != 0 )
	{
	  printf("Response is %d\n", memset_cmd[1]);
	  break;
	}

	if( memset_cmd[2] != 0 )
	{
	  printf("Error is %d\n", memset_cmd[2]);
	  while(1);
	}

      }
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
