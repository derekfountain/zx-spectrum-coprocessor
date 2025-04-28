/*
 * zcc +zx -vn -startup=0 -clib=sdcc_iy pxy2saddr.c -o z80_image
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
    129, 0, 0,                 // CMD type, result and error

    0x00, 0x00,                // x,y pixels
    0, 0,                      // answer
  };

  ioctl(1, IOCTL_OTERM_PAUSE, 0);

  while(1)
  {
    pxy2saddr_cmd[1] = 0;    // Response
    pxy2saddr_cmd[2] = 0;    // Error

    pxy2saddr_cmd[3] = 0;    // x
    pxy2saddr_cmd[4] = 1;    // y

    pxy2saddr_cmd[5] = 0;    // 16 bit answer goes here
    pxy2saddr_cmd[6] = 0;

    uint16_t pxy2saddr_cmd_addr = (uint16_t)(&pxy2saddr_cmd[0]);

#if 0
    printf("Check %d,%d\n", pxy2saddr_cmd[3], pxy2saddr_cmd[4]);

    printf("%02X %02X %02X  %02X %02X\n",
	   pxy2saddr_cmd[0],pxy2saddr_cmd[1],pxy2saddr_cmd[2],
	   pxy2saddr_cmd[3],pxy2saddr_cmd[4]
    );

    *((uint8_t*)14446) = (uint8_t)(pxy2saddr_cmd_addr & 0xFF);
    *((uint8_t*)14447) = (uint8_t)((pxy2saddr_cmd_addr >> 8) & 0xFF);
    
    while(1)
    {
      if( pxy2saddr_cmd[1] == 0 && pxy2saddr_cmd[2] == 0 )
      {
	printf("+\n");  // Spin on response going to 1
      }
      else
      {
	if( pxy2saddr_cmd[1] != 0 )
	{
	  printf("Response is %d\n", pxy2saddr_cmd[1]);
	  break;
	}

	if( pxy2saddr_cmd[2] != 0 )
	{
	  printf("Error is %d\n", pxy2saddr_cmd[2]);
	  while(1);
	}

      }
    }

    uint16_t answer = pxy2saddr_cmd[5] + pxy2saddr_cmd[6]*256;
    printf("Answer is %04X\n", answer);
#else

    uint8_t y;
    for( y=0; y<192; y++ )
    {
      uint8_t x = 0;

      do
      {
#if 1
	pxy2saddr_cmd[1] = 0;    // Response

	pxy2saddr_cmd[3] = x;    // x
	pxy2saddr_cmd[4] = y;    // y

//	*((uint8_t*)14446) = (uint8_t)(pxy2saddr_cmd_addr & 0xFF);
//	*((uint8_t*)14447) = (uint8_t)((pxy2saddr_cmd_addr >> 8) & 0xFF);

	*((uint16_t*)14446) = pxy2saddr_cmd_addr;

	/*
	 * This takes about 3.15 secs to fill the screen. Using a pointer
         * to this location results in the exact same code
	 */
	while( pxy2saddr_cmd[1] == 0 );

	uint16_t answer = *(uint16_t*)&pxy2saddr_cmd[5];
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
