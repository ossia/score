/*

					Spout.h

	Documentation - https://spoutgl-site.netlify.app/					

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

#ifndef __Spout__
#define __Spout__

#include "SpoutGL.h"

class SPOUT_DLLEXP Spout : public spoutGL {

	public:

	Spout();
	~Spout();

	//
	// ===================== SENDER =========================
	//

	// Set name for sender creation
	//   If no name is specified, the executable name is used  
	void SetSenderName(const char* sendername = nullptr);
	// Set sender DX11 shared texture format
	void SetSenderFormat(DWORD dwFormat);
	// Release sender and resources
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
	const char * GetName();
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
	// ====================== RECEIVER ===========================
	//

	// Specify sender for connection
	//   If a name is specified, the receiver will not connect to any other unless the user selects one
	//   If that sender closes, the receiver will wait for the nominated sender to open 
	//   If no name is specified, the receiver will connect to the active sender
	void SetReceiverName(const char * sendername = nullptr);
	// Get sender for connection
	bool GetReceiverName(char* sendername, int maxchars = 256);
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
	//   The texture must be RGBA of dimension (width * height) 
	bool ReceiveTexture(GLuint TextureID, GLuint TextureTarget,
		bool bInvert = false, GLuint HostFbo = 0);
	// Receive image pixels
	//   Connect to a sender and inform the application to update
	//   the receiving buffer if it has changed dimensions
	//   For no change, copy the sender shared texture to the pixel buffer
	//   The receiving image can be RGBA, BGRA, RGB or BGR formats of dimension (width * height) 
	bool ReceiveImage(unsigned char* pixels, GLenum glFormat = GL_RGBA, bool bInvert = false, GLuint HostFbo = 0);
	// Query whether the sender has changed
	//   Checked at every cycle before receiving data
	bool IsUpdated();
	// Query sender connection
	//   If the sender closes, receiving functions return false  
	bool IsConnected();
	// Query received frame status
	//   The receiving texture or pixel buffer is only refreshed if the sender has produced a new frame  
	//   This can be queried to process texture data only for new frames
	bool IsFrameNew();
	// Received sender name
	const char * GetSenderName();
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
	// Sender index into the set of names
	int GetSenderIndex(const char* sendername);
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
	void SetFrameSync(const char* name = nullptr);
	// Wait or test for a sync event
	bool WaitFrameSync(const char *SenderName, DWORD dwTimeout = 0);
	// Enable / disable frame sync
	void EnableFrameSync(bool bSync = true);
	// Close frame sync
	void CloseFrameSync();
	// Check for frame sync option
	bool IsFrameSyncEnabled();

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
	bool GetAdapterName(int index, char *adaptername, int maxchars = 256);
	// Return current adapter name
	char * AdapterName();
	// Get current adapter index
	int GetAdapter();
	// Get sender adapter index and name for a given sender
	int GetSenderAdapter(const char* sendername, char* adaptername = nullptr, int maxchars = 256);
	// Get the description and output display name of the current adapter
	bool GetAdapterInfo(char* description, char* output, int maxchars);
	// Get the description and output display name for a given adapter
	bool GetAdapterInfo(int index, char* description, char* output, int maxchars);

	//
	// Graphics preference
	// Windows 10 Vers 1803, build 17134 or later
	//

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

	//
	// 2.006 compatibility
	//

	// Find the index of the NVIDIA adapter in a multi-adapter system
	bool FindNVIDIA(int &nAdapter);
	// Graphics adapter details
	bool GetAdapterInfo(char* renderadapter,
		char* renderdescription, char* renderversion,
		char* displaydescription, char* displayversion,
		int maxsize);

	// Create a sender
	bool CreateSender(const char *Sendername, unsigned int width = 0, unsigned int height = 0, DWORD dwFormat = 0);
	// Update a sender
	bool UpdateSender(const char* Sendername, unsigned int width, unsigned int height);

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
	bool CheckSpoutPanel(char *sendername, int maxchars = 256);

	// Legacy OpenGL Draw functions
	// See _SpoutCommon.h_ #define legacyOpenGL
#ifdef legacyOpenGL
	// Render the shared texture
	bool DrawSharedTexture(float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = true, GLuint HostFBO = 0);
	// Render a texture to the shared texture. 
	bool DrawToSharedTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = false, GLuint HostFBO = 0);
#endif // #endif legacyOpenGL

protected:

	// Sender creation and change
	bool CheckSender(unsigned int width, unsigned int height);
	// Create receiver connection
	void InitReceiver(const char * sendername, unsigned int width, unsigned int height, DWORD dwFormat);
	// Receiver find sender and retrieve information
	bool ReceiveSenderData();

	//
	// Class globals
	//

	// Graphics adapter name
	char m_AdapterName[256];
	bool m_bAdapt; // Receiver adapt to the sender adapter


};

#endif
