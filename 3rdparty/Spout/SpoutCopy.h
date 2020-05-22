/*

									SpoutCopy.h

		Functions to manage pixel buffer copying and rgb/rgba <> bgr/bgra conversion

		- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		Copyright (c) 2016-2017, Lynn Jarvis. All rights reserved.

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
#ifndef __spoutCopy__ // standard way as well
#define __spoutCopy__

#include "SpoutCommon.h"
#include <windows.h>
#include <stdio.h> // for debug printf
#include <gl/gl.h> // For OpenGL definitions
#include <intrin.h> // for cpuid to test for SSE2
#include <emmintrin.h> // for SSE2
#include <tmmintrin.h> // for SSSE3


class SPOUT_DLLEXP spoutCopy {

	public:

		spoutCopy();
		~spoutCopy();

		void CopyPixels(const unsigned char *src, unsigned char *dst,
						unsigned int width, unsigned int height, 
						GLenum glFormat = GL_RGBA, bool bInvert = false);

		bool FlipBuffer(const unsigned char *src, unsigned char *dst,
						unsigned int width, unsigned int height,
						GLenum glFormat = GL_RGBA);

		void memcpy_sse2(void* dst, void* src, size_t size);

		void rgba2bgra(void* rgba_source, void *bgra_dest, unsigned int width, unsigned int height, bool bInvert = false);
		void bgra2rgba(void* bgra_source, void *rgba_dest, unsigned int width, unsigned int height, bool bInvert = false);

		void rgba_bgra(void *rgba_source, void *bgra_dest, unsigned int width, unsigned int height, bool bInvert = false);
		void rgba_bgra_sse2(void *rgba_source, void *rgba_dest, unsigned int width, unsigned int height, bool bInvert = false);
		void rgba_bgra_ssse3(void *rgba_source, void *rgba_dest, unsigned int width, unsigned int height, bool bInvert = false);
		
		// TODO avoid redundancy
		void rgb2rgba (void* rgb_source, void *rgba_dest, unsigned int width, unsigned int height, bool bInvert = false);
		void bgr2rgba (void* bgr_source, void *rgba_dest, unsigned int width, unsigned int height, bool bInvert = false);

		void rgb2bgra (void* rgb_source, void *bgra_dest, unsigned int width, unsigned int height, bool bInvert = false);
		void bgr2bgra (void* bgr_source, void *bgra_dest, unsigned int width, unsigned int height, bool bInvert = false);

		void rgba2rgb (void* rgba_source, void *rgb_dest,  unsigned int width, unsigned int height, bool bInvert = false);
		void rgba2bgr (void* rgba_source, void *bgr_dest,  unsigned int width, unsigned int height, bool bInvert = false);

		void bgra2rgb (void* bgra_source, void *rgb_dest,  unsigned int width, unsigned int height, bool bInvert = false);
		void bgra2bgr (void* bgra_source, void *bgr_dest,  unsigned int width, unsigned int height, bool bInvert = false);

	private :

		void CheckSSE();
		bool m_bSSE2;
		bool m_bSSE3;
		bool m_bSSSE3;

};

#endif
