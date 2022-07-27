/*

					SpoutFrameCount.h

				Frame counting management

	Copyright (c) 2019-2022. Lynn Jarvis. All rights reserved.

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

#ifndef __spoutFrameCount__
#define __spoutFrameCount__

#include <string>
#include <vector>
#include "SpoutCommon.h"
#include "SpoutSharedMemory.h"


#include <d3d11.h> // for keyed mutex texture access
#pragma comment (lib, "d3d11.lib")

using namespace spoututils;

// USE_CHRONO is defined in SpoutUtils.h
// Note comments about using an early platform toolset
#ifdef USE_CHRONO
#include <chrono> // c++11 timer
#include <thread>
#endif

class SPOUT_DLLEXP spoutFrameCount {

	public:

	spoutFrameCount();
    ~spoutFrameCount();

	// Enable or disable frame counting globally
	void SetFrameCount(bool bEnable);
	// Enable frame counting for this sender
	void EnableFrameCount(const char* SenderName);
	// Disable frame counting
	void DisableFrameCount();
	// Check status of frame counting
	bool IsFrameCountEnabled();
	// Is the received frame new
	bool IsFrameNew();
	// Received frame rate
	double GetSenderFps();
	// Received frame count
	long GetSenderFrame();
	// Frame rate control
	void HoldFps(int fps = 0);

	//
	// Used by other classes
	//

	// Sender increment the semaphore count
	void SetNewFrame();
	// Receiver read the semaphore count
	bool GetNewFrame();
	// For class cleanup functions
	void CleanupFrameCount();

	//
	// Mutex locks including DirectX 11 keyed mutex
	//

	// Test for texture access using a named sender or keyed texture mutex 
	bool CheckTextureAccess(ID3D11Texture2D* D3D11texture = nullptr);
	// Release mutex and allow textureaccess
	void AllowTextureAccess(ID3D11Texture2D* D3D11texture = nullptr);

	//
	// Named mutex for shared texture access
	//

	// Create named mutex for a sender
	bool CreateAccessMutex(const char * SenderName);
	// Release the mutex
	void CloseAccessMutex();
	// Test access using a named mutex
	bool CheckAccess();
	// Allow access after gaining ownership
	void AllowAccess();

	//
	// Sync events
	//

	// Set sync event 
	void SetFrameSync(const char* name);
	// Wait or test for a sync event
	bool WaitFrameSync(const char *name, DWORD dwTimeout = 0);
	// Close sync event
	void CloseFrameSync();

protected:

	// Texture access named mutex
	HANDLE m_hAccessMutex;

	// DX11 texture keyed mutex checks
	bool CheckKeyedAccess(ID3D11Texture2D* D3D11texture);
	void AllowKeyedAccess(ID3D11Texture2D* D3D11texture);
	bool IsKeyedMutex(ID3D11Texture2D* D3D11texture);

	// Frame count semaphore
	bool m_bFrameCount; // Registry setting of frame count
	bool m_bDisabled; // application disable
	bool m_bIsNewFrame; // received frame is new

	HANDLE m_hCountSemaphore; // semaphore handle
	char m_CountSemaphoreName[256]; // semaphore name
	char m_SenderName[256]; // sender currently connected to a receiver
	long m_FrameCount; // sender frame count
	long m_LastFrameCount; // receiver frame comparator
	double m_FrameTimeTotal;
	double m_FrameTimeNumber;
	double m_lastFrame;

	// Sender frame timing
	double m_SenderFps;
	void UpdateSenderFps(long framecount = 0);

	double GetRefreshRate();

	// Fps control
	double m_millisForFrame;

	// Sync event
	HANDLE m_hSyncEvent;
	void OpenFrameSync(const char* SenderName);

#ifdef USE_CHRONO
	// Avoid C4251 warnings in SpoutLibrary by using pointers
	// USE_CHRONO is defined in SpoutUtils.h
	std::chrono::steady_clock::time_point * m_FpsStartPtr;
	std::chrono::steady_clock::time_point * m_FpsEndPtr;
	std::chrono::steady_clock::time_point * m_FrameStartPtr;
	std::chrono::steady_clock::time_point * m_FrameEndPtr;
#endif

	// PC timer
	double PCFreq;
	__int64 CounterStart;
	double m_FrameStart;
	void StartCounter();
	double GetCounter();

};

#endif
