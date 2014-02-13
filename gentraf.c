/*
 * Copyright (c) 2014, Tsuyoshi Tanai <skmt.japan@gmail.com>,
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE. 
*/

/*
 * traffic generator
 *
 * author: skmt <mailto:skmt.japan@gmail.com>
 * create: Jan 21st, 2013
 *
 * description;
 *  This program can generate upd datagram and put load on a network interface.
 *  The usage is the following;
 *  bash$ gentraf [option(s)] hostname
 *      options;
 *      [-h] print help
 *      [-v] print progress
 *      [-b bandwidth] specify bandwidth
 *      [-p port_number] destination port number (default: 16666)
 *      [-s] size of transferred packet(s)
 *      [-t] using tcp socket instead of udp
 *
*/


/********************************************
 * include file
 ********************************************
*/
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <ctype.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/tcp.h>

#include <signal.h>
#include <sys/stat.h>
#include <arpa/inet.h>


/********************************************
 * macro
 ********************************************
*/
#define LOCALHOST	"localhost"
#define CHUNKSIZE	1024	/* max size of udp datagram */
#define DEFAULT_PORT	"16666"	/* default port number */


/********************************************
 * type definition
 ********************************************
*/

/*
 * for short-cut
*/
typedef struct sockaddr SA;
typedef struct sockaddr_in SA_IN;
typedef struct in_addr ADDR;

/*
 * for option
 * 
*/
typedef struct _opt {
	char *host;
	char *port;
	unsigned long int size;
	unsigned long int bandwidth;
	int socktype;	/* tcp or udp (SOCK_STREAM/SOCK_DGRAM) */
	int protocol;	/* IPPROTO_TCP or IPPROTO_UDP */
	struct addrinfo *dest;
} OPT;


/********************************************
 * global variable
 ********************************************
*/
static OPT *opt;


/********************************************
 * proto type
 ********************************************
*/
static void sys_err(char *, char *, long int);
static void sys_warn(char *, char *, long int);

static void * xmalloc(size_t);
/* static void * xrealloc(void *, size_t); */
static void xfree(void *);
static char * xstrdup(char *);

static void print_usage(void);
static void option(int, char **);

static unsigned long int translate_size(char *);
static struct addrinfo * translate_addr(char *, char *);
static int open_socket(void);

static int ub_set_progress_bar(unsigned long int);
static void ub_progress_countup(off_t);
static void ub_sigsend(pid_t);
static size_t calc_size_packet(size_t);

/*============================================================================
 * program section
 *============================================================================
*/

/********************************************
 * log funcion
 ********************************************
*/
void
sys_err(char *msg, char *source, long int line) {
	fprintf(stderr, "%s[%ld]: %s\n", source, line, msg);

	exit(1);
}

void
sys_warn(char *msg, char *source, long int line) {
	fprintf(stderr, "%s[%ld]: %s\n", source, line, msg);

	return;
}

/********************************************
 * memory funcion
 ********************************************
*/
void *
xmalloc(size_t size) {
	void *tmp;

	if (!size) {
		goto error_xmalloc;
	}

	if ((tmp = malloc(size)) == NULL) {
		goto error_xmalloc;
	}
	else {
		memset(tmp, 0, size);	/* clear */
	}

	return tmp;

error_xmalloc:
	exit(1);
}

/* 
 *comment out due to this function isn't needed so far.
 *
*/
/*
void *
xrealloc(void *src, size_t size) {
	void *dst;

	if (!size) {
		goto error_xrealloc;
	}

	if ((dst = realloc(src, size)) == NULL) {
		goto error_xrealloc;
	}

	return dst;

error_xrealloc:
	exit(1);
}
*/

void
xfree(void *ptr) {
	if (!ptr) {
		return;
	}

	free(ptr);

	return;
}

char *
xstrdup(char *ptr) {
	char *tmp;

	if (!ptr) {
		goto error_xstrdup;
	}

	if ((tmp = strdup(ptr)) == NULL) {
		goto error_xstrdup;
	}

	return tmp;

error_xstrdup:
	exit(1);
}

