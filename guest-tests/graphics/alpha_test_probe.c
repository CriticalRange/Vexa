// End-to-end desktop alpha-test regression through the real guest GL thunk.
// Run both ELF32/i386 and ELF64 with VEXA_GL_ALPHA_TEST=1.
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"ALPHA_FAIL line=%d %s\n",__LINE__,#x); exit(1); } } while(0)
#define FN(n,r,a) static r (*p_##n) a
FN(GetDoublev,void,(GLenum,GLdouble*)); FN(AlphaFunc,void,(GLenum,GLfloat)); FN(Enable,void,(GLenum)); FN(Disable,void,(GLenum));
FN(IsEnabled,GLboolean,(GLenum)); FN(GetError,GLenum,(void));
FN(GetFloatv,void,(GLenum,GLfloat*)); FN(GetIntegerv,void,(GLenum,GLint*)); FN(GetBooleanv,void,(GLenum,GLboolean*));
FN(CreateShader,GLuint,(GLenum)); FN(ShaderSource,void,(GLuint,GLsizei,const char*const*,const GLint*));
FN(CompileShader,void,(GLuint)); FN(GetShaderiv,void,(GLuint,GLenum,GLint*)); FN(GetShaderInfoLog,void,(GLuint,GLsizei,GLsizei*,char*));
FN(CreateProgram,GLuint,(void)); FN(AttachShader,void,(GLuint,GLuint)); FN(LinkProgram,void,(GLuint));
FN(GetProgramiv,void,(GLuint,GLenum,GLint*)); FN(GetProgramInfoLog,void,(GLuint,GLsizei,GLsizei*,char*));
FN(DeleteShader,void,(GLuint)); FN(DeleteProgram,void,(GLuint)); FN(UseProgram,void,(GLuint));
FN(GetUniformLocation,GLint,(GLuint,const char*)); FN(Uniform1f,void,(GLint,GLfloat)); FN(Uniform4f,void,(GLint,GLfloat,GLfloat,GLfloat,GLfloat));
FN(GenFramebuffers,void,(GLsizei,GLuint*)); FN(BindFramebuffer,void,(GLenum,GLuint));
FN(GenTextures,void,(GLsizei,GLuint*)); FN(BindTexture,void,(GLenum,GLuint)); FN(TexImage2D,void,(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*));
FN(FramebufferTexture2D,void,(GLenum,GLenum,GLenum,GLuint,GLint)); FN(CheckFramebufferStatus,GLenum,(GLenum));
FN(GenRenderbuffers,void,(GLsizei,GLuint*)); FN(BindRenderbuffer,void,(GLenum,GLuint)); FN(RenderbufferStorage,void,(GLenum,GLenum,GLsizei,GLsizei));
FN(FramebufferRenderbuffer,void,(GLenum,GLenum,GLenum,GLuint)); FN(Viewport,void,(GLint,GLint,GLsizei,GLsizei));
FN(ClearColor,void,(GLfloat,GLfloat,GLfloat,GLfloat)); FN(Clear,void,(GLbitfield)); FN(ReadPixels,void,(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,void*));
FN(DrawArrays,void,(GLenum,GLint,GLsizei)); FN(DepthFunc,void,(GLenum)); FN(DepthMask,void,(GLboolean));
FN(BlendFunc,void,(GLenum,GLenum)); FN(ColorMask,void,(GLboolean,GLboolean,GLboolean,GLboolean));
FN(GenBuffers,void,(GLsizei,GLuint*)); FN(BindBuffer,void,(GLenum,GLuint)); FN(BufferData,void,(GLenum,GLsizeiptr,const void*,GLenum));
FN(VertexAttribPointer,void,(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*)); FN(EnableVertexAttribArray,void,(GLuint)); FN(BindAttribLocation,void,(GLuint,GLuint,const char*));
FN(ProgramParameteri,void,(GLuint,GLenum,GLint)); FN(GetProgramBinary,void,(GLuint,GLsizei,GLsizei*,GLenum*,void*)); FN(ProgramBinary,void,(GLuint,GLenum,const void*,GLsizei));
static GLuint vs, program; static unsigned cases;
static void error_free(void) { GLenum e=p_GetError(); if(e)fprintf(stderr,"GL error %x\n",e); CHECK(!e); }
static GLuint shader(GLenum type,const char* source) {
  GLuint s=p_CreateShader(type);CHECK(s);p_ShaderSource(s,1,&source,0);p_CompileShader(s);
  GLint ok=0;p_GetShaderiv(s,GL_COMPILE_STATUS,&ok);
  if(!ok){char info[8192];p_GetShaderInfoLog(s,sizeof(info),0,info);fprintf(stderr,"shader: %s\n",info);}CHECK(ok);return s;
}
static GLuint link(const char* fragment,int legacy) {
  const char* vertex300="#version 300 es\nprecision highp float; uniform float z; void main(){vec2 p=vec2(float((gl_VertexID<<1)&2),float(gl_VertexID&2));gl_Position=vec4(p*2.0-1.0,z,1.0);}";
  const char* vertex100="#version 100\nattribute vec2 pos;uniform highp float z;void main(){gl_Position=vec4(pos,z,1.0);}";
  char modern[1024];snprintf(modern,sizeof(modern),"%s",vertex300);if(!strncmp(fragment,"#version 310",12))modern[10]='1';
  GLuint v=shader(GL_VERTEX_SHADER,legacy?vertex100:modern), f=shader(GL_FRAGMENT_SHADER,fragment), p=p_CreateProgram();
  p_AttachShader(p,v);p_AttachShader(p,f);p_BindAttribLocation(p,0,"pos");
  p_ProgramParameteri(p,GL_PROGRAM_BINARY_RETRIEVABLE_HINT,GL_TRUE);p_LinkProgram(p);
  GLint ok=0;p_GetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char info[8192];p_GetProgramInfoLog(p,sizeof(info),0,info);fprintf(stderr,"link: %s\n",info);}CHECK(ok);
  p_DeleteShader(v);p_DeleteShader(f);return p;
}
static GLuint target(void) {
  GLuint f,t,d;p_GenFramebuffers(1,&f);p_BindFramebuffer(GL_FRAMEBUFFER,f);p_GenTextures(1,&t);p_BindTexture(GL_TEXTURE_2D,t);
  p_TexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,0);p_FramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,t,0);
  p_GenRenderbuffers(1,&d);p_BindRenderbuffer(GL_RENDERBUFFER,d);p_RenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,8,8);
  p_FramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,d);CHECK(p_CheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);p_Viewport(0,0,8,8);return f;
}
static void blue(void) {p_ColorMask(1,1,1,1);p_DepthMask(1);p_ClearColor(0,0,1,1);p_Clear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);}
static void color(float r,float g,float b,float a,float z) {
  p_UseProgram(program);p_Uniform4f(p_GetUniformLocation(program,"color"),r,g,b,a);p_Uniform1f(p_GetUniformLocation(program,"z"),z);
}
static void pixel(int red,int green,int blue_) {
  unsigned char px[4]={0};p_ReadPixels(3,3,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  if(abs(px[0]-red)>1||abs(px[1]-green)>1||abs(px[2]-blue_)>1)fprintf(stderr,"pixel %u %u %u wanted %d %d %d\n",px[0],px[1],px[2],red,green,blue_);
  CHECK(abs(px[0]-red)<=1&&abs(px[1]-green)<=1&&abs(px[2]-blue_)<=1);error_free();++cases;
}
static void draw(void){p_DrawArrays(GL_TRIANGLES,0,3);}
int main(void) {
  setbuf(stdout,0);setbuf(stderr,0);puts("ALPHA_PROBE_START");
  void* lib=dlopen("libGL.so.1",RTLD_NOW|RTLD_LOCAL);if(!lib)fprintf(stderr,"dlopen %s\n",dlerror());CHECK(lib);
  void* (*getproc)(const GLubyte*)=dlsym(lib,"glXGetProcAddress");
  void* (*create)(void*,void*,void*,int)=dlsym(lib,"glXCreateContext");
  int (*bind)(void*,unsigned long,void*)=dlsym(lib,"glXMakeCurrent");CHECK(getproc&&create&&bind);
  void* first=create(0,0,0,1);CHECK(first&&bind(0,1,first));
#define LOAD(n) do{*(void**)(&p_##n)=getproc((const GLubyte*)"gl"#n);CHECK(p_##n);}while(0)
  LOAD(GetDoublev);LOAD(AlphaFunc);LOAD(Enable);LOAD(Disable);LOAD(IsEnabled);LOAD(GetError);LOAD(GetFloatv);LOAD(GetIntegerv);LOAD(GetBooleanv);
  LOAD(CreateShader);LOAD(ShaderSource);LOAD(CompileShader);LOAD(GetShaderiv);LOAD(GetShaderInfoLog);LOAD(CreateProgram);LOAD(AttachShader);LOAD(LinkProgram);LOAD(GetProgramiv);LOAD(GetProgramInfoLog);LOAD(DeleteShader);LOAD(DeleteProgram);LOAD(UseProgram);LOAD(GetUniformLocation);LOAD(Uniform1f);LOAD(Uniform4f);
  LOAD(GenFramebuffers);LOAD(BindFramebuffer);LOAD(GenTextures);LOAD(BindTexture);LOAD(TexImage2D);LOAD(FramebufferTexture2D);LOAD(CheckFramebufferStatus);LOAD(GenRenderbuffers);LOAD(BindRenderbuffer);LOAD(RenderbufferStorage);LOAD(FramebufferRenderbuffer);LOAD(Viewport);LOAD(ClearColor);LOAD(Clear);LOAD(ReadPixels);LOAD(DrawArrays);LOAD(DepthFunc);LOAD(DepthMask);LOAD(BlendFunc);LOAD(ColorMask);
  LOAD(GenBuffers);LOAD(BindBuffer);LOAD(BufferData);LOAD(VertexAttribPointer);LOAD(EnableVertexAttribArray);LOAD(BindAttribLocation);LOAD(ProgramParameteri);LOAD(GetProgramBinary);LOAD(ProgramBinary);
  while(p_GetError());
  CHECK(!p_IsEnabled(GL_ALPHA_TEST));GLint value=0;p_GetIntegerv(GL_ALPHA_TEST_FUNC,&value);CHECK(value==GL_ALWAYS);
  GLfloat ref=-1;p_GetFloatv(GL_ALPHA_TEST_REF,&ref);CHECK(ref==0);p_AlphaFunc(GL_LESS,-2);p_GetFloatv(GL_ALPHA_TEST_REF,&ref);CHECK(ref==0);p_AlphaFunc(GL_GREATER,2);p_GetFloatv(GL_ALPHA_TEST_REF,&ref);CHECK(ref==1);
  p_GetIntegerv(GL_ALPHA_TEST_REF,&value);CHECK(value==2147483647);p_AlphaFunc(0xffffffff,.25f);CHECK(p_GetError()==GL_INVALID_ENUM);CHECK(!p_GetError());p_GetFloatv(GL_ALPHA_TEST_REF,&ref);CHECK(ref==1);p_GetIntegerv(GL_ALPHA_TEST_FUNC,&value);CHECK(value==GL_GREATER);
  p_AlphaFunc(GL_ALWAYS,.5f);p_GetIntegerv(GL_ALPHA_TEST_REF,&value);CHECK(value==1073741824);
  struct {double value;double guard[16];} query={0};for(int i=0;i<16;++i)query.guard[i]=1234;
  p_GetDoublev(GL_ALPHA_TEST_REF,&query.value);CHECK(query.value==.5);for(int i=0;i<16;++i)CHECK(query.guard[i]==1234);
  void (*directAlpha)(GLenum,GLfloat)=dlsym(lib,"glAlphaFunc");CHECK(directAlpha);directAlpha(GL_GREATER,.25);p_GetFloatv(GL_ALPHA_TEST_REF,&ref);CHECK(ref==.25);
  GLuint fbo=target();
  const char* fragment="#version 300 es\nprecision highp float;uniform vec4 color;layout(location=0) out vec4 outputColor;void main(){outputColor=color;if(color.a<0.75)return;outputColor=color;}";
  program=link(fragment,0);p_Enable(GL_ALPHA_TEST);CHECK(p_IsEnabled(GL_ALPHA_TEST));GLboolean enabled=0;p_GetBooleanv(GL_ALPHA_TEST,&enabled);CHECK(enabled);
  for(int fn=0;fn<8;++fn)for(int a=0;a<3;++a){float alpha=.25f+.25f*a;int pass=fn==0?0:fn==1?alpha<.5f:fn==2?alpha==.5f:fn==3?alpha<=.5f:fn==4?alpha>.5f:fn==5?alpha!=.5f:fn==6?alpha>=.5f:1;
    blue();color(1,0,0,alpha,0);p_AlphaFunc(GL_NEVER+fn,.5);draw();pixel(pass?255:0,0,pass?0:255);}
  p_AlphaFunc(GL_NEVER,.5);p_Disable(GL_ALPHA_TEST);blue();color(1,0,0,.25,0);draw();pixel(255,0,0);
  // A discarded near fragment must not write depth, even with color masked off.
  p_Enable(GL_ALPHA_TEST);p_Enable(GL_DEPTH_TEST);p_DepthFunc(GL_LESS);blue();p_AlphaFunc(GL_NEVER,.5);color(1,0,0,.25,-.5);p_ColorMask(0,0,0,0);draw();p_ColorMask(1,1,1,1);p_Disable(GL_ALPHA_TEST);color(0,1,0,1,.5);draw();pixel(0,255,0);p_Disable(GL_DEPTH_TEST);
  p_Enable(GL_ALPHA_TEST);p_AlphaFunc(GL_GREATER,.1);p_Enable(GL_BLEND);p_BlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);blue();color(1,0,0,.25,0);draw();pixel(64,0,191);p_Disable(GL_BLEND);
  // Shared programs must use the current context's independent alpha state.
  p_AlphaFunc(GL_NEVER,.5);void* second=create(0,0,first,1);CHECK(second&&bind(0,1,second));target();CHECK(!p_IsEnabled(GL_ALPHA_TEST));blue();color(1,0,0,.25,0);draw();pixel(255,0,0);
  CHECK(bind(0,1,first));p_BindFramebuffer(GL_FRAMEBUFFER,fbo);p_Viewport(0,0,8,8);CHECK(p_IsEnabled(GL_ALPHA_TEST));blue();color(1,0,0,.25,0);draw();pixel(0,0,255);
  // Relinking invalidates cached uniform locations.
  p_LinkProgram(program);p_GetProgramiv(program,GL_LINK_STATUS,&value);CHECK(value);blue();p_AlphaFunc(GL_LESS,.5);color(1,0,0,.25,0);draw();pixel(255,0,0);
  // Program binary reload retains output selectors through reflected names.
  GLint formats=0;p_GetIntegerv(GL_NUM_PROGRAM_BINARY_FORMATS,&formats);
  if(formats){GLint length=0;p_GetProgramiv(program,GL_PROGRAM_BINARY_LENGTH,&length);CHECK(length>0);void* bytes=malloc(length);GLsizei written=0;GLenum format=0;p_GetProgramBinary(program,length,&written,&format,bytes);GLuint copy=p_CreateProgram();p_ProgramBinary(copy,format,bytes,written);free(bytes);p_GetProgramiv(copy,GL_LINK_STATUS,&value);CHECK(value);program=copy;blue();p_AlphaFunc(GL_GREATER,.5);color(1,0,0,.25,0);draw();pixel(0,0,255);}
  const char* mrt="#version 300 es\nprecision highp float;uniform vec4 color;layout(location=1) out vec4 one;layout(location=0) out vec4 zero;void main(){one=vec4(1.,0.,0.,1.);zero=color;}";
  program=link(mrt,0);blue();p_AlphaFunc(GL_GREATER,.5);color(1,0,0,.25,0);draw();pixel(0,0,255);
  const char* early="#version 310 es\nprecision highp float;layout(early_fragment_tests) in;uniform vec4 color;layout(location=0) out vec4 o;void main(void);void /* main comment */ main(void){o=color;return;}";
  program=link(early,0);p_Enable(GL_DEPTH_TEST);blue();p_AlphaFunc(GL_NEVER,.5);color(1,0,0,.25,-.5);draw();p_Disable(GL_ALPHA_TEST);color(0,1,0,1,.5);draw();pixel(0,255,0);p_Disable(GL_DEPTH_TEST);p_Enable(GL_ALPHA_TEST);
  const char* array="#version 300 es\nprecision highp float;uniform vec4 color;layout(location=0) out vec4 colors[2];void main(){colors[0]=color;colors[1]=vec4(1.);}";
  program=link(array,0);blue();p_AlphaFunc(GL_GREATER,.5);color(1,0,0,.25,0);draw();pixel(0,0,255);
  const char* legacy="#version 120\nuniform vec4 color;void main(){gl_FragColor=color;return;}";
  program=link(legacy,1);GLuint buffer;p_GenBuffers(1,&buffer);p_BindBuffer(GL_ARRAY_BUFFER,buffer);float vertices[]={-1,-1,3,-1,-1,3};p_BufferData(GL_ARRAY_BUFFER,sizeof(vertices),vertices,GL_STATIC_DRAW);p_VertexAttribPointer(0,2,GL_FLOAT,0,0,0);p_EnableVertexAttribArray(0);
  blue();p_AlphaFunc(GL_GREATER,.5);color(1,0,0,.25,0);draw();pixel(0,0,255);p_AlphaFunc(GL_LESS,.5);draw();pixel(255,0,0);
  printf("ALPHA_PROBE_PASS bits=%u cases=%u\n",(unsigned)(8*sizeof(void*)),cases);return 0;
}
