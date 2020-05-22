/**

	SpoutMemoryShare.cpp

	Spout memory map management for sharing images via shared memory
	Revised over original single reader/writer pair

	Thanks and credit to Malcolm Bechard for the SpoutSharedMemory class

	https://github.com/mbechard	

	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
	21.08.15 - started class file
	25.09.15 - set sendermem object pointer to NULL in constructor
	11.10.15 - introduced global width and height and GetSenderMemorySize function
	29.02.16 - cleanup
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
	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

*/
#include "SpoutMemoryShare.h"
#include <assert.h>

spoutMemoryShare::spoutMemoryShare() {
	senderMem = NULL; // Important because this is checked
	m_Width = 0;
	m_Height = 0;
}

spoutMemoryShare::~spoutMemoryShare() {

	if(senderMem) delete senderMem;
	senderMem = NULL;
	m_Width = 0;
	m_Height = 0;
	
}


// SENDER : Create a sender named shared memory map
// RECEIVER : Attach to an existing named shared memory map
bool spoutMemoryShare::CreateSenderMemory(const char *sendername, unsigned int width, unsigned int height)
{
	string namestring = sendername;

	// Create a name for the map from the sender name
	namestring += "_map";

	// Create a new shared memory class object for this sender
	if(senderMem) delete senderMem;
	senderMem = new SpoutSharedMemory();

	// Create a shared memory map for this sender
	// Allocate enough width*height*4 - RGBA image
	SpoutCreateResult result = senderMem->Create(namestring.c_str(), width*height*4 );

	if(result == SPOUT_CREATE_FAILED) {
		delete senderMem;
		senderMem = NULL;
		m_Width = 0;
		m_Height = 0;
		return false;
	}
	
	// Set the global width and height for future reference
	m_Width = width;
	m_Height = height;

	return true;
		
} // end CreateSenderMemory


// SENDER : Update a sender shared memory map size
// Only the sender can update the memory map size by creating a new one because 
// the sender creates the map and it's map handle cannot be released by a receiver,
// so a new map cannot be created by a receiver.
bool spoutMemoryShare::UpdateSenderMemorySize(const char *sendername, unsigned int width, unsigned int height)
{
	string namestring = sendername;

	namestring += "_map";

	// Delete the existing sender shared memory object - Releases mutex and maps
	if(senderMem) delete senderMem;
	// Create a new shared memory map for this sender
	senderMem = new SpoutSharedMemory();

	SpoutCreateResult result = senderMem->Create(namestring.c_str(), width*height*4 );
	if(result == SPOUT_CREATE_FAILED) {
		delete senderMem;
		senderMem = NULL;
		m_Width = 0;
		m_Height = 0;
		return false;
	}

	// Reset the global width and height
	m_Width = width;
	m_Height = height;

	return true;
		
} // end UpdateSenderMemorySize



// RECEIVER : Open an existing named shared memory map
bool spoutMemoryShare::OpenSenderMemory(const char *sendername)
{
	string namestring = sendername;

	// Create a name for the map from the sender name
	namestring += "_map";

	// Create a new shared memory class object for this receiver
	if(!senderMem) senderMem = new SpoutSharedMemory();

	if(!senderMem->Open(namestring.c_str()) ) {
		return false;
	}

	// TODO : Set the global width and height - how ?
	// m_Width = width;
	// m_Height = height;

	return true;
		
} // end OpenSenderMemory


// Return the global shared memory size
bool spoutMemoryShare::GetSenderMemorySize(unsigned int &width, unsigned int &height)
{
	if(m_Width == 0 || m_Height == 0) return false;

	width = m_Width;
	height = m_Height;

	return true;
		
} // end GetSenderMemorySize



//	Close the sender shared memory map
void spoutMemoryShare::CloseSenderMemory()
{
	if(senderMem) senderMem->Close();
	m_Width = 0;
	m_Height = 0;
		
} // end CloseSenderMemory



void spoutMemoryShare::ReleaseSenderMemory()
{
	// Delete the sender shared memory object - Releases mutex and maps
	if(senderMem) delete senderMem;
	senderMem = NULL;
	m_Width = 0;
	m_Height = 0;

}

// Lock and unlock memory and retrieve buffer pointer - no size checks
unsigned char * spoutMemoryShare::LockSenderMemory() 
{
	if(!senderMem) return NULL;

	char *pBuf = senderMem->Lock();
	if (!pBuf) {
		// https://github.com/leadedge/Spout2/issues/15
		// senderMem->Unlock();
		return NULL;
	}

	return (unsigned char *)pBuf;

}

void spoutMemoryShare::UnlockSenderMemory() 
{
	if(!senderMem) return;

	senderMem->Unlock();
}

