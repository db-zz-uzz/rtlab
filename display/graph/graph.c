#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <string.h>

#include "graph.h"

#define COLOR(c) (c).r, (c).g, (c).b
#define PADDING 10

static GtkDrawingAreaClass *parent_class = NULL;

static void
gtk_graph_class_init (GtkGraphClass *class);

static void
gtk_graph_init (GtkGraph *graph);

static void
gtk_graph_destroy (GtkObject *object);

static void
gtk_graph_realize (GtkWidget *widget);

static void
gtk_graph_size_request (GtkWidget *widget, GtkRequisition *requisition);

static void
gtk_graph_size_allocate (GtkWidget *widget, GtkAllocation *allocation);

static gboolean
gtk_graph_expose( GtkWidget *widget, GdkEventExpose *event );





GtkType
gtk_graph_get_type ()
{
	static GtkType graph_type = 0;

	if (!graph_type)
	{
		static const GtkTypeInfo graph_info =
			{
				"GtkGraph",
				sizeof (GtkGraph),
				sizeof (GtkGraphClass),
				(GtkClassInitFunc) gtk_graph_class_init,
				(GtkObjectInitFunc) gtk_graph_init,
				/* reserved_1 */ NULL,
				/* reserved_1 */ NULL,
				(GtkClassInitFunc) NULL
			};

		graph_type = gtk_type_unique (GTK_TYPE_WIDGET, &graph_info);
	}

	return graph_type;
}

static void
gtk_graph_class_init (GtkGraphClass *class)
{
  GtkObjectClass *object_class;
  GtkWidgetClass *widget_class;

  object_class = (GtkObjectClass*) class;
  widget_class = (GtkWidgetClass*) class;

  parent_class = gtk_type_class (gtk_widget_get_type ());

  object_class->destroy = gtk_graph_destroy;

  widget_class->realize = gtk_graph_realize;
  widget_class->expose_event = gtk_graph_expose;
  widget_class->size_request = gtk_graph_size_request;
  widget_class->size_allocate = gtk_graph_size_allocate;
  widget_class->button_press_event = NULL; //gtk_graph_button_press;
  widget_class->button_release_event = NULL; //gtk_graph_button_release;
  widget_class->motion_notify_event = NULL; //gtk_graph_motion_notify;
}

static void
set_color(mcolor *color, double r, double g, double b)
{
	g_return_if_fail (color != NULL);

	color->r = r;
	color->g = g;
	color->b = b;

	return;
}

static void
gtk_graph_init (GtkGraph *graph)
{
	sprintf(graph->title, "Title");
	sprintf(graph->subt1, "Line #1");
	sprintf(graph->subt2, "Line #1");

	set_color(&graph->color_title, 0.0, 0.0, 0.0);
	set_color(&graph->color_subt1, 0.1647, 0.3372, 1.0);
	set_color(&graph->color_subt2, 0.4823, 0.0, 0.4627);
	//set_color(&graph->color_subt1, 1.0, 0.0, 0.0);
	//set_color(&graph->color_subt2, 0.0, 1.0, 0.0);

	graph->maxy1 = 0.2;
	graph->miny1 = -0.2;

	graph->maxy2 = 200.0;
	graph->miny2 = -200.0;

	graph->updated_once = 0;
}

GtkWidget*
gtk_graph_new (char *_title, char *_subt1, char *_subt2)
{
	GtkGraph *graph;

	graph = gtk_type_new (gtk_graph_get_type ());

	strncpy(graph->title, _title, GRAPH_TITLES_LENGHT);
	graph->title[GRAPH_TITLES_LENGHT] = 0;

	strncpy(graph->subt1, _subt1, GRAPH_TITLES_LENGHT);
	graph->subt1[GRAPH_TITLES_LENGHT] = 0;

	strncpy(graph->subt2, _subt2, GRAPH_TITLES_LENGHT);
	graph->subt2[GRAPH_TITLES_LENGHT] = 0;

	graph->mutex = g_mutex_new();

	return GTK_WIDGET (graph);
}

