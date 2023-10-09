#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "string-utils.h"

static int min(int a, int b, int c)
{
	int min = a;
	if (b < min)
		min = b;
	if (c < min)
		min = c;
	return min;
}

unsigned str_levenshtein_distance(const char *str1, const char *str2)
{
	uint32_t len1 = (uint32_t)strlen(str1);
	uint32_t len2 = (uint32_t)strlen(str2);
	uint32_t *col = malloc((len2 + 1) * sizeof(int));
	uint32_t *prevCol = malloc((len2 + 1) * sizeof(int));

	for (uint32_t i = 0; i <= len2; i++) {
		prevCol[i] = i;
	}

	for (uint32_t i = 0; i < len1; i++) {
		col[0] = i + 1;
		for (uint32_t j = 0; j < len2; j++) {
			col[j + 1] = min(prevCol[j + 1] + 1, col[j] + 1,
					 prevCol[j] + (str1[i] == str2[j] ? 0 : 1));
		}
		uint32_t *temp = col;
		col = prevCol;
		prevCol = temp;
	}
	uint32_t result = prevCol[len2];
	free(col);
	free(prevCol);
	return result;
}

void str_remove_excess_whitespace(char *str)
{
	int write_index = 0; // Where to write the next character
	bool last_was_space = false;

	for (int read_index = 0; str[read_index]; read_index++) {
		if (isspace(str[read_index])) {
			if (!last_was_space) { // Only write a space if the last written character wasn't a space
				str[write_index++] = ' ';
				last_was_space = true;
			}
		} else {
			str[write_index++] = str[read_index];
			last_was_space = false;
		}
	}

	if (write_index != 0 && str[write_index - 1] == ' ') // Remove trailing space
		write_index--;

	str[write_index] = '\0'; // Null terminate the string
}
