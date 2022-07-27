//
//		SpoutReceiver
//
// ====================================================================================
//		Revisions :
//
//		27-07-14	- CreateReceiver - bUseActive flag instead of null name
//		03.09.14	- Cleanup
//		23.09.14	- return DirectX 11 capability in SetDX9
//		28.09.14	- Added Host FBO for ReceiveTexture
//		12.10.14	- changed SelectSenderPanel arg to const char
//		23.12.14	- added host fbo arg to ReceiveImage
//		08.02.15	- Changed default texture format for ReceiveImage in header to GL_RGBA
//		29.05.15	- Included SetAdapter for multiple adapters - Franz Hildgen.
//		02.06.15	- Added GetAdapter, GetNumAdapters, GetAdapterName
//		24.08.15	- Added GetHostPath to retrieve the path of the host that produced the sender
//		15.09.15	- Removed SetMemoryShareMode for 2.005 - now done globally by SpoutDirectX.exe
//		10.10.15	- Added transition flag to set invert true for 2.004 rather than default false for 2.005
//					- currently not used - see SpoutSDK.cpp CreateSender
//		14.11.15	- changed functions to "const char *" where required
//		18.11.15	- added CheckReceiver so that DrawSharedTexture can be used by a receiver
//		18.06.16	- Add invert to ReceiveImage
//		17.09.16	- removed CheckSpout2004() from constructor
//		13.01.17	- Add SetCPUmode, GetCPUmode, SetBufferMode, GetBufferMode
//					- Add HostFBO arg to DrawSharedTexture
//		15.01.17	- Add GetShareMode, SetShareMode
//		06.06.17	- Add OpenSpout
//		05.11.18	- Add IsSpoutInitialized
//		11.11.18	- Add 2.007 high level application functions
//		13.11.18	- Remove SetCPUmode, GetCPUmode
//		24.11.18	- Remove redundant GetImageSize
//		28.11.18	- Add IsFrameNew
//		11.12.18	- Add utility functions
//		05.01.19	- Make names for 2.007 functions compatible with SpoutLibrary
//		16.01.19	- Initialize class variables
//		16.03.19	- Add IsFrameCountEnabled
//		19.03.19	- Change IsInitialized to IsConnected
//		05.04.19	- Change GetSenderName(index, ..) to GetSender
//					  Create const char * GetSenderName for receiver class
//		18.09.19	- Remove UseDX9 from GetDX9 to avoid registry change
//					- Remove reset of m_SenderNameSetup from SetupReceiver
//					- Add connected test to IsUpdated
//					- Remove redundant CloseReceiver
//		28.11.19	- Remove SetupReceiver
//					  Add invert option to ReceiveTextureData and ReceiveImageData
//		13.01.20	- Add null texture option for ReceiveTextureData
//					  Add ReceiveTextureData option with no args and GetSenderTextureID()
//					  Updated receiver example
//		18.01.20	- Add CopyTexture. Update receiver example
//		20.01.20	- Changed GetSenderTextureID() to GetSharedTextureID
//		25.01.20	- Remove GetDX9compatible and SetDX9compatible
//		25.01.20	- Change ReceiveTextureData and ReceiveImageData to overloads
//		26.04.20	- Reset the update flag in IsUpdated
//		30.04.20	- Add ReceiveTexture()
//		17.06.20	- Add GetSenderFormat()
//		17.09.20	- Change GetMemoryShare(const char* sendername) to
//					  GetSenderMemoryShare(const char* sendername) for compatibility with SpoutLibrary
//					  Add GetSenderAdapter
//		25.09.20	- Remove GetSenderAdapter - not reliable 
//		17.10.20	- Change SetDX9format from D3D_FORMAT to DWORD
//		27.12.20	- Multiple changes for SpoutGL base class - see SpoutSDK.cpp
//		05.02.21	- Add GetCPUshare and SetCPUshare
//		26.02.21	- Add GetSenderGLDXready
//		11.03.21	- Rename functions GetSenderCPU and GetSenderGLDX
//		02.04.21	- Add event functions SetFrameSync/WaitFrameSync
//					- Add data function ReadMemoryBuffer
//		24.04.21	- Add OpenGL shared texture access functions
//		03.06.21	- Add GetMemoryBufferSize
//		15.10.21	- Allow no argument for SetReceiverName
//
// ====================================================================================
//
//	Copyright (c) 2014-2022, Lynn Jarvis. All rights reserved.
//
//	Redistribution and use in source and binary forms, with or without modification, 
//	are permitted provided that the following conditions are met:
//
//		1. Redistributions of source code must retain the above copyright notice, 
//		   this list of conditions and the following disclaimer.
//
//		2. Redistributions in binary form must reproduce the above copyright notice, 
//		   this list of conditions and the following disclaimer in the documentation 
//		   and/or other materials provided with the distribution.
//
//	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"	AND ANY 
//	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
//	OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE	ARE DISCLAIMED. 
//	IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
//	INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
//	PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
//	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//	LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//	OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "SpoutReceiver.h"

