/**


	Functions to manage pixel buffer copying

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
		========================

		20.08.16 - start class
		23.08.16 - finished first version
		10.10.16 - Added SSE3 detection
				   Optimized SSE2 rgba-bgra function
				   Revise rgb2rgba etc.
		11.10.16 - Added SSSE detection and rgba-bgra function
		04.01.17 - Added rgb2bgra, bgr2bgra, bgra2rgb, bgra2bgr

*/
#include "SpoutCopy.h"

spoutCopy::spoutCopy() {
	m_bSSE2 = false;
	m_bSSE3 = false;
	m_bSSSE3 = false;
	CheckSSE(); // SSE available - sets m_bSSE2, m_bSSE3, m_bSSSE3
}

spoutCopy::~spoutCopy() {

}



void spoutCopy::CopyPixels(const unsigned char *source, 
							unsigned char *dest, 
							unsigned int width, 
							unsigned int height, 
							GLenum glFormat,
							bool bInvert) 
{
	unsigned int Size = width*height*4; // RGBA default

	if (glFormat == GL_RGB || glFormat == GL_BGR_EXT) {
		Size = width*height*3;
	}

	if(bInvert) {
		FlipBuffer(source, dest, width, height, glFormat);
	}
	else {
		if (width < 320 || height < 240) { // Too small for assembler
			memcpy((void *)dest, (void *)source, Size);
		}
		else if ((Size % 16) == 0 && m_bSSE2) { // 16 byte aligned SSE assembler
			memcpy_sse2((void *)dest, (void *)source, Size);
		}
		else if ((Size % 4) == 0) { // 4 byte aligned assembler
			__movsd((unsigned long *)dest, (unsigned long const *)source, Size/4);
		}
		else { // Default is standard memcpy
			memcpy((void *)dest, (void *)source, Size);
		}
	}
}



bool spoutCopy::FlipBuffer(const unsigned char *src, 
						   unsigned char *dst,
						   unsigned int width,
						   unsigned int height,
						   GLenum glFormat)
{
	const unsigned char * From = src;
	unsigned char * To = dst;
	unsigned int pitch = width * 4; // RGBA default

	if (glFormat == GL_RGB || glFormat == GL_BGR_EXT) {
		pitch = width * 3; // RGB format specified
	}

	unsigned int line_s = 0;
	unsigned int line_t = (height - 1)*pitch;

	for (unsigned int y = 0; y<height; y++) {
		if (width < 320 || height < 240) // too small for assembler
			memcpy((void *)(To + line_t), (void *)(From + line_s), pitch);
		else if ((pitch % 16) == 0 && m_bSSE2) // use sse assembler function
			memcpy_sse2((void *)(To + line_t), (void *)(From + line_s), pitch);
		else if ((pitch % 4) == 0) // use 4 byte move assembler function
			__movsd((unsigned long *)(To + line_t), (unsigned long *)(From + line_s), pitch / 4);
		else
			memcpy((void *)(To + line_t), (void *)(From + line_s), pitch);
		line_s += pitch;
		line_t -= pitch;
	}

	return true;
}



