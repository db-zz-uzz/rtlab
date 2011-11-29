#include <stdio.h>
#include <stdlib.h>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <pthread.h>

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

#define MESSAGE_END (void *)-1

typedef struct tagSAUDIORECTHRPARAMS {
	int argc;
	char **argv;
	pa_sample_spec *sample_spec;
	int pipefd;
	int buffer_size;
} SAUDIORECTHRPARAMS, *PSAUDIORECTHRPARAMS;

static void
sample_zero_buffer(HSAMPLE sample)
{
	memset(sample->buf + HEADER_SIZE, 0, sample->alloced_size - HEADER_SIZE);
}

static void *
audio_capture_thr(void *args)
{
	int error;
	PSAUDIORECTHRPARAMS params = (PSAUDIORECTHRPARAMS)args;
	pa_simple *pa_context = NULL;
	uint8_t active = 1;
	uint32_t buffer_size;

	SSAMPLEHEADER  sample_header = {0};
	HSAMPLE sample;

	buffer_size = HEADER_SIZE + params->sample_spec->channels *
					params->buffer_size * pa_sample_size(params->sample_spec);

	sample_header.number = 0;
	sample_header.buf_type = BUF_TYPE_INTERLEAVED;
	sample_header.sample_size = pa_sample_size(params->sample_spec);
	sample_header.samples = params->buffer_size;
	sample_header.channels = params->sample_spec->channels;

	if ( !(pa_context = pa_simple_new(NULL, params->argv[0],
			PA_STREAM_RECORD, NULL, "record",
			params->sample_spec, NULL, NULL, &error)) ) {
		fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
		goto audio_thr_finish;
	}

	printf("[source] audio thr\n");

	while (active) {
		sample = buf_alloc(NULL);
		buf_resize(sample, buffer_size);
		sample->full_size = sample->size = sample->alloced_size;
		memcpy(sample->buf, &sample_header, sizeof(SSAMPLEHEADER));

		sample_zero_buffer(sample);

        if (pa_simple_read(pa_context, sample->buf + HEADER_SIZE,
						sample->alloced_size - HEADER_SIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto audio_thr_finish;
        }
		pa_gettimeofday(&(sample_header.timestamp));
		sample_header.number += 1;

		printf("[audio] read %p\n", sample);

		if ( (error = write(params->pipefd, &sample, sizeof(sample))) != sizeof(sample)) {
			if (error == -1) {
				handle_error("[audio] write()");
			}

			buf_free(sample);
			perror("[audio] ");
			printf("[audio] free buffer");
		}
	}

audio_thr_finish:

	pa_simple_free(pa_context);

	return NULL;
}

int
main(int argc, char *argv[])
{
	uint8_t active = 1;
	int error;
	int pipefd[2];

	/* buffer length in samples. would be multiplied by channels and sample size */
	int buffer_length = 9192;
	int listen_port = 5002;

	pthread_t audio_thr;
/*	pthread_attr_t audio_thr_attr; */

	/* record parameters */
    pa_sample_spec ss = {
    	/* for fft need PA_SAMPLE_FLOAT32LE or PA_SAMPLE_FLOAT32BE */
        .format = PA_SAMPLE_FLOAT32LE, // PA_SAMPLE_S16LE,
        .rate = 11025,
        .channels = 2
    };

	SAUDIORECTHRPARAMS audio_thr_params = {0};
	HPINLIST connection = NULL;
	HPIN pin, pipe_pin;
	HSAMPLE sample, dummy_sample;

	if (argc > 1) {
		sscanf(argv[1], "%i", &listen_port);
		printf("Will listen %i port\n", listen_port);
	}

	connection = pin_list_create(MAX_EVENTS);
	pin_listen(connection, listen_port, BACKLOG, NULL);

	dummy_sample = buf_alloc(dummy_size_callback);

	if (pipe(pipefd) == -1) {
		handle_error("pipe()");
	}
	setnonblocking(pipefd[0]);
	setnonblocking(pipefd[1]);

	pipe_pin = pin_list_add_custom_fd(connection, pipefd[0], PIN_TYPE_CUSTOM);

	/* thread creation */
	audio_thr_params.pipefd = pipefd[1];
	audio_thr_params.buffer_size = buffer_length;
	audio_thr_params.sample_spec = &ss;
	audio_thr_params.argc = argc;
	audio_thr_params.argv = argv;

#if 0
	if ( (error = pthread_attr_init(&audio_thr_attr)) != 0 )
		handle_error_en(error, "pthread_attr_init()");

	if ( (error = pthread_attr_setstacksize(&audio_thr_attr, 5000)) != 0)
		handle_error_en(error, "pthread_attr_setstacksize()");
#endif
	if ( (error = pthread_create(&audio_thr, NULL, audio_capture_thr, &audio_thr_params)) != 0 )
		handle_error_en(error, "pthread_create()");


	/**
	 * :TODO: It's need to improve latency while sending buffers
	 */
	while (active && pin_list_wait(connection, 0) != PIN_ERROR) {

		pin_list_deliver(connection);

		while ( (pin = pin_list_get_next_event(connection, PIN_EVENT_READ)) != NULL ) {

			if (pin == pipe_pin) {
				pin_read_raw(pin, &sample, sizeof(void *));

				/* if pin = pipe_pin, read pointer to buffer,
					write buffer into socket and free it */
				printf("[source] read %p\n", sample);

				print_header((PSSAMPLEHEADER)sample->buf,
							sample->buf + HEADER_SIZE,
							sample->size - HEADER_SIZE);

				pin_list_write_sample(connection, sample, 0);

				buf_free(sample);

				continue;
			}

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
					printf(" partial data. %u / %u\n", dummy_sample->size, dummy_sample->full_size);
					/* do nothing since no data ready */
					break;
				}
				case PIN_STATUS_NO_DATA:
				{
					printf("      no data. %u / %u\n", dummy_sample->size, dummy_sample->full_size);
					/* do nothing since no data ready */
					break;
				}
				default:
				{
					break;
				}
			}

			dummy_sample->size = 0;
		}
	}

//finish:

	/*
		loop until pipe_pin is null.
		and free recieved buffers.
	*/
	pthread_join(audio_thr, NULL);

	pin_list_destroy(connection);

	exit(EXIT_SUCCESS);
}
