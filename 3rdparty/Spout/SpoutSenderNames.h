/*

	spoutSenderNames.h
	Spout sender management

	Thanks and credit to Malcolm Bechard for modifications to this class

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
#ifndef __spoutSenderNames__ // standard way as well
#define __spoutSenderNames__

#include <windowsx.h>
#include <d3d9.h>
#include <d3d11.h>
#include <wingdi.h>
#include <set>
#include <map>
#include <string>
#include <vector>
#include <unordered_map>

#include "SpoutCommon.h"
#include "SpoutSharedMemory.h"

#define SPOUT_WAIT_TIMEOUT 100 // 100 msec wait for events
// Now replaced by a global class variable // #define MaxSenders 10 // Max for list of Sender names
#define SpoutMaxSenderNameLen 256

// The texture information structure that is saved to shared memory
// and used for communication between senders and receivers
// unsigned __int32 is used for compatibility between 32bit and 64bit
// See : http://msdn.microsoft.com/en-us/library/windows/desktop/aa384203%28v=vs.85%29.aspx
// This is also compatible with wyphon : 
// The structure is declared here so that this class is can be independent of opengl
//
// 03.07-16 - Use helper functions for conversion of 64bit HANDLE to unsigned __int32
// and unsigned __int32 to 64bit HANDLE
// https://msdn.microsoft.com/en-us/library/aa384267%28VS.85%29.aspx
// in SpoutGLDXinterop.cpp and SpoutSenderNames
//
struct SharedTextureInfo {
	unsigned __int32 shareHandle;
	unsigned __int32 width;
	unsigned __int32 height;
	DWORD format; // Texture pixel format
	DWORD usage; // not used
	wchar_t description[128]; // Wyhon compatible description (not used)
	unsigned __int32 partnerId; // Wyphon id of partner that shared it with us (not unused)
};


class SPOUT_DLLEXP spoutSenderNames {

	public:

		spoutSenderNames();
		~spoutSenderNames();

		// public functions

		// ------------------------------------------------------------
		// You must first register a sender name being using
		bool RegisterSenderName(const char* senderName);
		bool ReleaseSenderName(const char* senderName);
		bool FindSenderName     (const char* Sendername);

		// ------------------------------------------------------------
		// Functions to retrieve info about the sender set map and the senders in it
		bool GetSenderNames	   (std::set<std::string> *Sendernames);
		int  GetSenderCount();
		bool GetSenderNameInfo (int index, char* sendername, int sendernameMaxSize, unsigned int &width, unsigned int &height, HANDLE &dxShareHandle);

		// ------------------------------------------------------------
		// New for 2.005
		int GetMaxSenders();
		void SetMaxSenders(int maxSenders); // Set the maximum number of senders in a new sender map

		// ------------------------------------------------------------
		// Functions to read and write info to a sender memory map
		bool GetSenderInfo (const char* sendername, unsigned int &width, unsigned int &height, HANDLE &dxShareHandle, DWORD &dwFormat);
		bool SetSenderInfo (const char* sendername, unsigned int width, unsigned int height, HANDLE dxShareHandle, DWORD dwFormat);

		// Generic sender map info retrieval
		bool getSharedInfo (const char* SenderName, SharedTextureInfo* info);
		bool setSharedInfo (const char* SenderName, SharedTextureInfo* info);

		// ------------------------------------------------------------
		// Functions to maintain the active sender
		bool SetActiveSender     (const char* Sendername);
		bool GetActiveSender     (char Sendername[SpoutMaxSenderNameLen]);
		bool GetActiveSenderInfo (SharedTextureInfo* info);
		bool FindActiveSender    (char activename[SpoutMaxSenderNameLen], unsigned int &width, unsigned int &height, HANDLE &hSharehandle, DWORD &dwFormat);

		// ------------------------------------------------------------
		// Functions to Create, Find or Update a sender without initializing DirectX or the GL/DX interop functions
		bool CreateSender (const char *sendername, unsigned int width, unsigned int height, HANDLE hSharehandle, DWORD dwFormat = 0);
		bool UpdateSender (const char *sendername, unsigned int width, unsigned int height, HANDLE hSharehandle, DWORD dwFormat = 0);
		bool CheckSender  (const char *sendername, unsigned int &width, unsigned int &height, HANDLE &hSharehandle, DWORD &dwFormat);
		bool FindSender   (char *sendername, unsigned int &width, unsigned int &height, HANDLE &hSharehandle, DWORD &dwFormat);
		// ------------------------------------------------------------

		// Debug function
		bool SenderDebug (const char *Sendername, int size);

protected:

		// Sender name set management
		bool CreateSenderSet();
		bool GetSenderSet (std::set<std::string>& SenderNames);

		// Active sender management
		bool setActiveSenderName (const char* SenderName);
		bool getActiveSenderName (char SenderName[SpoutMaxSenderNameLen]);


		// Goes through the full list of sender names and cleans up
		// any that shouldn't still be around
		void cleanSenderSet();

		// Functions to manage shared memory map access
		static void readSenderSetFromBuffer(const char* buffer, std::set<std::string>& SenderNames, int maxSenders);
		static void	writeBufferFromSenderSet(const std::set<std::string>& SenderNames, char *buffer, int maxSenders);

		SpoutSharedMemory	m_senderNames;
		SpoutSharedMemory	m_activeSender;

		// This should be a unordered_map of sender names ->SharedMemory
		// to handle multiple inputs and outputs all going through the
		// same spoutSenderNames class
		// Make this a pointer to avoid size differences between compilers
		// if the .dll is compiled with something different
		std::unordered_map<std::string, SpoutSharedMemory*>*	m_senders;
		int m_MaxSenders; // user defined maximum for the number of senders - development testing only

};

#endif
