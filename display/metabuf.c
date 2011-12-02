#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "display.h"
#include "buffer.h"
#include "audio_sample.h"

#define BUFFER_CHANNELS	2
static struct sample_buf sample_buf[BUFFER_CHANNELS];


PSMETABUFER
metabuf_alloc()
{
	PSMETABUFER res = malloc(sizeof(SMETABUFER));
	memset(res, 0, sizeof(SMETABUFER));
	return res;
}

void
buffering_init()
{
	memset(sample_buf, 0, BUFFER_CHANNELS);
}

void
metabuf_free(PSMETABUFER metabuf)
{
	if (metabuf->left)
		buf_free(metabuf->left);

	if (metabuf->right)
		buf_free(metabuf->right);

	if (metabuf->left_fft)
		buf_free(metabuf->left_fft);

	if (metabuf->right_fft)
		buf_free(metabuf->right_fft);

	if (metabuf->sd_log)
		buf_free(metabuf->sd_log);

	if (metabuf->sd_mod)
		buf_free(metabuf->sd_mod);

	free(metabuf);
}

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
#if 0
static int
have_sample()
{
	int res = -1;
	int i;


	return res;
}
#endif
PSMETABUFER
buferize_sample(HBUF sample)
{
	PSSAMPLEHEADER this_h = (PSSAMPLEHEADER)sample->buf;
	PSMETABUFER res = 0;
	int i, diff, have_pair = -1;

	uint8_t this_ch = this_h->channel_no - 1;
	uint8_t other_ch = 1 - this_ch;
/*
	//print_header(this_h, sample->buf + HEADER_SIZE, sample->size - HEADER_SIZE);
	//assert(this_h->channel_no >= 2);
*/
	if (this_h->buf_type == BUF_TYPE_SEQUENTIAL) {
		/* printf("free buf %u %u\n", this_h->channel_no, this_h->number); */
		buf_free(sample);
		return NULL;
	}

	if (sample_buf[this_ch].count >= MAX_BUFFERED) {
		for (i = 0; i < MAX_BUFFERED - 1; i++) {
			sample_buf[this_ch].ptr[i] = sample_buf[this_ch].ptr[i+1];
		}
		sample_buf[this_ch].ptr[MAX_BUFFERED - 1] = sample;
		sample_buf[this_ch].count = MAX_BUFFERED;
		/* printf("[reorderer] bufferize %u. max count reached.\n", this_ch); */
	} else {
		sample_buf[this_ch].ptr[sample_buf[this_ch].count] = sample;
		sample_buf[this_ch].count += 1;
	}

	for (i = 0; i < sample_buf[other_ch].count; i++) {
		PSSAMPLEHEADER other_h = (PSSAMPLEHEADER)((sample_buf[other_ch].ptr[i])->buf);
		if (other_h->number == this_h->number) {
			have_pair = i;
			break;
		}
	}

	if (have_pair != -1) {
		res = metabuf_alloc();
		if (this_ch == CHANNEL_LEFT) {
			res->left_fft = sample_buf[this_ch].ptr[sample_buf[this_ch].count - 1];
			res->right_fft = sample_buf[other_ch].ptr[sample_buf[other_ch].count - 1];
		} else {
			res->right_fft = sample_buf[this_ch].ptr[sample_buf[this_ch].count - 1];
			res->left_fft = sample_buf[other_ch].ptr[sample_buf[other_ch].count - 1];
		}

		if (have_pair > sample_buf[this_ch].count - 1) {
			diff = have_pair - sample_buf[this_ch].count - 1;
			have_pair = sample_buf[this_ch].count;

			shift(sample_buf[this_ch].ptr, have_pair, 0);
			shift(sample_buf[other_ch].ptr, have_pair, diff);
		} else {
			diff = sample_buf[this_ch].count - 1 - have_pair;
			have_pair += 1;

			shift(sample_buf[this_ch].ptr, have_pair, diff);
			shift(sample_buf[other_ch].ptr, have_pair, 0);
		}
		sample_buf[this_ch].count -= have_pair;
		sample_buf[other_ch].count -= have_pair;
	}

	return res;
}

void
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
/*
	//assert(l_head->samples == r_head->samples);
*/
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