//
// Fast memcpy
//
// Original source - William Chan
// (dead link) http://williamchan.ca/portfolio/assembly/ssememcpy/
// See also :
//	http://stackoverflow.com/questions/1715224/very-fast-memcpy-for-image-processing
//	http://www.gamedev.net/topic/502313-special-case---faster-than-memcpy/
//	and others.
//
// Approx 1.7 times speed of memcpy (0.84 msec per frame 1920x1080)
//
void spoutCopy::memcpy_sse2(void* dst, void* src, size_t Size)
{
	char * pSrc = (char *)src;				  // Source buffer
	char * pDst = (char *)dst;				  // Destination buffer
	unsigned int n = (unsigned int)Size >> 7; // Counter = size divided by 128 (8 * 128bit registers)

	__m128i Reg0, Reg1, Reg2, Reg3, Reg4, Reg5, Reg6, Reg7;
	for (unsigned int Index = n; Index > 0; --Index) {

		// SSE2 prefetch
		_mm_prefetch(pSrc + 256, _MM_HINT_NTA);
		_mm_prefetch(pSrc + 256 + 64, _MM_HINT_NTA);

		// move data from src to registers
		// 8 x 128 bit (16 bytes each)
		// Increment source pointer by 16 bytes each
		// for a total of 128 bytes per cycle
		Reg0 = _mm_load_si128((__m128i *)(pSrc));
		Reg1 = _mm_load_si128((__m128i *)(pSrc + 16));
		Reg2 = _mm_load_si128((__m128i *)(pSrc + 32));
		Reg3 = _mm_load_si128((__m128i *)(pSrc + 48));
		Reg4 = _mm_load_si128((__m128i *)(pSrc + 64));
		Reg5 = _mm_load_si128((__m128i *)(pSrc + 80));
		Reg6 = _mm_load_si128((__m128i *)(pSrc + 96));
		Reg7 = _mm_load_si128((__m128i *)(pSrc + 112));

		// move data from registers to dest
		_mm_stream_si128((__m128i *)(pDst), Reg0);
		_mm_stream_si128((__m128i *)(pDst + 16), Reg1);
		_mm_stream_si128((__m128i *)(pDst + 32), Reg2);
		_mm_stream_si128((__m128i *)(pDst + 48), Reg3);
		_mm_stream_si128((__m128i *)(pDst + 64), Reg4);
		_mm_stream_si128((__m128i *)(pDst + 80), Reg5);
		_mm_stream_si128((__m128i *)(pDst + 96), Reg6);
		_mm_stream_si128((__m128i *)(pDst + 112), Reg7);

		pSrc += 128;
		pDst += 128;
	}

}


//
//					CheckSSE()
//
// https://msdn.microsoft.com/en-us/library/hskdteyh.aspx
//
// static bool SSE2 (void) { return CPU_Rep.f_1_EDX_[26]; }
// SSE2 | [bit 26] EDX
// SSE2 = (cpuid03 & (0x1 << 26))
//
// static bool SSE3 (void) { return CPU_Rep.f_1_ECX_[0]; }
// SSE3 | [bit 0] ECX
// SSE3 = (cpuid02 & (0x1)
//
// static bool SSSE3(void) { return CPU_Rep.f_1_ECX_[9]; }
// SSSE3 | [bit 9] ECX
// SSSE3 = (cpuid02 & (0x1 << 9)
//
// SSE4 not currently used :
//
// SSE4.1 and SSE4.2 are extensions of SSE, SSE2, SSE3, and SSSE3. 
// To check if the processor supports SSE4.1, execute CPUID with EAX = 1 as input.
// If bit 19 of ECX is set, then the processor supports SSE4.1.
// To check if the processor supports SSE4.2 instructions for string / text processing,
// PCMPGTQ, and CRC32, execute CPUID with EAX = 1 as input.
// If bit 20 of ECX is set, then the processor supports these SSE4.2 in structions.
//
// static bool SSE41(void) { return CPU_Rep.f_1_ECX_[19]; }
// SSE41 | [bit 19] ECX
// SSE41 = (cpuid02 & (0x1 << 19))
//
// static bool SSE42(void) { return CPU_Rep.f_1_ECX_[20]; }
// SSE42 | [bit 20] ECX
// SSE42 = (cpuid02 & (0x1 << 20))
//
// EAX - CPUInfo[0]
// EBX - CPUInfo[1]
// ECX - CPUInfo[2]
// EDX - CPUInfo[3]
//
// For intrinsics and SSE : https://software.intel.com/sites/landingpage/IntrinsicsGuide/
//
void spoutCopy::CheckSSE()
{
	// An array of four integers that contains the information returned
	// in EAX (0), EBX (1), ECX (2), and EDX (3) about supported features of the CPU.
	int CPUInfo[4] = { -1 };

	//-- Get number of valid info ids
	__cpuid(CPUInfo, 0);
	int nIds = CPUInfo[0];

	//-- Get info for id "1"
	if (nIds >= 1) {
		// SSE2 | [bit 26] EDX
		// SSE2 = (cpuid03 & (0x1 << 26))
		__cpuid(CPUInfo, 1); // EAX = 1 for __cpuid
		m_bSSE2 = ((CPUInfo[3] & (0x1 << 26)) || false);
		// SSE3 | [bit 0] ECX
		// SSE3 = (cpuid02 & (0x1)
		m_bSSE3 = ((CPUInfo[2] & (0x1)) || false);
		// SSSE3 | [bit 9] ECX
		// SSSE3 = (cpuid02 & (0x1 << 9)
		m_bSSSE3 = ((CPUInfo[2] & (0x1 << 9)) || false);
	}

}


