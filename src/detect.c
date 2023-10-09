#include <tesseract/capi.h>
#include <leptonica/allheaders.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include "detect.h"

#ifdef __arm64

#define NUM_SMASH_CHARACTERS 2

static struct expected_pixel_area loadin_screen_detector[] = {
	// GREY AREAS
	{
		// top left corner
		.rgba = {0x36, 0x43, 0x48, 0xFF},
		.pixel_threshold = 28,
		.startx = 0,
		.endx = 16,
		.starty = 0,
		.endy = 1,
	},
	{
		// top left center
		.rgba = {0x36, 0x43, 0x48, 0xFF},
		.pixel_threshold = 28,
		.startx = 480,
		.endx = 496,
		.starty = 0,
		.endy = 1,
	},
	{
		// top right center
		.rgba = {0x36, 0x43, 0x48, 0xFF},
		.pixel_threshold = 28,
		.startx = 1440,
		.endx = 1456,
		.starty = 0,
		.endy = 1,
	},
	{
		// center,
		.rgba = {0x36, 0x43, 0x48, 0xFF},
		.pixel_threshold = 28,
		.startx = 968,
		.endx = 984,
		.starty = 0,
		.endy = 1,
	},
	// BLACK AREAS
	{
		// top left corner, beginning of name background
		.rgba = {0x0f, 0x10, 0x18, 0xFF},
		.pixel_threshold = 28,
		.startx = 0,
		.endx = 1,
		.starty = 80,
		.endy = 96,
	},
	{
		// center of name background
		.rgba = {0x0f, 0x10, 0x18, 0xFF},
		.pixel_threshold = 28,
		.startx = 960,
		.endx = 961,
		.starty = 80,
		.endy = 96,
	},
};

//for testing
const char *ASCII_CHARS = "@%#*+=-:. ";

void print_frame_to_ascii_file(struct frame_data *frame, const char *filename)
{
	FILE *file = fopen(filename, "w");
	if (!file) {
		fprintf(stderr, "Failed to open file %s for writing.\n", filename);
		return;
	}

	for (uint32_t y = 0; y < frame->height; y++) {
		for (uint32_t x = 0; x < frame->width; x++) {
			uint32_t index = (y * frame->width + x) * 4; // 4 for RGBA

			// Average the R, G, and B values to get the brightness
			uint8_t brightness =
				(frame->rgba_data[index] + frame->rgba_data[index + 1] +
				 frame->rgba_data[index + 2]) /
				3;

			// Map brightness to an ASCII character
			char pixel_char =
				ASCII_CHARS[(brightness * (strlen(ASCII_CHARS) - 1)) / 255];

			fputc(pixel_char, file);
			fputc(pixel_char, file); // Repeat for better horizontal resolution
		}
		fputc('\n', file);
	}

	fclose(file);
}

void write_png(struct frame_data *frame, const char *filename)
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

static char *analyze_for_text(TessBaseAPI *tess, struct frame_data *frame)
{
	PIX *pixs = pixCreate(frame->width, frame->height, 32); // 32 for RGBA
	l_uint32 *lines = pixGetData(pixs);

	for (uint32_t y = 0; y < frame->height; y++) {
		for (uint32_t x = 0; x < frame->width; x++) {
			uint32_t src_index = (y * frame->width + x) * 4; // 4 for RGBA

			lines[y * frame->width + x] =
				(frame->rgba_data[src_index + 3] << 24) | // Alpha
				(frame->rgba_data[src_index + 2] << 16) | // Red
				(frame->rgba_data[src_index + 1] << 8) |  // Green
				(frame->rgba_data[src_index]);            // Blue
		}
	}

	TessBaseAPISetImage2(tess, pixs);
	TessBaseAPIRecognize(tess, NULL);

	TessResultIterator *ri = TessBaseAPIGetIterator(tess);
	TessPageIteratorLevel level = RIL_BLOCK;

	do {
		const char *text = TessResultIteratorGetUTF8Text(ri, level);
		obs_log(LOG_INFO, "Text: %s", text);
		TessDeleteText(text);
	} while (TessResultIteratorNext(ri, level));

	TessResultIteratorDelete(ri);
	pixDestroy(&pixs);

	//TODO: temporary to fit the rest of the code
	char *out_text = malloc(1);
	out_text[0] = '\0';
	return out_text;
}

