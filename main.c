#include <stdio.h>
#include "pico/stdlib.h"
#include <hardware/i2c.h>
#include <pico/i2c_slave.h>
#include "pico/time.h"
#include <string.h>
#include "ff.h"
#include "lcd.h"
#include "paint.h"

#define SCREEN_BG_COLOR 0x1E0

static const uint I2C_SLAVE_ADDRESS = 050 ; // 0x28
static const uint I2C_BAUDRATE = 100000 ;  // 100 kHz
static const uint I2C_SLAVE_SDA_PIN = PICO_DEFAULT_I2C_SDA_PIN ; // 4
static const uint I2C_SLAVE_SCL_PIN = PICO_DEFAULT_I2C_SCL_PIN ; // 5

#define LP11_LPS 014
#define LP11_LPB 016
#define PC11_PRS 050
#define PC11_PRB 052
#define PC11_PPS 054
#define PC11_PPB 056
#define PC11_RST 060

extern Font font24 ;
extern Font font16 ;

static uint16_t image[224 * 32] ;
static PAINT paint ;
static FATFS fs ;
static FIL ptpfile, ptrfile, lpfile ;
static char ptr_file_name[11] ;
static bool rst = false ;
static uint16_t prs, prb, pps, ppb, lps, lpb ;
static FSIZE_t ptr_size = 0 ;
static FSIZE_t ptr_pos = 0 ;
volatile bool progress_update = false ;

typedef struct _Tapes {
    uint8_t PAGESIZE ;
    int     foundFiles ;
    int8_t  page ;
    int16_t selIdx ;
    DIR     dir ;
    FILINFO fio ;
    char    filenames[7][11] ;
} Tapes ;

static struct {
    uint8_t addr ;
    uint16_t value ;
} context ;

// 12 characters max
static void show_error(const char *msg) {
    paint_clear(&paint, RED) ;
    paint.color = WHITE ;
    paint_draw_string(&paint, 10, 6, msg, &font24, RED) ;
    lcd_display_window(8, 104, 232, 136, image) ;
}

static void show_current_ptr_filename(const char *name) {
    paint_clear(&paint, BLACK) ;
    paint.color = WHITE ;
    char filename[10] ;
    memset(filename, 0, 10) ;
    char *ptr = strstr(name, ".TAP") ;
    if (ptr == NULL) {
        strncpy(filename, name, 9) ;
    } else {
        strncpy(filename, name, (ptr - name)) ;
    }

    paint_draw_string(&paint, 10, 6, filename, &font24, BLACK) ;
    lcd_display_window(8, 104, 232, 136, image) ;
}

static void show_ptr_progress() {
    paint_clear(&paint, SCREEN_BG_COLOR) ;
    paint.color = WHITE ;
    paint_draw_string(&paint, 10, 6, "pos", &font16, SCREEN_BG_COLOR) ;
    paint_draw_number(&paint, 8, 10, ptr_size, &font16, SCREEN_BG_COLOR) ;
    paint_draw_number(&paint, 150, 10, ptr_pos, &font16, SCREEN_BG_COLOR) ;
    lcd_display_window(8, 168, 232, 200, image) ;
}

static void show_list_item(Tapes *tapes, const uint8_t pos) {
    paint_clear(&paint, pos == tapes->selIdx ? BLACK : SCREEN_BG_COLOR) ;
    paint.color = pos == tapes->selIdx ? WHITE : YELLOW ;
    char filename[10] ;
    memset(filename, 0, 10) ;
    char *ptr = strstr(tapes->filenames[pos], ".TAP") ;
    if (ptr == NULL) {
        strncpy(filename, tapes->filenames[pos], 9) ;
    } else {
        strncpy(filename, tapes->filenames[pos], (ptr - tapes->filenames[pos])) ;
    }

    paint_draw_string(&paint, 10, 6, filename, &font24, pos == tapes->selIdx ? BLACK : SCREEN_BG_COLOR) ;
    lcd_display_window(8, pos * 32, 232, (pos + 1) * 32, image) ;
}

