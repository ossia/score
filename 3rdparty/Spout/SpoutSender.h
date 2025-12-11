/*

					SpoutSender.h

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

#ifndef __SpoutSender__
#define __SpoutSender__

#include "Spout.h"

class SPOUT_DLLEXP SpoutSender {

	public:

	SpoutSender();
    ~SpoutSender();

	// Set name for sender creation
	//   If no name is specified, the executable name is used.  
	void SetSenderName(const char* sendername = nullptr);
	// Set the sender DX11 shared texture format
	void SetSenderFormat(DWORD dwFormat);
	// Close sender and free resources
	//   A sender is created or updated by all sending functions
	void ReleaseSender();

	// Send OpenGL framebuffer
	//   The fbo must be bound for read.
	//   The sending texture can be larger than the size that the sender is set up for
	//   For example, if the application is using only a portion of the allocated texture space,  
	//   such as for Freeframe plugins. (The 2.006 equivalent is DrawToSharedTexture)
	//   To send the default OpenGL framebuffer, specify FboID = 0.
	//   If width and height are also 0, the function determines the viewport size.
	bool SendFbo(GLuint FboID, unsigned int width, unsigned int height, bool bInvert = true);
	// Send OpenGL texture
	bool SendTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert = true, GLuint HostFBO = 0);
	// Send image pixels
	bool SendImage(const unsigned char* pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert = false, GLuint HostFBO = 0);
	// Sender status
	bool IsInitialized();
	// Sender name
	const char* GetName();
	// Sender width
	unsigned int GetWidth();
	// Sender height
	unsigned int GetHeight();
	// Sender frame rate
	double GetFps();
	// Sender frame number
	long GetFrame();
	// Sender share handle
	HANDLE GetHandle();
	// Sender sharing method
	bool GetCPU();
	// Sender GL/DX hardware compatibility
	bool GetGLDX();

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

	// Write data
	bool WriteMemoryBuffer(const char* name, const char* data, int length);
	// Create a shared memory buffer
	bool CreateMemoryBuffer(const char* name, int length);
	// Delete a shared memory buffer
	bool DeleteMemoryBuffer();
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

	// Get user Auto GPU/CPU share
	bool GetAutoShare();
	// Set application Auto GPU/CPU share
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
	// User settings recorded in the registry by "SpoutSettings"
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
	int  GetVerticalSync();
	// Lock to monitor vertical sync
	bool SetVerticalSync(int interval = 1);
	// Get Spout version
	int GetSpoutVersion();

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
	//   Textures must be the same size
	bool CopyTexture(GLuint SourceID, GLuint SourceTarget,
		GLuint DestID, GLuint DestTarget,
		unsigned int width, unsigned int height,
		bool bInvert = false, GLuint HostFBO = 0);
	// Copy OpenGL texture data to a pixel buffer
	bool ReadTextureData(GLuint SourceID, GLuint SourceTarget,
		void* data, unsigned int width, unsigned int height, unsigned int rowpitch,
		GLenum dataformat, GLenum datatype, bool bInvert = false, GLuint HostFBO = 0);

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

	// Create a sender
	bool CreateSender(const char* Sendername, unsigned int width = 0, unsigned int height = 0, DWORD dwFormat = 0);
	// Update a sender
	bool UpdateSender(const char* Sendername, unsigned int width, unsigned int height);

	// Legacy OpenGL DrawTo function
	// See _SpoutCommon.h_ #define legacyOpenGL
#ifdef legacyOpenGL
	// Render a texture to the shared texture. 
	bool DrawToSharedTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = false, GLuint HostFBO = 0);
#endif

	// For access to all functions
	Spout spout;

protected :


};

#endif
