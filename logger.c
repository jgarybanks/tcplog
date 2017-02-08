#include "logger.h"

static int
inet_socket(const char *servername, const char *port)
{
	int fd, errcode;
	struct addrinfo hints, *res;
	const char *p = port;

	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	if (port == NULL)
		p = "syslog-conn";

	hints.ai_family = AF_UNSPEC;
	errcode = getaddrinfo(servername, p, &hints, &res);
	if (errcode != 0) {
		fprintf(stderr, "failed to resolve name %s port %s: %s",
		     servername, p, strerror(errcode));
		return -1;
	}
	if ((fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) == -1) {
		freeaddrinfo(res);
		fprintf(stderr, "Unable to get socket for name %s port %s: %s",
		     servername, p, strerror(errno));
		return -1;
	}
	if (connect(fd, res->ai_addr, res->ai_addrlen) == -1) {
		fprintf(stderr, "Unable to connect : %s\n", strerror(errno));
		freeaddrinfo(res);
		return -1;
	}
	freeaddrinfo(res);

	if (fd == -1) {
		fprintf(stderr, "failed to connect to %s port %s", servername, p);
		return -1;
	}

	return fd;
}

#define next_iovec(ary, idx) __extension__ ({		\
		assert(idx >= 0);			\
		&ary[idx++];				\
})

#define iovec_add_string(ary, idx, str, len)		\
	do {						\
		struct iovec *v = next_iovec(ary, idx);	\
		v->iov_base = (void *) str;		\
		v->iov_len = len ? len : strlen(str);	\
	} while (0)

#define iovec_memcmp(ary, idx, str, len)		\
		memcmp((ary)[(idx) - 1].iov_base, str, len)

/*
** Write buffer to dest. Use RFC6587
*/
static void
writeout(const struct log_logger *ctl, const char *const msg)
{
	struct iovec iov[4];
	int iovlen = 0;
	char *octet = NULL;
	const char *cp = msg;
	struct msghdr message = { 0 };
	size_t len;
	int end;
	
	while (*cp && *cp == '\n') {
		cp++;
	}
	end = strlen(cp);

	/*
	** octet cnt, hdr, msg, finish with \n
	*/
	
	len = asprintf(&octet, "%zu ", strlen(ctl->hdr) + end);

	iovec_add_string(iov, iovlen, octet, len);
	iovec_add_string(iov, iovlen, ctl->hdr, 0);
	iovec_add_string(iov, iovlen, cp, 0);
	if (cp[end] != '\n') {
		iovec_add_string(iov, iovlen, "\n", 1);
	}

	message.msg_iov = iov;
	message.msg_iovlen = iovlen;

	if (sendmsg(ctl->fd, &message, 0) < 0) {
		fprintf(stderr, "send message failed on %d: %s\n",
				ctl->fd, strerror(errno));
	}

	free(octet);
}

static void
buildheader(struct log_logger *const ctl)
{
	char *time;
	char hostname[NAME_MAX];
	char *const app_name = ctl->tag;
	char *procid;
	struct timeval tv;
	struct tm *tm;

	if (ctl->fd < 0)
		return;

	logger_gettimeofday(&tv, NULL);
	if ((tm = localtime(&tv.tv_sec)) != NULL) {
		char fmt[64];
		const size_t i = strftime(fmt, sizeof(fmt),
					  "%Y-%m-%dT%H:%M:%S.%%06u%z ", tm);
		fmt[i - 1] = fmt[i - 2];
		fmt[i - 2] = fmt[i - 3];
		fmt[i - 3] = ':';
		asprintf(&time, fmt, tv.tv_usec);
	} else {
		fprintf(stderr, "localtime() failed");
		return;
	}

	gethostname(hostname, NAME_MAX);

	if (48 < strlen(ctl->tag)) {
		fprintf(stderr, "tag '%s' is too long", ctl->tag);
		return;
	}

	if (ctl->pid)
		asprintf(&procid, "%d", ctl->pid);
	else
		procid = strdup(NILVALUE);

	asprintf(&ctl->hdr, "<%d>1 %s %s %s %s %s ",
		ctl->pri,
		time,
		hostname,
		app_name,
		procid,
		NILVALUE);

	free(time);
	/* app_name points to ctl->tag, do NOT free! */
	free(procid);
	ctl->inc += 1;
}

static void
logger_open(char *progname, struct log_logger *ctl)
{
	ctl->fd = inet_socket(ctl->server, ctl->port);
	if (!ctl->tag) {
		ctl->tag = progname;
	}
	buildheader(ctl);
}

void
log_logclose(const struct log_logger *ctl)
{
	if (ctl->fd != -1 && close(ctl->fd) != 0) {
		fprintf(stderr, "close failed");
		return;
	}
	free(ctl->hdr);
}

struct log_logger *
log_logopen(char *progname, char *server, char *port, int pri)
{
	struct log_logger *ctl;
	
	ctl = calloc(sizeof(*ctl), 1);
	ctl->fd = -1;
	ctl->pid = getpid();
	ctl->pri = pri;
	ctl->inc = 0;
	ctl->tag = progname;
	ctl->server = server;
	ctl->port = port;
	ctl->hdr = NULL;

	logger_open(progname, ctl);
	return ctl;
}

void
log_logmsg(struct log_logger *ctl, int lvl, char *_msg)
{
	char msg[LOG_LEN_MAX];

	if (!ctl || ctl->fd == -1 || *_msg) {
		return;
	}

	snprintf(msg, LOG_LEN_MAX, "%d LVL[%d] %s",
			ctl->inc, lvl, _msg);
	buildheader(ctl);
	writeout(ctl, msg);
}

#ifdef _T
int
main(int argc, char *argv[])
{
	struct log_logger *ctl;
	
	ctl = log_logopen(argv[0], "localhost", "514", LOG_LOCAL0 | LOG_ERR);
	assert(ctl);
	log_logmsg(ctl, 10, "Test msg lvl 10 no nl");
	log_logmsg(ctl, 5, "Test msg lvl 5 with nl\n");
	log_logmsg(ctl, 100, "\nTest msg lvl 100 beg nl");
	log_logmsg(ctl, 0, "Test msg lvl 0 high 铬");
	log_logmsg(ctl, 10, NULL);
	log_logmsg(ctl, 10, "Test msg lvl 10 after NULL");
	log_logmsg(ctl, -1, "Test msg lvl -1");
	log_logmsg(NULL, 10, "Test msg NULL ctl");
	log_logmsg(ctl, 10, "Test msg lvl 10 after NULL 2");
	log_logclose(ctl);
	return 0;
}

#endif /* _T */

