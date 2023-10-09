#include "img-utils.h"
#include <png.h>
#include <obs-module.h>
#include <plugin-support.h>

#ifdef __arm64

static bool compare_pixel_colors(uint8_t *color1, uint8_t *color2, uint8_t threshold)
{
	for (uint32_t i = 0; i < 4; i++) {
		if (abs(color1[i] - color2[i]) > threshold)
			return false;
	}
	return true;
}

float img_check_expected_pixels(struct frame_data *frame, struct expected_pixel_area *area)
{
	uint32_t total_pixels = (area->endx - area->startx) * (area->endy - area->starty);
	uint32_t matched_pixels = 0;

	for (uint32_t y = area->starty; y < area->endy; y++) {
		for (uint32_t x = area->startx; x < area->endx; x++) {
			uint32_t index = (y * frame->width + x) * 4;

			if (compare_pixel_colors(&frame->rgba_data[index], area->rgba,
						 area->pixel_threshold)) {
				matched_pixels++;
			}
		}
	}

	return (float)matched_pixels / (float)total_pixels;
}

void img_write_png(struct frame_data *frame, const char *filename)
{
	FILE *fp = NULL;
	png_structp png = NULL;
	png_infop info = NULL;

	fp = fopen(filename, "wb");
	if (!fp) {
		goto error;
	}

	png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png) {
		goto error;
	}

	info = png_create_info_struct(png);
	if (!info) {
		goto error;
	}

	png_init_io(png, fp);

	png_set_IHDR(png, info, frame->width, frame->height, 8, PNG_COLOR_TYPE_RGBA,
		     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png, info);

	for (uint32_t y = 0; y < frame->height; y++) {
		png_write_row(png, &frame->rgba_data[y * frame->width * 4]);
	}

	png_write_end(png, NULL);
	fclose(fp);

	if (png && info)
		png_destroy_write_struct(&png, &info);

	return;

error:
	obs_log(LOG_INFO, "Error writing PNG file");

	if (png && info)
		png_destroy_write_struct(&png, &info);
	if (png)
		png_destroy_write_struct(&png, NULL);
	if (fp)
		fclose(fp);
}

#else // __arm64

float img_check_expected_pixels(struct frame_data *frame, struct expected_pixel_area *area)
{
	(void)frame;
	(void)area;
	return 0.0f;
}

void img_write_png(struct frame_data *frame, const char *filename)
{
	(void)frame;
	(void)filename;
}

#endif // __arm64

void frame_data_init(struct frame_data *frame, uint32_t width, uint32_t height)
{
	frame->width = width;
	frame->height = height;
	frame->rgba_data = bzalloc((width + 32) * height * 4);
}

void frame_data_destroy(struct frame_data *frame)
{
	bfree(frame->rgba_data);
	frame->rgba_data = NULL;
	frame->width = 0;
	frame->height = 0;
}
