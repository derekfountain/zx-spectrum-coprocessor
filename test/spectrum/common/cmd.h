#ifndef __CMD_H
#define __CMD_H

#include <stdint.h>
#include "zx_copro.h"
#include "dma_status.h"

typedef enum
{
  ZXCOPRO_MEMSET_SMALL  = 128,
  ZXCOPRO_PXY2SADDR,

  ZXCOPRO_MEMSET_LARGE,          // FIXME Still not sure if commands which run on /int should be separate or flagged
}
ZXCOPRO_CMD;

typedef enum
{
  CMD_ERR_BAD_STRUCT = DMA_STATUS_LAST,
  CMD_ERR_UNKNOWN_CMD,

  CMD_ERR_BAD_ARG,         // Arguments make no sense
  CMD_ERR_TOO_BIG,         // Number of bytes to DMA is too large
  CMD_ERR_BAD_INCR,        // An increment value is way out

  CMD_ERR_LAST
}
CMD_ERROR;

#define ZXCOPRO_NONE         (uint8_t)0
#define ZXCOPRO_OK           (uint8_t)1
#define ZXCOPRO_ERROR        (uint8_t)2


#define CMD_CLEAR_STATUS(NAME)  NAME[2] = ZXCOPRO_NONE
#define CMD_CLEAR_ERROR(NAME)   NAME[3] = 0

#define CMD_TRIGGER_IMMEDIATE_CMD(NAME)    *((uint16_t*)14446) = (uint16_t)NAME
#define CMD_SPIN_ON_STATUS(NAME)           while( NAME[2] == ZXCOPRO_NONE )

#define CMD_IS_COPRO_ERROR(NAME)    (NAME[2] == ZXCOPRO_ERROR)
#define CMD_QUERY_COPRO_ERROR(NAME) (NAME[3])

/* Initialise structure for a memset */
#define MEMSET_INIT(NAME) static uint8_t NAME[] =	   \
{                                                          \
ZXCOPRO_MEMSET_SMALL, 0,   /* CMD type and flags */	   \
0, 0,                      /* Status and error */	   \
                                                           \
0x00, 0x00,                /* zx_addr to set memory at */  \
0x00,                      /* c, constant value to set */  \
0, 0,                      /* n, 16 bit count to set */	   \
}

#define MEMSET_SET_DEST(NAME,DEST)     NAME[4] = DEST & 0xFF; \
                                       NAME[5] = (DEST>>8) & 0xFF
#define MEMSET_SET_C(NAME,C)           NAME[6] = C
#define MEMSET_SET_LENGTH(NAME,LENGTH) NAME[7] = LENGTH & 0xFF; \
                                       NAME[8] = (LENGTH>>8) & 0xFF



/* Initialise structure for a pxy2saddr */
#define PXY2SADDR_INIT(NAME) static uint8_t NAME[] =	   \
{                                                          \
ZXCOPRO_PXY2SADDR, 0,      /* CMD type and flags */	   \
0, 0,                      /* Status and error */	   \
                                                           \
0x00, 0x00,                /* x,y */                       \
0, 0,                      /* answer */	                   \
}

#define PXY2SADDR_SET_X(NAME,X)         NAME[4] = X
#define PXY2SADDR_SET_Y(NAME,Y)         NAME[5] = Y
#define PXY2SADDR_CLEAR_ANSWER(NAME)    NAME[6] = NAME[7] = 0
#define PXY2SADDR_QUERY_ANSWER(NAME)    (*(uint16_t*)&NAME[6])


#endif
