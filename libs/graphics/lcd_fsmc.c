/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2013 Gordon Williams <gw@pur3.co.uk>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * Graphics Backend for 16 bit parallel LCDs (ILI9325 and similar)
 *
 * Loosely based on example code that comes with 'HY' branded STM32 boards,
 * original Licence unknown.
 * ----------------------------------------------------------------------------
 */

#include "platform_config.h"
#include "lcd_fsmc.h"
#include "jshardware.h"
#include "jsinteractive.h" // for debug
#include "graphics.h"

const unsigned int DELAY_SHORT = 10;

#define GPIO_PORT_FROM_PIN(pin) (GPIOA_BASE + 0x400*(pin>>4)) // Get the GPIO port from a pin number
#define GPIO_BIT_FROM_PIN(pin) (1<<(pin&15)) // Get the GPIO pin bitmask from a pin number

void LCD_DELAY(__IO uint32_t nCount) {
  for(; nCount != 0; nCount--) ;//n++;
}

static inline void delay_ms(__IO uint32_t mSec) {
  jshDelayMicroseconds(mSec*1000);
}

static uint8_t LCD_Code;
#define  ILI9320    0  /* 0x9320 */
#define  ILI9325    1  /* 0x9325 */
#define  ILI9328    2  /* 0x9328 */
#define  ILI9331    3  /* 0x9331 */
#define  SSD1298    4  /* 0x8999 */
#define  SSD1289    5  /* 0x8989 */
#define  ST7781     6  /* 0x7783 */
#define  LGDP4531   7  /* 0x4531 */
#define  SPFD5408B  8  /* 0x5408 */
#define  R61505U    9  /* 0x1505 0x0505 */
#define  HX8346A    10 /* 0x0046 */
#define  HX8347D    11 /* 0x0047 */
#define  HX8347A    12 /* 0x0047 */
#define  LGDP4535   13 /* 0x4535 */
#define  SSD2119    14 /* 3.5 LCD 0x9919 */
#define  ILI9341    15 /* 0x41 */
#define  ST7796     16 /* 0x7796 */

// ST7796 registers
#define ST7796_NOP     0x00
#define ST7796_SWRESET 0x01
#define ST7796_RDDID   0x04
#define ST7796_RDDST   0x09
#define ST7796_SLPIN   0x10
#define ST7796_SLPOUT  0x11
#define ST7796_PTLON   0x12
#define ST7796_NORON   0x13
#define ST7796_RDMODE  0x0A
#define ST7796_RDMADCTL  0x0B
#define ST7796_RDPIXFMT  0x0C
#define ST7796_RDIMGFMT  0x0A
#define ST7796_RDSELFDIAG  0x0F
#define ST7796_INVOFF  0x20
#define ST7796_INVON   0x21
#define ST7796_DISPOFF 0x28
#define ST7796_DISPON  0x29
#define ST7796_CASET   0x2A
#define ST7796_PASET   0x2B
#define ST7796_RAMWR   0x2C
#define ST7796_RAMRD   0x2E
#define ST7796_PTLAR   0x30
#define ST7796_VSCRDEF 0x33
#define ST7796_MADCTL  0x36
#define ST7796_VSCRSADD 0x37
#define ST7796_PIXFMT  0x3A
#define ST7796_WRDISBV  0x51
#define ST7796_RDDISBV  0x52
#define ST7796_WRCTRLD  0x53
#define ST7796_FRMCTR1 0xB1
#define ST7796_FRMCTR2 0xB2
#define ST7796_FRMCTR3 0xB3
#define ST7796_INVCTR  0xB4
#define ST7796_DFUNCTR 0xB6
#define ST7796_ENTRYMODE 0xB7
#define ST7796_PWCTR1  0xC0
#define ST7796_PWCTR2  0xC1
#define ST7796_PWCTR3  0xC2
#define ST7796_VMCTR1  0xC5
#define ST7796_VMCOFF  0xC6
#define ST7796_RDID4   0xD3
#define ST7796_GMCTRP1 0xE0
#define ST7796_GMCTRN1 0xE1
#define ST7796_DGAMMA1 0xE2
#define ST7796_DGAMMA2 0xE3
#define ST7796_DOCA    0xE8
#define ST7796_CSCON   0xF0
#define ST7796_MADCTL_MY  0x80
#define ST7796_MADCTL_MX  0x40
#define ST7796_MADCTL_MV  0x20
#define ST7796_MADCTL_ML  0x10
#define ST7796_MADCTL_RGB 0x00
#define ST7796_MADCTL_BGR 0x08
#define ST7796_MADCTL_MH  0x04

static inline void LCD_WR_CMD(unsigned int index,unsigned int val);
static inline unsigned int LCD_RD_CMD(unsigned int index);

#ifdef ILI9325_BITBANG
#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2))
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr))
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum))
#define GPIOA_ODR_Addr    (GPIOA_BASE+12) //0x4001080C
#define GPIOB_ODR_Addr    (GPIOB_BASE+12) //0x40010C0C
#define GPIOC_ODR_Addr    (GPIOC_BASE+12) //0x4001100C
#define GPIOD_ODR_Addr    (GPIOD_BASE+12) //0x4001140C
#define GPIOE_ODR_Addr    (GPIOE_BASE+12) //0x4001180C
#define GPIOF_ODR_Addr    (GPIOF_BASE+12) //0x40011A0C
#define GPIOG_ODR_Addr    (GPIOG_BASE+12) //0x40011E0C
#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)

#define LCD_CS  PCout(8)
#define LCD_RS  PCout(9)
#define LCD_WR  PCout(10)
#define LCD_RD  PCout(11)

static inline void LCD_WR_REG(unsigned int index) {
  LCD_CS = 0;
  LCD_RS = 0;
  GPIOC->ODR = (GPIOC->ODR&0xff00)|(index&0x00ff);
  GPIOB->ODR = (GPIOB->ODR&0x00ff)|(index&0xff00);
  LCD_WR = 0;
  LCD_WR = 1;
  LCD_CS = 1;
}

static inline unsigned int LCD_RD_Data(void) {
  uint16_t temp;

  GPIOB->CRH = (GPIOB->CRH & 0x00000000) | 0x44444444;
  GPIOC->CRL = (GPIOC->CRL & 0x00000000) | 0x44444444;
  LCD_CS = 0;
  LCD_RS = 1;
  LCD_RD = 0;
  temp = ((GPIOB->IDR&0xff00)|(GPIOC->IDR&0x00ff));
  LCD_RD = 1;
  LCD_CS = 1;
  GPIOB->CRH = (GPIOB->CRH & 0x00000000) | 0x33333333;
  GPIOC->CRL = (GPIOC->CRL & 0x00000000) | 0x33333333;

  return temp;
}

static inline void LCD_WR_Data(unsigned int val) {
  LCD_CS = 0;
  LCD_RS = 1;
  GPIOC->ODR = (GPIOC->ODR&0xff00)|(val&0x00ff);
  GPIOB->ODR = (GPIOB->ODR&0x00ff)|(val&0xff00);
  LCD_WR = 0;
  LCD_WR = 1;
  LCD_CS = 1;
}

static inline void LCD_WR_Data_multi(unsigned int val, unsigned int count) {
  LCD_CS = 0;
  LCD_RS = 1;
  GPIOC->ODR = (GPIOC->ODR&0xff00)|(val&0x00ff);
  GPIOB->ODR = (GPIOB->ODR&0x00ff)|(val&0xff00);
  unsigned int i;
  for (i=0;i<count;i++) {
    LCD_WR = 0;
    LCD_WR = 1;
  }
  LCD_CS = 1;
}

void LCD_init_hardware() {
  GPIO_InitTypeDef GPIO_InitStructure;
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC,ENABLE);

  /* ÅäÖÃÊýŸÝIO Á¬œÓµœGPIOB *********************/
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11
                              | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   // ÍÆÍìÊä³ö·œÊœ
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  // Êä³öIO¿Ú×îŽó×îËÙÎª50MHZ
  GPIO_Init(GPIOB, &GPIO_InitStructure);

  /* ÅäÖÃ¿ØÖÆIO Á¬œÓµœPD12.PD13.PD14.PD15 *********************/
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3
                              | GPIO_Pin_4 | GPIO_Pin_5 | GPIO_Pin_6 | GPIO_Pin_7
                              | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;   // ÍÆÍìÊä³ö·œÊœ
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  // Êä³öIO¿Ú×îŽó×îËÙÎª50MHZ
  GPIO_Init(GPIOC, &GPIO_InitStructure);
}

#elif defined(FSMC_BITBANG)

// bitbanged FSMC - because for some reason normal one seems unreliable on HYSTM32_32
// Nasty, slow, but easy to write + test
/*#define LCD_FSMC_RS JSH_PORTD_OFFSET+11
#define LCD_FSMC_RD JSH_PORTD_OFFSET+4
#define LCD_FSMC_WR JSH_PORTD_OFFSET+5
#define LCD_FSMC_CS JSH_PORTD_OFFSET+7
#define LCD_FSMC_D0 JSH_PORTD_OFFSET+14
#define LCD_FSMC_D1 JSH_PORTD_OFFSET+15
#define LCD_FSMC_D2 JSH_PORTD_OFFSET+0
#define LCD_FSMC_D3 JSH_PORTD_OFFSET+1
#define LCD_FSMC_D4 JSH_PORTE_OFFSET+7
#define LCD_FSMC_D5 JSH_PORTE_OFFSET+8
#define LCD_FSMC_D6 JSH_PORTE_OFFSET+9
#define LCD_FSMC_D7 JSH_PORTE_OFFSET+10
#define LCD_FSMC_D8 JSH_PORTE_OFFSET+11
#define LCD_FSMC_D9 JSH_PORTE_OFFSET+12
#define LCD_FSMC_D10 JSH_PORTE_OFFSET+13
#define LCD_FSMC_D11 JSH_PORTE_OFFSET+14
#define LCD_FSMC_D12 JSH_PORTE_OFFSET+15
#define LCD_FSMC_D13 JSH_PORTD_OFFSET+8
#define LCD_FSMC_D14 JSH_PORTD_OFFSET+9
#define LCD_FSMC_D15 JSH_PORTD_OFFSET+10*/


static void _LCD_WR(unsigned int d) {
  jshPinSetValue(LCD_FSMC_D0 , ((d>>0 )&1)!=0);
  jshPinSetValue(LCD_FSMC_D1 , ((d>>1 )&1)!=0);
  jshPinSetValue(LCD_FSMC_D2 , ((d>>2 )&1)!=0);
  jshPinSetValue(LCD_FSMC_D3 , ((d>>3 )&1)!=0);
  jshPinSetValue(LCD_FSMC_D4 , ((d>>4 )&1)!=0);
  jshPinSetValue(LCD_FSMC_D5 , ((d>>5 )&1)!=0);
  jshPinSetValue(LCD_FSMC_D6 , ((d>>6 )&1)!=0);
  jshPinSetValue(LCD_FSMC_D7 , ((d>>7 )&1)!=0);
  jshPinSetValue(LCD_FSMC_D8 , ((d>>8 )&1)!=0);
  jshPinSetValue(LCD_FSMC_D9 , ((d>>9 )&1)!=0);
  jshPinSetValue(LCD_FSMC_D10, ((d>>10)&1)!=0);
  jshPinSetValue(LCD_FSMC_D11, ((d>>11)&1)!=0);
  jshPinSetValue(LCD_FSMC_D12, ((d>>12)&1)!=0);
  jshPinSetValue(LCD_FSMC_D13, ((d>>13)&1)!=0);
  jshPinSetValue(LCD_FSMC_D14, ((d>>14)&1)!=0);
  jshPinSetValue(LCD_FSMC_D15, ((d>>15)&1)!=0);
}

static unsigned int _LCD_RD() {
  unsigned int d = 0;
  if (jshPinGetValue(LCD_FSMC_D0 )) d|=1<<0 ;
  if (jshPinGetValue(LCD_FSMC_D1 )) d|=1<<1 ;
  if (jshPinGetValue(LCD_FSMC_D2 )) d|=1<<2 ;
  if (jshPinGetValue(LCD_FSMC_D3 )) d|=1<<3 ;
  if (jshPinGetValue(LCD_FSMC_D4 )) d|=1<<4 ;
  if (jshPinGetValue(LCD_FSMC_D5 )) d|=1<<5 ;
  if (jshPinGetValue(LCD_FSMC_D6 )) d|=1<<6 ;
  if (jshPinGetValue(LCD_FSMC_D7 )) d|=1<<7 ;
  if (jshPinGetValue(LCD_FSMC_D8 )) d|=1<<8 ;
  if (jshPinGetValue(LCD_FSMC_D9 )) d|=1<<9 ;
  if (jshPinGetValue(LCD_FSMC_D10)) d|=1<<10;
  if (jshPinGetValue(LCD_FSMC_D11)) d|=1<<11;
  if (jshPinGetValue(LCD_FSMC_D12)) d|=1<<12;
  if (jshPinGetValue(LCD_FSMC_D13)) d|=1<<13;
  if (jshPinGetValue(LCD_FSMC_D14)) d|=1<<14;
  if (jshPinGetValue(LCD_FSMC_D15)) d|=1<<15;
  return d;
}