/*-------------------------------------------------------------------------
 * print usage and die
 *-------------------------------------------------------------------------
*/
void
print_usage(void) {
	fprintf(stderr, "usage: gentraf [option(s)] hostname \n"
		        "     options; \n"
			"        [-h] print help \n"
			"        [-v] print progress \n"
			"        [-b bandwidth] specify bandwidth \n"
			"        [-p port_number] destination port number (default: 16666) \n"
			"        [-s] size of transferred packet(s) \n"
			"        [-t] using tcp socket instead of udp \n");
	exit(1);
}

unsigned long int
translate_size(char *size) {
	long int cc;
	size_t len;
	char *str;
	char lastchar;

	len = strlen(size);
	str = xmalloc(len+1);
	strncpy(str, size, len);
	lastchar = size[len-1];

	if (isdigit(lastchar)) {
		cc = atol(str);
		free(str);
		return (cc);
	}

	str[len] = '\0';
	switch (lastchar) {
		case 'k':
			cc = atol(str) * 1000;
			break;
		case 'm':
			cc = atol(str) * 1000000;
			break;
		case 'g':
			cc = atol(str) * 1000000000;
			break;
		case 't':
			cc = atol(str) * 1000000000000;
			break;
		default:
			cc = 0;
			break;
	}

	xfree(str);
	return (cc);
}

void
option(int argc, char **argv) {
	FILE *null;
	int verb;
	int ch;

	opt = xmalloc(sizeof(OPT));

	verb = 0;
	opt->host = LOCALHOST;
	opt->port = DEFAULT_PORT;

	/* use udp as default */
	opt->socktype = SOCK_DGRAM;
	opt->protocol = IPPROTO_UDP;

	/* unlimited mode as default */
	opt->bandwidth = 0;

	while ((ch = getopt(argc, argv, "b:hp:s:tv")) != -1) {
		switch(ch) {
		case 'b':
			opt->bandwidth = translate_size(optarg);
			break;
		case 'h':
			print_usage();
			/* NOT REACH */
			break;
		case 'p':
			opt->port = xstrdup(optarg);
			break;
		case 'v':
			verb = 1;
			break;
		case 's':
			opt->size = translate_size(optarg);
			break;
		case 't':
			opt->socktype = SOCK_STREAM;
			opt->protocol = IPPROTO_TCP;
			break;
		default:
			print_usage();
			break;
		}
	}

	/*
	 * not verbose mode, standart out is duplicated /dev/null
	*/
	if (!verb) {
		if ((null = fopen("/dev/null", "w")) == NULL) {
			sys_err("can not open /dev/null", __FILE__, __LINE__);
			/* NOT REACH */
		}
		dup2(fileno(null), STDOUT_FILENO);
	}

	argc -= optind;
	argv += optind;

	if (argc > 1) {
		sys_warn("sorry, only one hostname or ip address accepted", __FILE__, __LINE__);
	}

	if (argv)
		opt->host = xstrdup(*argv);

	return;
}

/*-------------------------------------------------------------------------
 * translate host name to addrinfo struct
 *-------------------------------------------------------------------------
*/
struct addrinfo *
translate_addr(char *host, char *port) {
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));

	hints.ai_flags     = AI_CANONNAME;
	hints.ai_family    = AF_INET;
	hints.ai_socktype  = opt->socktype;
	hints.ai_protocol  = opt->protocol;

	if (getaddrinfo(host, port, &hints, &res)) {
		sys_err("can not getaddrinfo()", __FILE__, __LINE__);
		/* NOT REACH */
	}

	return res;
}

/*-------------------------------------------------------------------------
 * open socket
 *-------------------------------------------------------------------------
*/
int
open_socket() {
	int sockfd;

	opt->dest = translate_addr(opt->host, opt->port);

	if ((sockfd = socket(opt->dest->ai_family, opt->dest->ai_socktype, opt->dest->ai_protocol)) < 0) {
		sys_err("can not open socket descriptor", __FILE__, __LINE__);
		/* NOT REACH */
	}
	if (opt->protocol == IPPROTO_TCP) {
		if (connect(sockfd, opt->dest->ai_addr, opt->dest->ai_addrlen) < 0) {
			sys_err("can not connect to target", __FILE__, __LINE__);
			/* NOT REACH */
		}
	}

	return sockfd;
}

