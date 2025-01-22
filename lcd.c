#include "lcd.h"

#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/time.h"

#define LCD_HEIGHT 240
#define LCD_WIDTH 240

static int slice_num ;

void lcd_reset() {
    gpio_put(LCD_RST_PIN, true) ;
    sleep_ms(100);
    gpio_put(LCD_RST_PIN, false) ;
    sleep_ms(100);
    gpio_put(LCD_RST_PIN, true) ;
    sleep_ms(100);
}

static void lcd_send_command(uint8_t cmd) {
    gpio_put(LCD_DC_PIN, false) ;
    gpio_put(LCD_CS_PIN, false) ;
    spi_write_blocking(SPI_PORT, &cmd, 1) ;
    gpio_put(LCD_CS_PIN, true) ;
}

static void lcd_send_data8(uint8_t data) {
    gpio_put(LCD_DC_PIN, true) ;
    gpio_put(LCD_CS_PIN, false) ;
    spi_write_blocking(SPI_PORT, &data, 1) ;
    gpio_put(LCD_CS_PIN, true) ;
}

static void lcd_send_data16(uint16_t data) {
    gpio_put(LCD_DC_PIN, true) ;
    gpio_put(LCD_CS_PIN, false) ;
    uint8_t d = (data >> 8) & 0xFF ;
    spi_write_blocking(SPI_PORT, &d, 1) ;
    d = data & 0xFF ;
    spi_write_blocking(SPI_PORT, &d, 1) ;
    gpio_put(LCD_CS_PIN, true) ;
}

void lcd_set_window(uint8_t xs, uint8_t ys, uint8_t xe, uint8_t ye) {
    lcd_send_command(0x2A);
    lcd_send_data8(0x00);
    lcd_send_data8(xs);
	lcd_send_data8(0x00);
    lcd_send_data8(xe - 1);

    //set the Y coordinates
    lcd_send_command(0x2B);
    lcd_send_data8(0x00);
	lcd_send_data8(ys);
	lcd_send_data8(0x00);
    lcd_send_data8(ye - 1);

    lcd_send_command(0x2C);
}

void lcd_clear(uint16_t color) {
    uint16_t image[LCD_WIDTH] ;
    int i ;
    color = ((color << 8) & 0xff00) | (color >> 8) ;
    for (i = 0; i < LCD_WIDTH; i++) {
        image[i] = color ;
    }

    lcd_set_window(0, 0, LCD_WIDTH, LCD_HEIGHT) ;

    gpio_put(LCD_DC_PIN, true);
    gpio_put(LCD_CS_PIN, false);
    for(i = 0; i < LCD_HEIGHT; i++) {
        spi_write_blocking(SPI_PORT, (uint8_t *)image, LCD_WIDTH * 2) ;
    }
    gpio_put(LCD_CS_PIN, true) ;
}

