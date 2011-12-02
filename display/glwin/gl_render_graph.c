#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/queue.h>
#include <math.h>
#include <pthread.h>

#include "glwin/gl_window.h"
#include "glwin/gl_window_internal.h"

#if 0
#define AUTOLEVELS
#endif

#define BRESENHAM_SCALING

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
pthread_mutex_t draw_mutex;

void
draw_lock()
{
	/* printf("locking\n"); */
	pthread_mutex_lock(&draw_mutex);
	/* printf("locked\n"); */
}

void
draw_unlock()
{
	/* printf("unlocking\n"); */
	pthread_mutex_unlock(&draw_mutex);
}

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

#define AVERAGE(a, b)   (PIXEL)( ((a) + (b)) >> 1 )


static void
render_line(struct SGraph *graph, int channel,
			float *data, uint32_t samples,
			struct SRect *rect, uint32_t pixels)
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

	uint32_t increment = samples / pixels;

#ifdef BRESENHAM_SCALING
	int num_pixels = pixels;
	int int_part = samples / pixels;
	int frac_part = samples % pixels;
	int e = 0;
#endif

	if (increment == 0)
		increment = 1;

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
	glLineWidth(0.1);
	/* glEnable(GL_LINE_SMOOTH); */
	/* glHint(GL_FASTEST, GL_LINE_SMOOTH_HINT); */
	glBegin(GL_LINE_STRIP);
#ifdef BRESENHAM_SCALING
	i = 0;
	while (num_pixels-- > 0) {
		glVertex2f(rect->left + hstep * i, vcenter - vstep * clamp_limits(data[i], min, max));
		i += int_part;
		e += frac_part;
		if (e >= pixels) {
			e -= pixels;
			i += 1;
		}
	}
#else
	for (i = 0; i < samples; i += increment) {
		if (i > samples)
			i = samples - 1;

		glVertex2f(rect->left + hstep * i, vcenter - vstep * clamp_limits(data[i], min, max));
#ifdef AUTOLEVELS
		if (data[i] < min)
			newmin = min;

		if (data[i] > max)
			newmax = max;
#endif
	}
#endif
	glEnd();

#ifdef AUTOLEVELS
	if (newmax > max)
		graph->limits[channel].max = newmax;

	if (newmin < min)
		graph->limits[channel].min = newmin;
#endif
}

static void
render_buffer(struct SGraph *graph, struct SRect *rect, uint32_t pixels)
{
	struct SDataBuffer *buf = STAILQ_FIRST(&graph->head);

	if (buf == NULL)
		return;

	while (STAILQ_NEXT(buf, entry) != NULL) {
		STAILQ_REMOVE_HEAD(&graph->head, entry);
		del_buf(buf);
		buf = STAILQ_FIRST(&graph->head);
	}

	render_line(graph, LEFT, buf->left, buf->samples, rect, pixels);
	render_line(graph, RIGHT, buf->right, buf->samples, rect, pixels);

}

void
glwin_draw_init()
{
	int i, j;

	pthread_mutexattr_t mutex_attr;

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

	pthread_mutexattr_init(&mutex_attr);
/*
	pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_NORMAL);
*/
	pthread_mutex_init(&draw_mutex, &mutex_attr);

	pthread_mutexattr_destroy(&mutex_attr);
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

	pthread_mutex_destroy(&draw_mutex);
}

void
render_graph(GLWindow *win)
{
	float third, two;
	struct SRect trect;
	struct SRect mrect;
	struct SRect brect;
	int pixel_width = win->width - PADDING * 2;
	float glpadding = (float)PADDING / win->height * VERT_SIZE;

	third = win->lh / 3;
	two = win->lh - third;

	trect.left = glpadding;
	trect.right = win->lw - glpadding;
	trect.top = glpadding;
	trect.bottom = third - glpadding;

	mrect.left = glpadding;
	mrect.right = win->lw - glpadding;
	mrect.top = third + glpadding;
	mrect.bottom = two - glpadding;

	brect.left = glpadding;
	brect.right = win->lw - glpadding;
	brect.top = two + glpadding;
	brect.bottom = win->lh - glpadding;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glTranslatef(0.0f, 0.0f, -1.0f);

	glColor3f(0.2, 0.2, 0.2);
	glBegin(GL_LINES);
		glVertex2f(0.0 + glpadding, third);
		glVertex2f(win->lw - glpadding, third);
		glVertex2f(0.0 + glpadding, two);
		glVertex2f(win->lw - glpadding, two);
	glEnd();

	render_buffer(&graphs[GRAPH_SAMPLES], &trect, pixel_width);
	render_buffer(&graphs[GRAPH_FFT], &mrect, pixel_width);
	render_buffer(&graphs[GRAPH_SDENS], &brect, pixel_width);
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

void
glwin_draw_data_c(int graph, float *left, float *right, uint32_t samples)
{
	uint32_t i;

	if (graph > GRAPH_COUNT || left == NULL || right == NULL || samples == 0) {
		return;
	}

	uint32_t buflen = sizeof(float) * samples;
	struct SDataBuffer *buf = malloc(sizeof(struct SDataBuffer));
	memset(buf, 0, sizeof(struct SDataBuffer));

	buf->samples = samples;

	buf->left = malloc(buflen);
	buf->right = malloc(buflen);

	for (i = 0; i < samples; i++) {
		buf->left[i] = (float)sqrt(left[i*2] * left[i*2] + left[i*2+1] + left[i*2+1]);
		buf->right[i] = (float)sqrt(right[i*2] * right[i*2] + right[i*2+1] + right[i*2+1]);
	}

	STAILQ_INSERT_TAIL(&graphs[graph].head, buf, entry);
}

void
glwin_render_data()
{
	draw_unlock();
}
