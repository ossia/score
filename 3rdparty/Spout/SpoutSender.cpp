//
//		SpoutSender
//
// ====================================================================================
//		Revisions :
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
//		06.06.17	- Add OpenSpout
//		10.06.17	- Add SetFrameReady
//					- Changed CreateSender from (const char* sendername)
//					  to (char* sendername) to return the sender name if changed
//		18.08.18	- Changed CreateSender, InitSender back to const char
//		23.08.18	- Added SendFboTexture
//		05.11.18	- Add IsInitialized
//		11.11.18	- Add high level application functions
//		13.11.18	- Remove CPU mode functions
//		27.11.18	- Add RemovePadding
//		01.12.18	- Add GetFps and GetFrame
//		11.12.18	- Add utility functions
//		14.12.18	- Clean up
//		16.01.19	- Initialize class variables
//		21.01.19	- Add Bind and UnBindSharedTexture
//		26.02.19	- Add IsFrameCountEnabled
//		07.05.19	- Add HoldFps
//		18.06.19	- Change sender Update to include sender name
//		26.06.19	- Changes to Update and spout.UpdateSender
//		13.09.19	- UpdateSender - update class variables for 2.007 methods
//		18.09.19	- Remove UseDX9 from GetDX9 to avoid registry change
//		18.09.19	- Remove redundant 2.007 functions SetupSender and Update
//					- Add invert argument to CreateSender
//		15.10.19	- Check zero width and height for SendData functions 
//		13.01.20	- Remove send data functions and replace with overloads of 2.006 functions
//		19.01.20	- Remove send data functions entirely to simplify
//					- Change SendFboTexture to SendFbo
//		21.01.20	- Remove auto sender update in send functions
//		24.01.20	- Add GetSharedTextureID and CopyTexture for sender as well as receiver
//					- Removed SelectSenderPanel
//		25.01.20	- Remove GetDX9compatible and SetDX9compatible
//		28.04.20	- Add GetName() - get sender name
//		19.06.20	- Remove delay argument from ReleaseSender
//				    - Remove SenderDebug function - retain in SpoutSenderNames
//		06.07.20	- Add SetSenderName and private CheckSender
//		14.07.20	- CheckSender add zero dimension check
//		04.08.20	- Document header file functions 
//		17.09.20	- Change GetMemoryShare(const char* sendername) to
//					  GetSenderMemoryShare(const char* sendername) for compatibility with SpoutLibrary
//		17.10.20	- Change SetDX9format from D3D_FORMAT to DWORD
//		27.12.20	- Multiple changes for SpoutGL base class - see SpoutSDK.cpp
//		24.01.21	- Rebuild with SDK restructure.
//		05.02.21	- Add GetCPUshare and SetCPUshare
//		02.04.21	- Add event functions SetFrameSync/WaitFrameSync
//					- Add data function WriteMemoryBuffer
//		10.04.21	- Add GetCPU and GetGLDX
//		24.04.21	- Add OpenGL shared texture access functions
//		03.06.21	- Add CreateMemoryBuffer, DeleteMemoryBuffer, GetMemoryBufferSize
//		22.11.21	- Remove ReleaseSender() from destructor
//
// ====================================================================================
/*

	Copyright (c) 2014-2022, Lynn Jarvis. All rights reserved.

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

//
// Class: SpoutSender
//
// Convenience wrapper class for developing sender applications.
//
// Insulates the programmer from receiver functions.
//
// --- Code
//      #include "SpoutSender.h"
// ---
//
// The main Spout class can be used but will expose both Sender and Receiver functions
// which cannot be used within the same object.
//
// A Sender can still access lower level common functions for example :
// --- Code
//      SpoutSender sender;
//      sender.spout.GLDXready();
// ---
//   
// Refer to the Spout class for function documentation.
//

//---------------------------------------------------------
SpoutSender::SpoutSender()
{

}

//---------------------------------------------------------
SpoutSender::~SpoutSender()
{

}

//---------------------------------------------------------
void SpoutSender::SetSenderName(const char* sendername)
{
	spout.SetSenderName(sendername);
}

//---------------------------------------------------------
void SpoutSender::SetSenderFormat(DWORD dwFormat)
{
	spout.SetSenderFormat(dwFormat);
}

//---------------------------------------------------------
void SpoutSender::ReleaseSender()
{
	spout.ReleaseSender();
}

//---------------------------------------------------------
bool SpoutSender::SendFbo(GLuint FboID, unsigned int width, unsigned int height, bool bInvert)
{
	return spout.SendFbo(FboID, width, height, bInvert);
}

//---------------------------------------------------------
bool SpoutSender::SendTexture(GLuint TextureID, GLuint TextureTarget,
	unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	return spout.SendTexture(TextureID, TextureTarget, width, height, bInvert, HostFBO);
}

//---------------------------------------------------------
bool SpoutSender::SendImage(const unsigned char* pixels, unsigned int width, unsigned int height, GLenum glFormat, bool bInvert, GLuint HostFBO)
{
	return spout.SendImage(pixels, width, height, glFormat, bInvert, HostFBO);
}

//---------------------------------------------------------
bool SpoutSender::IsInitialized()
{
	return spout.IsInitialized();
}

//---------------------------------------------------------
const char * SpoutSender::GetName()
{
	return spout.GetName();
}

//---------------------------------------------------------
unsigned int SpoutSender::GetWidth()
{
	return spout.GetWidth();
}

//---------------------------------------------------------
unsigned int SpoutSender::GetHeight()
{
	return spout.GetHeight();
}

//---------------------------------------------------------
double SpoutSender::GetFps()
{
	return spout.GetFps();
}

//---------------------------------------------------------
long SpoutSender::GetFrame()
{
	return spout.GetFrame();
}

//---------------------------------------------------------
HANDLE SpoutSender::GetHandle()
{
	return spout.GetHandle();
}

//---------------------------------------------------------
bool SpoutSender::GetCPU()
{
	return spout.GetCPU();
}

//---------------------------------------------------------
bool SpoutSender::GetGLDX()
{
	return spout.GetGLDX();
}

//
// Frame count
//

//---------------------------------------------------------
void SpoutSender::SetFrameCount(bool bEnable)
{
	return spout.SetFrameCount(bEnable);
}

//---------------------------------------------------------
void SpoutSender::DisableFrameCount()
{
	spout.DisableFrameCount();
}

//---------------------------------------------------------
bool SpoutSender::IsFrameCountEnabled()
{
	return spout.IsFrameCountEnabled();
}

//---------------------------------------------------------
void SpoutSender::HoldFps(int fps)
{
	spout.HoldFps(fps);
}

//---------------------------------------------------------
void SpoutSender::SetFrameSync(const char* SenderName)
{
	spout.SetFrameSync(SenderName);
}

//---------------------------------------------------------
bool SpoutSender::WaitFrameSync(const char *SenderName, DWORD dwTimeout)
{
	return spout.WaitFrameSync(SenderName, dwTimeout);
}

//
// Data sharing
//

//---------------------------------------------------------
bool SpoutSender::WriteMemoryBuffer(const char *name, const char* data, int length)
{
	return spout.WriteMemoryBuffer(name, data, length);
}

//---------------------------------------------------------
bool SpoutSender::CreateMemoryBuffer(const char *name, int length)
{
	return spout.CreateMemoryBuffer(name,length);
}

//---------------------------------------------------------
bool SpoutSender::DeleteMemoryBuffer()
{
	return spout.DeleteMemoryBuffer();
}

//---------------------------------------------------------
int SpoutSender::GetMemoryBufferSize(const char* name)
{
	return spout.GetMemoryBufferSize(name);
}


//
// OpenGL shared texture access
//

//---------------------------------------------------------
bool SpoutSender::BindSharedTexture()
{
	return spout.BindSharedTexture();
}

//---------------------------------------------------------
bool SpoutSender::UnBindSharedTexture()
{
	return spout.UnBindSharedTexture();
}

//---------------------------------------------------------
GLuint SpoutSender::GetSharedTextureID()
{
	return spout.GetSharedTextureID();
}

//
// Graphics compatibility
//

//---------------------------------------------------------
bool SpoutSender::GetAutoShare()
{
	return spout.GetAutoShare();
}

//---------------------------------------------------------
void SpoutSender::SetAutoShare(bool bAuto)
{
	spout.SetAutoShare(bAuto);
}

//---------------------------------------------------------
bool SpoutSender::GetCPUshare()
{
	return spout.GetCPUshare();
}

//---------------------------------------------------------
void SpoutSender::SetCPUshare(bool bCPU)
{
	spout.SetCPUshare(bCPU);
}

//---------------------------------------------------------
bool SpoutSender::IsGLDXready()
{
	return spout.IsGLDXready();
}

//
// Sender names
//

//---------------------------------------------------------
int SpoutSender::GetSenderCount()
{
	return spout.GetSenderCount();
}

//---------------------------------------------------------
// Get a sender name given an index into the sender names set
bool SpoutSender::GetSender(int index, char* sendername, int sendernameMaxSize)
{
	return spout.GetSender(index, sendername, sendernameMaxSize);
}

//---------------------------------------------------------
bool SpoutSender::GetSenderInfo(const char* sendername, unsigned int &width, unsigned int &height, HANDLE &dxShareHandle, DWORD &dwFormat)
{
	return spout.GetSenderInfo(sendername, width, height, dxShareHandle, dwFormat);
}

//---------------------------------------------------------
bool SpoutSender::GetActiveSender(char* Sendername)
{
	return spout.GetActiveSender(Sendername);
}

//---------------------------------------------------------
bool SpoutSender::SetActiveSender(const char* Sendername)
{
	return spout.SetActiveSender(Sendername);
}

//
// Adapter functions
//

//---------------------------------------------------------
int SpoutSender::SpoutSender::GetNumAdapters()
{
	return spout.GetNumAdapters();
}

//---------------------------------------------------------
bool SpoutSender::GetAdapterName(int index, char *adaptername, int maxchars)
{
	return spout.GetAdapterName(index, adaptername, maxchars);
}

//---------------------------------------------------------
char * SpoutSender::AdapterName()
{
	return spout.AdapterName();
}

//---------------------------------------------------------
int SpoutSender::GetAdapter()
{
	return spout.GetAdapter();
}

//---------------------------------------------------------
bool SpoutSender::SetAdapter(int index)
{
	return spout.SetAdapter(index);
}

//---------------------------------------------------------
bool SpoutSender::GetAdapterInfo(char *renderdescription, char *displaydescription, int maxchars)
{
	return spout.GetAdapterInfo(renderdescription, displaydescription, maxchars);
}

//
// User settings recorded by "SpoutSettings"
//

//---------------------------------------------------------
bool SpoutSender::GetBufferMode()
{
	return spout.GetBufferMode();
}

//---------------------------------------------------------
void SpoutSender::SetBufferMode(bool bActive)
{
	spout.SetBufferMode(bActive);
}

//---------------------------------------------------------
int SpoutSender::GetBuffers()
{
	return spout.GetBuffers();
}

//---------------------------------------------------------
void SpoutSender::SetBuffers(int nBuffers)
{
	spout.SetBuffers(nBuffers);
}

//---------------------------------------------------------
int SpoutSender::GetMaxSenders()
{
	return spout.GetMaxSenders();
}

//---------------------------------------------------------
void SpoutSender::SetMaxSenders(int maxSenders)
{
	spout.SetMaxSenders(maxSenders);
}

//
// For 2.006 compatibility
//

bool SpoutSender::GetDX9()
{
	return spout.GetDX9();
}

bool SpoutSender::SetDX9(bool bDX9)
{
	return spout.SetDX9(bDX9);
}

bool SpoutSender::GetMemoryShareMode()
{
	return spout.GetMemoryShareMode();
}

bool SpoutSender::SetMemoryShareMode(bool bMem)
{
	return spout.SetMemoryShareMode(bMem);
}


bool SpoutSender::GetCPUmode()
{
	return spout.GetCPUmode();
}

bool SpoutSender::SetCPUmode(bool bCPU)
{
	return spout.SetCPUmode(bCPU);
}

int SpoutSender::GetShareMode()
{
	return spout.GetShareMode();
}

void SpoutSender::SetShareMode(int mode)
{
	spout.SetShareMode(mode);
}

//
// Information
//

//---------------------------------------------------------
bool SpoutSender::GetHostPath(const char *sendername, char *hostpath, int maxchars)
{
	return spout.GetHostPath(sendername, hostpath, maxchars);
}

//---------------------------------------------------------
int SpoutSender::GetVerticalSync()
{
	return spout.GetVerticalSync();
}

//---------------------------------------------------------
bool SpoutSender::SetVerticalSync(bool bSync)
{
	return spout.SetVerticalSync(bSync);
}

//---------------------------------------------------------
int SpoutSender::GetSpoutVersion()
{
	return spout.GetSpoutVersion();
}

//
// OpenGL utilities
//

//---------------------------------------------------------
bool SpoutSender::CreateOpenGL()
{
	return spout.CreateOpenGL();
}

//---------------------------------------------------------
bool SpoutSender::CloseOpenGL()
{
	return spout.CloseOpenGL();
}

//---------------------------------------------------------
bool SpoutSender::CopyTexture(GLuint SourceID, GLuint SourceTarget,
	GLuint DestID, GLuint DestTarget,
	unsigned int width, unsigned int height,
	bool bInvert, GLuint HostFBO)
{
	return spout.CopyTexture(SourceID, SourceTarget, DestID, DestTarget,
		width, height, bInvert, HostFBO);
}

//
// 2.006 compatibility
//

//---------------------------------------------------------
bool SpoutSender::CreateSender(const char* name, unsigned int width, unsigned int height, DWORD dwFormat)
{
	return spout.CreateSender(name, width, height, dwFormat);
}

//---------------------------------------------------------
bool SpoutSender::UpdateSender(const char* name, unsigned int width, unsigned int height)
{
	return spout.UpdateSender(name, width, height);
}

// Legacy OpenGL DrawTo function
#ifdef legacyOpenGL

//---------------------------------------------------------
bool SpoutSender::DrawToSharedTexture(GLuint TextureID, GLuint TextureTarget,
	unsigned int width, unsigned int height,
	float max_x, float max_y, float aspect,
	bool bInvert, GLuint HostFBO)
{
	return spout.DrawToSharedTexture(TextureID, TextureTarget,
		width, height, max_x, max_y, aspect, bInvert, HostFBO);

}
#endif