static void
gtk_graph_destroy (GtkObject *object)
{
  GtkGraph *graph;

  g_return_if_fail (object != NULL);
  g_return_if_fail (GTK_IS_GRAPH (object));

  graph = GTK_GRAPH (object);

  g_mutex_free(graph->mutex);

  if (GTK_OBJECT_CLASS (parent_class)->destroy)
    (* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
gtk_graph_realize (GtkWidget *widget)
{
  GtkGraph *graph;
  GdkWindowAttr attributes;
  gint attributes_mask;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_GRAPH (widget));

  GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
  graph = GTK_GRAPH (widget);

  attributes.x = widget->allocation.x;
  attributes.y = widget->allocation.y;
  attributes.width = widget->allocation.width;
  attributes.height = widget->allocation.height;
  attributes.wclass = GDK_INPUT_OUTPUT;
  attributes.window_type = GDK_WINDOW_CHILD;
  attributes.event_mask = gtk_widget_get_events (widget) |
    GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK |
    GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK |
    GDK_POINTER_MOTION_HINT_MASK;
  attributes.visual = gtk_widget_get_visual (widget);
  attributes.colormap = gtk_widget_get_colormap (widget);

  attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
  widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attributes, attributes_mask);

  widget->style = gtk_style_attach (widget->style, widget->window);

  gdk_window_set_user_data (widget->window, widget);

  gtk_style_set_background (widget->style, widget->window, GTK_STATE_ACTIVE);
}

static void
gtk_graph_size_request (GtkWidget      *widget,
		       GtkRequisition *requisition)
{
  requisition->width = 600;
  requisition->height = 150;
}


static void
gtk_graph_size_allocate (GtkWidget     *widget,
			GtkAllocation *allocation)
{
  GtkGraph *graph;

  g_return_if_fail (widget != NULL);
  g_return_if_fail (GTK_IS_GRAPH (widget));
  g_return_if_fail (allocation != NULL);

  widget->allocation = *allocation;
  if (GTK_WIDGET_REALIZED (widget))
    {
      graph = GTK_GRAPH (widget);

      gdk_window_move_resize (widget->window,
			      allocation->x, allocation->y,
			      allocation->width, allocation->height);
    }
}

#define CLIP(val, min, max) ( val > max ? max : ( val < min ? min : val) )

static void
graph_draw_line(cairo_t *cr, float *buf, uint32_t samples,
				gint width, gint height,
				float maxy, float miny,
				mcolor *color)
{
	double hstep = (double)(width - 2 * PADDING) / samples;
	double vstep = (double)(height - 2 * PADDING)/(maxy - miny);
	double vcenter = maxy * vstep + PADDING;
	uint32_t i;
	cairo_save(cr);

	    cairo_set_source_rgb (cr, COLOR(*color));
		cairo_set_line_width(cr, 1.0);

		cairo_move_to(cr, PADDING, vcenter - CLIP(buf[0], miny, maxy) * vstep);
		//printf("%6i\t%.3f\t%.3f\n", 0, (float)PADDING, vcenter - CLIP(buf[0], miny, maxy) * vstep );
		for (i = 1; i < samples; i += 2) {
			cairo_line_to(cr, PADDING + hstep * i, vcenter - CLIP(buf[i], miny, maxy) * vstep );
			//printf("%6i\t%.9f\n", i, buf[i]);
//			printf("%6i\t%.9f\t%.3f\t%.3f\n", i, buf[i], PADDING + hstep * i, CLIP(buf[i], miny, maxy) * vstep );
		}
//		printf("hstep %.3f\tvstep %.3f\tvcenter %.3f\twidth %u\theight %u\tmin %.3f\tmax %.3f\n",
//				hstep, vstep, vcenter, width, height, miny, maxy);
		cairo_stroke (cr);

	cairo_restore(cr);
}

