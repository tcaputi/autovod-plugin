#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

bool str_approximate_match(const char *str1, const char *str2, unsigned threshold);
void str_remove_excess_whitespace(char *str);

#ifdef __cplusplus
}
#endif