/*-------------------------------------------------------------------------
 * print progress
 *-------------------------------------------------------------------------
*/
static off_t __ub_total = 0;
static off_t __ub_current = 0;

static off_t __ub_diff_prev = 0;
static off_t __ub_diff_bit_prev = 0;
static off_t __ub_diff = 0;
static int __ub_diff_count_prev = 0;
static int __ub_diff_count = 0;

static int __ub_calc_digit(int);
static char * __ub_convert_unit(off_t);
static off_t __ub_calc_diff(void);
static void __ub_print_cr(void);
static void __ub_print_progress_unlimited(void);
static void __ub_print_progress_limited(void);
static void __ub_set_signal_handler(int);

int
__ub_calc_digit(int value) {
	return (value > 99 ? 3 : (value > 9 ? 2 : 1));
}

char * 
__ub_convert_unit(off_t diff) {
	float value = 0;
	int digit = 0;
	char unit;
	/*
	 * char p[] filled with the followings;
	 *    "XXX Y", 6 bytes string or,
	 *    "X.XX Y", 7 bytes string or,
	 *    "XX.X Y", 7 bytes string
	 * Maximaum size is 7, incluing termination character '\0'.
	*/
	char *p;

	p = xmalloc(8);
	memset(p, 0, 8);
	/* giga byte */
	if (diff >= 1000000000) {
		value = diff / 1000000000;
		digit = __ub_calc_digit((int)value);
		unit = 'G';
	}
	/* mega byte */
	else if (diff >= 1000000) {
		value = diff / 1000000;
		digit = __ub_calc_digit((int)value);
		unit = 'M';
	}
	/* kiro byte */
	else if (diff >= 1000) {
		value = diff / 1000;
		digit = __ub_calc_digit((int)value);
		unit = 'K';
	}

	switch (digit) {
		case 1:
			sprintf(p, "%1.2f %c", value, unit);
			break;
		case 2:
			sprintf(p, "%2.1f %c", value, unit);
			break;
		default:
			sprintf(p, "%d %c", (int)value, unit);
			break;
	}
	return (p);
}

off_t
__ub_calc_diff() {
	__ub_diff_prev = __ub_diff;
	__ub_diff_bit_prev = __ub_diff * 8;
	__ub_diff = 0;

	__ub_diff_count_prev = __ub_diff_count;
	__ub_diff_count = 0;

	return (__ub_diff_bit_prev);
}

void
__ub_print_cr(void) {
	fprintf(stderr, "\r");
	return;
}

void
__ub_print_progress_unlimited(void) {
	static char *msg = "unlimited mode, bandwidth approximately %s bps  ";
	char *p;

	__ub_print_cr();
	fprintf(stderr, msg, (p = __ub_convert_unit(__ub_calc_diff())));

	xfree(p);
	alarm(1);
	return;
}

void
__ub_print_progress_limited(void) {
	static char *msg = "limited mode, progress: (%d%%), total %s bytes, %s bytes sent, bandwidth approximately %s bps  ";
	off_t prog;
	char *t, *c, *d; /* p links total, q links current, r links diff */

	prog = (100 * __ub_current) / __ub_total;

	__ub_print_cr();
	fprintf(stderr, msg, prog,
		(t = __ub_convert_unit(__ub_total)),
		(c = __ub_convert_unit(__ub_current)),
		(d = __ub_convert_unit(__ub_calc_diff())));

	xfree(t); xfree(c); xfree(d);
	alarm(1);
	return;
}

void
__ub_set_signal_handler(int limit) {
	struct sigaction act, oact;

	if (limit)
		act.sa_handler = (void (*)())__ub_print_progress_limited;
	else
		act.sa_handler = (void (*)())__ub_print_progress_unlimited;

	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	//act.sa_flags |= SA_RESTART;

	if (sigaction(SIGALRM, &act, &oact) < 0)
		fprintf(stderr, "can not set signal hander SIGALRM\n");

	alarm(1);
	return;
}

int
ub_set_progress_bar(unsigned long int size) {
	__ub_total = size;
	__ub_current = 0;
	__ub_diff = 0;
	__ub_diff_count = 0;
	__ub_set_signal_handler(size);
	return (1);
}

