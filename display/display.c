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

#define MAX_EVENTS 5

int
main(int argc, char *argv[])
{
	uint8_t active = 1;

	HPINLIST connection = NULL;
	HPIN pin;
	HPIN input_spl_pin, input_fft_pin;
	HSAMPLE sample, input_spl_sample, input_fft_sample, dummy_sample;

	if (argc < 5) {
		printf("use: display <splitted_host> <splitted_port> <ffted_host> <ffted_port>\n");
		return 0;
	}

	connection = pin_list_create(MAX_EVENTS);
	input_spl_pin = pin_connect(connection, argv[1], argv[2]);
	input_fft_pin = pin_connect(connection, argv[3], argv[4]);

	input_spl_sample = buf_alloc(sample_size_callback);
	input_fft_sample = buf_alloc(sample_size_callback);
	dummy_sample = buf_alloc(dummy_size_callback);

	if (!input_spl_pin || !input_fft_pin)
		active = 0;

	if (active)
		printf("Enter main loop\n");

	while (active && pin_list_wait(connection, -1) != PIN_ERROR) {

		/* loop for pins with active events */
		while ( (pin = pin_list_get_next_event(connection, PIN_EVENT_READ)) != NULL ) {

			sample = (pin == input_spl_pin) ?
						input_spl_sample :
						( (pin == input_fft_pin) ?
							input_fft_sample :
							dummy_sample );

			switch (pin_read_sample(pin, sample)) {
				case PIN_STATUS_READY:
				{
					PSSAMPLEHEADER header = (PSSAMPLEHEADER)sample->buf;
					print_header(header, sample->buf + HEADER_SIZE, sample->size - HEADER_SIZE);

					/* process data here */

					sample->size = 0;
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
					printf(" partial data. %u / %u\n", sample->size, sample->full_size);
					/* do nothing since no data ready */
					break;
				}
				case PIN_STATUS_NO_DATA:
				{
					printf("      no data. %u / %u\n", sample->size, sample->full_size);
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