static void _LCD_STATE(JshPinState state) {
  jshPinSetState(LCD_FSMC_D0 , state);
  jshPinSetState(LCD_FSMC_D1 , state);
  jshPinSetState(LCD_FSMC_D2 , state);
  jshPinSetState(LCD_FSMC_D3 , state);
  jshPinSetState(LCD_FSMC_D4 , state);
  jshPinSetState(LCD_FSMC_D5 , state);
  jshPinSetState(LCD_FSMC_D6 , state);
  jshPinSetState(LCD_FSMC_D7 , state);
  jshPinSetState(LCD_FSMC_D8 , state);
  jshPinSetState(LCD_FSMC_D9 , state);
  jshPinSetState(LCD_FSMC_D10, state);
  jshPinSetState(LCD_FSMC_D11, state);
  jshPinSetState(LCD_FSMC_D12, state);
  jshPinSetState(LCD_FSMC_D13, state);
  jshPinSetState(LCD_FSMC_D14, state);
  jshPinSetState(LCD_FSMC_D15, state);
}

static inline void LCD_WR_REG(unsigned int index) {
  jshPinSetValue(LCD_FSMC_CS, 0);
  jshPinSetValue(LCD_FSMC_RS, 0);
  _LCD_WR(index);
  jshPinSetValue(LCD_FSMC_WR, 0);
  jshPinSetValue(LCD_FSMC_WR, 1);
  jshPinSetValue(LCD_FSMC_CS, 1);
}

static inline unsigned int LCD_RD_Data(void) {
  _LCD_STATE(JSHPINSTATE_GPIO_IN);
  jshPinSetValue(LCD_FSMC_CS, 0);
  jshPinSetValue(LCD_FSMC_RS, 1);
  jshPinSetValue(LCD_FSMC_RD, 0);
  uint16_t temp = (uint16_t)_LCD_RD();
  jshPinSetValue(LCD_FSMC_RD, 1);
  jshPinSetValue(LCD_FSMC_CS, 1);
  _LCD_STATE(JSHPINSTATE_GPIO_OUT);

  return temp;
}

static inline void LCD_WR_Data(unsigned int val) {
  jshPinSetValue(LCD_FSMC_CS, 0);
  jshPinSetValue(LCD_FSMC_RS, 1);
  _LCD_WR(val);
  jshPinSetValue(LCD_FSMC_WR, 0);
  jshPinSetValue(LCD_FSMC_WR, 1);
  jshPinSetValue(LCD_FSMC_CS, 1);
}

static inline void LCD_WR_Data_multi(unsigned int val, unsigned int count) {
  jshPinSetValue(LCD_FSMC_CS, 0);
  jshPinSetValue(LCD_FSMC_RS, 1);
  _LCD_WR(val);
  unsigned int i;
  for (i=0;i<count;i++) {
    jshPinSetValue(LCD_FSMC_WR, 0);
    jshPinSetValue(LCD_FSMC_WR, 1);
  }
  jshPinSetValue(LCD_FSMC_CS, 1);
}

void LCD_init_hardware() {
  jshPinSetState(LCD_FSMC_RS , JSHPINSTATE_GPIO_OUT);
  jshPinSetState(LCD_FSMC_RD , JSHPINSTATE_GPIO_OUT);
  jshPinSetState(LCD_FSMC_WR , JSHPINSTATE_GPIO_OUT);
  jshPinSetState(LCD_FSMC_CS , JSHPINSTATE_GPIO_OUT);
  _LCD_STATE(JSHPINSTATE_GPIO_OUT);

#ifdef LCD_BL
  jshPinSetState(LCD_BL, JSHPINSTATE_GPIO_OUT);
  jshPinSetValue(LCD_BL, 0); // BACKLIGHT=off -> reduce flicker when initing the LCD. We turn it on after LCD_init_panel
#endif

  // Toggle LCD reset pin
#ifdef LCD_RESET
  jshPinSetState(LCD_RESET, JSHPINSTATE_GPIO_OUT);
  jshPinSetValue(LCD_RESET, 0); //RESET=0
#endif
  delay_ms(50);
#ifdef LCD_RESET
  jshPinSetValue(LCD_RESET, 1); //RESET=1
#endif
}


#else

#if defined(HYSTM32_24)
  #define LCD_RESET (Pin)(JSH_PORTE_OFFSET + 1)
#elif defined(HYSTM32_32)
#elif defined(STM32F4LCD)
#elif defined(PIPBOY)
#else
  #error Unsupported board for ILI9325 LCD
#endif

#define LCD_REG              (*((volatile unsigned short *) 0x60000000)) /* RS = 0 */

#if (LCD_FSMC_RS == 59) // D11
  #define LCD_RAM              (*((volatile unsigned short *) 0x60020000)) /* RS = 1 (D11 -> A16) */
#elif (LCD_FSMC_RS == 61) // D13
  #define LCD_RAM              (*((volatile unsigned short *) 0x60080000)) /* RS = 1 (D13 -> A18) */
#else
  #error Unsupported LCD_FSMC_RS
#endif

static inline void LCD_WR_REG(unsigned int index) {
  LCD_REG = (uint16_t)index;
}

static inline unsigned int LCD_RD_Data(void) {
    return LCD_RAM;
}

static inline void LCD_WR_Data(unsigned int val) {
  LCD_RAM = (uint16_t)val;
}

static inline void LCD_WR_Data_multi(unsigned int val, unsigned int count) {
  int i;
  for (i=0;i<count;i++)
    LCD_RAM = (uint16_t)val;
}


void LCD_init_hardware() {
#ifdef LCD_BL
  jshPinSetState(LCD_BL, JSHPINSTATE_GPIO_OUT);
  jshPinSetValue(LCD_BL, 0); // BACKLIGHT=off -> reduce flicker when initing the LCD. We turn it on after LCD_init_panel
#endif

  delay_ms(100);
  // not sure why, but adding a delay here with the debugger means
  // that everything works great

  GPIO_InitTypeDef GPIO_InitStructure;
  memset(&GPIO_InitStructure, 0, sizeof(GPIO_InitStructure));

#ifdef STM32F4
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB|RCC_AHB1Periph_GPIOD|RCC_AHB1Periph_GPIOE|RCC_AHB1Periph_GPIOF|RCC_AHB1Periph_GPIOG, ENABLE);
	RCC_AHB3PeriphClockCmd(RCC_AHB3Periph_FSMC,ENABLE);
#else
  RCC_AHBPeriphClockCmd(RCC_AHBPeriph_FSMC, ENABLE); /* Enable the FSMC Clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC |
                   RCC_APB2Periph_GPIOD | RCC_APB2Periph_GPIOE , ENABLE);
#endif

  /* Enable the FSMC pins for LCD control */
#ifdef STM32F4
  GPIO_InitStructure.GPIO_Speed = GPIO_Low_Speed; // ST datasheet says "Low Speed" is OK at 8 MHz if VDD>2.7V and pin load capacitance < 10pF
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
#else
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
#endif
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5 |
                          GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_14 |
                          GPIO_Pin_15;
  GPIO_Init(GPIOD, &GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 |
                          GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13 | GPIO_Pin_14 |
                          GPIO_Pin_15;
  GPIO_Init(GPIOE, &GPIO_InitStructure);

  // Setup RS and CS (NE1) pins
  GPIO_InitStructure.GPIO_Pin = GPIO_BIT_FROM_PIN(LCD_FSMC_RS);
  GPIO_Init(GPIO_PORT_FROM_PIN(LCD_FSMC_RS), &GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Pin = GPIO_BIT_FROM_PIN(LCD_FSMC_CS);
  GPIO_Init(GPIO_PORT_FROM_PIN(LCD_FSMC_CS), &GPIO_InitStructure);

#ifdef STM32F4
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource0,GPIO_AF_FSMC);//PD0,AF12
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource1,GPIO_AF_FSMC);//PD1,AF12
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource4,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource5,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource8,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource9,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource10,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource14,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOD,GPIO_PinSource15,GPIO_AF_FSMC);//PD15,AF12

	GPIO_PinAFConfig(GPIOE,GPIO_PinSource7,GPIO_AF_FSMC);//PE7,AF12
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource8,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource9,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource10,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource11,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource12,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource13,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource14,GPIO_AF_FSMC);
	GPIO_PinAFConfig(GPIOE,GPIO_PinSource15,GPIO_AF_FSMC);//PE15,AF12

	GPIO_PinAFConfig(GPIO_PORT_FROM_PIN(LCD_FSMC_RS),LCD_FSMC_RS&0x0F,GPIO_AF_FSMC); // RS pin
	GPIO_PinAFConfig(GPIO_PORT_FROM_PIN(LCD_FSMC_CS),LCD_FSMC_CS&0x0F,GPIO_AF_FSMC); // CS pin

	FSMC_NORSRAMTimingInitTypeDef  readWriteTiming;
	FSMC_NORSRAMTimingInitTypeDef  writeTiming;

	readWriteTiming.FSMC_AddressSetupTime = 0XF;	 // Read address setup time (ADDSET) is 16 HCLKs (1/168 MHz) = 6ns*16 = 96ns
	readWriteTiming.FSMC_AddressHoldTime = 0x00;	 // Address hold time (ADDHLD) is not used in Mode A
	readWriteTiming.FSMC_DataSetupTime = 60;			 // Read data setup time is 60 HCLKs = 6ns*60 = 360ns
	readWriteTiming.FSMC_BusTurnAroundDuration = 0x00;
	readWriteTiming.FSMC_CLKDivision = 0x00;
	readWriteTiming.FSMC_DataLatency = 0x00;
	readWriteTiming.FSMC_AccessMode = FSMC_AccessMode_A;	 // Mode A


	writeTiming.FSMC_AddressSetupTime =9;	      // Write address setup time (ADDSET) is 9 HCLKs = 54ns
	writeTiming.FSMC_AddressHoldTime = 0x00;    // Address hold time (ADDHLD) is not used in Mode A
	writeTiming.FSMC_DataSetupTime = 8;		      // Write data setup time is 9 HCLKs = 6ns*9 = 54ns
	writeTiming.FSMC_BusTurnAroundDuration = 0x00;
	writeTiming.FSMC_CLKDivision = 0x00;
	writeTiming.FSMC_DataLatency = 0x00;
	writeTiming.FSMC_AccessMode = FSMC_AccessMode_A;	 // Mode A

  FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
  FSMC_NORSRAMStructInit(&FSMC_NORSRAMInitStructure);
	FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM1;  // Use NE4, which corresponds to BTCR[6],[7]
	FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable; // Do not reuse data addresses
	FSMC_NORSRAMInitStructure.FSMC_MemoryType =FSMC_MemoryType_SRAM;
	FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b; // 16-bit data width
	FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode =FSMC_BurstAccessMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
	FSMC_NORSRAMInitStructure.FSMC_AsynchronousWait=FSMC_AsynchronousWait_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
	FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
	FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Enable; // Use different timings for reading and writing
	FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
	FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &readWriteTiming;
	FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &writeTiming;

	FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);  // Initialize FSMC with the FSMC_InitStruct that we just filled out
#else

  FSMC_NORSRAMInitTypeDef  FSMC_NORSRAMInitStructure;
  FSMC_NORSRAMStructInit(&FSMC_NORSRAMInitStructure);
  FSMC_NORSRAMTimingInitTypeDef  p;
  p.FSMC_AddressSetupTime = 0x02;
  p.FSMC_AddressHoldTime = 0x00;
  p.FSMC_DataSetupTime = 0x05;
  p.FSMC_BusTurnAroundDuration = 0x00;
  p.FSMC_CLKDivision = 0x00;
  p.FSMC_DataLatency = 0x00;
  p.FSMC_AccessMode = FSMC_AccessMode_B;

  FSMC_NORSRAMInitStructure.FSMC_MemoryType = FSMC_MemoryType_NOR;
  FSMC_NORSRAMInitStructure.FSMC_Bank = FSMC_Bank1_NORSRAM1;
  FSMC_NORSRAMInitStructure.FSMC_DataAddressMux = FSMC_DataAddressMux_Disable;
  FSMC_NORSRAMInitStructure.FSMC_MemoryDataWidth = FSMC_MemoryDataWidth_16b;
  FSMC_NORSRAMInitStructure.FSMC_BurstAccessMode = FSMC_BurstAccessMode_Disable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignalPolarity = FSMC_WaitSignalPolarity_Low;
  FSMC_NORSRAMInitStructure.FSMC_WrapMode = FSMC_WrapMode_Disable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignalActive = FSMC_WaitSignalActive_BeforeWaitState;
  FSMC_NORSRAMInitStructure.FSMC_WriteOperation = FSMC_WriteOperation_Enable;
  FSMC_NORSRAMInitStructure.FSMC_WaitSignal = FSMC_WaitSignal_Disable;
  FSMC_NORSRAMInitStructure.FSMC_ExtendedMode = FSMC_ExtendedMode_Disable;
  FSMC_NORSRAMInitStructure.FSMC_WriteBurst = FSMC_WriteBurst_Disable;
  FSMC_NORSRAMInitStructure.FSMC_ReadWriteTimingStruct = &p;
  FSMC_NORSRAMInitStructure.FSMC_WriteTimingStruct = &p;
  FSMC_NORSRAMInit(&FSMC_NORSRAMInitStructure);