//
// rgba2bgra, bgra2rgba
//
void spoutCopy::rgba2bgra(void *rgba_source, void *bgra_dest, unsigned int width, unsigned int height, bool bInvert)
{
	if (m_bSSE2 && m_bSSSE3 && ((width % 16) == 0)) // SSE3 available and 16 byte aligned width
		rgba_bgra_ssse3(rgba_source, bgra_dest, width, height, bInvert);
	else if (m_bSSE2) // SSE2 available
		rgba_bgra_sse2(rgba_source, bgra_dest, width, height, bInvert);
	else
		rgba_bgra(rgba_source, bgra_dest, width, height, bInvert);
}


// Both are swapping red and blue, so use the same function
void spoutCopy::bgra2rgba(void *bgra_source, void *rgba_dest, unsigned int width, unsigned int height, bool bInvert)
{
	rgba2bgra(bgra_source, rgba_dest, width, height, bInvert);
}


// Without SSE
void spoutCopy::rgba_bgra(void *rgba_source, void *bgra_dest, unsigned int width, unsigned int height, bool bInvert)
{
    unsigned __int32 *source = NULL;
    unsigned __int32 *dest = NULL;
	unsigned __int32 rgbapix; // 32bit rgba pixel
	unsigned int x = 0;
	unsigned int y = 0;

    for (y = 0; y < height; y++) {

  		// Start of buffer
		source = (unsigned __int32*)rgba_source; // unsigned int = 4 bytes
        dest   = (unsigned __int32*)bgra_dest;

		// Increment to current line
		if(bInvert) {
			source += (unsigned __int32)(width*height); // end of rgba buffer
			source -= (unsigned __int32)width; // beginning of the last rgba line
			source -= (unsigned __int32)y*width; // current line
			dest   += (unsigned __int32)y*width; // dest is not inverted
		}
		else {
			source += (unsigned __int32)y*width;
			dest   += (unsigned __int32)y*width;
		}

        for (x = 0; x < width; x++) {
            rgbapix = source[x];
            dest[x] = (_rotl(rgbapix, 16) & 0x00ff00ff) | (rgbapix & 0xff00ff00);
        }

	}
} // end rgba_bgra


//
// Adapted from : https://searchcode.com/codesearch/view/5070982/
// 
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// https://chromium.googlesource.com/angle/angle/+/master/LICENSE
//
// All instructions SSE2.
//
void spoutCopy::rgba_bgra_sse2(void *rgba_source, void *bgra_dest, unsigned int width, unsigned int height, bool bInvert)
{
    unsigned __int32 *source = NULL;
    unsigned __int32 *dest = NULL;
	unsigned __int32 rgbapix; // 32bit rgba pixel
	unsigned int x = 0;
	unsigned int y = 0;
    __m128i brMask = _mm_set1_epi32(0x00ff00ff); // argb

    for (y = 0; y < height; y++) {

  		// Start of buffer
		source = (unsigned __int32*)rgba_source; // unsigned int = 4 bytes
        dest   = (unsigned __int32*)bgra_dest;

		// Increment to current line
		if(bInvert) {
			source += (unsigned __int32)(width*height); // end of rgba buffer
			source -= (unsigned __int32)width;          // beginning of the last rgba line
			source -= (unsigned __int32)y*width;        // current line
		}
		else {
			source += (unsigned __int32)y*width;
		}
		dest   += (unsigned __int32)y*width; // dest is not inverted

        // Make output writes aligned
        for (x = 0; ((reinterpret_cast<intptr_t>(&dest[x]) & 15) != 0) && x < width; x++) {
            rgbapix = source[x];
			// rgbapix << 16		: a r g b > g b a r
			//        & 0x00ff00ff  : r g b . > . b . r
			// rgbapix & 0xff00ff00 : a r g b > a . g .
			// result of or			:           a b g r
            dest[x] = (_rotl(rgbapix, 16) & 0x00ff00ff) | (rgbapix & 0xff00ff00);
        }

        for (; x + 3 < width; x += 4) {
            __m128i sourceData = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&source[x]));
            // Mask out g and a, which don't change
            __m128i gaComponents = _mm_andnot_si128(brMask, sourceData);
            // Mask out b and r
            __m128i brComponents = _mm_and_si128(sourceData, brMask);
            // Swap b and r
            __m128i brSwapped = _mm_shufflehi_epi16(_mm_shufflelo_epi16(brComponents, _MM_SHUFFLE(2, 3, 0, 1)), _MM_SHUFFLE(2, 3, 0, 1));
            __m128i result = _mm_or_si128(gaComponents, brSwapped);
            _mm_store_si128(reinterpret_cast<__m128i*>(&dest[x]), result);
        }

        // Perform leftover writes
        for (; x < width; x++) {
            rgbapix = source[x];
            dest[x] = (_rotl(rgbapix, 16) & 0x00ff00ff) | (rgbapix & 0xff00ff00);
        }
    }

} // end rgba_bgra_sse2


