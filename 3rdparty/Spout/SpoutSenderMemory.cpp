/**

	spoutSenderMemory.cpp

	Spout memory map management for sharing images via shared memory

	LJ - leadedge@adam.com.au

	Thanks and credit to Malcolm Bechard for the SpoutSharedMemory class

	https://github.com/mbechard	

	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	21.08.15 - started class file

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
	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

*/
#include "SpoutSenderMemory.h"
#include <assert.h>

spoutSenderMemory::spoutSenderMemory() {

}

spoutSenderMemory::~spoutSenderMemory() {

	if(senderMem) delete senderMem;
	
}


bool spoutSenderMemory::GetImageSizeFromSharedMemory(const char* sendername, unsigned int &width, unsigned int &height)
{
	char temp[16];

	if(!senderMem) return false;

	char *pBuf = senderMem->Lock();
	if (!pBuf) {
		printf("senderMem lock failed\n");
		return false;
	}

	// printf("senderMem pBuf = [%x]\n", pBuf);
	// printf("%c%c%c%c:%c%c%c%c\n", pBuf[0],  pBuf[1], pBuf[2], pBuf[3], pBuf[4], pBuf[5], pBuf[6], pBuf[7]);

	// Width - 1st 4 bytes
	memcpy((void *)temp, (void *)pBuf, 4);
	temp[4] = 0;
	width = (unsigned int)atoi(temp);
	pBuf += 4;

	// Height - 2nd 4 bytes
	memcpy((void *)temp, (void *)pBuf, 4);
	temp[4] = 0;
	height = (unsigned int)atoi(temp);

	senderMem->Unlock();

	return true;
}


//	Create a sender shared memory map
bool spoutSenderMemory::CreateSenderMemory(const char *sendername, unsigned int width, unsigned int height)
{
	HANDLE hMap = NULL;
	char *pBuffer = NULL;
	DWORD size = 0;
	string namestring = sendername;

	// Create a name for the map from the sendr name
	namestring += "_map";
	printf("CreateSenderMemory : %s (%dx%d) %d\n", namestring.c_str(), width, height, (width*height*4)+8);

	// Create a new shared memory class object for this sender
	senderMem = new SpoutSharedMemory();

	// Create a shared memory map for this sender
	// Allocate enough width, height and RGBA image
	SpoutCreateResult result = senderMem->Create(namestring.c_str(), (width*height*4)+8 );
	if(result == SPOUT_CREATE_FAILED) {
		printf("CreateSenderMemory : failed\n");
		delete senderMem;
		return false;
	}

	return true;
		
} // end CreateSenderMemory


//	SENDER - Update a sender shared memory map size
bool spoutSenderMemory::UpdateSenderMemory(const char *sendername, unsigned int width, unsigned int height)
{
	string namestring = sendername;

	namestring += "_map";

	// Delete the sender shared memory object - Releases mutex and maps
	if(senderMem) delete senderMem;
	senderMem = NULL;

	// Create a new shared memory map for this sender
	SpoutSharedMemory *senderMem = new SpoutSharedMemory();
	SpoutCreateResult result = senderMem->Create(namestring.c_str(), 8 + (width*height*4) );
	if(result == SPOUT_CREATE_FAILED) {
		delete senderMem;
		return false;
	}

	return true;
		
} // end UpdateSenderMemory


//	Close a sender shared memory map
void spoutSenderMemory::CloseSenderMemory(const char *sendername)
{
	// Delete the sender shared memory object - Releases mutex and maps
	if(senderMem) delete senderMem;
	senderMem = NULL;
		
} // end CloseSenderMemory


// SENDER - set image size and pixels to a sender shared memory map
bool spoutSenderMemory::SetSenderMemory(const char* sendername, unsigned int width, unsigned int height, unsigned char *pixels) 
{
	char temp[16];
	char *buf = NULL;

	if(!senderMem) return false;

	char *pBuf = senderMem->Lock();
	if (!pBuf) {
		printf("SetSenderMemory : buffer not found\n");
		return false;
	}

	// printf("SetSenderMemory : %d x %d (%d) [%x]\n", width, height, (width*height+8), pixels);
	buf = pBuf;
	
	// Width - 1st 4 bytes
	sprintf_s(temp, "%4d", width);
	temp[4] = 0;
	memcpy((void *)buf, (void *)temp, 4);
	// printf("%c%c%c%c\n", pBuf[0],  pBuf[1], pBuf[2], pBuf[3]);
	buf += 4;

	// Height - 2nd 4 bytes
	sprintf_s(temp, "%4d", height);
	temp[4] = 0;
	memcpy((void *)buf, (void *)temp, 4);
	// printf("%c%c%c%c %c%c%c%c\n", pBuf[0],  pBuf[1], pBuf[2], pBuf[3], pBuf[4], pBuf[5], pBuf[6], pBuf[7]);
	buf += 4;


	// Image data
	memcpy((void *)buf, (void *)pixels, width*height*4 );
	// printf("%c%c%c%c:%c%c%c%c\n", pBuf[0],  pBuf[1], pBuf[2], pBuf[3], pBuf[4], pBuf[5], pBuf[6], pBuf[7]);

	senderMem->Unlock();

	return true;

} // end SetSenderMemory


// Get image size and pixels from a sender shared memory map
bool spoutSenderMemory::GetSenderMemory(const char* sendername, unsigned int &width, unsigned int &height, unsigned char *pixels) 
{
	char temp[16];

	if(!senderMem) return false;

	char *pBuf = senderMem->Lock();
	if (!pBuf) {
		printf("SpoutSenderMemory::GetSenderMemory - error 2\n");
		return false;
	}

	// Width - 1st 4 bytes
	memcpy((void *)temp, (void *)pBuf, 4);
	temp[4] = 0;
	width = (unsigned int)atoi(temp);
	pBuf += 4;

	// Height - 2nd 4 bytes
	memcpy((void *)temp, (void *)pBuf, 4);
	temp[4] = 0;
	height = (unsigned int)atoi(temp);
	pBuf += 4;

	// Image data
	memcpy((void *)pixels, (void *)pBuf, width*height*4 );

	senderMem->Unlock();

	return true;

} // end GetSenderMemory




void spoutSenderMemory::ReleaseSenderMemory()
{
	// Delete the sender shared memory object - Releases mutex and maps
	if(senderMem) delete senderMem;
	senderMem = NULL;

}

