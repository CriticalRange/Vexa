#define VEXA_ARB_PROBE_EMBEDDED
#include "arb_shader_objects_probe.c"

int arb_assembly_probe_main(void) {
  CHECK(arb_shader_objects_probe_main()==0);unsigned before=cases;
  puts("ASSEMBLY_PROBE_START");
  void* lib=dlopen("libGL.so.1",RTLD_NOW|RTLD_LOCAL);CHECK(lib);
  void* (*getproc)(const GLubyte*)=dlsym(lib,"glXGetProcAddress");CHECK(getproc);
  ARB(GenPrograms,PFNGLGENPROGRAMSARBPROC);ARB(BindProgram,PFNGLBINDPROGRAMARBPROC);
  ARB(DeletePrograms,PFNGLDELETEPROGRAMSARBPROC);ARB(IsProgram,PFNGLISPROGRAMARBPROC);
  ARB(ProgramString,PFNGLPROGRAMSTRINGARBPROC);ARB(GetProgramiv,PFNGLGETPROGRAMIVARBPROC);
  ARB(GetProgramString,PFNGLGETPROGRAMSTRINGARBPROC);
  ARB(ProgramLocalParameter4f,PFNGLPROGRAMLOCALPARAMETER4FARBPROC);
  ARB(ProgramLocalParameter4dv,PFNGLPROGRAMLOCALPARAMETER4DVARBPROC);
  ARB(GetProgramLocalParameterdv,PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC);
  ARB(ProgramEnvParameter4f,PFNGLPROGRAMENVPARAMETER4FARBPROC);
  ARB(GetProgramEnvParameterfv,PFNGLGETPROGRAMENVPARAMETERFVARBPROC);
  void (*attribute)(GLuint,GLfloat,GLfloat,GLfloat,GLfloat)=getproc((const GLubyte*)"glVertexAttrib4f");CHECK(attribute);
  const GLubyte* (*getstring)(GLenum)=getproc((const GLubyte*)"glGetString");CHECK(getstring);
  void* (*create)(void*,void*,void*,int)=dlsym(lib,"glXCreateContext");
  void* (*getcontext)(void)=dlsym(lib,"glXGetCurrentContext");
  int (*bind)(void*,unsigned long,void*)=dlsym(lib,"glXMakeCurrent");CHECK(create&&getcontext&&bind);
  GLboolean (*iscoreprogram)(GLuint)=getproc((const GLubyte*)"glIsProgram");CHECK(iscoreprogram);
  GLuint names[3]={0,0,0xdeadbeef};a_GenPrograms(2,names);CHECK(names[0]&&names[1]&&names[0]!=names[1]&&names[2]==0xdeadbeef);
  CHECK(!a_IsProgram(names[0]));a_BindProgram(GL_VERTEX_PROGRAM_ARB,names[0]);CHECK(a_IsProgram(names[0]));
  GLint value=-1;a_GetProgramiv(GL_VERTEX_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&value);CHECK(value==(GLint)names[0]);
  a_BindProgram(GL_FRAGMENT_PROGRAM_ARB,names[0]);CHECK(p_GetError()==GL_INVALID_OPERATION);
  a_GetProgramiv(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&value);CHECK(!value);
  a_BindProgram(0xffffffff,names[1]);CHECK(p_GetError()==GL_INVALID_ENUM);
  a_BindProgram(GL_FRAGMENT_PROGRAM_ARB,names[1]);CHECK(a_IsProgram(names[1]));
  const char* vertex="!!ARBvp1.0\nATTRIB pos = vertex.position; PARAM offset = program.local[0]; ADD result.position, pos, offset; MOV result.texcoord[0], vertex.attrib[1]; END";
  const char* fragment="!!ARBfp1.0\nTEMP r; MUL r, program.local[0], program.env[0]; MOV result.color, r; END";
  a_ProgramString(GL_VERTEX_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,strlen(vertex),vertex);
  a_ProgramString(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,strlen(fragment),fragment);error_free();
  p_GetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB,&value);CHECK(value==-1);
  a_GetProgramiv(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_UNDER_NATIVE_LIMITS_ARB,&value);CHECK(value==1);
  a_GetProgramiv(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_LENGTH_ARB,&value);CHECK(value==(GLint)strlen(fragment));
  char copy[1024];memset(copy,0xcc,sizeof(copy));a_GetProgramString(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_STRING_ARB,copy);CHECK(!memcmp(copy,fragment,strlen(fragment))&&(unsigned char)copy[strlen(fragment)]==0xcc);
  a_ProgramLocalParameter4f(GL_FRAGMENT_PROGRAM_ARB,0,1,0,0,.25);
  a_ProgramEnvParameter4f(GL_FRAGMENT_PROGRAM_ARB,0,1,1,1,1);
  GLdouble params[4]={0};a_GetProgramLocalParameterdv(GL_FRAGMENT_PROGRAM_ARB,0,params);CHECK(params[0]==1&&params[3]==.25);
  GLfloat env[4]={0};a_GetProgramEnvParameterfv(GL_FRAGMENT_PROGRAM_ARB,0,env);CHECK(env[0]==1&&env[3]==1);
  a_ProgramLocalParameter4f(GL_VERTEX_PROGRAM_ARB,0,0,0,0,0);
  attribute(1,.5,.5,0,1);p_UseProgram(0);p_Enable(GL_VERTEX_PROGRAM_ARB);p_Enable(GL_FRAGMENT_PROGRAM_ARB);
  CHECK(p_IsEnabled(GL_VERTEX_PROGRAM_ARB)&&p_IsEnabled(GL_FRAGMENT_PROGRAM_ARB));
  p_Enable(GL_ALPHA_TEST);p_AlphaFunc(GL_GREATER,.5);blue();draw();pixel(0,0,255);
  p_AlphaFunc(GL_LESS,.5);draw();pixel(255,0,0);
  p_GetIntegerv(GL_CURRENT_PROGRAM,&value);CHECK(!value);
  // Vertex and fragment local register zero must remain independent.
  a_ProgramLocalParameter4f(GL_VERTEX_PROGRAM_ARB,0,16,0,0,0);blue();draw();pixel(0,0,255);
  a_ProgramLocalParameter4f(GL_VERTEX_PROGRAM_ARB,0,0,0,0,0);
  p_Disable(GL_ALPHA_TEST);
  // Names/local registers are shared; binding/enables/env registers are per context.
  void* first=getcontext();GLint fbo=0;p_GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&fbo);
  void* second=create(0,0,first,1);CHECK(second&&bind(0,1,second));target();
  GLuint vertices;p_GenBuffers(1,&vertices);p_BindBuffer(GL_ARRAY_BUFFER,vertices);
  GLfloat triangle[]={-1,-1,3,-1,-1,3};p_BufferData(GL_ARRAY_BUFFER,sizeof(triangle),triangle,GL_STATIC_DRAW);
  p_VertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);p_EnableVertexAttribArray(0);attribute(1,.5,.5,0,1);
  CHECK(!p_IsEnabled(GL_VERTEX_PROGRAM_ARB));a_BindProgram(GL_VERTEX_PROGRAM_ARB,names[0]);a_BindProgram(GL_FRAGMENT_PROGRAM_ARB,names[1]);
  p_Enable(GL_VERTEX_PROGRAM_ARB);p_Enable(GL_FRAGMENT_PROGRAM_ARB);blue();draw();pixel(0,0,0);
  a_ProgramEnvParameter4f(GL_FRAGMENT_PROGRAM_ARB,0,1,1,1,1);blue();draw();pixel(255,0,0);
  GLdouble green[4]={0,1,0,1};a_ProgramLocalParameter4dv(GL_FRAGMENT_PROGRAM_ARB,0,green);blue();draw();pixel(0,255,0);
  CHECK(bind(0,1,first));p_BindFramebuffer(GL_FRAMEBUFFER,fbo);p_Viewport(0,0,8,8);blue();draw();pixel(0,255,0);
  // GLSL programs take precedence and the assembly draw must restore program 0.
  program=link("#version 120\nvoid main(){gl_FragColor=vec4(1.,0.,0.,1.);}",1);p_UseProgram(program);blue();draw();pixel(255,0,0);p_UseProgram(0);blue();draw();pixel(0,255,0);
  // Reupload invalidates the linked pair; ALU constants, swizzles and saturation.
  const char* alu="!!ARBfp1.0\nPARAM c={0.25,0.5,0.75,1}; TEMP r; MUL r,c,{2,2,2,1}; ADD_SAT r,r,{-0.25,-0.5,-0.75,0}; MOV result.color,r; END";
  a_ProgramString(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,strlen(alu),alu);error_free();blue();draw();pixel(64,128,191);
  const char* tex="!!ARBfp1.0\nTEX result.color, fragment.texcoord[0], texture[0], 2D; END";
  a_ProgramString(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,strlen(tex),tex);error_free();
  GLuint texture;p_GenTextures(1,&texture);p_BindTexture(GL_TEXTURE_2D,texture);unsigned char yellow[]={255,255,0,255};
  p_TexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,yellow);blue();draw();pixel(255,255,0);
  const char* kill="!!ARBfp1.0\nKIL {-1,0,0,0}; MOV result.color,{1,0,0,1}; END";
  a_ProgramString(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,strlen(kill),kill);error_free();blue();draw();pixel(0,0,255);
  const char* invalid="!!ARBfp1.0\nUNSUPPORTED result.color,{1,0,0,1}; END";
  a_ProgramString(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,strlen(invalid),invalid);CHECK(p_GetError()==GL_INVALID_OPERATION);
  p_GetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB,&value);CHECK(value>=0);CHECK(strlen((const char*)getstring(GL_PROGRAM_ERROR_STRING_ARB))>0);
  blue();draw();CHECK(p_GetError()==GL_INVALID_OPERATION);pixel(0,0,255);
  value=12345;a_GetProgramiv(GL_FRAGMENT_PROGRAM_ARB,0xffffffff,&value);CHECK(value==12345&&p_GetError()==GL_INVALID_ENUM);
  a_ProgramLocalParameter4f(GL_FRAGMENT_PROGRAM_ARB,128,0,0,0,0);CHECK(p_GetError()==GL_INVALID_VALUE);
  a_GenPrograms(-1,names);CHECK(p_GetError()==GL_INVALID_VALUE);a_DeletePrograms(-1,names);CHECK(p_GetError()==GL_INVALID_VALUE);
  a_DeletePrograms(2,names);CHECK(!a_IsProgram(names[0])&&!a_IsProgram(names[1]));a_GetProgramiv(GL_VERTEX_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&value);CHECK(!value);
  CHECK(bind(0,1,second));a_GetProgramiv(GL_VERTEX_PROGRAM_ARB,GL_PROGRAM_BINDING_ARB,&value);CHECK(!value);CHECK(bind(0,1,first));
  p_Disable(GL_VERTEX_PROGRAM_ARB);p_Disable(GL_FRAGMENT_PROGRAM_ARB);error_free();
  printf("ASSEMBLY_PROBE_PASS bits=%u pixels=%u\n",(unsigned)(8*sizeof(void*)),cases-before);return 0;
}

#ifndef VEXA_ASSEMBLY_PROBE_EMBEDDED
int main(void){return arb_assembly_probe_main();}
#endif
