#include <string.h>

#include "audio_sample.h"
#include "buffer.h"

static int
do_split_2ch2bytes(void *dst, void *src, uint32_t siz)
{
	return 0;
}

/* siz - samples count. */
static int
do_split_2ch4bytes(void *dst, void *src, uint32_t siz)
{
	uint32_t *d = dst;
	uint32_t *s = src;
	uint32_t i;

	for (i = 0; i < siz; i++) {
		d[i] = s[i * 2];
		d[i + siz] = s[i * 2 + 1];
	}

	return 0;
}

int
do_process_data(HBUF in_sample, HBUF *out_sample)
{
	int (*proc_func)(void *, void *, uint32_t) = NULL;

	PSSAMPLEHEADER out_header = NULL;
	uint8_t *out_data = NULL;

	PSSAMPLEHEADER in_header = (PSSAMPLEHEADER)in_sample->buf;
	uint8_t *in_data = in_sample->buf + HEADER_SIZE;

	if (*out_sample == NULL) {
		*out_sample = buf_alloc(NULL);
		buf_resize(*out_sample, in_sample->full_size);
		/* create out_sample and set buf size here */
		(*out_sample)->full_size = in_sample->full_size;
		(*out_sample)->size = in_sample->size;
	}

	out_header = (PSSAMPLEHEADER)(*out_sample)->buf;
	out_data = (*out_sample)->buf + HEADER_SIZE;

	out_header->sample_size = in_header->sample_size;
	out_header->samples = in_header->samples;
	out_header->buf_type = BUF_TYPE_SEQUENTIAL;
	out_header->number = in_header->number;
	out_header->timestamp = in_header->timestamp;
	out_header->channels = in_header->channels;

	/* nothing to do since data already sequential */
	if (in_header->buf_type == BUF_TYPE_SEQUENTIAL || in_header->channels == 1) {
		memcpy(out_data, in_data, in_sample->size);
	} else {

		switch (in_header->sample_size) {
			case 2: {
				proc_func = do_split_2ch2bytes;
				break;
			}
			case 4: {
				proc_func = do_split_2ch4bytes;
				break;
			}
			default: {
				memcpy(out_data, in_data, in_sample->size);
				break;
			}
		}
	}

	if (proc_func)
		proc_func(out_data, in_data, in_header->samples);

	return 0;
}
