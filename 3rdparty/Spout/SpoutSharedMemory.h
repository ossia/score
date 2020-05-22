/*

	spoutSharedMemory.h
	
	Thanks and credit to Malcolm Bechard the author of this class

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

#ifndef __SpoutSharedMemory_ // standard way as well
#define __SpoutSharedMemory_

#include "SpoutCommon.h"
#include <windowsx.h>
#include <d3d9.h>
#include <wingdi.h>

enum SpoutCreateResult
{
	SPOUT_CREATE_FAILED = 0,
	SPOUT_CREATE_SUCCESS,
	SPOUT_ALREADY_EXISTS,
	SPOUT_ALREADY_CREATED,
};

class SPOUT_DLLEXP SpoutSharedMemory {
public:
	SpoutSharedMemory();
	~SpoutSharedMemory();


	// Create a new memory segment, or attach to an existing one
	SpoutCreateResult Create(const char* name, int size);

	// Opens an existing one
	bool Open(const char* name);
	void Close();

	// Returns the buffer
	char* Lock();
	void Unlock();

	void Debug();

private:

	char*  m_pBuffer;
	HANDLE m_hMap;
	HANDLE m_hMutex;

	int m_lockCount;

	const char*	m_pName;
	int m_size;

};

#endif
