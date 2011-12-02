#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include "pin.h"
#include "buffer.h"
#include "display.h"
#include "audio_sample.h"
#include "macros.h"
#include "timing.h"

void *
reorderer_thr(void *args)
{
	PSTHRPARAMS params = (PSTHRPARAMS)args;

	HPINLIST pin_list;
	HPIN input_pin, pin;
	HBUF sample;
	PSMETABUFER metabuf;

	TIMING_MEASURE_AREA;

	/* int outfd = params->outfd; */
	uint8_t active = 1;
	int s;

	safe_print(params->print_mutex, "[reorderer] started\n");

	buffering_init();

	pin_list = pin_list_create(2);
	input_pin = pin_list_add_custom_fd(pin_list, params->infd, PIN_TYPE_CUSTOM);

	while (active && pin_list_wait(pin_list, -1) != PIN_ERROR) {

		while ( (pin = pin_list_get_next_event(pin_list, PIN_EVENT_READ)) != NULL ) {

			pin_read_raw(pin, &sample, PTR_SIZE);
			if (sample == MESSAGE_END) {
				active = 0;
				safe_print(params->print_mutex, "[reorderer] MESSAGE_END recieved\n");
				continue;
			}

			if ( (metabuf = buferize_sample(sample)) != NULL ) {
				TIMING_START();
				/* calculate data. send to ui thread */
				calc_data(metabuf);
#if 0
				PRINT_LOCK(params->print_mutex);

				header = (PSSAMPLEHEADER)metabuf->left->buf;
				print_header(header, metabuf->left->buf + HEADER_SIZE, metabuf->left->size - HEADER_SIZE);

				header = (PSSAMPLEHEADER)metabuf->right->buf;
				print_header(header, metabuf->right->buf + HEADER_SIZE, metabuf->right->size - HEADER_SIZE);

				header = (PSSAMPLEHEADER)metabuf->sd_log->buf;
				print_header(header, metabuf->sd_log->buf + HEADER_SIZE, metabuf->sd_log->size - HEADER_SIZE);

				header = (PSSAMPLEHEADER)metabuf->sd_mod->buf;
				print_header(header, metabuf->sd_mod->buf + HEADER_SIZE, metabuf->sd_mod->size - HEADER_SIZE);

				/* printf("[reorderer] send ptr %p\n", metabuf); */
				PRINT_UNLOCK(params->print_mutex);
#endif
				if ( (s = write(params->outfd, &metabuf, PTR_SIZE)) != PTR_SIZE ) {
					if (s == -1) {
						handle_error("write()");
					}

					printf("[reorderer] overrun. free buf.\n");
					metabuf_free(metabuf);
				}


				TIMING_END("reorderer");
			}

			//buf_free(sample);

		}

	}

	/* free buffered samples */

	return NULL;
}
