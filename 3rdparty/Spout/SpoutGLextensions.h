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
//			19.04.23 - correct glGenBuffersPROC and glUnmapBufferPROC
//					   to match with Glew.h
//			18.01.25 - Rename "standalone" to "StandAloneExtensions"
//					   to avoid naming conflicts
//			26.10.25 - Fix opengl header name #130 (change GL.h to gl.h)
//
//			All changes now documented in SpoutGLextensions.cpp
//
/*

	Copyright (c) 2014-2025, Lynn Jarvis. All rights reserved.

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
// Define "standaloneExtensions" here to use
// the extensions independently of Spout source files.
// Leave undefined otherwise.
//
// #define standaloneExtensions
//

#ifdef standaloneExtensions

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
#include "SpoutCommon.h" // for legacyOpenGL define and Utils
#include <stdint.h> // for _uint32 etc

// ===================== GLEW ======================
// set this to use GLEW instead of dynamic load of extensions
// #define USE_GLEW	
// set this to use glew32s.lib instead of glew32.lib
// #define GLEW_STATIC
// =================================================
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

// GL memory extensions
#define USE_GLMEMORY_EXTENSIONS

// Compute shader extensions
// Remove for Processing library build (JSpoutLib)
#define USE_COMPUTE_EXTENSIONS

// If load of context creation extension conflicts, disable it here
// Only used for testing
#define USE_CONTEXT_EXTENSION
// ========================= end Compile options ================================


//------------------------------------------------------------
// Allow for use of Glew instead of dynamic load of extensions
//------------------------------------------------------------
#ifdef USE_GLEW
	#include <GL/glew.h>
	#include <GL/wglew.h> // wglew.h and glxew.h, which define the available WGL and GLX extensions
#else
	#include <GL/gl.h>
	#include <cstdint> // For MingW
	#ifndef USE_FBO_EXTENSIONS
		// For Max/Msp Jitter
		#include "jit.gl.h"
		#define glDeleteFramebuffersEXT	(_jit_gl_get_proctable()->DeleteFramebuffersEXT)
	#endif
#endif


//
// Spout compatible Log levels
//
enum ExtLogLevel {
	SPOUT_EXT_LOG_SILENT,
	SPOUT_EXT_LOG_VERBOSE,
	SPOUT_EXT_LOG_NOTICE,
	SPOUT_EXT_LOG_WARNING,
	SPOUT_EXT_LOG_ERROR,
	SPOUT_EXT_LOG_FATAL,
	SPOUT_EXT_LOG_NONE
};


//
// ====================== EXTENSIONS ============================
//


#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif

// FRAMEBUFFER
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER 0x8CA8
#endif

#ifndef GL_DRAW_FRAMEBUFFER
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#endif


// FRAMEBUFFER EXT
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

/*
		GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX
		GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
		GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
		GPU_MEMORY_INFO_EVICTION_COUNT_NVX
		GPU_MEMORY_INFO_EVICTED_MEMORY_NVX
*/

// dedicated video memory, total size (in kb) of the GPU memory
#ifndef GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX
#define GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX 0x9047
#endif

#ifndef GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX
#define GL_GPU_MEM_INFO_TOTAL_AVAILABLE_MEM_NVX 0x9048
#endif

#ifndef GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX
#define GL_GPU_MEM_INFO_CURRENT_AVAILABLE_MEM_NVX 0x9049
#endif

#ifndef GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
#define GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX 0x9049
#endif

#ifndef GPU_MEMORY_INFO_EVICTION_COUNT_NVX
#define GPU_MEMORY_INFO_EVICTION_COUNT_NVX 0x904A
#endif

#ifndef GPU_MEMORY_INFO_EVICTED_MEMORY_NVX
#define GPU_MEMORY_INFO_EVICTED_MEMORY_NVX 0x904B
#endif

