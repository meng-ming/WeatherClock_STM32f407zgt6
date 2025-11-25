#ifndef __IMAGE_H
#define __IMAGE_H

#include <stdint.h>

// 添加函数声明
void TFT_ShowImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* pData);

#endif
