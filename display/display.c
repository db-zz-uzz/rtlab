#include <stdio.h>
#include <stdlib.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#include "audio_sample.h"
#include "buffer.h"
#include "pin.h"
#include "timing.h"
#include "macros.h"

#include "display.h"

#define MAX_EVENTS 5

int
main(int argc, char *argv[])
{
	uint8_t active = 1;
	int s;

	HPINLIST connection = NULL;
	HPIN pin;
	HPIN input_l_pin, input_r_pin;
	HBUF *sample, input_l_sample, input_r_sample, dummy_sample;

#ifdef PRINT_DEBUG
	pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

	int reordfd[2];
	int uifd[2];
	int reorderer_fd;

	STHRPARAMS reord_param = {0}, ui_param = {0};
	pthread_t reord_thr, ui_thr;

	if (argc < 5) {
		printf("use: display <ffted_left_host> <ffted_left_port> <ffted_right_host> <ffted_right_port>\n");
		return 0;
	}

	/* create pipe/queue, mutex */

	/* spawn ui thread */

	/* wait mutex unlocked (ui thread being inited) */

	connection = pin_list_create(MAX_EVENTS);
	input_l_pin = pin_connect(connection, argv[1], argv[2]);
	input_r_pin = pin_connect(connection, argv[3], argv[4]);

	input_l_sample = buf_alloc(sample_size_callback);
	input_r_sample = buf_alloc(sample_size_callback);
	dummy_sample = buf_alloc(dummy_size_callback);

	if (!input_l_pin || !input_r_pin) {
		active = 0;
		/* terminate/signal to exit ui thread */
	}

	if (pipe(reordfd) == -1)
		handle_error("pipe()");

	if (pipe(uifd) == -1)
		handle_error("pipe()");

	setnonblocking(reordfd[1]);
	setnonblocking(reordfd[0]);
	setnonblocking(uifd[1]);
	setnonblocking(uifd[0]);

	reorderer_fd = reordfd[1];
	reord_param.infd = reordfd[0];
	reord_param.outfd = uifd[1];
	ui_param.infd = uifd[0];
	reord_param.argc = argc;
	reord_param.argv = argv;
	ui_param.argc = argc;
	ui_param.argv = argv;

#ifdef PRINT_DEBUG
	reord_param.print_mutex = ui_param.print_mutex = &print_mutex;
#endif

	if ( (s = pthread_create(&reord_thr, NULL, reorderer_thr, &reord_param)) != 0) {
		handle_error_en(s, "pthread_create()");
	}

	if ( (s = pthread_create(&ui_thr, NULL, spawn_ui_thr, &ui_param)) != 0) {
		handle_error_en(s, "pthread_create()");
	}

	if (active)
		printf("Enter main loop\n");

	while (active && pin_list_wait(connection, -1) != PIN_ERROR) {

		/* loop for pins with active events */
		while ( (pin = pin_list_get_next_event(connection, PIN_EVENT_READ)) != NULL ) {

			if (pin == input_l_pin) {
				/* safe_print(&print_mutex, "choose left\n"); */
				sample = &input_l_sample;
			} else if (pin == input_r_pin) {
				/* safe_print(&print_mutex, "choose right\n"); */
				sample = &input_r_sample;
			} else {
				sample = &dummy_sample;
			}

			switch (pin_read_sample(pin, *sample)) {
				case PIN_STATUS_READY:
				{
/*
					PSSAMPLEHEADER header = (PSSAMPLEHEADER)(*sample)->buf;
					PRINT_LOCK(&print_mutex);
					print_header(header, (*sample)->buf + HEADER_SIZE, (*sample)->size - HEADER_SIZE);
					PRINT_UNLOCK(&print_mutex);
*/
					/* send data to reorderer */
					if ( (s = write(reorderer_fd, sample, PTR_SIZE)) != PTR_SIZE ) {
						if (s == -1) {
							handle_error("write()");
						}

						printf("[reciever] overrun. free buf.\n");
						buf_free(*sample);
					}

					*sample = buf_alloc(sample_size_callback);
					//sample->size = 0;
					break;
				}
				case PIN_STATUS_CLOSED:
				{
					if (pin->type == PIN_TYPE_INPUT) {
						printf("one of inputs closed. exit.\n");
						active = 0;
						continue;
					} else {
						printf("connection closed\n");
					}
					pin_disconnect(pin);
					/* close data and skip iteration */
					continue;
				}
				case PIN_STATUS_PARTIAL:
				{
					printf(" partial data. %u / %u\n", (*sample)->size, (*sample)->full_size);
					/* do nothing since no data ready */
					break;
				}
				case PIN_STATUS_NO_DATA:
				{
					printf("      no data. %u / %u\n", (*sample)->size, (*sample)->full_size);
					/* do nothing since no data ready */
					break;
				}
				default:
				{
					break;
				}
			}
		}

		pin_list_deliver(connection);
	}

	/* join ui thread */
	{
		void *p = MESSAGE_END;
		write(reordfd[0], &p, sizeof(p));

		pthread_join(reord_thr, NULL);
	}

	pin_list_destroy(connection);

	exit(EXIT_SUCCESS);
}