static gboolean
gtk_graph_expose( GtkWidget *widget, GdkEventExpose *event )
{
  GtkGraph *graph;
  gint height, width;
  cairo_t *cr;

  g_return_val_if_fail (widget != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_GRAPH (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (event->count > 0)
    return FALSE;

/*
  gdk_threads_enter();

  if (g_mutex_trylock(graph->mutex) == FALSE) {
  	gdk_threads_leave();
  	return FALSE;
  }
*/
  graph = GTK_GRAPH (widget);

  gdk_window_clear_area (widget->window,
			 0, 0,
			 widget->allocation.width,
			 widget->allocation.height);


  height = widget->allocation.height;
  width = widget->allocation.width;


  cr = gdk_cairo_create (widget->window);

    /* clear background */
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_paint (cr);

    cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                                        CAIRO_FONT_WEIGHT_NORMAL);

    /* enclosing in a save/restore pair since we alter the
     * font size
     */
    cairo_save (cr);
      cairo_set_font_size (cr, 14);
      cairo_move_to (cr, 10, 18);
      cairo_set_source_rgb (cr, COLOR(graph->color_title));
      cairo_show_text (cr, graph->title);

      if (graph->updated_once) {
		char str[64];
		cairo_move_to (cr, width - 10 - 100, 18);
		sprintf(str, "# %u", graph->pending_frame_no);
		cairo_show_text(cr, str);
      }

    cairo_restore (cr);

	if (graph->pending_buf1 != NULL && graph->pending_samples > 0) {
		graph_draw_line(cr, graph->pending_buf1, graph->pending_samples,
						width, height,
						graph->maxy1,
						graph->miny1,
						&graph->color_subt1);
	}

	if (graph->pending_buf2 != NULL && graph->pending_samples > 0) {
		graph_draw_line(cr, graph->pending_buf2, graph->pending_samples,
						width, height,
						graph->maxy2,
						graph->miny2,
						&graph->color_subt2);
	}

    cairo_set_source_rgb (cr, COLOR(graph->color_subt1));
    cairo_set_font_size (cr, 10);
    cairo_move_to (cr, 25, 36);
    cairo_show_text (cr, graph->subt1);

    cairo_set_source_rgb (cr, COLOR(graph->color_subt2));
    cairo_set_font_size (cr, 10);
    cairo_move_to (cr, 25, 52);
    cairo_show_text (cr, graph->subt2);

    cairo_set_source_rgb (cr, 0,0,0);

    cairo_move_to(cr, PADDING, height/2);
    cairo_set_line_width(cr, 0.5);
    cairo_line_to(cr, width - PADDING, height/2);

    cairo_stroke (cr);

  cairo_destroy (cr);
/*
  g_mutex_unlock(graph->mutex);

  gdk_threads_leave();
*/

  return FALSE;
}

void
gtk_graph_draw_data (GtkGraph *graph,
					float *buf1, float *buf2,
					uint32_t samples, uint32_t frame_no)
{
	g_return_if_fail (graph != NULL);
	graph->updated_once = 1;

	gdk_threads_enter();

//	g_mutex_lock(graph->mutex);

	if (buf1) {
		if (graph->pending_buf1 == NULL) {
			graph->pending_buf1 = g_malloc(samples * sizeof(float));
		} else if (samples != graph->pending_samples) {
			graph->pending_buf1 = g_realloc(graph->pending_buf1, samples * sizeof(float));
		}
		memcpy(graph->pending_buf1, buf1, samples * sizeof(float));
	}

	if (buf2) {
		if (graph->pending_buf2 == NULL) {
			graph->pending_buf2 = g_malloc(samples * sizeof(float));
		} else if (samples != graph->pending_samples) {
			graph->pending_buf2 = g_realloc(graph->pending_buf2, samples * sizeof(float));
		}
		memcpy(graph->pending_buf2, buf2, samples * sizeof(float));
	}

	graph->pending_frame_no = frame_no;
	graph->pending_samples = samples;

//	g_mutex_unlock(graph->mutex);

	gtk_widget_queue_draw(GTK_WIDGET(graph));

    while (gtk_events_pending ())
        gtk_main_iteration ();

	gdk_threads_leave();
}
