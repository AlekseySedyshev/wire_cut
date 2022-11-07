// Library has been modified for STM32F0xx by Aleksey Sedyshev
// https://github.com/AlekseySedyshev
//==================================================================
//  Siemens M55 LCD (LM15SGFNZ07) library
//  Original code by Mindogas 2011. markauskas.mindaugas at gmail.com
//  Adapted by Maarten Janssen in 2015. maarten@cheerful.nl

#ifndef LCD_M15SGF_h
#define LCD_M15SGF_h

#include "stm32f0xx.h"

#define SPI1_DR_8bit *((__IO uint8_t *)&SPI1->DR) // Limit for spi bus 8 bit

#define HI 1
#define LO 0

#define CMD_() GPIOA->BSRR |= 1 << 6 // Pa6->0
#define DAT_() GPIOA->BRR |= 1 << 6  // Pa6->1

#define CS_HI() GPIOB->BSRR |= 1 << 1
#define CS_LO() GPIOB->BRR |= 1 << 1

#define BLACK 0x000
#define BLUE 0x00F
#define GREEN 0x0F0
#define CYAN 0x0FF
#define RED 0xF00
#define PINK 0xF0F
#define YELLOW 0xFF0
#define WHITE 0xFFF

// Line inverse options
#define LCD_INVERSE_OFF 0x00
#define LCD_INVERSE_ON 0x01
#define LCD_INVERSE_BLINK 0x03

typedef enum
{
    false = 0,
    true = 1,
    ON = 1,
    OFF = 0
} bool;

typedef enum
{
    CMD = 0,
    DAT = 1
} cmd_data_sv;

void LCD_Init(void);
void SPI_Wr(cmd_data_sv data, unsigned char value);

void setContrast(unsigned char contrast);
void setLcdOn(bool on);
void setStartLine(unsigned char y);
void setWindow(unsigned char x, unsigned char y, unsigned char width, unsigned char height);
void clear(unsigned int color);

void drawBitmap(unsigned char x, unsigned char y, unsigned char width, unsigned char height, unsigned short *bitmap);
void drawLine(unsigned char x1, unsigned char y1, unsigned char x2, unsigned char y2, unsigned int color);
void drawRect(unsigned char x, unsigned char y, unsigned char width, unsigned char height, unsigned int color);
void fillRect(unsigned char x, unsigned char y, unsigned char width, unsigned char height, unsigned int color);
void LCD_DrawCircle(char xcenter, char ycenter, char rad, int color);
void LCD_FillCircle(char xcenter, char ycenter, char rad, int color);
void LCD_DrawTriangle(char x1, char y1, char x2, char y2, char x3, char y3, int color);
void LCD_FillTriangle(char x1, char y1, char x2, char y2, char x3, char y3, int color);

void inverseScreen(bool inverse);
void setInverseLines(unsigned char y, unsigned char height, unsigned char inverseMode);
void LCD_Print(char *str, int x, int y, int t_color, int b_color, char zoom_width, char zoom_height, int rot);
void LCD_PrintDec(long val, int x, int y, int t_color, int b_color, char zoom_width, char zoom_height, int rot);
void LCD_PrintHex(long val, int x, int y, int t_color, int b_color, char zoom_width, char zoom_height, int rot);
void LCD_PrintBin(unsigned char value, int x, int y, int t_color, int b_color, char zoom_width, char zoom_height, int rot);

#endif
