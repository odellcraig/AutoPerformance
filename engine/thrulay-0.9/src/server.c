/*
 * server.c -- thrulay library, server API implementation.
 * 
 * Written by Stanislav Shalunov, http://www.internet2.edu/~shalunov/
 *            Bernhard Lutzmann, belu@users.sf.net
 *            Federico Montesino Pouzols, fedemp@altern.org
 * 
 * @(#) $Id: server.c,v 1.1.2.17 2006/08/20 19:30:19 fedemp Exp $
 *
 * Copyright 2003, 2006, Internet2.
 * Legal conditions are in file LICENSE
 * (MD5 = ecfa50d1b0bfbb81b658c810d0476a52).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <strings.h>
#include <signal.h>
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#include <string.h>
#include <fcntl.h>
#include <math.h>
#include <sys/types.h>
#include <sys/param.h>
#include <unistd.h>
#include <inttypes.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifndef WIN32
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/socket.h>
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#else
#define _WIN32_WINNT 0x0501   /* Will only work in Windows XP or greater */
#include <winsock2.h>
#include <ws2tcpip.h>
#define sleep(t)    _sleep(t)
#define close(sock) closesocket(sock)  /* If close is only used with sockets*/
#endif /* ndef WIN32 */
#ifdef HAVE_FLOAT_H
#include <float.h>
#endif
#include "assertd.h"
#include "util.h"
#include "reporting.h"
#include "rcs.h"
#include "server.h"

#ifndef IPV6_ADD_MEMBERSHIP
#ifdef  IPV6_JOIN_GROUP
#define IPV6_ADD_MEMBERSHIP IPV6_JOIN_GROUP
#endif
#endif

#define INDICATOR		"thrulay"
#define THRULAY_VERSION_NUMBER	"2"
#define THRULAY_GREET		"thrulay/2+"
#define LISTEN_BACKLOG		128
#define UDP_PORT		5003
#define TRY_UDP_PORTS		1000
#define UDP_BUFFER_SIZE		4194304
#define URANDOM_PATH		"/dev/urandom"


/*
 * Access Control List (ACL)
 */
#define ACL_ALLOW	1
#define ACL_DENY	0

typedef struct acl {
	struct acl *next;
	struct sockaddr_storage sa;
	int mask;
} acl_t;

acl_t *acl_head = NULL;

/* ACL Prototypes */
int 
acl_allow_add (const char *const);

acl_t *
acl_allow_add_list (acl_t *, struct sockaddr *, int);

int 
acl_check (struct sockaddr *);

/* Logging prototypes */
int
logging_init (thrulay_server_logtype_t lt);

void
logging_exit (void);

void
logging_log (int, const char *, ...);

/*
 * Logging
 */
#define LOGGING_MAXLEN	255		/* maximum string length */

void logging_log_string (int, const char *);
char *logging_time (void);

char timestr[20];
char *logstr = NULL;
int log_type = LOGTYPE_SYSLOG;		/* default is syslog */
int metrics_verbosity = 0;

RCS_ID("@(#) $Id: server.c,v 1.1.2.17 2006/08/20 19:30:19 fedemp Exp $")

const char *
thrulay_server_strerror(int errorcode)
{
	/* Be very careful with error code numbers! */
	static const char *thrulay_server_error_s[] = {
		"No error", /* 0 */
		"gettimeofday(): failed", /* 1 */
		"strdup(): failed", /* 2 */
		"getaddrinfo(): failed in UDP test", /* 3 */
		"malloc(): failed", /* 4 */
		"could not send greeting to client", /* 5 */
		"could not receive session proposal", /* 6 */
		"malformed protocol indicator in client proposal", /* 7 */
		"protocol indicator of client proposal not followed by '/'", /* 8 */
		"malformed protocol version in client proposal", /* 9 */
		"protocol version not followed by ':'", /* 10 */
		"unknown test proposal type", /* 11 */
		"malformed session proposal from client", /* 12 */
		"window size in client proposal is negative", /* 13 */
		"block size in client proposal is negative", /* 14 */
		"snprintf(): failed", /* 15 */
		"could not send TCP session reponse to client", /* 16 */
		"malformed parameters in UDP proposal", /* 17 */
		"could not bind a UDP socket", /* 18 */
		"could not open random device", /* 19 */
		"could not read urandom for UDP nonce", /* 20 */
		"could not send UDP session response to client", /* 21 */
		"error in quantile computation algorithm", /* 22 */
		"could not receive from TCP connection at the end of a UDP test", /* 23 */
		"malformed end of UDP test", /* 24 */
		"could not send final report to client", /* 25 */
		"could not ignore SIGPIPE", /* 26 */
		"getaddrinfo() failed before listen", /* 27 */
		"unable to start server (bind failed)", /* 28 */
		"listen(): failed", /* 29 */
		"daemon(): failed", /* 30 */
		"getaddrinfo(): failed in ACL add operation", /* 31 */
		"WSAStartup failed while initializing server" /* 32 */
	};

	static const int max_thrulay_server_error = 32;

	if( 0 >= errorcode && errorcode >= -max_thrulay_server_error){
		return thrulay_server_error_s[-errorcode];
	} else {
		return NULL;
	}	
}

/* Add address to list of allowed hosts. Returns 0 on success, -1 on failure. */
int
acl_allow_add (const char *const str)
{
	struct addrinfo hints, *res;
	char *pmask = NULL;
	int mask = -1;
	int rc;

	/* Extract netmask. */
	pmask = strchr(str, '/');
	if (pmask != NULL) {
		*pmask++ = '\0';
		mask = atoi(pmask);
	}

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rc = getaddrinfo(str, NULL, &hints, &res)) != 0) {
		fprintf(stderr, "Error: getaddrinfo(): failed in ACL add "
			"operation , %s\n", gai_strerror(rc));
		return -31;
	}

	acl_head = acl_allow_add_list(acl_head, res->ai_addr, mask);

	freeaddrinfo(res);

	return 0;
}

acl_t *
acl_allow_add_list (acl_t *p, struct sockaddr *ss, int mask)
{
	if (p == NULL) {
		p = malloc(sizeof(acl_t));
		if (p == NULL) {
			perror("malloc");
			exit(1);
		}
		p->next = NULL;
		memcpy(&p->sa, ss, sizeof(struct sockaddr_storage));
		p->mask = mask;
	} else {
		p->next = acl_allow_add_list(p->next, ss, mask);
	}

	return p;
}

/* Checks if address in `sa' is allowed to connect. Returns ACL_ALLOW if
 * host is allowed to connect or ACL_DENY if host is not allowed to connect. */