static void list_files(Tapes *tapes) {
    lcd_clear(SCREEN_BG_COLOR) ;

    for (uint8_t pos = 0; pos < tapes->PAGESIZE * tapes->page; pos++) {
        if ((pos + 1) > tapes->foundFiles) {
            FRESULT fr ;
            if (tapes->foundFiles == 0) {
                fr = f_findfirst(&tapes->dir, &tapes->fio, "", "*.TAP") ;
                if (fr != FR_OK) {
                    show_error("NO TAPES ...") ;
                    return ;
                }
            } else {
                fr = f_findnext(&tapes->dir, &tapes->fio) ;
            }

            if (fr == FR_OK && tapes->fio.fname[0]) {
                tapes->foundFiles++ ;
            }
        }
    }

    for (uint8_t pos = 0; pos < tapes->PAGESIZE; pos++)  {
        if ((tapes->page * tapes->PAGESIZE + pos + 1) > tapes->foundFiles) {
            FRESULT fr ;
            if (tapes->foundFiles == 0) {
                fr = f_findfirst(&tapes->dir, &tapes->fio, "", "*.TAP") ;
                if (fr != FR_OK) {
                    show_error("NO TAPES ...") ;
                    return ;
                }
            } else {
                fr = f_findnext(&tapes->dir, &tapes->fio) ;
            }

            if (fr == FR_OK && tapes->fio.fname[0]) {
                tapes->foundFiles++ ;
                memset(tapes->filenames[pos], 0, 11) ;
                strncpy(tapes->filenames[pos], tapes->fio.fname, 10) ;
            }
        }
    }

    while (tapes->selIdx >= 0 && !((tapes->page * tapes->PAGESIZE + tapes->selIdx) < tapes->foundFiles)) {
        tapes->selIdx-- ;
    }

    for (uint8_t pos = 0; pos < tapes->PAGESIZE; pos++)  {
        if ((tapes->page * tapes->PAGESIZE + pos) < tapes->foundFiles) {
            show_list_item(tapes, pos) ;
        } else {
            return ;
        }
    }
}

static uint16_t pc11_read16(uint8_t a) {
    switch (a) {
        case LP11_LPS:
            return lps ;
        case LP11_LPB:
            return lpb ;
        case PC11_PRS:
            return rst ? 04000 : prs ;
        case PC11_PRB:
            prs = prs & ~0200 ; // clear DONE,
            return prb;
        case PC11_PPS:
            return rst ? 0 : pps ;
        case PC11_PPB:
            return ppb ;
        default:
            break ;
    }
    return 0 ;
}

static void pc11_write16(const uint8_t a, const uint16_t v) {
    switch (a & ~0100) {
        case LP11_LPS:
            lps = (lps & 0100200) | (v & 077577) ; // bits 15,7 are read-only
            break ;
        case LP11_LPB:
            lpb = v ;
            lps = lps & ~0200 ;
            break;
        case PC11_PRS: {
                uint16_t r = (prs & 0177676) | (v & 0101) ; //only bits 6,0 is write-able
                if (r & 01) {
                    r = (r & ~0200) | 04000 ; // if GO - clear buffer; clear done and set busy
                    prb = 0123 ;
                }
                prs = r ;
            }
            break;
        case PC11_PRB:
            break ; //read-only
        case PC11_PPS:
            pps = (pps & 0100200) | (v & 077577) ; // bits 15,7 are read-only
            break ;
        case PC11_PPB:
            ppb = v ;
            pps = pps & ~0200 ;
            break;
        default:
            break ;
    }
}

static void ptr_reset() {
    prs = 0 ;
    ptr_size = 0 ;
    ptr_pos = 0 ;

    f_close(&ptrfile) ;
    FRESULT fr = f_open(&ptrfile, ptr_file_name, FA_READ | FA_OPEN_ALWAYS) ;
    if (fr != FR_OK && fr != FR_EXIST) {
        show_error("PTR OPEN ERR") ;
    } else {
        f_lseek(&ptrfile, 0) ;
        FILINFO fio ;
        if (f_stat(ptr_file_name, &fio) == FR_OK) {
            ptr_size = fio.fsize ;
            show_ptr_progress() ;
        } else {
            show_error("PTR STAT ERR") ;
        }
    }
}

static void pc11_reset() {
    rst = true ;

    pps = 0200 ; // clear interrupt, set ready

    f_close(&ptpfile) ;

    FRESULT fr = f_open(&ptpfile, "PTP.TAP", FA_WRITE | FA_CREATE_ALWAYS) ;
    if (fr != FR_OK && fr != FR_EXIST) {
        show_error("PTP OPEN ERR") ;
    } else {
        f_truncate(&ptpfile) ;
        f_lseek(&ptpfile, 0) ;
    }

    ptr_reset() ;

    rst = false ;
}

static void lp11_reset() {
    lps = 0200 ; // clear interrupt, set ready

    f_close(&lpfile) ;

    FRESULT fr = f_open(&lpfile, "PRT.TXT", FA_WRITE | FA_CREATE_ALWAYS) ;
    if (fr != FR_OK && fr != FR_EXIST) {
        printf("lp11 f_open %d", fr) ;
    } else {
        f_truncate(&lpfile) ;
        f_lseek(&lpfile, 0) ;
    }
}


