// Exercise ARB shader objects through both FEX guest ABIs, including alpha
// regression first. This reuses the same real GLX/FBO readback harness.
#define main alpha_probe_main
#include "alpha_test_probe.c"
#undef main

#define ARB(n, type) type a_##n = (type)getproc((const GLubyte*)"gl"#n"ARB"); CHECK(a_##n)
int arb_shader_objects_probe_main(void) {
  CHECK(alpha_probe_main() == 0);
  unsigned before = cases;
  puts("ARB_PROBE_START");
  void* lib = dlopen("libGL.so.1", RTLD_NOW | RTLD_LOCAL); CHECK(lib);
  void* (*getproc)(const GLubyte*) = dlsym(lib, "glXGetProcAddress"); CHECK(getproc);
  CHECK(sizeof(GLhandleARB) == sizeof(GLuint));
  ARB(CreateShaderObject, PFNGLCREATESHADEROBJECTARBPROC);
  ARB(CreateProgramObject, PFNGLCREATEPROGRAMOBJECTARBPROC);
  ARB(ShaderSource, PFNGLSHADERSOURCEARBPROC);
  ARB(CompileShader, PFNGLCOMPILESHADERARBPROC);
  ARB(AttachObject, PFNGLATTACHOBJECTARBPROC);
  ARB(DetachObject, PFNGLDETACHOBJECTARBPROC);
  ARB(LinkProgram, PFNGLLINKPROGRAMARBPROC);
  ARB(UseProgramObject, PFNGLUSEPROGRAMOBJECTARBPROC);
  ARB(ValidateProgram, PFNGLVALIDATEPROGRAMARBPROC);
  ARB(DeleteObject, PFNGLDELETEOBJECTARBPROC);
  ARB(GetHandle, PFNGLGETHANDLEARBPROC);
  ARB(GetObjectParameteriv, PFNGLGETOBJECTPARAMETERIVARBPROC);
  ARB(GetObjectParameterfv, PFNGLGETOBJECTPARAMETERFVARBPROC);
  ARB(GetInfoLog, PFNGLGETINFOLOGARBPROC);
  ARB(GetAttachedObjects, PFNGLGETATTACHEDOBJECTSARBPROC);
  ARB(GetShaderSource, PFNGLGETSHADERSOURCEARBPROC);
  ARB(GetUniformLocation, PFNGLGETUNIFORMLOCATIONARBPROC);
  ARB(GetUniformfv, PFNGLGETUNIFORMFVARBPROC);
  ARB(GetUniformiv, PFNGLGETUNIFORMIVARBPROC);
  ARB(GetActiveUniform, PFNGLGETACTIVEUNIFORMARBPROC);
  ARB(Uniform4fv, PFNGLUNIFORM4FVARBPROC);
  ARB(Uniform1i, PFNGLUNIFORM1IARBPROC);
  ARB(UniformMatrix4fv, PFNGLUNIFORMMATRIX4FVARBPROC);
  ARB(BindAttribLocation, PFNGLBINDATTRIBLOCATIONARBPROC);
  ARB(GetAttribLocation, PFNGLGETATTRIBLOCATIONARBPROC);
  ARB(GetActiveAttrib, PFNGLGETACTIVEATTRIBARBPROC);
  while (p_GetError());
  GLhandleARB v = a_CreateShaderObject(GL_VERTEX_SHADER);
  GLuint f = p_CreateShader(GL_FRAGMENT_SHADER); CHECK(v && f);
  const char* vertex = "#version 100\nattribute vec2 pos;uniform mat4 transform;void main(){gl_Position=transform*vec4(pos,0.,1.);}";
  const char* fragments[] = {"#version 120\nuniform vec4 color;uniform int which;", "void main(){gl_FragColor=which==7?color:vec4(0.,1.,0.,1.);}"};
  GLint lengths[] = {(GLint)strlen(fragments[0]), -1};
  a_ShaderSource(v, 1, &vertex, 0); a_ShaderSource(f, 2, fragments, lengths);
  a_CompileShader(v); a_CompileShader(f);
  GLint value = -1; a_GetObjectParameteriv(v, GL_OBJECT_COMPILE_STATUS_ARB, &value); CHECK(value);
  a_GetObjectParameteriv(f, GL_OBJECT_COMPILE_STATUS_ARB, &value); CHECK(value);
  a_GetObjectParameteriv(v, GL_OBJECT_TYPE_ARB, &value); CHECK(value == GL_SHADER_OBJECT_ARB);
  a_GetObjectParameteriv(v, GL_OBJECT_SUBTYPE_ARB, &value); CHECK(value == GL_VERTEX_SHADER);
  GLfloat fv = -1; a_GetObjectParameterfv(v, GL_OBJECT_TYPE_ARB, &fv); CHECK(fv == GL_SHADER_OBJECT_ARB);
  struct { char text[1024]; unsigned guard; } source = {{0}, 0x1234abcd};
  GLsizei length = -1; a_GetShaderSource(v, sizeof(source.text), &length, source.text);
  CHECK(length > 0 && strstr(source.text, "main") && source.guard == 0x1234abcd);
  GLhandleARB p = a_CreateProgramObject(); CHECK(p);
  void (*direct_attach)(GLuint, GLuint) = dlsym(lib, "glAttachObjectARB"); CHECK(direct_attach);
  direct_attach(p, v); a_AttachObject(p, f);
  a_GetObjectParameteriv(p, GL_OBJECT_TYPE_ARB, &value); CHECK(value == GL_PROGRAM_OBJECT_ARB);
  a_GetObjectParameteriv(p, GL_OBJECT_ATTACHED_OBJECTS_ARB, &value); CHECK(value == 2);
  GLuint attached[3] = {0,0,0x98765432}; GLsizei count = -1;
  a_GetAttachedObjects(p, 2, &count, attached); CHECK(count == 2 && attached[2] == 0x98765432);
  CHECK((attached[0] == v && attached[1] == f) || (attached[1] == v && attached[0] == f));
  a_AttachObject(p, f); CHECK(p_GetError() == GL_INVALID_OPERATION); CHECK(!p_GetError());
  a_BindAttribLocation(p, 0, "pos"); a_LinkProgram(p);
  a_GetObjectParameteriv(p, GL_OBJECT_LINK_STATUS_ARB, &value); CHECK(value);
  CHECK(a_GetAttribLocation(p, "pos") == 0);
  char name[128]; GLint size = 0; GLenum type = 0;
  a_GetActiveAttrib(p, 0, sizeof(name), &length, &size, &type, name); CHECK(!strcmp(name, "pos") && size == 1 && type == GL_FLOAT_VEC2);
  a_UseProgramObject(p); CHECK(a_GetHandle(GL_PROGRAM_OBJECT_ARB) == p);
  GLint colorLoc = a_GetUniformLocation(p, "color"), whichLoc = a_GetUniformLocation(p, "which"), matrixLoc = a_GetUniformLocation(p, "transform"); CHECK(colorLoc >= 0 && whichLoc >= 0 && matrixLoc >= 0);
  GLfloat red[] = {1,0,0,.25}, matrix[] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  a_Uniform4fv(colorLoc, 1, red); a_Uniform1i(whichLoc, 7); a_UniformMatrix4fv(matrixLoc, 1, GL_FALSE, matrix);
  GLfloat got[4] = {0}; a_GetUniformfv(p, colorLoc, got); CHECK(!memcmp(got, red, sizeof(got)));
  a_GetUniformiv(p, whichLoc, &value); CHECK(value == 7);
  a_GetObjectParameteriv(p, GL_OBJECT_ACTIVE_UNIFORMS_ARB, &value); CHECK(value >= 3);
  int foundColor = 0;
  for (int i = 0; i < value; ++i) { a_GetActiveUniform(p, i, sizeof(name), &length, &size, &type, name); if (!strcmp(name,"color")) {CHECK(size == 1 && type == GL_FLOAT_VEC4); foundColor = 1;} }
  CHECK(foundColor); a_ValidateProgram(p); a_GetObjectParameteriv(p, GL_OBJECT_VALIDATE_STATUS_ARB, &value); CHECK(value);
  p_Enable(GL_ALPHA_TEST); p_AlphaFunc(GL_GREATER,.5); blue(); draw(); pixel(0,0,255);
  p_AlphaFunc(GL_LESS,.5); draw(); pixel(255,0,0);
  // Wrong object kind, unknown pname and stale handles must preserve outputs.
  value = 12345; a_GetObjectParameteriv(p, GL_OBJECT_COMPILE_STATUS_ARB, &value); CHECK(value == 12345 && p_GetError() == GL_INVALID_OPERATION);
  fv = 12345; a_GetObjectParameterfv(v, GL_OBJECT_LINK_STATUS_ARB, &fv); CHECK(fv == 12345 && p_GetError() == GL_INVALID_OPERATION);
  a_GetObjectParameteriv(v, 0xffffffff, &value); CHECK(value == 12345 && p_GetError() == GL_INVALID_ENUM);
  a_GetObjectParameteriv(0xffffffff, GL_OBJECT_TYPE_ARB, &value); CHECK(value == 12345 && p_GetError() == GL_INVALID_VALUE);
  CHECK(a_GetHandle(0xffffffff) == 0 && p_GetError() == GL_INVALID_ENUM);
  a_DeleteObject(0); error_free();
  // Compile errors must be observable via the generic ARB information log.
  GLuint bad = a_CreateShaderObject(GL_FRAGMENT_SHADER); const char* invalid = "#version 100\nvoid main(){invalid syntax;}";
  a_ShaderSource(bad, 1, &invalid, 0); a_CompileShader(bad); a_GetObjectParameteriv(bad, GL_OBJECT_COMPILE_STATUS_ARB, &value); CHECK(!value);
  a_GetObjectParameteriv(bad, GL_OBJECT_INFO_LOG_LENGTH_ARB, &value); CHECK(value > 1);
  char log[1024] = {0}; a_GetInfoLog(bad, sizeof(log), &length, log); CHECK(length > 0 && log[length] == 0);
  length = 12345; log[0] = 'x'; a_GetInfoLog(bad, -1, &length, log); CHECK(length == 12345 && log[0] == 'x' && p_GetError() == GL_INVALID_VALUE);
  a_DeleteObject(bad); value = 12345; a_GetObjectParameteriv(bad, GL_OBJECT_TYPE_ARB, &value); CHECK(value == 12345 && p_GetError() == GL_INVALID_VALUE);
  // Deferred shader and current-program deletion must preserve a usable draw.
  a_DeleteObject(v); a_DeleteObject(f); a_GetObjectParameteriv(v, GL_OBJECT_DELETE_STATUS_ARB, &value); CHECK(value);
  a_DeleteObject(p); a_GetObjectParameteriv(p, GL_OBJECT_DELETE_STATUS_ARB, &value); CHECK(value);
  blue(); draw(); pixel(255,0,0);
  a_UseProgramObject(0); CHECK(a_GetHandle(GL_PROGRAM_OBJECT_ARB) == 0);
  value = 12345; a_GetObjectParameteriv(p, GL_OBJECT_TYPE_ARB, &value); CHECK(value == 12345 && p_GetError() == GL_INVALID_VALUE);
  // Detaching a deleted shader releases its final attachment.
  GLuint spare = a_CreateProgramObject(), pending = a_CreateShaderObject(GL_VERTEX_SHADER);
  a_AttachObject(spare, pending); a_DeleteObject(pending); a_DetachObject(spare, pending);
  value = 12345; a_GetObjectParameteriv(pending, GL_OBJECT_TYPE_ARB, &value); CHECK(value == 12345 && p_GetError() == GL_INVALID_VALUE);
  a_GetAttachedObjects(spare, 2, &count, attached); CHECK(!count); a_DeleteObject(spare); error_free();
  printf("ARB_PROBE_PASS bits=%u pixels=%u\n", (unsigned)(8*sizeof(void*)), cases-before);
  return 0;
}

#ifndef VEXA_ARB_PROBE_EMBEDDED
int main(void) { return arb_shader_objects_probe_main(); }
#endif