//
// ATI
// https://registry.khronos.org/OpenGL/extensions/ATI/ATI_meminfo.txt
//
// Information on the current memory resources available can be queried
// by specifying VBO_FREE_MEMORY_ATI, TEXTURE_FREE_MEMORY_ATI, or
// RENDERBUFFER_FREE_MEMORY_ATI as the value parameter to  GetIntergerv.
// These return the memory status for pools of memory used for vertex
// buffer objects, textures, and render buffers respectively.
//
// param[0] - total memory free in the pool
// param[1] - largest available free block in the pool
// param[2] - total auxiliary memory free
// param[3] - largest auxiliary free block
//
#ifndef VBO_FREE_MEMORY_ATI
#define VBO_FREE_MEMORY_ATI            0x87FB
#endif

#ifndef TEXTURE_FREE_MEMORY_ATI
#define TEXTURE_FREE_MEMORY_ATI        0x87FC
#endif

#ifndef RENDERBUFFER_FREE_MEMORY_ATI
#define RENDERBUFFER_FREE_MEMORY_ATI   0x87FD
#endif

// glext.h
#define GL_TEXTURE_TARGET              0x1006

// glext_1.h
#define GL_TEXTURE_DEPTH               0x8071
#define GL_TEXTURE_BUFFER_OFFSET       0x919D
#define GL_TEXTURE_BUFFER_SIZE         0x919E


// Define GL_BGRA in case it is used
#ifndef GL_BGRA
#define GL_BGRA                        0x80E1
#endif
#ifndef GL_BGRA8
#define GL_BGRA8                       0x93A1
#endif
#ifndef GL_BGRA8_EXT
#define GL_BGRA8_EXT                   0x93A1
#endif


// RGBA <> BGRA
#ifndef GL_TEXTURE_SWIZZLE_RGBA
#define GL_TEXTURE_SWIZZLE_RGBA        0x8E46
#endif
#ifndef GL_TEXTURE_SWIZZLE_R
#define GL_TEXTURE_SWIZZLE_R           0x8E42
#endif
#ifndef GL_TEXTURE_SWIZZLE_G
#define GL_TEXTURE_SWIZZLE_G           0x8E43
#endif
#ifndef GL_TEXTURE_SWIZZLE_B
#define GL_TEXTURE_SWIZZLE_B           0x8E44
#endif

// OpenGL floating point formats

// gl3.h
#ifndef GL_RGBA16F
#define GL_RGBA16F                     0x881A
#endif

#ifndef GL_RGB16F
#define GL_RGB16F                      0x881B
#endif

// gl4.h
#ifndef GL_RGBA32F
#define GL_RGBA32F                     0x8814
#endif

#ifndef GL_RGB32F
#define GL_RGB32F                      0x8815
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
#define GLEXT_SUPPORT_GLMEMORY		128
#define GLEXT_SUPPORT_COMPUTE		256
#define GLEXT_SUPPORT_CONTEXT       512

//-----------------------------------------------------
// GL consts that are needed and aren't present in GL.h
//-----------------------------------------------------
#ifndef GL_TEXTURE_2D_MULTISAMPLE
#define GL_TEXTURE_2D_MULTISAMPLE 	0x9100
#endif
#ifndef WGL_ACCESS_READ_ONLY_NV
#define WGL_ACCESS_READ_ONLY_NV		0x0000
#endif
#ifndef WGL_ACCESS_READ_WRITE_NV
#define WGL_ACCESS_READ_WRITE_NV	0x0001
#endif
#ifndef WGL_ACCESS_WRITE_DISCARD_NV
#define WGL_ACCESS_WRITE_DISCARD_NV	0x0002
#endif

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
#define GL_FRAMEBUFFER_UNDEFINED							0x8219
#define GL_MAX_RENDERBUFFER_SIZE_EXT                        0x84E8
#define GL_FRAMEBUFFER_BINDING_EXT                          0x8CA6
#define GL_RENDERBUFFER_BINDING_EXT                         0x8CA7
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT           0x8CD0
#define GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT           0x8CD1
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL_EXT         0x8CD2
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_CUBE_MAP_FACE_EXT 0x8CD3
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_3D_ZOFFSET_EXT    0x8CD4
#define GL_COLOR_ATTACHMENT0                                0x8CE0
#define GL_COLOR_ATTACHMENT1                                0x8CE1
#define GL_COLOR_ATTACHMENT2                                0x8CE2
#define GL_COLOR_ATTACHMENT3                                0x8CE3
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