#endif

  /* Enable FSMC Bank1_SRAM Bank */
  FSMC_NORSRAMCmd(FSMC_Bank1_NORSRAM1, ENABLE);

  // Toggle LCD reset pin
#ifdef LCD_RESET
  jshPinSetState(LCD_RESET, JSHPINSTATE_GPIO_OUT);
  jshPinSetValue(LCD_RESET, 0); //RESET=0
  delay_ms(50);
  jshPinSetValue(LCD_RESET, 1); //RESET=1
#endif
  delay_ms(50);
}

#endif // NOT ILI9325_BITBANG

static inline void LCD_WR_CMD(unsigned int index,unsigned int val) {
  LCD_WR_REG(index);
  LCD_WR_Data(val);
}

static inline void LCD_WR_CMD2(unsigned int index,unsigned int val1, unsigned int val2) {
  LCD_WR_REG(index);
  LCD_WR_Data(val1);
  LCD_WR_Data(val2);
}

static inline void LCD_WR_CMD4(unsigned int index,unsigned int val1, unsigned int val2, unsigned int val3, unsigned int val4) {
  LCD_WR_REG(index);
  LCD_WR_Data(val1);
  LCD_WR_Data(val2);
  LCD_WR_Data(val3);
  LCD_WR_Data(val4);
}

static inline unsigned int LCD_RD_CMD(unsigned int index) {
  LCD_WR_REG(index);
  return LCD_RD_Data();
}

// LEVEL 1 register control
//#define  ILI_NOP             0x00       // No Operation - NOP
#define   ILI_SWRESET         0x01       // Software Reset - SWRESET
//#define  ILI_RDDIDIF         0x04       // Read Display Identification Information (odczytuje dummy byte + 3 bajty informacji)
//#define  ILI_RDDST           0x09       // Read Display Status (odczytuje dummy byte + 4 bajty informacji)
//#define  ILI_RDDPM           0x0A       // Read Display Power Mode (odczytuje dummy byte + 1 bajt informacji)
//#define  ILI_RDDMADCTL       0x0B       // Read Display MADCTL (odczytuje dummy byte + 1 bajt informacji)
//#define  ILI_RDDCOLMOD       0x0C       // Read Display Pixel Format (odczytuje dummy byte + 1 bajt informacji)
//#define  ILI_RDDIM           0x0D       // Read Display Image Format (odczytuje dummy byte + 1 bajt informacji)
//#define  ILI_RDDSM           0x0E       // Read Display Signal Mode (odczytuje dummy byte + 1 bajt informacji)
//#define  ILI_RDDSDR          0x0F       // Read Display SELF-Diagnostic Result (odczytuje dummy byte + 1 bajt informacji)
//#define  ILI_SLPIN           0x10       // Enter Sleep Mode
#define   ILI_SLPOUT          0x11       // Sleep Out
//#define  ILI_PTLON           0x12       // Partial Mode ON
//#define  ILI_NORON           0x13       // Normal Display Mode ON
//#define  ILI_DINVOFF         0x20       // Display Inversion OFF
//#define  ILI_DINVON          0x21       // Display Inversion ON
#define   ILI_GAMSET          0x26       // Gamma Set (1 parametr 8b)
#define   ILI_DISPOFF         0x28       // Display OFF
#define   ILI_DISPON          0x29       // Display ON
#define   ILI_CASET           0x2A       // Column Address Set (4 parametry 8b: SC[15..8], SC[7..0], EC[15..8], EC[7..0])
#define   ILI_PASET           0x2B       // Page (row) Address Set (4 parametry 8b: SC[15..8], SC[7..0], EC[15..8], EC[7..0])
#define   ILI_RAMWR           0x2C       // Memory Write (n parametr體 18b, wywo硑wana bez parametr體 po CASET i PASET)
//#define  ILI_RGBSET          0x2D       // Color Set (128 parametr體 8b, 32 dla R, 64 dla G i 32 dla B)
//#define  ILI_RAMRD           0x2E       // Memory Read (n parametr體 18b)
//#define  ILI_PLTAR           0x30       // Partial Area (4 parametry 8b: SR[15..8], SR[7..0], ER[15..8], ER[7..0])
//#define  ILI_VSCRDEF         0x33       // Vertical Scrolling Definition (6 parametr體 8b)
//#define  ILI_TEOFF           0x34       // Tearing Effect Line OFF
//#define  ILI_TEON            0x35       // Tearing Effect Line ON (1 parametr 8b)
#define   ILI_MADCTL          0x36       // Memory Access Control (1 parametr 8b)
//#define  ILI_VSCRSADD        0x37       // Vertical Scrolling Start Address (2 parametry 8b)
#define   ILI_IDMOFF          0x38       // Idle Mode OFF
#define   ILI_IDMON           0x39       // Idle Mode ON
#define   ILI_PIXSET          0x3A       // COLMOD: Pixel Format Set (1 parametr 8b)
#define   ILI_RAMWRCont       0x3C       // Write Memory Continue (n parametr體 18b)
//#define  ILI_RAMRDCont       0x3E       // Read Memory Continue (odczytuje dummy byte i n danych 18b)
//#define  ILI_STS             0x44       // Set Tear Scanline (2 parametry 8b)
//#define  ILI_GS              0x45       // Get Scanline (odczytuje dummy i 2 bajty informacji)
//#define  ILI_WRDISBV         0x51       // Write Display Brightness (1 parametr 8b)
//#define  ILI_RDDISBV         0x52       // Read Display Brightness (odczytuje dummy byte i 1 bajt informacji)
//#define  ILI_WRCTRLD         0x53       // Write CTRL Display (1 parametr 8b)
//#define  ILI_RDCTRLD         0x54       // Read CTRL Display (odczytuje dummy byte i 1 bajt informacji)
//#define  ILI_WRCABC          0x55       // Write Content Adaptive Brightness Control (1 parametr 8b)
//#define  ILI_RDCABC          0x56       // Read Content Adaptive Brightness Control (odczytuje dummy byte i 1 bajt informacji)
//#define  ILI_WRCABCMB        0x5E       // Write CABC Minimum Brightness (1 parametr 8b)
//#define  ILI_RDCABCMB        0x5F       // Read CABC Minimum Brightness (odczytuje dummy byte i 1 bajt informacji)
//#define  ILI_RDID1           0xDA       // Read ID1 (odczytuje dummy byte i 1 bajt informacji)
//#define  ILI_RDID2           0xDB       // Read ID2 (odczytuje dummy byte i 1 bajt informacji)
//#define  ILI_RDID3           0xDC       // Read ID3 (odczytuje dummy byte i 1 bajt informacji)
// LEVEL 2 register control
//#define  ILI_IFMODE          0xB0       // RGB  Interface Signal Control (1 parametr 8b)
#define   ILI_FRMCTR1         0xB1       // Frame Rate Control (In Normal Mode/Full Colors) (2 parametry 8b)
//#define  ILI_FRMCTR2         0xB2       // Frame Rate Control (In Idle Mode/8 Colors) (2 parametry 8b)
//#define  ILI_FRMCTR3         0xB3       // Frame Rate Control (In Partial Mode/Full Colors) (2 parametry 8b)
//#define  ILI_INVTR           0xB4       // Display Inversion Control (1 parametr 8b)
//#define  ILI_PRCTR           0xB5       // Blanking Porch Control (4 parametry 8b)
#define   ILI_DISCTRL         0xB6       // Display Function Control (4 parametry 8b)
//#define  ILI_ETMOD           0xB7       // Entry Mode Set (1 parametr 8b)
//#define  ILI_BLCTRL1         0xB8       // Backlight Control 1 (1 parametr 8b)
//#define  ILI_BLCTRL2         0xB9       // Backlight Control 2 (1 parametr 8b)
//#define  ILI_BLCTRL3         0xBA       // Backlight Control 3 (1 parametr 8b)
//#define  ILI_BLCTRL4         0xBB       // Backlight Control 4 (1 parametr 8b)
//#define  ILI_BLCTRL5         0xBC       // Backlight Control 5 (1 parametr 8b)
//#define  ILI_BLCTRL7         0xBE       // Backlight Control 7 (1 parametr 8b)
//#define  ILI_BLCTRL8         0xBF       // Backlight Control 8 (1 parametr 8b)
#define   ILI_PWCTRL1         0xC0       // Power Control 1 (1 parametr 8b)
#define   ILI_PWCTRL2         0xC1       // Power Control 2 (1 parametr 8b)
#define   ILI_VMCTRL1         0xC5       // VCOM Control 1 (2 parametry 8b)
#define   ILI_VMCTRL2         0xC7       // VCOM Control 2 (1 parametr 8b)
//#define  ILI_NVMWR           0xD0       // NV Memory Write (2 parametry 8b)
//#define  ILI_NVMPKEY         0xD1       // NV Memory Protection Key (3 parametry 8b)
//#define  ILI_RDNVM           0xD2       // NV Memory Status Read (odczyt dummy byte i 2 bajty informacji)
//#define  ILI_RDID4           0xD3       // Read ID4 (odczytuje dummy byte i 3 bajty informacji)
#define   ILI_PGAMCTRL        0xE0       // Positive Gamma Correction (15 parametr體 8b)
#define   ILI_NGAMCTRL        0xE1       // Negative Gamma Correction (15 parametr體 8b)
//#define  ILI_DGAMCTRL1       0xE2       // Digital Gamma Control 1 (16 parametr體 8b)
//#define  ILI_DGAMCTRL2       0xE3       // Digital Gamma Control 2 (16 parametr體 8b)
//#define  ILI_IFCTL           0xF6       // Interface Control (3 parametry 8b)
// EXTEND register control
#define   ILI_PCA             0xCB       // Power Control A (5 parametr體 8b)
#define   ILI_PCB             0xCF       // Power Control B (3 parametry 8b)
#define   ILI_DTCA_ic         0xE8       // Driver Timming Control A (3 parametry 8b) - for internal clock
//#define  ILI_DTCA_ec         0xE9       // Driver Timming Control A (3 parametry 8b) - for external clock
#define   ILI_DTCB            0xEA       // Driver Timming Control B (2 parametry 8b)
#define   ILI_POSC            0xED       // Power On Sequence Control (4 parametry 8b)
#define   ILI_E3G             0xF2       // Enable 3G (1 parametr 8b)
#define   ILI_PRC             0xF7       // Pump Ratio Control (1 parametr 8b)

uint16_t LCD_read_ID(unsigned int regId, bool ignoreTwoBytes) {
  uint16_t id;
  LCD_WR_REG(regId);
  if (ignoreTwoBytes) {
    id = LCD_RD_Data(); // Dummy read
    id = LCD_RD_Data(); // 2nd byte might contain something, but we'll ignore it
  }
  id = LCD_RD_Data() << 8; // Keep these two bytes
  id |= LCD_RD_Data();
  return id;
}

