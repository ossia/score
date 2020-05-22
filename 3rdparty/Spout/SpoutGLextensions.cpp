/*
//
//
//			spoutGLextensions.cpp
//
//			Used for load of openGL extensions with option
//			to use Glew or disable dynamic load of specific extensions
//			See spoutGLext.h
//
//			01.09.15	- added MessageBox error warning in LoadGLextensions
//			11.11.15	- removed (unsigned) cast from GetProcAddress in FBO extensions
//			17.03.16	- added bgra extensions to find out if they are supported at compile and runtime
//			28.03.16	- caps is returned instead of fail for interop extensions
//			29.03.16	- Fixed loadInteropExtensions flag test in loadGLextensions
//			12.08.16	- Removed "isExtensionSupported" (https://github.com/leadedge/Spout2/issues/19)
//			13.01.17	- Removed try/catch from wglDXRegisterObjectNV calls
//						- Clean up #ifdefs in all functions - return true if FBO of PBO are defined elsewhere
//

		Copyright (c) 2014-2017, Lynn Jarvis. All rights reserved.

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
glMapBufferPROC							glMapBufferEXT					= NULL;
glUnmapBufferPROC						glUnmapBufferEXT				= NULL;
#endif

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
	wglDXOpenDeviceNV = (PFNWGLDXOPENDEVICENVPROC)wglGetProcAddress("wglDXOpenDeviceNV");
	if(!wglDXOpenDeviceNV) {
		return false;
	}
	wglDXRegisterObjectNV = (PFNWGLDXREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXRegisterObjectNV");
	if(!wglDXRegisterObjectNV) {
		return false;
	}
	wglDXUnregisterObjectNV = (PFNWGLDXUNREGISTEROBJECTNVPROC)wglGetProcAddress("wglDXUnregisterObjectNV");
	if(!wglDXUnregisterObjectNV) {
		return false;
	}
	wglDXSetResourceShareHandleNV = (PFNWGLDXSETRESOURCESHAREHANDLENVPROC)wglGetProcAddress("wglDXSetResourceShareHandleNV");
	if(!wglDXSetResourceShareHandleNV) {
		return false;
	}
	wglDXLockObjectsNV = (PFNWGLDXLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXLockObjectsNV");
	if(!wglDXLockObjectsNV)	{
		return false;
	}
	wglDXUnlockObjectsNV = (PFNWGLDXUNLOCKOBJECTSNVPROC)wglGetProcAddress("wglDXUnlockObjectsNV");
	if(!wglDXUnlockObjectsNV) {
		return false;
	}
	wglDXCloseDeviceNV = (PFNWGLDXCLOSEDEVICENVPROC)wglGetProcAddress("wglDXCloseDeviceNV");
	if(!wglDXUnlockObjectsNV) {
		return false;
	}

	return true;
#endif

}

bool loadFBOextensions() {

#ifdef USE_FBO_EXTENSIONS

	#ifdef USE_GLEW
	if(GLEW_EXT_framebuffer_object)
		return true;
	else
		return false;
	#else
	glBindFramebufferEXT						= (glBindFramebufferEXTPROC)wglGetProcAddress("glBindFramebufferEXT");
	glBindRenderbufferEXT						= (glBindRenderbufferEXTPROC)wglGetProcAddress("glBindRenderbufferEXT");
	glCheckFramebufferStatusEXT					= (glCheckFramebufferStatusEXTPROC)wglGetProcAddress("glCheckFramebufferStatusEXT");
	glDeleteFramebuffersEXT						= (glDeleteFramebuffersEXTPROC)wglGetProcAddress("glDeleteFramebuffersEXT");
	glDeleteRenderBuffersEXT					= (glDeleteRenderBuffersEXTPROC)wglGetProcAddress("glDeleteRenderbuffersEXT");
	glFramebufferRenderbufferEXT				= (glFramebufferRenderbufferEXTPROC)wglGetProcAddress("glFramebufferRenderbufferEXT");
	glFramebufferTexture1DEXT					= (glFramebufferTexture1DEXTPROC)wglGetProcAddress("glFramebufferTexture1DEXT");
	glFramebufferTexture2DEXT					= (glFramebufferTexture2DEXTPROC)wglGetProcAddress("glFramebufferTexture2DEXT");
	glFramebufferTexture3DEXT					= (glFramebufferTexture3DEXTPROC)wglGetProcAddress("glFramebufferTexture3DEXT");
	glGenFramebuffersEXT						= (glGenFramebuffersEXTPROC)wglGetProcAddress("glGenFramebuffersEXT");
	glGenRenderbuffersEXT						= (glGenRenderbuffersEXTPROC)wglGetProcAddress("glGenRenderbuffersEXT");
	glGenerateMipmapEXT							= (glGenerateMipmapEXTPROC)wglGetProcAddress("glGenerateMipmapEXT");
	glGetFramebufferAttachmentParameterivEXT	= (glGetFramebufferAttachmentParameterivEXTPROC)wglGetProcAddress("glGetFramebufferAttachmentParameterivEXT");
	glGetRenderbufferParameterivEXT				= (glGetRenderbufferParameterivEXTPROC)wglGetProcAddress("glGetRenderbufferParameterivEXT");
	glIsFramebufferEXT							= (glIsFramebufferEXTPROC)wglGetProcAddress("glIsFramebufferEXT");
	glIsRenderbufferEXT							= (glIsRenderbufferEXTPROC)wglGetProcAddress("glIsRenderbufferEXT");
	glRenderbufferStorageEXT					= (glRenderbufferStorageEXTPROC)wglGetProcAddress("glRenderbufferStorageEXT");
	
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
	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
	if(wglSwapIntervalEXT == NULL || wglGetSwapIntervalEXT == NULL) {
		return false;
	}

	return true;

}


// =================== PBO support 18.01.14 ==================
bool loadPBOextensions() 
{

#ifdef USE_PBO_EXTENSIONS

	#ifdef USE_GLEW
	if(glGenBuffersARB)
		return true;
	else
		return false;
	#else
	glGenBuffersEXT	= (glGenBuffersPROC)wglGetProcAddress("glGenBuffers");
	glDeleteBuffersEXT = (glDeleteBuffersPROC)wglGetProcAddress("glDeleteBuffers");
	glBindBufferEXT	= (glBindBufferPROC)wglGetProcAddress("glBindBuffer");
	glBufferDataEXT	= (glBufferDataPROC)wglGetProcAddress("glBufferData");
	glMapBufferEXT = (glMapBufferPROC)wglGetProcAddress("glMapBuffer");
	glUnmapBufferEXT = (glUnmapBufferPROC)wglGetProcAddress("glUnmapBuffer");

	if(glGenBuffersEXT != NULL && glDeleteBuffersEXT != NULL
	&& glBindBufferEXT != NULL && glBufferDataEXT    != NULL
	&& glMapBufferEXT  != NULL && glUnmapBufferEXT   != NULL) {
		return true;
	}
	else {
		return false;
	}
	#endif

#else
	// PBO extensions defined elsewhere
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

	// printf("loadGLextensions\n");

#ifdef USE_GLEW
	InitializeGlew(); // probably needs failure check
#endif

	// Check for FBO extensions first - no use continuing without them
	if(!loadFBOextensions()) {
		printf("    loadFBOextensions fail\n");
		return 0;
	}

	caps |= GLEXT_SUPPORT_FBO;

	// Load PBO extension and FBO blit extension
	if(loadBLITextension()) {
		caps |= GLEXT_SUPPORT_FBO_BLIT;
	}

	if(loadSwapExtensions()) {
		caps |= GLEXT_SUPPORT_SWAP;
	}

	if(loadPBOextensions()) {
		caps |= GLEXT_SUPPORT_PBO;
	}

	// Find out whether bgra extensions are supported at compile and runtime
#ifdef GL_EXT_bgra
	//
	// "isExtensionSupported" code yet to be fully tested for
	// various compilers, operating systems and environments.
	// Activate this code if you are confident that it works OK.
	// 
	// if(isExtensionSupported("GL_EXT_bgra")) {
		caps |= GLEXT_SUPPORT_BGRA;
	// }
#endif

	// Load wgl interop extensions - not needed for memoryshare
	if (loadInteropExtensions()) {
		caps |= GLEXT_SUPPORT_NVINTEROP;
	}

	return caps;

}


//
// Used to determine support for GL_EXT_bgra extensions
// Currently not used
/*
bool isExtensionSupported(const char *extension)
{
	const char * extensionsstr = NULL;
	const char * versionstr = NULL;
	const char * start;
	const char * exc;
	char *where, *terminator;
	int n, i;

	// Extension names should not have spaces.
	where = (char *)strchr(extension, ' ');
	if (where || *extension == '\0')
		return false;

	versionstr = (const char *)glGetString(GL_VERSION);
	// printf("OpenGL version (%s)\n", versionstr);

	extensionsstr = (const char *)glGetString(GL_EXTENSIONS);

	#ifndef GL_NUM_EXTENSIONS
	#define GL_NUM_EXTENSIONS 0x821D // in gl3.h
	#endif

	if(extensionsstr == NULL) {

		// printf("glGetString(GL_VERSION) not supported\n");

		//
		// glGetstring not supported
		//
		// Code adapted from : https://bitbucket.org/Coin3D/coin/issues/54/support-for-opengl-3x-specifically
		//

		typedef GLubyte* (APIENTRY * COIN_PFNGLGETSTRINGIPROC)(GLenum enm, GLuint idx);
		COIN_PFNGLGETSTRINGIPROC glGetStringi = 0;
		glGetStringi = (COIN_PFNGLGETSTRINGIPROC)wglGetProcAddress("glGetStringi");
		if(glGetStringi != NULL) {
			glGetIntegerv(GL_NUM_EXTENSIONS, &n);
			// printf("%d extensions\n", n);
			if(n > 0) {
				for (i = 0; i < n; i++) {
					exc = (const char *)glGetStringi(GL_EXTENSIONS, i);
					if(strcmp(exc, extension) == 0) {
						break;
					}
				}
				if(i < n) {
					// printf("glGetStringi(%d) %s found\n", i, exc);
					return true;
				}
			}
			else {
				// printf("glGetIntegerv(GL_NUM_EXTENSIONS) did not return a value\nso unable to get extensions for this gl driver\n");
			}
		}
		else {
			// printf("glGetString(GL_EXTENSIONS) returned null, but glGetStringi is NULL,\nso unable to get extensions for this gl driver\n");
		}
	} 
	else {

		// printf("glGetString(GL_VERSION) supported\n");

		//
		// glGetString supported
		//
		// Code adapted from : ftp://ftp.sgi.com/opengl/contrib/blythe/advanced99/notes/node395.html
		//

		// It takes a bit of care to be fool-proof about parsing the
		// OpenGL extensions string.  Don't be fooled by sub-strings, etc.
		start = extensionsstr;
		for (;;) {
			where = (char *)strstr((const char *)start, extension);
			if (!where)
				break;
			terminator = where + strlen(extension);
		    if (where == start || *(where - 1) == ' ') {
				if (*terminator == ' ' || *terminator == '\0') {
					*terminator = '\0';
					// printf("Extension %s found\n", where);
					return true;
				}
			}
			start = terminator;
		}
	}

	return false;

}
*/