int
acl_check (struct sockaddr *sa)
{
	struct sockaddr *acl_sa = NULL;
	struct sockaddr_in *sin = NULL, *acl_sin = NULL;
	struct sockaddr_in6 *sin6 = NULL, *acl_sin6 = NULL;
	acl_t *acl = NULL;
	int allow, i;

	/* If there are no hosts in ACL, then allow connection. */
	if (acl_head == NULL) {
		return ACL_ALLOW;
	}

	/* Check client sockaddr against entries in ACL */
	for (acl = acl_head; acl != NULL; acl = acl->next) {

		acl_sa = (struct sockaddr *)&acl->sa;

		/* Check that type matches. */
		if (sa->sa_family != acl_sa->sa_family) {
			continue;
		}

		switch (sa->sa_family) {
		case AF_INET:
			/* IPv4 address */
			sin = (struct sockaddr_in *)sa;
			acl_sin = (struct sockaddr_in *)acl_sa;

			/* Mask has been set to -1 if no netmask has been
			 * supplied. So we set default netmask here if
			 * that is the case. */
			if (acl->mask == -1) {
				acl->mask = 32;
			}

			/* Sanity check for mask. */
			if (acl->mask < 1 || acl->mask > 32) {
				error(ERR_WARNING, "Error: Bad netmask.");
				break;
			}

			if ((ntohl(sin->sin_addr.s_addr) >>
						(32 - acl->mask)) ==
					(ntohl(acl_sin->sin_addr.s_addr) >>
					 (32 - acl->mask))) {
				return ACL_ALLOW;
			}

			break;
		case AF_INET6:
			/* IPv6 address */
			sin6 = (struct sockaddr_in6 *)sa;
			acl_sin6 = (struct sockaddr_in6 *)acl_sa;

			/* Mask has been set to -1 if no netmask has been
			 * supplied. So we set default netmask here if
			 * that is the case. */
			if (acl->mask == -1) {
				acl->mask = 128;
			}

			/* Sanity check for netmask. */
			if (acl->mask < 1 || acl->mask > 128) {
				error(ERR_WARNING, "Error: Bad netmask.");
				break;
			}

			allow = 1;

			for (i = 0; i < (acl->mask / 8); i++) {
				if (sin6->sin6_addr.s6_addr[i]
					!= acl_sin6->sin6_addr.s6_addr[i]) {
					allow = 0;
					break;
				}
			}

			if ((sin6->sin6_addr.s6_addr[i] >>
			    (8 - (acl->mask % 8))) !=
					(acl_sin6->sin6_addr.s6_addr[i] >>
					 (8 - (acl->mask % 8)))) {
				allow = 0;
			}

			if (allow) {
				return ACL_ALLOW;
			}

			break;
		default:
			logging_log(LOG_WARNING, "Unknown address family.");
			break;
		}
	}

	return ACL_DENY;
}

/*
 * Logging
 */
/* Initialize logging. */
int
logging_init (thrulay_server_logtype_t lt)
{
	log_type = lt;

	/* Allocate memory for logging string. */
	logstr = malloc(LOGGING_MAXLEN);
	if (logstr == NULL) {
		error(ERR_WARNING, "Error: Unable to allocate memory for "
		      "logging string.");
		return -4;
	}

	switch (log_type) {
		case LOGTYPE_SYSLOG:
			openlog("thrulayd", LOG_NDELAY | LOG_CONS | LOG_PID,
					LOG_DAEMON);
			break;
		case LOGTYPE_STDERR:
			break;
	}

	return 0;
}

/* Deinitialize logging. */
void
logging_exit (void)
{
	switch (log_type) {
		case LOGTYPE_SYSLOG:
			closelog();
			break;
		case LOGTYPE_STDERR:
			break;
	}

	free(logstr);
}

/* Log message. Prepares input and hands it over to responsible log
 * facility. `priority' is the same priority as in syslog(3). */
void
logging_log (int priority, const char *fmt, ...)
{
	int n;
	va_list ap;

	memset(logstr, 0, LOGGING_MAXLEN);

	va_start(ap, fmt);
	n = vsnprintf(logstr, LOGGING_MAXLEN, fmt, ap);
	va_end(ap);

	/* If that worked, log the string. */
	if (n > -1 && n < LOGGING_MAXLEN)
		logging_log_string(priority, logstr);
}

void
logging_log_string (int priority, const char *s)
{
	switch (log_type) {
		case LOGTYPE_SYSLOG:
			syslog(priority, "%s", s);
			break;
		case LOGTYPE_STDERR:
			fprintf(stderr, "%s %s\n", logging_time(), s);
			fflush(stderr);
			break;
	}
}

/* Timestring will be stored in global variable `timestr'. Return pointer
 * to `timestr[0]'. */
char *
logging_time(void)
{
	time_t tp;
	struct tm *loc = NULL;

	/* Get current time. */
	tp = time(NULL);

	/* Convert it to local time representation. */
	loc = localtime(&tp);

	memset(&timestr, 0, sizeof(timestr));

	/* We use the format `month/day/year hrs:mins:secs'. Read strftime(3)
	 * if you want to change this. */
	strftime(&timestr[0], sizeof(timestr), "%m/%d/%Y %H:%M:%S", loc);

	return (&timestr[0]);
}

int
quantile_alg_error(int rc)
{
	switch (rc) {
	case -1:
		logging_log(LOG_ERR, "Error: quantiles not initialized.");
		break;
	case -2:
		logging_log(LOG_ERR, "Error: NEW needs an empty buffer.");
		break;
	case -3:
		logging_log(LOG_ERR, "Error: Bad input sequence length.");
		break;
	case -4:
		logging_log(LOG_ERR, "Error: Not enough buffers for COLLAPSE.");
		break;
	default:
		logging_log(LOG_ERR, "Error: Unknown quantile_algorithm error.");
	}
	return rc;
}

void
connection_end_log(char *test_type, struct timeval start, 
		   unsigned int packet_size, uint64_t packets_sent)
{
	struct timeval end;
	double bytes, seconds;
	struct rusage process_usage;
	int rc;

	if (start.tv_sec == 0 && start.tv_usec == 0) {
		logging_log(LOG_NOTICE, "nothing transfered");
		return;
	}
	tsc_gettimeofday(&end);
	seconds = time_diff(&start, &end);
	bytes = (double) packet_size * (double) packets_sent;
	rc = getrusage(RUSAGE_SELF,&process_usage);
	if (-1 == rc) {
		logging_log(LOG_NOTICE, "getrusage failed: "
			    "CPU usage report may be wrong");
	}
	logging_log(LOG_NOTICE, "%s test duration = %.3fs, "
		    "average throughput = %.3fMb/s, "
		    "CPU user/sys time = %.3f/%.3fs",
		    test_type,
		    seconds, bytes * 8.0 / seconds / 1000000.0,
		    process_usage.ru_utime.tv_sec + (double) 
		    process_usage.ru_utime.tv_usec/1000000.0,
		    process_usage.ru_stime.tv_sec + (double) 
		    process_usage.ru_stime.tv_usec/1000000.0);
}