static bool compare_pixel_colors(uint8_t *color1, uint8_t *color2, uint8_t threshold)
{
	for (uint32_t i = 0; i < 4; i++) {
		if (abs(color1[i] - color2[i]) > threshold)
			return false;
	}
	return true;
}

static float check_expected_pixels(struct frame_data *frame, struct expected_pixel_area *area)
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

	obs_log(LOG_INFO, "Matched pixels: %d, total_pixels: %d", matched_pixels, total_pixels);
	return (float)matched_pixels / (float)total_pixels;
}

static bool is_loadin_screen(struct frame_data *frame)
{
	float matches = 0.0f;
	uint32_t num_areas = sizeof(loadin_screen_detector) / sizeof(struct expected_pixel_area);

	for (uint32_t i = 0; i < num_areas; i++) {
		matches += check_expected_pixels(frame, &loadin_screen_detector[i]);
	}

	obs_log(LOG_INFO, "matches: %f, num areas: %d, result: %f", matches, num_areas,
		matches / (float)num_areas);
	return matches / (float)num_areas > 0.6f;
}

static void get_character_name_image(struct frame_data *in_frame, struct frame_data *out_frame,
				     uint32_t startx, uint32_t endx, uint32_t starty, uint32_t endy)
{
	for (uint32_t y = starty; y < endy; y++) {
		for (uint32_t x = startx; x < endx; x++) {
			uint32_t in_index = (y * in_frame->width + x) * 4;
			uint32_t out_index = ((y - starty) * out_frame->width + x - startx) * 4;

			uint8_t r = in_frame->rgba_data[in_index + 0];
			uint8_t g = in_frame->rgba_data[in_index + 1];
			uint8_t b = in_frame->rgba_data[in_index + 2];

			if (r >= 200 && g >= 200 && b >= 200) {
				// close enough to white becomes black
				out_frame->rgba_data[out_index + 0] = 0;   // R
				out_frame->rgba_data[out_index + 1] = 0;   // G
				out_frame->rgba_data[out_index + 2] = 0;   // B
				out_frame->rgba_data[out_index + 3] = 255; // A
			} else {
				// everything else becomes white
				out_frame->rgba_data[out_index + 0] = 255; // R
				out_frame->rgba_data[out_index + 1] = 255; // G
				out_frame->rgba_data[out_index + 2] = 255; // B
				out_frame->rgba_data[out_index + 3] = 255; // A
			}
		}
	}
}

static int png_count = 11;

static void get_character_name_boxes(struct frame_data *in_frame, struct frame_data *out_frames)
{
	frame_data_init(&out_frames[0], in_frame->width * 3 / 8, in_frame->height / 8);
	get_character_name_image(in_frame, &out_frames[0],
				 in_frame->width * 1 / 16, // startx
				 in_frame->width * 7 / 16, // endx
				 0,                        // starty
				 in_frame->height * 1 / 8  // endy
	);

	frame_data_init(&out_frames[1], in_frame->width * 3 / 8, in_frame->height / 8);
	get_character_name_image(in_frame, &out_frames[1],
				 in_frame->width * 9 / 16,  // startx
				 in_frame->width * 15 / 16, // endx
				 0,                         // starty
				 in_frame->height * 1 / 8   // endy
	);

	// for debugging
	if (png_count > 10) {
		obs_log(LOG_INFO, "Writing PNG files");
		write_png(&out_frames[0], "/Users/Tom/Desktop/character0.png");
		write_png(&out_frames[1], "/Users/Tom/Desktop/character1.png");
		write_png(in_frame, "/Users/Tom/Desktop/both.png");
		png_count = 0;
	} else {
		png_count++;
	}
}

void detect_smash_data(TessBaseAPI *tess, struct frame_data *frame)
{
	struct frame_data name_boxes[NUM_SMASH_CHARACTERS] = {0};

	bool should_detect_chars = is_loadin_screen(frame);
	if (!should_detect_chars)
		return;

	obs_log(LOG_INFO, "--------------------------------------------------");
	obs_log(LOG_INFO, "LOADIN SCREEN DETECTED");
	get_character_name_boxes(frame, name_boxes);

	for (int i = 0; i < NUM_SMASH_CHARACTERS; i++) {
		char *text = analyze_for_text(tess, &name_boxes[i]);
		frame_data_destroy(&name_boxes[i]);
		//TODO: do something with the text
		free(text);
	}
}

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

#endif // __arm64
