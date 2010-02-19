/* Copyright 2010 Nick Johnson */

#include <string.h>
#include <stdint.h>

char *strcpy(char *d, const char *s) {
	size_t i;

	for (i = 0; s[i] != '\0'; i++) {
		d[i] = s[i];
	}

	return d;
}