#ifndef GL_WINDOW_H_INCLUDED
#define GL_WINDOW_H_INCLUDED

#include <stdint.h>

enum EGraphType {
	GRAPH_SAMPLES = 0,
	GRAPH_FFT,

	/* should be last */
	GRAPH_COUNT
};

enum EGraphChannels {
	LEFT = 0,
	RIGHT,
	CHANNELS
};

void
glwin_draw_init();

void
glwin_draw_close();

int
glwin_main(int argc, char **argv);

void
glwin_draw_data(int graph, float *left, float *right, uint32_t samples);

void
glwin_set_color(int graph, int channel, float r, float g, float b);

void
glwin_set_limits(int graph, int channel, float min, float max);

#endif // GL_WINDOW_H_INCLUDED
