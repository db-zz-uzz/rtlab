#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include "pin.h"
#include "buffer.h"
#include "display.h"
#include "audio_sample.h"
#include "macros.h"

#define MAX_BUFFERED	2

struct sample_buf {
	HBUF ptr[MAX_BUFFERED];
	uint8_t count;
};

static PSMETABUFER
buferize_sample(struct sample_buf *buf, HBUF sample)
{
	PSSAMPLEHEADER this_h = (PSSAMPLEHEADER)sample->buf;
	PSMETABUFER res = 0;
	int i, diff, have_pair = -1;

	uint8_t this_ch = this_h->channel_no - 1;
	uint8_t other_ch = 1 - this_ch;

	printf("[reorderer] bufferize. this %u, other %u\n", this_ch, other_ch);
	//assert(this_h->channel_no >= 2);

	if (buf[this_ch].count >= MAX_BUFFERED) {
		for (i = 0; i < MAX_BUFFERED - 1; i++) {
			buf[this_ch].ptr[i] = buf[this_ch].ptr[i+1];
		}
		buf[this_ch].ptr[MAX_BUFFERED - 1] = sample;
		buf[this_ch].count = MAX_BUFFERED;
		printf("[reorderer] bufferize %u. max count reached.\n", this_ch);
	} else {
		buf[this_ch].ptr[buf[this_ch].count] = sample;
		buf[this_ch].count += 1;
	}
	printf("[reorderer] bufferize %u-%u: %p (%u)\n",
			this_ch, buf[other_ch].count, sample, buf[this_ch].count);

	for (i = 0; i < buf[other_ch].count; i++) {
		PSSAMPLEHEADER other_h = (PSSAMPLEHEADER)((buf[other_ch].ptr[i])->buf);
		printf("[reorderer] search %i %u ? %u\n", i,
				other_h->number,
				this_h->number );
		if (other_h->number == this_h->number) {
			have_pair = i;
			break;
		}
	}

	if (have_pair != -1) {
		printf("have pair\n");
		res = metabuf_alloc();
		if (this_ch == CHANNEL_LEFT) {
			res->left = buf[this_ch].ptr[buf[this_ch].count - 1];
			res->right = buf[other_ch].ptr[buf[other_ch].count - 1];
		} else {
			res->right = buf[this_ch].ptr[buf[this_ch].count - 1];
			res->left = buf[other_ch].ptr[buf[other_ch].count - 1];
		}

		if (have_pair > buf[this_ch].count - 1) {
			diff = have_pair - buf[this_ch].count - 1;
			have_pair = buf[this_ch].count;

			for (i = 0; i < have_pair; i++) {
				buf[this_ch].ptr[i] = buf[this_ch].ptr[i + have_pair];
			}
			for (i = 0; i < have_pair + diff; i++) {
				buf[other_ch].ptr[i] = buf[other_ch].ptr[i + have_pair];
			}
		} else {
			diff = buf[this_ch].count - 1 - have_pair;
			have_pair += 1;
			for (i = 0; i < have_pair + diff; i++) {
				buf[this_ch].ptr[i] = buf[this_ch].ptr[i + have_pair];
			}
			for (i = 0; i < have_pair; i++) {
				buf[other_ch].ptr[i] = buf[other_ch].ptr[i + have_pair];
			}
		}
		buf[this_ch].count -= have_pair;
		buf[other_ch].count -= have_pair;
	}

	return res;
}

void *
reorderer_thr(void *args)
{
	PSTHRPARAMS params = (PSTHRPARAMS)args;
	struct sample_buf samples[2];

	HPINLIST pin_list;
	HPIN input_pin, pin;
	HBUF sample;
	PSSAMPLEHEADER header;
	PSMETABUFER metabuf;

	/* int outfd = params->outfd; */
	uint8_t active = 1;

	safe_print(params->print_mutex, "[reorderer] started\n");

	memset(samples, 0, sizeof(samples));

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
/*
			header = (PSSAMPLEHEADER)sample->buf;
*/
			/* header->buf_type = BUF_TYPE_INTERLEAVED; */
/*
			PRINT_LOCK(params->print_mutex);
			print_header(header, sample->buf + HEADER_SIZE, sample->size - HEADER_SIZE);
			PRINT_UNLOCK(params->print_mutex);
*/
			if ( (metabuf = buferize_sample(samples, sample)) != NULL ) {
				/* calculate data. send to ui thread */
				PRINT_LOCK(params->print_mutex);

				header = (PSSAMPLEHEADER)metabuf->left->buf;
				print_header(header, metabuf->left->buf + HEADER_SIZE, metabuf->left->size - HEADER_SIZE);

				header = (PSSAMPLEHEADER)metabuf->right->buf;
				print_header(header, metabuf->right->buf + HEADER_SIZE, metabuf->right->size - HEADER_SIZE);

				PRINT_UNLOCK(params->print_mutex);

				metabuf_free(metabuf);
			}

			//buf_free(sample);

		}

	}

	/* free buffered samples */

	return NULL;
}
