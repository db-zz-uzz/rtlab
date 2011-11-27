#include <fftw3.h>
#include <string.h>

#include "audio_sample.h"
#include "buffer.h"

static void
data_f2c(void *dst, void *src, uint32_t siz)
{
	fftwf_complex *d = dst;
	float *s = src;

	while (siz--) {
		(*d++)[0] = *s++;
	}
}

int
do_process_data(HBUF in_sample, HBUF *out_sample)
{
	fftwf_complex *out_plane, *in_plane;
	fftwf_plan p;
	uint32_t plane_size;
	uint32_t in_channel_size;
	uint8_t i;

	PSSAMPLEHEADER out_header = NULL;
	uint8_t *out_data = NULL;

	PSSAMPLEHEADER in_header = (PSSAMPLEHEADER)in_sample->buf;
	uint8_t *in_data = in_sample->buf + HEADER_SIZE;

	in_channel_size = in_header->sample_size * in_header->samples;
	plane_size = sizeof(fftwf_complex) * in_header->samples;
	out_plane = fftw_malloc(plane_size);
	in_plane = fftw_malloc(plane_size);

	if (*out_sample == NULL) {
		*out_sample = buf_alloc(NULL);
		buf_resize(*out_sample, plane_size * in_header->channels + HEADER_SIZE);
		/* create out_sample and set buf size here */
		(*out_sample)->full_size = plane_size * in_header->channels + HEADER_SIZE;
	}

	out_header = (PSSAMPLEHEADER)(*out_sample)->buf;
	out_data = (*out_sample)->buf + HEADER_SIZE;

	out_header->sample_size = sizeof(fftwf_complex);
	out_header->samples = in_header->samples;
	out_header->buf_type = in_header->buf_type;
	out_header->number = in_header->number;
	out_header->timestamp = in_header->timestamp;
	out_header->channels = in_header->channels;

	(*out_sample)->size = HEADER_SIZE;

	p = fftwf_plan_dft_1d(in_header->samples, in_plane, out_plane, FFTW_FORWARD, FFTW_ESTIMATE);

	for(i = 0; i < out_header->channels; i++) {
		// set input buffer
		data_f2c(in_plane,
				in_data + i * in_header->sample_size * in_header->samples,
				in_header->samples);
		fftwf_execute(p);
		memcpy(out_data + i * plane_size, out_plane, plane_size);
		(*out_sample)->size += plane_size;
		// copy result
	}

	fftwf_destroy_plan(p);
	fftwf_free(out_plane);

	return 0;
}
