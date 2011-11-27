#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pin.h"
#include "macros.h"

/* ********************/
/* internal functions */

#define CONN_DOMAIN		AF_INET
#define CONN_SOCKET		SOCK_STREAM
/* tcp protocol */
#define CONN_PROTO		"tcp"

static int
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

static int
connect_to(char* addr, char *port)
{
	struct addrinfo hints = {0};
	struct addrinfo *info_res, *rp;
	int s, sfd;

	hints.ai_family = CONN_DOMAIN;
	hints.ai_socktype = CONN_SOCKET;

	if ( (s = getaddrinfo(addr, port, &hints, &info_res)) != 0 ) {
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



static HPIN
pin_create()
{
	HPIN pin = NULL;

	pin = malloc(sizeof(SPIN));
	if (pin) {
		memset(pin, 0, sizeof(SPIN));
		pin->buf_list_tail = &(pin->buf_list_head);
	}

	return pin;
}

static void
pin_destroy(HPIN pin)
{
	PSBUFLISTENTRY buf_entry;

	if (!pin)
		return;

	while (pin->buf_list_head) {
		buf_entry = pin->buf_list_head;
		pin->buf_list_head = buf_entry->next;

		if (buf_entry->buffer) {
			buf_entry->buffer[0] -= 1;
			if (buf_entry->buffer[0] == 0)
				free(buf_entry->buffer);
		}

		free(buf_entry);
	}

	free(pin);
}

void
pin_list_insert_pin(HPINLIST pin_list, HPIN new_pin)
{
	HPIN pin = NULL;

	if (!pin_list || !new_pin)
		return;

	if (!pin_list->head) {
		pin_list->head = new_pin;
		new_pin->parent_list = pin_list;
		return;
	}

	for (pin = pin_list->head; pin->next_pin != NULL; pin = pin->next_pin) {
		/* */
	}

	if (pin) {
		pin->next_pin = new_pin;
		new_pin->parent_list = pin_list;
	}

	return;
}

HPIN
pin_list_get_pin(HPINLIST pin_list, int fd)
{
	HPIN pin = NULL;

	if (!pin_list)
		return pin;

	if (!pin_list->head)
		return pin;

	for (pin = pin_list->head; pin != NULL; pin = pin->next_pin) {
		if (pin->fd == fd) {
			break;
		}
	}

	return pin;
}

void
pin_list_attach_event(HPIN *pin_ptr, int *size, HPIN pin)
{
	int i;
	for (i = 0; i < *size; i++) {
		if (pin_ptr[i] == pin) {
			return;
		}
	}

	pin_ptr[*size] = pin;
	*size += 1;

	return;
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

/* ********************/
/* external functions */

HPINLIST
pin_list_create()
{
	HPINLIST pin_list = malloc(sizeof(SPINLIST));

	if (pin_list) {
		memset(pin_list, 0, sizeof(SPINLIST));
		pin_list->epollfd = -1;
	}

	return pin_list;
}

void
pin_list_destroy(HPINLIST pin_list) {
	HPIN pin;

	if (!pin_list)
		return;

	while (pin_list->head) {
		pin = pin_list->head;
		pin_list->head = pin->next_pin;

		pin_disconnect(pin);
	}

	if (pin_list->read_list)
		free(pin_list->read_list);

	if (pin_list->write_list)
		free(pin_list->write_list);

	free(pin_list->events);
	free(pin_list);
}

int
pin_listen(HPINLIST pin_list, int port, int events_count, int backlog)
{
	int listen_sock, epollfd;
	HPIN pin;
	HPIN *read_list, *write_list;
	struct epoll_event ev, *events;

	if (!pin_list || pin_list->epollfd != -1)
		return PIN_ERROR;

	if (!(events = malloc(events_count * sizeof(struct epoll_event))) ||
		!(read_list = malloc(events_count * sizeof(HPIN))) ||
		!(write_list = malloc(events_count * sizeof(HPIN))) ) {

		if (events) {
			free(events);
		}
		if (read_list) {
			free(read_list);
		}
		if (write_list) {
			free(write_list);
		}
		return PIN_ERROR;
	}



	listen_sock = bind_addr(port, backlog);

	if ( (epollfd = epoll_create(events_count)) == -1 ) {
		handle_error("epoll_create()");
	}

	ev.events = EPOLLIN;
	ev.data.fd = listen_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
		handle_error("epoll_ctl()");
	}

	pin = pin_create();
	pin->fd = listen_sock;
	pin->type = PIN_TYPE_LISTEN;

	pin_list->epollfd = epollfd;
	pin_list->events = events;
	pin_list->events_count = events_count;
	pin_list->read_list = read_list;
	pin_list->write_list = write_list;

	pin_list_insert_pin(pin_list, pin);

	return PIN_OK;
}

HPIN
pin_connect(HPINLIST pin_list, char *addr, char *port)
{
	int source_sock;
	HPIN pin;
	struct epoll_event ev;

	if (!pin_list || !addr || !port) {
		return NULL;
	}

	if ( (source_sock = connect_to(addr, port)) == -1 )
	{
		printf("Cannot connect to '%s:%s'\n", addr, port);
		return NULL;
	}

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = source_sock;
	if (epoll_ctl(pin_list->epollfd, EPOLL_CTL_ADD, source_sock, &ev) == -1) {
		handle_error("epoll_ctl()");
	}

	pin = pin_create();
	pin->fd = source_sock;
	pin->type = PIN_TYPE_INPUT;

	pin_list_insert_pin(pin_list, pin);

	return pin;
}

int
pin_disconnect(HPIN pin)
{
	HPINLIST pin_list = NULL;
	HPIN iter = NULL;

	if (!pin) {
		return PIN_ERROR;
	}

	pin_list = pin->parent_list;

	epoll_ctl(pin_list->epollfd, EPOLL_CTL_DEL, pin->fd, NULL);
	close(pin->fd);

	/* deattach pin from list */
	if (pin_list->head == pin) {
		pin_list->head = pin->next_pin;
	} else {
		for (iter = pin_list->head; iter != NULL; iter = iter->next_pin) {
			if (iter->next_pin == pin) {
				iter->next_pin = pin->next_pin;
			}
		}
	}

	pin_destroy(pin);

	return PIN_OK;
}

int
pin_list_wait(HPINLIST pin_list, int timeout)
{
	struct epoll_event *events;
	int nfds, i;
	HPIN pin;

	if (!pin_list || !pin_list->head) {
		return PIN_ERROR;
	}

	events = (struct epoll_event *)pin_list->events;

	if ( (nfds = epoll_wait(pin_list->epollfd, events,
							pin_list->events_count, timeout)) == -1 ) {
		handle_error("epoll_pwait()");
	}

	for (i = 0; i < nfds; i++) {
		pin = pin_list_get_pin(pin_list, events[i].data.fd);

		if (pin->type == PIN_TYPE_LISTEN) {
			/* accept new connection */
			/* :BUG: need to check max connections count */
			struct epoll_event ev;
			HPIN new_pin = pin_create();
			socklen_t addrlen = sizeof(new_pin->addr);

			if ( (new_pin->fd = accept(events[i].data.fd,
										(struct sockaddr *)&new_pin->addr,
										&addrlen)) == -1 ) {
				handle_error("accept()");
			}
			new_pin->type = PIN_TYPE_OUTPUT;
			setnonblocking(new_pin->fd);

			ev.events = EPOLLIN | EPOLLET;
			ev.data.fd = new_pin->fd;

			if (epoll_ctl(pin_list->epollfd, EPOLL_CTL_ADD, new_pin->fd, &ev) == -1) {
				handle_error("epoll_ctl()");
			}

			pin_list_insert_pin(pin_list, new_pin);

			printf("new connection: %s:%u\n",
		 			inet_ntoa(new_pin->addr.sin_addr),
					ntohs(new_pin->addr.sin_port));

		} else {
			if (events[i].events & EPOLLIN) {
				pin_list_attach_event(pin_list->read_list,
									&pin_list->read_list_size,
									pin);
			}
			if (events[i].events & EPOLLOUT) {
				pin_list_attach_event(pin_list->write_list,
									&pin_list->write_list_size,
									pin);
			}
		}
	}

	return PIN_OK;
}

HPIN
pin_list_get_next_event(HPINLIST pin_list, uint16_t event_type)
{
	HPIN pin = NULL;
	HPIN *arr = NULL;
	int *size = NULL;

	if (!pin_list) {
		return NULL;
	}

	switch (event_type) {
		case PIN_EVENT_READ: {
			arr = pin_list->read_list;
			size = &pin_list->read_list_size;
			break;
		}
		case PIN_EVENT_WRITE: {
			arr = pin_list->write_list;
			size = &pin_list->write_list_size;
			break;
		}
		default: {
			return NULL;
		}
	}

	if (*size > 0) {
		*size -= 1;
		pin = arr[*size];
	}

	return pin;
}

int
pin_list_deliver(HPINLIST pin_list)
{
	HPIN pin;
	PSBUFLISTENTRY buf;
	uint32_t ret = 0;

	/* loop through pin list
		and write data if we have pending buffers */
	while ( (pin = pin_list_get_next_event(pin_list, PIN_EVENT_WRITE)) != NULL ) {
	//for (pin = pin_list->head; pin != NULL; pin = pin->next_pin) {

		while ( (buf = pin->buf_list_head) != NULL ) {

			ret = send(pin->fd, buf->buffer + 1 + buf->written, buf->size - buf->written, MSG_DONTWAIT);

			if (ret == 0) {
				HPIN tmp_pin = pin->next_pin;
				fprintf(stderr, "error while sending data (0). disconnect client.\n");
				pin_disconnect(pin);
				pin = tmp_pin;
				/* connection closed */
				return PIN_STATUS_CLOSED;
			} else if (ret == -1) {
				if (errno == EWOULDBLOCK || errno == EAGAIN) {
					/* no more data can be written*/
					break;
				} else {
					/* error occured */
					HPIN tmp_pin = pin->next_pin;

					fprintf(stderr, "%s(): send error\n", __func__);
					pin_disconnect(pin);
					pin = tmp_pin;

					return PIN_STATUS_CLOSED;
				}
			}

			buf->written += ret;

			if (buf->written == buf->size) {
				PSBUFLISTENTRY tmp_buf = buf->next;
				/* if entrie buffer was written, free it and go to next */
				buf->buffer[0] -= 1;
				if (buf->buffer[0] == 0) {
					free(buf->buffer);
				}
				free(buf);

				buf = tmp_buf;
				if ( (pin->buf_list_head = buf) == NULL ) {
					pin->buf_list_tail = &(pin->buf_list_head);
				}

			} else {
				/* continue wait */
				break;
			}
		}

		if (pin->buf_list_head == NULL) {
			/* remove fd from epoll wait */
			struct epoll_event ev;

			ev.events = EPOLLIN | EPOLLET;
			ev.data.fd = pin->fd;

			if (epoll_ctl(pin_list->epollfd, EPOLL_CTL_MOD, pin->fd, &ev) == -1) {
				handle_error("epoll_ctl()");
			}

		}

	}

	return PIN_OK;
}


int
pin_read_sample(HPIN pin, HSAMPLE buffer)
{
	int res = PIN_STATUS_NO_DATA;
	int ret;
	uint32_t read_len;

	read_len = (buffer->alloced_size > buffer->full_size) ?
				buffer->full_size - buffer->size :
				buffer->alloced_size - buffer->size;

	for (;read_len > 0;) {
		ret = recv(pin->fd, buffer->buf + buffer->size, read_len, MSG_DONTWAIT);
		if (ret == 0) {
			/* connection closed */
			return PIN_STATUS_CLOSED;
		} else if (ret == -1) {
			if (errno == EWOULDBLOCK || errno == EAGAIN) {
				/* no more data */
				/* printf("return no-data\n"); */
				break;
			} else {
				/* error occured */
				fprintf(stderr, "%s(): recv error\n", __func__);
				return PIN_STATUS_CLOSED;
			}
		}

		buffer-> size += ret;
		read_len -= ret;

		if (buffer->size == buffer->full_size) {
			if ((buffer->flags & BUFFER_FLAG_HEADER_READED) == 0 &&
				buffer->size_callback != NULL) {

				uint32_t full_size = buffer->size_callback(buffer, BUFFER_SIZE_TYPE_MESSAGE);
				/* have entrie header.
				 * mark buffer as recieved header and get entrie message size */
				buf_resize(buffer, full_size);
				buffer->full_size = full_size;

				read_len = buffer->full_size - buffer->size;

				buffer->flags |= BUFFER_FLAG_HEADER_READED;
			} else {
				/* have complete buffer */
				break;
			}
		}
	}

	if (buffer->size == buffer->full_size) {
		if (buffer->flags & BUFFER_FLAG_HEADER_READED) {
			res = PIN_STATUS_READY;
		} else {
			res = PIN_STATUS_PARTIAL;
		}
	} else {
		res = PIN_STATUS_NO_DATA;
	}

	return res;
}

int
pin_list_write_sample(HPINLIST pin_list, HSAMPLE sample)
{
	PSBUFLISTENTRY packet = NULL;
	HPIN pin;
	uint8_t inited = 0;
	uint8_t *buffer;
	struct epoll_event ev;

//	int loop = 1;

	/* loop through output pins */
	for (pin = pin_list->head; pin != NULL; pin = pin->next_pin) {
		if (pin->type == PIN_TYPE_OUTPUT) {
			if (inited == 0) {
				buffer = malloc(sample->size + 1);
				memcpy(buffer + 1, sample->buf, sample->size);
				buffer[0] = 0;
				inited = 1;
			}
			packet = malloc(sizeof(SBUFLISTENTRY));
			packet->buffer = buffer;
			packet->size = sample->size;
			packet->written = 0;
			packet->next = NULL;
			packet->buffer[0] += 1;

			/* !! POSSIBLE BUG HERE !! Fixed */
/*
			printf("pin %p ", pin);
			printf(" ph %p ", pin->buf_list_head);
			printf(" pt %p ", pin->buf_list_tail);
			printf("*pt %p\n", *pin->buf_list_tail);
*/
			*(pin->buf_list_tail) = packet;
			pin->buf_list_tail = &(packet->next);
/*
			printf("pin %p ", pin);
			printf(" ph %p ", pin->buf_list_head);
			printf(" pt %p ", pin->buf_list_tail);
			printf("*pt %p\n", *pin->buf_list_tail);
*/
			ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
			ev.data.fd = pin->fd;

			if (epoll_ctl(pin_list->epollfd, EPOLL_CTL_MOD, pin->fd, &ev) == -1) {
				handle_error("epoll_ctl()");
			}
		}
	}

	return PIN_OK;
}










