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

#define MAX_EVENTS 5
#define BACKLOG 50

#if 0
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
#endif

int
main(int argc, char *argv[])
{
	uint8_t active = 1;

	int listen_port = 15020;

	HPINLIST connection = NULL;
	HPIN pin;
	HPIN input_pin;
	HSAMPLE sample, input_sample, dummy_sample, processed_sample = NULL;

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
					do_process_data(sample, &processed_sample);

					print_header((PSSAMPLEHEADER)processed_sample->buf,
								processed_sample->buf + HEADER_SIZE,
								processed_sample->size - HEADER_SIZE);

					pin_list_write_sample(connection, sample);
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
