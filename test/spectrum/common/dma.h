#ifndef __DMA_STATUS_H
#define __DMA_STATUS_H

typedef enum
{
  DMA_STATUS_OK = 0,

  DMA_STATUS_BAD_STRUCT,
  DMA_STATUS_TOO_BIG,                    // Number of bytes to DMA is too large
  DMA_STATUS_TOO_SMALL,                  // Number of bytes to DMA is too small (zero)
  DMA_STATUS_TOP_BORDER_TOO_BIG,         // Number of bytes to DMA in top border time is too large
  DMA_STATUS_BAD_INCR,                   // An increment value is way out
  DMA_STATUS_CONTENTION_FAIL,            // DMA would clash with ULA's contention

  DMA_STATUS_LAST
}
DMA_STATUS;

#endif