void
sighandler(int sig)
{
	int status;

	switch (sig) {
#ifdef SIGCHLD
	case SIGCHLD:
		while (waitpid(-1, &status, WNOHANG) > 0)
			;
		break;
	case SIGHUP:
		logging_log(LOG_NOTICE, "got SIGHUP, don't know what do do");
		break;
	case SIGPIPE:
		break;
#endif
	default:
		logging_log(LOG_ALERT, "got signal %d, but don't remember "
				"intercepting it, aborting", sig);
		abort();
	}
}

char *
sock_ntop (const struct sockaddr *sa)
{
	static char str[128];
	struct sockaddr_in *sin = NULL;
	struct sockaddr_in6 *sin6 = NULL;

	switch (sa->sa_family) {
	case AF_INET:
		sin = (struct sockaddr_in *)sa;

		if (inet_ntop(AF_INET, &sin->sin_addr, str,
					sizeof(str)) == NULL)
			return NULL;

		return (str);
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;

		if (inet_ntop(AF_INET6, &sin6->sin6_addr, str,
					sizeof(str)) == NULL)
			return NULL;

		return (str);
	default:
		return NULL;
	}
}

void
log_client_address (const struct sockaddr *sa)
{
	logging_log(LOG_NOTICE, "connection from %s", sock_ntop(sa));
}

static int thrulay_server_listenfd;
static socklen_t thrulay_server_addrlen;
static char *thrulay_server_mcast_group = NULL;

int
thrulay_server_init(thrulay_server_logtype_t log_type, int reporting_verbosity)
{
	int rc;

	rc = logging_init(log_type);
	if (rc < 0)
		return rc;

	metrics_verbosity = reporting_verbosity;

	tsc_init();

#ifdef WIN32
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
			return -32;
		}
	}
#endif

	return 0;
}

int
thrulay_server_listen(int port, int window)
{
	int n;
	struct addrinfo hints, *res, *ressave;
	char service[7];
	int on = 1;

	/* Ignore SIGPIPE.  Always do in any socket code. */
#ifdef SIGPIPE
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		perror("ignoring SIGPIPE");
		return -26;
	}
#endif
#ifdef SIGCHLD
	{
		struct sigaction sa;

		sa.sa_handler = sighandler;
		sa.sa_flags = 0;
		sigemptyset (&sa.sa_mask);
		sigaction (SIGHUP, &sa, NULL);
		sigaction (SIGALRM, &sa, NULL);
		sigaction (SIGCHLD, &sa, NULL);
	}
#endif

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/* Convert integer port number to string for getaddrinfo(). */
	snprintf(service, sizeof(service), "%d", port);

	if ((n = getaddrinfo(NULL, service, &hints, &res)) != 0) {
		fprintf(stderr, "Error: getaddrinfo() failed: %s\n",
			gai_strerror(n));
		return -27;
	}
	ressave = res;

	do {
		thrulay_server_listenfd = socket(res->ai_family, 
						 res->ai_socktype, 
						 res->ai_protocol);
		if (thrulay_server_listenfd < 0)
			continue;

		if (setsockopt(thrulay_server_listenfd, SOL_SOCKET, 
			       SO_REUSEADDR, (char *)&on, sizeof(on)) == -1) {
			perror("setsockopt");
			error(ERR_WARNING, "setsockopt(SO_REUSEADDR): failed, "
					"continuing");
		}

		if (setsockopt(thrulay_server_listenfd, SOL_SOCKET, 
			       SO_KEEPALIVE, (char *)&on, sizeof(on)) == -1) {
			perror("setsockopt");
			error(ERR_WARNING, "setsockopt(SO_KEEPALIVE): failed, "
					"continuing");
		}

		window = set_window_size(thrulay_server_listenfd, window);

		if (bind(thrulay_server_listenfd, res->ai_addr, 
			 res->ai_addrlen) == 0)
			break;		/* success */

		/* bind error, close and try next one */
		close(thrulay_server_listenfd);	
	} while ((res = res->ai_next) != NULL);

	if (res == NULL) {
		error(ERR_WARNING, "unable to start server");
		return -28;
	}

	if (listen(thrulay_server_listenfd, LISTEN_BACKLOG) < 0) {
		perror("listen");
		return -29;
	}

	/* Save size of client address */
	thrulay_server_addrlen = res->ai_addrlen;

	freeaddrinfo(ressave);
	
#ifdef HAVE_SYSLOG_H
	/* Daemonize thrulayd */
	if (log_type == LOGTYPE_SYSLOG) {
		if (daemon(0, 0) == -1) {
			perror("daemon");	/* It could be going to
						   /dev/null... */
			return -30;
		}
	}
#endif

	logging_log(LOG_NOTICE, "thrulayd started, listening on port %d with "
			"window size %d", port, window);

	return 0;
}

/* For each client, a child process, a thread (Windows) or whatever
   runs this function. */
int
serve_client(int fd,  struct sockaddr *cliaddr)
{
	int rc;

	log_client_address(cliaddr);
	rc = thrulay_server_process_client(fd);
	logging_exit();
	return rc;
}

#ifdef WIN32
/*#include <windows.h> */

/* Parameters for client service threads. */
typedef struct client_thread_parameters_s {
	int fd;
	struct sockaddr *cliaddr;
} client_thread_parameters;

/* In POSIX systems, each incoming connection from a thrulay client is
   served by a child process. For Windows, we use native functions to
   spawn a thread for each client. This function defines the code that
   will run for each client (essentially the same as child processes
   run in POSIX systems). */
DWORD WINAPI
client_service_thread(LPVOID lpParam)
{
	int rc;
	client_thread_parameters *params = 
		(client_thread_parameters*)lpParam;

	close(thrulay_server_listenfd);
	rc = serve_client(params->fd, params->cliaddr);

	return rc;
}
#endif