void
ub_progress_countup(off_t c) {
	__ub_current += c;
	__ub_diff += c;
	++__ub_diff_count;
	return;
}

void
ub_sigsend(pid_t myself) {
	if (kill(myself, SIGALRM) != 0) {
		fprintf(stderr, "can not send sigalrm, quit immediately\n");
		exit (1);
	}
	return;
}


size_t 
calc_size_packet(size_t max_lenght) {
	static int previous_packet_size = 0;
	int packet_size;

	/* only first time */
	if (previous_packet_size == 0) {
		previous_packet_size = max_lenght;
		return (max_lenght);
	}

	/* in case of not sent any packets, just return */
	if (__ub_diff_bit_prev == 0) {
		return (max_lenght);
	}

	/* 
	 * Adjust size of packet which will be sent in next second.
	 * In case of low load it must bust, or in case of high
	 * load it must slow down.
	 * To determine whether bursting or slowing down, at first
	 * calculate last-second total amount of packet(s), then compare 
	 * the bandwidth you specified with it.
	 *
	*/
	if (opt->bandwidth < __ub_diff_bit_prev) {
		/* slow down */
		off_t diff, next, unit;
		diff = __ub_diff_bit_prev - opt->bandwidth;
		next = opt->bandwidth - diff;
		unit = next / __ub_diff_count_prev;
		packet_size = unit / 8; /* convert unit, bit to byte */
		if (packet_size < 16) {
			packet_size = 16;
		}
	} else {
		/* burst */
		off_t diff, next, unit;
		diff = opt->bandwidth - __ub_diff_bit_prev;
		next = opt->bandwidth + diff;
		unit = next / __ub_diff_count_prev;
		packet_size = unit / 8; /* convert unit, bit to byte */
		if (packet_size > max_lenght) {
			packet_size = max_lenght;
		}
	}

	previous_packet_size = packet_size;

	return (packet_size);
}

/*-------------------------------------------------------------------------
 * main
 *-------------------------------------------------------------------------
*/
int
main(int argc, char **argv) {
	int sockfd;
	SA_IN *sin;

	long int remain; /* for opt->size, to calculate the size of the rest of traffic */

	size_t ncontent;
	char data[CHUNKSIZE], content[CHUNKSIZE];

	pid_t myself = getpid();


	/* go */
	option(argc, argv);
	sockfd = open_socket();

	sin = (SA_IN *)opt->dest->ai_addr;
	printf("connect to %s(%s) \n", inet_ntoa(sin->sin_addr), opt->port);
	printf("transfered(k byte): ");

	ub_set_progress_bar(opt->size);
	ub_sigsend(myself);

	memset(data, 0, sizeof(data));
	memset(content, '0', sizeof(content));

	for(remain = opt->size; ;) {
		size_t cc;
		/*
		 * At first make the content of packet and calculate its size.
		 * Allay of data is real content of the packet, content is a format of it,
		 * which is filled with '0' literally.
		*/
		if (opt->bandwidth) {
			ncontent = calc_size_packet(sizeof(content));
		} else {
			ncontent = sizeof(data);
		}
		snprintf(data, ncontent, content, NULL);
		if ((cc = sendto(sockfd, data, ncontent, 0, opt->dest->ai_addr, opt->dest->ai_addrlen)) > 0) {
			remain -= cc;
			ub_progress_countup(cc);
			/*
			 * In case of limited mode (opt->size is greater than 0),
			 * print on terminal "100%" immediatelly as generating traffic ended up.
			*/
			if (opt->size) {
				if (remain <= 0) {
					ub_sigsend(myself);
					fprintf(stderr, "...completed\n");
					break;
				}
			}
		}
		else {
			sys_err("sendto() failure, terminating connection immediately", __FILE__, __LINE__);
			/* NOT REACH */
		}
	}

	if (opt->protocol == IPPROTO_TCP) {
		if (shutdown(sockfd, SHUT_WR) == 0) {	/* send FIN */
			printf("tcp session closed\n");
		}
		else {
			sys_err("tcp close failure, terminating connection immediately", __FILE__, __LINE__);
			/* NOT REACH */
		}
	}

	return 0;
}

/* end of source */