//
//	Adapted from a Gist snippet by Aurélien Vallée (NewbiZ) http://newbiz.github.io/
//
//	https://gist.github.com/NewbiZ/5541524
//
//	Approximately 15% faster than SSE2 function
//
void spoutCopy::rgba_bgra_ssse3(void* rgba_source,  void *rgba_dest, unsigned int width, unsigned int height, bool bInvert)
{
	unsigned __int32* source = (unsigned int*)rgba_source;
	unsigned __int32* dest = (unsigned int*)rgba_dest;
	unsigned int PIXEL_COUNT = width;
	// unsigned int x = 0;
	unsigned int y = 0;
	__m128i* src = NULL;
	__m128i* dst = NULL;

	// Shuffling mask (RGBA -> BGRA) x 4, in reverse byte order
	static const __m128i m = _mm_set_epi8(15,12,13,14,11,8,9,10,7,4,5,6,3,0,1,2);

    for (y = 0; y < height; y++) {

		// Start of buffer
		source = (unsigned __int32*)rgba_source; // unsigned int = 4 bytes
        dest   = (unsigned __int32*)rgba_dest;

		// Increment to current line
		if(bInvert) {
			source += (unsigned __int32)(width*height); // end of rgba buffer
			source -= (unsigned __int32)width;          // beginning of the last rgba line
			source -= (unsigned __int32)y*width;        // current line
		}
		else {
			source += (unsigned __int32)y*width;
		}
		dest   += (unsigned __int32)y*width; // dest is not inverted

		// Assert pixels will NOT be aliased here : TODO
		// __m128i* __restrict__ pix = (__m128i*)pixels;
		src = (__m128i*)source;
		dst = (__m128i*)dest;

		// Tile the LHS to match 64B cache line size
		for (; (unsigned int*)src < source+PIXEL_COUNT; src+=4, dst+=4)  {

			__m128i p1 = _mm_load_si128(src); // SSE2
			__m128i p2 = _mm_load_si128(src+1);
			__m128i p3 = _mm_load_si128(src+2);
			__m128i p4 = _mm_load_si128(src+3);

			p1 = _mm_shuffle_epi8(p1, m); // SSSE3
			p2 = _mm_shuffle_epi8(p2, m);
			p3 = _mm_shuffle_epi8(p3, m);
			p4 = _mm_shuffle_epi8(p4, m);

			_mm_store_si128(dst,   p1); // SSE2
			_mm_store_si128(dst+1, p2);
			_mm_store_si128(dst+2, p3);
			_mm_store_si128(dst+3, p4);

		}
	}
} // end rgba_bgra_ssse3



//
// rgb2rgba, bgr2rgba, rgba2rgb, rgba2bgr, rgb2bgra
//