#ifdef ENABLE_THRULAY_MULTICAST
/* Returns !=0 for IPv4 and IPv6 multicast addresses. */
int
is_multicast(struct sockaddr *address)
{
	switch (address->sa_family) {
		struct sockaddr_in *addr4;
		struct sockaddr_in6 *addr6;
	case AF_INET:
		addr4 = (struct sockaddr_in*)address;
		return IN_MULTICAST(ntohl(addr4->sin_addr.s_addr));
		break;
	case AF_INET6:
		addr6 = (struct sockaddr_in6*)address;
		return IN6_IS_ADDR_MULTICAST(&addr6->sin6_addr);
		break;
	default:
		return 0;
	}
}
#endif

int
thrulay_server_start(uint32_t max_clients, const char *const mcast_address)
{
	struct sockaddr *cliaddr = NULL;
	socklen_t len;
	uint32_t served_clients = 0;
	int rc = 0;

	/* Allocate memory for client address */
	cliaddr = malloc(thrulay_server_addrlen);
	if (cliaddr == NULL) {
		perror("malloc");
		return -4;
	}

#ifdef ENABLE_THRULAY_MULTICAST
	/* Check (and save) multicast address */
	if (NULL != mcast_address) {
		struct addrinfo hints, *mcast_res;

		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		rc = getaddrinfo(mcast_address, NULL, &hints, &mcast_res);

		if (0 != rc || !is_multicast(mcast_res->ai_addr)){
			logging_log(LOG_WARNING, "Wrong multicast address "
				    "given: %s", mcast_address);
			logging_log(LOG_WARNING, "getaddrinfo(): %s\n", 
				    gai_strerror(rc));
		} else {
			/* Save multicast group to join to during UDP tests. */
			thrulay_server_mcast_group = strdup(mcast_address);
			if (NULL == thrulay_server_mcast_group) {
				perror("strdup");
				return -2;
			}
		}
	}
#endif /* #ifdef ENABLE_THRULAY_MULTICAST */

	while (!max_clients || served_clients < max_clients) {
		int fd, pid;

		len = thrulay_server_addrlen;

		fd = accept(thrulay_server_listenfd, cliaddr, &len);
		if (fd == -1) {
			if (errno != EINTR) {
				logging_log(LOG_WARNING, "accept(): failed, "
						"continuing");
			}
			continue;
		}

		if (acl_check(cliaddr) == ACL_DENY) {
			logging_log(LOG_WARNING, "Access denied for host %s",
					sock_ntop(cliaddr));
			close(fd);
			continue;
		}

#ifndef WIN32
		pid = fork();
		switch (pid) {
		case 0:
			close(thrulay_server_listenfd);
			rc = serve_client(fd, cliaddr);
			_exit(rc);
			/* NOTREACHED */
		case -1:
			logging_log(LOG_ERR, "fork(): failed, closing "
					"connection");
			close(fd);
			break;
		default:
			close(fd);
			break;
		}
#else
		{
			HANDLE thread;
			DWORD thread_id;
			client_thread_parameters thread_param;
			thread_param.param = 44;
			thread_param.fd = fd;
			memcpy(thread_param.cliaddr, cliaddr, 
			       thrulay_server_addrlen);

			thread = CreateThread(NULL, 0, client_service_thread, 
					      &thread_param, 0, &thread_id);
			
			if (NULL == thread) {
				logging_log(LOG_ERR, "CreateThread(): failed, "
					    "closing connection");
				close(fd);
			} else {
				/* So that the thread exits when it ends. */
				CloseHandle(thread);
			}
		}
#endif
		served_clients++;
	}
	free(cliaddr);

#ifdef WIN32
	WSACleanup();
#endif

	return 0;
}

int
udp_test(int fd, char* proposal)
{
	int final_rc = 0, rc, rc2, rc3, rc4, rc5;
	int opt;
	unsigned int client_port;
	unsigned int packet_size;
	int udp_buffer_size;
	uint64_t rate;			/* In packets per 1000 seconds. */
	uint64_t npackets;		/* Proposed number of packets. */
	uint64_t packets_received = 0;	/* Unique packets received. */
	uint64_t packets_sent = 0;	/* What the client said at the end. */
	uint64_t packets_total = 0;	/* Total packets received, including
					   duplicates. */
	char nonce[8];
	char buffer[65536];
	int response_size;
	int sock;
	struct timeval start = {0, 0};
	/* Decrement will be canceled before first use. */
	int server_port = UDP_PORT - 1;
	struct sockaddr *cliaddr = NULL;
	socklen_t addrlen, len;
	struct addrinfo hints, *res, *ressave;
	char service[7];
	int tries;
	int random_fd;
	int nfds;
	fd_set readfds;
	int test_done = 0;
	double delay;
	struct timeval tv, tvc;
	long long unsigned rate_llu, npackets_llu, packets_sent_llu;
	uint64_t packet_sqn;		/* Packet sequence number */
	uint32_t msb, lsb;		/* 32bit parts of sequence number */
	/* reordering */
	const uint64_t reordering_max = 100;
	char buffer_reord[reordering_max * 80];
	size_t r = 0;
	uint64_t j = 0;
	/* duplication */
	uint64_t packets_dup = 0;	/* Duplicated packets */
	/* quantiles */
	double quantile_00, quantile_25, quantile_50, quantile_75, quantile_95;
	double packet_loss, jitter;

	rc = sscanf(proposal, "%u:%u:%llu:%llu+", &client_port, &packet_size,
		    &rate_llu, &npackets_llu);
	if (rc != 4) {
		logging_log(LOG_WARNING, "malformed parameters in UDP "
			    "proposal");
		final_rc = -17;
		goto log;
	}
	rate = (uint64_t)rate_llu;
	npackets = (uint64_t)npackets_llu;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;

	/* Get a port number for the server. */
	tries = 0;
	do {
		server_port++;
		tries++;

		/* Convert integer port number to string for getaddrinfo(). */
		snprintf(service, sizeof(service), "%d", server_port);

		if ((rc = getaddrinfo(NULL, service, &hints, &res)) != 0) {
			logging_log(LOG_ERR, "getaddrinfo(): failed, %s",
					gai_strerror(rc));
			final_rc = -3;
			goto log;
		}
		ressave = res;

		do {
			sock = socket(res->ai_family, res->ai_socktype,
					res->ai_protocol);
			if (sock < 0)
				continue;

			if ((rc = bind(sock, res->ai_addr,
							res->ai_addrlen)) == 0)
				break;		/* success */

			close(sock);
		} while ((res = res->ai_next) != NULL);
	} while ((rc < 0) && (tries < TRY_UDP_PORTS));
	if (rc < 0) {
		logging_log(LOG_WARNING, "could not bind a UDP socket");
		final_rc = -18;
		goto log;
	}

	addrlen = res->ai_addrlen;

	freeaddrinfo(ressave);

	cliaddr = malloc(addrlen);
	if (cliaddr == NULL) {
		logging_log(LOG_ERR, "malloc(): failed");
		final_rc = -4;
		goto log;
	}

	/* Obtain random data for the nonce. */
	random_fd = open(URANDOM_PATH, O_RDONLY);
	if (random_fd < 0) {
		logging_log(LOG_WARNING, "could not open random device");
		final_rc = -19;
		goto log;
	}
	rc = read(random_fd, nonce, sizeof(nonce));
	if (rc != sizeof(nonce)) {
		logging_log(LOG_WARNING, "could not read urandom for UDP "
				"nonce");
		final_rc = -20;
		goto log;
	}
	close(random_fd);
	/* 
	 * Build a UDP session response.  Note that since nonce may
	 * contain NUL characters, it is not a C string.  The length
	 * is kept in response_size.
	 */
	sprintf(buffer, "%u:%u:%llu:%llu:", server_port, packet_size,
		(long long unsigned)rate, (long long unsigned)npackets);
	response_size = strlen(buffer);
	memcpy(buffer + response_size, nonce, sizeof(nonce));
	response_size += sizeof(nonce);
	buffer[response_size] = '+';
	response_size++;

	/* Disable keep-alives on the control TCP connection. */
	opt = 0;
	rc = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&opt,
			sizeof(opt));
	if (rc == -1) {
		logging_log(LOG_WARNING, "setsockopt(SO_KEEPALIVE): failed, "
				"continuing");
	}

	/* Increase the buffer space of the receiving UDP socket. */
	udp_buffer_size = set_window_size_directed(sock, UDP_BUFFER_SIZE,
						   SO_RCVBUF);
	if (udp_buffer_size < 0) {
		logging_log(LOG_WARNING, "failed to set window size to %d", 
			    UDP_BUFFER_SIZE);
	}

