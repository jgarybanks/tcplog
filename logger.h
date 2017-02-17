#ifndef _LOG_LOGGER_H
#define _LOG_LOGGER_H

#define _GNU_SOURCE 1
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include <pwd.h>
#include <signal.h>
#include <syslog.h>
#include <sys/timex.h>
#include <sys/uio.h>

#define NILVALUE "-"
#define LOG_LEN_MAX 4096 /* syslog TCP RFC */

struct log_logger {
	int fd;
	int pri;
	int inc;
	pid_t pid;
	char *hdr;
	char *tag;
	char *server;
	char *port;
};

struct log_logger *log_logopen(char*, char*, char*, int);
void log_logmsg(struct log_logger*, int, char *);
void log_logclose(const struct log_logger*);

#endif /* _LOG_LOGGER_H */
