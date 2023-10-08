#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __arm64

#include <tesseract/capi.h>

struct frame_data {
	uint8_t *rgba_data;
	uint32_t width;
	uint32_t height;
};

void frame_data_init(struct frame_data *frame, uint32_t width, uint32_t height);
void frame_data_destroy(struct frame_data *frame);
void detect_smash_data(TessBaseAPI *tess, struct frame_data *frame);

#endif // __arm64

#ifdef __cplusplus
}
#endif