#ifdef ENABLE_THRULAY_MULTICAST
	/* Join to multicast group if requested */
	if (NULL != thrulay_server_mcast_group) {
		int rc;
		struct addrinfo *mcast_res;
		snprintf(service, sizeof(service), "%d", server_port);
		rc = getaddrinfo(thrulay_server_mcast_group, service, &hints, 
				 &mcast_res);
		if (0 != rc) {
			logging_log(LOG_WARNING, "Wrong multicast group "
				    "given: %s", thrulay_server_mcast_group);
			logging_log(LOG_WARNING, "getaddrinfo(): %s\n", 
				    gai_strerror(rc));
		}

#if defined(HAVE_STRUCT_IP_MREQN) || defined(HAVE_STRUCT_IP_MREQ)
		if (NULL != mcast_res && is_multicast(mcast_res->ai_addr) ) {

			switch (((struct sockaddr *)
				 mcast_res->ai_addr)->sa_family) {
				int rc, on;
				struct ip_mreq mreq;
				struct ipv6_mreq mreq_v6;
			case AF_INET:
				mreq.imr_multiaddr.s_addr =
					((struct sockaddr_in*)
					 mcast_res->ai_addr)->sin_addr.s_addr;
				mreq.imr_interface.s_addr = htonl(INADDR_ANY);
				rc = setsockopt(sock, SOL_SOCKET, 
						SO_REUSEADDR, &on, sizeof(on));
				if (rc < 0) {
					logging_log(LOG_WARNING, 
					    "setsockopt(SO_REUSEADDR failed, "
					    "continuing.");
				}
				setsockopt(sock, IPPROTO_IP, 
					   IP_ADD_MEMBERSHIP, &mreq, 
					   sizeof(mreq));
				if (rc < 0) {
					logging_log(LOG_WARNING, 
					    "setsockopt(IP_ADD_MEMBERSHIP "
					    "failed, continuing.");
				}
				break;
			case AF_INET6:
				memcpy(&mreq_v6.ipv6mr_multiaddr, 
				       &(((struct sockaddr_in6*)
					  mcast_res->ai_addr)->sin6_addr),
				       sizeof(struct in6_addr));
				mreq_v6.ipv6mr_interface = 0;
				/* mreq_v6.ipv6mr_ifindex = ; */
				rc = setsockopt(sock, IPPROTO_IPV6, 
						IPV6_ADD_MEMBERSHIP, &mreq_v6, 
						sizeof(mreq_v6)); 
				if (rc < 0) {
					logging_log(LOG_WARNING, 
					    "setsockopt(IPV6_ADD_MEMBERSHIP "
					    "failed, continuing.");
				}
				break;
			default:
				logging_log(LOG_WARNING, "socket address "
					    "family not supported for "
					    "multicast.");
			}
		}
#else
		logging_log(LOG_WARNING, "Joining to a multicast group is not "
			    "supported, continuing.");
#endif
	}
#endif /* ENABLE_THRULAY_MULTICAST */

	rc = send_exactly(fd, buffer, response_size);
	if (rc < 0 || rc > response_size) {
		final_rc = -21;
		goto log;
	}

	/* Start of test processing. */
	if (tsc_gettimeofday(&start) == -1) {
		logging_log(LOG_ERR, "gettimeofday(): failed");
		final_rc = -1;
		goto log;
	}

	/* Set sockets to nonblocking mode: will use select(). */
#ifndef WIN32
	opt = fcntl(fd, F_GETFL);
	if (opt == -1) {
		logging_log(LOG_WARNING, "fcntl(F_GETFL): failed, continuing");
	} else {
		/* Set TCP socket to nonblocking mode. */
		rc = fcntl(fd, F_SETFL, opt | O_NONBLOCK);
		if (rc == -1) {
			logging_log(LOG_WARNING, "fcntl(F_SETFL): failed, "
					"continuing");
		}
	}
#else
	opt = ioctlsocket(fd, FIONBIO, (unsigned long*)1);
	if (-1 == opt) {
		logging_log(LOG_WARNING, "ioctlsocket(FIONBIO): failed");
	}
#endif

#ifndef WIN32
	opt = fcntl(sock, F_GETFL);
	if (opt == -1) {
		logging_log(LOG_WARNING, "fcntl(F_GETFL): failed, continuing");
	} else {
		/* Set UDP socket to nonblocking mode. */
		rc = fcntl(sock, F_SETFL, opt | O_NONBLOCK);
		if (rc == -1) {
			logging_log(LOG_WARNING, "fcntl(F_SETFL): failed, "
					"continuing");
		}
	}
