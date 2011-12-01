#include <stdio.h>
#include <stdlib.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>

#include "audio_sample.h"
#include "buffer.h"
#include "pin.h"
#include "process_data.h"
#include "timing.h"

#define MAX_EVENTS 5
#define BACKLOG 50

#define PIN_OUT_LEFT	0x01
#define PIN_OUT_RIGHT	0x02

void
split_accepct_callback(HPIN parent_pin, HPIN new_pin)
{
	pin_set_flags(new_pin, pin_get_flags(parent_pin));
}

int
main(int argc, char *argv[])
{
	uint8_t active = 1;

	int listen_left_port = 15020;
	int listen_right_port = 15021;

	HPINLIST connection = NULL;
	HPIN pin;
	HPIN input_pin, listen_left_pin, listen_right_pin;
	HBUF sample, input_sample, dummy_sample;
	HBUF processed_left_sample = NULL;
	HBUF processed_right_sample = NULL;

	TIMING_MEASURE_AREA;

	input_sample = buf_alloc(sample_size_callback);
	dummy_sample = buf_alloc(dummy_size_callback);

	if (argc < 3 || argc == 4) {
		printf("use: split <host> <port> <listen_left_port> <listen_right_port>\n");
		return 0;
	}

	if (argc > 3) {
		sscanf(argv[3], "%i", &listen_left_port);
		sscanf(argv[4], "%i", &listen_right_port);
		printf("Will listen: left on %i, right on %i port\n", listen_left_port, listen_right_port);
	}

	connection = pin_list_create(MAX_EVENTS);

	listen_left_pin = pin_listen(connection, listen_left_port, BACKLOG, split_accepct_callback);
	pin_set_flags(listen_left_pin, PIN_OUT_LEFT);

	listen_right_pin = pin_listen(connection, listen_right_port, BACKLOG, split_accepct_callback);
	pin_set_flags(listen_right_pin, PIN_OUT_RIGHT);

	input_pin = pin_connect(connection, argv[1], argv[2]);

	if (!input_pin)
		active = 0;

	if (active)
		printf("Enter main loop\n");

	while (active && pin_list_wait(connection, -1) != PIN_ERROR) {

		/* loop for pins with active events */
		while ( (pin = pin_list_get_next_event(connection, PIN_EVENT_READ)) != NULL ) {

			sample = pin == input_pin ? input_sample : dummy_sample;

			switch (pin_read_sample(pin, sample)) {
				case PIN_STATUS_READY:
				{
					TIMING_START();

					PSSAMPLEHEADER header = (PSSAMPLEHEADER)sample->buf;
					print_header(header, sample->buf + HEADER_SIZE, sample->size - HEADER_SIZE);

					/* process data here */
					do_process_data(sample, &processed_left_sample, CHANNEL_LEFT);
					do_process_data(sample, &processed_right_sample, CHANNEL_RIGHT);

					print_header((PSSAMPLEHEADER)processed_left_sample->buf,
								processed_left_sample->buf + HEADER_SIZE,
								processed_left_sample->size - HEADER_SIZE);

					print_header((PSSAMPLEHEADER)processed_right_sample->buf,
								processed_right_sample->buf + HEADER_SIZE,
								processed_right_sample->size - HEADER_SIZE);

					pin_list_write_sample(connection, processed_left_sample, PIN_OUT_LEFT);
					pin_list_write_sample(connection, processed_right_sample, PIN_OUT_RIGHT);
					sample->size = 0;

					TIMING_END("split");
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
					// printf(" partial data. %u / %u\n", sample->size, sample->full_size);
					/* do nothing since no data ready */
					break;
				}
				case PIN_STATUS_NO_DATA:
				{
					// printf("      no data. %u / %u\n", sample->size, sample->full_size);
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

	pin_list_destroy(connection);

	exit(EXIT_SUCCESS);
}
