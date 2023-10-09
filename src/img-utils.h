#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

struct frame_data {
	uint8_t *rgba_data;
	uint32_t width;
	uint32_t height;
};

struct expected_pixel_area {
	uint8_t rgba[4];
	uint8_t pixel_threshold;
	uint32_t startx;
	uint32_t endx;
	uint32_t starty;
	uint32_t endy;
};

float img_check_expected_pixels(struct frame_data *frame, struct expected_pixel_area *area);
void img_write_png(struct frame_data *frame, const char *filename);
void frame_data_init(struct frame_data *frame, uint32_t width, uint32_t height);
void frame_data_destroy(struct frame_data *frame);

#ifdef __cplusplus
}
#endif
