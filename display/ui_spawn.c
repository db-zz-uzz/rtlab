#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "display.h"
#include "macros.h"
#include "buffer.h"
#include "audio_sample.h"
#include "pin.h"
#include "timing.h"
#include "glwin/gl_window.h"

#define COLOR(c) (c).r, (c).g, (c).b

#define PADDING 10

typedef struct tagSTRACE {
	void *graph;
	int first;
	int second;
} STRACE, *PSTRACE;

typedef struct tagSUIUPDATERPARAMS {
	PSTHRPARAMS params;
	void *samples;
	void *sdens;
} SUIUPDATERPARAMS, *PSUIUPDATERPARAMS;



static void *
ui_updater_thr(void *args)
{
	PSUIUPDATERPARAMS params = (PSUIUPDATERPARAMS)args;
	uint8_t active = 1;

	HPINLIST pin_list;
	HPIN pin;

	PSMETABUFER meta;
	PSSAMPLEHEADER header;

	TIMING_MEASURE_AREA;

	pin_list = pin_list_create(2);
	pin_list_add_custom_fd(pin_list, params->params->infd, PIN_TYPE_CUSTOM);

	printf("[updater] started\n");

	while (active && pin_list_wait(pin_list, -1) != PIN_ERROR) {

		while ( (pin = pin_list_get_next_event(pin_list, PIN_EVENT_READ)) != NULL ) {

			pin_read_raw(pin, &meta, PTR_SIZE);
			if (meta == MESSAGE_END) {
				active = 0;
				continue;
			}

			if ( meta != NULL ) {
				TIMING_START();
				/* calculate data. send to ui thread */

				PRINT_LOCK(params->params->print_mutex);

				header = (PSSAMPLEHEADER)meta->left_fft->buf;
				print_header(header, meta->left_fft->buf + HEADER_SIZE, meta->left_fft->size - HEADER_SIZE);

				header = (PSSAMPLEHEADER)meta->right_fft->buf;
				print_header(header, meta->right_fft->buf + HEADER_SIZE, meta->right_fft->size - HEADER_SIZE);

				header = (PSSAMPLEHEADER)meta->sd_log->buf;
				print_header(header, meta->sd_log->buf + HEADER_SIZE, meta->sd_log->size - HEADER_SIZE);

				header = (PSSAMPLEHEADER)meta->sd_mod->buf;
				print_header(header, meta->sd_mod->buf + HEADER_SIZE, meta->sd_mod->size - HEADER_SIZE);

				PRINT_UNLOCK(params->params->print_mutex);


				header = (PSSAMPLEHEADER)meta->sd_log->buf;
				glwin_draw_data(GRAPH_SAMPLES,
								(float *)(meta->sd_log->buf + HEADER_SIZE),
								(float *)(meta->sd_mod->buf + HEADER_SIZE),
								header->samples);

				glwin_draw_data_c(GRAPH_FFT,
								(float *)(meta->left_fft->buf + HEADER_SIZE),
								(float *)(meta->right_fft->buf + HEADER_SIZE),
								header->samples);

				glwin_draw_data(GRAPH_SDENS,
								(float *)(meta->sd_log->buf + HEADER_SIZE),
								(float *)(meta->sd_mod->buf + HEADER_SIZE),
								header->samples);

				metabuf_free(meta);

				TIMING_END("updater");
			}

			//buf_free(sample);

		}
	}

	return NULL;
}


void *
spawn_ui_thr(void *args)
{
	PSTHRPARAMS params = (PSTHRPARAMS)args;

	pthread_t ui_updater;
	int s;

	SUIUPDATERPARAMS updparams = {0};
	updparams.params = params;

	printf("[ui] started\n");

	glwin_draw_init();

	glwin_set_color(GRAPH_FFT, LEFT, 0, 0.8, 0.2);
	glwin_set_color(GRAPH_FFT, RIGHT, 0, 0.2, 0.8);

	glwin_set_limits(GRAPH_FFT, LEFT, -1.0, 1.0);
	glwin_set_limits(GRAPH_FFT, RIGHT, -1.0, 1.0);

	glwin_set_color(GRAPH_SAMPLES, LEFT, 0.8, 0.0, 0.2);
	glwin_set_color(GRAPH_SAMPLES, RIGHT, 0.2, 0.8, 0.0);

	glwin_set_limits(GRAPH_SAMPLES, LEFT, -200.0, 200.0);
	glwin_set_limits(GRAPH_SAMPLES, RIGHT, -1.0, 1.0);

	glwin_set_color(GRAPH_SDENS, LEFT, 0.8, 0.4, 0.0);
	glwin_set_color(GRAPH_SDENS, RIGHT, 0.4, 0.2, 0.8);

	glwin_set_limits(GRAPH_SDENS, LEFT, -100.0, 100.0);
	glwin_set_limits(GRAPH_SDENS, RIGHT, -10.0, 10.0);

	if ( (s = pthread_create(&ui_updater, NULL, ui_updater_thr, &updparams)) != 0) {
		handle_error_en(s, "pthread_create()");
	}

	glwin_main(0, NULL);

	pthread_join(ui_updater, NULL);

	return NULL;
}