// gl3.h Read Format
#define GL_IMPLEMENTATION_COLOR_READ_TYPE                   0x8B9A
#define GL_IMPLEMENTATION_COLOR_READ_FORMAT                 0x8B9B

#define GL_FRAMEBUFFER_DEFAULT_WIDTH      0x9310
#define GL_FRAMEBUFFER_DEFAULT_HEIGHT     0x9311

typedef void   (APIENTRY *glBindFramebufferEXTPROC)			(GLenum target, GLuint framebuffer);
typedef void   (APIENTRY *glBindRenderbufferEXTPROC)		(GLenum target, GLuint renderbuffer);
typedef GLenum (APIENTRY *glCheckFramebufferStatusEXTPROC)	(GLenum target);
typedef GLenum (APIENTRY* glCheckNamedFramebufferStatusEXTPROC)	(GLuint framebuffer, GLenum target);
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
extern glCheckNamedFramebufferStatusEXTPROC			glCheckNamedFramebufferStatusEXT;
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
#define GL_READ_WRITE					0x88BA

#define GL_BUFFER_SIZE					0x8764
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
typedef void   (APIENTRY *glGenBuffersPROC)    (GLsizei n, GLuint* buffers);
typedef void   (APIENTRY *glDeleteBuffersPROC) (GLsizei n, const GLuint* buffers);
typedef void   (APIENTRY *glBindBufferPROC)    (GLenum target, const GLuint buffer);
typedef void   (APIENTRY *glBufferDataPROC)    (GLenum target,  GLsizeiptr size,  const GLvoid * data,  GLenum usage);
typedef void   (APIENTRY *glBufferStoragePROC) (GLenum target, GLsizeiptr size, const void * data, GLbitfield flags);
typedef void * (APIENTRY *glMapBufferPROC) (GLenum target,  GLenum access);
typedef void * (APIENTRY *glMapBufferRangePROC) (GLenum target, GLintptr offset, GLsizeiptr length, GLbitfield access);
typedef GLboolean (APIENTRY *glUnmapBufferPROC) (GLenum target);
typedef void   (APIENTRY *glGetBufferParameterivPROC) (GLenum target, GLenum value,	GLint * data);
typedef void   (APIENTRY* glGetTextureParameterivPROC) (GLenum texture, GLenum value, GLint* data);


extern glGenBuffersPROC		glGenBuffers;
extern glDeleteBuffersPROC	glDeleteBuffers;
extern glBindBufferPROC		glBindBuffer;
extern glBufferDataPROC		glBufferData;
extern glBufferStoragePROC	glBufferStorage;
extern glMapBufferPROC		glMapBuffer;
extern glMapBufferRangePROC	glMapBufferRange;
extern glUnmapBufferPROC	glUnmapBuffer;
extern glGetBufferParameterivPROC glGetBufferParameteriv;
extern glGetTextureParameterivPROC glGetTextureParameteriv;

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

#ifndef GL_INTERNALFORMAT_SUPPORTED
#define GL_INTERNALFORMAT_SUPPORTED 0x826F
#endif

#ifdef USE_COPY_EXTENSIONS
typedef void (APIENTRY * PFNGLCOPYIMAGESUBDATAPROC)(GLuint srcName, GLenum srcTarget, GLint srcLevel, GLint srcX, GLint srcY, GLint srcZ, GLuint dstName, GLenum dstTarget, GLint dstLevel, GLint dstX, GLint dstY, GLint dstZ, GLsizei srcWidth, GLsizei srcHeight, GLsizei srcDepth);
extern PFNGLCOPYIMAGESUBDATAPROC glCopyImageSubData;