void spoutCopy::rgb2rgba(void *rgb_source, void *rgba_dest, unsigned int width, unsigned int height, bool bInvert)
{
	unsigned long rgbsize = width*height*3;
	unsigned long rgbpitch = width*3;
	unsigned int x = 0;
	unsigned int y = 0;

	// Start of buffers
	unsigned char *rgb  = (unsigned char *)rgb_source; // RGB
	unsigned char *rgba = (unsigned char *)rgba_dest; // RGBA
	if(bInvert) {
		rgb += rgbsize; // end of rgb buffer
		rgb -= rgbpitch; // beginning of the last rgb line
	}

	for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
			// rgb source - rgba dest
			*(rgba + 0) = *(rgb + 0); // red
			*(rgba + 1) = *(rgb + 1); // grn
			*(rgba + 2) = *(rgb + 2); // blu
			*(rgba + 3) =  (unsigned char)255; // alpha
			rgb += 3;
			rgba += 4;
        }
		if(bInvert)
			rgb -= rgbpitch*2; // move up a line for invert
	}

} // end rgb2rgba



void spoutCopy::bgr2rgba(void *bgr_source, void *rgba_dest, unsigned int width, unsigned int height, bool bInvert)
{
	unsigned long rgbsize = width*height*3;
	unsigned long rgbpitch = width*3;
	unsigned int x = 0;
	unsigned int y = 0;

	// Start of buffers
	unsigned char *rgb  = (unsigned char *)bgr_source; // RGB
	unsigned char *rgba = (unsigned char *)rgba_dest; // RGBA
	if(bInvert) {
		rgb += rgbsize; // end of rgb buffer
		rgb -= rgbpitch; // beginning of the last rgb line
	}

	for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
			// bgr source - rgba dest
			*(rgba + 0) = *(rgb + 2); // blu 
			*(rgba + 1) = *(rgb + 1); // grn
			*(rgba + 2) = *(rgb + 0); // red
			*(rgba + 3) =  (unsigned char)255; // alpha
			rgb += 3;
			rgba += 4;
        }
		if(bInvert)
			rgb -= rgbpitch*2; // move up a line for invert
	}

} // end bgr2rgba


void spoutCopy::rgb2bgra(void *rgb_source, void *bgra_dest, unsigned int width, unsigned int height, bool bInvert)
{
	unsigned long rgbsize = width*height*3;
	unsigned long rgbpitch = width*3;
	unsigned int x = 0;
	unsigned int y = 0;

	// Start of buffers
	unsigned char *rgb  = (unsigned char *)rgb_source; // RGB
	unsigned char *bgra = (unsigned char *)bgra_dest; // BGRA
	if(bInvert) {
		rgb += rgbsize; // end of rgb buffer
		rgb -= rgbpitch; // beginning of the last rgb line
	}

	for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
			// rgb source - bgra dest
			*(bgra + 0) = *(rgb + 2); // red
			*(bgra + 1) = *(rgb + 1); // grn
			*(bgra + 2) = *(rgb + 0); // blu
			*(bgra + 3) =  (unsigned char)255; // alpha
			rgb += 3;
			bgra += 4;
        }
		if(bInvert)
			rgb -= rgbpitch*2; // move up a line for invert
	}

} // end rgb2bgra


void spoutCopy::bgr2bgra(void *bgr_source, void *bgra_dest, unsigned int width, unsigned int height, bool bInvert)
{
	unsigned long bgrsize = width*height*3;
	unsigned long bgrpitch = width*3;
	unsigned int x = 0;
	unsigned int y = 0;

	// Start of buffers
	unsigned char *bgr  = (unsigned char *)bgr_source; // BGR
	unsigned char *bgra = (unsigned char *)bgra_dest;  // BGRA
	if(bInvert) {
		bgr += bgrsize; // end of rgb buffer
		bgr -= bgrpitch; // beginning of the last rgb line
	}

	for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
			// bgr source - bgra dest
			*(bgra + 0) = *(bgr + 0); // blu
			*(bgra + 1) = *(bgr + 1); // grn
			*(bgra + 2) = *(bgr + 2); // red
			*(bgra + 3) =  (unsigned char)255; // alpha
			bgr += 3;
			bgra += 4;
        }
		if(bInvert)
			bgr -= bgrpitch*2; // move up a line for invert
	}

} // end bgr2bgra



