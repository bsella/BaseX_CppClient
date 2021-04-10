/* Copyright (c) 2005-12, Alexander Holupirek <alex@holupirek.de>, BSD license */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL_net.h>

#include "readstring.h"

static size_t READSTRING_MAX = 1024 * 1024 * 10; // 10MB

/**
 * Reads string from file descriptor into dynamically allocated string.
 *
 * A variable length of characters is read from fd until a \0 byte
 * is detected or a predefined maximum READSTRING_MAX is reached.
 * The read bytes are stored into a dynamically allocated buffer str.
 * It is the responsibility of the caller to free(3) str.
 *
 * @param fd file descriptor to read from
 * @param str address of the newly allocated c string
 * @return number of characters read or -1 in case of failure
 *         in case of an error str is set to NULL
 */
int
readstring(void* socket, char **str)
{
	char b;
	size_t chars = 0;  // # of stored chars in str
	size_t size  = 32; // start capacity of alloc'ed string

	// allocate default capacity
	*str = calloc(size, sizeof(char));
	if (str == NULL) {
		printf("malloc failed.\n");
		return -1;
	}

	// read until \0 is detected or predefined maximum is reached
	while(1) {
		if (!(chars < size - 1)) { // reallocate
			if (size && 2 > SIZE_MAX / size) {
				errno = ENOMEM;
				printf("overflow\n");
				goto err;
			}
			size_t newsize = size * 2;
			if (newsize < READSTRING_MAX) {
				char *newstr;
				if ((newstr = realloc(*str, newsize)) == NULL) {
					printf("reallocation failed.\n");
					goto err;
				}
				*str  = newstr;
				size = newsize;
			} else {
				errno = ENOBUFS;
				printf("variable string exceeds maximum of %zu\n"
					, READSTRING_MAX);
				goto err;
			}
		}
		if (SDLNet_TCP_Recv(socket, &b, 1) <= 0) {
			printf("Cannot read\n");
			goto err;
		}
		// store another read char
		*((*str) + chars) = b;
		chars++;
		if (b == '\0') {  // We are done.
			break;
		}
	}
	
	return chars;

err:
	free(*str);
	*str = NULL;
	return -1;
}