//
// Class: SpoutReceiver
//
// Convenience wrapper class for developing receiver applications.
//
// Insulates the programmer from sender functions.
//
// --- Code
//      #include "SpoutReceiver.h"
// ---
//
// The main Spout class can be used but will expose both Sender and Receiver functions
// which cannot be used within the same object.
// A Receiver can still access lower level common functions for example :
// --- Code
//      SpoutReceiver receiver;
//      receiver.spout.GLDXready();
// ---
//   
// Refer to the Spout class for function documentation.
//

//---------------------------------------------------------
SpoutReceiver::SpoutReceiver()
{

}

//---------------------------------------------------------
SpoutReceiver::~SpoutReceiver()
{

}

//---------------------------------------------------------
void SpoutReceiver::SetReceiverName(const char * SenderName)
{
	spout.SetReceiverName(SenderName);
}


//---------------------------------------------------------
// Release receiver and resources
// ready to connect to another sender
void SpoutReceiver::ReleaseReceiver()
{
	spout.ReleaseReceiver();
}



//---------------------------------------------------------
bool SpoutReceiver::ReceiveTexture()
{
	return spout.ReceiveTexture(0, 0);
}

//---------------------------------------------------------
bool SpoutReceiver::ReceiveTexture(GLuint TextureID, GLuint TextureTarget, bool bInvert, GLuint HostFbo)
{
	return spout.ReceiveTexture(TextureID, TextureTarget, bInvert, HostFbo);
}

//---------------------------------------------------------
bool SpoutReceiver::ReceiveImage(char* Sendername, unsigned int &width, unsigned int &height,
	unsigned char* pixels, GLenum glFormat, bool bInvert, GLuint HostFBO)
{
	return spout.ReceiveImage(Sendername, width, height, pixels, glFormat, bInvert, HostFBO);
}

//---------------------------------------------------------
bool SpoutReceiver::IsUpdated()
{
	return spout.IsUpdated();
}

//---------------------------------------------------------
bool SpoutReceiver::IsConnected()
{
	return spout.IsConnected();
}

//---------------------------------------------------------
bool SpoutReceiver::IsFrameNew()
{
	return spout.IsFrameNew();
}

//---------------------------------------------------------
DWORD SpoutReceiver::GetSenderFormat()
{
	return spout.GetSenderFormat();
}

//---------------------------------------------------------
const char * SpoutReceiver::GetSenderName()
{
	return spout.GetSenderName();
}

//---------------------------------------------------------
unsigned int SpoutReceiver::GetSenderWidth()
{
	return spout.GetSenderWidth();
}

//---------------------------------------------------------
unsigned int SpoutReceiver::GetSenderHeight()
{
	return spout.GetSenderHeight();
}

//---------------------------------------------------------
double SpoutReceiver::GetSenderFps()
{
	return spout.GetSenderFps();
}

//---------------------------------------------------------
long SpoutReceiver::GetSenderFrame()
{
	return spout.GetSenderFrame();
}

//---------------------------------------------------------
HANDLE SpoutReceiver::GetSenderHandle()
{
	return spout.GetSenderHandle();
}

//---------------------------------------------------------
bool SpoutReceiver::GetSenderCPU()
{
	return spout.GetSenderCPU();
}

//---------------------------------------------------------
bool SpoutReceiver::GetSenderGLDX()
{
	return spout.GetSenderGLDX();
}

//---------------------------------------------------------
void SpoutReceiver::SelectSender()
{
	spout.SelectSenderPanel();
}

//
// Frame count
//

//---------------------------------------------------------
void SpoutReceiver::SetFrameCount(bool bEnable)
{
	return spout.SetFrameCount(bEnable);
}

//---------------------------------------------------------
void SpoutReceiver::DisableFrameCount()
{
	spout.DisableFrameCount();
}