void LCD_init_panel() {
  uint16_t DeviceCode;
  uint8_t *p;
  DeviceCode = LCD_read_ID(0xD3, true);   // Read ID4 from register 0xD3 (which is where it's located for ILI9341, ST7796, ILI9806, etc.)
  // jsiConsolePrintf("LCD DeviceCode from 0xD3: 0x%04x\n", DeviceCode);
  if (DeviceCode == 0x9341 || DeviceCode == 0x7796) {
		// Reconfigure the FSMC write timing control register
		FSMC_Bank1E->BWTR[6]&=~(0XF<<0); // Clear address setup time (ADDSET)
		FSMC_Bank1E->BWTR[6]&=~(0XF<<8); // Clear data setup time (DATAST)
		FSMC_Bank1E->BWTR[6]|=4<<0;      // Set address setup time (ADDSET) to 3 HCLKs = 18ns
    FSMC_Bank1E->BWTR[6]|=4<<8;      // Set data setup time (DATASET) to 3 HCLKs = 18ns

    if (DeviceCode == 0x9341) { // ILI9341
      // jsiConsolePrintf("Configuring ILI9341\n");
      LCD_Code = ILI9341;
      static const uint8_t init_tab[] = {
        ILI_PCB, 3, 0x00, 0xC1, 0X30,  \
        ILI_POSC, 4, 0x64, 0x03, 0X12, 0X81,  \
        ILI_DTCA_ic, 3, 0x85, 0x10, 0x7A,  \
        ILI_PCA, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,  \
        ILI_PRC,1, 0x20,  \
        ILI_DTCB, 2, 0x00, 0x00,  \
        ILI_PWCTRL1, 1, 0x1B,  \
        ILI_PWCTRL2,1, 0x01,  \
        ILI_VMCTRL1, 2, 0x30, 0x30,  \
        ILI_VMCTRL2, 1, 0XB7,  \
        ILI_MADCTL, 1, 0x48,  \
        ILI_PIXSET, 1, 0x55,  \
        ILI_FRMCTR1, 2, 0x00, 0x1A,  \
        ILI_DISCTRL, 2, 0x0A, 0xA2,  \
        ILI_E3G, 1, 0x00,  \
        ILI_GAMSET, 1, 0x01,  \
        ILI_PGAMCTRL, 15, 0x0F, 0x2A, 0x28, 0x08, 0x0E, 0x08, 0x54, 0XA9, 0x43, 0x0A, 0x0F, 0x00, 0x00, 0x00, 0x00,  \
        ILI_NGAMCTRL, 15, 0x00, 0x15, 0x17, 0x07, 0x11, 0x06, 0x2B, 0x56, 0x3C, 0x05, 0x10, 0x0F, 0x3F, 0x3F, 0x0F,  \
        ILI_PASET, 4, 0x00, 0x00, 0x01, 0x3f,  \
        ILI_CASET, 4, 0x00, 0x00, 0x00, 0xef,  \
        ILI_SLPOUT, 120,  \
        ILI_DISPON, 0,  \
        ILI_MADCTL, 1, 0x68,  \
        0
      };
      p = init_tab;
    } else if (DeviceCode == 0x7796) { // ST7796
      // jsiConsolePrintf("Configuring ST7796\n");
      LCD_Code = ST7796;
      static const uint8_t init_tab[] = {
        ST7796_CSCON, 1, 0xC3, \
        ST7796_CSCON, 1, 0x96, \
        ST7796_MADCTL, 1, 0x28, \
        ST7796_PIXFMT, 1, 0x55, \
        ST7796_INVCTR, 1, 0x01, \
        ST7796_ENTRYMODE, 1, 0xC6, \
        ST7796_DOCA, 8, 0x40, 0x8A, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33, \
        ST7796_PWCTR2, 1, 0x06, \
        ST7796_PWCTR3, 1, 0xA7, \
        ST7796_VMCTR1, 1, 0x18, \
        ST7796_GMCTRP1, 14, 0xF0, 0x09, 0x0B, 0x06, 0x04, 0x15, 0x2F, 0x54, 0x42, 0x3C, 0x17, 0x14, 0x18, 0x1B, \
        ST7796_GMCTRN1, 14, 0xF0, 0x09, 0x0B, 0x06, 0x04, 0x03, 0x2D, 0x43, 0x42, 0x3B, 0x16, 0x14, 0x17, 0x1B, \
        ST7796_CSCON, 1, 0x3C, \
        ST7796_CSCON, 1, 0x69, \
        ST7796_SLPOUT, 120, \
        ST7796_DISPON, 0, \
        0
      };
      p = init_tab;
    }
    while (*p) {
      LCD_WR_REG(*p);
      p++;
      int c = *p;
      p++;
      if (c>15) delay_ms(c);
      else while (c--) {
        LCD_WR_Data(*p);
        p++;
      }
    }
  } else {
    // Not ID4 from 0xD3, so try reading ID2 from register 0x00 (for ILI9320, ILI9325, ILI9331, SSD2119, SSD1298, R61505U, SPFD5408B, LGDP4531, HX8347D, ST7781, etc.)
    DeviceCode = LCD_read_ID(0x00, false);

    if (DeviceCode == 0x4532) { // For the 2.4" LCD boards
      LCD_Code = ILI9325;
      LCD_WR_CMD(0x0000,0x0001);
      LCD_DELAY(DELAY_SHORT);

      LCD_WR_CMD(0x0015,0x0030);
      LCD_WR_CMD(0x0011,0x0040);
      LCD_WR_CMD(0x0010,0x1628);
      LCD_WR_CMD(0x0012,0x0000);
      LCD_WR_CMD(0x0013,0x104d);
      LCD_DELAY(DELAY_SHORT);
      LCD_WR_CMD(0x0012,0x0010);
      LCD_DELAY(DELAY_SHORT);
      LCD_WR_CMD(0x0010,0x2620);
      LCD_WR_CMD(0x0013,0x344d); //304d
      LCD_DELAY(DELAY_SHORT);

      LCD_WR_CMD(0x0001,0x0100);
      LCD_WR_CMD(0x0002,0x0300);
      LCD_WR_CMD(0x0003,0x1030); // ORG is 0
      LCD_WR_CMD(0x0008,0x0604);
      LCD_WR_CMD(0x0009,0x0000);
      LCD_WR_CMD(0x000A,0x0008);

      LCD_WR_CMD(0x0041,0x0002);
      LCD_WR_CMD(0x0060,0x2700);
      LCD_WR_CMD(0x0061,0x0001);
      LCD_WR_CMD(0x0090,0x0182);
      LCD_WR_CMD(0x0093,0x0001);
      LCD_WR_CMD(0x00a3,0x0010);
      LCD_DELAY(DELAY_SHORT);

      //################# void Gamma_Set(void) ####################//
          LCD_WR_CMD(0x30,0x0000);
          LCD_WR_CMD(0x31,0x0502);
          LCD_WR_CMD(0x32,0x0307);
          LCD_WR_CMD(0x33,0x0305);
          LCD_WR_CMD(0x34,0x0004);
          LCD_WR_CMD(0x35,0x0402);
          LCD_WR_CMD(0x36,0x0707);
          LCD_WR_CMD(0x37,0x0503);
          LCD_WR_CMD(0x38,0x1505);
          LCD_WR_CMD(0x39,0x1505);
          LCD_DELAY(DELAY_SHORT);

          //################## void Display_ON(void) ####################//
          LCD_WR_CMD(0x0007,0x0001);
          LCD_DELAY(DELAY_SHORT);
          LCD_WR_CMD(0x0007,0x0021);
          LCD_WR_CMD(0x0007,0x0023);
          LCD_DELAY(DELAY_SHORT);
          LCD_WR_CMD(0x0007,0x0033);
          LCD_DELAY(DELAY_SHORT);
          LCD_WR_CMD(0x0007,0x0133);
    }
    else if( DeviceCode == 0x9325 || DeviceCode == 0x9328 )
    {
      LCD_Code = ILI9325;
      LCD_WR_CMD(0x00e7,0x0010);
      LCD_WR_CMD(0x0000,0x0001);  	/* start internal osc */
      LCD_WR_CMD(0x0001,0x0100);
      LCD_WR_CMD(0x0002,0x0700); 	/* power on sequence */
      LCD_WR_CMD(0x0003,(1<<12)|(1<<5)|(1<<4)|(0<<3) ); 	/* importance */
      LCD_WR_CMD(0x0004,0x0000);
      LCD_WR_CMD(0x0008,0x0207);
      LCD_WR_CMD(0x0009,0x0000);
      LCD_WR_CMD(0x000a,0x0000); 	/* display setting */
      LCD_WR_CMD(0x000c,0x0001);	/* display setting */
      LCD_WR_CMD(0x000d,0x0000);
      LCD_WR_CMD(0x000f,0x0000);
      /* Power On sequence */
      LCD_WR_CMD(0x0010,0x0000);
      LCD_WR_CMD(0x0011,0x0007);
      LCD_WR_CMD(0x0012,0x0000);
      LCD_WR_CMD(0x0013,0x0000);
      delay_ms(50);  /* delay 50 ms */
      LCD_WR_CMD(0x0010,0x1590);
      LCD_WR_CMD(0x0011,0x0227);
      delay_ms(50);  /* delay 50 ms */
      LCD_WR_CMD(0x0012,0x009c);
      delay_ms(50);  /* delay 50 ms */
      LCD_WR_CMD(0x0013,0x1900);
      LCD_WR_CMD(0x0029,0x0023);
      LCD_WR_CMD(0x002b,0x000e);
      delay_ms(50);  /* delay 50 ms */
      LCD_WR_CMD(0x0020,0x0000);
      LCD_WR_CMD(0x0021,0x0000);
      delay_ms(50);  /* delay 50 ms */
      LCD_WR_CMD(0x0030,0x0007);
      LCD_WR_CMD(0x0031,0x0707);
      LCD_WR_CMD(0x0032,0x0006);
      LCD_WR_CMD(0x0035,0x0704);
      LCD_WR_CMD(0x0036,0x1f04);
      LCD_WR_CMD(0x0037,0x0004);
      LCD_WR_CMD(0x0038,0x0000);
      LCD_WR_CMD(0x0039,0x0706);
      LCD_WR_CMD(0x003c,0x0701);
      LCD_WR_CMD(0x003d,0x000f);
      delay_ms(50);  /* delay 50 ms */
      LCD_WR_CMD(0x0050,0x0000);
      LCD_WR_CMD(0x0051,0x00ef);
      LCD_WR_CMD(0x0052,0x0000);
      LCD_WR_CMD(0x0053,0x013f);
      LCD_WR_CMD(0x0060,0xa700);
      LCD_WR_CMD(0x0061,0x0001);
      LCD_WR_CMD(0x006a,0x0000);
      LCD_WR_CMD(0x0080,0x0000);
      LCD_WR_CMD(0x0081,0x0000);
      LCD_WR_CMD(0x0082,0x0000);
      LCD_WR_CMD(0x0083,0x0000);
      LCD_WR_CMD(0x0084,0x0000);
      LCD_WR_CMD(0x0085,0x0000);

      LCD_WR_CMD(0x0090,0x0010);
      LCD_WR_CMD(0x0092,0x0000);
      LCD_WR_CMD(0x0093,0x0003);
      LCD_WR_CMD(0x0095,0x0110);
      LCD_WR_CMD(0x0097,0x0000);
      LCD_WR_CMD(0x0098,0x0000);
      /* display on sequence */
      LCD_WR_CMD(0x0007,0x0133);

      LCD_WR_CMD(0x0020,0x0000);  /* ÐÐÊ×Ö·0 */
      LCD_WR_CMD(0x0021,0x0000);  /* ÁÐÊ×Ö·0 */
    }
    else if( DeviceCode == 0x9320 || DeviceCode == 0x9300 )
    {
      LCD_Code = ILI9320;
      LCD_WR_CMD(0x00,0x0000);
      LCD_WR_CMD(0x01,0x0100);	/* Driver Output Contral */
      LCD_WR_CMD(0x02,0x0700);	/* LCD Driver Waveform Contral */
      LCD_WR_CMD(0x03,0x1018);	/* Entry Mode Set */

      LCD_WR_CMD(0x04,0x0000);	/* Scalling Contral */
      LCD_WR_CMD(0x08,0x0202);	/* Display Contral */
      LCD_WR_CMD(0x09,0x0000);	/* Display Contral 3.(0x0000) */
      LCD_WR_CMD(0x0a,0x0000);	/* Frame Cycle Contal.(0x0000) */
      LCD_WR_CMD(0x0c,(1<<0));	/* Extern Display Interface Contral */
      LCD_WR_CMD(0x0d,0x0000);	/* Frame Maker Position */
      LCD_WR_CMD(0x0f,0x0000);	/* Extern Display Interface Contral 2. */

      delay_ms(100);  /* delay 100 ms */
      LCD_WR_CMD(0x07,0x0101);	/* Display Contral */
      delay_ms(100);  /* delay 100 ms */

      LCD_WR_CMD(0x10,(1<<12)|(0<<8)|(1<<7)|(1<<6)|(0<<4));	/* Power Control 1.(0x16b0)	*/
      LCD_WR_CMD(0x11,0x0007);								/* Power Control 2 */
      LCD_WR_CMD(0x12,(1<<8)|(1<<4)|(0<<0));				/* Power Control 3.(0x0138)	*/
      LCD_WR_CMD(0x13,0x0b00);								/* Power Control 4 */
      LCD_WR_CMD(0x29,0x0000);								/* Power Control 7 */

      LCD_WR_CMD(0x2b,(1<<14)|(1<<4));

      LCD_WR_CMD(0x50,0);       /* Set X Start */
      LCD_WR_CMD(0x51,239);	    /* Set X End */
      LCD_WR_CMD(0x52,0);	    /* Set Y Start */
      LCD_WR_CMD(0x53,319);	    /* Set Y End */

      LCD_WR_CMD(0x60,0x2700);	/* Driver Output Control */
      LCD_WR_CMD(0x61,0x0001);	/* Driver Output Control */
      LCD_WR_CMD(0x6a,0x0000);	/* Vertical Srcoll Control */

      LCD_WR_CMD(0x80,0x0000);	/* Display Position? Partial Display 1 */
      LCD_WR_CMD(0x81,0x0000);	/* RAM Address Start? Partial Display 1 */
      LCD_WR_CMD(0x82,0x0000);	/* RAM Address End-Partial Display 1 */
      LCD_WR_CMD(0x83,0x0000);	/* Displsy Position? Partial Display 2 */
      LCD_WR_CMD(0x84,0x0000);	/* RAM Address Start? Partial Display 2 */
      LCD_WR_CMD(0x85,0x0000);	/* RAM Address End? Partial Display 2 */

      LCD_WR_CMD(0x90,(0<<7)|(16<<0));	/* Frame Cycle Contral.(0x0013)	*/
      LCD_WR_CMD(0x92,0x0000);	/* Panel Interface Contral 2.(0x0000) */
      LCD_WR_CMD(0x93,0x0001);	/* Panel Interface Contral 3. */
      LCD_WR_CMD(0x95,0x0110);	/* Frame Cycle Contral.(0x0110)	*/
      LCD_WR_CMD(0x97,(0<<8));
      LCD_WR_CMD(0x98,0x0000);	/* Frame Cycle Contral */

      LCD_WR_CMD(0x07,0x0173);
    }
  #ifndef SAVE_ON_FLASH
    else if( DeviceCode == 0x9331  )
    {
      LCD_Code = ILI9331;
      LCD_WR_CMD(0x00E7, 0x1014);
      LCD_WR_CMD(0x0001, 0x0100);   /* set SS and SM bit */
      LCD_WR_CMD(0x0002, 0x0200);   /* set 1 line inversion */
      LCD_WR_CMD(0x0003, 0x1030);   /* set GRAM write direction and BGR=1 */
      LCD_WR_CMD(0x0008, 0x0202);   /* set the back porch and front porch */
      LCD_WR_CMD(0x0009, 0x0000);   /* set non-display area refresh cycle ISC[3:0] */
      LCD_WR_CMD(0x000A, 0x0000);   /* FMARK function */
      LCD_WR_CMD(0x000C, 0x0000);   /* RGB interface setting */
      LCD_WR_CMD(0x000D, 0x0000);   /* Frame marker Position */
      LCD_WR_CMD(0x000F, 0x0000);   /* RGB interface polarity */
      /* Power On sequence */
      LCD_WR_CMD(0x0010, 0x0000);   /* SAP, BT[3:0], AP, DSTB, SLP, STB	*/
      LCD_WR_CMD(0x0011, 0x0007);   /* DC1[2:0], DC0[2:0], VC[2:0] */
      LCD_WR_CMD(0x0012, 0x0000);   /* VREG1OUT voltage	*/
      LCD_WR_CMD(0x0013, 0x0000);   /* VDV[4:0] for VCOM amplitude */
      delay_ms(200);  /* delay 200 ms */
      LCD_WR_CMD(0x0010, 0x1690);   /* SAP, BT[3:0], AP, DSTB, SLP, STB	*/
      LCD_WR_CMD(0x0011, 0x0227);   /* DC1[2:0], DC0[2:0], VC[2:0] */
      delay_ms(50);  /* delay 50 ms */
      LCD_WR_CMD(0x0012, 0x000C);   /* Internal reference voltage= Vci	*/
      delay_ms(50);  /* delay 50 ms */
      LCD_WR_CMD(0x0013, 0x0800);   /* Set VDV[4:0] for VCOM amplitude */
      LCD_WR_CMD(0x0029, 0x0011);   /* Set VCM[5:0] for VCOMH */
      LCD_WR_CMD(0x002B, 0x000B);   /* Set Frame Rate */
      delay_ms(50);  /* delay 50 ms */
      LCD_WR_CMD(0x0020, 0x0000);   /* GRAM horizontal Address */
      LCD_WR_CMD(0x0021, 0x0000);   /* GRAM Vertical Address */
      /* Adjust the Gamma Curve */
      LCD_WR_CMD(0x0030, 0x0000);
      LCD_WR_CMD(0x0031, 0x0106);
      LCD_WR_CMD(0x0032, 0x0000);
      LCD_WR_CMD(0x0035, 0x0204);
      LCD_WR_CMD(0x0036, 0x160A);
      LCD_WR_CMD(0x0037, 0x0707);
      LCD_WR_CMD(0x0038, 0x0106);
      LCD_WR_CMD(0x0039, 0x0707);
      LCD_WR_CMD(0x003C, 0x0402);
      LCD_WR_CMD(0x003D, 0x0C0F);
      /* Set GRAM area */
      LCD_WR_CMD(0x0050, 0x0000);   /* Horizontal GRAM Start Address */
      LCD_WR_CMD(0x0051, 0x00EF);   /* Horizontal GRAM End Address */
      LCD_WR_CMD(0x0052, 0x0000);   /* Vertical GRAM Start Address */
      LCD_WR_CMD(0x0053, 0x013F);   /* Vertical GRAM Start Address */
      LCD_WR_CMD(0x0060, 0x2700);   /* Gate Scan Line */
      LCD_WR_CMD(0x0061, 0x0001);   /*  NDL,VLE, REV */
      LCD_WR_CMD(0x006A, 0x0000);   /* set scrolling line */
      /* Partial Display Control */
      LCD_WR_CMD(0x0080, 0x0000);
      LCD_WR_CMD(0x0081, 0x0000);
      LCD_WR_CMD(0x0082, 0x0000);
      LCD_WR_CMD(0x0083, 0x0000);
      LCD_WR_CMD(0x0084, 0x0000);
      LCD_WR_CMD(0x0085, 0x0000);
      /* Panel Control */
      LCD_WR_CMD(0x0090, 0x0010);
      LCD_WR_CMD(0x0092, 0x0600);
      LCD_WR_CMD(0x0007,0x0021);
      delay_ms(50);  /* delay 50 ms */
      LCD_WR_CMD(0x0007,0x0061);
      delay_ms(50);  /* delay 50 ms */
      LCD_WR_CMD(0x0007,0x0133);    /* 262K color and display ON */
    }
    else if( DeviceCode == 0x9919 )
    {
      LCD_Code = SSD2119;
      /* POWER ON &RESET DISPLAY OFF */
      LCD_WR_CMD(0x28,0x0006);
      LCD_WR_CMD(0x00,0x0001);
      LCD_WR_CMD(0x10,0x0000);
      LCD_WR_CMD(0x01,0x72ef);
      LCD_WR_CMD(0x02,0x0600);
      LCD_WR_CMD(0x03,0x6a38);
      LCD_WR_CMD(0x11,0x6874);
      LCD_WR_CMD(0x0f,0x0000);    /* RAM WRITE DATA MASK */
      LCD_WR_CMD(0x0b,0x5308);    /* RAM WRITE DATA MASK */
      LCD_WR_CMD(0x0c,0x0003);
      LCD_WR_CMD(0x0d,0x000a);
      LCD_WR_CMD(0x0e,0x2e00);
      LCD_WR_CMD(0x1e,0x00be);
      LCD_WR_CMD(0x25,0x8000);
      LCD_WR_CMD(0x26,0x7800);
      LCD_WR_CMD(0x27,0x0078);
      LCD_WR_CMD(0x4e,0x0000);
      LCD_WR_CMD(0x4f,0x0000);
      LCD_WR_CMD(0x12,0x08d9);
      /* Adjust the Gamma Curve */
      LCD_WR_CMD(0x30,0x0000);
      LCD_WR_CMD(0x31,0x0104);
      LCD_WR_CMD(0x32,0x0100);
      LCD_WR_CMD(0x33,0x0305);
      LCD_WR_CMD(0x34,0x0505);
      LCD_WR_CMD(0x35,0x0305);
      LCD_WR_CMD(0x36,0x0707);
      LCD_WR_CMD(0x37,0x0300);
      LCD_WR_CMD(0x3a,0x1200);
      LCD_WR_CMD(0x3b,0x0800);
      LCD_WR_CMD(0x07,0x0033);
    }
    else if( DeviceCode == 0x1505 || DeviceCode == 0x0505 )
    {
      LCD_Code = R61505U;
      /* initializing funciton */
      LCD_WR_CMD(0xe5,0x8000);  /* Set the internal vcore voltage */
      LCD_WR_CMD(0x00,0x0001);  /* start OSC */
      LCD_WR_CMD(0x2b,0x0010);  /* Set the frame rate as 80 when the internal resistor is used for oscillator circuit */
      LCD_WR_CMD(0x01,0x0100);  /* s720  to  s1 ; G1 to G320 */
      LCD_WR_CMD(0x02,0x0700);  /* set the line inversion */
      LCD_WR_CMD(0x03,0x1018);  /* 65536 colors */
      LCD_WR_CMD(0x04,0x0000);
      LCD_WR_CMD(0x08,0x0202);  /* specify the line number of front and back porch periods respectively */
      LCD_WR_CMD(0x09,0x0000);
      LCD_WR_CMD(0x0a,0x0000);
      LCD_WR_CMD(0x0c,0x0000);  /* select  internal system clock */
      LCD_WR_CMD(0x0d,0x0000);
      LCD_WR_CMD(0x0f,0x0000);
      LCD_WR_CMD(0x50,0x0000);  /* set windows adress */
      LCD_WR_CMD(0x51,0x00ef);
      LCD_WR_CMD(0x52,0x0000);
      LCD_WR_CMD(0x53,0x013f);
      LCD_WR_CMD(0x60,0x2700);
      LCD_WR_CMD(0x61,0x0001);
      LCD_WR_CMD(0x6a,0x0000);
      LCD_WR_CMD(0x80,0x0000);
      LCD_WR_CMD(0x81,0x0000);
      LCD_WR_CMD(0x82,0x0000);
      LCD_WR_CMD(0x83,0x0000);
      LCD_WR_CMD(0x84,0x0000);
      LCD_WR_CMD(0x85,0x0000);
      LCD_WR_CMD(0x90,0x0010);
      LCD_WR_CMD(0x92,0x0000);
      LCD_WR_CMD(0x93,0x0003);
      LCD_WR_CMD(0x95,0x0110);
      LCD_WR_CMD(0x97,0x0000);
      LCD_WR_CMD(0x98,0x0000);
      /* power setting function */
      LCD_WR_CMD(0x10,0x0000);
      LCD_WR_CMD(0x11,0x0000);
      LCD_WR_CMD(0x12,0x0000);
      LCD_WR_CMD(0x13,0x0000);
      delay_ms(100);
      LCD_WR_CMD(0x10,0x17b0);
      LCD_WR_CMD(0x11,0x0004);
      delay_ms(50);
      LCD_WR_CMD(0x12,0x013e);
      delay_ms(50);
      LCD_WR_CMD(0x13,0x1f00);
      LCD_WR_CMD(0x29,0x000f);
      delay_ms(50);
      LCD_WR_CMD(0x20,0x0000);
      LCD_WR_CMD(0x21,0x0000);

      /* initializing function */
      LCD_WR_CMD(0x30,0x0204);
      LCD_WR_CMD(0x31,0x0001);
      LCD_WR_CMD(0x32,0x0000);
      LCD_WR_CMD(0x35,0x0206);
      LCD_WR_CMD(0x36,0x0600);
      LCD_WR_CMD(0x37,0x0500);
      LCD_WR_CMD(0x38,0x0505);
      LCD_WR_CMD(0x39,0x0407);
      LCD_WR_CMD(0x3c,0x0500);
      LCD_WR_CMD(0x3d,0x0503);

      /* display on */
      LCD_WR_CMD(0x07,0x0173);
    }
    else if( DeviceCode == 0x8989 )
    {
      LCD_Code = SSD1289;
      LCD_WR_CMD(0x0000,0x0001);    delay_ms(50);   /* Žò¿ªŸ§Õñ */
      LCD_WR_CMD(0x0003,0xA8A4);    delay_ms(50);
      LCD_WR_CMD(0x000C,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x000D,0x080C);    delay_ms(50);
      LCD_WR_CMD(0x000E,0x2B00);    delay_ms(50);
      LCD_WR_CMD(0x001E,0x00B0);    delay_ms(50);
      LCD_WR_CMD(0x0001,0x2B3F);    delay_ms(50);   /* Çý¶¯Êä³ö¿ØÖÆ320*240 0x2B3F */
      LCD_WR_CMD(0x0002,0x0600);    delay_ms(50);
      LCD_WR_CMD(0x0010,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x0011,0x6070);    delay_ms(50);   /* ¶šÒåÊýŸÝžñÊœ 16Î»É« ºáÆÁ 0x6070 */
      LCD_WR_CMD(0x0005,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x0006,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x0016,0xEF1C);    delay_ms(50);
      LCD_WR_CMD(0x0017,0x0003);    delay_ms(50);
      LCD_WR_CMD(0x0007,0x0133);    delay_ms(50);
      LCD_WR_CMD(0x000B,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x000F,0x0000);    delay_ms(50);   /* ÉšÃè¿ªÊŒµØÖ· */
      LCD_WR_CMD(0x0041,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x0042,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x0048,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x0049,0x013F);    delay_ms(50);
      LCD_WR_CMD(0x004A,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x004B,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x0044,0xEF00);    delay_ms(50);
      LCD_WR_CMD(0x0045,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x0046,0x013F);    delay_ms(50);
      LCD_WR_CMD(0x0030,0x0707);    delay_ms(50);
      LCD_WR_CMD(0x0031,0x0204);    delay_ms(50);
      LCD_WR_CMD(0x0032,0x0204);    delay_ms(50);
      LCD_WR_CMD(0x0033,0x0502);    delay_ms(50);
      LCD_WR_CMD(0x0034,0x0507);    delay_ms(50);
      LCD_WR_CMD(0x0035,0x0204);    delay_ms(50);
      LCD_WR_CMD(0x0036,0x0204);    delay_ms(50);
      LCD_WR_CMD(0x0037,0x0502);    delay_ms(50);
      LCD_WR_CMD(0x003A,0x0302);    delay_ms(50);
      LCD_WR_CMD(0x003B,0x0302);    delay_ms(50);
      LCD_WR_CMD(0x0023,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x0024,0x0000);    delay_ms(50);
      LCD_WR_CMD(0x0025,0x8000);    delay_ms(50);
      LCD_WR_CMD(0x004f,0);        /* ÐÐÊ×Ö·0 */
      LCD_WR_CMD(0x004e,0);        /* ÁÐÊ×Ö·0 */
    }
    else if( DeviceCode == 0x8999 )
    {
      LCD_Code = SSD1298;
      LCD_WR_CMD(0x0028,0x0006);
      LCD_WR_CMD(0x0000,0x0001);
      LCD_WR_CMD(0x0003,0xaea4);    /* power control 1---line frequency and VHG,VGL voltage */
      LCD_WR_CMD(0x000c,0x0004);    /* power control 2---VCIX2 output voltage */
      LCD_WR_CMD(0x000d,0x000c);    /* power control 3---Vlcd63 voltage */
      LCD_WR_CMD(0x000e,0x2800);    /* power control 4---VCOMA voltage VCOML=VCOMH*0.9475-VCOMA */
      LCD_WR_CMD(0x001e,0x00b5);    /* POWER CONTROL 5---VCOMH voltage */
      LCD_WR_CMD(0x0001,0x3b3f);
      LCD_WR_CMD(0x0002,0x0600);
      LCD_WR_CMD(0x0010,0x0000);
      LCD_WR_CMD(0x0011,0x6830);
      LCD_WR_CMD(0x0005,0x0000);
      LCD_WR_CMD(0x0006,0x0000);
      LCD_WR_CMD(0x0016,0xef1c);
      LCD_WR_CMD(0x0007,0x0033);    /* Display control 1 */
      /* when GON=1 and DTE=0,all gate outputs become VGL */
      /* when GON=1 and DTE=0,all gate outputs become VGH */
      /* non-selected gate wires become VGL */
      LCD_WR_CMD(0x000b,0x0000);
      LCD_WR_CMD(0x000f,0x0000);
      LCD_WR_CMD(0x0041,0x0000);
      LCD_WR_CMD(0x0042,0x0000);
      LCD_WR_CMD(0x0048,0x0000);
      LCD_WR_CMD(0x0049,0x013f);
      LCD_WR_CMD(0x004a,0x0000);
      LCD_WR_CMD(0x004b,0x0000);
      LCD_WR_CMD(0x0044,0xef00);	/* Horizontal RAM start and end address */
      LCD_WR_CMD(0x0045,0x0000);	/* Vretical RAM start address */
      LCD_WR_CMD(0x0046,0x013f);	/* Vretical RAM end address */
      LCD_WR_CMD(0x004e,0x0000);	/* set GDDRAM x address counter */
      LCD_WR_CMD(0x004f,0x0000);    /* set GDDRAM y address counter */
      /* y control */
      LCD_WR_CMD(0x0030,0x0707);
      LCD_WR_CMD(0x0031,0x0202);
      LCD_WR_CMD(0x0032,0x0204);
      LCD_WR_CMD(0x0033,0x0502);
      LCD_WR_CMD(0x0034,0x0507);
      LCD_WR_CMD(0x0035,0x0204);
      LCD_WR_CMD(0x0036,0x0204);
      LCD_WR_CMD(0x0037,0x0502);
      LCD_WR_CMD(0x003a,0x0302);
      LCD_WR_CMD(0x003b,0x0302);
      LCD_WR_CMD(0x0023,0x0000);
      LCD_WR_CMD(0x0024,0x0000);
      LCD_WR_CMD(0x0025,0x8000);
      LCD_WR_CMD(0x0026,0x7000);
      LCD_WR_CMD(0x0020,0xb0eb);
      LCD_WR_CMD(0x0027,0x007c);
    }
    else if( DeviceCode == 0x5408 )
    {
      LCD_Code = SPFD5408B;

      LCD_WR_CMD(0x0001,0x0100);	  /* Driver Output Contral Register */
      LCD_WR_CMD(0x0002,0x0700);      /* LCD Driving Waveform Contral */
      LCD_WR_CMD(0x0003,0x1030);	  /* Entry ModeÉèÖÃ */

      LCD_WR_CMD(0x0004,0x0000);	  /* Scalling Control register */
      LCD_WR_CMD(0x0008,0x0207);	  /* Display Control 2 */
      LCD_WR_CMD(0x0009,0x0000);	  /* Display Control 3 */
      LCD_WR_CMD(0x000A,0x0000);	  /* Frame Cycle Control */
      LCD_WR_CMD(0x000C,0x0000);	  /* External Display Interface Control 1 */
      LCD_WR_CMD(0x000D,0x0000);      /* Frame Maker Position */
      LCD_WR_CMD(0x000F,0x0000);	  /* External Display Interface Control 2 */
      delay_ms(50);
      LCD_WR_CMD(0x0007,0x0101);	  /* Display Control */
      delay_ms(50);
      LCD_WR_CMD(0x0010,0x16B0);      /* Power Control 1 */
      LCD_WR_CMD(0x0011,0x0001);      /* Power Control 2 */
      LCD_WR_CMD(0x0017,0x0001);      /* Power Control 3 */
      LCD_WR_CMD(0x0012,0x0138);      /* Power Control 4 */
      LCD_WR_CMD(0x0013,0x0800);      /* Power Control 5 */
      LCD_WR_CMD(0x0029,0x0009);	  /* NVM read data 2 */
      LCD_WR_CMD(0x002a,0x0009);	  /* NVM read data 3 */
      LCD_WR_CMD(0x00a4,0x0000);
      LCD_WR_CMD(0x0050,0x0000);	  /* ÉèÖÃ²Ù×÷Ž°¿ÚµÄXÖá¿ªÊŒÁÐ */
      LCD_WR_CMD(0x0051,0x00EF);	  /* ÉèÖÃ²Ù×÷Ž°¿ÚµÄXÖáœáÊøÁÐ */
      LCD_WR_CMD(0x0052,0x0000);	  /* ÉèÖÃ²Ù×÷Ž°¿ÚµÄYÖá¿ªÊŒÐÐ */
      LCD_WR_CMD(0x0053,0x013F);	  /* ÉèÖÃ²Ù×÷Ž°¿ÚµÄYÖáœáÊøÐÐ */

      LCD_WR_CMD(0x0060,0x2700);	  /* Driver Output Control */
      /* ÉèÖÃÆÁÄ»µÄµãÊýÒÔŒ°ÉšÃèµÄÆðÊŒÐÐ */
      LCD_WR_CMD(0x0061,0x0003);	  /* Driver Output Control */
      LCD_WR_CMD(0x006A,0x0000);	  /* Vertical Scroll Control */

      LCD_WR_CMD(0x0080,0x0000);	  /* Display Position šC Partial Display 1 */
      LCD_WR_CMD(0x0081,0x0000);	  /* RAM Address Start šC Partial Display 1 */
      LCD_WR_CMD(0x0082,0x0000);	  /* RAM address End - Partial Display 1 */
      LCD_WR_CMD(0x0083,0x0000);	  /* Display Position šC Partial Display 2 */
      LCD_WR_CMD(0x0084,0x0000);	  /* RAM Address Start šC Partial Display 2 */
      LCD_WR_CMD(0x0085,0x0000);	  /* RAM address End šC Partail Display2 */
      LCD_WR_CMD(0x0090,0x0013);	  /* Frame Cycle Control */
      LCD_WR_CMD(0x0092,0x0000); 	  /* Panel Interface Control 2 */
      LCD_WR_CMD(0x0093,0x0003);	  /* Panel Interface control 3 */
      LCD_WR_CMD(0x0095,0x0110);	  /* Frame Cycle Control */
      LCD_WR_CMD(0x0007,0x0173);
    }
    else if( DeviceCode == 0x4531 )
    {
      LCD_Code = LGDP4531;
      /* Setup display */
      LCD_WR_CMD(0x00,0x0001);
      LCD_WR_CMD(0x10,0x0628);
      LCD_WR_CMD(0x12,0x0006);
      LCD_WR_CMD(0x13,0x0A32);
      LCD_WR_CMD(0x11,0x0040);
      LCD_WR_CMD(0x15,0x0050);
      LCD_WR_CMD(0x12,0x0016);
      delay_ms(50);
      LCD_WR_CMD(0x10,0x5660);
      delay_ms(50);
      LCD_WR_CMD(0x13,0x2A4E);
      LCD_WR_CMD(0x01,0x0100);
      LCD_WR_CMD(0x02,0x0300);
      LCD_WR_CMD(0x03,0x1030);
      LCD_WR_CMD(0x08,0x0202);
      LCD_WR_CMD(0x0A,0x0000);
      LCD_WR_CMD(0x30,0x0000);
      LCD_WR_CMD(0x31,0x0402);
      LCD_WR_CMD(0x32,0x0106);
      LCD_WR_CMD(0x33,0x0700);
      LCD_WR_CMD(0x34,0x0104);
      LCD_WR_CMD(0x35,0x0301);
      LCD_WR_CMD(0x36,0x0707);
      LCD_WR_CMD(0x37,0x0305);
      LCD_WR_CMD(0x38,0x0208);
      LCD_WR_CMD(0x39,0x0F0B);
      delay_ms(50);
      LCD_WR_CMD(0x41,0x0002);
      LCD_WR_CMD(0x60,0x2700);
      LCD_WR_CMD(0x61,0x0001);
      LCD_WR_CMD(0x90,0x0119);
      LCD_WR_CMD(0x92,0x010A);
      LCD_WR_CMD(0x93,0x0004);
      LCD_WR_CMD(0xA0,0x0100);
      delay_ms(50);
      LCD_WR_CMD(0x07,0x0133);
      delay_ms(50);
      LCD_WR_CMD(0xA0,0x0000);
    }
    else if( DeviceCode == 0x4535 )
    {
      LCD_Code = LGDP4535;
      LCD_WR_CMD(0x15, 0x0030); 	 /* Set the internal vcore voltage */
      LCD_WR_CMD(0x9A, 0x0010); 	 /* Start internal OSC */
      LCD_WR_CMD(0x11, 0x0020);	     /* set SS and SM bit */
      LCD_WR_CMD(0x10, 0x3428);	     /* set 1 line inversion */
      LCD_WR_CMD(0x12, 0x0002);	     /* set GRAM write direction and BGR=1 */
      LCD_WR_CMD(0x13, 0x1038);	     /* Resize register */
      delay_ms(40);
      LCD_WR_CMD(0x12, 0x0012);	     /* set the back porch and front porch */
      delay_ms(40);
      LCD_WR_CMD(0x10, 0x3420);	     /* set non-display area refresh cycle ISC[3:0] */
      LCD_WR_CMD(0x13, 0x3045);	     /* FMARK function */
      delay_ms(70);
      LCD_WR_CMD(0x30, 0x0000);      /* RGB interface setting */
      LCD_WR_CMD(0x31, 0x0402);	     /* Frame marker Position */
      LCD_WR_CMD(0x32, 0x0307);      /* RGB interface polarity */
      LCD_WR_CMD(0x33, 0x0304);      /* SAP, BT[3:0], AP, DSTB, SLP, STB */
      LCD_WR_CMD(0x34, 0x0004);      /* DC1[2:0], DC0[2:0], VC[2:0] */
      LCD_WR_CMD(0x35, 0x0401);      /* VREG1OUT voltage */
      LCD_WR_CMD(0x36, 0x0707);      /* VDV[4:0] for VCOM amplitude */
      LCD_WR_CMD(0x37, 0x0305);      /* SAP, BT[3:0], AP, DSTB, SLP, STB */
      LCD_WR_CMD(0x38, 0x0610);      /* DC1[2:0], DC0[2:0], VC[2:0] */
      LCD_WR_CMD(0x39, 0x0610);      /* VREG1OUT voltage */
      LCD_WR_CMD(0x01, 0x0100);      /* VDV[4:0] for VCOM amplitude */
      LCD_WR_CMD(0x02, 0x0300);      /* VCM[4:0] for VCOMH */
      LCD_WR_CMD(0x03, 0x1030);      /* GRAM horizontal Address */
      LCD_WR_CMD(0x08, 0x0808);      /* GRAM Vertical Address */
      LCD_WR_CMD(0x0A, 0x0008);
      LCD_WR_CMD(0x60, 0x2700);	     /* Gate Scan Line */
      LCD_WR_CMD(0x61, 0x0001);	     /* NDL,VLE, REV */
      LCD_WR_CMD(0x90, 0x013E);
      LCD_WR_CMD(0x92, 0x0100);
      LCD_WR_CMD(0x93, 0x0100);
      LCD_WR_CMD(0xA0, 0x3000);
      LCD_WR_CMD(0xA3, 0x0010);
      LCD_WR_CMD(0x07, 0x0001);
      LCD_WR_CMD(0x07, 0x0021);
      LCD_WR_CMD(0x07, 0x0023);
      LCD_WR_CMD(0x07, 0x0033);
      LCD_WR_CMD(0x07, 0x0133);
    }
    else if( DeviceCode == 0x0047 )
    {
      LCD_Code = HX8347D;
      /* Start Initial Sequence */
      LCD_WR_CMD(0xEA,0x00);
      LCD_WR_CMD(0xEB,0x20);
      LCD_WR_CMD(0xEC,0x0C);
      LCD_WR_CMD(0xED,0xC4);
      LCD_WR_CMD(0xE8,0x40);
      LCD_WR_CMD(0xE9,0x38);
      LCD_WR_CMD(0xF1,0x01);
      LCD_WR_CMD(0xF2,0x10);
      LCD_WR_CMD(0x27,0xA3);
      /* GAMMA SETTING */
      LCD_WR_CMD(0x40,0x01);
      LCD_WR_CMD(0x41,0x00);
      LCD_WR_CMD(0x42,0x00);
      LCD_WR_CMD(0x43,0x10);
      LCD_WR_CMD(0x44,0x0E);
      LCD_WR_CMD(0x45,0x24);
      LCD_WR_CMD(0x46,0x04);
      LCD_WR_CMD(0x47,0x50);
      LCD_WR_CMD(0x48,0x02);
      LCD_WR_CMD(0x49,0x13);
      LCD_WR_CMD(0x4A,0x19);
      LCD_WR_CMD(0x4B,0x19);
      LCD_WR_CMD(0x4C,0x16);
      LCD_WR_CMD(0x50,0x1B);
      LCD_WR_CMD(0x51,0x31);
      LCD_WR_CMD(0x52,0x2F);
      LCD_WR_CMD(0x53,0x3F);
      LCD_WR_CMD(0x54,0x3F);
      LCD_WR_CMD(0x55,0x3E);
      LCD_WR_CMD(0x56,0x2F);
      LCD_WR_CMD(0x57,0x7B);
      LCD_WR_CMD(0x58,0x09);
      LCD_WR_CMD(0x59,0x06);
      LCD_WR_CMD(0x5A,0x06);
      LCD_WR_CMD(0x5B,0x0C);
      LCD_WR_CMD(0x5C,0x1D);
      LCD_WR_CMD(0x5D,0xCC);
      /* Power Voltage Setting */
      LCD_WR_CMD(0x1B,0x18);
      LCD_WR_CMD(0x1A,0x01);
      LCD_WR_CMD(0x24,0x15);
      LCD_WR_CMD(0x25,0x50);
      LCD_WR_CMD(0x23,0x8B);
      LCD_WR_CMD(0x18,0x36);
      LCD_WR_CMD(0x19,0x01);
      LCD_WR_CMD(0x01,0x00);
      LCD_WR_CMD(0x1F,0x88);
      delay_ms(50);
      LCD_WR_CMD(0x1F,0x80);
      delay_ms(50);
      LCD_WR_CMD(0x1F,0x90);
      delay_ms(50);
      LCD_WR_CMD(0x1F,0xD0);
      delay_ms(50);
      LCD_WR_CMD(0x17,0x05);
      LCD_WR_CMD(0x36,0x00);
      LCD_WR_CMD(0x28,0x38);
      delay_ms(50);
      LCD_WR_CMD(0x28,0x3C);
    }
    else if( DeviceCode == 0x7783 )
    {
      LCD_Code = ST7781;
      /* Start Initial Sequence */
      LCD_WR_CMD(0x00FF,0x0001);
      LCD_WR_CMD(0x00F3,0x0008);
      LCD_WR_CMD(0x0001,0x0100);
      LCD_WR_CMD(0x0002,0x0700);
      LCD_WR_CMD(0x0003,0x1030);
      LCD_WR_CMD(0x0008,0x0302);
      LCD_WR_CMD(0x0008,0x0207);
      LCD_WR_CMD(0x0009,0x0000);
      LCD_WR_CMD(0x000A,0x0000);
      LCD_WR_CMD(0x0010,0x0000);
      LCD_WR_CMD(0x0011,0x0005);
      LCD_WR_CMD(0x0012,0x0000);
      LCD_WR_CMD(0x0013,0x0000);
      delay_ms(50);
      LCD_WR_CMD(0x0010,0x12B0);
      delay_ms(50);
      LCD_WR_CMD(0x0011,0x0007);
      delay_ms(50);
      LCD_WR_CMD(0x0012,0x008B);
      delay_ms(50);
      LCD_WR_CMD(0x0013,0x1700);
      delay_ms(50);
      LCD_WR_CMD(0x0029,0x0022);
      LCD_WR_CMD(0x0030,0x0000);
      LCD_WR_CMD(0x0031,0x0707);
      LCD_WR_CMD(0x0032,0x0505);
      LCD_WR_CMD(0x0035,0x0107);
      LCD_WR_CMD(0x0036,0x0008);
      LCD_WR_CMD(0x0037,0x0000);
      LCD_WR_CMD(0x0038,0x0202);
      LCD_WR_CMD(0x0039,0x0106);
      LCD_WR_CMD(0x003C,0x0202);
      LCD_WR_CMD(0x003D,0x0408);
      delay_ms(50);
      LCD_WR_CMD(0x0050,0x0000);
      LCD_WR_CMD(0x0051,0x00EF);
      LCD_WR_CMD(0x0052,0x0000);
      LCD_WR_CMD(0x0053,0x013F);
      LCD_WR_CMD(0x0060,0xA700);
      LCD_WR_CMD(0x0061,0x0001);
      LCD_WR_CMD(0x0090,0x0033);
      LCD_WR_CMD(0x002B,0x000B);
      LCD_WR_CMD(0x0007,0x0133);
    }
    else	/* special ID */
    {
      uint16_t DeviceCode2 = LCD_RD_CMD(0x67);
      if( DeviceCode2 == 0x0046 )
      {
        LCD_Code = HX8346A;

        /* Gamma for CMO 3.2 */
        LCD_WR_CMD(0x46,0x94);
        LCD_WR_CMD(0x47,0x41);
        LCD_WR_CMD(0x48,0x00);
        LCD_WR_CMD(0x49,0x33);
        LCD_WR_CMD(0x4a,0x23);
        LCD_WR_CMD(0x4b,0x45);
        LCD_WR_CMD(0x4c,0x44);
        LCD_WR_CMD(0x4d,0x77);
        LCD_WR_CMD(0x4e,0x12);
        LCD_WR_CMD(0x4f,0xcc);
        LCD_WR_CMD(0x50,0x46);
        LCD_WR_CMD(0x51,0x82);
        /* 240x320 window setting */
        LCD_WR_CMD(0x02,0x00);
        LCD_WR_CMD(0x03,0x00);
        LCD_WR_CMD(0x04,0x01);
        LCD_WR_CMD(0x05,0x3f);
        LCD_WR_CMD(0x06,0x00);
        LCD_WR_CMD(0x07,0x00);
        LCD_WR_CMD(0x08,0x00);
        LCD_WR_CMD(0x09,0xef);

        /* Display Setting */
        LCD_WR_CMD(0x01,0x06);
        LCD_WR_CMD(0x16,0xC8);	/* MY(1) MX(1) MV(0) */

        LCD_WR_CMD(0x23,0x95);
        LCD_WR_CMD(0x24,0x95);
        LCD_WR_CMD(0x25,0xff);

        LCD_WR_CMD(0x27,0x02);
        LCD_WR_CMD(0x28,0x02);
        LCD_WR_CMD(0x29,0x02);
        LCD_WR_CMD(0x2a,0x02);
        LCD_WR_CMD(0x2c,0x02);
        LCD_WR_CMD(0x2d,0x02);

        LCD_WR_CMD(0x3a,0x01);
        LCD_WR_CMD(0x3b,0x01);
        LCD_WR_CMD(0x3c,0xf0);
        LCD_WR_CMD(0x3d,0x00);
        delay_ms(2);
        LCD_WR_CMD(0x35,0x38);
        LCD_WR_CMD(0x36,0x78);

        LCD_WR_CMD(0x3e,0x38);

        LCD_WR_CMD(0x40,0x0f);
        LCD_WR_CMD(0x41,0xf0);
        /* Power Supply Setting */
        LCD_WR_CMD(0x19,0x49);
        LCD_WR_CMD(0x93,0x0f);
        delay_ms(1);
        LCD_WR_CMD(0x20,0x30);
        LCD_WR_CMD(0x1d,0x07);
        LCD_WR_CMD(0x1e,0x00);
        LCD_WR_CMD(0x1f,0x07);
        /* VCOM Setting for CMO 3.2¡± Panel */
        LCD_WR_CMD(0x44,0x4d);
        LCD_WR_CMD(0x45,0x13);
        delay_ms(1);
        LCD_WR_CMD(0x1c,0x04);
        delay_ms(2);
        LCD_WR_CMD(0x43,0x80);
        delay_ms(5);
        LCD_WR_CMD(0x1b,0x08);
        delay_ms(4);
        LCD_WR_CMD(0x1b,0x10);
        delay_ms(4);
        /* Display ON Setting */
        LCD_WR_CMD(0x90,0x7f);
        LCD_WR_CMD(0x26,0x04);
        delay_ms(4);
        LCD_WR_CMD(0x26,0x24);
        LCD_WR_CMD(0x26,0x2c);
        delay_ms(4);
        LCD_WR_CMD(0x26,0x3c);
        /* Set internal VDDD voltage */
        LCD_WR_CMD(0x57,0x02);
        LCD_WR_CMD(0x55,0x00);
        LCD_WR_CMD(0x57,0x00);
      }
      if( DeviceCode2 == 0x0047 )
      {
        LCD_Code = HX8347A;
        LCD_WR_CMD(0x0042,0x0008);
        /* Gamma setting */
        LCD_WR_CMD(0x0046,0x00B4);
        LCD_WR_CMD(0x0047,0x0043);
        LCD_WR_CMD(0x0048,0x0013);
        LCD_WR_CMD(0x0049,0x0047);
        LCD_WR_CMD(0x004A,0x0014);
        LCD_WR_CMD(0x004B,0x0036);
        LCD_WR_CMD(0x004C,0x0003);
        LCD_WR_CMD(0x004D,0x0046);
        LCD_WR_CMD(0x004E,0x0005);
        LCD_WR_CMD(0x004F,0x0010);
        LCD_WR_CMD(0x0050,0x0008);
        LCD_WR_CMD(0x0051,0x000a);
        /* Window Setting */
        LCD_WR_CMD(0x0002,0x0000);
        LCD_WR_CMD(0x0003,0x0000);
        LCD_WR_CMD(0x0004,0x0000);
        LCD_WR_CMD(0x0005,0x00EF);
        LCD_WR_CMD(0x0006,0x0000);
        LCD_WR_CMD(0x0007,0x0000);
        LCD_WR_CMD(0x0008,0x0001);
        LCD_WR_CMD(0x0009,0x003F);
        delay_ms(10);
        LCD_WR_CMD(0x0001,0x0006);
        LCD_WR_CMD(0x0016,0x00C8);
        LCD_WR_CMD(0x0023,0x0095);
        LCD_WR_CMD(0x0024,0x0095);
        LCD_WR_CMD(0x0025,0x00FF);
        LCD_WR_CMD(0x0027,0x0002);
        LCD_WR_CMD(0x0028,0x0002);
        LCD_WR_CMD(0x0029,0x0002);
        LCD_WR_CMD(0x002A,0x0002);
        LCD_WR_CMD(0x002C,0x0002);
        LCD_WR_CMD(0x002D,0x0002);
        LCD_WR_CMD(0x003A,0x0001);
        LCD_WR_CMD(0x003B,0x0001);
        LCD_WR_CMD(0x003C,0x00F0);
        LCD_WR_CMD(0x003D,0x0000);
        delay_ms(20);
        LCD_WR_CMD(0x0035,0x0038);
        LCD_WR_CMD(0x0036,0x0078);
        LCD_WR_CMD(0x003E,0x0038);
        LCD_WR_CMD(0x0040,0x000F);
        LCD_WR_CMD(0x0041,0x00F0);
        LCD_WR_CMD(0x0038,0x0000);
        /* Power Setting */
        LCD_WR_CMD(0x0019,0x0049);
        LCD_WR_CMD(0x0093,0x000A);
        delay_ms(10);
        LCD_WR_CMD(0x0020,0x0020);
        LCD_WR_CMD(0x001D,0x0003);
        LCD_WR_CMD(0x001E,0x0000);
        LCD_WR_CMD(0x001F,0x0009);
        LCD_WR_CMD(0x0044,0x0053);
        LCD_WR_CMD(0x0045,0x0010);
        delay_ms(10);
        LCD_WR_CMD(0x001C,0x0004);
        delay_ms(20);
        LCD_WR_CMD(0x0043,0x0080);
        delay_ms(5);
        LCD_WR_CMD(0x001B,0x000a);
        delay_ms(40);
        LCD_WR_CMD(0x001B,0x0012);
        delay_ms(40);
        /* Display On Setting */
        LCD_WR_CMD(0x0090,0x007F);
        LCD_WR_CMD(0x0026,0x0004);
        delay_ms(40);
        LCD_WR_CMD(0x0026,0x0024);
        LCD_WR_CMD(0x0026,0x002C);
        delay_ms(40);
        LCD_WR_CMD(0x0070,0x0008);
        LCD_WR_CMD(0x0026,0x003C);
        LCD_WR_CMD(0x0057,0x0002);
        LCD_WR_CMD(0x0055,0x0000);
        LCD_WR_CMD(0x0057,0x0000);
      } else {
        jsiConsolePrintf("Unknown LCD code %d %d\n", DeviceCode, DeviceCode2);
      }
    }
  }
#endif
  delay_ms(50);   /* delay 50 ms */
  #if defined(LCD_BL) && !defined(ESPR_LCD_MANUAL_BACKLIGHT)
  jshPinOutput(LCD_BL, 1);
  #endif
}


