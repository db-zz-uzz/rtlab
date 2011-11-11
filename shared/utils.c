#include "utils.h"
#include "../include/utils.h"

#include <stdlib.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <string.h>

#include <xcrypt.h>


#define CONN_DOMAIN		AF_INET
#define CONN_SOCKET		SOCK_STREAM
/* tcp protocol */
#define CONN_PROTO		"tcp"

int
bind_addr(int port, int backlog)
{
	int s;
	struct protoent *ppe;
	struct sockaddr_in sin = {0};

	sin.sin_family = CONN_DOMAIN;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(port);

	if ( (ppe = getprotobyname(CONN_PROTO)) == NULL ) {
		handle_error("getprotobyname()");
	}

	s = socket(CONN_DOMAIN, CONN_SOCKET, ppe->p_proto);
	if ( s < 0 ) {
		handle_error_en(s, "socket()");
	}

	if ( bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0 ) {
		handle_error("bind()");
	}

	if ( listen(s, backlog) < 0 ) {
		handle_error("listen()");
	}

	return s;
}

int
connect_to(char* addr, char *port)
{
	struct addrinfo hints = {0};
	struct addrinfo *info_res, *rp;
	int s, sfd;

	hints.ai_family = CONN_DOMAIN;
	hints.ai_socktype = CONN_SOCKET;

	s = getaddrinfo(addr, port, &hints, &info_res);
	if ( s != 0 ) {
		handle_error(gai_strerror(s));
	}

	for (rp = info_res; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; /* success connect */

		close(sfd);
		sfd = -1;
	}

	freeaddrinfo(info_res);

	return sfd;
}

static void
setnonblocking(int sock)
{
	int opts;

	opts = fcntl(sock, F_GETFL);
	if (opts < 0) {
		perror("fcntl(F_GETFL)");
		exit(EXIT_FAILURE);
	}
	opts = (opts | O_NONBLOCK);
	if (fcntl(sock, F_SETFL, opts) < 0) {
		perror("fcntl(F_SETFL)");
		exit(EXIT_FAILURE);
	}
	return;
}

int
close_connection(int epollfd, struct fd_list_head *fd_list, int fd, const char *debug_str)
{
	struct fd_list_entry *np = NULL;

	for (np = fd_list->lh_first; np != NULL; np = np->l.le_next) {
		if (np->fd == fd) {
			epoll_ctl(epollfd, EPOLL_CTL_DEL, np->fd, NULL);
			close(np->fd);

			LIST_REMOVE(np, l);

			printf("connection closed: %s:%u (%s)\n",
					inet_ntoa(np->addr.sin_addr),
					ntohs(np->addr.sin_port),
					debug_str);

			free(np);
			/* closed properly */
			return 1;
		}
	}

	/* cannot find appropriate fd */
	return 0;
}

int
make_connection(int epollfd, struct fd_list_head *fd_list, int fd)
{
	struct fd_list_entry *fd_entry = NULL;
	struct sockaddr_in addr = {0};
	socklen_t addr_len = sizeof(addr);
	int conn_sock;
	struct epoll_event ev;

	if ( (conn_sock = accept(fd, (struct sockaddr *)&addr, &addr_len)) == -1 ) {
		handle_error("accept()");
	}
	setnonblocking(conn_sock);

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = conn_sock;

	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev) == -1) {
		handle_error("epoll_ctl()");
	}

	fd_entry = malloc(sizeof(struct fd_list_entry));
	fd_entry->fd = conn_sock;
	memcpy(&(fd_entry->addr), &addr, addr_len);
	LIST_INSERT_HEAD(fd_list, fd_entry, l);

	printf("new connection: %s:%u\n",
 			inet_ntoa(addr.sin_addr),
			ntohs(addr.sin_port));

	return 0;
}

int
sample_alloc_buffer(PSSAMPLE sample, uint32_t size)
{
	uint32_t alloc_size = size * sample->header->channels * sample->header->sample_size;

	alloc_size ?: size;

	if ( (sample->buf = malloc(alloc_size)) != NULL ) {
		sample->buf_size = alloc_size;

		return 0;
	}

	sample->buf_size = 0;
	return -1;
}

int
sample_free_buffer(PSSAMPLE sample)
{
	if ( sample->buf != NULL ) {
		free(sample->buf);
		sample->buf = NULL;
		sample->buf_size = 0;
	}

	if (sample->private_destructor) {
		(sample->private_destructor)(sample->private_data);
	}

	return 0;
}

void
sample_zero_buffer(PSSAMPLE sample)
{
	if (sample->buf) {
		memset(sample->buf, 0, sample->buf_size);
	}

	return;
}

int
deliver_data(struct fd_list_head *fd_list, PSSAMPLE sample)
{
	/* returns count of fds to which delivered */
	uint8_t count = 0;
	struct fd_list_entry *np = NULL;

	for (np = fd_list->lh_first; np != NULL; np = np->l.le_next) {
		if (sample->write_func) {
			if (sample->write_func(np->fd, sample)) {
				close_connection(sample->epollfd, fd_list, np->fd, "point 2");
			}
		}
	}

	return count;
}

int
print_md5(PSSAMPLE sample)
{
	uint8_t i;
	uint8_t res[16];

	md5_buffer((char *)sample->buf, BUF_SIZE(sample->header), (void *)res);

	printf("[%6u] %i.%06i  %u/%u/%u (%s)  ",
			sample->header->number,
			sample->header->timestamp.tv_sec,
			sample->header->timestamp.tv_usec,
			sample->header->channels,
			sample->header->sample_size,
			sample->header->samples,
			sample->header->buf_type == BUF_TYPE_INTERLEAVED ? "I" : "S");

	for (i = 0; i < 16; i++) {
		printf("%02x", res[i]);
	}
	printf("  %u B\n", BUF_SIZE(sample->header));
/*
	printf("      ");
	for (i = 0; i < 48; i += 2) {
		printf("%02x%02x ", sample->buf[i], sample->buf[i+1]);
	}
	printf("\n");
*/
	return 0;
}


void
print_buf_head(void *buf, int size, char *prefix)
{
	int i;
	printf(" %s:", prefix ?: "");
	for (i = 0; i < size / sizeof(uint16_t); i++) {
		printf("%04x ", ((uint16_t *)buf)[i]);
	}
	printf("\n");
}

int
do_write_fd(int fd, PSSAMPLE sample)
{
	int res;
	int written;
	uint32_t data_size = HEADER_SIZE + BUF_SIZE(sample->header);
	res = data_size;

	uint8_t *bufptr = malloc (data_size);

	memcpy(bufptr, sample->header, HEADER_SIZE);
	memcpy(bufptr + HEADER_SIZE, sample->buf, BUF_SIZE(sample->header));
/*
	print_buf_head(bufptr, 48, "buffer");
	print_buf_head(sample->header, 48, "header");
*/

	if ( (written = write(fd, bufptr, data_size)) < 1) {
		return res;
	}
	res -= written;

	free(bufptr);

	return res;
}
