#include <stdio.h>
#include <stdlib.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "utils.h"

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <pulse/timeval.h>

#define MAX_EVENTS 5

int
do_read_fd(int fd, PSSAMPLE sample)
{
	/* read data here */

	return SOCK_USE_CLOSE;
}


int
main(int argc, char *argv[])
{
	int listen_sock, epollfd, nfds, n, error;
	struct epoll_event ev, events[MAX_EVENTS];

	struct fd_list_head fd_list = {0};
	struct fd_list_entry *np = NULL;

	/* buffer length in samples. would be multiplied by channels and sample size */
	int buffer_length = 10000;
	int listen_port = 5002;

	/* record parameters */
    static const pa_sample_spec ss = {
    	/* for fft need PA_SAMPLE_FLOAT32LE or PA_SAMPLE_FLOAT32BE */
        .format = PA_SAMPLE_FLOAT32LE, // PA_SAMPLE_S16LE,
        .rate = 11025,
        .channels = 2
    };

	SSAMPLEHEADER sh = {0};
	SSAMPLE sample = {0};
	sample.header = &sh;

	pa_simple *pa_context = NULL;

	if (argc > 1) {
		sscanf(argv[1], "%i", &listen_port);
		printf("Will listen %i port\n", listen_port);
	}

	listen_sock = bind_addr(listen_port, 50);

	if ( !(pa_context = pa_simple_new(NULL, argv[0],
							PA_STREAM_RECORD, NULL, "record",
							&ss, NULL, NULL, &error)) ) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
	}

	if ( (epollfd = epoll_create(MAX_EVENTS)) == -1 ) {
		handle_error("epoll_create()");
	}

	ev.events = EPOLLIN;
	ev.data.fd = listen_sock;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_sock, &ev) == -1) {
		handle_error("epoll_ctl()");
	}

	sh.channels = ss.channels;
	sh.sample_size = pa_sample_size(&ss) / 2;
	sh.samples = buffer_length;
	sh.buf_type = BUF_TYPE_INTERLEAVED;

	sample_alloc_buffer(&sample, buffer_length);
	sample.write_func = do_write_fd;
	sample.epollfd = epollfd;

	LIST_INIT(&fd_list);

	for (;;) {
		sample_zero_buffer(&sample);

        if (pa_simple_read(pa_context, sample.buf, sample.buf_size, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }
		pa_gettimeofday(&sh.timestamp);
		sh.number++;

		if ( (nfds = epoll_wait(epollfd, events, MAX_EVENTS, 0)) == -1 ) {
			handle_error("epoll_pwait()");
		}

		/* handle connections */
		for (n = 0; n < nfds; n++) {
			if (events[n].data.fd == listen_sock) {
				/* accept new connection */
				make_connection(epollfd, &fd_list, events[n].data.fd);

			} else {
				/* use socket */
				switch (do_read_fd(events[n].data.fd, &sample)) {
					case SOCK_USE_CLOSE: {
						close_connection(epollfd, &fd_list, events[n].data.fd, "point 1");
						break;
					}
					case SOCK_USE_OK:
					default: {
						break;
					}
				}
			}
		}

		print_md5(&sample);
		deliver_data(&fd_list, &sample);
	}

	sample_free_buffer(&sample);

finish:

	while (fd_list.lh_first != NULL) {
		np = fd_list.lh_first;
		LIST_REMOVE(np, l);

		epoll_ctl(epollfd, EPOLL_CTL_DEL, np->fd, NULL);
		close(np->fd);

		free(np);
	}
	close(listen_sock);

	pa_simple_free(pa_context);

	exit(EXIT_SUCCESS);
}
