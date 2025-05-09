/*
 * zcc +zx -vn -startup=4 -clib=sdcc_iy memset_isr.c ../common/cmd.c -o z80_image
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
#include <string.h>

#include "../common/cmd.h"

#define TABLE_HIGH_BYTE        ((unsigned int)0xD0)
#define JUMP_POINT_HIGH_BYTE   ((unsigned int)0xD1)

#define UI_256                 ((unsigned int)256)
#define TABLE_ADDR             ((void*)(TABLE_HIGH_BYTE*UI_256))
#define JUMP_POINT             ((unsigned char*)( (unsigned int)(JUMP_POINT_HIGH_BYTE*UI_256) + JUMP_POINT_HIGH_BYTE ))

IM2_DEFINE_ISR(isr)
{
  static uint8_t  c_value = 0x00;

  MEMSET_INIT(memset_cmd,CMD_FLAG_TOP_BORDER);

  uint16_t dest_addr = 0x4000 + (rand() & 0x00FF);            // Lower RAM, contended, screen
  uint16_t length = (rand() & 0x00FF)+1;                      // 256 bytes

  CMD_CLEAR_STATUS(memset_cmd);
  CMD_CLEAR_ERROR(memset_cmd);

  MEMSET_SET_DEST(memset_cmd,dest_addr);
  MEMSET_SET_C(memset_cmd,c_value);
  MEMSET_SET_LENGTH(memset_cmd,length);

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

  c_value++;

}

void setup_int(void)
{
  memset( TABLE_ADDR, JUMP_POINT_HIGH_BYTE, 257 );
  z80_bpoke( JUMP_POINT,   195 );
  z80_wpoke( JUMP_POINT+1, (unsigned int)isr );
  im2_init( TABLE_ADDR );

  intrinsic_ei();
}

void main(void)
{
  srand( 1000 );
  ioctl(1, IOCTL_OTERM_PAUSE, 0);

  setup_int();

  while(1);
}