void spoutCopy::rgba2rgb(void *rgba_source, void *rgb_dest, unsigned int width, unsigned int height, bool bInvert)
{
	unsigned long rgbsize = width*height*3;
	unsigned long rgbpitch = width*3;
	unsigned int x = 0;
	unsigned int y = 0;

	// Start of buffers
	unsigned char *rgba = (unsigned char *)rgba_source; // RGBA
	unsigned char *rgb  = (unsigned char *)rgb_dest; // RGB
	if(bInvert) {
		rgb += rgbsize; // end of rgb buffer
		rgb -= rgbpitch; // beginning of the last rgb line
	}

	for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
			// rgba source - rgb dest
			*(rgb + 0) = *(rgba + 0); // red
			*(rgb + 1) = *(rgba + 1); // grn
			*(rgb + 2) = *(rgba + 2); // blu
			rgb += 3;
			rgba += 4;
        }
		if(bInvert)
			rgb -= rgbpitch*2; // move up a line for invert
	}

} // end rgba2rgb


void spoutCopy::rgba2bgr(void *rgba_source, void *bgr_dest, unsigned int width, unsigned int height, bool bInvert)
{
	unsigned long rgbsize = width*height*3;
	unsigned long rgbpitch = width*3;
	unsigned int x = 0;
	unsigned int y = 0;

	// Start of buffers
	unsigned char *rgba = (unsigned char *)rgba_source; // RGBA
	unsigned char *rgb  = (unsigned char *)bgr_dest; // RGB
	if(bInvert) {
		rgb += rgbsize; // end of rgb buffer
		rgb -= rgbpitch; // beginning of the last rgb line
	}

	for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
			// rgba source - bgr dest
			*(rgb + 0) = *(rgba + 2); // blu
			*(rgb + 1) = *(rgba + 1); // grn
			*(rgb + 2) = *(rgba + 0); // red
			rgb += 3;
			rgba += 4;
        }
		if(bInvert)
			rgb -= rgbpitch*2; // move up a line for invert
	}

} // end rgba2bgr



void spoutCopy::bgra2rgb(void *bgra_source, void *rgb_dest, unsigned int width, unsigned int height, bool bInvert)
{
	unsigned long rgbsize = width*height*3;
	unsigned long rgbpitch = width*3;
	unsigned int x = 0;
	unsigned int y = 0;

	// Start of buffers
	unsigned char *bgra = (unsigned char *)bgra_source; // BGRA
	unsigned char *rgb  = (unsigned char *)rgb_dest; // RGB
	if(bInvert) {
		rgb += rgbsize; // end of rgb buffer
		rgb -= rgbpitch; // beginning of the last rgb line
	}

	for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
			// bgra source - rgb dest
			*(rgb + 0) = *(bgra + 2); // red
			*(rgb + 1) = *(bgra + 1); // grn
			*(rgb + 2) = *(bgra + 0); // blu
			rgb += 3;
			bgra += 4;
        }
		if(bInvert)
			rgb -= rgbpitch*2; // move up a line for invert
	}

} // end bgra2rgb



void spoutCopy::bgra2bgr(void *bgra_source, void *bgr_dest, unsigned int width, unsigned int height, bool bInvert)
{
	unsigned long bgrsize = width*height*3;
	unsigned long bgrpitch = width*3;
	unsigned int x = 0;
	unsigned int y = 0;

	// Start of buffers
	unsigned char *bgra = (unsigned char *)bgra_source; // BGRA
	unsigned char *bgr  = (unsigned char *)bgr_dest; // BGR
	if(bInvert) {
		bgr += bgrsize; // end of bgr buffer
		bgr -= bgrpitch; // beginning of the last bgr line
	}

	for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
			// bgra source - bgr dest
			*(bgr + 2) = *(bgra + 2); // red
			*(bgr + 1) = *(bgra + 1); // grn
			*(bgr + 0) = *(bgra + 0); // blu
			bgr += 3;
			bgra += 4;
        }
		if(bInvert)
			bgr -= bgrpitch*2; // move up a line for invert
	}

} // end bgra2bgr