int prg_cnt = 0 ;

static void pclp11_step() {
    if (rst) {
        pc11_reset() ;
        lp11_reset() ;
        return ;
    }

    if (((prs & 01) == 0) && ((pps & 0200) != 0) && ((lps & 0200) != 0)) {
        if (progress_update) {
            if (++prg_cnt > 10000000) {
                prg_cnt = 0 ;
                show_ptr_progress() ;
                progress_update = false ;
            }
        }
    }

    // ptr
    if (prs & 01) {
        uint16_t r = (prs & ~04001) | 0200 ; // clear enable, busy. set done
        UINT br = 0 ;
        FRESULT fr = f_read(&ptrfile, &prb, 1, &br) ;
        if (fr != FR_OK || br != 1) {
            r |= 0100000 ;
        } else {
            ptr_pos += br ;
            progress_update = true ;
        }
        
        prs = r ;
    }

    // ptp
    if ((pps & 0200) == 0) {
        UINT bw = 0 ;
        FRESULT fr = f_write(&ptpfile, &ppb, 1, &bw) ;
        if (fr != FR_OK || bw != 1) {
            pps |= 0100000 ;
        } else {
            f_sync(&ptpfile) ;
        }

        pps |= 0200 ; // set ready
    }

    //lp
    if ((lps & 0200) == 0) {
        UINT bw = 0 ;
        FRESULT fr = f_write(&lpfile, &lpb, 1, &bw) ;
        if (fr != FR_OK || bw != 1) {
            lps |= 0100000 ;
        } else {
            f_sync(&lpfile) ;
        }

        lps |= 0200 ; // set ready
    }    
}

static void i2c_slave_handler(i2c_inst_t *i2c, i2c_slave_event_t event) {
    switch (event) {
        case I2C_SLAVE_RECEIVE:{
                gpio_put(PICO_DEFAULT_LED_PIN, false) ;

                while (!i2c_get_read_available(i2c)) {tight_loop_contents();}
                context.addr = i2c_read_byte_raw(i2c) ;

                if (context.addr == PC11_RST) {
                    rst = 1 ;
                } else {
                    if (context.addr & 0100) {
                        while (!i2c_get_read_available(i2c)) {tight_loop_contents();}
                        context.value = i2c_read_byte_raw(i2c) ;
                        while (!i2c_get_read_available(i2c)) {tight_loop_contents();}
                        context.value |= i2c_read_byte_raw(i2c) << 8 ;
                    }
                }

                gpio_put(PICO_DEFAULT_LED_PIN, true) ;
            }

            break;
        case I2C_SLAVE_REQUEST: {
                gpio_put(PICO_DEFAULT_LED_PIN, false) ;

                switch (context.addr) {
                    case PC11_RST:
                        break ;

                    case LP11_LPS:
                    case LP11_LPB:
                    case PC11_PRS:
                    case PC11_PRB:
                    case PC11_PPS:
                    case PC11_PPB: {
                            uint16_t v = pc11_read16(context.addr) ;
                            i2c_write_byte_raw(i2c, (uint8_t)(v & 0377)) ;
                            i2c_write_byte_raw(i2c, (uint8_t)((v >> 8) & 0377)) ;
                            i2c_write_byte_raw(i2c, 1) ;
                        }
                        
                        break ;
                    
                    case LP11_LPS | 0100:
                    case LP11_LPB | 0100:
                    case PC11_PRS | 0100:
                    case PC11_PRB | 0100:
                    case PC11_PPS | 0100:
                    case PC11_PPB | 0100:
                        pc11_write16(context.addr, context.value) ;
                        i2c_write_byte_raw(i2c, 1) ;
                        break ;
                    
                    default:
                        break;
                }

                gpio_put(PICO_DEFAULT_LED_PIN, true) ;
            }

            break;
        case I2C_SLAVE_FINISH:
            gpio_put(PICO_DEFAULT_LED_PIN, false) ;
            break;
        default:
            break;
    }
}