#else
	opt = ioctlsocket(sock, FIONBIO, (unsigned long*)1);
	if (-1 == opt) {
		logging_log(LOG_WARNING, "ioctlsocket(FIONBIO): failed");
	}
#endif

	/* Initialize reordering */
	rc = reordering_init(reordering_max);
	if (-1 == rc) {
		logging_log(LOG_ERR, "calloc(): failed");
		final_rc = -4;
		goto log;
	}

	/* Initialize quantiles */
	rc = quantile_init(1, QUANTILE_EPS, npackets);
	if (-1 == rc) {
		logging_log(LOG_ERR, "malloc(): failed");
		final_rc = -4;
		goto log;
	}

	/* Inititalize duplication */
	rc = duplication_init(npackets);
	if (-1 == rc) {
		logging_log(LOG_ERR, "calloc(): failed");
		final_rc = -4;
		goto log;
	}

	/*
	 * Could have set nfds to FD_SETSIZE, but this is marginally
	 * more efficient.
	 */
	nfds = (fd > sock? fd: sock) + 1;
	while (! test_done) {
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		FD_SET(sock, &readfds);
		rc = select(nfds, &readfds, 0, 0, 0);
		if (FD_ISSET(sock, &readfds)) {
			/* A new UDP packet arrived. */
			/* XXX: Need to loop in case of coalescence. */
			len = addrlen;
			rc = recvfrom(sock, buffer, sizeof(buffer), 0,
					cliaddr, &len);
			if (rc < 0) {
				if (errno != EAGAIN) {
					logging_log(LOG_WARNING, "recvfrom(): "
							"failed");
				}
			}
			if (memcmp(buffer, nonce, sizeof(nonce)) == 0) {
				/* Legitimate packet -- nonce matches.
				 * This includes duplicated packets. */
				packets_total++;

				/*
				 * Calculate packet sequence number
				 */
				memcpy(&msb, buffer+8, 4);
				memcpy(&lsb, buffer+12, 4);

				msb = ntohl(msb);
				lsb = ntohl(lsb);

				packet_sqn = (uint64_t)msb << 32;
				packet_sqn |= lsb;

				/* Sanity check for packet sequence number */
				if (packet_sqn > (npackets - 1)) {
					error(ERR_WARNING, "Packet has bad "
							"sequence number, "
							"skipping.");
					continue;
				}

				/*
				 * Duplication
				 */
				if (duplication_check(packet_sqn)) {
					/* Duplicated packet */
					packets_dup++;
					continue;
				} else {
					/* Unique packet */
					packets_received++;
				}

				/*
				 * Calculate delay for this packet
				 */
				/* Get current time for packet delay */
				if (tsc_gettimeofday(&tv) == -1) {
					logging_log(LOG_ERR, "gettimeofday(): "
						    "failed");
					final_rc = -1;
					goto log;
				}
				normalize_tv(&tv);

				/* Get client gettimeofday() for packet */
				ntp2tv(&tvc, buffer+16);

				delay = time_diff(&tvc, &tv);

				/*
				 * Delay quantiles.
				 */
				rc = quantile_value_checkin(0, delay);
				if (rc < 0) {
					quantile_alg_error(rc);
					final_rc = -22;
					goto log;
				}

				/*
				 * Reordering
				 */
				reordering_checkin(packet_sqn);
			} else {
				logging_log(LOG_WARNING, "nonce does not "
						"match");
			}
		}
		if (FD_ISSET(fd, &readfds)) {
			/* Data on the TCP connection -- test done. */
			test_done = 1;
			/* XXX: Assume it comes through in one piece. */
			rc = recv(fd, buffer, sizeof(buffer), 0);
			if (rc < 0) {
				if (errno != EAGAIN) {
					logging_log(LOG_WARNING, "could not "
							"recv TCP connection "
							"at the end of a UDP "
							"test");
					final_rc = -23;
					goto log;
				}
			}
			if (buffer[0] == '+') {
				rc = sscanf(buffer+1, "%llu:",
					    &packets_sent_llu);
				packets_sent = (uint64_t)packets_sent_llu;
				if (rc != 1) {
					logging_log(LOG_WARNING, "malformed "
							"end of test");
					final_rc = -24;
					goto log;
				}
			}
		}
	}
	/* 
	 * Done with the UDP test.  Set the TCP socket back to
	 * blocking mode.
	 */
#ifndef WIN32
	opt = fcntl(fd, F_GETFL);
	if (opt == -1) {
		logging_log(LOG_WARNING, "fcntl(F_GETFL): failed, continuing");
	} else {
		/* Set TCP socket back to blocking mode */
		rc = fcntl(fd, F_SETFL, opt & ~O_NONBLOCK);
		if (rc == -1) {
			logging_log(LOG_WARNING, "fcntl(F_SETFL): failed, "
					"continuing");
		}
	}
#else
	opt = ioctlsocket(fd, FIONBIO, (unsigned long*)0);
	if (-1 == opt) {
		logging_log(LOG_WARNING, "ioctlsocket(FIONBIO): failed");
	}
