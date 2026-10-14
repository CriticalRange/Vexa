#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <assert.h>
int main(void) {
 EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY); assert(eglInitialize(d,0,0));
 EGLint cfg[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT_KHR,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
 EGLConfig c; EGLint n; assert(eglChooseConfig(d,cfg,&c,1,&n)&&n);
 EGLint ca[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE}; EGLContext ctx=eglCreateContext(d,c,EGL_NO_CONTEXT,ca); assert(ctx!=EGL_NO_CONTEXT);
 int result[2];
 for(int srgb=0;srgb<2;++srgb) {
  EGLint a[]={EGL_WIDTH,2,EGL_HEIGHT,2,EGL_GL_COLORSPACE_KHR,srgb?EGL_GL_COLORSPACE_SRGB_KHR:EGL_GL_COLORSPACE_LINEAR_KHR,EGL_NONE};
  EGLSurface s=eglCreatePbufferSurface(d,c,a); assert(s!=EGL_NO_SURFACE); assert(eglMakeCurrent(d,s,s,ctx));
  glEnable(0x8DB9);
  GLuint t,f; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t); glTexStorage2D(GL_TEXTURE_2D,1,GL_SRGB8_ALPHA8,2,2);
  glGenFramebuffers(1,&f); glBindFramebuffer(GL_FRAMEBUFFER,f); glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t,0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE); glClearColor(.18f,.18f,.18f,1); glClear(GL_COLOR_BUFFER_BIT);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER,0); GLint encoding=0;
  glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER,GL_BACK,GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING,&encoding);
  glBlitFramebuffer(0,0,2,2,0,0,2,2,GL_COLOR_BUFFER_BIT,GL_NEAREST);
  glBindFramebuffer(GL_READ_FRAMEBUFFER,0); unsigned char pixel[16];glReadPixels(0,0,2,2,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
  GLenum error=glGetError(); printf("surface_srgb=%d encoding=%x gray=%u error=%x\n",srgb,encoding,pixel[0],error); assert(error==0); result[srgb]=pixel[0];
  glDeleteFramebuffers(1,&f);glDeleteTextures(1,&t);eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);eglDestroySurface(d,s);
 }
 assert(result[0]>=44&&result[0]<=48); assert(result[1]>=116&&result[1]<=120);puts("SRGB_BLIT_TEST_OK");
 eglDestroyContext(d,ctx);eglTerminate(d);return 0;
}
