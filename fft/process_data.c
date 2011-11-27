#include <fftw3.h>
#include <string.h>

#include "audio_sample.h"
#include "buffer.h"

int
do_process_data(HBUF in_sample, HBUF *out_sample)
{
	fftwf_complex *out_plane;
	fftwf_plan p;
	uint32_t out_plane_size;
	uint32_t in_channel_size;
	uint8_t i;

	PSSAMPLEHEADER out_header = NULL;
	uint8_t *out_data = NULL;

	PSSAMPLEHEADER in_header = (PSSAMPLEHEADER)in_sample->buf;
	uint8_t *in_data = in_sample->buf + HEADER_SIZE;

	in_channel_size = in_header->sample_size * in_header->samples;
	out_plane_size = sizeof(fftwf_complex) * (in_header->samples / 2 + 1);
	out_plane = fftw_malloc(out_plane_size);

	if (*out_sample == NULL) {
		*out_sample = buf_alloc(NULL);
		buf_resize(*out_sample, out_plane_size * in_header->channels + HEADER_SIZE);
		/* create out_sample and set buf size here */
		(*out_sample)->full_size = out_plane_size * in_header->channels + HEADER_SIZE;
	}

	out_header = (PSSAMPLEHEADER)(*out_sample)->buf;
	out_data = (*out_sample)->buf + HEADER_SIZE;

	out_header->sample_size = sizeof(fftwf_complex);
	out_header->samples = in_header->samples / 2 + 1;
	out_header->buf_type = in_header->buf_type;
	out_header->number = in_header->number;
	out_header->timestamp = in_header->timestamp;
	out_header->channels = in_header->channels;

	(*out_sample)->size = HEADER_SIZE;

	p = fftwf_plan_dft_r2c_1d(in_header->samples,
							(float *)in_data,
							out_plane,
							FFTW_FORWARD);

	for(i = 0; i < out_header->channels; i++) {
		// set input buffer
		fftwf_execute_dft_r2c(p, (float *)(in_data + i * in_channel_size), out_plane);
		memcpy(out_data + i * out_plane_size, out_plane, out_plane_size);
		(*out_sample)->size += out_plane_size;
		// copy result
	}

	fftwf_destroy_plan(p);
	fftwf_free(out_plane);

	return 0;
}
