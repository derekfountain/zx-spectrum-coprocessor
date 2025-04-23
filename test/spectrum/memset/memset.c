/*
 * zcc +zx -vn -startup=1 -clib=sdcc_iy memset.c -o z80_image
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
  uint8_t memset_cmd[] =
  {
    128, 0, 0,                 // CMD type, result and error

    0, 0xc0,                   // zx_addr to set memory at C000
    0x55,                      // c, constant value to set
    0, 0,                      // n, 16 bit count to set
  };

  ioctl(1, IOCTL_OTERM_PAUSE, 0);

  uint32_t check_counter = 0;
  while(1)
  {
    memset_cmd[1] = 0;    // Response

    memset_cmd[5] = 0x55; //make this a counter

    memset_cmd[6] = 32;
    memset_cmd[7] = 0;

    uint16_t memset_cmd_addr = (uint16_t)(&memset_cmd[0]);

    *((uint8_t*)14446) = (uint8_t)(memset_cmd_addr & 0xFF);
    *((uint8_t*)14447) = (uint8_t)((memset_cmd_addr >> 8) & 0xFF);
    
    while( memset_cmd[1] == 0 );  // Spin on response going to 1

    uint16_t counter;
    for( counter=0; counter < 32; counter++ )
    {
      uint8_t *check_addr = (uint8_t*)(0xc000+counter);
      if( *check_addr != 0x55 )
      {
	printf("Fail at %ld, expected 0x55, found %02X\n",
	       check_addr, *check_addr);
	while(1);
      }
    }

    printf("Check %ld OK!\n", ++check_counter);
  }
}
