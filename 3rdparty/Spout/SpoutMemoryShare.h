/*

	SpoutMemoryShare.h

	Spout memory map management for sharing images via shared memory
	Revised over original single reader/writer pair

	Thanks and credit to Malcolm Bechard for the SpoutSharedMemory class

	https://github.com/mbechard	

	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
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
#pragma once
#ifndef __spoutMemoryShare__
#define __spoutMemoryShare__

#include <windowsx.h>
#include <string>
#include "SpoutCommon.h"
#include "SpoutSharedMemory.h"

using namespace std;

class SPOUT_DLLEXP spoutMemoryShare {

	public:

		spoutMemoryShare();
		~spoutMemoryShare();

		// Create / Open, Update or Close a sender memory map
		bool CreateSenderMemory (const char *sendername, unsigned int width, unsigned int height);
		bool UpdateSenderMemorySize (const char* sendername, unsigned int width, unsigned int height);
		bool OpenSenderMemory (const char *sendername);
		void CloseSenderMemory ();

		// Retrieve global width and height
		bool GetSenderMemorySize(unsigned int &width, unsigned int &height);

		// Lock and unlock memory and retrieve buffer pointer
		unsigned char * LockSenderMemory();
		void UnlockSenderMemory();

		// Close and release memory object
		void ReleaseSenderMemory ();

protected:

		SpoutSharedMemory *senderMem;
		unsigned int m_Width;
		unsigned int m_Height;

};

#endif