int main() {
    stdio_init_all() ;

    gpio_init(PICO_DEFAULT_LED_PIN) ;
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT) ;
    gpio_put(PICO_DEFAULT_LED_PIN, true) ;

    gpio_init(I2C_SLAVE_SDA_PIN) ;
    gpio_set_function(I2C_SLAVE_SDA_PIN, GPIO_FUNC_I2C) ;
    gpio_pull_up(I2C_SLAVE_SDA_PIN) ;

    gpio_init(I2C_SLAVE_SCL_PIN) ;
    gpio_set_function(I2C_SLAVE_SCL_PIN, GPIO_FUNC_I2C) ;
    gpio_pull_up(I2C_SLAVE_SCL_PIN) ;

    i2c_init(i2c0, I2C_BAUDRATE) ;
    i2c_slave_init(i2c0, I2C_SLAVE_ADDRESS, &i2c_slave_handler) ;

    sleep_ms(100) ;
    lcd_init() ;
    lcd_clear(SCREEN_BG_COLOR) ;

    paint_new_image(&paint, image, 224, 32, WHITE) ;

    if (FR_OK != f_mount(&fs, "SD", 1)) {
        show_error("SDCARD ERROR") ;
        while(1) {
            gpio_put(PICO_DEFAULT_LED_PIN, true) ;
            sleep_ms(200) ;
            gpio_put(PICO_DEFAULT_LED_PIN, false) ;
            sleep_ms(200) ;
        }
    }

    strcpy(ptr_file_name, "PTR.TAP") ;
    show_current_ptr_filename(ptr_file_name) ;

    uint8_t key = 0 ;

    Tapes tapes = {7, 0, 0, 0} ;
    for (uint8_t i = 0; i < tapes.PAGESIZE; i++) {
        memset(tapes.filenames[i], 0, 11) ;
    }

    pc11_reset() ;

    while (true) {
        if (gpio_get(LCD_KEY_LEFT) == 0) {
            key = LCD_KEY_LEFT ;
        } else if (gpio_get(LCD_KEY_RIGHT) == 0) {
            key = LCD_KEY_RIGHT ;
        } else if (gpio_get(LCD_KEY_UP) == 0) {
            key = LCD_KEY_UP ;
        } else if (gpio_get(LCD_KEY_B) == 0) {
            key = LCD_KEY_B ;
        } else if (gpio_get(LCD_KEY_DOWN) == 0) {
            key = LCD_KEY_DOWN ;
        } else if (gpio_get(LCD_KEY_CTRL) == 0) {
            key = LCD_KEY_CTRL ;
        } else if (gpio_get(LCD_KEY_X) == 0) {
            key = LCD_KEY_X ;
        } else if (gpio_get(LCD_KEY_Y) == 0) {
            key = LCD_KEY_Y ;
        } else if (gpio_get(LCD_KEY_A) == 0) {
            key = LCD_KEY_A ;
        } else {
            switch (key) {
                case LCD_KEY_LEFT:
                case LCD_KEY_RIGHT:
                    tapes.foundFiles = 0 ;
                    tapes.selIdx = 0 ;
                    tapes.page = 0 ;
                    list_files(&tapes) ;
                    break;

                case LCD_KEY_UP:
                case LCD_KEY_B:
                    if (--tapes.selIdx < 0) {
                        tapes.selIdx = tapes.PAGESIZE - 1 ;
                        if (--tapes.page < 0) {
                            tapes.page = 0 ;
                        }
                        tapes.foundFiles = 0 ;
                    }
                    list_files(&tapes) ;
                    break;
                
                case LCD_KEY_DOWN:
                case LCD_KEY_Y:
                    if (++tapes.selIdx > (tapes.PAGESIZE - 1)) {
                        tapes.selIdx = 0 ;
                        tapes.page++ ;
                    }
                    list_files(&tapes) ;
                    break;

                case LCD_KEY_CTRL:
                case LCD_KEY_X:
                    memset(ptr_file_name, 0, 11) ;
                    strncpy(ptr_file_name, tapes.filenames[tapes.selIdx], 11) ;
                    lcd_clear(SCREEN_BG_COLOR) ;
                    show_current_ptr_filename(ptr_file_name) ;
                    rst = true ;
                    ptr_reset() ;
                    rst = false ;
                    break;

                case LCD_KEY_A:
                    tapes.foundFiles = 0 ;
                    tapes.page = 0 ;
                    f_unmount("SD") ;
                    lcd_clear(SCREEN_BG_COLOR) ;
                    if (FR_OK != f_mount(&fs, "SD", 1)) {
                        show_error("SDCARD ERROR") ;
                    } else {
                        show_current_ptr_filename(ptr_file_name) ;
                        pc11_reset() ;
                    }

                    break ;
                
                default:
                    break ;
            }

            key = 0 ;
        }

        pclp11_step() ;
    }
    
    return 0 ;
}
