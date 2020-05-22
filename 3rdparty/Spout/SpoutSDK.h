/*

	SpoutSDK.h
 
	The main SDK include file


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
#ifndef __SpoutSDK__
#define __SpoutSDK__

#include <windows.h>
#include <set>
#include <string>
#include <iostream>
#include <fstream>
#include <Mmsystem.h> // for timegettime
#include <direct.h>   // for _getcwd
#include <shlwapi.h>  // for path functions
#include "Shellapi.h" // for shellexecute

#pragma comment(lib, "shlwapi.lib")  // for path functions
#pragma comment(lib, "Shell32.lib")  // for shellexecute
#pragma comment(lib, "Advapi32.lib") // for registry functions
#pragma comment(lib, "Version.lib")  // for VersionInfo API


#include "SpoutCommon.h"
#include "SpoutMemoryShare.h"
#include "SpoutSenderNames.h"
#include "SpoutGLDXinterop.h"

// Compile flag only - not currently used
#if defined(__x86_64__) || defined(_M_X64)
	#define is64bit
// #elif defined(__i386) || defined(_M_IX86)
//	x86 32-bit
#endif

class SPOUT_DLLEXP Spout {

	public:

	Spout();
    ~Spout();

	spoutGLDXinterop interop; // Opengl/directx interop texture sharing object

	// ================== //
	//	PUBLIC FUNCTIONS  //
	// ================== //

	// Sender
	bool CreateSender  (const char *Sendername, unsigned int width, unsigned int height, DWORD dwFormat = 0);
	bool UpdateSender  (const char* Sendername, unsigned int width, unsigned int height);
	void ReleaseSender (DWORD dwMsec = 0);

	// Receiver
	bool CreateReceiver (char* Sendername, unsigned int &width, unsigned int &height, bool bUseActive = false);
	void ReleaseReceiver(); 
	bool CheckReceiver	(char* Sendername, unsigned int &width, unsigned int &height, bool &bConnected);
	bool GetImageSize   (char* sendername, unsigned int &width, unsigned int &height, bool &mMemoryMode);	

	// Texture functions
	bool SendTexture    (GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert=true, GLuint HostFBO=0);
	bool SendImage      (const unsigned char* pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert=true, GLuint HostFBO = 0);
	bool ReceiveTexture (char* Sendername, unsigned int &width, unsigned int &height, GLuint TextureID = 0, GLuint TextureTarget = 0, bool bInvert = false, GLuint HostFBO=0);
	bool ReceiveImage   (char* Sendername, unsigned int &width, unsigned int &height, unsigned char* pixels, GLenum glFormat = GL_RGBA, bool bInvert = false, GLuint HostFBO=0);
	bool DrawSharedTexture(float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = true, GLuint HostFBO = 0);
	bool DrawToSharedTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = false, GLuint HostFBO = 0);
	bool BindSharedTexture();
	bool UnBindSharedTexture();

	int  GetSenderCount ();
	bool GetSenderName  (int index, char* sendername, int MaxSize = 256);
	bool GetSenderInfo  (const char* sendername, unsigned int &width, unsigned int &height, HANDLE &dxShareHandle, DWORD &dwFormat);
	bool GetActiveSender(char* Sendername);
	bool SetActiveSender(const char* Sendername);
	
	// Utilities
	bool SetDX9(bool bDX9 = true); // User request to use DirectX 9 (default is DirectX 11)
	bool GetDX9(); // Return the flag that has been set
	bool SetMemoryShareMode(bool bMem = true);
	bool GetMemoryShareMode();
	bool SetCPUmode(bool bCPU = true);
	bool GetCPUmode();
	int  GetShareMode();
	bool SetShareMode(int mode);
	int  GetMaxSenders(); // Get maximum senders allowed
	void SetMaxSenders(int maxSenders); // Set maximum senders allowed
	
	// Access to globals
	bool GetSpoutSenderName(char * sendername, int maxchars); // get the global sender name
	bool IsSpoutInitialized(); // has the class been initialized
	bool IsBGRAavailable(); // Are bgra extensions supported (in interop class)
	bool IsPBOavailable(); // Are pbo extensions supported (in interop class)
	void SetBufferMode(bool bActive); // Set the pbo availability on or off
	bool GetBufferMode();

	// Adapter functions
	int  GetNumAdapters(); // Get the number of graphics adapters in the system
	bool GetAdapterName(int index, char *adaptername, int maxchars); // Get an adapter name
	bool SetAdapter(int index = 0); // Set required graphics adapter for output
	int  GetAdapter(); // Get the SpoutDirectX global adapter index

	bool GetHostPath(const char *sendername, char *hostpath, int maxchars); // The path of the host that produced the sender

	int  GetVerticalSync();
	bool SetVerticalSync(bool bSync = true);
	bool SelectSenderPanel(const char* message = NULL);

	bool CheckSpoutPanel(); // Public for debugging
	bool OpenSpout(); // Public for debugging
	
	// Registry read/write
	bool WritePathToRegistry (const char *filepath, const char *subkey, const char *valuename);
	bool ReadPathFromRegistry(char *filepath, const char *subkey, const char *valuename);
	bool RemovePathFromRegistry(const char *subkey, const char *valuename);

	// Public for debugging only
	void UseAccessLocks(bool bUseLocks); // to disable/enable texture access locks in SpoutDirectX.cpp
	void SpoutCleanUp(bool bExit = false);
	void CleanSenders();
	int ReportMemory();

/*
//
//
// http://msdn.microsoft.com/en-us/library/windows/desktop/bb172558%28v=vs.85%29.aspx
//
// Compatible DX11/DX9 format for Texture2D
// http://msdn.microsoft.com/en-us/library/windows/desktop/ff471324%28v=vs.85%29.aspx
//
// DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, DXGI_FORMAT_B8G8R8A8_UNORM
// are compatible with DX9 - D3DFMT_A8B8G8R8
//
// Noted that DX11 -> DX9 only works if the DX11 format is set to DXGI_FORMAT_B8G8R8A8_UNORM
// if the DX9 format is set to D3DFMT_A8B8G8R8

	DXGI_FORMAT_R8G8B8A8_TYPELESS			= 27,
    DXGI_FORMAT_R8G8B8A8_UNORM              = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
    DXGI_FORMAT_R8G8B8A8_UINT               = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM              = 31,
    DXGI_FORMAT_R8G8B8A8_SINT               = 32,
    DXGI_FORMAT_B8G8R8A8_UNORM              = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM              = 88,
    DXGI_FORMAT_B8G8R8A8_TYPELESS           = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS           = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,

*/

	protected :

	// ================================= //
	//  PRIVATE VARIABLES AND FUNCTIONS  //
	// ================================= //
	char g_SharedMemoryName[256];
	char UserSenderName[256]; // used for the sender selection dialog
	unsigned int g_Width;
	unsigned int g_Height;
	HANDLE g_ShareHandle;
	DWORD g_Format;
	GLuint g_TexID;
	HWND g_hWnd;
	bool bGLDXcompatible;
	bool bMemoryShareInitOK;
	bool bDxInitOK;
	bool bUseCPU;
	bool bMemory; // force memoryshare flag
	bool bInitialized;
	bool bIsSending;
	bool bIsReceiving;
	bool bChangeRequested;
	bool bSpoutPanelOpened;
	bool bSpoutPanelActive;
	bool bUseActive; // Use the active sender for CreateReceiver
	SHELLEXECUTEINFOA m_ShExecInfo;

	bool GLDXcompatible();
	bool OpenReceiver (char *name, unsigned int& width, unsigned int& height);
	bool InitReceiver (HWND hwnd, char* sendername, unsigned int width, unsigned int height, bool bMemoryMode);
	bool InitSender   (HWND hwnd, const char* sendername, unsigned int width, unsigned int height, DWORD dwFormat, bool bMemoryMode);
	bool InitMemoryShare(bool bReceiver);
	bool ReleaseMemoryShare();

	// Find a file version
	bool FindFileVersion(const char *filepath, DWORD &versMS, DWORD &versLS);

};

#endif