#endif

	/*
	 * Quantiles
	 *
	 * Perform last algorithm operation with a possibly not full
	 * input buffer.
	 */
	rc = quantile_finish(0);
	if (packets_total > 0 && rc < 0) {
		quantile_alg_error(rc);
		final_rc = -22;
		goto log;
	}

	rc = quantile_output(0, packets_received, 0.00, &quantile_00);
	rc2 = quantile_output(0, packets_received, 0.25, &quantile_25);
	rc3 = quantile_output(0, packets_received, 0.50, &quantile_50);
	rc4 = quantile_output(0, packets_received, 0.75, &quantile_75);
	rc5 = quantile_output(0, packets_received, 0.95, &quantile_95);
	if (packets_total > 0 && (-1 == rc || -1 == rc2 || -1 == rc3 || 
	    -1 == rc4 || -1 == rc5)) {
		logging_log(LOG_ERR, "Error: Bad number of full "
			    "buffers in OUTPUT operation.");
		final_rc = -22;
		goto log;
	}

	/*
	 * Write reordering statistics to buffer if verbose output was
	 * requested
	 */
	memset(&buffer_reord, 0, sizeof(buffer_reord));
	if (metrics_verbosity >= 1) {
		r = 0;
		for (j = 0; j < reordering_max; j++) {
			double jreord = reordering_output(j);
			if (0.0 != jreord) {
				r += snprintf(buffer_reord + r, 
					      sizeof(buffer_reord) - r,
					      "\t%"PRIu64
					      "-reordering = %3.3f%%\n",
					      j + 1, 100.0 * jreord);
			} else {
				break;
			}
		}

		if (j == 0) {
			snprintf(buffer_reord + r, sizeof(buffer_reord) - r,
				 "\tno reordering\n");
		} else if (j < reordering_max) {
			snprintf(buffer_reord + r, sizeof(buffer_reord) - r,
				 "\tno %"PRIu64"-reordering\n", j + 1);
		} else {
			snprintf(buffer_reord + r, sizeof(buffer_reord) - r,
				 "\tonly up to %"PRIu64"-reordering is "
				 "handled\n", reordering_max);
		}
	}

	packet_loss = packets_sent > packets_received?
		(100.0*(packets_sent - packets_received))/packets_sent: 0;
	delay = (packet_loss > 50.0)? INFINITY : quantile_50;
	if (packet_loss < 25.0 ) {
		jitter = quantile_75 - quantile_25;
	} else if (packet_loss > 75.0) { 
		jitter = NAN;
	} else {
		jitter = INFINITY;
	}

	if (metrics_verbosity >= 1) {
		rc = snprintf(buffer, sizeof(buffer),
			      "Server used UDP buffer size of %d bytes\n"
			      "Client proposed sending %llu packets\n"
			      "Client said it sent %llu packets\n"
			      "Server received %llu total packets\n"
			      "Server received %llu unique packets\n",
			      udp_buffer_size,
			      (long long unsigned)npackets,
			      (long long unsigned)packets_sent,
			      (long long unsigned)packets_total,
			      (long long unsigned)packets_received);
	}

	rc += snprintf(buffer + rc, sizeof(buffer) - rc,
		      "Delay:\t\t %3.3fms\n",
		      1000.0 * delay);
	if (metrics_verbosity >= 1) {
		rc += snprintf(buffer + rc, sizeof(buffer) - rc,
			       "\tDelay quantiles (ignoring clock offset):\n"
			       "\t  0th: %8.3fms\n"
			       "\t 25th: %8.3fms\n"
			       "\t 50th: %8.3fms\n"
			       "\t 75th: %8.3fms\n"
			       "\t 95th: %8.3fms\n",
			       1000.0 * quantile_00,
			       1000.0 * quantile_25,
			       1000.0 * quantile_50,
			       1000.0 * quantile_75,
			       1000.0 * quantile_95);
	}

	rc += snprintf(buffer + rc, sizeof(buffer) -rc,
		       "Loss:\t\t %3.3f%%\n"
		       "Jitter:\t\t %3.3fms\n"
		       "Duplication:\t %3.3f%%\n"
		       "Reordering:\t %3.3f%%\n",
		       packet_loss,
		       1000.0 * jitter,
		       100 * (double)packets_dup/packets_total,
		       100.0 * reordering_output(0));

	if (metrics_verbosity >= 1) {
		rc += snprintf(buffer + rc, sizeof(buffer) - rc, "%s", 
			       buffer_reord);
	}

	rc = send_exactly(fd, buffer, strlen(buffer));
	if (rc < 0 || rc > (int) strlen(buffer)) {
		logging_log(LOG_WARNING, "could not send final report to "
			    "client");
		final_rc = -25;
		goto log;
	}
	
 log:
	reordering_exit();
	quantile_exit();
	duplication_exit();
	free(cliaddr);

	connection_end_log("UDP", start, packet_size, packets_sent);
	
	return final_rc;
}