typedef void(APIENTRY * glGetInternalformativPROC)(GLenum target, GLenum internalfrmat, GLenum pname, GLsizei buffSize, GLint *params);
extern glGetInternalformativPROC glGetInternalformativ;
#endif // USE_COPY_EXTENSIONS

//---------------------------
// Compute shader extensions
//---------------------------

#ifndef GL_COMPUTE_SHADER
#define GL_COMPUTE_SHADER 0x91B9
#endif

#ifndef GL_MAX_COMPUTE_WORK_GROUP_COUNT
#define GL_MAX_COMPUTE_WORK_GROUP_COUNT   0x91BE
#endif

#ifndef GL_MAX_COMPUTE_WORK_GROUP_SIZE
#define GL_MAX_COMPUTE_WORK_GROUP_SIZE    0x91BF
#endif

#ifndef GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS
#define GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS 0x90EB
#endif

#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS 0x8B82
#endif

#ifndef GL_ATTACHED_SHADERS
#define GL_ATTACHED_SHADERS 0x8B85
#endif

#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH 0x8B84
#endif

#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#endif

#ifndef GL_TEXTURE1
#define GL_TEXTURE1 0x84C1
#endif

#ifndef GL_TEXTURE2
#define GL_TEXTURE2 0x84C2
#endif

#ifndef GL_TEXTURE3
#define GL_TEXTURE3 0x84C3
#endif

#ifndef GL_TEXTURE_IMMUTABLE_FORMAT
#define GL_TEXTURE_IMMUTABLE_FORMAT 0x912F
#endif


