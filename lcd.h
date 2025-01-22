#ifndef _LCD_H_
#define _LCD_H_

#include "pico/types.h"

#define SPI_PORT   spi1
#define LCD_DC_PIN    8
#define LCD_CS_PIN    9
#define LCD_CLK_PIN  10
#define LCD_MOSI_PIN 11
#define LCD_RST_PIN  12
#define LCD_BL_PIN   13

#define LCD_KEY_A     15
#define LCD_KEY_B      7
#define LCD_KEY_X     22
#define LCD_KEY_Y     21
#define LCD_KEY_UP     2
#define LCD_KEY_DOWN  14
#define LCD_KEY_LEFT   6
#define LCD_KEY_RIGHT 20
#define LCD_KEY_CTRL   3

void lcd_init() ;
void lcd_clear(uint16_t color) ;
// void lcd_set_window(uint8_t xs, uint8_t ys, uint8_t xe, uint8_t ye) ;
void lcd_display_point(uint8_t x, uint8_t y, uint16_t c) ;
void lcd_display_window(uint8_t xs, uint8_t ys, uint8_t xe, uint8_t ye, uint16_t *image) ;

#endif
