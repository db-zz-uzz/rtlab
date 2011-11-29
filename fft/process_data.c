#include <fftw3.h>
#include <string.h>

#include "audio_sample.h"
#include "buffer.h"

#if 0
static void
center_data_4b(void *buf, uint32_t siz)
{
	fftwf_complex *samples = buf;
	float center = 0;
	float min = samples[0][0];
	float max = samples[0][0];
	uint32_t i = 0;

	for (i = 0; i < siz; i++) {
		center += samples[i][0];

		if (samples[i][0] < min)
			min = samples[i][0];

		if (samples[i][0] > max)
			max = samples[i][0];
	}
	center /= siz;
/*
	for (i = 0; i < siz; i++) {

	}
*/
	printf("%u samples, average %.4f, min %.4f, max %.4f\n", siz, center, min, max);
}
#endif

static void
data_f2c_4b(void *dst, void *src, uint32_t siz)
{
	fftwf_complex *d = dst;
	float *s = src;

	while (siz--) {
		(*d++)[0] = *s++;
	}
}

int
do_process_data(HBUF in_sample, HBUF *out_sample, uint32_t user_data)
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

	memcpy(out_header, in_header, HEADER_SIZE);
	out_header->sample_size = sizeof(fftwf_complex);
	out_header->buf_type = BUF_TYPE_FFTED;

	(*out_sample)->size = HEADER_SIZE;

	p = fftwf_plan_dft_1d(in_header->samples, in_plane, out_plane, FFTW_FORWARD, FFTW_ESTIMATE);

	for(i = 0; i < out_header->channels; i++) {
		// set input buffer
		data_f2c_4b(in_plane,
				in_data + i * in_header->sample_size * in_header->samples,
				in_header->samples);

		/* data centering doesn't necessary in case of real data */
	/*
		center_data_4b(in_plane, in_header->samples);
	*/
		fftwf_execute(p);
		memcpy(out_data + i * plane_size, out_plane, plane_size);
		(*out_sample)->size += plane_size;
		// copy result
	}

	fftwf_destroy_plan(p);
	fftwf_free(out_plane);

	return 0;
}
