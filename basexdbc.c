/**
 * basexdbc.c : communicate with BaseX database server
 * Works with BaseX 7.x and with BaseX 8.0 and later
 *
 * Copyright (c) 2005-12, Alexander Holupirek <alex@holupirek.de>, BSD license
 *
 * Significant Changes:
 * 11 Dec 2016 - Craig Phillips <github.com/smallfriex> - to support newer authentication
 *
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>

#include "basexdbc.h"
#include "md5.h"
#include "readstring.h"

static int send_db(void* socket, const char *buf, size_t buf_len);
static int basex_status(void* socket);

#ifdef DEBUG
	#define WARNF(...) printf(__VA_ARGS__)
#else
	#define WARNF(...)
#endif

/**
 * Connect to host on port using stream sockets.
 *
 * @param host string representing host to connect to
 * @param port string representing port on host to connect to
 * @return socket file descriptor or -1 in case of failure
 */
void*
basex_connect(const char *host, const char *port)
{
	if(SDL_Init(0) == -1)
	{
		printf("SDL_Init: %s\n", SDL_GetError());
		return NULL;
	}

	if(SDLNet_Init() == -1)
	{
		printf("SDLNet_Init: %s\n", SDLNet_GetError());
		return NULL;
	}

	if (host == NULL || port == NULL)
	{
		printf("Missing hostname '%s' / port '%s'.\n", host, port);
		return NULL;
	}

	IPaddress ip;
	if (SDLNet_ResolveHost(&ip, host, atoi(port)) == -1)
	{
		printf("SDLNet_ResolveHost: %s\n", SDLNet_GetError());
		return NULL;
	}
	return SDLNet_TCP_Open(&ip);
}

/**
 * Authenticate against BaseX server connected on sfd using user and passwd.
 *
 * Authentication as defined by BaseX transfer protocol (BaseX 7.0 ff.):
 * https://github.com/BaseXdb/basex-api/blob/master/etc/readme.txt
 * {...} = string; \n = single byte
 *
 *   1. Client connects to server socket (basex_connect)
 *   2. Server sends timestamp: {timestamp} \0
 *   3. Client sends username and hash:
 *      {username} \0 {md5(md5(password) + timestamp)} \0
 *   4. Server sends \0 (success) or \1 (error)
 *
 * @param sfd socket file descriptor successfully connected to BaseX server
 * @param user string with database username
 * @param passwd string with password for database username
 * @return 0 in case of success, -1 in case of failure
 */
int
basex_authenticate(void* socket, const char *user, const char *passwd)
{
	char ts[BUFSIZ]; /* timestamp returned by basex. */
	char *md5_pwd;   /* md5'ed passwd */
	int ts_len, rc, i;

	/* Right after the first connect BaseX returns a nul-terminated
		 * timestamp string. */
	memset(ts, 0, BUFSIZ);
	rc = SDLNet_TCP_Recv(socket, &ts, BUFSIZ);
	if (rc <= 0) {
		printf("Reading timestamp failed. SDLNet_TCP_Recv : %s\n", SDLNet_GetError());
		return -1;
	}
	ts_len = strlen(ts);

	WARNF("timestamp       : %s (%d)\n", ts, strlen(ts));

	/* BaseX Server expects an authentification sequence:
		   {username}\0{md5(md5(user:realm:password) + timestamp)}\0 */
	/* legacy - 
		/* {username}\0{md5(md5(password) + timestamp)}\0 */

	/* Send {username}\0 */
	int user_len = strlen(user) + 1;
	rc = SDLNet_TCP_Send(socket, user, user_len);
	if (rc < user_len) {
		printf("Sending username failed. SDLNet_TCP_Recv : %s\n", SDLNet_GetError());
		return -1;
	}

	char* p = strchr(ts,':');
	char* t;
	if (!p) {
		/* legacy login */
		t = ts;
		/* Compute md5 for passwd. */
		md5_pwd = md5(passwd);
		if (md5_pwd == NULL) {
			printf("md5 computation for password failed.\n");
			return -1;
		}
	}
	else {
		/* v8.0+ login */
		t = p + 1;
		/* Compute md5 for codeword. */
		int user_len = strlen(user);
		int pass_len = strlen(passwd);
		int realm_len = p - ts;
		char codewd[user_len + realm_len + pass_len + 3];
		strncpy(codewd, user, user_len);
		codewd[user_len] = ':';
		strncpy(codewd + user_len + 1, ts, realm_len);
		codewd[user_len + 1 + realm_len] = ':';
		strncpy(codewd + user_len + 1 + realm_len + 1, passwd, pass_len);
		codewd[user_len + 1 + realm_len + 1 + pass_len] = '\0';
		md5_pwd = md5(codewd);
		if (md5_pwd == NULL) {
			printf("md5 computation for password failed.\n");
			return -1;
		}
		ts_len = ts_len - realm_len -1;
	}
	int md5_pwd_len = strlen(md5_pwd);
		
	WARNF("md5(pwd)        : %s (%d)\n", md5_pwd, md5_pwd_len);
	
	/* Concat md5'ed codewd string and timestamp/nonce string. */
	int pwdts_len = md5_pwd_len + ts_len + 1;
	char pwdts[pwdts_len];
	memset(pwdts, 0, sizeof(pwdts));
	for (i = 0; i < md5_pwd_len; i++)
		pwdts[i] = md5_pwd[i];
	int j = md5_pwd_len;
	for (i = 0; i < ts_len; i++,j++)
		pwdts[j] = t[i];
	pwdts[pwdts_len - 1] = '\0';

	WARNF("md5(pwd)+ts     : %s (%d)\n", pwdts, strlen(pwdts));

	/* Compute md5 for md5'ed codeword + timestamp */
	char *md5_pwdts = md5(pwdts);
	if (md5_pwdts == NULL) {
		printf("md5 computation for password + timestamp failed.\n");
		return -1;
	}
	int md5_pwdts_len = strlen(md5_pwdts);

	WARNF("md5(md5(pwd)+ts): %s (%d)\n", md5_pwdts, md5_pwdts_len);

	/* Send md5'ed(md5'ed codeword + timestamp) to basex. */
	rc = SDLNet_TCP_Send(socket, md5_pwdts, md5_pwdts_len + 1);  // also send '\0'
	if (rc <= 0) {
		printf("Sending credentials failed.\n");
		return -1;
	}

	free(md5_pwd);
	free(md5_pwdts);

	/* Retrieve authentification status. */
	rc = basex_status(socket);
	if (rc == -1) {
		printf("Reading authentification status failed.\n");
		return -1;
	}
	if (rc != 0) {
		printf("Authentification failed\n");
		return -1;
	}

	WARNF("Authentification succeeded.\n");

	return 0;
}

