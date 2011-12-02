#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "display.h"
#include "buffer.h"
#include "audio_sample.h"

#define MAX_BUFFERED	10
#define BUFFER_CHANNELS	4

struct sample_buf {
	HBUF ptr[MAX_BUFFERED];
	uint8_t count;
};

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

static void
print_sample_buf(struct sample_buf *buf, char* prefix)
{

	int i;
	printf("%s # meta %p. count %u ", prefix, buf, buf->count);
	for (i = 0; i < buf->count; i++) {
		if (i < buf->count) {
			printf("<%u> ", ((PSSAMPLEHEADER)buf->ptr[i]->buf)->number);
		} else {
			printf("%p ", buf->ptr[i]);
		}
	}
	printf("\n");

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
shift(struct sample_buf *buf, int shift)
{
	int i;
	HBUF save_ptr = NULL;
	//printf("[shift] shift %i, of %u\n", shift, buf->count);
	//print_sample_buf(buf, "shift 1 ");
	for (i = 0; i < buf->count - shift + 1; i++) {
		if (i < shift - 1) {
			save_ptr = buf->ptr[i];
			/*
			printf("[reorderer] !! Drop sample # %u\n",
					((PSSAMPLEHEADER)(buf->ptr[i]->buf))->number);
			buf_free(buf->ptr[i]);
			*/
		}
/*
		printf("[%i].%u = [%i].%u\n",
				i, 0,
				i + 1 + shift, 0);
*/
		buf->ptr[i] = buf->ptr[i + 1 + shift];

		if (save_ptr) {
			buf_free(save_ptr);
			save_ptr = NULL;
		}
	}

	buf->count -= shift + 1;
	//print_sample_buf(buf, "shift 2 ");
}

static int
meta_channel(PSSAMPLEHEADER head)
{
	int res = head->channel_no - 1;

	if (res >= CHANNELS_COUNT) {
		res = CHANNELS_COUNT - 1;
	}

	if (head->buf_type == BUF_TYPE_SEQUENTIAL) {
		res += CHANNELS_COUNT;
	}

	return res;
}

static int
have_sample(struct sample_buf *buf, uint32_t sample_no)
{
	int res = -1;
	int i;
	PSSAMPLEHEADER header;

	//print_sample_buf(buf, "have_sample ");
	for (i = 0; i < buf->count; i++) {
		header = (PSSAMPLEHEADER)buf->ptr[i]->buf;
		if (header->number == sample_no) {
			res = i;
			break;
		}
	}
	return res;
}

static void
push_buffered(HBUF sample, int channel)
{
	if (sample_buf[channel].count >= MAX_BUFFERED) {
		int i;

		for (i = 0; i < sample_buf[channel].count; i++) {
			sample_buf[channel].ptr[i] = sample_buf[channel].ptr[i + 1];
		}
		sample_buf[channel].ptr[MAX_BUFFERED - 1] = sample;
		sample_buf[channel].count = MAX_BUFFERED;
	} else {
		sample_buf[channel].ptr[sample_buf[channel].count++] = sample;
	}
}

PSMETABUFER
buferize_sample(HBUF sample)
{
	static int first_send = 0;

	PSSAMPLEHEADER header = (PSSAMPLEHEADER)sample->buf;
	PSMETABUFER res = NULL;
	int i;

	uint8_t this_ch = meta_channel(header);

	int having[BUFFER_CHANNELS];
	int have_set = 1;

	if (first_send > 10) {
		buf_free(sample);
		return NULL;
	}

	//print_header(header, sample->buf + HEADER_SIZE, sample->size - HEADER_SIZE);

	for (i = 0; i < BUFFER_CHANNELS; i++) {
		having[i] = -1;
	}

	//assert(this_h->channel_no >= 2);

	/* push sample into buffer */
	//printf("bufferize. actial channel %i\n", this_ch);
	//print_sample_buf(&sample_buf[this_ch], "push_buffered 1 ");
	push_buffered(sample, this_ch);
	//print_sample_buf(&sample_buf[this_ch], "push_buffered 2 ");

	/* check if we have complete set: left + right + left_fft + right_fft */
	for (i = 0; i < BUFFER_CHANNELS; i++) {
		//printf("having[%i] %i\n", i, have_sample(&sample_buf[i], header->number));
		if ( (having[i] = have_sample(&sample_buf[i], header->number)) == -1 ) {
			have_set = 0;
			break;
		}
	}

	/* pop complete set and return it, if we does */
	if (have_set) {
		//printf("!!! have sample !!!\n");
		//first_send += 1;

		res = metabuf_alloc();
		/* set struct as array of HBUF ptrs.
		 * so we can loo through it */
		HBUF *metabuf = (HBUF *)res;

		for (i = 0; i < BUFFER_CHANNELS; i++) {
			metabuf[i] = sample_buf[i].ptr[having[i]];
			shift(&sample_buf[i], having[i]);
		}
	}


#if 0
	for (i = 0; i < BUFFER_CHANNELS; i++) {
		print_sample_buf(&sample_buf[i], "bufferize  ");
	}
#endif
	//printf("----------------------------------------\n");
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
/*
		printf("1. %f\t%u\t%u\n", coef, samples, r_head->samplerate);
		printf("2. %.6f\t%.6f\t%.6f\n", coef, sxy[i*2], sxy[i*2+1]);
*/
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

