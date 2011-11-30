#ifndef GRAPH_H_INCLUDED
#define GRAPH_H_INCLUDED

#include <glib.h>
#include <cairo.h>
#include <gtk/gtkdrawingarea.h>
#include <stdint.h>

#define GTK_GRAPH(obj)          GTK_CHECK_CAST (obj, gtk_graph_get_type (), GtkGraph)
#define GTK_GRAPH_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, gtk_graph_get_type (), GtkGraphClass)
#define GTK_IS_GRAPH(obj)       GTK_CHECK_TYPE (obj, gtk_graph_get_type ())

#define GRAPH_TITLES_LENGHT 64

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct tag_mcolor mcolor;
struct tag_mcolor {
	double r;
	double g;
	double b;
};


typedef struct _GtkGraph        GtkGraph;
typedef struct _GtkGraphClass   GtkGraphClass;

struct _GtkGraph
{
  GtkDrawingArea widget;

  /* update policy (GTK_UPDATE_[CONTINUOUS/DELAYED/DISCONTINUOUS]) */
  guint policy : 2;

  char title[GRAPH_TITLES_LENGHT + 1];
  char subt1[GRAPH_TITLES_LENGHT + 1];
  char subt2[GRAPH_TITLES_LENGHT + 1];

  mcolor color_title;
  mcolor color_subt1;
  mcolor color_subt2;

  uint8_t updated_once;

  uint32_t pending_frame_no;
  uint32_t pending_samples;
  float *pending_buf1;
  float *pending_buf2;

  float maxy1;
  float miny1;

  float maxy2;
  float miny2;

  GMutex *mutex;
};

struct _GtkGraphClass
{
  GtkDrawingAreaClass parent_class;
};

GtkWidget*     gtk_graph_new		(char *_title, char *_subt1, char *_subt2);
GtkType        gtk_graph_get_type	(void);
void           gtk_graph_draw_data  (GtkGraph *graph, float *buf1, float *buf2, uint32_t samples, uint32_t frame_no);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // GRAPH_H_INCLUDED
