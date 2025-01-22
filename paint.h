#ifndef _PAINT_H_
#define _PAINT_H_

#include "pico/types.h"
#include "font.h"

#define WHITE          0xFFFF
#define BLACK          0x0000
#define BLUE           0x001F
#define BRED           0xF81F
#define GRED           0xFFE0
#define GBLUE          0x07FF
#define RED            0xF800
#define MAGENTA        0xF81F
#define GREEN          0x07E0
#define CYAN           0x7FFF
#define YELLOW         0xFFE0
#define BROWN          0xBC40
#define BRRED          0xFC07
#define GRAY           0x8430

typedef struct {
    uint16_t *image;
    uint8_t width;
    uint8_t height;
    uint8_t widthMemory;
    uint8_t heightMemory;
    uint16_t color;
    uint8_t widthByte;
    uint8_t heightByte;
} PAINT ;

void paint_new_image(PAINT *paint, uint16_t *image, uint8_t width, uint8_t height, uint16_t color) ;
void paint_clear(PAINT *paint, uint16_t color) ;
void paint_draw_char(PAINT *paint, uint8_t x, uint8_t y, const char ch, Font *font, uint16_t bg) ;
void paint_draw_string(PAINT *paint, uint8_t x, uint8_t y, const char *pString, Font *font, uint16_t bg) ;
void paint_draw_number(PAINT *paint, uint8_t x, uint8_t y, uint64_t num, Font *font, uint16_t bg) ;

#endif
