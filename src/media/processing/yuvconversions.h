#pragma once

#include <stdint.h>

int  yuv420_to_rgb_i_avx2_mt (uint8_t* input, uint8_t* output, uint16_t width, uint16_t height, uint8_t threads);
int  yuv420_to_rgb_i_avx2    (uint8_t* input, uint8_t* output, uint16_t width, uint16_t height);
int  yuv420_to_rgb_i_sse41   (uint8_t* input, uint8_t* output, uint16_t width, uint16_t height);
void yuv420_to_rgb_i_c       (uint8_t* input, uint8_t* output, uint16_t width, uint16_t height);

int  rgb_to_yuv420_i_sse41_mt(uint8_t* input, uint8_t* output, int width, int height, int threads);
int  rgb_to_yuv420_i_sse41   (uint8_t* input, uint8_t* output, int width, int height);
void rgb_to_yuv420_i_c       (uint8_t* input, uint8_t* output, uint16_t width, uint16_t height);

void yuyv_to_yuv420_c        (uint8_t* input, uint8_t* output, uint16_t width, uint16_t height);