//---------------------------------------------------------
bool SpoutReceiver::IsFrameCountEnabled()
{
	return spout.IsFrameCountEnabled();
}

//---------------------------------------------------------
void SpoutReceiver::HoldFps(int fps)
{
	spout.HoldFps(fps);
}

//---------------------------------------------------------
void SpoutReceiver::SetFrameSync(const char* SenderName)
{
	spout.SetFrameSync(SenderName);
}

//---------------------------------------------------------
bool SpoutReceiver::WaitFrameSync(const char *SenderName, DWORD dwTimeout)
{
	return spout.WaitFrameSync(SenderName, dwTimeout);
}

//---------------------------------------------------------
int SpoutReceiver::ReadMemoryBuffer(const char* name, char* data, int maxlength)
{
	return spout.ReadMemoryBuffer(name, data, maxlength);
}

//---------------------------------------------------------
int SpoutReceiver::GetMemoryBufferSize(const char* name)
{
	return spout.GetMemoryBufferSize(name);
}


//
// OpenGL shared texture access
//

//---------------------------------------------------------
bool SpoutReceiver::BindSharedTexture()
{
	return spout.BindSharedTexture();
}

//---------------------------------------------------------
bool SpoutReceiver::UnBindSharedTexture()
{
	return spout.UnBindSharedTexture();
}

//---------------------------------------------------------
GLuint SpoutReceiver::GetSharedTextureID()
{
	return spout.GetSharedTextureID();
}

//
// Graphics compatibility
//

//---------------------------------------------------------
bool SpoutReceiver::GetAutoShare()
{
	return spout.GetAutoShare();
}

//---------------------------------------------------------
void SpoutReceiver::SetAutoShare(bool bAuto)
{
	spout.SetAutoShare(bAuto);
}

//---------------------------------------------------------
bool SpoutReceiver::GetCPUshare()
{
	return spout.GetCPUshare();
}

//---------------------------------------------------------
void SpoutReceiver::SetCPUshare(bool bCPU)
{
	spout.SetCPUshare(bCPU);
}

//---------------------------------------------------------
bool SpoutReceiver::IsGLDXready()
{
	return spout.IsGLDXready();
}

//
// Sender names
//

//---------------------------------------------------------
int SpoutReceiver::GetSenderCount()
{
	return spout.GetSenderCount();
}

//---------------------------------------------------------
// Get a sender name given an index into the sender names set
bool SpoutReceiver::GetSender(int index, char* sendername, int sendernameMaxSize)
{
	return spout.GetSender(index, sendername, sendernameMaxSize);
}

//---------------------------------------------------------
bool SpoutReceiver::GetSenderInfo(const char* sendername, unsigned int &width, unsigned int &height, HANDLE &dxShareHandle, DWORD &dwFormat)
{
	return spout.GetSenderInfo(sendername, width, height, dxShareHandle, dwFormat);
}

//---------------------------------------------------------
bool SpoutReceiver::GetActiveSender(char* Sendername)
{
	return spout.GetActiveSender(Sendername);
}

//---------------------------------------------------------
bool SpoutReceiver::SetActiveSender(const char* Sendername)
{
	return spout.SetActiveSender(Sendername);
}

//
// Adapter functions
//

//---------------------------------------------------------
int SpoutReceiver::GetNumAdapters()
{
	return spout.GetNumAdapters();
}

//---------------------------------------------------------
bool SpoutReceiver::GetAdapterName(int index, char *adaptername, int maxchars)
{
	return spout.GetAdapterName(index, adaptername, maxchars);
}

//---------------------------------------------------------
char * SpoutReceiver::AdapterName()
{
	return spout.AdapterName();
}

//---------------------------------------------------------
int SpoutReceiver::GetAdapter()
{
	return spout.GetAdapter();
}

//---------------------------------------------------------
bool SpoutReceiver::SetAdapter(int index)
{
	return spout.SetAdapter(index);
}

//---------------------------------------------------------
bool SpoutReceiver::GetAdapterInfo(char *renderdescription, char *displaydescription, int maxchars)
{
	return spout.GetAdapterInfo(renderdescription, displaydescription, maxchars);
}

//
// User settings recorded by "SpoutSettings"
//

//---------------------------------------------------------
bool SpoutReceiver::GetBufferMode()
{
	return spout.GetBufferMode();
}

