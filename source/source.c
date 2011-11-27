#include <stdio.h>
#include <stdlib.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "audio_sample.h"
#include "buffer.h"
#include "macros.h"
#include "pin.h"

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <pulse/timeval.h>

#define MAX_EVENTS 5
#define BACKLOG 50

static void
sample_zero_buffer(HSAMPLE sample)
{
	memset(sample->buf + HEADER_SIZE, 0, sample->alloced_size - HEADER_SIZE);
}

int
main(int argc, char *argv[])
{
	uint8_t active = 1;
	int error;

	/* buffer length in samples. would be multiplied by channels and sample size */
	int buffer_length = 11025;
	int listen_port = 5002;

	/* record parameters */
    static const pa_sample_spec ss = {
    	/* for fft need PA_SAMPLE_FLOAT32LE or PA_SAMPLE_FLOAT32BE */
        .format = PA_SAMPLE_FLOAT32LE, // PA_SAMPLE_S16LE,
        .rate = 11025,
        .channels = 2
    };

	HPINLIST connection = NULL;
	HPIN pin;
	HSAMPLE sample, dummy_sample;

	PSSAMPLEHEADER  sample_header;

	pa_simple *pa_context = NULL;

	if (argc > 1) {
		sscanf(argv[1], "%i", &listen_port);
		printf("Will listen %i port\n", listen_port);
	}

	if ( !(pa_context = pa_simple_new(NULL, argv[0],
			PA_STREAM_RECORD, NULL, "record",
			&ss, NULL, NULL, &error)) ) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		goto finish;
	}

	connection = pin_list_create(MAX_EVENTS);
	pin_listen(connection, listen_port, BACKLOG);

	sample = buf_alloc(NULL);
	buf_resize(sample, ss.channels * buffer_length * pa_sample_size(&ss) + HEADER_SIZE);
	sample->full_size = sample->size = sample->alloced_size;
	sample_header = (PSSAMPLEHEADER)sample->buf;

	sample_header->number = 0;
	sample_header->buf_type = BUF_TYPE_INTERLEAVED;
	sample_header->sample_size = pa_sample_size(&ss);
	sample_header->samples = buffer_length;
	sample_header->channels = ss.channels;

	dummy_sample = buf_alloc(dummy_size_callback);

	/**
	 * :TODO: It's need to improve latency while sending buffers
	 */
	while (active && pin_list_wait(connection, 0) != PIN_ERROR) {
		sample_zero_buffer(sample);

		pin_list_deliver(connection);

        if (pa_simple_read(pa_context, sample->buf + HEADER_SIZE,
						sample->alloced_size - HEADER_SIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }
		pa_gettimeofday(&(sample_header->timestamp));
		sample_header->number += 1;

		while ( (pin = pin_list_get_next_event(connection, PIN_EVENT_READ)) != NULL ) {

			switch (pin_read_sample(pin, dummy_sample)) {
				case PIN_STATUS_READY:
				{
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

			sample->size = 0;
		}

		print_header((PSSAMPLEHEADER)sample->buf,
					sample->buf + HEADER_SIZE,
					sample->size - HEADER_SIZE);

		pin_list_write_sample(connection, sample);
	}

finish:

	pin_list_destroy(connection);

	pa_simple_free(pa_context);

	exit(EXIT_SUCCESS);
}
