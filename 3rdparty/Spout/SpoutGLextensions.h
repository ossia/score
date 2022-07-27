//
//			spoutGLextensions.h
//
//			Used for load of openGL extensions with options	to 
//			use Glew or disable dynamic load of specific extension types
//
//			If Glew is used, none of the extensions are loaded dynamically.
//			Individual extension types can be disabled if they conflict
//			with extensions already managed by particular applications.
//
//			NVIDIA GL/DX interop extensions
//			Fbo extensions
//			Fbo blit extensions
//			Pbo extensions
//			wglSwapInterval extensions
//
//
//			03.11.14 - added additional defines for framebuffer status checks
//			02.01.15 - added GL_BGR for SpoutCam
//			21.11.18 - added preprocessor define for Jitter externals
//					   https://github.com/robtherich/Spout2
//
//			All changes now documented in SpoutGLextensions.cpp
//
/*

	Copyright (c) 2014-2022, Lynn Jarvis. All rights reserved.

	Redistribution and use in source and binary forms, with or without modification, 
	are permitted provided that the following conditions are met:

		1. Redistributions of source code must retain the above copyright notice, 
		   this list of conditions and the following disclaimer.

		2. Redistributions in binary form must reproduce the above copyright notice, 
		   this list of conditions and the following disclaimer in the documentation 
		   and/or other materials provided with the distribution.

	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"	AND ANY 
	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE	ARE DISCLAIMED. 
	IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#pragma once
#ifndef __spoutGLextensions__	// standard way as well
#define __spoutGLextensions__


//
// Header: spoutGLextensions
//
// Used for load of openGL extensions with options to use Glew
// or disable dynamic load of specific extension types.
// If Glew is used, none of the extensions are loaded dynamically.
// Individual extension types can be disabled if they conflict
// with extensions already managed by particular applications.
//
// Refer to source code for documentation.
//

//
// ====================== COMPILE OPTIONS ============================
//

//
// Define "standalone" here to use the extensions independently of Spout source files.
// Leave undefined otherwise.
//
// #define standalone
//

#ifdef standalone

#include <windows.h>
#include <stdio.h> // for console
#include <iostream> // std::cout, std::end

//
// Define For use of 'EXT_framebuffer_object' in loadFBOextensions
// and glGetString in isExtensionSupported
// Not required unless compatibility with OpenGL < 3 is necessary
// * Note that the same definition is in SpoutCommon.h if not standalone
//
// #define legacyOpenGL
//
#else

// For use together with Spout source files
#include "SpoutCommon.h" // for legacyOpenGL define
#include "SpoutUtils.h"

using namespace spoututils;

// set this to use GLEW instead of dynamic load of extensions
// #define USE_GLEW	
// set this to use glew32s.lib instead of glew32.lib
// #define GLEW_STATIC

#endif


// If load of FBO extensions conflicts with FFGL or Jitter, disable them here
#ifndef UNDEF_USE_FBO_EXTENSIONS
#define USE_FBO_EXTENSIONS // don't use for jitter
#endif

// If load of PBO extensions conflicts, disable them here - OK for Jitter
#define USE_PBO_EXTENSIONS

// If load of COPY extensions conflicts, disable them here
// Only used for testing
#define USE_COPY_EXTENSIONS

// If load of context creation extension conflicts, disable it here
// Only used for testing
#define USE_CONTEXT_EXTENSION
// ========================= end Compile options ================================


//------------------------------------------------------------
// Allow for use of Glew instead of dynamic load of extensions
//------------------------------------------------------------
#ifdef USE_GLEW
	#include <GL\glew.h>
	#include <GL\wglew.h> // wglew.h and glxew.h, which define the available WGL and GLX extensions
#else
	#include <GL\GL.h>
	#ifndef USE_FBO_EXTENSIONS
		// For Max/Msp Jitter
		#include "jit.gl.h"
		#define glDeleteFramebuffersEXT	(_jit_gl_get_proctable()->DeleteFramebuffersEXT)
	#endif
#endif


//
// Spout compatible Log levels
//
enum LogLevel {
	LOG_SILENT,
	LOG_VERBOSE,
	LOG_NOTICE,
	LOG_WARNING,
	LOG_ERROR,
	LOG_FATAL,
	LOG_NONE,
};


//
// ====================== EXTENSIONS ============================
//


#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

#ifndef GL_READ_FRAMEBUFFER_EXT
#define GL_READ_FRAMEBUFFER_EXT 0x8CA8
#endif

#ifndef GL_DRAW_FRAMEBUFFER_EXT
#define GL_DRAW_FRAMEBUFFER_EXT 0x8CA9
#endif

#ifndef GL_DRAW_FRAMEBUFFER_BINDING_EXT
#define GL_DRAW_FRAMEBUFFER_BINDING_EXT 0x8CA6
#endif

#ifndef GL_READ_FRAMEBUFFER_BINDING_EXT
#define GL_READ_FRAMEBUFFER_BINDING_EXT 0x8CAA
#endif

#ifndef GL_INVALID_FRAMEBUFFER_OPERATION_EXT
#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT 0x0506
#endif


//------------------------
// EXTENSION SUPPORT FLAGS
//------------------------
#define GLEXT_SUPPORT_NVINTEROP		  1
#define GLEXT_SUPPORT_FBO			  2
#define GLEXT_SUPPORT_FBO_BLIT		  4
#define GLEXT_SUPPORT_PBO			  8
#define GLEXT_SUPPORT_SWAP			 16
#define GLEXT_SUPPORT_BGRA			 32
#define GLEXT_SUPPORT_COPY			 64
#define GLEXT_SUPPORT_CONTEXT       128

//-----------------------------------------------------
// GL consts that are needed and aren't present in GL.h
//-----------------------------------------------------
#define GL_TEXTURE_2D_MULTISAMPLE		0x9100
#define WGL_ACCESS_READ_ONLY_NV			0x0000
#define WGL_ACCESS_READ_WRITE_NV		0x0001
#define WGL_ACCESS_WRITE_DISCARD_NV		0x0002

#define GL_CLAMP_TO_EDGE				0x812F

// Other
#ifndef  GL_MAJOR_VERSION
#define GL_MAJOR_VERSION				0x821B
#endif
#ifndef GL_MINOR_VERSION 
#define GL_MINOR_VERSION				0x821C
#endif
#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS				0x821D
#endif
#ifndef GL_CONTEXT_FLAGS // The flags with which the context was created.
#define GL_CONTEXT_FLAGS				0x821E
#endif

#ifndef GL_CONTEXT_FLAGS
#define GL_CONTEXT_FLAGS				0x821E
#endif
#ifndef GL_CONTEXT_PROFILE_MASK
#define GL_CONTEXT_PROFILE_MASK			0x9126
#endif

#ifndef GL_CONTEXT_CORE_PROFILE_BIT
#define GL_CONTEXT_CORE_PROFILE_BIT            0x00000001
#endif
#ifndef GL_CONTEXT_COMPATIBILITY_PROFILE_BIT
#define GL_CONTEXT_COMPATIBILITY_PROFILE_BIT   0x00000002
#endif
#ifndef GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT
#define GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT 0x00000001
#endif
#ifndef GL_CONTEXT_FLAG_DEBUG_BIT
#define GL_CONTEXT_FLAG_DEBUG_BIT              0x00000002
#endif
#ifndef GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT
#define GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT      0x00000004
#endif
#ifndef GL_CONTEXT_FLAG_NO_ERROR_BIT
#define GL_CONTEXT_FLAG_NO_ERROR_BIT           0x00000008
#endif

#ifndef USE_GLEW

// ----------------------------
// Memory management extensions
// ----------------------------
#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX 0x9048
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049


//----------------------
// GL interop extensions
//----------------------
typedef HANDLE	(WINAPI * PFNWGLDXOPENDEVICENVPROC)				(void* dxDevice);
typedef BOOL	(WINAPI * PFNWGLDXCLOSEDEVICENVPROC)			(HANDLE hDevice);
typedef HANDLE	(WINAPI * PFNWGLDXREGISTEROBJECTNVPROC)			(HANDLE hDevice, void* dxObject, GLuint name, GLenum type, GLenum access);
typedef BOOL	(WINAPI * PFNWGLDXUNREGISTEROBJECTNVPROC)		(HANDLE hDevice, HANDLE hObject);
typedef BOOL	(WINAPI * PFNWGLDXSETRESOURCESHAREHANDLENVPROC)	(void *dxResource, HANDLE shareHandle);
typedef BOOL	(WINAPI * PFNWGLDXLOCKOBJECTSNVPROC)			(HANDLE hDevice, GLint count, HANDLE *hObjects);
typedef BOOL	(WINAPI * PFNWGLDXUNLOCKOBJECTSNVPROC)			(HANDLE hDevice, GLint count, HANDLE *hObjects);

extern PFNWGLDXOPENDEVICENVPROC				wglDXOpenDeviceNV;
extern PFNWGLDXCLOSEDEVICENVPROC			wglDXCloseDeviceNV;
extern PFNWGLDXREGISTEROBJECTNVPROC			wglDXRegisterObjectNV;
extern PFNWGLDXUNREGISTEROBJECTNVPROC		wglDXUnregisterObjectNV;
extern PFNWGLDXSETRESOURCESHAREHANDLENVPROC wglDXSetResourceShareHandleNV;
extern PFNWGLDXLOCKOBJECTSNVPROC			wglDXLockObjectsNV;
extern PFNWGLDXUNLOCKOBJECTSNVPROC			wglDXUnlockObjectsNV;


//---------------
// FBO extensions
//---------------
#ifdef USE_FBO_EXTENSIONS
#define GL_INVALID_FRAMEBUFFER_OPERATION_EXT                0x0506
#define GL_FRAMEBUFFER_UNDEFINED_EXT						0x8219
#define GL_MAX_RENDERBUFFER_SIZE_EXT                        0x84E8
#define GL_FRAMEBUFFER_BINDING_EXT                          0x8CA6
#define GL_RENDERBUFFER_BINDING_EXT                         0x8CA7
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT           0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT           0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT         0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT 0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT    0x8CD4
#define GL_FRAMEBUFFER_COMPLETE_EXT                         0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT            0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT    0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DUPLICATE_ATTACHMENT_EXT  0x8CD8
#define GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT            0x8CD9
#define GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT               0x8CDA
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT           0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT           0x8CDC
#define GL_FRAMEBUFFER_UNSUPPORTED_EXT                      0x8CDD
#define GL_FRAMEBUFFER_STATUS_ERROR_EXT                     0x8CDE
#define GL_MAX_COLOR_ATTACHMENTS_EXT                        0x8CDF
#define GL_COLOR_ATTACHMENT0_EXT                            0x8CE0
#define GL_COLOR_ATTACHMENT1_EXT                            0x8CE1
#define GL_COLOR_ATTACHMENT2_EXT                            0x8CE2
#define GL_COLOR_ATTACHMENT3_EXT                            0x8CE3
#define GL_COLOR_ATTACHMENT4_EXT                            0x8CE4
#define GL_COLOR_ATTACHMENT5_EXT                            0x8CE5
#define GL_COLOR_ATTACHMENT6_EXT                            0x8CE6
#define GL_COLOR_ATTACHMENT7_EXT                            0x8CE7
#define GL_COLOR_ATTACHMENT8_EXT                            0x8CE8
#define GL_COLOR_ATTACHMENT9_EXT                            0x8CE9
#define GL_COLOR_ATTACHMENT10_EXT                           0x8CEA
#define GL_COLOR_ATTACHMENT11_EXT                           0x8CEB
#define GL_COLOR_ATTACHMENT12_EXT                           0x8CEC
#define GL_COLOR_ATTACHMENT13_EXT                           0x8CED
#define GL_COLOR_ATTACHMENT14_EXT                           0x8CEE
#define GL_COLOR_ATTACHMENT15_EXT                           0x8CEF
#define GL_DEPTH_ATTACHMENT_EXT                             0x8D00
#define GL_STENCIL_ATTACHMENT_EXT                           0x8D20
#define GL_FRAMEBUFFER_EXT                                  0x8D40
#define GL_RENDERBUFFER_EXT                                 0x8D41
#define GL_RENDERBUFFER_WIDTH_EXT                           0x8D42
#define GL_RENDERBUFFER_HEIGHT_EXT                          0x8D43
#define GL_RENDERBUFFER_INTERNAL_FORMAT_EXT                 0x8D44
#define GL_STENCIL_INDEX_EXT                                0x8D45
#define GL_STENCIL_INDEX1_EXT                               0x8D46
#define GL_STENCIL_INDEX4_EXT                               0x8D47
#define GL_STENCIL_INDEX8_EXT                               0x8D48
#define GL_STENCIL_INDEX16_EXT                              0x8D49
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT			0x8D56
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS_EXT			0x8DA8

typedef void   (APIENTRY *glBindFramebufferEXTPROC)			(GLenum target, GLuint framebuffer);
typedef void   (APIENTRY *glBindRenderbufferEXTPROC)		(GLenum target, GLuint renderbuffer);
typedef GLenum (APIENTRY *glCheckFramebufferStatusEXTPROC)	(GLenum target);
typedef void   (APIENTRY *glDeleteFramebuffersEXTPROC)		(GLsizei n, const GLuint* framebuffers);
typedef void   (APIENTRY *glDeleteRenderBuffersEXTPROC)		(GLsizei n, const GLuint* renderbuffers);
typedef void   (APIENTRY *glFramebufferRenderbufferEXTPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void   (APIENTRY *glFramebufferTexture1DEXTPROC)	(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void   (APIENTRY *glFramebufferTexture2DEXTPROC)	(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void   (APIENTRY *glFramebufferTexture3DEXTPROC)	(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void   (APIENTRY *glGenFramebuffersEXTPROC)			(GLsizei n, GLuint* framebuffers);
typedef void   (APIENTRY *glGenRenderbuffersEXTPROC)		(GLsizei n, GLuint* renderbuffers);
typedef void   (APIENTRY *glGenerateMipmapEXTPROC)			(GLenum target);
typedef void   (APIENTRY *glGetFramebufferAttachmentParameterivEXTPROC) (GLenum target, GLenum attachment, GLenum pname, GLint* params);
typedef void   (APIENTRY *glGetRenderbufferParameterivEXTPROC) (GLenum target, GLenum pname, GLint* params);
typedef GLboolean (APIENTRY *glIsFramebufferEXTPROC)		(GLuint framebuffer);
typedef GLboolean (APIENTRY *glIsRenderbufferEXTPROC)		(GLuint renderbuffer);
typedef void (APIENTRY *glRenderbufferStorageEXTPROC)		(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);

extern glBindFramebufferEXTPROC						glBindFramebufferEXT;
extern glBindRenderbufferEXTPROC					glBindRenderbufferEXT;
extern glCheckFramebufferStatusEXTPROC				glCheckFramebufferStatusEXT;
extern glDeleteFramebuffersEXTPROC					glDeleteFramebuffersEXT;
extern glDeleteRenderBuffersEXTPROC					glDeleteRenderBuffersEXT;
extern glFramebufferRenderbufferEXTPROC				glFramebufferRenderbufferEXT;
extern glFramebufferTexture1DEXTPROC				glFramebufferTexture1DEXT;
extern glFramebufferTexture2DEXTPROC				glFramebufferTexture2DEXT;
extern glFramebufferTexture3DEXTPROC				glFramebufferTexture3DEXT;
extern glGenFramebuffersEXTPROC						glGenFramebuffersEXT;
extern glGenRenderbuffersEXTPROC					glGenRenderbuffersEXT;
extern glGenerateMipmapEXTPROC						glGenerateMipmapEXT;
extern glGetFramebufferAttachmentParameterivEXTPROC	glGetFramebufferAttachmentParameterivEXT;
extern glGetRenderbufferParameterivEXTPROC			glGetRenderbufferParameterivEXT;
extern glIsFramebufferEXTPROC						glIsFramebufferEXT;
extern glIsRenderbufferEXTPROC						glIsRenderbufferEXT;
extern glRenderbufferStorageEXTPROC					glRenderbufferStorageEXT;

#endif // USE_FBO_EXTENSIONS

//-------------------
// Blit FBO extension
//-------------------
#define READ_FRAMEBUFFER_EXT	0x8CA8
#define DRAW_FRAMEBUFFER_EXT	0x8CA9

typedef void   (APIENTRY *glBlitFramebufferEXTPROC) (GLint srcX0,GLint srcY0,GLint srcX1,GLint srcY1,GLint dstX0,GLint dstY0,GLint dstX1,GLint dstY1,GLbitfield mask,GLenum filter);
extern glBlitFramebufferEXTPROC glBlitFramebufferEXT;

//-------------------
// Blit FBO extension
//-------------------
#define READ_FRAMEBUFFER_EXT	0x8CA8
#define DRAW_FRAMEBUFFER_EXT	0x8CA9

typedef void   (APIENTRY *glBlitFramebufferEXTPROC) (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
extern glBlitFramebufferEXTPROC glBlitFramebufferEXT;

// ------------------------------
// OpenGL vsync control extensions
// ------------------------------
#ifdef USE_FBO_EXTENSIONS
typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
typedef int (WINAPI * PFNWGLGETSWAPINTERVALEXTPROC) (void);
extern PFNWGLSWAPINTERVALEXTPROC       wglSwapIntervalEXT;
extern PFNWGLGETSWAPINTERVALEXTPROC    wglGetSwapIntervalEXT;
#endif

//----------------
//	PBO extensions
//----------------
#ifdef USE_PBO_EXTENSIONS
#define GL_ARRAY_BUFFER					0x8892
#define GL_PIXEL_PACK_BUFFER			0x88EB
#define GL_PIXEL_UNPACK_BUFFER			0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING	0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING	0x88EF
#define GL_STREAM_DRAW					0x88E0
#define GL_STREAM_READ					0x88E1
#define GL_READ_ONLY					0x88B8
#define GL_WRITE_ONLY					0x88B9
#define GL_BUFFER_SIZE_EXT				0x8764
#ifndef GL_MAP_READ_BIT
#define GL_MAP_READ_BIT					0x0001
#endif
#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT				0x0002
#endif
#ifndef GL_MAP_PERSISTENT_BIT
#define GL_MAP_PERSISTENT_BIT			0x0040
#endif
#ifndef GL_MAP_COHERENT_BIT
#define GL_MAP_COHERENT_BIT				0x0080 
#endif
//
// Optional flag bits
//
#ifndef GL_MAP_INVALIDATE_RANGE_BIT
#define GL_MAP_INVALIDATE_RANGE_BIT		0x0004
#endif
#ifndef GL_MAP_INVALIDATE_BUFFER_BIT
#define GL_MAP_INVALIDATE_BUFFER_BIT	0x0008
#endif
#ifndef GL_MAP_FLUSH_EXPLICIT_BIT
#define GL_MAP_FLUSH_EXPLICIT_BIT		0x0010
#endif
#ifndef GL_MAP_UNSYNCHRONIZED_BIT
#define GL_MAP_UNSYNCHRONIZED_BIT		0x0020
#endif
#ifndef GL_SYNC_FLUSH_COMMANDS_BIT
#define GL_SYNC_FLUSH_COMMANDS_BIT		0x0001
#endif


//
// Sync
//
#ifndef GL_SYNC_CONDITION
#define GL_SYNC_CONDITION                 0x9113
#endif
#ifndef GL_SYNC_STATUS
#define GL_SYNC_STATUS                    0x9114
#endif
#ifndef GL_SYNC_FLAGS
#define GL_SYNC_FLAGS                     0x9115
#endif
#ifndef GL_SYNC_FENCE
#define GL_SYNC_FENCE                     0x9116
#endif
#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE     0x9117
#endif
#ifndef GL_UNSIGNALED
#define GL_UNSIGNALED                     0x9118
#endif
#ifndef GL_SIGNALED
#define GL_SIGNALED                       0x9119
#endif
#ifndef GL_ALREADY_SIGNALED
#define GL_ALREADY_SIGNALED               0x911A
#endif
#ifndef GL_TIMEOUT_EXPIRED
#define GL_TIMEOUT_EXPIRED                0x911B
#endif
#ifndef GL_CONDITION_SATISFIED
#define GL_CONDITION_SATISFIED            0x911C
#endif
#ifndef GL_WAIT_FAILED
#define GL_WAIT_FAILED                    0x911D
#endif

// ------------------------------
// PBO extensions
// ------------------------------
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;
typedef void   (APIENTRY *glGenBuffersPROC)    (GLsizei n, const GLuint* buffers);
typedef void   (APIENTRY *glDeleteBuffersPROC) (GLsizei n, const GLuint* buffers);
typedef void   (APIENTRY *glBindBufferPROC)    (GLenum target, const GLuint buffer);
typedef void   (APIENTRY *glBufferDataPROC)    (GLenum target,  GLsizeiptr size,  const GLvoid * data,  GLenum usage);
typedef void   (APIENTRY *glBufferStoragePROC) (GLenum target, GLsizeiptr size, const void * data, GLbitfield flags);
typedef void * (APIENTRY *glMapBufferPROC) (GLenum target,  GLenum access);
typedef void * (APIENTRY *glMapBufferRangePROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef void   (APIENTRY *glUnmapBufferPROC) (GLenum target);
typedef void   (APIENTRY *glGetBufferParameterivPROC) (GLenum target, GLenum value,	GLint * data);

extern glGenBuffersPROC		glGenBuffersEXT;
extern glDeleteBuffersPROC	glDeleteBuffersEXT;
extern glBindBufferPROC		glBindBufferEXT;
extern glBufferDataPROC		glBufferDataEXT;
extern glBufferStoragePROC	glBufferStorageEXT;
extern glMapBufferPROC		glMapBufferEXT;
extern glMapBufferRangePROC	glMapBufferRangeEXT;
extern glUnmapBufferPROC	glUnmapBufferEXT;
extern glGetBufferParameterivPROC glGetBufferParameterivEXT;

// ------------------------------
// SYNC objects
// https://www.khronos.org/opengl/wiki/Sync_Object
// ------------------------------
typedef struct __GLsync *GLsync;
typedef uint64_t GLuint64;
typedef GLenum(APIENTRY *glClientWaitSyncPROC) (GLsync sync, GLbitfield flags, GLuint64 timeout);
typedef void   (APIENTRY *glDeleteSyncPROC) (GLsync sync);
typedef GLsync(APIENTRY *glFenceSyncPROC) (GLenum condition, GLbitfield flags);

extern glClientWaitSyncPROC glClientWaitSyncEXT;
extern glDeleteSyncPROC     glDeleteSyncEXT;
extern glFenceSyncPROC      glFenceSyncEXT;

#endif // USE_PBO_EXTENSIONS

//-------------------
// Copy extensions
//-------------------
#ifdef USE_COPY_EXTENSIONS
typedef void (APIENTRY * PFNGLCOPYIMAGESUBDATAPROC)(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
extern PFNGLCOPYIMAGESUBDATAPROC glCopyImageSubData;

typedef void(APIENTRY * glGetInternalFormativPROC)(GLenum target, GLenum internalfrmat, GLenum pname, GLsizei buffSize, GLint *params);
extern glGetInternalFormativPROC glGetInternalFormativ;

#endif // USE_COPY_EXTENSIONS

//---------------------------
// Context creation extension
//---------------------------
typedef HGLRC (APIENTRY * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int *attribList);
extern PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

// Tokens accepted as an attribute name in <*attribList>:
#define		WGL_CONTEXT_MAJOR_VERSION_ARB           0x2091
#define		WGL_CONTEXT_MINOR_VERSION_ARB           0x2092
#define		WGL_CONTEXT_LAYER_PLANE_ARB             0x2093
#define		WGL_CONTEXT_FLAGS_ARB                   0x2094
#define		WGL_CONTEXT_PROFILE_MASK_ARB            0x9126

//	Accepted as bits in the attribute value for WGL_CONTEXT_FLAGS in <*attribList>:
#define		WGL_CONTEXT_DEBUG_BIT_ARB               0x0001
#define		WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002

//	Accepted as bits in the attribute value for
//	WGL_CONTEXT_PROFILE_MASK_ARB in <*attribList>:
#define		WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001
#define		WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

//	New errors returned by GetLastError:
#define		ERROR_INVALID_VERSION_ARB               0x2095
#define		ERROR_INVALID_PROFILE_ARB               0x2096

#endif // end GLEW

//----------------
// Local functions
//----------------
bool InitializeGlew();
unsigned int loadGLextensions();
bool loadInteropExtensions();
bool loadFBOextensions();
bool loadBLITextension();
bool loadSwapExtensions();
bool loadPBOextensions();
bool loadCopyExtensions();
bool loadContextExtension();
bool isExtensionSupported(const char *extension);
void ExtLog(LogLevel level, const char* format, ...);

#endif // end __spoutGLextensions__
