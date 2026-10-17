"""Compile the actual X11 pointer helpers into an Android regression probe."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
out = root / 'logs/ets2-fix'
source = (root / 'guest-tests/x11-fake/libX11_fake.c').read_text()
def function(signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]

code = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef void Display;
typedef unsigned long Window;
typedef int Bool;
#define None 0
#define True 1
#define MotionNotify 6
struct vexa_window_geometry { int width, height; };
static struct { int width,height; } g_screen = {1280,720};
typedef struct { struct { Window window; } xmotion; } XEvent;
static int g_pointer_x=100,g_pointer_y=200,g_warp_x,g_warp_y,g_warp_centered,g_warp_mode;
static int g_pointer_grabbed;
static unsigned g_pointer_state;
static Window g_input_window=1;
static int events,poll;
static const struct vexa_window_geometry* vexa_window(Window w) { return NULL; }
static void vexa_fill_pointer(XEvent* e,Display* d,int t,int x,int y,int s) {}
static void vexa_q_push(XEvent* e) { ++events; }
static void vexa_poll_pointer(Display* d) { if(poll) {g_pointer_x=321;g_pointer_y=432;} }
static void vexa_trace_pointer(const char* k,int x,int y,int dx,int dy,int ox,int oy,Window w) {}
''' + function('static int vexa_absolute_pointer(') + '\n' + function('static void vexa_relative_pointer(') + '\n' + function('int XWarpPointer(') + '\n' + function('Bool XQueryPointer(') + r'''
int main(void) {
  XWarpPointer(NULL,0,0,0,0,0,0,0,0);
  assert(g_pointer_x==100 && g_pointer_y==200 && events==0);
  XWarpPointer(NULL,0,0,0,0,0,0,12,-20);
  assert(g_pointer_x==112 && g_pointer_y==180 && events==1);
  XWarpPointer(NULL,0,1,0,0,0,0,640,360);
  assert(g_pointer_x==640 && g_pointer_y==360 && g_warp_centered);
  for (int i=0;i<12;++i) {
    int mx,my;
    vexa_relative_pointer(10,0,&mx,&my);
    g_pointer_x=mx;g_pointer_y=my;
    assert(mx==650+i*10 && my==360);
  }
  assert(events==2); /* Moving must not fabricate center events. */
  XWarpPointer(NULL,0,1,0,0,0,0,640,360);
  int mx,my;vexa_relative_pointer(-10,10,&mx,&my);
  assert(mx==630 && my==370);
  int x,y; poll=1;
  assert(XQueryPointer(NULL,1,NULL,NULL,NULL,NULL,&x,&y,NULL));
  assert(x==321 && y==432);
  setenv("VEXA_X11_ABSOLUTE_POINTER","1",1);
  g_pointer_grabbed=1;
  int before=events;
  g_pointer_x=1100;g_pointer_y=650;
  XWarpPointer(NULL,0,1,0,0,0,0,640,360);
  assert(events==before && g_pointer_x==1100 && g_pointer_y==650);
  unsetenv("VEXA_X11_ABSOLUTE_POINTER");
  g_pointer_grabbed=0;
  XWarpPointer(NULL,0,1,0,0,0,0,640,360);
  assert(events==before+1);
  puts("PASS relative zero, relative delta, absolute center, fresh pointer query");
}
'''
out.mkdir(parents=True, exist_ok=True)
cpp = out / 'pointer_probe.c'
cpp.write_text(code)
clang = Path.home() / 'AppData/Local/Android/Sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe'
subprocess.run([str(clang), '--target=aarch64-linux-android28', str(cpp),
                '-o', str(out / 'pointer_probe')], check=True)