//---------------------------------------------------------
void SpoutReceiver::SetBufferMode(bool bActive)
{
	spout.SetBufferMode(bActive);
}

//---------------------------------------------------------
int SpoutReceiver::GetBuffers()
{
	return spout.GetBuffers();
}

//---------------------------------------------------------
void SpoutReceiver::SetBuffers(int nBuffers)
{
	spout.SetBuffers(nBuffers);
}

//---------------------------------------------------------
int SpoutReceiver::GetMaxSenders()
{
	return spout.GetMaxSenders();
}

//---------------------------------------------------------
void SpoutReceiver::SetMaxSenders(int maxSenders)
{
	spout.SetMaxSenders(maxSenders);
}

//
// For 2.006 compatibility
//

bool SpoutReceiver::GetDX9()
{
	return spout.GetDX9();
}

bool SpoutReceiver::SetDX9(bool bDX9)
{
	return spout.SetDX9(bDX9);
}

bool SpoutReceiver::GetMemoryShareMode()
{
	return spout.GetMemoryShareMode();
}

bool SpoutReceiver::SetMemoryShareMode(bool bMem)
{
	return spout.SetMemoryShareMode(bMem);
}


bool SpoutReceiver::GetCPUmode()
{
	return spout.GetCPUmode();
}

bool SpoutReceiver::SetCPUmode(bool bCPU)
{
	return spout.SetCPUmode(bCPU);
}

int SpoutReceiver::GetShareMode()
{
	return spout.GetShareMode();
}

void SpoutReceiver::SetShareMode(int mode)
{
	spout.SetShareMode(mode);
}

//
// Information
//

//---------------------------------------------------------
bool SpoutReceiver::GetHostPath(const char *sendername, char *hostpath, int maxchars)
{
	return spout.GetHostPath(sendername, hostpath, maxchars);
}

//---------------------------------------------------------
int SpoutReceiver::GetVerticalSync()
{
	return spout.GetVerticalSync();
}

//---------------------------------------------------------
bool SpoutReceiver::SetVerticalSync(bool bSync)
{
	return spout.SetVerticalSync(bSync);
}

//---------------------------------------------------------
int SpoutReceiver::GetSpoutVersion()
{
	return spout.GetSpoutVersion();
}

//
// OpenGL utilities
//

//---------------------------------------------------------
bool SpoutReceiver::CreateOpenGL()
{
	return spout.CreateOpenGL();
}

//---------------------------------------------------------
bool SpoutReceiver::CloseOpenGL()
{
	return spout.CloseOpenGL();
}

//---------------------------------------------------------
bool SpoutReceiver::CopyTexture(GLuint SourceID, GLuint SourceTarget,
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
bool SpoutReceiver::CreateReceiver(char* sendername, unsigned int &width, unsigned int &height, bool bUseActive)
{
	return spout.CreateReceiver(sendername, width, height, bUseActive);
}

//---------------------------------------------------------
bool SpoutReceiver::CheckReceiver(char* name, unsigned int &width, unsigned int &height, bool &bConnected)
{
	return spout.CheckReceiver(name, width, height, bConnected);
}

//---------------------------------------------------------
bool SpoutReceiver::ReceiveTexture(char* name, unsigned int &width, unsigned int &height,
	GLuint TextureID, GLuint TextureTarget, bool bInvert, GLuint HostFBO)
{
	return spout.ReceiveTexture(name, width, height, TextureID, TextureTarget, bInvert, HostFBO);
}

//---------------------------------------------------------
bool SpoutReceiver::ReceiveImage(unsigned char *pixels, GLenum glFormat, bool bInvert, GLuint HostFbo)
{
	return spout.ReceiveImage(pixels, glFormat, bInvert, HostFbo);
}

//---------------------------------------------------------
bool SpoutReceiver::SelectSenderPanel(const char *message)
{
	return spout.SelectSenderPanel(message);
}

//---------------------------------------------------------
bool SpoutReceiver::CheckSenderPanel(char *sendername, int maxchars)
{
	return spout.CheckSpoutPanel(sendername, maxchars);
}


// Legacy OpenGL Draw function
#ifdef legacyOpenGL

//---------------------------------------------------------
bool SpoutReceiver::DrawSharedTexture(float max_x, float max_y, float aspect, bool bInvert, GLuint HostFBO)
{
	return spout.DrawSharedTexture(max_x, max_y, aspect, bInvert, HostFBO);
}
#endif