static inline void lcdSetCursor(JsGraphics *gfx, unsigned short x, unsigned short y) {
  if (LCD_Code!=ILI9341 && LCD_Code!=ST7796) {
    x = (gfx->data.width-1)-x;
  }
  switch( LCD_Code )
  {
     default:		 /* 0x9320 0x9325 0x9328 0x9331 0x5408 0x1505 0x0505 0x7783 0x4531 0x4535 */
          LCD_WR_CMD(0x0020, y );
          LCD_WR_CMD(0x0021, x );
	      break;
#ifndef SAVE_ON_FLASH
     case ILI9341:
     case ST7796:
          LCD_WR_CMD2(0x002A, x>>8,x&0xFF );
          LCD_WR_CMD2(0x002B, y>>8,y&0xFF );
	      break;
     case SSD1298: 	 /* 0x8999 */
     case SSD1289:   /* 0x8989 */
	      LCD_WR_CMD(0x004e, y );
          LCD_WR_CMD(0x004f, x );
	      break;

     case HX8346A: 	 /* 0x0046 */
     case HX8347A: 	 /* 0x0047 */
     case HX8347D: 	 /* 0x0047 */
	      LCD_WR_CMD(0x02, y>>8 );
	      LCD_WR_CMD(0x03, y );

	      LCD_WR_CMD(0x06, x>>8 );
	      LCD_WR_CMD(0x07, x );

	      break;
     case SSD2119:	 /* 3.5 LCD 0x9919 */
	      break;
#endif
  }
}

