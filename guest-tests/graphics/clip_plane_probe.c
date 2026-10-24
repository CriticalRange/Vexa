#define VEXA_ASSEMBLY_PROBE_EMBEDDED
#include "arb_assembly_probe.c"
static void at(int x,int y,int r,int g,int b) {
  GLubyte px[4]={0};p_ReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  if(abs(px[0]-r)>1||abs(px[1]-g)>1||abs(px[2]-b)>1)fprintf(stderr,"CLIP pixel %d,%d=%u,%u,%u expected %d,%d,%d\n",x,y,px[0],px[1],px[2],r,g,b);
  CHECK(abs(px[0]-r)<=1&&abs(px[1]-g)<=1&&abs(px[2]-b)<=1);error_free();++cases;
}
int clip_plane_probe_main(void) {
  CHECK(arb_assembly_probe_main()==0);unsigned before=cases;puts("CLIP_PROBE_START");
  void* lib=dlopen("libGL.so.1",RTLD_NOW|RTLD_LOCAL);CHECK(lib);
  void* (*getproc)(const GLubyte*)=dlsym(lib,"glXGetProcAddress");CHECK(getproc);
#define CLIP_FN(name,result,params) result (*c_##name)params=getproc((const GLubyte*)"gl"#name);CHECK(c_##name)
  CLIP_FN(ClipPlane,void,(GLenum,const GLdouble*));CLIP_FN(GetClipPlane,void,(GLenum,GLdouble*));
  CLIP_FN(MatrixMode,void,(GLenum));CLIP_FN(LoadIdentity,void,(void));CLIP_FN(Translatef,void,(GLfloat,GLfloat,GLfloat));
  CLIP_FN(Ortho,void,(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble));CLIP_FN(PushMatrix,void,(void));CLIP_FN(PopMatrix,void,(void));
  CLIP_FN(ClearStencil,void,(GLint));CLIP_FN(StencilFunc,void,(GLenum,GLint,GLuint));CLIP_FN(StencilOp,void,(GLenum,GLenum,GLenum));CLIP_FN(StencilMask,void,(GLuint));
  c_MatrixMode(GL_MODELVIEW);c_LoadIdentity();c_MatrixMode(GL_PROJECTION);c_LoadIdentity();
  GLint limit=0;p_GetIntegerv(GL_MAX_CLIP_PLANES,&limit);CHECK(limit==6);
  for(int i=0;i<6;++i){CHECK(!p_IsEnabled(GL_CLIP_PLANE0+i));GLdouble q[4]={1,1,1,1};c_GetClipPlane(GL_CLIP_PLANE0+i,q);CHECK(q[0]==0&&q[3]==0);}
  GLdouble plane[4]={1,0,0,0};c_MatrixMode(GL_MODELVIEW);c_Translatef(.5,0,0);c_ClipPlane(GL_CLIP_PLANE0,plane);
  GLdouble eq[6]={0,0,0,0,1234,5678};c_GetClipPlane(GL_CLIP_PLANE0,eq);CHECK(eq[0]==1&&eq[3]==-.5&&eq[4]==1234&&eq[5]==5678);
  c_LoadIdentity();c_GetClipPlane(GL_CLIP_PLANE0,eq);CHECK(eq[3]==-.5);error_free();
  const char* fs="#version 300 es\nprecision highp float;uniform vec4 color;layout(location=0) out vec4 o;void main(){o=color;return;}";
  program=link(fs,0);p_Disable(GL_ALPHA_TEST);p_Enable(GL_CLIP_PLANE0);blue();color(1,0,0,1,0);draw();at(5,3,0,0,255);at(7,3,255,0,0);
  c_MatrixMode(GL_PROJECTION);c_PushMatrix();c_Ortho(-2,0,-1,1,-1,1);blue();draw();at(7,3,0,0,255);c_PopMatrix();blue();draw();at(7,3,255,0,0);
  // Crossing triangle: per-fragment clipping must keep the surviving half.
  c_MatrixMode(GL_MODELVIEW);c_ClipPlane(GL_CLIP_PLANE0,plane);blue();draw();at(1,3,0,0,255);at(6,3,255,0,0);
  GLdouble upper[4]={0,1,0,0};c_ClipPlane(GL_CLIP_PLANE1,upper);p_Enable(GL_CLIP_PLANE1);blue();draw();at(6,1,0,0,255);at(6,6,255,0,0);
  p_Disable(GL_CLIP_PLANE1);p_Disable(GL_CLIP_PLANE0);blue();draw();at(1,3,255,0,0);
  // Shared programs use the active context's enable/equation values.
  p_Enable(GL_CLIP_PLANE0);void* (*getcontext)(void)=dlsym(lib,"glXGetCurrentContext");void* first=getcontext();
  void* (*create)(void*,void*,void*,int)=dlsym(lib,"glXCreateContext");int(*bind)(void*,unsigned long,void*)=dlsym(lib,"glXMakeCurrent");
  GLint fbo=0;p_GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo);void* second=create(0,0,first,1);CHECK(second&&bind(0,1,second));target();
  CHECK(!p_IsEnabled(GL_CLIP_PLANE0));blue();color(1,0,0,1,0);draw();at(1,3,255,0,0);
  CHECK(bind(0,1,first));p_BindFramebuffer(GL_FRAMEBUFFER,fbo);p_Viewport(0,0,8,8);CHECK(p_IsEnabled(GL_CLIP_PLANE0));blue();color(1,0,0,1,0);draw();at(1,3,0,0,255);
  // Shader-supplied eye-space clip vertex overrides projected-position fallback.
  const char* vs="#version 100\nattribute vec2 pos;uniform float z;void main(){gl_Position=vec4(pos,z,1.);gl_ClipVertex=vec4(pos.x-0.5,pos.y,z,1.);}";
  GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,"#version 120\nuniform vec4 color;void main(){gl_FragColor=color;}");
  program=p_CreateProgram();p_AttachShader(program,v);p_AttachShader(program,f);p_BindAttribLocation(program,0,"pos");p_LinkProgram(program);GLint status=0;p_GetProgramiv(program,GL_LINK_STATUS,&status);CHECK(status);
  blue();color(1,0,0,1,0);draw();at(5,3,0,0,255);at(7,3,255,0,0);
  // Clipped pixels must leave depth and stencil unchanged, including early tests.
  p_Disable(GL_CLIP_PLANE0);GLuint ds;p_GenRenderbuffers(1,&ds);p_BindRenderbuffer(GL_RENDERBUFFER,ds);p_RenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH24_STENCIL8,8,8);p_FramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_STENCIL_ATTACHMENT,GL_RENDERBUFFER,ds);CHECK(p_CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
  program=link("#version 310 es\nprecision highp float;layout(early_fragment_tests) in;uniform vec4 color;layout(location=0)out vec4 o;void main(){o=color;}",0);
  GLdouble reject[4]={0,0,0,-1};c_ClipPlane(GL_CLIP_PLANE0,reject);p_Enable(GL_CLIP_PLANE0);p_Enable(GL_DEPTH_TEST);p_DepthFunc(GL_LESS);p_Enable(GL_STENCIL_TEST);c_StencilMask(255);c_ClearStencil(0);blue();p_Clear(GL_STENCIL_BUFFER_BIT);
  c_StencilFunc(GL_ALWAYS,1,255);c_StencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);color(1,0,0,1,-.5);p_ColorMask(0,0,0,0);draw();p_ColorMask(1,1,1,1);
  p_Disable(GL_CLIP_PLANE0);c_StencilFunc(GL_EQUAL,0,255);c_StencilOp(GL_KEEP,GL_KEEP,GL_KEEP);color(0,1,0,1,.5);draw();at(3,3,0,255,0);p_Disable(GL_DEPTH_TEST);p_Disable(GL_STENCIL_TEST);
  eq[0]=1234;c_GetClipPlane(GL_CLIP_PLANE0+6,eq);CHECK(eq[0]==1234&&p_GetError()==GL_INVALID_ENUM);
  c_ClipPlane(GL_CLIP_PLANE0+6,plane);CHECK(p_GetError()==GL_INVALID_ENUM);
  c_Ortho(0,0,-1,1,-1,1);CHECK(p_GetError()==GL_INVALID_VALUE);error_free();
  printf("CLIP_PROBE_PASS bits=%u pixels=%u\n",(unsigned)(8*sizeof(void*)),cases-before);return 0;
}

#ifndef VEXA_CLIP_PROBE_EMBEDDED
int main(void){return clip_plane_probe_main();}
#endif
