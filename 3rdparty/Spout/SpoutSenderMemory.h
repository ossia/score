/*

	spoutSenderMemory.h
	Spout memory map management
	For sharing images via shared memory

	LJ - leadedge@adam.com.au

	Thanks and credit to Malcolm Bechard for the SpoutSharedMemory class

	https://github.com/mbechard	

	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
		Copyright (c) 2014-2015, Lynn Jarvis. All rights reserved.

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
#ifndef __spoutSenderMemory__
#define __spoutSenderMemory__

#include <windowsx.h>
#include <set>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>

#include "SpoutCommon.h"
#include "SpoutSharedMemory.h"

// functions will finally go into SpoutGLDXinterop
// #include "SpoutGLDXinterop.h"

using namespace std;

class SPOUT_DLLEXP spoutSenderMemory {

	public:

		spoutSenderMemory();
		~spoutSenderMemory();

		// SpoutSDK.cpp
		// bool InitMemoryShare(const char *sendername, bool bReceiver);

		// SpoutMemoryShare
		// initialize / release shared memory
		// bool Initialize();
		// bool DeInitialize();

		// Texture equivalent
		// bool CreateInterop(hwnd, sendername, width, height, format, true); // true meaning receiver
		// bool CreateMemoryShare (sendername, width, height, true); // true meaning receiver
		// functions will finally go into SpoutGLDXinterop
		// bool CreateMemoryShare(const char *sendername, unsigned int width, unsigned int height, bool bReceiver);

		// Create or Close a sender memory map
		bool CreateSenderMemory (const char *sendername, unsigned int width, unsigned int height);
		bool UpdateSenderMemory (const char* sendername, unsigned int width, unsigned int height);
		void CloseSenderMemory  (const char *sendername);

		// Read and write to a sender memory map
		bool GetSenderMemory (const char* sendername, unsigned int &width, unsigned int &height, unsigned char *pixels);
		bool SetSenderMemory (const char* sendername, unsigned int  width, unsigned int  height, unsigned char *pixels);

		// A receiver - is a memoryshare sender running ?
		bool GetImageSizeFromSharedMemory(const char* sendername, unsigned int &width, unsigned int &height);

		// Close all sender maps
		void ReleaseSenderMemory();


protected:

		// functions will finally go into SpoutGLDXinterop
		// spoutGLDXinterop interop;
		// SharedTextureInfo m_TextureInfo;

		// HANDLE m_hMutex;
		// HANDLE m_hMap;
		// unsigned char *m_pBuffer;
		SpoutSharedMemory *senderMem;

		// std::unordered_map<std::string, SpoutSharedMemory*>*	m_senders;

};

#endif
