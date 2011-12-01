#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/queue.h>

#include "glwin/gl_window.h"
#include "glwin/gl_window_internal.h"

//#define AUTOLEVELS

STAILQ_HEAD(SDataBufferHead, SDataBuffer);

struct SDataBuffer {
	float *left;
	float *right;
	uint32_t samples;
	STAILQ_ENTRY(SDataBuffer) entry;
};

struct scolor {
	float r;
	float g;
	float b;
};

struct SRect {
	float left;
	float right;
	float top;
	float bottom;
};

struct slimits {
	float max;
	float min;
};

struct SGraph {
	struct SDataBufferHead head;
	struct scolor color[CHANNELS];
	struct slimits limits[CHANNELS];
};

static struct SGraph graphs[GRAPH_COUNT];


static void
del_buf(struct SDataBuffer *buf)
{
	if (buf->left)
		free(buf->left);

	if (buf->right)
		free(buf->right);

	free(buf);
}

static float
clamp_limits(float data, float min, float max)
{
	if (data > max)
		return max;

	if (data < min)
		return min;

	return data;
}

static void
render_line(struct SGraph *graph, int channel, float *data, uint32_t samples, struct SRect *rect)
{
	uint32_t i;
	float hstep;
	float vstep;
	float vcenter;

	float max = graph->limits[channel].max;
	float min = graph->limits[channel].min;
#ifdef AUTOLEVELS
	float newmax = max;
	float newmin = min;
#endif
	float hsize = max - min;

	struct scolor *color = &graph->color[channel];

	vcenter = (rect->bottom + rect->top) / 2;

	vstep = (rect->bottom - rect->top) / hsize;
	hstep = (rect->right - rect->left) / samples;
/*
	printf("%.3f %.3f %.3f %.3f, %.3f\n",
			rect->top, rect->bottom,
			rect->left, rect->right,
			vcenter);
*/
	glColor3f(color->r, color->g, color->b);
	glBegin(GL_LINE_STRIP);
	for (i = 0; i < samples; i += 2) {
		glVertex2f(rect->left + hstep * i, vcenter - vstep * clamp_limits(data[i], min, max));
#ifdef AUTOLEVELS
		if (data[i] < min)
			newmin = min;

		if (data[i] > max)
			newmax = max;
#endif
	}

#ifdef AUTOLEVELS
	if (newmax > max)
		graph->limits[channel].max = newmax;

	if (newmin < min)
		graph->limits[channel].min = newmin;
#endif

	glEnd();
}

static void
render_buffer(struct SGraph *graph, struct SRect *rect)
{
	struct SDataBuffer *buf = STAILQ_FIRST(&graph->head);

	if (buf == NULL)
		return;

	while (STAILQ_NEXT(buf, entry) != NULL) {
		STAILQ_REMOVE_HEAD(&graph->head, entry);
		del_buf(buf);
		buf = STAILQ_FIRST(&graph->head);
	}

	render_line(graph, LEFT, buf->left, buf->samples, rect);
	render_line(graph, RIGHT, buf->right, buf->samples, rect);

}

void
glwin_draw_init()
{
	int i, j;
	for (i = 0; i < GRAPH_COUNT; i++) {
		memset(&graphs[i].head, 0, sizeof(struct SDataBufferHead));
		STAILQ_INIT(&graphs[i].head);

		for (j = 0; j < CHANNELS; j++) {
			graphs[i].color[j].r = 1.0;
			graphs[i].color[j].g = 1.0;
			graphs[i].color[j].b = 1.0;

			graphs[i].limits[j].max = 20;
			graphs[i].limits[j].min = -20;
		}
	}
}

void
glwin_draw_close()
{
	int i;
	struct SDataBuffer *buf;
	for (i = 0; i < GRAPH_COUNT; i++) {
		while (STAILQ_FIRST(&graphs[i].head) != NULL) {
			buf = STAILQ_FIRST(&graphs[i].head);
			STAILQ_REMOVE_HEAD(&graphs[i].head, entry);

			del_buf(buf);
		}
	}
}

void
render_graph(GLWindow *win)
{
	float vcenter;
	struct SRect trect;
	struct SRect brect;

	vcenter = win->lh / 2;

	trect.left = PADDING;
	trect.right = win->lw - PADDING;
	trect.top = PADDING;
	trect.bottom = vcenter - PADDING;

	brect.left = PADDING;
	brect.right = win->lw - PADDING;
	brect.top = vcenter + PADDING;
	brect.bottom = win->lh - PADDING;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -1.0f);

	glColor3f(0.2, 0.2, 0.2);
	glBegin(GL_LINES);
		glVertex2f(0.0 + PADDING, vcenter);
		glVertex2f(win->lw - PADDING, vcenter);
	glEnd();

	render_buffer(&graphs[GRAPH_SAMPLES], &trect);
	render_buffer(&graphs[GRAPH_FFT], &brect);
#if 0
	glColor3f(0.0, 1.0, 0.2);
    glBegin(GL_LINE_LOOP);
        /* front face */
        glVertex2f(-1.0, -1.0);
		glVertex2f(-1.0,  1.0);
        glVertex2f( 1.0,  1.0);
		glVertex2f( 1.0, -1.0);
    glEnd();

	glBegin(GL_LINES);
		glVertex2f(2.0, 2.0);
		glVertex2f(4.0, 4.0);
	glEnd();
#endif
    glXSwapBuffers(GLWin.dpy, GLWin.win);
    return;
}

void
glwin_set_color(int graph, int channel, float r, float g, float b)
{
	graphs[graph].color[channel].r = r;
	graphs[graph].color[channel].g = g;
	graphs[graph].color[channel].b = b;
}

void
glwin_set_limits(int graph, int channel, float min, float max)
{
	graphs[graph].limits[channel].min = min;
	graphs[graph].limits[channel].max = max;
}

void
glwin_draw_data(int graph, float *left, float *right, uint32_t samples)
{
	if (graph > GRAPH_COUNT || left == NULL || right == NULL || samples == 0) {
		return;
	}

	uint32_t buflen = sizeof(float) * samples;
	struct SDataBuffer *buf = malloc(sizeof(struct SDataBuffer));
	memset(buf, 0, sizeof(struct SDataBuffer));

	buf->samples = samples;

	buf->left = malloc(buflen);
	buf->right = malloc(buflen);

	memcpy(buf->left, left, buflen);
	memcpy(buf->right, right, buflen);

	STAILQ_INSERT_TAIL(&graphs[graph].head, buf, entry);
}
