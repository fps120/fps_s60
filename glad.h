#ifndef GLAD_H
#define GLAD_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>
#include <stdint.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>
typedef ptrdiff_t  GLsizeiptr;
typedef ptrdiff_t  GLintptr;
typedef char       GLchar;
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER          0x8892
#define GL_ELEMENT_ARRAY_BUFFER  0x8893
#define GL_STATIC_DRAW           0x88B4
#define GL_DYNAMIC_DRAW          0x88E8
#define GL_FRAGMENT_SHADER       0x8B30
#define GL_VERTEX_SHADER         0x8B31
#define GL_COMPILE_STATUS        0x8B81
#define GL_LINK_STATUS           0x8B82
#define GL_INFO_LOG_LENGTH       0x8B84
#define GL_TEXTURE0              0x84C0
#define GL_CLAMP_TO_EDGE         0x812F
#define GL_MULTISAMPLE           0x809D
#endif
typedef void  (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei,GLuint*);
typedef void  (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum,GLuint);
typedef void  (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum,GLsizeiptr,const void*,GLenum);
typedef void  (APIENTRY *PFNGLDELETEBUFFERSPROC)(GLsizei,const GLuint*);
typedef void  (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei,GLuint*);
typedef void  (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint);
typedef void  (APIENTRY *PFNGLDELETEVERTEXARRAYSPROC)(GLsizei,const GLuint*);
typedef void  (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint);
typedef void  (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint,GLint,GLenum,GLboolean,GLsizei,const void*);
typedef GLuint(APIENTRY *PFNGLCREATESHADERPROC)(GLenum);
typedef void  (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint,GLsizei,const GLchar**,const GLint*);
typedef void  (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint);
typedef void  (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint,GLenum,GLint*);
typedef void  (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint,GLsizei,GLsizei*,GLchar*);
typedef void  (APIENTRY *PFNGLDELETESHADERPROC)(GLuint);
typedef GLuint(APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void  (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint,GLuint);
typedef void  (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint);
typedef void  (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint,GLenum,GLint*);
typedef void  (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint,GLsizei,GLsizei*,GLchar*);
typedef void  (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint);
typedef void  (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint,const GLchar*);
typedef void  (APIENTRY *PFNGLUNIFORM1IPROC)(GLint,GLint);
typedef void  (APIENTRY *PFNGLUNIFORM1FPROC)(GLint,GLfloat);
typedef void  (APIENTRY *PFNGLUNIFORM3FPROC)(GLint,GLfloat,GLfloat,GLfloat);
typedef void  (APIENTRY *PFNGLUNIFORM4FPROC)(GLint,GLfloat,GLfloat,GLfloat,GLfloat);
typedef void  (APIENTRY *PFNGLUNIFORMMATRIX4FVPROC)(GLint,GLsizei,GLboolean,const GLfloat*);
typedef void  (APIENTRY *PFNGLACTIVETEXTUREPROC)(GLenum);
typedef void  (APIENTRY *PFNGLGENERATEMIPMAPPROC)(GLenum);
typedef void  (APIENTRY *PFNGLBUFFERSUBDATAPROC)(GLenum,GLintptr,GLsizeiptr,const void*);
extern PFNGLGENBUFFERSPROC              glGenBuffers;
extern PFNGLBINDBUFFERPROC              glBindBuffer;
extern PFNGLBUFFERDATAPROC              glBufferData;
extern PFNGLDELETEBUFFERSPROC           glDeleteBuffers;
extern PFNGLGENVERTEXARRAYSPROC         glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC         glBindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC      glDeleteVertexArrays;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC     glVertexAttribPointer;
extern PFNGLCREATESHADERPROC            glCreateShader;
extern PFNGLSHADERSOURCEPROC            glShaderSource;
extern PFNGLCOMPILESHADERPROC           glCompileShader;
extern PFNGLGETSHADERIVPROC             glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC        glGetShaderInfoLog;
extern PFNGLDELETESHADERPROC            glDeleteShader;
extern PFNGLCREATEPROGRAMPROC           glCreateProgram;
extern PFNGLATTACHSHADERPROC            glAttachShader;
extern PFNGLLINKPROGRAMPROC             glLinkProgram;
extern PFNGLGETPROGRAMIVPROC            glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC       glGetProgramInfoLog;
extern PFNGLUSEPROGRAMPROC              glUseProgram;
extern PFNGLDELETEPROGRAMPROC           glDeleteProgram;
extern PFNGLGETUNIFORMLOCATIONPROC      glGetUniformLocation;
extern PFNGLUNIFORM1IPROC               glUniform1i;
extern PFNGLUNIFORM1FPROC               glUniform1f;
extern PFNGLUNIFORM3FPROC               glUniform3f;
extern PFNGLUNIFORM4FPROC               glUniform4f;
extern PFNGLUNIFORMMATRIX4FVPROC        glUniformMatrix4fv;
extern PFNGLACTIVETEXTUREPROC           glActiveTexture;
extern PFNGLGENERATEMIPMAPPROC          glGenerateMipmap;
extern PFNGLBUFFERSUBDATAPROC           glBufferSubData;
int gladLoadGL(void);
#ifdef __cplusplus
}
#endif
#endif
