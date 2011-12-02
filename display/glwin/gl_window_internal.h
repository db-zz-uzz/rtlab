#ifndef GL_WINDOW_INTERNAL_H_INCLUDED
#define GL_WINDOW_INTERNAL_H_INCLUDED

#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <X11/extensions/xf86vmode.h>

#define VERT_SIZE 100.0f
#define PADDING 20 /* pixels */

/* stuff about our window grouped together */
typedef struct {
    Display *dpy;
    int screen;
    Window win;
    GLXContext ctx;
    XSetWindowAttributes attr;
    Bool fs;
    XF86VidModeModeInfo deskMode;
    int x, y;
    unsigned int width, height;
    unsigned int depth;
    float lh;
    float lw;
} GLWindow;

typedef struct {
    int width;
    int height;
    unsigned char *data;
} textureImage;

extern GLWindow GLWin;

void
render_graph(GLWindow *GLWin);

#endif // GL_WINDOW_INTERNAL_H_INCLUDED
