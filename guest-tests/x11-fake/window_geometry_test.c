#include <X11/Xlib.h>
#include <assert.h>
#include <stdio.h>
int main(void) {
 Display* d = XOpenDisplay(":0"); assert(d);
 Window helper = XCreateSimpleWindow(d, 1, 0, 0, 32, 32, 0, 0, 0);
 Window game = XCreateSimpleWindow(d, 1, 8, 9, 1280, 720, 0, 0, 0);
 XWindowAttributes a; assert(XGetWindowAttributes(d, game, &a));
 assert(a.width == 1280 && a.height == 720 && a.x == 8 && a.y == 9);
 XResizeWindow(d, game, 1920, 1080); assert(XGetWindowAttributes(d, game, &a));
 assert(a.width == 1920 && a.height == 1080);
 Window root; int x, y; unsigned w, h, border, depth;
 assert(XGetGeometry(d, game, &root, &x, &y, &w, &h, &border, &depth));
 assert(w == 1920 && h == 1080 && x == 8 && y == 9);
 assert(XGetWindowAttributes(d, helper, &a)); assert(a.width != 1920);
 XWindowChanges c = {.x=11,.y=12,.width=960,.height=540};
 XConfigureWindow(d, game, CWX|CWY|CWWidth|CWHeight, &c);
 assert(XGetWindowAttributes(d, game, &a));
 assert(a.x==11 && a.y==12 && a.width==960 && a.height==540);
 puts("WINDOW_GEOMETRY_TEST_OK"); return 0;
}
