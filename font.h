#ifndef _FONT_H_
#define _FONT_H_

#include "pico/types.h"

typedef struct _Font {    
  const uint8_t *table;
  uint8_t width;
  uint8_t height;
} Font ;



#endif
