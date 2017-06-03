/*
 * xsixhidtest version 2008-05-15
 * Compile with: gcc -o xsixhidtest xsixhidtest.c -lX11 -lm
 * Run with: xsixhidtest < /dev/hidrawX
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

void fatal(const char *msg) { perror(msg); exit(1); }

void xopen(int width, int height,
	   Display **display, int *screen, Window *window, GC *gc) {
  *display = XOpenDisplay(getenv("DISPLAY"));
  if ( ! *display ) fatal("XOpenDisplay");
  *screen = DefaultScreen(*display);
  XSetWindowAttributes xswa;
  xswa.background_pixel = BlackPixel(*display, *screen);
  *window = XCreateWindow(*display, DefaultRootWindow(*display), 
			  32, 32, width, height, 1,
			  CopyFromParent, InputOutput,
			  CopyFromParent, CWBackPixel, &xswa);
  if ( ! *window ) fatal("XCreateWindow");
  XMapWindow(*display, *window);
  *gc = XCreateGC(*display, *window, 0, NULL);
  if ( ! *gc ) fatal("XCreateGC");
  XSync(*display, False);
}

struct state {
  double time;
  int ax, ay, az;       // Raw accelerometer data
  double ddx, ddy, ddz; // Acceleration
  double dx, dy, dz;    // Speed
  double x, y, z;       // Position
};

int main(int argc, char *argv[]) {
  Display *display; int screen; Window window; GC gc;
  int ww = 640, wh = 480;
  xopen(ww, wh, &display, &screen, &window, &gc);
  
  void draw(struct state *s) {
    int xc = ww/2, yc = wh/2;
    double ppm = 1000 / 0.3;
    // Position
    int x = xc + s->x*ppm;
    int y = yc - s->z*ppm;
    int r = xc * 0.05 / (0.5+s->y);
    XDrawLine(display, window, gc, xc,yc, x,y);
    XDrawArc(display, window, gc, x-r,y-r, r*2,r*2, 0,360*64);
    // Speed
    int x1 = x + s->dx*1000;
    int y1 = y - s->dz*1000;
    XDrawLine(display, window, gc, x,y, x1,y1);
    // Acceleration
    int x2 = x1 + s->ddx*100;
    int y2 = y1 - s->ddz*100;
    XDrawLine(display, window, gc, x1,y1, x2,y2);
    // Orientation
    int a0 = 512;  // Note: Not all controllers are centered at 512 ?
    double roll = - atan2(s->ax-a0, s->az-a0);
    double azx = hypot(s->ax-a0, s->az-a0);
    double pitch = atan2(s->ay-a0, azx);
    int hw = 100, hh = 200;
    double cr = cos(roll);
    double sr = sin(roll);
    int xh = xc + pitch*hh/(M_PI/2) * sr;
    int yh = yc - pitch*hh/(M_PI/2) * cr;;
    // Lines at 0 degrees
    XDrawLine(display,window,gc, xh-cr*hw,yh-sr*hw,xh-cr*hw/2,yh-sr*hw/2);
    XDrawLine(display,window,gc, xh+cr*hw,yh+sr*hw,xh+cr*hw/2,yh+sr*hw/2);
    // Line at -90 degrees
    XDrawLine(display,window,gc,
	      xh+sr*hh-cr*hw, yh-cr*hh-sr*hw,
	      xh+sr*hh+cr*hw, yh-cr*hh+sr*hw);
    // Short line at +90 degrees
    XDrawLine(display,window,gc,
	      xh-sr*hh-cr*hw/2, yh+cr*hh-sr*hw/2,
	      xh-sr*hh+cr*hw/2, yh+cr*hh+sr*hw/2);
  }

  struct state prev;
  memset(&prev, 0, sizeof(prev));

  unsigned char buf[128];
  int nr;
  while ( (nr=read(0, buf, sizeof(buf))) ) {
    if ( nr < 0 ) { perror("read(stdin)"); exit(1); }
    if ( nr != 49 ) {
      fprintf(stderr, "Unsupported report length %d."
	      " Wrong hidraw device or kernel<2.6.26 ?\n", nr);
      exit(1);
    }

    struct state new;
    struct timeval tv;
    if ( gettimeofday(&tv, NULL) ) fatal("gettimeofday");
    new.time = tv.tv_sec + tv.tv_usec*1e-6;
    new.ax = buf[41]<<8 | buf[42];
    new.ay = buf[43]<<8 | buf[44];
    new.az = buf[45]<<8 | buf[46];
    if ( ! prev.time ) {
      prev.time = new.time;
      prev.ax = new.ax;
      prev.ay = new.ay;
      prev.az = new.az;
    }
    double dt = new.time - prev.time;
    double rc_dd = 2.0;  // Time constant for highpass filter on acceleration
    double alpha_dd = rc_dd / (rc_dd+dt);
    new.ddx = alpha_dd*(prev.ddx + (new.ax-prev.ax)*0.01);
    new.ddy = alpha_dd*(prev.ddy + (new.ay-prev.ay)*0.01);
    new.ddz = alpha_dd*(prev.ddz - (new.az-prev.az)*0.01);
    double rc_d = 2.0;  // Time constant for highpass filter on speed
    double alpha_d = rc_d / (rc_d+dt);
    new.dx = alpha_d*(prev.dx + new.ddx*dt);
    new.dy = alpha_d*(prev.dy + new.ddy*dt);
    new.dz = alpha_d*(prev.dz + new.ddz*dt);
    double rc = 1.0;  // Time constant for highpass filter on position
    double alpha = rc / (rc+dt);
    new.x = alpha*(prev.x + new.dx*dt);
    new.y = alpha*(prev.y + new.dy*dt);
    new.z = alpha*(prev.z + new.dz*dt);
    XClearWindow(display, window);
    XSetForeground(display, gc, WhitePixel(display,screen));
    draw(&new);
    XSync(display, False);
    // printf("%f %f %f %f\n", dt, new.x, new.y, new.z);
    prev = new;
  }
  return 0;
}
