#include <fftw3.h>
#include <string.h>

#include "buffer.h"

int
do_process_data(HBUF in_sample, HBUF *out_sample)
{
	fftwf_complex *out_plane;
	fftwf_plan p;
	uint32_t out_plane_size;
	uint32_t in_channel_size;
	uint8_t i;
/*
	PSSAMPLESTATE in_state = (PSSAMPLESTATE)sample->private_data;
	PSSAMPLESTATE out_state = (PSSAMPLESTATE)out_sample->private_data;

	//printf("data processed\n");
	state->flags |= STATE_FLAG_DATA_PROCESSED;

	in_channel_size = in_sample->header->sample_size * in_sample->header->samples;
	out_plane_size = sizeof(fftwf_complex) * (sample->header->samples / 2 + 1);
	out_plane = fftw_malloc(out_plane_size);

	out_sample->header->sample_size = sizeof(fftw_complex);
	out_sample->header->samples = in_sample->header->samples / 2 + 1;

	p = fftwf_plan_dft_r2c_1d(in_sample->header->samples,
							(float *)in_sample->buf,
							out_plane,
							FFTW_FORWARD);

	for(i = 0; i < out_sample->header->channels; i++) {
		// set input buffer
		fftwf_execute_dft_r2c(p, (float *)(sample->buf + i * in_channel_size), out_plane);
		memcpy(new_head + HEADER_SIZE + i * out_plane_size, out_plane, out_plane_size);
		// copy result
	}

	free(state->buf);
	state->buf =

	fftwf_destroy(p);
	fftwf_free(out_plane);
*/
	return 0;
}
