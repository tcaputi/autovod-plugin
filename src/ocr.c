#include <tesseract/capi.h>
#include <leptonica/allheaders.h>
#include <obs-module.h>
#include <plugin-support.h>
#include "ocr.h"
#include "string-utils.h"
#include "img-utils.h"

#ifdef __arm64

static TessBaseAPI *tess = NULL;

void ocr_init(void)
{
	int ret;

	tess = TessBaseAPICreate();
	if (!tess) {
		goto error;
	}

	ret = TessBaseAPIInit3(tess, NULL, "eng");
	if (ret != 0) {
		obs_log(LOG_ERROR, "Failed to initialize tesseract");
		goto error;
	}

	TessBaseAPISetPageSegMode(tess, PSM_SINGLE_BLOCK);
	TessBaseAPISetSourceResolution(tess, 700);
	TessBaseAPISetVariable(tess, "language_model_penalty_non_dict_word", "0");
	TessBaseAPISetVariable(tess, "tessedit_char_whitelist",
			       "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789&./- ");

	return;

error:
	obs_log(LOG_ERROR, "Failed to initialize tesseract");
	if (tess) {
		TessBaseAPIDelete(tess);
		tess = NULL;
	}
}

void ocr_destroy(void)
{
	TessBaseAPIEnd(tess);
	TessBaseAPIDelete(tess);
	tess = NULL;
}

char *ocr_analyze_for_text(struct frame_data *frame)
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
	char *text = TessBaseAPIGetUTF8Text(tess);
	str_remove_excess_whitespace(text);

	pixDestroy(&pixs);
	return text;
}

#else // __arm64

void ocr_init(void) {}

void ocr_destroy(void) {}

char *ocr_analyze_for_text(struct frame_data *frame)
{
	(void)frame;
	return NULL;
}

#endif // __arm64