void lcd_init() {
    spi_init(SPI_PORT, 10000 * 1000) ;
    gpio_set_function(LCD_CLK_PIN, GPIO_FUNC_SPI) ;
    gpio_set_function(LCD_MOSI_PIN, GPIO_FUNC_SPI) ;

    gpio_init(LCD_RST_PIN);
    gpio_set_dir(LCD_RST_PIN, GPIO_OUT) ;

    gpio_init(LCD_DC_PIN);
    gpio_set_dir(LCD_DC_PIN, GPIO_OUT) ;

    gpio_init(LCD_CS_PIN);
    gpio_set_dir(LCD_CS_PIN, GPIO_OUT) ;

    gpio_init(LCD_BL_PIN);
    gpio_set_dir(LCD_BL_PIN, GPIO_OUT) ;

    gpio_init(LCD_KEY_A);
    gpio_set_dir(LCD_KEY_A, GPIO_IN) ;
    gpio_pull_up(LCD_KEY_A) ;

    gpio_init(LCD_KEY_B);
    gpio_set_dir(LCD_KEY_B, GPIO_IN) ;
    gpio_pull_up(LCD_KEY_B) ;

    gpio_init(LCD_KEY_X);
    gpio_set_dir(LCD_KEY_B, GPIO_IN) ;
    gpio_pull_up(LCD_KEY_X) ;

    gpio_init(LCD_KEY_Y);
    gpio_set_dir(LCD_KEY_B, GPIO_IN) ;
    gpio_pull_up(LCD_KEY_Y) ;

    gpio_init(LCD_KEY_UP);
    gpio_set_dir(LCD_KEY_UP, GPIO_IN) ;
    gpio_pull_up(LCD_KEY_UP) ;

    gpio_init(LCD_KEY_DOWN);
    gpio_set_dir(LCD_KEY_DOWN, GPIO_IN) ;
    gpio_pull_up(LCD_KEY_DOWN) ;

    gpio_init(LCD_KEY_LEFT);
    gpio_set_dir(LCD_KEY_LEFT, GPIO_IN) ;
    gpio_pull_up(LCD_KEY_LEFT) ;

    gpio_init(LCD_KEY_RIGHT);
    gpio_set_dir(LCD_KEY_RIGHT, GPIO_IN) ;
    gpio_pull_up(LCD_KEY_RIGHT) ;

    gpio_init(LCD_KEY_CTRL);
    gpio_set_dir(LCD_KEY_CTRL, GPIO_IN) ;
    gpio_pull_up(LCD_KEY_CTRL) ;

    gpio_put(LCD_CS_PIN, true) ;
    gpio_put(LCD_DC_PIN, false) ;
    gpio_put(LCD_BL_PIN, true) ;

    gpio_set_function(LCD_BL_PIN, GPIO_FUNC_PWM) ;
    slice_num = pwm_gpio_to_slice_num(LCD_BL_PIN) ;
    pwm_set_wrap(slice_num, 100) ;
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 1) ;
    pwm_set_clkdiv(slice_num, 50) ;
    pwm_set_enabled(slice_num, true) ;

    pwm_set_chan_level(slice_num, PWM_CHAN_B, 50) ;

    lcd_reset() ;

    // Horizontal
    lcd_send_command(0x36) ;
    lcd_send_data8(0x70) ;

    // Initialize the lcd register
    lcd_send_command(0x3A);
    lcd_send_data8(0x05);

    lcd_send_command(0xB2);
    lcd_send_data8(0x0C);
    lcd_send_data8(0x0C);
    lcd_send_data8(0x00);
    lcd_send_data8(0x33);
    lcd_send_data8(0x33);

    lcd_send_command(0xB7);  //Gate Control
    lcd_send_data8(0x35);

    lcd_send_command(0xBB);  //VCOM Setting
    lcd_send_data8(0x19);

    lcd_send_command(0xC0); //LCM Control     
    lcd_send_data8(0x2C);

    lcd_send_command(0xC2);  //VDV and VRH Command Enable
    lcd_send_data8(0x01);
    lcd_send_command(0xC3);  //VRH Set
    lcd_send_data8(0x12);
    lcd_send_command(0xC4);  //VDV Set
    lcd_send_data8(0x20);

    lcd_send_command(0xC6);  //Frame Rate Control in Normal Mode
    lcd_send_data8(0x0F);
    
    lcd_send_command(0xD0);  // Power Control 1
    lcd_send_data8(0xA4);
    lcd_send_data8(0xA1);

    lcd_send_command(0xE0);  //Positive Voltage Gamma Control
    lcd_send_data8(0xD0);
    lcd_send_data8(0x04);
    lcd_send_data8(0x0D);
    lcd_send_data8(0x11);
    lcd_send_data8(0x13);
    lcd_send_data8(0x2B);
    lcd_send_data8(0x3F);
    lcd_send_data8(0x54);
    lcd_send_data8(0x4C);
    lcd_send_data8(0x18);
    lcd_send_data8(0x0D);
    lcd_send_data8(0x0B);
    lcd_send_data8(0x1F);
    lcd_send_data8(0x23);

    lcd_send_command(0xE1);  //Negative Voltage Gamma Control
    lcd_send_data8(0xD0);
    lcd_send_data8(0x04);
    lcd_send_data8(0x0C);
    lcd_send_data8(0x11);
    lcd_send_data8(0x13);
    lcd_send_data8(0x2C);
    lcd_send_data8(0x3F);
    lcd_send_data8(0x44);
    lcd_send_data8(0x51);
    lcd_send_data8(0x2F);
    lcd_send_data8(0x1F);
    lcd_send_data8(0x1F);
    lcd_send_data8(0x20);
    lcd_send_data8(0x23);

    lcd_send_command(0x21);  //Display Inversion On

    lcd_send_command(0x11);  //Sleep Out

    lcd_send_command(0x29);  //Display On
}

void lcd_display_point(uint8_t x, uint8_t y, uint16_t c) {
    lcd_set_window(x, y, x, y);
    lcd_send_data16(c) ;
}

//8, 104, 232, 136 
void lcd_display_window(uint8_t xs, uint8_t ys, uint8_t xe, uint8_t ye, uint16_t *image) {
    lcd_set_window(xs, ys, xe , ye) ;
    gpio_put(LCD_DC_PIN, true) ;
    gpio_put(LCD_CS_PIN, false) ;
    for (uint8_t y = 0; y < (ye - ys); y++) {
        uint8_t *addr = (uint8_t *) &image[y * (xe - xs)] ;
        spi_write_blocking(SPI_PORT, addr, (xe - xs) * 2) ;
   }
    gpio_put(LCD_CS_PIN, true);
}