static inline void lcdSetWindow(JsGraphics *gfx, unsigned short x1, unsigned short y1, unsigned short x2, unsigned short y2) {
  // x1>=x2  and y1>=y2
  if (LCD_Code!=ILI9341 && LCD_Code!=ST7796) {
    x2 = (gfx->data.width-1)-x2;
    x1 = (gfx->data.width-1)-x1;
  }
  // jsiConsolePrintf("SetWindow x1=%d y1=%d x2=%d y2=%d\n", x1, y1, x2, y2);
  switch (LCD_Code) {
     default:
        LCD_WR_CMD(0x50, y1);
        LCD_WR_CMD(0x51, y2);
        LCD_WR_CMD(0x52, x2);
        LCD_WR_CMD(0x53, x1);
        break;
#ifndef SAVE_ON_FLASH
     case ILI9341:
     case ST7796:
          LCD_WR_CMD4(0x002A, x1>>8, x1&0xFF, x2>>8, x2&0xFF );
          LCD_WR_CMD4(0x002B, y1>>8, y1&0xFF, y2>>8, y2&0xFF );
	      break;
     case SSD1289:   /* 0x8989 */
        LCD_WR_CMD(0x44, y1 | (y2<<8));
        LCD_WR_CMD(0x45, x2);
        LCD_WR_CMD(0x46, x1);
        break;
     case HX8346A:
     case HX8347A:
     case HX8347D:
        LCD_WR_CMD(0x02,y1>>8);
        LCD_WR_CMD(0x03,y1);
        LCD_WR_CMD(0x04,y2>>8);
        LCD_WR_CMD(0x05,y2);
        LCD_WR_CMD(0x06,x2>>8);
        LCD_WR_CMD(0x07,x2);
        LCD_WR_CMD(0x08,x1>>8);
        LCD_WR_CMD(0x09,x1);
        break;
#endif
  }
}