int
tcp_test(int fd, char* proposal)
{
	int rc, final_rc = 0, opt;
	char buffer[1024];
	char *block = NULL;
	int blocks = 0;
	int w = -1, b = -1;
	int to_write;
	struct timeval start = {0, 0};
	int stop_test = 0;
	fd_set rfds_orig;
	int maxfd = 0;
	struct timeval select_tv_orig = {0, 1000};
	struct timeval select_tv = {0, 1000};
	size_t rcount = 0, wcount = 0;

	/* XXX: Very long numbers will not, of course, be processed
           correctly by sscanf() below.  We could, if the number
           exceeds a certain value (e.g., has more than a certain
           number of characters, use some ``large'' -- still supported
           -- value for window or block size.  It's not worth the
           trouble. */
	rc = sscanf(proposal, "%d:%d+", &w, &b);
	if (rc != 2) {
		logging_log(LOG_WARNING, "malformed session proposal from "
				"client");
		final_rc = -12;
		goto log;
	}

	if ( w < 0 ) {
		logging_log(LOG_WARNING, "window size in client proposal is "
			    "negative");
		final_rc = -13;
		goto log;
	}
	if ( b < 0 ) {
		logging_log(LOG_WARNING, "block size in client proposal is "
			    "negative");
		final_rc = -14;
		goto log;
	}
	if (b < MIN_BLOCK)
		b = MIN_BLOCK;
	if (b > MAX_BLOCK)
		b = MAX_BLOCK;
	if (w < 1500)
		w = 1500;
	block = malloc((size_t) b);
	if (! block) {
		logging_log(LOG_ALERT, "could not allocate memory");
		final_rc = -4;
		goto log;
	}
	rc = set_window_size(fd, w);
	if (rc < 0) {
		logging_log(LOG_WARNING, "failed to set window size to %d", w);
	}
	to_write = snprintf(buffer, sizeof(buffer), "%u:%u+", rc, b);
	if (to_write < 0 || to_write > (int) sizeof(buffer)) {
		logging_log(LOG_ALERT, "snprintf(): failed ");
		final_rc = -15;
		goto log;
	}
	rc = send_exactly(fd, buffer, (size_t) to_write);
	if (rc < 0 || rc > to_write) {
		logging_log(LOG_WARNING, "could not send session response to "
			    "client");
		final_rc = -16;
		goto log;
	}
	if (tsc_gettimeofday(&start) == -1) {
		logging_log(LOG_ALERT, "gettimeofday(): failed");
		final_rc = -1;
		goto log;
	}

	/* Set socket to nonblocking mode: will use select(). */
#ifndef WIN32
	opt = fcntl(fd, F_GETFL);
	if (opt == -1) {
		logging_log(LOG_WARNING, "fcntl(F_GETFL): failed, continuing");
	} else {
		rc = fcntl(fd, F_SETFL, opt | O_NONBLOCK);
		if (rc == -1) {
			logging_log(LOG_WARNING, "fcntl(F_SETFL): failed, "
					"continuing");
		}
	}
#else
	opt = ioctlsocket(fd, FIONBIO, (unsigned long*)0);
	if (-1 == opt) {
		logging_log(LOG_WARNING, "ioctlsocket(FIONBIO): failed");
	}
#endif

	/* initialize FD sets for select() */
	FD_ZERO(&rfds_orig);
	FD_SET(fd,&rfds_orig);
	maxfd = (fd > maxfd)? fd : maxfd;

	/* The idea of this select() loop is that recv/send can
	   advance independently. However, both must finish with the
	   current block/header before any of them steps to the next
	   one.

	   This could be improved a bit: for example, the first bytes
	   of the next incoming block could be recv if say half the
	   header has been sent. This would of course require changes
	   in both the client and server. As it would be a very
	   minimal improvement, I would say it is not worth the pain
	   and the code obfuscation. I guess better improvements can
	   be achieved easier by increasing block sizes. */
	while ( !stop_test ) {
		size_t wready = 0;
		fd_set rfds = rfds_orig;
		select_tv = select_tv_orig;

		rc = select(maxfd + 1 , &rfds, NULL, NULL, &select_tv);

		if (0 == rc)  
			continue;

		if (rc < 0) {
			logging_log(LOG_NOTICE, "select(): failed, "
				    "continuing");
			continue;
		}

		/* Recv blocks. */
		/* Why don't do recv while there is data available
		   instead or calling recv just once? The read block
		   code seems far less time consuming (30% sys and 20%
		   user time) if a while is not used. Failing calls to
		   recv seem more costly than extra loops. Note: these
		   values were obtained with a lot of fprintf
		   around. Without them, the difference is lower but
		   it nevertheless seems that user time in particular
		   is higher if we do a while() { read }. */
		if (rcount < (size_t)b && FD_ISSET(fd,&rfds)) {
			rc = recv(fd, block + rcount, (size_t)b - rcount, 0);
			if (rc > 0) {
				rcount += rc;
				if (rcount == (size_t)b) {
					blocks++;
					if (BLOCK_HEADER == wcount) {
						rcount = wcount = 0;
					}
				}
			} else if (0 == rc) {
				stop_test = 1;
			} else {
				if (errno == ECONNRESET || errno == EPIPE) {
					stop_test = 1;
				} else if (EAGAIN != errno) {
					logging_log(LOG_NOTICE, 
						    "while testing: "
						    "recv(): failed");
				}
			}
		}

		/* Bytes ready for sending back to client? */
		wready = (rcount < BLOCK_HEADER - wcount)? rcount :
			BLOCK_HEADER - wcount;
		/* Send header. We don't care about FD_ISSET(fd,&wfds).
		   If it was false, can be outdated. Also, having 
		   FS_SET(fd,&wfds_orig) generally leads to unnecessary calls 
		   to and returns from select and higher CPU utilization. */
		if (wready > 0) {
			rc = send(fd, block + wcount, wready, 0);
			if (-1 != rc) {
				wcount += rc;
				if (BLOCK_HEADER == wcount && 
				    rcount == (size_t)b) {
					wcount = rcount = 0;
				}
			} else {
				if (errno == ECONNRESET || errno == EPIPE) {
					/* It's possible that this is the last
					 * packet the client sent in test.
					 * So the client has maybe closed the
					 * TCP socket and we are unable to send
					 * back this header and get an error
					 * message here.
					 * If this is the case, don't print an
					 * error message. */
					stop_test = 1;
				} else if (EAGAIN != errno) {
					logging_log(LOG_NOTICE, 
						    "send(block_header): "
						    "failed");
				}
			}
		}
		/* After recv and send, we could do a FD_CLR or a
		   FD_SET of wfds_orit depending on avaiability of
		   data to write (wready). But it increases CPU usage
		   and provides no advantage. */
	}
 log:
	connection_end_log("TCP", start, b, blocks);
	free(block);

	if (close(fd) == -1)
		logging_log(LOG_WARNING, "close(): failed");

	return final_rc;
}

int
thrulay_server_process_client(int fd)
{
	int rc, final_rc = 0;
	struct timeval start = {0, 0};
	char buffer[1024];
	char *buf;

	buf = buffer;
	rc = send_exactly(fd, THRULAY_GREET, sizeof THRULAY_GREET - 1);
	if (rc < (int) sizeof THRULAY_GREET - 1) {
		if (rc == -1) {
			logging_log(LOG_WARNING, "send(greeting): failed");
		}
		logging_log(LOG_WARNING, "could not send greeting, exiting");
		return -5;
	}
	/* XXX: Assume that few-byte session proposal will come in one
           TCP packet and recv in one block. */
	rc = recv(fd, buf, sizeof(buffer) - 1, 0);
	if (rc < 0 || rc > ((int)sizeof(buffer) -1)) {
		if (rc == -1) {
			logging_log(LOG_WARNING, "recv(proposal): failed");
		}
		logging_log(LOG_WARNING, "could not receive session proposal");
		return -6;
	}

	buf[rc] = '\0';
	rc = memcmp(buf, INDICATOR, sizeof(INDICATOR) - 1);
	if (rc != 0) {
		logging_log(LOG_WARNING, "malformed protocol indicator");
		return -7;
	}
	buf += sizeof(INDICATOR) - 1;	/* Skip the indicator. */
	if (*buf != '/') {
		logging_log(LOG_WARNING, "protocol indicator not followed by "
				"'/'");
		return -8;
	}
	buf++;				/* Skip the slash. */
	rc = memcmp(buf, THRULAY_VERSION_NUMBER, 
		    sizeof(THRULAY_VERSION_NUMBER) - 1);
	if (rc != 0) {
		logging_log(LOG_WARNING, "malformed protocol version");
		return -9;
	}
	buf += sizeof(THRULAY_VERSION_NUMBER) - 1;	/* Skip the version. */
	if (*buf != ':') {
		logging_log(LOG_WARNING, "protocol version not followed by "
				"':'");
		return -10;
	}
	buf++;				/* Skip the colon. */
	if ((buf[0] == 'u') && (buf[1] == ':')) {
		buf += 2;		/* Skip the test type. */
		final_rc = udp_test(fd, buf);
		return final_rc;
	} else if ((buf[0] == 't') && (buf[1] == ':')) {
		buf += 2; 			/* Skip "t:". */
		final_rc = tcp_test(fd, buf);
	} else if ((buf[0] != 't') || (buf[1] != ':')) {
		logging_log(LOG_WARNING, "unknown test proposal type");
		connection_end_log("unknown", start, 0, 0);
		if (close(fd) == -1)
			logging_log(LOG_WARNING, "close(): failed");
		final_rc = -11;
	} 

	return final_rc;
}
