#include "paint.h"
#include <stdio.h>

void paint_new_image(PAINT *paint, uint16_t *image, uint8_t width, uint8_t height, uint16_t color) {
    paint->image = image;

    paint->width = width;
    paint->height = height;
    paint->widthMemory = width;
    paint->heightMemory = height;
    paint->color = color;

    paint->widthByte = (width % 8 == 0) ? (width / 8) : (width / 8 + 1) ;
    paint->heightByte = height ;
}

void paint_set_pixel(PAINT *paint, uint8_t x, uint8_t y, uint16_t color) {
    if (x > paint->width || y > paint->height) {
        return;
    }

    paint->image[x + y * paint->width] = ((color << 8) & 0xff00) | (color >> 8) ;
}

void paint_clear(PAINT *paint, uint16_t color) {
    for (uint8_t y = 0; y < paint->height; y++) {
        for (uint8_t x = 0; x < paint->width; x++) {
            paint->image[x + y * paint->width] = ((color << 8) & 0xff00) | (color >> 8) ;
        }
    }
}

void paint_draw_char(PAINT *paint, uint8_t x, uint8_t y, const char ch, Font *font, uint16_t bg) {
    if (x > paint->width || y > paint->height) {
        return;
    }

    uint32_t off = (ch - ' ') * font->height * (font->width / 8 + (font->width % 8 ? 1 : 0));
    const unsigned char *ptr = &font->table[off];

    for (uint8_t p = 0; p < font->height; p++) {
        for (uint8_t c = 0; c < font->width; c++) {

            if (*ptr & (0x80 >> (c % 8))) {
                paint_set_pixel(paint, x + c, y + p, paint->color);
            } else {
                paint_set_pixel(paint, x + c, y + p, bg);
            }
            // One pixel is 8 bits
            if (c % 8 == 7) {
                ptr++;
            }
        }
        if (font->width % 8 != 0) {
            ptr++ ;
        }
    }
}

void paint_draw_string(PAINT *paint, uint8_t x, uint8_t y, const char *pString, Font *font, uint16_t bg) {
    uint8_t X = x;
    uint8_t Y = y;

    if (x > paint->width || y > paint->height) {
        return;
    }

    while (*pString != '\0') {
        if ((X + font->width) > paint->width) {
            X = x;
            Y += font->height ;
        }

        if ((Y + font->height) > paint->height){
            X = x;
            Y = y;
        }
        paint_draw_char(paint, X, Y, *pString, font, bg) ;

        pString++;
        X += font->width;
    }
}

void paint_draw_number(PAINT *paint, uint8_t x, uint8_t y, uint64_t num, Font *font, uint16_t bg) {
    char str[10] ;
    sprintf(str, "%o", num) ;
    paint_draw_string(paint, x, y, str, font, bg) ;
}
