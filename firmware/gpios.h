/*
 * GPIOs for RP2350 Stamp XL
 *
 * These need to match the hardware design.
 */

#ifndef __GPIOS_H
#define GPIOS_H

/* Test signals to scope */
#define GPIO_BLIPPER1     46
#define GPIO_BLIPPER2     47

/* Take over the Spectrum's ROM */
#define GPIO_ROMCS        28

/*
 * This pin is the output connected to the base of Q101.
 * Driving it positive pulls ZXRESET low
 */
#define GPIO_RESET_Z80    43

/*
 * This pin is an input to the RP2350. It's the Z80's
 * /RESET signal and is here so the RP2350 can tell when
 * the Spectrum is being reset. This isn't an output for
 * resetting the Spectrum, GPIO_RESET_Z80 does that.
*/
#define GPIO_Z80_RESET    36

/* Z80 Control bus */
#define GPIO_Z80_CLK      24
#define GPIO_Z80_INT      25
#define GPIO_Z80_RD       26
#define GPIO_Z80_WR       27
#define GPIO_Z80_MREQ     29
#define GPIO_Z80_IORQ     30
#define GPIO_Z80_BUSREQ   31
#define GPIO_Z80_BUSACK   32
#define GPIO_Z80_WAIT     33

/* Z80 Data bus */
#define GPIO_DBUS_D0      0
#define GPIO_DBUS_D1      1
#define GPIO_DBUS_D2      2
#define GPIO_DBUS_D3      3
#define GPIO_DBUS_D4      4
#define GPIO_DBUS_D5      5
#define GPIO_DBUS_D6      6
#define GPIO_DBUS_D7      7

#define GPIO_DBUS_BITMASK 0x000000FF

/* Z80 Address bus */
#define GPIO_ABUS_A0      8
#define GPIO_ABUS_A1      9
#define GPIO_ABUS_A2      10
#define GPIO_ABUS_A3      11
#define GPIO_ABUS_A4      12
#define GPIO_ABUS_A5      13
#define GPIO_ABUS_A6      14
#define GPIO_ABUS_A7      15
#define GPIO_ABUS_A8      16
#define GPIO_ABUS_A9      17
#define GPIO_ABUS_A10     18
#define GPIO_ABUS_A11     19
#define GPIO_ABUS_A12     20
#define GPIO_ABUS_A13     21
#define GPIO_ABUS_A14     22
#define GPIO_ABUS_A15     23

#define GPIO_ABUS_BITMASK 0x00FFFF00

#endif
