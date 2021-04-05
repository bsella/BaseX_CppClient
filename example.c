/* Copyright (c) 2005-12, Alexander Holupirek <alex@holupirek.de>, BSD license */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "basexdbc.h"
/* once libbasexdbc.so is installed in /usr/include/basex/ use:
#include "basex/basexdbc.h"
*/

#define DBHOST   "localhost"
#define DBPORT   "1984"
#define DBUSER   "admin"
#define DBPASSWD "admin"

/*
 * Example to demonstrate communication with running BaseX database server.
 *
 * $ cc -L. -lbasexdbc example.c -o example
 */
int
main(void)
{
	/* Connect to server and receive socket descriptor for this session. */
	void* socket = basex_connect(DBHOST, DBPORT);
	if (!socket) {
		printf("Can not connect to BaseX server.\n");
		return 0;
	}

	/* We are connected, let's authenticate for this session. */
	if (basex_authenticate(socket, DBUSER, DBPASSWD) == -1) {
		printf("Access to DB denied.\n");
		goto out;
	}

	/* Send command in default mode and receive the result string. */
	const char *command = "xquery 1 + 1";
	char *result;
	char *info;
	int rc = basex_execute(socket, command, &result, &info);
	if (rc == -1) { // general (i/o or the like) error
		printf("An error occurred during execution of '%s'.\n", command);
		goto free_and_out;		
	}
	if (rc == 1) { // database error while processing command
		printf("Processing of '%s' failed.\n", command);
	}

	/* print command, result and info/error */
	printf("command: '%s'\n", command);
	printf("result : '%s'\n", result);
	printf("%s : '%s'\n", (rc == 1) ? "error" : "info", info);

free_and_out:
	free(result);
	free(info);
out:
	basex_close(socket);
	return 0;
}