static inline void lcdSetWrite() {
  switch (LCD_Code) {
     default:
        LCD_WR_REG(0x22); // start data tx
        break;
#ifndef SAVE_ON_FLASH
     case ILI9341:
     case ST7796:
        LCD_WR_REG(0x2C); // start data tx
      break;
#endif
  }
}

static inline void lcdSetFullWindow(JsGraphics *gfx) {
  lcdSetWindow(gfx,0,0,gfx->data.width-1,gfx->data.height-1);
}

void lcdFSMC_setPower(bool isOn) {
  if (LCD_Code == ILI9341 || LCD_Code == ST7796) {
    if (isOn) {
      LCD_WR_CMD(0x11, 0); // SLPOUT
      jshDelayMicroseconds(20);
      LCD_WR_CMD(0x29, 0); // DISPON
      // don't turn backlight on right away to save on flicker
      #if defined(LCD_BL) && !defined(ESPR_LCD_MANUAL_BACKLIGHT)
      jshPinOutput(LCD_BL, 1);
      #endif
    } else {
      #ifdef LCD_BL
      jshPinOutput(LCD_BL, 0);
      #endif
      LCD_WR_CMD(0x28, 0); // DISPOFF
      jshDelayMicroseconds(20);
      LCD_WR_CMD(0x10, 0); // SLPIN
    }
  }
}

