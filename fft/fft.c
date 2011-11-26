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
#define BACKLOG 50

static uint32_t
sample_size_callback(HBUF buf, uint8_t type)
{
	uint32_t res = 0;

	switch (type) {
	case BUFFER_SIZE_TYPE_MESSAGE:
		if (buf) {
			PSSAMPLEHEADER header = (PSSAMPLEHEADER)buf->buf;
			res += BUF_SIZE(header);
		}
		/* fall through due to need complete data size */
	case BUFFER_SIZE_TYPE_HEADER:
		res += HEADER_SIZE;

	default:
		/* unknown action */
		break;
	}

	return res;
}

static uint32_t
dummy_size_callback(HBUF buf, uint8_t type)
{
	return 8;
}

int
main(int argc, char *argv[])
{
	uint8_t active = 1;

	int listen_port = 15020;

	HPINLIST connection = NULL;
	HPIN pin;
	HPIN input_pin;
	HSAMPLE sample, input_sample, dummy_sample;

	input_sample = buf_alloc(sample_size_callback);
	dummy_sample = buf_alloc(dummy_size_callback);

	if (argc < 3) {
		printf("use: split <host> <port> <listen_port>\n");
		return 0;
	}

	if (argc > 3) {
		sscanf(argv[3], "%i", &listen_port);
		printf("Will listen %i port\n", listen_port);
	}

	connection = pin_list_create();
	pin_listen(connection, listen_port, MAX_EVENTS, BACKLOG);
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
					PSSAMPLEHEADER header = (PSSAMPLEHEADER)sample->buf;
					print_header(header, sample->buf + HEADER_SIZE, sample->size - HEADER_SIZE);

					/* process data here */

					pin_list_write_sample(connection, sample);
					sample->size = 0;
					break;
				}
				case PIN_STATUS_CLOSED:
				{
					printf("connection closed\n");
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
