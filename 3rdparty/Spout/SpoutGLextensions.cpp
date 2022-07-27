/*
//
//
//			spoutGLextensions.cpp
//
//			Used for load of openGL extensions with option
//			to use Glew or disable dynamic load of specific extensions
//			See spoutGLextions.h
//
//			01.09.15	- added MessageBox error warning in LoadGLextensions
//			11.11.15	- removed (unsigned) cast from GetProcAddress in FBO extensions
//			17.03.16	- added bgra extensions to find out if they are supported at compile and runtime
//			28.03.16	- caps is returned instead of fail for interop extensions
//			29.03.16	- Fixed loadInteropExtensions flag test in loadGLextensions
//			12.08.16	- Removed "isExtensionSupported" (https://github.com/leadedge/Spout2/issues/19)
//			13.01.17	- Removed try/catch from wglDXRegisterObjectNV calls
//						- Clean up #ifdefs in all functions - return true if FBO of PBO are defined elsewhere
//			27.10.18	- Test for opengl context in loadglextensions
//			21.11.18	- Add copy extensions for future use
//			23.11.18	- Fix test for wglDXCloseDeviceNV in loadInteropExtensions
//			14.09.20	- Add legacyOpenGL define test in "isExtensionSupported" to avoid glGetString
//						  Thanks to Alexandre Buge (https://github.com/Qlex42) for the notice and fix
//			23.09.20	- Correct isExtensionSupported
//						  Include SpoutCommon.h for legacyOpenGL
//			11.12.20	- Add glGetBufferParameterivEXT
//			08.11.21	- Add glMapBufferRangeEXT
//			09.11.21	- Add glClientWaitSyncEXT, glDeleteSyncEXT, glFenceSyncEXT
//			13.11.21	- Add "standalone" define in SpoutGLextensions.h for independent use
//						  without dependence on Spout source files.
//						- Add "legacyOpenGL" define in SpoutGLextensions.h for standalone.
//			14.11.21	- Add ExtLog for Spout error logs including printf for standalone.
//			23.11.21	- Add debugging console print to loadPBOextensions
//						  
//

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

#include "SpoutGLextensions.h"

#ifndef USE_GLEW

// GL/DX extensions
PFNWGLDXOPENDEVICENVPROC				wglDXOpenDeviceNV				= NULL;
PFNWGLDXREGISTEROBJECTNVPROC			wglDXRegisterObjectNV			= NULL;
PFNWGLDXSETRESOURCESHAREHANDLENVPROC	wglDXSetResourceShareHandleNV	= NULL;
PFNWGLDXLOCKOBJECTSNVPROC				wglDXLockObjectsNV				= NULL;
PFNWGLDXUNLOCKOBJECTSNVPROC				wglDXUnlockObjectsNV			= NULL;
PFNWGLDXCLOSEDEVICENVPROC				wglDXCloseDeviceNV				= NULL;
PFNWGLDXUNREGISTEROBJECTNVPROC			wglDXUnregisterObjectNV			= NULL;

// FBO extensions
#ifdef USE_FBO_EXTENSIONS
glBindFramebufferEXTPROC				glBindFramebufferEXT			= NULL;
glBindRenderbufferEXTPROC				glBindRenderbufferEXT			= NULL;
glCheckFramebufferStatusEXTPROC			glCheckFramebufferStatusEXT		= NULL;
glDeleteFramebuffersEXTPROC				glDeleteFramebuffersEXT			= NULL;
glDeleteRenderBuffersEXTPROC			glDeleteRenderBuffersEXT		= NULL;
glFramebufferRenderbufferEXTPROC		glFramebufferRenderbufferEXT	= NULL;
glFramebufferTexture1DEXTPROC			glFramebufferTexture1DEXT		= NULL;
glFramebufferTexture2DEXTPROC			glFramebufferTexture2DEXT		= NULL;
glFramebufferTexture3DEXTPROC			glFramebufferTexture3DEXT		= NULL;
glGenFramebuffersEXTPROC				glGenFramebuffersEXT			= NULL;
glGenRenderbuffersEXTPROC				glGenRenderbuffersEXT			= NULL;
glGenerateMipmapEXTPROC					glGenerateMipmapEXT				= NULL;
glGetFramebufferAttachmentParameterivEXTPROC glGetFramebufferAttachmentParameterivEXT = NULL;
glGetRenderbufferParameterivEXTPROC		glGetRenderbufferParameterivEXT	= NULL;
glIsFramebufferEXTPROC					glIsFramebufferEXT				= NULL;
glIsRenderbufferEXTPROC					glIsRenderbufferEXT				= NULL;
glRenderbufferStorageEXTPROC			glRenderbufferStorageEXT		= NULL;
#endif

// FBO blit extensions
glBlitFramebufferEXTPROC				glBlitFramebufferEXT			= NULL;

#ifdef USE_FBO_EXTENSIONS
// OpenGL sync control extensions
PFNWGLSWAPINTERVALEXTPROC				wglSwapIntervalEXT				= NULL;
PFNWGLGETSWAPINTERVALEXTPROC			wglGetSwapIntervalEXT			= NULL;
#endif

// PBO extensions
#ifdef USE_PBO_EXTENSIONS
glGenBuffersPROC						glGenBuffersEXT					= NULL;
glDeleteBuffersPROC						glDeleteBuffersEXT				= NULL;
glBindBufferPROC						glBindBufferEXT					= NULL;
glBufferDataPROC						glBufferDataEXT					= NULL;
glBufferStoragePROC						glBufferStorageEXT				= NULL;
glMapBufferPROC							glMapBufferEXT					= NULL;
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glMapBufferRange.xhtml
glMapBufferRangePROC					glMapBufferRangeEXT				= NULL;
glUnmapBufferPROC						glUnmapBufferEXT				= NULL;
glGetBufferParameterivPROC				glGetBufferParameterivEXT		= NULL;
glClientWaitSyncPROC					glClientWaitSyncEXT				= NULL;
glDeleteSyncPROC						glDeleteSyncEXT					= NULL;
glFenceSyncPROC							glFenceSyncEXT					= NULL;

#endif

//-------------------
// Copy extensions
// (for future use)
//-------------------
#ifdef USE_COPY_EXTENSIONS
PFNGLCOPYIMAGESUBDATAPROC glCopyImageSubData = NULL;
glGetInternalFormativPROC glGetInternalFormativ = NULL;

#endif

//---------------------------
// Context creation extension
//---------------------------
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;

#endif

//
// Load the Nvidia-Extensions dynamically
//
bool loadInteropExtensions() {

#ifdef USE_GLEW
	if(WGLEW_NV_DX_interop)
		return true;
	else
		return false;
#else

	// Here we provide warnings for individual extensions

	wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC)wglGetProcAddress("wglDXOpenDeviceNV");
	if(!wglDXOpenDeviceNV) {
		ExtLog(LOG_WARNING, "loadInteropExtensions : wglDXOpenDeviceNV NULL");
		return false;
	}

	wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXRegisterObjectNV");
	if(!wglDXRegisterObjectNV) {
		ExtLog(LOG_WARNING, "loadInteropExtensions : wglDXRegisterObjectNV NULL");
		return false;
	}

	wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXUnregisterObjectNV");
	if(!wglDXUnregisterObjectNV) {
		ExtLog(LOG_WARNING, "loadInteropExtensions : wglDXUnregisterObjectNV NULL");
		return false;
	}

	wglDXSetResourceShareHandleNV = (PFNWGLDXSETRESOURCESHAREHANDLENVPROC)wglGetProcAddress("wglDXSetResourceShareHandleNV");
	if(!wglDXSetResourceShareHandleNV) {
		ExtLog(LOG_WARNING, "loadInteropExtensions : wglDXSetResourceShareHandleNV NULL");
		return false;
	}

	wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXLockObjectsNV");
	if(!wglDXLockObjectsNV)	{
		ExtLog(LOG_WARNING, "loadInteropExtensions : wglDXLockObjectsNV NULL");
		return false;
	}

	wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXUnlockObjectsNV");
	if(!wglDXUnlockObjectsNV) {
		ExtLog(LOG_WARNING, "loadInteropExtensions : wglDXUnlockObjectsNV NULL");
		return false;
	}

	wglDXCloseDeviceNV = (PFNWGLDXCLOSEDEVICENVPROC)wglGetProcAddress("wglDXCloseDeviceNV");
	if(!wglDXCloseDeviceNV) {
		ExtLog(LOG_WARNING, "loadInteropExtensions : wglDXCloseDeviceNV NULL");
		return false;
	}

	return true;

#endif

}

bool loadFBOextensions() {

	// Here we use 'EXT_framebuffer_object'
	// But for OpenGL version >= 3, framebuffer objects are core.
	// Control this using the "legacyOpenGL" define in SpoutGLextensions.h

	// Thanks and credit to Menno Vink of Resolume for sharing the POSTFIX code
	
#ifdef legacyOpenGL
	#define FBO_EXTENSION_POSTFIX "EXT"
#else
	#define FBO_EXTENSION_POSTFIX
#endif

#ifdef USE_FBO_EXTENSIONS

	#ifdef USE_GLEW
	if(GLEW_EXT_framebuffer_object)
		return true;
	else
		return false;
	#else

	glBindFramebufferEXT                     = (glBindFramebufferEXTPROC)wglGetProcAddress("glBindFramebuffer" FBO_EXTENSION_POSTFIX);
	glBindRenderbufferEXT                    = (glBindRenderbufferEXTPROC)wglGetProcAddress("glBindRenderbuffer" FBO_EXTENSION_POSTFIX);
	glCheckFramebufferStatusEXT              = (glCheckFramebufferStatusEXTPROC)wglGetProcAddress("glCheckFramebufferStatus" FBO_EXTENSION_POSTFIX);
	glDeleteFramebuffersEXT                  = (glDeleteFramebuffersEXTPROC)wglGetProcAddress("glDeleteFramebuffers" FBO_EXTENSION_POSTFIX);
	glDeleteRenderBuffersEXT                 = (glDeleteRenderBuffersEXTPROC)wglGetProcAddress("glDeleteRenderbuffers" FBO_EXTENSION_POSTFIX);
	glFramebufferRenderbufferEXT             = (glFramebufferRenderbufferEXTPROC)wglGetProcAddress("glFramebufferRenderbuffer" FBO_EXTENSION_POSTFIX);
	glFramebufferTexture1DEXT                = (glFramebufferTexture1DEXTPROC)wglGetProcAddress("glFramebufferTexture1D" FBO_EXTENSION_POSTFIX);
	glFramebufferTexture2DEXT                = (glFramebufferTexture2DEXTPROC)wglGetProcAddress("glFramebufferTexture2D" FBO_EXTENSION_POSTFIX);
	glFramebufferTexture3DEXT                = (glFramebufferTexture3DEXTPROC)wglGetProcAddress("glFramebufferTexture3D" FBO_EXTENSION_POSTFIX);
	glGenFramebuffersEXT                     = (glGenFramebuffersEXTPROC)wglGetProcAddress("glGenFramebuffers" FBO_EXTENSION_POSTFIX);
	glGenRenderbuffersEXT                    = (glGenRenderbuffersEXTPROC)wglGetProcAddress("glGenRenderbuffers" FBO_EXTENSION_POSTFIX);
	glGenerateMipmapEXT                      = (glGenerateMipmapEXTPROC)wglGetProcAddress("glGenerateMipmap" FBO_EXTENSION_POSTFIX);
	glGetFramebufferAttachmentParameterivEXT = (glGetFramebufferAttachmentParameterivEXTPROC)wglGetProcAddress("glGetFramebufferAttachmentParameteriv" FBO_EXTENSION_POSTFIX);
	glGetRenderbufferParameterivEXT          = (glGetRenderbufferParameterivEXTPROC)wglGetProcAddress("glGetRenderbufferParameteriv" FBO_EXTENSION_POSTFIX);
	glIsFramebufferEXT                       = (glIsFramebufferEXTPROC)wglGetProcAddress("glIsFramebuffer" FBO_EXTENSION_POSTFIX);
	glIsRenderbufferEXT                      = (glIsRenderbufferEXTPROC)wglGetProcAddress("glIsRenderbuffer" FBO_EXTENSION_POSTFIX);
	glRenderbufferStorageEXT                 = (glRenderbufferStorageEXTPROC)wglGetProcAddress("glRenderbufferStorage" FBO_EXTENSION_POSTFIX);

	if	  ( glBindFramebufferEXT						!= NULL && 
			glBindRenderbufferEXT						!= NULL && 
			glCheckFramebufferStatusEXT					!= NULL && 
			glDeleteFramebuffersEXT						!= NULL && 
			glDeleteRenderBuffersEXT					!= NULL &&
			glFramebufferRenderbufferEXT				!= NULL && 
			glFramebufferTexture1DEXT					!= NULL && 
			glFramebufferTexture2DEXT					!= NULL && 
			glFramebufferTexture3DEXT					!= NULL && 
			glGenFramebuffersEXT						!= NULL &&
			glGenRenderbuffersEXT						!= NULL && 
			glGenerateMipmapEXT							!= NULL && 
			glGetFramebufferAttachmentParameterivEXT	!= NULL && 
			glGetRenderbufferParameterivEXT				!= NULL && 
			glIsFramebufferEXT							!= NULL &&
			glIsRenderbufferEXT							!= NULL && 
			glRenderbufferStorageEXT					!= NULL) {
		return true;
	}
	else {
		return false;
	}
	#endif
#else
	// FBO extensions defined elsewhere
	return true;
#endif
}


bool loadBLITextension() {

#ifdef USE_GLEW
	if(GLEW_EXT_framebuffer_blit)
		return true;
	else
		return false;
#else
	glBlitFramebufferEXT = (glBlitFramebufferEXTPROC) wglGetProcAddress("glBlitFramebufferEXT");
	return glBlitFramebufferEXT!=NULL;
#endif

}

bool loadSwapExtensions()
{
	wglSwapIntervalEXT    = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
	if(wglSwapIntervalEXT == NULL || wglGetSwapIntervalEXT == NULL) {
		return false;
	}
	return true;
}


// =================== PBO support ==================
// Include sync here - could be separated later
bool loadPBOextensions() 
{

#ifdef USE_PBO_EXTENSIONS

	#ifdef USE_GLEW
	if(glGenBuffersARB)
		return true;
	else
		return false;
	#else
	glGenBuffersEXT	    = (glGenBuffersPROC)wglGetProcAddress("glGenBuffers");
	glDeleteBuffersEXT  = (glDeleteBuffersPROC)wglGetProcAddress("glDeleteBuffers");
	glBindBufferEXT	    = (glBindBufferPROC)wglGetProcAddress("glBindBuffer");
	glBufferDataEXT	    = (glBufferDataPROC)wglGetProcAddress("glBufferData");
	glBufferStorageEXT	= (glBufferStoragePROC)wglGetProcAddress("glBufferStorage");
	glMapBufferEXT      = (glMapBufferPROC)wglGetProcAddress("glMapBuffer");
	glMapBufferRangeEXT = (glMapBufferRangePROC)wglGetProcAddress("glMapBufferRange");
	glUnmapBufferEXT    = (glUnmapBufferPROC)wglGetProcAddress("glUnmapBuffer");
	glGetBufferParameterivEXT = (glGetBufferParameterivPROC)wglGetProcAddress("glGetBufferParameteriv");
	glClientWaitSyncEXT = (glClientWaitSyncPROC)wglGetProcAddress("glClientWaitSync");
	glDeleteSyncEXT     = (glDeleteSyncPROC)wglGetProcAddress("glDeleteSync");
	glFenceSyncEXT      = (glFenceSyncPROC)wglGetProcAddress("glFenceSync");

	if(glGenBuffersEXT  != NULL && glDeleteBuffersEXT  != NULL
	&& glBindBufferEXT  != NULL && glBufferDataEXT     != NULL
	&& glBufferStorageEXT != NULL && glMapBufferEXT   != NULL
	&& glMapBufferRangeEXT != NULL && glUnmapBufferEXT != NULL
	&& glGetBufferParameterivEXT != NULL
	&& glClientWaitSyncEXT != NULL && glDeleteSyncEXT != NULL && glFenceSyncEXT != NULL) {
		return true;
	}
	else {
		ExtLog(LOG_WARNING, "loadPBOextensions() fail");
		return false;
	}
	#endif

#else
	// PBO extensions defined elsewhere
	return true;
#endif
}



bool loadCopyExtensions()
{

#ifdef USE_COPY_EXTENSIONS

#ifdef USE_GLEW
	if (glCopyImageSubData)
		return true;
	else
		return false;
#else

	// Copy extensions
	glCopyImageSubData = (PFNGLCOPYIMAGESUBDATAPROC)wglGetProcAddress("glCopyImageSubData");

	glGetInternalFormativ = (glGetInternalFormativPROC)wglGetProcAddress("glGetInternalFormativ");

	if (glCopyImageSubData != NULL) {
		return true;
	}
	else {
		return false;
	}
#endif

#else
	// COPY extensions defined elsewhere
	return true;
#endif

}


bool loadContextExtension()
{

#ifdef USE_CONTEXT_EXTENSION

	// Return if loadContextExtension() has been called before loading all the extensions
	if (wglCreateContextAttribsARB) {
		return true;
	}

#ifdef USE_GLEW
	if (wglCreateContextAttribsARB)
		return true;
	else
		return false;
#else

	// Context creation extension
	wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

	if (wglCreateContextAttribsARB != NULL) {
		return true;
	}
	else {
		return false;
	}
#endif

#else
	// Context creation extension defined elsewhere
	return true;
#endif

}


bool InitializeGlew()
{
#ifdef USE_GLEW
	HGLRC glContext;
	GLenum glew_error;

	// Opengl context is necessary
	glContext = wglGetCurrentContext();
	if(glContext == NULL) {
		return false;
	}
	//
	// Note from Glew : GLEW obtains information on the supported extensions from the graphics driver.
	// Experimental or pre-release drivers, however, might not report every available extension through
	// the standard mechanism, in which case GLEW will report it unsupported. To circumvent this situation,
	// the glewExperimental global switch can be turned on by setting it to GL_TRUE before calling glewInit(),
	// which ensures that all extensions with valid entry points will be exposed. 
	//
	glewExperimental = GL_TRUE;
	glew_error = glewInit();
	if (glew_error != GLEW_OK) {
		return false;
	}
	//
	// Glew should have loaded all the extensions and we can check for them
	//
	// http://glew.sourceforge.net/basic.html
	//
	return true;
#else
	// Glew usage not defined so cannot initialize
	return false;
#endif
}


//
// Load GL extensions
//
unsigned int loadGLextensions() {
	
	unsigned int caps = 0; // as per elio glextensions

	// wglGetProcAddress requires an OpenGL rendering context
	HGLRC glContext = wglGetCurrentContext();
	if (glContext == NULL) {
		ExtLog(LOG_ERROR, "loadGLextensions : no OpenGL context");
		return 0;
	}

#ifdef USE_GLEW
	InitializeGlew(); // probably needs failure check
#endif

	// Check for FBO extensions - no use continuing without them
	if (loadFBOextensions()) {
		caps |= GLEXT_SUPPORT_FBO;
	}
	else {
		ExtLog(LOG_ERROR, "loadGLextensions : loadFBOextensions fail");
		return 0;
	}

	// Load other extensions
	if(loadBLITextension()) {
		caps |= GLEXT_SUPPORT_FBO_BLIT;
	}
	else {
		ExtLog(LOG_WARNING, "loadGLextensions : loadBLITextensions fail");
	}

	if(loadSwapExtensions()) {
		caps |= GLEXT_SUPPORT_SWAP;
	}
	else {
		ExtLog(LOG_WARNING, "loadGLextensions : loadSwapExtensions fail");
	}

	if(loadPBOextensions()) {
		caps |= GLEXT_SUPPORT_PBO;
	}
	else {
		ExtLog(LOG_WARNING, "loadGLextensions : loadPBOextensions fail");
	}

	if (loadCopyExtensions()) {
		caps |= GLEXT_SUPPORT_COPY;
	}
	else {
		ExtLog(LOG_WARNING, "loadGLextensions : loadCopyExtensions fail");
	}

	if (loadContextExtension()) {
		caps |= GLEXT_SUPPORT_CONTEXT;
	}
	else {
		ExtLog(LOG_WARNING, "loadGLextensions : loadContextExtension fail");
	}

	// Load wgl interop extensions
	if (loadInteropExtensions()) {
		caps |= GLEXT_SUPPORT_NVINTEROP;
	}
	else {
		ExtLog(LOG_WARNING, "loadGLextensions : loadInteropExtensions fail");
	}

	// Find out whether bgra extensions are supported at compile and runtime
#ifdef GL_EXT_bgra
	//
	// "isExtensionSupported" code yet to be fully tested for
	// various compilers, operating systems and environments.
	// De-activate this function if you have problems.
	// 
	if (isExtensionSupported("GL_EXT_bgra")) {
		caps |= GLEXT_SUPPORT_BGRA;
	}
#endif

	return caps;

}


//
// Used to determine support for GL_EXT_bgra extensions
//
bool isExtensionSupported(const char *extension)
{
	if (!extension || *extension == '\0')
		return false;

	// Extension names should not have spaces.
	if(strchr(extension, ' '))
		return false;

// glGetString can cause problems for core OpenGL context
#ifdef legacyOpenGL
	const char * extensionsstr = (const char *)glGetString(GL_EXTENSIONS);
	if (extensionsstr) {
		std::string extensions = extensionsstr;
		std::size_t found = extensions.find(extension);
		if (found != std::string::npos) {
			return true;
		}
		ExtLog(LOG_WARNING, "isExtensionSupported : extension [%s] not found", extension);
		return false;
	}
#else
	//
	// glGetstring not supported
	// for a core GL context
	//
	// Code adapted from : https://bitbucket.org/Coin3D/coin/issues/54/support-for-opengl-3x-specifically
	// Also : http://www.opengl.org/resources/features/OGLextensions/
	//
	int n = 0;
	int i = 0;
	typedef GLubyte* (APIENTRY * COIN_PFNGLGETSTRINGIPROC)(GLenum enm, GLuint idx);
	COIN_PFNGLGETSTRINGIPROC glGetStringi = 0;
	glGetStringi = (COIN_PFNGLGETSTRINGIPROC)wglGetProcAddress("glGetStringi");
	if(glGetStringi != NULL) {
		glGetIntegerv(GL_NUM_EXTENSIONS, &n);
		if(n > 0) {
			const char * exc = nullptr;
			for (i = 0; i < n; i++) {
				exc = (const char *)glGetStringi(GL_EXTENSIONS, (GLuint)i);
				if(exc) {
					if(strcmp(exc, extension) == 0)
						break;
				}
			}
			if(exc && i < n) {
				return true;
			}
			ExtLog(LOG_WARNING, "isExtensionSupported : extension [%s] not found", extension);
			return false;
		}
		else {
			ExtLog(LOG_WARNING, "isExtensionSupported : glGetIntegerv(GL_NUM_EXTENSIONS) did not return a value");
		}
	}
	else {
		ExtLog(LOG_WARNING, "isExtensionSupported : glGetStringi not found");
	}
#endif

	ExtLog(LOG_WARNING, "isExtensionSupported : unable to find extension [%s]", extension);
	
	return false;

}

void ExtLog(LogLevel level, const char* format, ...)
{
	va_list args;
	va_start(args, format);

#ifdef standalone
	char currentLog[512];
	vsprintf_s(currentLog, 512, format, args);
	std::string logstring;
	logstring = "SpoutGLextensions : ";
	switch (level) {
		case LOG_NOTICE:
			logstring += "Notice - ";
			break;
		case LOG_WARNING:
			logstring += "Warning - ";
			break;
		case LOG_ERROR:
			logstring += "Error - ";
			break;
		default:
			break;
	}
	logstring += currentLog;
	printf("%s\n", currentLog);
	// Note that this will not be recorded in a Spout log file.
#else
	_doLog(static_cast<SpoutLogLevel>(level), format, args); // SpoutUtils function
#endif

	va_end(args);
}