/**
 * Read status single byte from socket.
 */
int
basex_status(void* socket)
{
	char c;
	if (SDLNet_TCP_Recv(socket, &c, 1) <= 0) {
		return -1;
	}
	return c;
}

/**
 * Executes a command and returns a result string and an info/error string.
 *
 * A database command is sent to BaseX server connected on sfd.
 * The result is a \0 terminated, dynamically allocated string, which is placed
 * at the given result address or NULL.  The same holds for the processing
 * information stored at info.
 *
 * In either case it is the responsibility of the caller to free(3) those
 * strings.
 *
 * The returned int is 0 if the command could be processed successfully, in that
 * case the result contains the result string of the command and info holds
 * the processing information.
 * If a value >0 is returned, the command could not be processed successfully,
 * result contains NULL and info contains the database error message.
 * If -1 is interned, an error occurred, result and info are set to NULL.
 *
 *  int | result* | info* |
 * -----+---------+-------|
 *  -1  |  NULL   | NULL  |
 *   0  | result  | info  |
 *  >0  |  NULL   | error |
 *
 *  * strings shall be free(3)'ed by caller
 *
 * BaseX C/S protocol:
 *
 * client sends: {command} \0
 * server sends: {result}  \0 {info}  \0 \0
 *            or           \0 {error} \0 \1
 *
 * @param sfd socket file descriptor connected to BaseX server
 * @param command to be processed by BaseX server
 * @param result address at which result from BaseX server is placed
 * @param info address at which info/error message from BaseX server is placed
 * @return int 0 for success (result and info contain strings sent from BaseX)
 * -1 in case of failure (result and info are set to NULL), >0 an error occurred
 * while processing the command (result contains NULL, info contains error
 * message)
 */
int
basex_execute(void* socket, const char *command, char **result, char **info)
{
	int rc;

	/* Send {command}\0 to server. */
	rc = send_db(socket, command, strlen(command) + 1);
	if (rc == -1) {
		printf("Can not send command '%s' to server.\n", command);
		goto err;
	}

	/* --- Receive from server:  {result} \0 {info}  \0 \0
	 *                                    \0 {error} \0 \1 */
	/* Receive {result} \0 */
	rc = readstring(socket, result);
	if (rc == -1) {
		printf("Can not retrieve result for command '%s' from server.\n", command);
		goto err;
	}

	WARNF("[execute] result: '%s'\n", *result);

	/* Receive {info/error} \0 .*/
	rc = readstring(socket, info);
	if (rc == -1) {
		printf("Can not retrieve info for command '%s' from server.\n", *info);
		goto err;
	}

	WARNF("[execute] info/error: '%s'\n", *info);

	/* Receive terminating \0 for success or \1 for error .*/
	rc = basex_status(socket);

	WARNF("[execute] status: '%d'\n", rc);

	if (rc == -1) {
		printf("Can not retrieve status.");
		goto err;
	}
	if (rc == 1) {
		WARNF("BaseX error message : %s\n", *info);
		free(*result);
		*result = NULL;
	}

	assert(rc == 0 || rc == 1);
	return rc;

err:
	*result = NULL;
	*info = NULL;
	return -1;
}

/**
 * Quits database session and closes stream connection to database server.
 *
 * @param socket file descriptor for database session.
 */
void
basex_close(void* socket){

	send_db(socket, "exit", 4+1);

	SDLNet_TCP_Close(socket);

	SDLNet_Quit();

	SDL_Quit();
}

/**
 * Writes buffer buf of buf_len to socket sfd.
 *
 * @param socket file descriptor for database session.
 * @param buf to be sent to server
 * @param buf_len # of bytes in buf
 * @return 0 if all data has successfully been written to server,
 *        -1 in case of failure.
 */
static int
send_db(void* socket, const char *buf, size_t buf_len){
	ssize_t ret;

	while (buf_len != 0 && (ret = SDLNet_TCP_Send(socket, buf, buf_len)) != 0) {
		if (ret < 0) {
			printf("Can not write to server\n");
			return -1;
		}
#if DEBUG
		int i;
		printf("write: \n");
		for (i = 0; i < ret; i++)
			printf("[write] %3d : 0x%08x %4d %c\n", i, buf[i], buf[i], buf[i]);
#endif /* DEBUG */
		buf_len -= ret;
		buf += ret;
	}
	return 0;
}
