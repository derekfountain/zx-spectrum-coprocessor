/*
 * zcc +zx -vn -startup=4 -clib=sdcc_iy pxy2saddr.c ../common/cmd.c -o z80_image
 * xxd -i -c 16 z80_image_CODE.bin > ../../../firmware/z80_image.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <z80.h>
#include <intrinsic.h>
#include <arch/zx.h>

#include "../common/cmd.h"

void main(void)
{
  PXY2SADDR_INIT(pxy2saddr_cmd);

  ioctl(1, IOCTL_OTERM_PAUSE, 0);

  while(1)
  {
    CMD_CLEAR_STATUS(pxy2saddr_cmd);
    CMD_CLEAR_ERROR(pxy2saddr_cmd);

    PXY2SADDR_SET_X(pxy2saddr_cmd,0);      // x
    PXY2SADDR_SET_Y(pxy2saddr_cmd,1);      // y

    PXY2SADDR_CLEAR_ANSWER(pxy2saddr_cmd); // 16 bit answer goes here

#if 1
    printf("Check %d,%d\n", pxy2saddr_cmd[4], pxy2saddr_cmd[5]);

    printf("%02X %02X %02X %02X  %02X %02X\n",
	   pxy2saddr_cmd[0],pxy2saddr_cmd[1],pxy2saddr_cmd[2],pxy2saddr_cmd[3],
	   pxy2saddr_cmd[4],pxy2saddr_cmd[5]
    );

    CMD_TRIGGER_IMMEDIATE_CMD( pxy2saddr_cmd );
    CMD_SPIN_ON_STATUS( pxy2saddr_cmd );

    if( CMD_IS_COPRO_ERROR( pxy2saddr_cmd ) )
    {
      printf("Error is %d\n", CMD_QUERY_COPRO_ERROR( pxy2saddr_cmd ));
      while(1);
    }

    uint16_t answer = PXY2SADDR_QUERY_ANSWER( pxy2saddr_cmd );
    printf("Answer is %04X\n", answer);
#else

    uint8_t y;
    for( y=0; y<192; y++ )
    {
      uint8_t x = 0;

      do
      {
#if 1
	CMD_CLEAR_STATUS(pxy2saddr_cmd);

	PXY2SADDR_SET_X(pxy2saddr_cmd,x);
	PXY2SADDR_SET_Y(pxy2saddr_cmd,y);

//	uint16_t pxy2saddr_cmd_addr = (uint16_t)(&pxy2saddr_cmd[0]);
//	*((uint8_t*)14446) = (uint8_t)(pxy2saddr_cmd_addr & 0xFF);
//	*((uint8_t*)14447) = (uint8_t)((pxy2saddr_cmd_addr >> 8) & 0xFF);

	CMD_TRIGGER_IMMEDIATE_CMD( pxy2saddr_cmd );
	CMD_SPIN_ON_STATUS( pxy2saddr_cmd );

	/*
	 * This takes about 3.15 secs to fill the screen. Using a pointer
         * to this location results in the exact same code
	 */

	if( CMD_IS_COPRO_ERROR( pxy2saddr_cmd ) )
	{
	  printf("Error is %d\n", CMD_QUERY_COPRO_ERROR( pxy2saddr_cmd ));
	  while(1);
	}

	uint16_t answer = PXY2SADDR_QUERY_ANSWER( pxy2saddr_cmd );
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
