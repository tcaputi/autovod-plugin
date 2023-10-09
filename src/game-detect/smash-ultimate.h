#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include "img-utils.h"

bool ssbu_detect_loadin_screen(struct frame_data *frame);
void ssbu_detect(struct frame_data *frame);

#ifdef __cplusplus
}
#endif
