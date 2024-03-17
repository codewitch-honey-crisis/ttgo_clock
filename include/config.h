
#ifndef CONFIG_H
#define CONFIG_H
// for the TTGO

#define LCD_HOST SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define LCD_BK_LIGHT_ON_LEVEL 1
#define LCD_BK_LIGHT_OFF_LEVEL !LCD_BK_LIGHT_ON_LEVEL
#define LCD_PIN_NUM_DATA0 19  //!< for 1-line SPI, this also refereed as MOSI
#define LCD_PIN_NUM_PCLK 18

#define LCD_PIN_NUM_CS 5
#define LCD_PIN_NUM_DC 16
#define LCD_PIN_NUM_RST 23
#define LCD_PIN_NUM_BK_LIGHT 4

#define LCD_X_OFFSET 40
#define LCD_Y_OFFSET 52
#define LCD_INVERT_COLOR
#define LCD_SWAP_XY
// #define LCD_MIRROR_X
#define LCD_MIRROR_Y
#define LCD_COLOR_SPACE ESP_LCD_COLOR_SPACE_RGB
// The pixel number in horizontal and vertical
#define LCD_H_RES 240
#define LCD_V_RES 135
// Bit number used to represent command and parameter
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

#define PIN_BUTTON_1 35
#define PIN_BUTTON_2 0

#define COMMAND Serial
#endif