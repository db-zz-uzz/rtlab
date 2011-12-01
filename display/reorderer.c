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

#define MAX_BUFFERED	3

struct sample_buf {
	HBUF ptr[MAX_BUFFERED];
	uint8_t count;
};

static void
shift(HBUF *buf, int shift, int diff)
{
	int i;
	for (i = 0; i < shift + diff; i++) {
		if (i < shift + diff - 1)
			buf_free(buf[i]);
		buf[i] = buf[i + shift];
	}
}

static PSMETABUFER
buferize_sample(struct sample_buf *buf, HBUF sample)
{
	PSSAMPLEHEADER this_h = (PSSAMPLEHEADER)sample->buf;
	PSMETABUFER res = 0;
	int i, diff, have_pair = -1;

	uint8_t this_ch = this_h->channel_no - 1;
	uint8_t other_ch = 1 - this_ch;

	//print_header(this_h, sample->buf + HEADER_SIZE, sample->size - HEADER_SIZE);
	//assert(this_h->channel_no >= 2);

	if (buf[this_ch].count >= MAX_BUFFERED) {
		for (i = 0; i < MAX_BUFFERED - 1; i++) {
			buf[this_ch].ptr[i] = buf[this_ch].ptr[i+1];
		}
		buf[this_ch].ptr[MAX_BUFFERED - 1] = sample;
		buf[this_ch].count = MAX_BUFFERED;
		//printf("[reorderer] bufferize %u. max count reached.\n", this_ch);
	} else {
		buf[this_ch].ptr[buf[this_ch].count] = sample;
		buf[this_ch].count += 1;
	}

	for (i = 0; i < buf[other_ch].count; i++) {
		PSSAMPLEHEADER other_h = (PSSAMPLEHEADER)((buf[other_ch].ptr[i])->buf);
		if (other_h->number == this_h->number) {
			have_pair = i;
			break;
		}
	}

	if (have_pair != -1) {
		res = metabuf_alloc();
		if (this_ch == CHANNEL_LEFT) {
			res->left_fft = buf[this_ch].ptr[buf[this_ch].count - 1];
			res->right_fft = buf[other_ch].ptr[buf[other_ch].count - 1];
		} else {
			res->right_fft = buf[this_ch].ptr[buf[this_ch].count - 1];
			res->left_fft = buf[other_ch].ptr[buf[other_ch].count - 1];
		}

		if (have_pair > buf[this_ch].count - 1) {
			diff = have_pair - buf[this_ch].count - 1;
			have_pair = buf[this_ch].count;

			shift(buf[this_ch].ptr, have_pair, 0);
			shift(buf[other_ch].ptr, have_pair, diff);
		} else {
			diff = buf[this_ch].count - 1 - have_pair;
			have_pair += 1;

			shift(buf[this_ch].ptr, have_pair, diff);
			shift(buf[other_ch].ptr, have_pair, 0);
		}
		buf[this_ch].count -= have_pair;
		buf[other_ch].count -= have_pair;
	}

	return res;
}

static void
calc_data(PSMETABUFER metabuf)
{
	PSSAMPLEHEADER l_head, r_head, sd_log_head, sd_mod_head;
	float *l_data, *r_data;
	float *sd_log_data, *sd_mod_data;
	float *sxy;
	float coef;
	int i, samples;

	l_head = (PSSAMPLEHEADER)metabuf->left_fft->buf;
	r_head = (PSSAMPLEHEADER)metabuf->right_fft->buf;

	samples = l_head->samples;
	sxy = malloc(sizeof(float) * 2 * samples);

	//assert(l_head->samples == r_head->samples);

	coef = (float)1 / (samples * r_head->samplerate);

	l_data = (float *)metabuf->left_fft->buf + HEADER_SIZE;
	r_data = (float *)metabuf->right_fft->buf + HEADER_SIZE;

	for (i = 0; i < samples; i++) {
		sxy[i * 2] = coef * (l_data[i * 2] * r_data[i * 2] + l_data[i * 2 + 1] * r_data[i * 2 + 1]);
		sxy[i * 2 + 1] = coef * (-l_data[i * 2] * r_data[i * 2 + 1] + l_data[i * 2 + 1] * r_data[i * 2]);
	}

	metabuf->sd_log = buf_alloc(NULL);
	metabuf->sd_log->size = metabuf->sd_log->full_size = HEADER_SIZE + sizeof(float) * samples;
	buf_resize(metabuf->sd_log, metabuf->sd_log->size);
	sd_log_head = (PSSAMPLEHEADER)metabuf->sd_log->buf;
	sd_log_data = (float *)(metabuf->sd_log->buf + HEADER_SIZE);
	memcpy(sd_log_head, l_head, HEADER_SIZE);
	sd_log_head->buf_type = BUF_TYPE_UNKNOWN;
	sd_log_head->sample_size = sizeof(float);
	sd_log_head->channel_no = 0;
	sd_log_head->channels = 0;

	metabuf->sd_mod = buf_alloc(NULL);
	metabuf->sd_mod->size = metabuf->sd_mod->full_size = HEADER_SIZE + sizeof(float) * samples;
	buf_resize(metabuf->sd_mod, metabuf->sd_mod->size);
	sd_mod_head = (PSSAMPLEHEADER)metabuf->sd_mod->buf;
	sd_mod_data = (float *)(metabuf->sd_mod->buf + HEADER_SIZE);
	memcpy(sd_mod_head, sd_log_head, HEADER_SIZE);

	for (i = 0; i < samples; i++) {
		coef = (float)sqrt(sxy[i*2] * sxy[i*2] + sxy[i*2+1] + sxy[i*2+1]);
		sd_log_data[i] = (coef > 0) ? (float) 10 * log(coef) : 0;

		sd_mod_data[i] = (sxy[i * 2] != 0) ? (float)atan2(sxy[i * 2 + 1], sxy[i * 2]) : 0;
	}

	free(sxy);
}

void *
reorderer_thr(void *args)
{
	PSTHRPARAMS params = (PSTHRPARAMS)args;
	struct sample_buf samples[2];

	HPINLIST pin_list;
	HPIN input_pin, pin;
	HBUF sample;
	//PSSAMPLEHEADER header;
	PSMETABUFER metabuf;

	TIMING_MEASURE_AREA;

	/* int outfd = params->outfd; */
	uint8_t active = 1;
	int s;

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

			if ( (metabuf = buferize_sample(samples, sample)) != NULL ) {
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
