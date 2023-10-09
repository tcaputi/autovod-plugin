#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "img-utils.h"

void ocr_init(void);
void ocr_destroy(void);
char *ocr_analyze_for_text(struct frame_data *frame);

#ifdef __cplusplus
}
#endif
