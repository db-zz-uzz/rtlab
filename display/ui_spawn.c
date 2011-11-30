#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <cairo.h>

#include "display.h"
#include "macros.h"
#include "buffer.h"
#include "audio_sample.h"
#include "pin.h"
#include "timing.h"
#include "graph/graph.h"

#define COLOR(c) (c).r, (c).g, (c).b

#define PADDING 10

typedef struct tagSTRACE {
	GtkWidget *graph;
	gint first;
	gint second;
} STRACE, *PSTRACE;

typedef struct tagSUIUPDATERPARAMS {
	PSTHRPARAMS params;
	GtkWidget *samples;
	GtkWidget *sdens;
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

	printf("[updater] started %i %i\n", sizeof(float), sizeof(gfloat));

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

				header = (PSSAMPLEHEADER)meta->left->buf;
				print_header(header, meta->left->buf + HEADER_SIZE, meta->left->size - HEADER_SIZE);

				header = (PSSAMPLEHEADER)meta->right->buf;
				print_header(header, meta->right->buf + HEADER_SIZE, meta->right->size - HEADER_SIZE);

				header = (PSSAMPLEHEADER)meta->sd_log->buf;
				print_header(header, meta->sd_log->buf + HEADER_SIZE, meta->sd_log->size - HEADER_SIZE);

				header = (PSSAMPLEHEADER)meta->sd_mod->buf;
				print_header(header, meta->sd_mod->buf + HEADER_SIZE, meta->sd_mod->size - HEADER_SIZE);

				PRINT_UNLOCK(params->params->print_mutex);

				header = (PSSAMPLEHEADER)meta->sd_mod->buf;
				gtk_graph_draw_data(GTK_GRAPH(params->sdens),
										(float *)(meta->sd_mod->buf + HEADER_SIZE),
										(float *)(meta->sd_log->buf + HEADER_SIZE),
										header->samples, header->number);

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
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *sdcanv;
	GtkWidget *samplecanv;

	pthread_t ui_updater;
	int s;

	SUIUPDATERPARAMS updparams = {0};
	updparams.params = params;

	printf("[ui] started\n");

	g_thread_init(NULL);
	gtk_init(&params->argc, &params->argv);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "RT Lab");
	g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(exit), NULL);

	vbox = gtk_vbox_new(TRUE, 2);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	sdcanv = gtk_graph_new("Spectral density", "mod[Sxy]", "10Lg[Sxy]");
	gtk_box_pack_end(GTK_BOX(vbox), sdcanv, TRUE, TRUE, 5);

	samplecanv = gtk_graph_new("Samples source", "Left", "Right");
	gtk_box_pack_end(GTK_BOX(vbox), samplecanv, TRUE, TRUE, 5);

	gtk_window_set_default_size(GTK_WINDOW(window), 800, 480);
	gtk_widget_show_all(window);

	updparams.samples = samplecanv;
	updparams.sdens = sdcanv;

	if ( (s = pthread_create(&ui_updater, NULL, ui_updater_thr, &updparams)) != 0) {
		handle_error_en(s, "pthread_create()");
	}

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	return NULL;
}
