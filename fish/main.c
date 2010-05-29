/* 
 * Copyright 2009, 2010 Nick Johnson 
 * ISC Licensed, see LICENSE for details
 */

#include <flux/arch.h>
#include <flux/vfs.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
	char buffer[100];
	char fbuffer[1000];
	FILE *file;
	size_t i;
	
	printf("\n");

	while (1) {
		printf("$ ");

		fgets(buffer, 100, stdin);

		for (i = 0; buffer[i]; i++) {
			if (buffer[i] == '\n') {
				buffer[i] = '\0';
			}
		}

		file = fopen(buffer, "r");

		if (file) {
			info(file->filedes, fbuffer, "size");

			printf("%s: found, size %s bytes\n", buffer, fbuffer);

			fclose(file);
		}
		else {
			printf("%s: not found\n", buffer);
		}
	}

	return 0;
}