typedef GLuint (APIENTRY* glCreateProgramPROC) (void);
typedef GLuint (APIENTRY* glCreateShaderPROC) (GLenum type);
typedef void   (APIENTRY* glShaderSourcePROC) (GLuint shader, GLsizei count, const char* const* string, const GLint* length);
typedef void   (APIENTRY* glCompileShaderPROC) (GLuint shader);
typedef void   (APIENTRY* glAttachShaderPROC) (GLuint program, GLuint shader);
typedef void   (APIENTRY* glLinkProgramPROC) (GLuint program);
typedef void   (APIENTRY* glGetProgramivPROC) (GLuint program, GLenum pname, GLint* param);
typedef void   (APIENTRY* glGetProgramInfoLogPROC) (GLuint program, GLsizei maxLength, GLsizei* length, char* infoLog);
typedef void   (APIENTRY* glGetShaderInfoLogPROC) (GLuint shader, GLsizei maxLength, GLsizei* length, char* infoLog);
typedef void   (APIENTRY* glGetIntegeri_vPROC) (GLenum target, GLuint index, GLint* data);
typedef void   (APIENTRY* glDetachShaderPROC) (GLuint program, GLuint shader);
typedef void   (APIENTRY* glUseProgramPROC) (GLuint program);
typedef void   (APIENTRY* glBindImageTexturePROC) (GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format);
typedef void   (APIENTRY* glDispatchComputePROC) (GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
typedef void   (APIENTRY* glDeleteProgramPROC) (GLuint program);
typedef void   (APIENTRY* glDeleteShaderPROC) (GLuint shader);

typedef void   (APIENTRY* glMemoryBarrierPROC) (GLbitfield barriers);

#ifndef GL_SHADER_IMAGE_ACCESS_BARRIER_BIT
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#endif

#ifndef GL_ALL_BARRIER_BITS
#define GL_ALL_BARRIER_BITS 0xFFFFFFFF
#endif

extern glCreateProgramPROC		glCreateProgram;
extern glCreateShaderPROC       glCreateShader;
extern glShaderSourcePROC       glShaderSource;
extern glCompileShaderPROC      glCompileShader;
extern glAttachShaderPROC       glAttachShader;
extern glLinkProgramPROC        glLinkProgram;
extern glGetProgramivPROC       glGetProgramiv;
extern glGetProgramInfoLogPROC  glGetProgramInfoLog;
extern glGetShaderInfoLogPROC   glGetShaderInfoLog;
extern glGetIntegeri_vPROC      glGetIntegeri_v;
extern glDetachShaderPROC       glDetachShader;
extern glUseProgramPROC         glUseProgram;
extern glBindImageTexturePROC   glBindImageTexture;
extern glDispatchComputePROC    glDispatchCompute;
extern glDeleteProgramPROC      glDeleteProgram;
extern glDeleteShaderPROC       glDeleteShader;
extern glMemoryBarrierPROC      glMemoryBarrier;

typedef void (APIENTRY* glActiveTexturePROC)(GLenum texture);
extern glActiveTexturePROC      glActiveTexture;
typedef void (APIENTRY* glUniform1iPROC) (GLint location, GLint v0);
extern glUniform1iPROC          glUniform1i;
typedef void (APIENTRY* glUniform1fPROC) (GLint location, float v0);
extern glUniform1fPROC          glUniform1f;
typedef GLint (APIENTRY* glGetUniformLocationPROC) (GLuint program, const char* name);
extern glGetUniformLocationPROC glGetUniformLocation;

// Use instead of glTexture2D to create immutable texture
typedef void (APIENTRY* glTextureStorage2DPROC) (GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height);
extern glTextureStorage2DPROC glTextureStorage2D;

typedef void (APIENTRY * glCreateTexturesPROC) (GLenum target, GLsizei n, GLuint* textures);
extern glCreateTexturesPROC glCreateTextures;

// - - - - - - - - - - - - - -
//    GL memory extensions
// - - - - - - - - - - - - - -
//
// https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects.txt
// void CreateMemoryObjectsEXT(sizei n,	uint* memoryObjects);
// void DeleteMemoryObjectsEXT(sizei n, const uint* memoryObjects);
// void TexStorageMem2DEXT(enum target, sizei levels, enum internalFormat, sizei width,	sizei height, uint memory, uint64 offset);
// https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects_win32.txt
// void ImportMemoryWin32HandleEXT(uint memory, uint64 size, enum handleType, void* handle);
// void ImportMemoryWin32NameEXT(uint memory, uint64 size, enum handleType,	const void* name);

typedef void (APIENTRY* glCreateMemoryObjectsEXTPROC) (GLsizei n, GLuint* memoryObjects);
extern glCreateMemoryObjectsEXTPROC glCreateMemoryObjectsEXT;

typedef void (APIENTRY* glDeleteMemoryObjectsEXTPROC) (GLsizei n, const GLuint* memoryObjects);
extern glDeleteMemoryObjectsEXTPROC glDeleteMemoryObjectsEXT;

typedef void (APIENTRY* glTexStorageMem2DEXTPROC) (GLenum target, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset);
extern glTexStorageMem2DEXTPROC glTexStorageMem2DEXT;

typedef void (APIENTRY* glTextureStorageMem2DEXTPROC) (GLuint texture, GLsizei levels, GLenum internalFormat, GLsizei width, GLsizei height, GLuint memory, GLuint64 offset);
extern glTextureStorageMem2DEXTPROC glTextureStorageMem2DEXT;

typedef void (APIENTRY* glImportMemoryWin32HandleEXTPROC) (GLuint memory, GLuint64 size, GLenum handleType, void* handle);
extern glImportMemoryWin32HandleEXTPROC glImportMemoryWin32HandleEXT;

typedef void (APIENTRY* glBufferStorageMemEXTPROC) (GLenum target, GLsizeiptr size, GLuint memory, GLuint64 offset);
extern glBufferStorageMemEXTPROC glBufferStorageMemEXT;

typedef void (APIENTRY* glMemoryObjectParameterivEXTPROC) (GLuint memoryObject, GLenum pname, const GLint* params);
extern glMemoryObjectParameterivEXTPROC glMemoryObjectParameterivEXT;

typedef void (APIENTRY* glGetMemoryObjectParameterivEXTPROC) (GLuint memoryObject, GLenum pname, GLint* params);
extern glGetMemoryObjectParameterivEXTPROC glGetMemoryObjectParameterivEXT;

typedef GLboolean(APIENTRY* glIsMemoryObjectEXTPROC) (GLuint memoryObject);
extern glIsMemoryObjectEXTPROC glIsMemoryObjectEXT;

typedef void (APIENTRY* glCreateBuffersPROC) (GLsizei n, GLuint* buffers);
extern glCreateBuffersPROC glCreateBuffers;

typedef void (APIENTRY* glBindBufferBasePROC) (GLenum target, GLuint index, GLuint buffer);
extern glBindBufferBasePROC glBindBufferBase;

// TODO
// Shader extensions
#define GL_SHADER_STORAGE_BARRIER_BIT                 0x2000
#define GL_SHADER_STORAGE_BUFFER                      0x90D2
#define GL_SHADER_STORAGE_BUFFER_BINDING              0x90D3
#define GL_SHADER_STORAGE_BUFFER_START                0x90D4
#define GL_SHADER_STORAGE_BUFFER_SIZE                 0x90D5
#define GL_STATIC_DRAW                                0x88E4
#define GL_STATIC_READ                                0x88E5
#define GL_STATIC_COPY                                0x88E6


#ifndef GL_DEDICATED_MEMORY_OBJECT_EXT
#define GL_DEDICATED_MEMORY_OBJECT_EXT                0x9581
#endif

#ifndef GL_PROTECTED_MEMORY_OBJECT_EXT                
#define GL_PROTECTED_MEMORY_OBJECT_EXT                0x959B
#endif

// https://registry.khronos.org/OpenGL/extensions/EXT/EXT_external_objects_win32.txt
// Accepted by the <handleType> parameter of ImportMemoryWin32HandleEXT(), ImportMemoryWin32NameEXT()
#ifndef GL_HANDLE_TYPE_OPAQUE_WIN32_EXT
#define GL_HANDLE_TYPE_OPAQUE_WIN32_EXT               0x9587
#endif
#ifndef GL_HANDLE_TYPE_OPAQUE_IMAGE_KMT_EXT
#define GL_HANDLE_TYPE_OPAQUE_IMAGE_KMT_EXT           0x9588
#endif
#ifndef GL_HANDLE_TYPE_D3D12_TILEPOOL_EXT
#define GL_HANDLE_TYPE_D3D12_TILEPOOL_EXT             0x9589
#endif
#ifndef GL_HANDLE_TYPE_D3D12_RESOURCE_EXT
#define GL_HANDLE_TYPE_D3D12_RESOURCE_EXT             0x958A
#endif
#ifndef GL_HANDLE_TYPE_D3D11_IMAGE_EXT
#define GL_HANDLE_TYPE_D3D11_IMAGE_EXT                0x958B
#endif
#ifndef GL_HANDLE_TYPE_D3D11_IMAGE_KMT_EXT
#define GL_HANDLE_TYPE_D3D11_IMAGE_KMT_EXT            0x958C
#endif


//---------------------------
// Context creation extension
// https://registry.khronos.org/OpenGL/extensions/ARB/WGL_ARB_create_context.txt
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
#ifndef WGL_CONTEXT_DEBUG_BIT_ARB
#define		WGL_CONTEXT_DEBUG_BIT_ARB               0x0001
#endif
#ifndef WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#define		WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002
#endif

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
bool loadGLmemoryExtensions();
bool loadComputeShaderExtensions();
bool loadContextExtension();
bool isExtensionSupported(const char *extension);
void ExtLog(ExtLogLevel level, const char* format, ...);

#endif // end __spoutGLextensions__