void lcdFillRect_FSMC(JsGraphics *gfx, int x1, int y1, int x2, int y2, unsigned int col) {
  // finally!
#ifdef LCD_ORIENTATION_LANDSCAPE
  if (y1==y2) { // special case for single horizontal line (appears as vertical on landscape display) - no window needed
    lcdSetCursor(gfx,x1,y1);
    lcdSetWrite();
    unsigned int i=0, l=(1+x2-x1);
    LCD_WR_Data_multi(col, l);
#else
  if (x1==x2) { // special case for single vertical line - no window needed
    lcdSetCursor(gfx,x2,y1);
    lcdSetWrite();
    unsigned int i=0, l=(1+y2-y1);
    LCD_WR_Data_multi(col, l);
#endif
  } else {
    lcdSetWindow(gfx,x1,y1,x2,y2);
    if (LCD_Code!=ILI9341 && LCD_Code!=ST7796)
      lcdSetCursor(gfx,x1,y1);// FIXME - we don't need this?
    lcdSetWrite();
    unsigned int l=(1+x2-x1)*(1+y2-y1);
    LCD_WR_Data_multi(col, l);
    lcdSetFullWindow(gfx);
  }
}

unsigned int lcdGetPixel_FSMC(JsGraphics *gfx, int x, int y) {
  lcdSetCursor(gfx,x,y);
  LCD_WR_REG(0x2E); // start read
  LCD_RD_Data(); // dummy read
  return LCD_RD_Data();
}


void lcdSetPixel_FSMC(JsGraphics *gfx, int x, int y, unsigned int col) {
  lcdSetCursor(gfx,x,y);
  lcdSetWrite();
  LCD_WR_Data(col);
}

void lcdInit_FSMC(JsGraphics *gfx) {
  assert(gfx->data.bpp == 16);


  LCD_init_hardware();
  LCD_init_panel();
  lcdSetFullWindow(gfx);
}

void lcdSetCallbacks_FSMC(JsGraphics *gfx) {
  gfx->setPixel = lcdSetPixel_FSMC;
  gfx->getPixel = lcdGetPixel_FSMC;
  gfx->fillRect = lcdFillRect_FSMC;
}

void lcdFSMC_blitStart(JsGraphics *gfx, int x, int y, int w, int h) {
  lcdSetWindow(gfx, x, y, x+w-1, y+h-1);
  lcdSetWrite();
}
void lcdFSMC_setCursor(JsGraphics *gfx, int x, int y) {
  lcdSetCursor(gfx,x,y);
  //lcdSetWindow(gfx,x,y,gfx->data.width-1,y);
  lcdSetWrite();
}
void lcdFSMC_blitPixel(unsigned int col) {
  LCD_RAM = col;
}

void lcdFSMC_blitEnd() {
}

void lcdFSMC_blit4Bit(JsGraphics *gfx, int x, int y, int w, int h, int scale, JsvStringIterator *pixels, uint16_t *palette, FsmcNewLineCallback callback) {
  assert((w&1)==0); // only works on even image widths
  int w2 = w>>1;
  lcdFSMC_blitStart(gfx, x,y, w*scale,h*scale);
  for (int row=0;row<h;row++) {
    JsvStringIterator lastPixels;
    jsvStringIteratorClone(&lastPixels, pixels);
    for (int n=1;n<=scale;n++) {
      if(callback) callback(y+row*scale+n, palette);
      if (scale==1) {
        for (int x=0;x<w2;x++) {
          int bitData = jsvStringIteratorGetCharAndNext(pixels);
          uint16_t c = palette[(bitData>>4)&15];
          lcdFSMC_blitPixel(c);
          c = palette[bitData&15];
          lcdFSMC_blitPixel(c);
        }
      } else if (scale==2) {
        for (int x=0;x<w2;x++) {
          int bitData = jsvStringIteratorGetCharAndNext(pixels);
          uint16_t c = palette[(bitData>>4)&15];
          lcdFSMC_blitPixel(c);
          lcdFSMC_blitPixel(c);
          c = palette[bitData&15];
          lcdFSMC_blitPixel(c);
          lcdFSMC_blitPixel(c);
        }
      } else { // fallback for not 1/2 scale
        for (int x=0;x<w2;x++) {
          int bitData = jsvStringIteratorGetCharAndNext(pixels);
          uint16_t c = palette[(bitData>>4)&15];
          for (int s=0;s<scale;s++)
            lcdFSMC_blitPixel(c);
          c = palette[bitData&15];
          for (int s=0;s<scale;s++)
            lcdFSMC_blitPixel(c);
        }
      }
      // display the same row multiple times if needed
      if (n<scale) {
        jsvStringIteratorFree(pixels);
        jsvStringIteratorClone(pixels, &lastPixels);
      }
    }
    jsvStringIteratorFree(&lastPixels);
  }
  lcdFSMC_blitEnd();
}

void lcdFSMC_blit2Bit(JsGraphics *gfx, int x, int y, int w, int h, int scale, JsvStringIterator *pixels, uint16_t *palette, FsmcNewLineCallback callback) {
  assert((w&3)==0); // only works on image widths that are a multiple of 4
  int w2 = w>>2;
  lcdFSMC_blitStart(gfx, x,y, w*scale,h*scale);
  for (int row=0;row<h;row++) {
    JsvStringIterator lastPixels;
    jsvStringIteratorClone(&lastPixels, pixels);
    for (int n=1;n<=scale;n++) {
      if(callback) callback(y+row*scale+n, palette);
      if (scale==1) {
        for (int x=0;x<w2;x++) {
          int bitData = jsvStringIteratorGetCharAndNext(pixels);
          uint16_t c = palette[(bitData>>6)&3];
          lcdFSMC_blitPixel(c);
          c = palette[(bitData>>4)&3];
          lcdFSMC_blitPixel(c);
          c = palette[(bitData>>2)&3];
          lcdFSMC_blitPixel(c);
          c = palette[bitData&3];
          lcdFSMC_blitPixel(c);
        }
      } else if (scale==2) {
        for (int x=0;x<w2;x++) {
          int bitData = jsvStringIteratorGetCharAndNext(pixels);
          uint16_t c = palette[(bitData>>6)&3];
          lcdFSMC_blitPixel(c);
          lcdFSMC_blitPixel(c);
          c = palette[(bitData>>4)&3];
          lcdFSMC_blitPixel(c);
          lcdFSMC_blitPixel(c);
          c = palette[(bitData>>2)&3];
          lcdFSMC_blitPixel(c);
          lcdFSMC_blitPixel(c);
          c = palette[bitData&3];
          lcdFSMC_blitPixel(c);
          lcdFSMC_blitPixel(c);
        }
      } else { // fallback for not 1/2 scale
        for (int x=0;x<w2;x++) {
          int bitData = jsvStringIteratorGetCharAndNext(pixels);
          uint16_t c = palette[(bitData>>6)&3];
          for (int s=0;s<scale;s++)
            lcdFSMC_blitPixel(c);
          c = palette[(bitData>>4)&3];
          for (int s=0;s<scale;s++)
            lcdFSMC_blitPixel(c);
          c = palette[(bitData>>2)&3];
          for (int s=0;s<scale;s++)
            lcdFSMC_blitPixel(c);
          c = palette[bitData&3];
          for (int s=0;s<scale;s++)
            lcdFSMC_blitPixel(c);
        }
      }
      // display the same row multiple times if needed
      if (n<scale) {
        jsvStringIteratorFree(pixels);
        jsvStringIteratorClone(pixels, &lastPixels);
      }
    }
    jsvStringIteratorFree(&lastPixels);
  }
  lcdFSMC_blitEnd();
}
