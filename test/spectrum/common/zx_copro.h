#ifndef __ZX_COPRO_H
#define __ZX_COPRO_H

#include <stdint.h>

typedef enum
{
  ZXCOPRO_NONE        = 0,
  ZXCOPRO_OK          = 1,
  ZXCOPRO_ERROR       = 2,

  ZXCOPRO_UNABLE_TO_RETURN_RESPONSE,
  ZXCOPRO_UNKNOWN
}
ZXCOPRO_STATUS;

#endif

