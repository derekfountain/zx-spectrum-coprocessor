/*
 * zcc +zx -vn -startup=4 -clib=sdcc_iy pxy2saddr.c -o z80_image
 * xxd -i -c 16 z80_image_CODE.bin > ../../../firmware/z80_image.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <z80.h>
#include <intrinsic.h>
#include <arch/zx.h>

void main(void)
{
  /* Compiler gives internal error if this isn't static */
  static uint8_t pxy2saddr_cmd[] =
  {
    129, 0,                    // CMD type and flags
    0, 0,                      // Status and error

    0x00, 0x00,                // x,y pixels
    0, 0,                      // answer
  };

  ioctl(1, IOCTL_OTERM_PAUSE, 0);

  while(1)
  {
    pxy2saddr_cmd[2] = 0;    // Status
    pxy2saddr_cmd[3] = 0;    // Error

    pxy2saddr_cmd[4] = 0;    // x
    pxy2saddr_cmd[5] = 1;    // y

    pxy2saddr_cmd[6] = 0;    // 16 bit answer goes here
    pxy2saddr_cmd[7] = 0;

#define ZXCOPRO_NONE  0
#define ZXCOPRO_OK    1
#define ZXCOPRO_ERROR 2
#if 0
    printf("Check %d,%d\n", pxy2saddr_cmd[4], pxy2saddr_cmd[5]);

    printf("%02X %02X %02X %02X  %02X %02X\n",
	   pxy2saddr_cmd[0],pxy2saddr_cmd[1],pxy2saddr_cmd[2],pxy2saddr_cmd[3],
	   pxy2saddr_cmd[4],pxy2saddr_cmd[5]
    );

    *((uint16_t*)14446) = (uint16_t)(&pxy2saddr_cmd[0]);

    while( pxy2saddr_cmd[2] == ZXCOPRO_NONE )
      printf("+\n");  // Spin on status going to 1

    if( pxy2saddr_cmd[2] == ZXCOPRO_ERROR )
    {
      printf("Error is %d\n", pxy2saddr_cmd[3]);
      while(1);
    }

    uint16_t answer = pxy2saddr_cmd[6] + pxy2saddr_cmd[7]*256;
    printf("Answer is %04X\n", answer);
#else

    uint8_t y;
    for( y=0; y<192; y++ )
    {
      uint8_t x = 0;

      do
      {
#if 1
	pxy2saddr_cmd[2] = 0;    // Status

	pxy2saddr_cmd[4] = x;    // x
	pxy2saddr_cmd[5] = y;    // y

//	uint16_t pxy2saddr_cmd_addr = (uint16_t)(&pxy2saddr_cmd[0]);
//	*((uint8_t*)14446) = (uint8_t)(pxy2saddr_cmd_addr & 0xFF);
//	*((uint8_t*)14447) = (uint8_t)((pxy2saddr_cmd_addr >> 8) & 0xFF);

	*((uint16_t*)14446) = (uint16_t)(&pxy2saddr_cmd[0]);

	/*
	 * This takes about 3.15 secs to fill the screen. Using a pointer
         * to this location results in the exact same code
	 */
	while( pxy2saddr_cmd[2] == ZXCOPRO_NONE );

	if( pxy2saddr_cmd[2] == ZXCOPRO_ERROR )
	{
	  printf("Error is %d\n", pxy2saddr_cmd[3]);
	  while(1);
	}

	uint16_t answer = *(uint16_t*)&pxy2saddr_cmd[6];
#else
	/* Takes about 3.4 secs to fill the screen */
	uint16_t answer = (uint16_t)zx_pxy2saddr( x, y );
#endif
	*(uint8_t*)answer = 255;
      
      } while( ++x != 0 );
    }

    while(1);

#endif

  }
}
