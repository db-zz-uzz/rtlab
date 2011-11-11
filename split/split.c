#include <stdio.h>
#include <stdlib.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#include "utils.h"
#include "workers.h"

#define MAX_EVENTS 5


static void
private_destructor(void *data)
{
	PSSAMPLESTATE state = (PSSAMPLESTATE)data;
	if (!state) {
		return;
	}
	if (state->buf) {
		free(state->buf);
	}
	return;
}

static int
do_split_2ch2bytes(PSSAMPLE sample)
{
	PSSAMPLEHEADER header = sample->header;
	uint32_t i;
//	uint32_t buf_len = header->samples * header->channels;

	uint16_t *buf = (uint16_t *)sample->buf;
	uint16_t *splitted = NULL;

	splitted = malloc(BUF_SIZE(header));

	for (i = 0; i < header->samples; i++) {
		splitted[i] = buf[i * 2];
		splitted[i + header->samples] = buf[i * 2 + 1];
	}

	memcpy(sample->buf, splitted, BUF_SIZE(header));
	free(splitted);

	return 0;
}

static int
do_split_2ch4bytes(PSSAMPLE sample)
{
	PSSAMPLEHEADER header = sample->header;
	uint32_t i;
//	uint32_t buf_len = header->samples * header->channels;

	uint32_t *buf = (uint32_t *)sample->buf;
	uint32_t *splitted = NULL;

	splitted = malloc(BUF_SIZE(header));

	for (i = 0; i < header->samples; i++) {
		splitted[i] = buf[i * 2];
		splitted[i + header->samples] = buf[i * 2 + 1];
	}

	memcpy(sample->buf, splitted, BUF_SIZE(header));
	free(splitted);

	return 0;
}

int
do_process_data(PSSAMPLE sample)
{
	PSSAMPLESTATE state = (PSSAMPLESTATE)sample->private_data;
	int (*proc_func)(PSSAMPLE) = NULL;

	//printf("data processed\n");
	state->flags |= STATE_FLAG_DATA_PROCESSED;

	if (sample->header->channels == 1) {
		sample->header->buf_type = BUF_TYPE_SEQUENTIAL;
		return 0;
	}

	switch (sample->header->sample_size) {
		case 2: {
			proc_func = do_split_2ch2bytes;
			break;
		}
		case 4: {
			proc_func = do_split_2ch4bytes;
			break;
		}
		default: {
			/* leave data as is */
			break;
		}
	}

	if (proc_func) {
		proc_func(sample);
		sample->header->buf_type = BUF_TYPE_SEQUENTIAL;
	}

	return 0;
}

int
main(int argc, char *argv[])
{
	int listen_sock, source_sock, epollfd, nfds, n;
	struct epoll_event ev, events[MAX_EVENTS];

	struct fd_list_head fd_list = {0};
	struct fd_list_entry *np = NULL;

	uint32_t buf_size = 0;
	uint8_t *buf = NULL;

	uint8_t active = 1;

	int listen_port = 15020;

	/* record parameters */
	SSAMPLE sample = {0};

	if (argc < 3) {
		printf("use: split <host> <port>\n");
		return 0;
	}

	if (argc > 3) {
		sscanf(argv[3], "%i", &listen_port);
		printf("Will listen %i port\n", listen_port);
	}

	source_sock = connect_to(argv[1], argv[2]);
	listen_sock = bind_addr(listen_port, 50);

	if ( (epollfd = epoll_create(MAX_EVENTS)) == -1 ) {
		handle_error("epoll_create()");
	}

	ev.events = EPOLLIN;
	ev.data.fd = listen_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
		handle_error("epoll_ctl()");
	}

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = source_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, source_sock, &ev) == -1) {
		handle_error("epoll_ctl()");
	}

	sample.epollfd = epollfd;
	sample.write_func = do_write_fd;
	sample.private_data = calloc(1, sizeof(SSAMPLESTATE));
	sample.private_destructor = private_destructor;

	LIST_INIT(&fd_list);

	printf("enter loop\n");
	while (active) {
		memset(buf, 0, buf_size);

		if ( (nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1)) == -1 ) {
			handle_error("epoll_pwait()");
		}

		/* handle connections */
		for (n = 0; n < nfds; n++) {
			if (events[n].data.fd == listen_sock) {
				/* accept new connection */
				printf("accept connection\n");
				make_connection(epollfd, &fd_list, events[n].data.fd);

			} else {
				/* use socket */
				/* read data from input socket,
					transform data if buffer is ready
					and write it to all output sockets */
				switch (do_read_fd(events[n].data.fd, &sample)) {
					case SOCK_USE_CLOSE:
					{
						if (!close_connection(epollfd, &fd_list, events[n].data.fd, "point 1"))
						{
							printf("Sample source closed connection. Exit.\n");
							active = 0;
							continue;
						}
						break;
					}
					case SOCK_USE_READY:
					{
						print_md5(&sample);

						do_process_data(&sample);

						print_md5(&sample);
						/* try to write data into open connections */
						deliver_data(&fd_list, &sample);
						break;
					}
					case SOCK_USE_OK:
					default:
					{
						break;
					}
				}
			}
		}
	}

	sample.buf = NULL;
	sample_free_buffer(&sample);

	while (fd_list.lh_first != NULL) {
		np = fd_list.lh_first;
		LIST_REMOVE(np, l);

		epoll_ctl(epollfd, EPOLL_CTL_DEL, np->fd, NULL);
		close(np->fd);

		free(np);
	}
	close(listen_sock);

	exit(EXIT_SUCCESS);
}
