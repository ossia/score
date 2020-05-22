/*

			SpoutReceiver.h

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

#ifndef __SpoutReceiver__
#define __SpoutReceiver__

#include "SpoutSDK.h"

class SPOUT_DLLEXP SpoutReceiver {

	public:

	SpoutReceiver();
    ~SpoutReceiver();

	bool CreateReceiver(char* Sendername, unsigned int &width, unsigned int &height, bool bUseActive = false);
	bool ReceiveTexture(char* Sendername, unsigned int &width, unsigned int &height, GLuint TextureID = 0, GLuint TextureTarget = 0, bool bInvert = false, GLuint HostFBO = 0);
	bool ReceiveImage(char* Sendername, unsigned int &width, unsigned int &height, unsigned char* pixels, GLenum glFormat = GL_RGBA, bool bInvert = false, GLuint HostFBO=0);
	bool CheckReceiver (char* Sendername, unsigned int &width, unsigned int &height, bool &bConnected);
	bool GetImageSize  (char* Sendername, unsigned int &width, unsigned int &height, bool &bMemoryMode);
	void ReleaseReceiver(); 

	bool BindSharedTexture();
	bool UnBindSharedTexture();
	bool DrawSharedTexture(float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = true, GLuint HostFBO=0);
	
	int  GetSenderCount();
	bool GetSenderName(int index, char* Sendername, int MaxSize = 256);
	bool GetSenderInfo(const char* Sendername, unsigned int &width, unsigned int &height, HANDLE &dxShareHandle, DWORD &dwFormat);

	bool GetActiveSender(char* Sendername);
	bool SetActiveSender(const char* Sendername);
		
	bool SelectSenderPanel(const char* message = NULL);

	bool SetDX9(bool bDX9 = true); // set to use DirectX 9 (default is DirectX 11)
	bool GetDX9();
	bool SetMemoryShareMode(bool bMem = true);
	bool GetMemoryShareMode();
	bool SetCPUmode(bool bCPU = true);
	bool GetCPUmode();
	int  GetShareMode();
	bool SetShareMode(int mode);
	void SetBufferMode(bool bActive); // Set the pbo availability on or off
	bool GetBufferMode();

	void SetDX9compatible(bool bCompatible = true);
	bool GetDX9compatible();

	int  GetNumAdapters(); // Get the number of graphics adapters in the system
	bool GetAdapterName(int index, char *adaptername, int maxchars); // Get an adapter name
	bool SetAdapter(int index = 0); // Set required graphics adapter for output
	int  GetAdapter(); // Get the current adapter index

	bool GetHostPath(const char *sendername, char *hostpath, int maxchars); // The path of the host that produced the sender

	bool SetVerticalSync(bool bSync = true);
	int  GetVerticalSync();

	Spout spout; // for access to all functions

protected :

};

#endif
