#pragma once

#ifdef __cplusplus
extern "C" {
#endif

unsigned str_levenshtein_distance(const char *str1, const char *str2);
void str_remove_excess_whitespace(char *str);

#ifdef __cplusplus
}
#endif
