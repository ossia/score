//
//		SpoutSender
//
//		Wrapper class so that a sender object can be created independent of a receiver
//
// ====================================================================================
//		Revisions :
//
//		23.09.14	- return DirectX 11 capability in SetDX9
//		28.09.14	- Added GL format for SendImage
//					- Added bAlignment (4 byte alignment) flag for SendImage
//					- Added Host FBO for SendTexture, DrawToSharedTexture
//		08.02.15	- Changed default texture format for SendImage in header to GL_RGBA
//		29.05.15	- Included SetAdapter for multiple adapters - Franz Hildgen.
//		02.06.15	- Added GetAdapter, GetNumAdapters, GetAdapterName
//		08.06.15	- Added SelectSenderPanel for user adapter output selection
//		24.08.15	- Added GetHostPath to retrieve the path of the host that produced the sender
//		25.09.15	- Changed SetMemoryShareMode for 2.005 - now will only set true for 2.005 and above
//		09.10.15	- DrawToSharedTexture - invert default false instead of true
//		10.10.15	- Added transition flag to set invert true for 2.004 rather than default false for 2.005
//					- currently not used - see SpoutSDK.cpp CreateSender
//		14.11.15	- changed functions to "const char *" where required
//		17.03.16	- changed to const unsigned char for Sendimage buffer
//		17.09.16	- removed CheckSpout2004() from constructor
//		13.01.17	- Add SetCPUmode, GetCPUmode, SetBufferMode, GetBufferMode
//		15.01.17	- Add GetShareMode, SetShareMode
//
// ====================================================================================
/*

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
#include "SpoutSender.h"

SpoutSender::SpoutSender()
{

}


//---------------------------------------------------------
SpoutSender::~SpoutSender()
{

}


//---------------------------------------------------------
bool SpoutSender::CreateSender(const char *name, unsigned int width, unsigned int height, DWORD dwFormat)
{
	return spout.CreateSender(name, width, height, dwFormat);
}


//---------------------------------------------------------
bool SpoutSender::UpdateSender(const char *name, unsigned int width, unsigned int height)
{
	return spout.UpdateSender(name, width, height);
}


//---------------------------------------------------------
void SpoutSender::ReleaseSender(DWORD dwMsec)
{
	spout.ReleaseSender(dwMsec);
}


//---------------------------------------------------------
bool SpoutSender::SendImage(const unsigned char* pixels, unsigned int width, unsigned int height, GLenum glFormat, bool bInvert, GLuint HostFBO)
{
	return spout.SendImage(pixels, width, height, glFormat, bInvert, HostFBO);
}


//---------------------------------------------------------
bool SpoutSender::SendTexture(GLuint TextureID, GLuint TextureTarget,  unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	return spout.SendTexture(TextureID, TextureTarget, width, height, bInvert, HostFBO);
}


//---------------------------------------------------------
bool SpoutSender::DrawToSharedTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, float max_x, float max_y, float aspect, bool bInvert, GLuint HostFBO)
{
	return spout.DrawToSharedTexture(TextureID, TextureTarget, width, height, max_x, max_y, aspect, bInvert, HostFBO);
}


//---------------------------------------------------------
bool SpoutSender::SelectSenderPanel(const char* message)
{
	return spout.SelectSenderPanel(message);
}


//---------------------------------------------------------
bool SpoutSender::SetMemoryShareMode(bool bMem)
{
	return spout.SetMemoryShareMode(bMem);
}


//---------------------------------------------------------
bool SpoutSender::GetMemoryShareMode()
{
	return spout.GetMemoryShareMode();
}

//---------------------------------------------------------
bool SpoutSender::SetCPUmode(bool bCPU)
{
	return (spout.SetCPUmode(bCPU));
}

//---------------------------------------------------------
bool SpoutSender::GetCPUmode()
{
	return (spout.GetCPUmode());
}


//---------------------------------------------------------
int SpoutSender::GetShareMode()
{
	return (spout.GetShareMode());
}

//---------------------------------------------------------
bool SpoutSender::SetShareMode(int mode)
{
	return (spout.SetShareMode(mode));
}

//---------------------------------------------------------
void SpoutSender::SetBufferMode(bool bActive)
{
	spout.SetBufferMode(bActive);
}

//---------------------------------------------------------
bool SpoutSender::GetBufferMode()
{
	return spout.GetBufferMode();
}

//---------------------------------------------------------
bool SpoutSender::SetDX9(bool bDX9)
{
	return spout.SetDX9(bDX9);
}


//---------------------------------------------------------
bool SpoutSender::GetDX9()
{
	return spout.interop.isDX9();
}


//---------------------------------------------------------
void SpoutSender::SetDX9compatible(bool bCompatible)
{
	if(bCompatible) {
		// DX11 -> DX9 only works if the DX11 format is set to DXGI_FORMAT_B8G8R8A8_UNORM
		spout.interop.SetDX11format(DXGI_FORMAT_B8G8R8A8_UNORM);
	}
	else {
		// DX11 -> DX11 only
		spout.interop.SetDX11format(DXGI_FORMAT_R8G8B8A8_UNORM);
	}
}


//---------------------------------------------------------
bool SpoutSender::GetDX9compatible()
{
	if(spout.interop.DX11format == DXGI_FORMAT_B8G8R8A8_UNORM)
		return true;
	else
		return false;
	
}


//---------------------------------------------------------
bool SpoutSender::SetAdapter(int index)
{
	return spout.SetAdapter(index);
}


//---------------------------------------------------------
// Get current adapter index
int SpoutSender::GetAdapter()
{
	return spout.GetAdapter();
}


//---------------------------------------------------------
// Get the number of graphics adapters in the system
int SpoutSender::GetNumAdapters()
{
	return spout.GetNumAdapters();
}


//---------------------------------------------------------
// Get an adapter name
bool SpoutSender::GetAdapterName(int index, char *adaptername, int maxchars)
{
	return spout.GetAdapterName(index, adaptername, maxchars);
}


//---------------------------------------------------------
// Get the path of the host that created the sender
bool SpoutSender::GetHostPath(const char *sendername, char *hostpath, int maxchars)
{
	return spout.GetHostPath(sendername, hostpath, maxchars);
}


//---------------------------------------------------------
bool SpoutSender::SetVerticalSync(bool bSync)
{
	return spout.interop.SetVerticalSync(bSync);
}


//---------------------------------------------------------
int SpoutSender::GetVerticalSync()
{
	return spout.interop.GetVerticalSync();
}


//------------------ debugging aid only --------------------
bool SpoutSender::SenderDebug(char *Sendername, int size)
{
	return spout.interop.senders.SenderDebug(Sendername, size);

}
