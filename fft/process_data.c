#include <fftw3.h>
#include <string.h>

#include "audio_sample.h"
#include "buffer.h"

#define FFT_R2C

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
#if 0
#ifndef FFT_R2C
static void
data_f2c_4b(void *dst, void *src, uint32_t siz)
{
	fftwf_complex *d = dst;
	float *s = src;

	while (siz--) {
		(*d++)[0] = *s++;
	}
}
#endif
#endif
int
do_process_data(HBUF in_sample, HBUF *out_sample, uint32_t user_data)
{
#ifdef FFT_R2C
	float *in_plane;
#else
	fftwf_complex *in_plane;
#endif
	fftwf_complex *out_plane;
	fftwf_plan p;
	uint32_t plane_size;
#ifdef FFT_R2C
	uint32_t in_plane_size;
#endif
	uint32_t in_channel_size;
	uint8_t i;

	PSSAMPLEHEADER out_header = NULL;
	uint8_t *out_data = NULL;

	PSSAMPLEHEADER in_header = (PSSAMPLEHEADER)in_sample->buf;
	uint8_t *in_data = in_sample->buf + HEADER_SIZE;

	in_channel_size = in_header->sample_size * in_header->samples;
#ifdef FFT_R2C
	in_plane_size = sizeof(float) * in_header->samples;
	plane_size = sizeof(fftwf_complex) * (in_header->samples / 2 + 1);
	in_plane = fftw_malloc(in_plane_size);
#else
	plane_size = sizeof(fftwf_complex) * in_header->samples;
	in_plane = fftw_malloc(plane_size);
#endif
	out_plane = fftw_malloc(plane_size);

	if (*out_sample == NULL) {
		*out_sample = buf_alloc(NULL);
		buf_resize(*out_sample, plane_size * in_header->channels + HEADER_SIZE);
		/* create out_sample and set buf size here */
		(*out_sample)->full_size = plane_size * in_header->channels + HEADER_SIZE;
	}

	out_header = (PSSAMPLEHEADER)(*out_sample)->buf;
	out_data = (*out_sample)->buf + HEADER_SIZE;

	memcpy(out_header, in_header, HEADER_SIZE);
#ifdef FFT_R2C
	out_header->samples = in_header->samples / 2 + 1;
	printf("fft in-plane %u, samples %u",
			in_plane_size, out_header->samples);
#endif
	out_header->sample_size = sizeof(fftwf_complex);
	out_header->buf_type = BUF_TYPE_FFTED;

	(*out_sample)->size = HEADER_SIZE;

#ifdef FFT_R2C
	p = fftwf_plan_dft_r2c_1d(in_header->samples, in_plane, out_plane, FFTW_FORWARD);
#else
	p = fftwf_plan_dft_1d(in_header->samples, in_plane, out_plane, FFTW_FORWARD, FFTW_ESTIMATE);
#endif

	for(i = 0; i < out_header->channels; i++) {
		// set input buffer
#ifdef FFT_R2C
		memcpy(in_plane, in_data, in_plane_size);
#else
		data_f2c_4b(in_plane,
				in_data + i * in_header->sample_size * in_header->samples,
				in_header->samples);
#endif
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
	fftwf_free(in_plane);

	return 0;
}
