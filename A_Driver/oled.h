/*
 *  oled.h  --  SSD1306 OLED 驱动 (SPI, 128x64)
 *
 *  接线: PB9=SCLK, PB8=MOSI (SPI1), PB3=RES, PB2=DC, PA27=CS
 *  从机地址: 0x3C
 */
#ifndef OLED_H
#define OLED_H

/* ---- 公开 API ---- */

void OLED_Init(void);
void OLED_Clear(void);
void OLED_Refresh(void);
void OLED_SetCursor(uint32_t page, uint32_t col);
void OLED_ShowString(uint32_t page, uint32_t col, const char *str);

#endif
