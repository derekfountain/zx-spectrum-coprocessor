/*
 * zcc +zx -vn -startup=5 -clib=sdcc_iy z80_image.c -o z80_image
 * xxd -i z80_image_CODE.bin > ../../../firmware/z80_image.h
 */

#include <input.h>
#include <input/input_zx.h>
#include <stdio.h>
#include <sys/ioctl.h>

void main(void)
{
  ioctl(1, IOCTL_OTERM_PAUSE, 0);

  while(1)
  {
    uint8_t i = in_inkey();
    printf("Hello world, keys value is 0x%02X\n", i);
  }
}
