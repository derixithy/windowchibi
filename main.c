/* standard headers */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* xlib */
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>

/* imlib2 */
#include <Imlib2.h>

/* function declarations */
static void die(const char *, ...);
static void move_win();
static void setup();
static void update_focus();
static int x_error_handler(Display *, XErrorEvent *);

/* global variables */
static int running = 1;
static Display *dpy = NULL;
static Screen *scr = NULL;
static Window root, focus = None, win;
static int win_w, win_h;
static int win_x_def, win_y_def;

static Atom active_window_atom;

/* user-set options */
static char *img_path = NULL;
static Imlib_Image *img;

/* function definitions */
static void die(const char *why, ...) {
    va_list vargs;
    va_start(vargs, why);
    fprintf(stderr, "FATAL: ");
    vfprintf(stderr, why, vargs);
    fprintf(stderr, "\n");
    fflush(stderr);
    va_end(vargs);
    exit(1);
}

static void move_win() {
    XWindowChanges wc;
    if(focus == None) {
        wc.x = win_x_def;
        wc.y = win_y_def;
    } else {
        Window root_r;
        int x_r, y_r;
        unsigned int w_r, h_r, bw_r, depth_r;
        /* can throw a BadWindow error */
        XGetGeometry(dpy, focus, &root_r, &x_r, &y_r, &w_r, &h_r, &bw_r, &depth_r);
        wc.x = x_r + w_r + bw_r * 2 - win_w;
        wc.y = y_r - win_h;
    }
    XConfigureWindow(dpy, win, CWX | CWY, &wc);
    XSync(dpy, 0);
}

static void setup() {
    XSetErrorHandler(x_error_handler);

    dpy = XOpenDisplay(NULL);
    if(!dpy) die("cannot connect to X");
    scr = DefaultScreenOfDisplay(dpy);
    root = RootWindowOfScreen(scr);

    XVisualInfo vinfo;
    XMatchVisualInfo(dpy, DefaultScreen(dpy), 32, TrueColor, &vinfo);

    Colormap cm = XCreateColormap(dpy, root, vinfo.visual, AllocNone);

    imlib_context_set_display(dpy);
    imlib_context_set_visual(vinfo.visual);
    imlib_context_set_colormap(cm);
    imlib_context_set_image(img);
    win_w = imlib_image_get_width();
    win_h = imlib_image_get_height();

    XSetWindowAttributes wa;
    wa.event_mask = SubstructureNotifyMask | PropertyChangeMask;
    wa.override_redirect = 1;
    wa.colormap = cm;
    wa.border_pixel = 0;
    wa.background_pixel = 0;

    XChangeWindowAttributes(dpy, root, CWEventMask, &wa);

    XClassHint *class_hint = XAllocClassHint();
    if(!class_hint) die("cannot allocate class hint");
    class_hint->res_name = "windowchibi";
    class_hint->res_class = "windowchibi";

    win = XCreateWindow(dpy, root, 0, 0, win_w, win_h, 0, vinfo.depth, CopyFromParent, 
        vinfo.visual, CWColormap | CWBorderPixel | CWBackPixel | CWOverrideRedirect, &wa);
    XSetClassHint(dpy, win, class_hint);
    XMapRaised(dpy, win);
    XSync(dpy, 0);

    active_window_atom = XInternAtom(dpy, "_NET_ACTIVE_WINDOW", True);
    if(active_window_atom == None) die("no _NET_ACTIVE_WINDOW atom");

    /* manually render image on window because imlib2 does it incorrectly with argb drawables */
    imlib_context_set_blend(0);

    Pixmap pm = XCreatePixmap(dpy, root, win_w, win_h, 32);
    imlib_context_set_drawable(pm);
    imlib_render_image_on_drawable(0, 0);

    Pixmap mask = XCreatePixmap(dpy, root, win_w, win_h, 32);
    imlib_context_set_drawable(mask);
    imlib_render_image_on_drawable(0, 0);

    Picture pict = XRenderCreatePicture(dpy, pm, XRenderFindStandardFormat(dpy, PictStandardARGB32), 0, 0);
    Picture pict_win = XRenderCreatePicture(dpy, win, XRenderFindVisualFormat(dpy, vinfo.visual), 0, 0);
    Picture pict_mask = XRenderCreatePicture(dpy, mask, XRenderFindVisualFormat(dpy, vinfo.visual), 0, 0);

    XRenderComposite(dpy, PictOpOver, pict, pict_mask, pict_win, 0, 0, 0, 0, 0, 0, win_w, win_h);

    XRenderFreePicture(dpy, pict_mask);
    XRenderFreePicture(dpy, pict_win);
    XRenderFreePicture(dpy, pict);   
    XFreePixmap(dpy, mask);
    XFreePixmap(dpy, pm);

    update_focus();
    move_win();
}

static void update_focus() {
    Atom actual_type_r;
    int actual_format_r;
    unsigned long nitems_r, bytes_after_r;
    unsigned char *prop_r;
    int status = XGetWindowProperty(dpy, root, active_window_atom, 0, 1000, False, 
        AnyPropertyType, &actual_type_r, &actual_format_r, &nitems_r, &bytes_after_r, &prop_r);
    if(status != Success) return;
    if(!prop_r) return;
    focus = (prop_r[0] << 0) | (prop_r[1] << 8) | (prop_r[2] << 16) | (prop_r[3] << 24);
}

static int x_error_handler(Display *dpy, XErrorEvent *ev) {
    fprintf(stderr, "windowchibi: X error, code %d\n", ev->error_code);
    return 0;
}

/* main program */
int main(int argc, char *argv[]) {
    if(argc != 4) die("wrong usage (%s <path/to/file> <default x> <default y>)", argv[0]);
    img_path = argv[1];
    img = imlib_load_image(img_path);
    if(!img_path) die("cannot load image %s");

    win_x_def = atoi(argv[2]); 
    win_y_def = atoi(argv[3]); 

    setup();

    XEvent ev;
    while(running && !XNextEvent(dpy, &ev)) {
        switch(ev.type) {
        case MapNotify:
            if(!ev.xmap.override_redirect) XRaiseWindow(dpy, win);
            break;
        case PropertyNotify:
            if(ev.xproperty.atom == active_window_atom) update_focus();
            /* fall through */
        case ConfigureNotify:
            move_win();
            break;
        }
    }
}
