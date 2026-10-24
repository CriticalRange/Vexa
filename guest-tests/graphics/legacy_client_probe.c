#define VEXA_CLIP_PROBE_EMBEDDED
#include "clip_plane_probe.c"
int main(void) {
  CHECK(clip_plane_probe_main()==0);unsigned before=cases;puts("LEGACY_PROBE_START");
  void* lib=dlopen("libGL.so.1",RTLD_NOW|RTLD_LOCAL);void*(*getproc)(const GLubyte*)=dlsym(lib,"glXGetProcAddress");CHECK(getproc);
  CLIP_FN(GetString,const GLubyte*,(GLenum));CLIP_FN(GetStringi,const GLubyte*,(GLenum,GLuint));
  const char* extensions=(const char*)c_GetString(GL_EXTENSIONS);
  const char* required[]={"GL_ARB_vertex_buffer_object","GL_EXT_draw_buffers2"};
  GLint extension_count=0;p_GetIntegerv(GL_NUM_EXTENSIONS,&extension_count);
  for(unsigned n=0;n<2;++n){CHECK(extensions&&strstr(extensions,required[n]));int found=0;
    for(GLint i=0;i<extension_count;++i){const char* name=(const char*)c_GetStringi(GL_EXTENSIONS,i);if(name&&!strcmp(name,required[n]))found=1;}CHECK(found);}
  CLIP_FN(VertexPointer,void,(GLint,GLenum,GLsizei,const void*));CLIP_FN(ColorPointer,void,(GLint,GLenum,GLsizei,const void*));CLIP_FN(TexCoordPointer,void,(GLint,GLenum,GLsizei,const void*));
  CLIP_FN(EnableClientState,void,(GLenum));CLIP_FN(DisableClientState,void,(GLenum));CLIP_FN(ClientActiveTexture,void,(GLenum));
  CLIP_FN(Color4f,void,(GLfloat,GLfloat,GLfloat,GLfloat));CLIP_FN(Color4sv,void,(const GLshort*));CLIP_FN(TexCoord2f,void,(GLfloat,GLfloat));CLIP_FN(GetPointerv,void,(GLenum,void**));
  CLIP_FN(PushAttrib,void,(GLbitfield));CLIP_FN(PopAttrib,void,(void));CLIP_FN(PushClientAttrib,void,(GLbitfield));CLIP_FN(PopClientAttrib,void,(void));
  CLIP_FN(PixelStorei,void,(GLenum,GLint));CLIP_FN(Uniform1i,void,(GLint,GLint));CLIP_FN(DrawElements,void,(GLenum,GLsizei,GLenum,const void*));
  CLIP_FN(Scissor,void,(GLint,GLint,GLsizei,GLsizei));CLIP_FN(MatrixMode,void,(GLenum));CLIP_FN(LoadIdentity,void,(void));CLIP_FN(ClipPlane,void,(GLenum,const GLdouble*));
  for(int i=0;i<6;++i)p_Disable(GL_CLIP_PLANE0+i);p_Disable(GL_ALPHA_TEST);p_Disable(GL_DEPTH_TEST);p_Disable(GL_STENCIL_TEST);p_Disable(GL_BLEND);
  const char* vertex="#version 120\nattribute vec2 pos;attribute vec4 col;attribute vec2 uv;varying highp vec4 vColor;centroid varying highp vec2 vUV;void main(){gl_Position=vec4(pos,0.,1.);vColor=col;vUV=uv;}";
  const char* fragment="#version 120\nprecision highp float;varying highp vec4 vColor;centroid varying highp vec2 vUV;uniform bool useTex;uniform sampler2D image;void main(){gl_FragData[0]=useTex?texture2D(image,vUV)*vColor:vColor;}";
  GLuint v=shader(GL_VERTEX_SHADER,vertex),f=shader(GL_FRAGMENT_SHADER,fragment);program=p_CreateProgram();p_AttachShader(program,v);p_AttachShader(program,f);p_BindAttribLocation(program,0,"pos");p_BindAttribLocation(program,3,"col");p_BindAttribLocation(program,8,"uv");p_LinkProgram(program);GLint value=0;p_GetProgramiv(program,GL_LINK_STATUS,&value);CHECK(value);p_UseProgram(program);
  const GLfloat triangle[]={-1,-1,3,-1,-1,3};p_BindBuffer(GL_ARRAY_BUFFER,0);c_VertexPointer(2,GL_FLOAT,0,triangle);c_EnableClientState(GL_VERTEX_ARRAY);c_DisableClientState(GL_COLOR_ARRAY);c_Color4f(1,0,0,1);blue();draw();pixel(255,0,0);
  struct{void* ptr;unsigned guard;} pointer={0,0xdeadbeef};c_GetPointerv(GL_VERTEX_ARRAY_POINTER,&pointer.ptr);CHECK(pointer.ptr==triangle&&pointer.guard==0xdeadbeef);
  CHECK(p_IsEnabled(GL_VERTEX_ARRAY));p_GetIntegerv(GL_VERTEX_ARRAY_SIZE,&value);CHECK(value==2);p_GetIntegerv(GL_VERTEX_ARRAY_BUFFER_BINDING,&value);CHECK(!value);
  GLshort green[]={0,32767,0,32767};c_Color4sv(green);blue();draw();pixel(0,255,0);
  GLubyte colors[]={0,0,255,255,0,0,255,255,0,0,255,255};c_ColorPointer(4,GL_UNSIGNED_BYTE,0,colors);c_EnableClientState(GL_COLOR_ARRAY);blue();draw();pixel(0,0,255);
  GLushort indices[]={2,1,0};p_BindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);blue();c_DrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,indices);pixel(0,0,255);p_GetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&value);CHECK(!value);
  GLuint buffer;p_GenBuffers(1,&buffer);p_BindBuffer(GL_ARRAY_BUFFER,buffer);p_BufferData(GL_ARRAY_BUFFER,sizeof(triangle),triangle,GL_STATIC_DRAW);c_VertexPointer(2,GL_FLOAT,0,0);c_DisableClientState(GL_COLOR_ARRAY);c_Color4f(1,0,0,1);blue();draw();pixel(255,0,0);
  // WRITE_ONLY does not invalidate untouched bytes. Source updates subranges
  // of a whole mapped VBO; the i386 shadow mapping must retain the rest.
  CLIP_FN(MapBufferARB,void*,(GLenum,GLenum));CLIP_FN(UnmapBufferARB,GLboolean,(GLenum));
  GLfloat* mapped=c_MapBufferARB(GL_ARRAY_BUFFER,GL_WRITE_ONLY);CHECK(mapped);mapped[5]=3;CHECK(c_UnmapBufferARB(GL_ARRAY_BUFFER));blue();draw();pixel(255,0,0);
  const GLdouble doubles[]={-1,-1,3,-1,-1,3};p_BindBuffer(GL_ARRAY_BUFFER,0);c_VertexPointer(2,GL_DOUBLE,0,doubles);c_Color4f(0,1,0,1);blue();draw();pixel(0,255,0);
  p_GetIntegerv(GL_VERTEX_ARRAY_TYPE,&value);CHECK(value==GL_DOUBLE);
  GLfloat uv[]={.5,.5,.5,.5,.5,.5};c_ClientActiveTexture(GL_TEXTURE0);c_TexCoordPointer(2,GL_FLOAT,0,uv);c_EnableClientState(GL_TEXTURE_COORD_ARRAY);c_Color4f(1,1,1,1);
  GLuint texture;p_GenTextures(1,&texture);p_BindTexture(GL_TEXTURE_2D,texture);GLubyte yellow[]={255,255,0,255};p_TexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,yellow);
  c_Uniform1i(p_GetUniformLocation(program,"image"),0);c_Uniform1i(p_GetUniformLocation(program,"useTex"),1);blue();draw();pixel(255,255,0);c_Uniform1i(p_GetUniformLocation(program,"useTex"),0);
  c_ClientActiveTexture(GL_TEXTURE1);p_GetIntegerv(GL_CLIENT_ACTIVE_TEXTURE,&value);CHECK(value==GL_TEXTURE1);c_ClientActiveTexture(GL_TEXTURE0);
  // Client stack restores CPU pointers, vertex enables, active client unit and pixel storage.
  c_PixelStorei(GL_UNPACK_ALIGNMENT,1);c_PushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);c_DisableClientState(GL_VERTEX_ARRAY);c_VertexPointer(2,GL_FLOAT,0,triangle);c_ClientActiveTexture(GL_TEXTURE1);c_PixelStorei(GL_UNPACK_ALIGNMENT,8);c_PopClientAttrib();
  CHECK(p_IsEnabled(GL_VERTEX_ARRAY));c_GetPointerv(GL_VERTEX_ARRAY_POINTER,&pointer.ptr);CHECK(pointer.ptr==doubles);p_GetIntegerv(GL_CLIENT_ACTIVE_TEXTURE,&value);CHECK(value==GL_TEXTURE0);p_GetIntegerv(GL_UNPACK_ALIGNMENT,&value);CHECK(value==1);c_Color4f(1,0,0,1);blue();draw();pixel(255,0,0);
  // Server stack restores current color, clipping, alpha/depth/blend, viewport and scissor.
  c_MatrixMode(GL_MODELVIEW);c_LoadIdentity();p_Disable(GL_CLIP_PLANE0);p_Disable(GL_ALPHA_TEST);c_Color4f(1,0,0,1);p_Viewport(0,0,8,8);p_Disable(GL_SCISSOR_TEST);
  c_PushAttrib(GL_ALL_ATTRIB_BITS);c_Color4f(0,1,0,1);p_Enable(GL_ALPHA_TEST);p_AlphaFunc(GL_NEVER,.5);p_Enable(GL_DEPTH_TEST);p_DepthFunc(GL_ALWAYS);p_DepthMask(GL_FALSE);p_Viewport(0,0,2,2);p_Enable(GL_SCISSOR_TEST);c_Scissor(0,0,1,1);
  GLdouble reject[]={0,0,0,-1};c_ClipPlane(GL_CLIP_PLANE0,reject);p_Enable(GL_CLIP_PLANE0);c_PopAttrib();CHECK(!p_IsEnabled(GL_ALPHA_TEST)&&!p_IsEnabled(GL_CLIP_PLANE0)&&!p_IsEnabled(GL_DEPTH_TEST)&&!p_IsEnabled(GL_SCISSOR_TEST));blue();draw();at(6,6,255,0,0);
  c_PopAttrib();CHECK(p_GetError()==GL_STACK_UNDERFLOW);c_PopClientAttrib();CHECK(p_GetError()==GL_STACK_UNDERFLOW);
  c_EnableClientState(0xffffffff);CHECK(p_GetError()==GL_INVALID_ENUM);c_VertexPointer(1,GL_FLOAT,0,triangle);CHECK(p_GetError()==GL_INVALID_VALUE);c_ClientActiveTexture(GL_TEXTURE0+8);CHECK(p_GetError()==GL_INVALID_ENUM);
  // Paired ARB query aliases and bulk env updates are real API operations.
  ARB(GenQueries,PFNGLGENQUERIESARBPROC);ARB(DeleteQueries,PFNGLDELETEQUERIESARBPROC);GLuint queries[2]={0};a_GenQueries(2,queries);CHECK(queries[0]&&queries[1]&&queries[0]!=queries[1]);a_DeleteQueries(2,queries);
  void(*bulk)(GLenum,GLuint,GLsizei,const GLfloat*)=getproc((const GLubyte*)"glProgramEnvParameters4fvEXT");CHECK(bulk);
  ARB(GetProgramEnvParameterfv,PFNGLGETPROGRAMENVPARAMETERFVARBPROC);GLfloat data[]={1,2,3,4,5,6,7,8},out[4]={0};bulk(GL_FRAGMENT_PROGRAM_ARB,126,2,data);a_GetProgramEnvParameterfv(GL_FRAGMENT_PROGRAM_ARB,127,out);CHECK(!memcmp(out,data+4,sizeof(out)));bulk(GL_FRAGMENT_PROGRAM_ARB,126,3,data);CHECK(p_GetError()==GL_INVALID_VALUE);error_free();
  CLIP_FN(ColorMaskIndexedEXT,void,(GLuint,GLboolean,GLboolean,GLboolean,GLboolean));
  CLIP_FN(GetBooleanIndexedvEXT,void,(GLenum,GLuint,GLboolean*));CLIP_FN(EnableIndexedEXT,void,(GLenum,GLuint));CLIP_FN(DisableIndexedEXT,void,(GLenum,GLuint));CLIP_FN(IsEnabledIndexedEXT,GLboolean,(GLenum,GLuint));
  c_EnableIndexedEXT(GL_BLEND,0);CHECK(c_IsEnabledIndexedEXT(GL_BLEND,0));c_DisableIndexedEXT(GL_BLEND,0);CHECK(!c_IsEnabledIndexedEXT(GL_BLEND,0));
  blue();c_ColorMaskIndexedEXT(0,1,0,0,1);GLboolean channels[4]={0};c_GetBooleanIndexedvEXT(GL_COLOR_WRITEMASK,0,channels);CHECK(channels[0]&&!channels[1]&&!channels[2]&&channels[3]);draw();pixel(255,0,255);c_ColorMaskIndexedEXT(0,1,1,1,1);error_free();
  CLIP_FN(VertexAttribPointer,void,(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*));CLIP_FN(EnableVertexAttribArray,void,(GLuint));CLIP_FN(DisableVertexAttribArray,void,(GLuint));
  GLuint bgraBuffer;p_GenBuffers(1,&bgraBuffer);p_BindBuffer(GL_ARRAY_BUFFER,bgraBuffer);
  GLubyte bgra[]={0,0,255,255,0,0,255,255,0,0,255,255};p_BufferData(GL_ARRAY_BUFFER,sizeof(bgra),bgra,GL_STATIC_DRAW);
  c_VertexAttribPointer(3,GL_BGRA,GL_UNSIGNED_BYTE,GL_TRUE,0,0);c_EnableVertexAttribArray(3);blue();draw();pixel(255,0,0);
  CLIP_FN(GetVertexAttribiv,void,(GLuint,GLenum,GLint*));c_GetVertexAttribiv(3,GL_VERTEX_ATTRIB_ARRAY_SIZE,&value);CHECK(value==GL_BGRA);
  c_VertexAttribPointer(3,GL_BGRA,GL_UNSIGNED_BYTE,GL_FALSE,0,0);CHECK(p_GetError()==GL_INVALID_OPERATION);c_GetVertexAttribiv(3,GL_VERTEX_ATTRIB_ARRAY_SIZE,&value);CHECK(value==GL_BGRA);
  GLushort reversed[]={2,1,0};p_BindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);blue();c_DrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,reversed);pixel(255,0,0);
  GLuint packedColors[]={0xC00003FFu,0xC00003FFu,0xC00003FFu};p_BufferData(GL_ARRAY_BUFFER,sizeof(packedColors),packedColors,GL_STATIC_DRAW);
  c_VertexAttribPointer(3,GL_BGRA,GL_UNSIGNED_INT_2_10_10_10_REV,GL_TRUE,0,0);blue();draw();pixel(0,0,255);
  GLuint signedColors[]={0x400001FFu,0x400001FFu,0x400001FFu};p_BufferData(GL_ARRAY_BUFFER,sizeof(signedColors),signedColors,GL_STATIC_DRAW);c_VertexAttribPointer(3,GL_BGRA,GL_INT_2_10_10_10_REV,GL_TRUE,0,0);blue();draw();pixel(0,0,255);
  // Switching between BGRA arrays, conventional RGBA and constants must reset
  // the shader-side channel permutation, including indexed buffer updates.
  p_BufferData(GL_ARRAY_BUFFER,sizeof(bgra),bgra,GL_STREAM_DRAW);
  c_VertexAttribPointer(3,GL_BGRA,GL_UNSIGNED_BYTE,GL_TRUE,0,0);blue();draw();pixel(255,0,0);
  CLIP_FN(BufferSubData,void,(GLenum,GLintptr,GLsizeiptr,const void*));
  GLubyte greenBGRAColors[]={0,255,0,255,0,255,0,255,0,255,0,255};
  c_BufferSubData(GL_ARRAY_BUFFER,0,sizeof(greenBGRAColors),greenBGRAColors);blue();draw();pixel(0,255,0);
  c_BufferSubData(GL_ARRAY_BUFFER,0,sizeof(bgra),bgra);blue();draw();pixel(255,0,0);
  c_VertexAttribPointer(3,4,GL_UNSIGNED_BYTE,GL_TRUE,0,0);blue();draw();pixel(0,0,255);
  c_VertexAttribPointer(3,GL_BGRA,GL_UNSIGNED_BYTE,GL_TRUE,0,0);blue();draw();pixel(255,0,0);
  c_DisableVertexAttribArray(3);c_Color4f(0,1,0,1);blue();draw();pixel(0,255,0);
  c_EnableVertexAttribArray(3);blue();draw();pixel(255,0,0);
  GLuint colorIndices;p_GenBuffers(1,&colorIndices);p_BindBuffer(GL_ELEMENT_ARRAY_BUFFER,colorIndices);
  p_BufferData(GL_ELEMENT_ARRAY_BUFFER,sizeof(indices),indices,GL_STATIC_DRAW);blue();c_DrawElements(GL_TRIANGLES,3,GL_UNSIGNED_SHORT,0);pixel(255,0,0);p_BindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
  // A partial upload must retain untouched vertices. All vertices stay red.
  const GLubyte redVertex[]={0,0,255,255};c_BufferSubData(GL_ARRAY_BUFFER,4,4,redVertex);blue();draw();pixel(255,0,0);
  // A vec3 shader input cannot swizzle the missing blue component natively;
  // this program must retain the CPU fallback and the desktop BGRA result.
  GLuint rgbaProgram=program;
  GLuint rgbVS=shader(GL_VERTEX_SHADER,"#version 120\nattribute vec2 pos;attribute vec3 col;varying vec3 rgb;void main(){gl_Position=vec4(pos,0.,1.);rgb=col;}");
  GLuint rgbFS=shader(GL_FRAGMENT_SHADER,"#version 120\nvarying vec3 rgb;void main(){gl_FragColor=vec4(rgb,1.);}");
  program=p_CreateProgram();p_AttachShader(program,rgbVS);p_AttachShader(program,rgbFS);p_BindAttribLocation(program,0,"pos");p_BindAttribLocation(program,3,"col");p_LinkProgram(program);p_GetProgramiv(program,GL_LINK_STATUS,&value);CHECK(value);p_UseProgram(program);blue();draw();pixel(255,0,0);
  program=rgbaProgram;p_UseProgram(program);blue();draw();pixel(255,0,0);
  c_DisableVertexAttribArray(3);p_BindBuffer(GL_ARRAY_BUFFER,0);error_free();
  // ToGL stage-marker comments, centroid/MRT above, and fixed-function MVP.
  v=shader(GL_VERTEX_SHADER,"//GLSLvp\n#version 120\nvoid main(){gl_Position=ftransform();}");
  f=shader(GL_FRAGMENT_SHADER,"//GLSLfp\n#version 120\nvoid main(){gl_FragColor=vec4(1.,0.,0.,1.);}");
  GLuint fixed=p_CreateProgram();p_AttachShader(fixed,v);p_AttachShader(fixed,f);p_LinkProgram(fixed);p_GetProgramiv(fixed,GL_LINK_STATUS,&value);CHECK(value);p_UseProgram(fixed);
  c_MatrixMode(GL_PROJECTION);c_LoadIdentity();c_MatrixMode(GL_MODELVIEW);c_LoadIdentity();blue();draw();pixel(255,0,0);
  CLIP_FN(Translatef,void,(GLfloat,GLfloat,GLfloat));c_Translatef(4,0,0);blue();draw();pixel(0,0,255);c_LoadIdentity();
  shader(GL_FRAGMENT_SHADER,"//GLSLfp\n#version 120\nuniform sampler3D image;varying vec3 uv;void main(){gl_FragColor=texture3D(image,uv);}");error_free();
  f=shader(GL_FRAGMENT_SHADER,"#version 120\nuniform sampler2D tex;void main(){vec4 t=texture2D(tex,vec2(.5));gl_FragColor=vec4(t.aaa,1.);}");
  GLuint font=p_CreateProgram();p_AttachShader(font,v);p_AttachShader(font,f);p_LinkProgram(font);p_GetProgramiv(font,GL_LINK_STATUS,&value);CHECK(value);p_UseProgram(font);c_Uniform1i(p_GetUniformLocation(font,"tex"),0);
  GLubyte opacity=128;p_TexImage2D(GL_TEXTURE_2D,0,0x803C,1,1,0,GL_ALPHA,GL_UNSIGNED_BYTE,&opacity);error_free();blue();draw();pixel(128,128,128);
  f=shader(GL_FRAGMENT_SHADER,"#version 120\nuniform sampler2D tex;void main(){gl_FragColor=texture2D(tex,vec2(.5));}");
  GLuint packedTex=p_CreateProgram();p_AttachShader(packedTex,v);p_AttachShader(packedTex,f);p_LinkProgram(packedTex);p_GetProgramiv(packedTex,GL_LINK_STATUS,&value);CHECK(value);p_UseProgram(packedTex);c_Uniform1i(p_GetUniformLocation(packedTex,"tex"),0);
  GLuint redBGRA=0xFFFF0000u;p_TexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,1,1,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,&redBGRA);error_free();blue();draw();pixel(255,0,0);
  CLIP_FN(TexSubImage2D,void,(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const void*));
  GLuint greenBGRA=0xFF00FF00u;c_TexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,&greenBGRA);error_free();blue();draw();pixel(0,255,0);
  GLuint redPacked=0x0000FFFFu;p_TexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,1,1,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8,&redPacked);error_free();blue();draw();pixel(255,0,0);
  c_PixelStorei(0x0CF0,1);p_TexImage2D(GL_TEXTURE_2D,0,GL_SRGB8_ALPHA8,1,1,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,&redPacked);c_PixelStorei(0x0CF0,0);error_free();blue();draw();pixel(255,0,0);
  p_TexImage2D(GL_TEXTURE_2D,0,GL_SRGB8,1,1,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,&redBGRA);error_free();blue();draw();pixel(255,0,0);
  c_TexSubImage2D(GL_TEXTURE_2D,0,0,0,1,1,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,&greenBGRA);error_free();blue();draw();pixel(0,255,0);
  CLIP_FN(TexImage3D,void,(GLenum,GLint,GLint,GLsizei,GLsizei,GLsizei,GLint,GLenum,GLenum,const void*));CLIP_FN(TexSubImage3D,void,(GLenum,GLint,GLint,GLint,GLint,GLsizei,GLsizei,GLsizei,GLenum,GLenum,const void*));CLIP_FN(Uniform1f,void,(GLint,GLfloat));CLIP_FN(TexParameteri,void,(GLenum,GLenum,GLint));
  f=shader(GL_FRAGMENT_SHADER,"#version 120\nuniform sampler3D tex;uniform float layer;void main(){gl_FragColor=texture3D(tex,vec3(.5,.5,layer));}");
  GLuint volume=p_CreateProgram();p_AttachShader(volume,v);p_AttachShader(volume,f);p_LinkProgram(volume);p_GetProgramiv(volume,GL_LINK_STATUS,&value);CHECK(value);p_UseProgram(volume);c_Uniform1i(p_GetUniformLocation(volume,"tex"),0);c_Uniform1f(p_GetUniformLocation(volume,"layer"),.25);
  GLuint volumeTexture;p_GenTextures(1,&volumeTexture);p_BindTexture(GL_TEXTURE_3D,volumeTexture);c_TexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);c_TexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  GLuint layers[]={0xFFFF0000u,0xFF0000FFu};c_TexImage3D(GL_TEXTURE_3D,0,GL_RGBA8,1,1,2,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,layers);error_free();blue();draw();pixel(255,0,0);
  c_TexSubImage3D(GL_TEXTURE_3D,0,0,0,1,1,1,1,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,&greenBGRA);c_Uniform1f(p_GetUniformLocation(volume,"layer"),.75);error_free();blue();draw();pixel(0,255,0);
  GLuint pbo;p_GenBuffers(1,&pbo);p_BindBuffer(GL_PIXEL_UNPACK_BUFFER,pbo);GLuint pboLayers[]={greenBGRA,redBGRA};p_BufferData(GL_PIXEL_UNPACK_BUFFER,sizeof(pboLayers),pboLayers,GL_STATIC_DRAW);c_TexImage3D(GL_TEXTURE_3D,0,GL_RGBA8,1,1,2,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,0);p_BindBuffer(GL_PIXEL_UNPACK_BUFFER,0);c_Uniform1f(p_GetUniformLocation(volume,"layer"),.25);error_free();blue();draw();pixel(0,255,0);
  c_TexImage3D(GL_TEXTURE_3D,0,GL_RGB8,1,1,2,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,layers);c_Uniform1f(p_GetUniformLocation(volume,"layer"),.25);error_free();blue();draw();pixel(255,0,0);
  c_TexSubImage3D(GL_TEXTURE_3D,0,0,0,1,1,1,1,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,&greenBGRA);c_Uniform1f(p_GetUniformLocation(volume,"layer"),.75);error_free();blue();draw();pixel(0,255,0);
  printf("LEGACY_PROBE_PASS bits=%u pixels=%u\n",(unsigned)(8*sizeof(void*)),cases-before);return 0;
}
