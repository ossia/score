/*

					SpoutReceiver.h

	Copyright (c) 2014-2025, Lynn Jarvis. All rights reserved.

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

#ifndef __SpoutReceiver__
#define __SpoutReceiver__

#include "Spout.h"

class SPOUT_DLLEXP SpoutReceiver {

	public:

	SpoutReceiver();
    ~SpoutReceiver();

	// Specify sender for connection
	//   The application will not connect to any other  unless the user selects one
	//   If that sender closes, the application will wait for the nominated sender to open 
	//   If no name is specified, the receiver will connect to the active sender
	void SetReceiverName(const char* sendername = nullptr);
	// Get sender for connection
	bool GetReceiverName(char* SenderName, int maxchars = 256);
	// Close receiver and release resources ready to connect to another sender
	void ReleaseReceiver();
	// Receive shared texture
	//   Connect to a sender and retrieve texture details ready for access
	//	 (see BindSharedTexture and UnBindSharedTexture)
	bool ReceiveTexture();
	// Receive OpenGL texture
	// 	 Connect to a sender and inform the application to update
	//   the receiving texture if it has changed dimensions
	//   For no change, copy the sender shared texture to the application texture
	bool ReceiveTexture(GLuint TextureID, GLuint TextureTarget, bool bInvert = false, GLuint HostFbo = 0);
	// Receive image pixels
	//   Connect to a sender and inform the application to update
	//   the receiving buffer if it has changed dimensions
	//   For no change, copy the sender shared texture to the pixel buffer
	bool ReceiveImage(unsigned char* pixels, GLenum glFormat = GL_RGBA, bool bInvert = false, GLuint HostFbo = 0);
	// Query whether the sender has changed
	//   Checked at every cycle before receiving data
	bool IsUpdated();
	// Query sender connection
	//   If the sender closes, receiving functions return false,  
	bool IsConnected();
	// Query received frame status
	//   The receiving texture or pixel buffer is only refreshed if the sender has produced a new frame  
	//   This can be queried to process texture data only for new frames
	bool IsFrameNew();
	// Received sender name
	const char* GetSenderName();
	// Received sender width
	unsigned int GetSenderWidth();
	// Received sender height
	unsigned int GetSenderHeight();
	// Received sender DX11 texture format
	DWORD GetSenderFormat();
	// Received sender frame rate
	double GetSenderFps();
	// Received sender frame number
	long GetSenderFrame();
	// Received sender share handle
	HANDLE GetSenderHandle();
	// Received sender texture
	ID3D11Texture2D* GetSenderTexture();
	// Received sender sharing method
	bool GetSenderCPU();
	// Received sender GL/DX hardware compatibility
	bool GetSenderGLDX();
	// Return a list of current senders
	std::vector<std::string> GetSenderList();
	// Open sender selection dialog
	bool SelectSender(HWND hwnd = NULL);

	//
	// Frame count
	//

	// Enable or disable frame counting globally
	void SetFrameCount(bool bEnable);
	// Disable frame counting specifically for this application
	void DisableFrameCount();
	// Return frame count status
	bool IsFrameCountEnabled();
	// Frame rate control
	void HoldFps(int fps);
	// Signal sync event 
	void SetFrameSync(const char* SenderName);
	// Wait or test for a sync event
	bool WaitFrameSync(const char* SenderName, DWORD dwTimeout = 0);
	// Enable / disable frame sync
	void EnableFrameSync(bool bSync = true);
	// Close frame sync
	void CloseFrameSync();
	// Check for frame sync option
	bool IsFrameSyncEnabled();

	//
	// Data sharing
	//

	// Read data
	int ReadMemoryBuffer(const char* name, char* data, int maxlength);
	// Get the size of a shared memory buffer
	int GetMemoryBufferSize(const char* name);

	//
	// OpenGL shared texture access
	//

	// Bind OpenGL shared texture
	bool BindSharedTexture();
	// Un-bind OpenGL shared texture
	bool UnBindSharedTexture();
	// OpenGL shared texture ID
	GLuint GetSharedTextureID();

	//
	// Graphics compatibility
	//

	// Get user auto GPU/CPU share
	bool GetAutoShare();
	// Set application auto GPU/CPU share
	void SetAutoShare(bool bAuto = true);
	// Get user CPU share
	bool GetCPUshare();
	// Set application CPU share
	// (re-test GL/DX compatibility if set to false)
	void SetCPUshare(bool bCPU = true);
	// OpenGL texture share compatibility
	bool IsGLDXready();

	//
	// Sender names
	//

	// Number of senders
	int GetSenderCount();
	// Sender item name
	bool GetSender(int index, char* sendername, int MaxSize = 256);
	// Sender information
	bool GetSenderInfo(const char* sendername, unsigned int &width, unsigned int &height, HANDLE &dxShareHandle, DWORD &dwFormat);
	// Current active sender
	bool GetActiveSender(char* sendername);
	// Set sender as active
	bool SetActiveSender(const char* sendername);

	//
	// Adapter functions
	//

	// The number of graphics adapters in the system
	int GetNumAdapters();
	// Get adapter item name
	bool GetAdapterName(int index, char* adaptername, int maxchars = 256);
	// Current adapter name
	char* AdapterName();
	// Get current adapter index
	int GetAdapter();
	// Get the description and output display name of the current adapter
	bool GetAdapterInfo(char* description, char* output, int maxchars);
	// Get the description and output display name for a given adapter
	bool GetAdapterInfo(int index, char* description, char* output, int maxchars);
	// Windows 10 Vers 1803, build 17134 or later
#ifdef NTDDI_WIN10_RS4
	// Get the Windows graphics preference for an application
	int GetPerformancePreference(const char* path = nullptr);
	// Set the Windows graphics preference for an application
	bool SetPerformancePreference(int preference, const char* path = nullptr);
	// Get the graphics adapter name for a Windows preference
	bool GetPreferredAdapterName(int preference, char* adaptername, int maxchars);
	// Set graphics adapter index for a Windows preference
	bool SetPreferredAdapter(int preference);
	// Availability of Windows graphics preference
	bool IsPreferenceAvailable();
	// Is the path a valid application
	bool IsApplicationPath(const char* path);
#endif

	//
	// User settings recorded by "SpoutSettings"
	//

	// Get user buffering mode
	bool GetBufferMode();
	// Set application buffering mode
	void SetBufferMode(bool bActive = true);
	// Get user number of pixel buffers
	int GetBuffers();
	// Set application number of pixel buffers
	void SetBuffers(int nBuffers);
	// Get user Maximum senders allowed
	int GetMaxSenders();
	// Set user Maximum senders allowed
	void SetMaxSenders(int maxSenders);

	//
	// 2.006 compatibility
	//

	// Get user DX9 mode
	bool GetDX9();
	// Set user DX9 mode
	bool SetDX9(bool bDX9 = true);
	// Get user memory share mode
	bool GetMemoryShareMode();
	// Set user memory share mode
	bool SetMemoryShareMode(bool bMem = true);
	// Get user CPU mode
	bool GetCPUmode();
	// Set user CPU mode
	bool SetCPUmode(bool bCPU);
	// Get user share mode
	//  0 - texture, 1 - memory, 2 - CPU
	int GetShareMode();
	// Set user share mode
	//  0 - texture, 1 - memory, 2 - CPU
	void SetShareMode(int mode);

	//
	// Information
	//

	// The path of the host that produced the sender
	bool GetHostPath(const char* sendername, char* hostpath, int maxchars);
	// Vertical sync status
	int GetVerticalSync();
	// Lock to monitor vertical sync
	//   1 - wait for 1 cycle vertical refresh
	//   0 - buffer swaps are not synchronized to a video frame
	//  -1 - adaptive vsync
	bool SetVerticalSync(bool bSync = true);
	// Get SDK version number string e.g. "2.007.000"
	// Optional - return as a single number
	std::string GetSDKversion(int* pNumber = nullptr);

	//
	// OpenGL utilities
	//

	// Create an OpenGL window and context for situations where there is none.
	//   Not used if applications already have an OpenGL context.
	//   Always call CloseOpenGL afterwards.
	bool CreateOpenGL();
	// Close OpenGL window
	bool CloseOpenGL();
	// Copy OpenGL texture with optional invert
	//   Textures can be different sizes
	bool CopyTexture(GLuint SourceID, GLuint SourceTarget,
		GLuint DestID, GLuint DestTarget,
		unsigned int width, unsigned int height,
		bool bInvert = false, GLuint HostFBO = 0);
	// Copy OpenGL texture data to a pixel buffer
	bool ReadTextureData(GLuint SourceID, GLuint SourceTarget,
		void* data, unsigned int width, unsigned int height, unsigned int rowpitch,
		GLenum dataformat, GLenum datatype, bool bInvert = false, GLuint HostFBO = false);

	//
	// Formats
	//

	// Get sender DX11 shared texture format
	DXGI_FORMAT GetDX11format();
	// Set sender DX11 shared texture format
	void SetDX11format(DXGI_FORMAT textureformat);
	// Return OpenGL compatible DX11 format
	DXGI_FORMAT DX11format(GLint glformat);
	// Return DX11 compatible OpenGL format
	GLint GLDXformat(DXGI_FORMAT textureformat = DXGI_FORMAT_UNKNOWN);
	// Return OpenGL texture internal format
	GLint GLformat(GLuint TextureID, GLuint TextureTarget);
	// Return OpenGL texture format description
	std::string GLformatName(GLint glformat = 0);

	//
	// 2.006 compatibility
	//

	// Create receiver connection
	bool CreateReceiver(char* Sendername, unsigned int &width, unsigned int &height);
	// Check receiver connection
	bool CheckReceiver(char* Sendername, unsigned int &width, unsigned int &height, bool &bConnected);
	// Receive OpenGL texture
	bool ReceiveTexture(char* Sendername, unsigned int &width, unsigned int &height, GLuint TextureID = 0, GLuint TextureTarget = 0, bool bInvert = false, GLuint HostFBO = 0);
	// Receive image pixels
	bool ReceiveImage(char* Sendername, unsigned int &width, unsigned int &height, unsigned char* pixels, GLenum glFormat = GL_RGBA, bool bInvert = false, GLuint HostFBO = 0);
	// Open dialog for the user to select a sender
	//   Optional message argument
	bool SelectSenderPanel(const char* message = nullptr);
	// Receiver detect sender selection
	bool CheckSenderPanel(char* sendername, int maxchars = 256);


	// Legacy OpenGL Draw function
	// See _SpoutCommon.h_ #define legacyOpenGL
#ifdef legacyOpenGL
	// Render the shared texture
	bool DrawSharedTexture(float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = true, GLuint HostFBO = 0);
#endif

	// For access to all functions
	Spout spout;

};

#endif
