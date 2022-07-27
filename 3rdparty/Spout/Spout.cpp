//
//		Spout SDK
//
//		Spout.cpp
//
// Documentation <https://spoutgl-site.netlify.app>	
//
// ====================================================================================
//		Revisions :
//
//		14-07-14	- SelectSenderPanel - return true was missing.
//		16-07-14	- deleted fbo & texture in SpoutCleanup - test for OpenGL context
//					- used CopyMemory in FlipVertical instead of memcpy
//					- cleanup
//		18-07-14	- removed SpoutSDK local fbo and texture - used in the interop class now
//		22-07-14	- added option for DX9 or DX11
//		25-07-14	- Malcolm Bechard mods to header to enable compilation as a dll
//					- ReceiveTexture - release receiver if the sender no longer exists
//					- ReceiveImage same change - to be tested
//		27-07-14	- CreateReceiver - bUseActive flag instead of null name
//		31-07-14	- Corrected DrawTexture aspect argument
//		01-08-14	- TODO - work on OpenReceiver for memoryshare
//		03-08-14	- CheckSpoutPanel allow for unregistered sender
//		04-08-14	- revise CheckSpoutPanel
//		05-08-14	- default true for setverticalsync in sender and receiver classes
//		11-08-14	- fixed incorrect name arg in OpenReceiver for ReceiveTexture / ReceiveImage
//					2.004 release 19-08-14
//		24-08-14	- changed back to WM_PAINT message instead of RedrawWindow due to FFGL receiver bug appearing again
//		27-08-14	- removed texture init check from SelectSenderPanel
//		29-08-14	- changed SelectSenderPanel to use revised SpoutPanel with user message support
//		03.09.14	- cleanup
//		15.09.14	- protect against null string copy in SelectSenderPanel
//		22.09.14	- checking of bUseAspect function in CreateReceiver
//		23.09.14	- test for DirectX 11 support in SetDX9 and GetDX9
//		24.09.14	- updated project file for DLL to include SpoutShareMemory class
//		28.09.14	- Added GL format for SendImage and FlipVertical
//					- Added bAlignment  (4 byte alignment) flag for SendImage
//					- Added Host FBO for SendTexture, DrawToSharedTexture
//					- Added Host FBO for ReceiveTexture
//		11.10.14	- Corrected UpdateSender to recreate sender using CreateInterop
//					- Corrected SelectSenderpanel so that an un-initialized string is not used
//		12.10.14	- Included SpoutPanel always bring to topmost in SelectSenderPanel
//					- allowed for change of sender size in DrawToSharedTexture
//		15.10.14	- added debugging aid for texture access locks
//		29.10.14	- changes to SendImage
//		23.12.14	- added host fbo arg to ReceiveImage
//		30.01.15	- Read SpoutPanel path from registry (dependent on revised installer)
//					  Next path checked is module path, then current working directory
//		06.02.15	- added #pragma comment(lib,.. for "Shell32.lib" and "Advapi32.lib"
//		10.02.15	- added Optimus NvOptimusEnablement export to Spout.h - should apply to all apps including this SDK.
//		22.02.15	- added FindFileVersion for future use
//		24.05.15	- Registry read of sender name for CheckSpoutPanel (see SpoutPanel)
//		29.05.15	- Included SetAdapter for multiple adapters - Franz Hildgen.
//		01.06.15	- Read/Write DX9 mode from registry
//		02.06.15	- Added GetAdapter, GetNumAdapters, GetAdapterName
//		04.07.15	- corrected "const char *" arg for GetSenderInfo
//		08.07.15	- Recompile for global DX9 flag
// 		01.08.15	- OpenReceiver - safety in case no opengl context
//		22.08.15	- Change to CheckSpoutPanel to wait for SpoutPanel mutex to open and then close
//		24.08.15	- Added GetHostPath to retrieve the path of the host that produced the sender
//		01.09.15	- added MessageBox error warnings in InitSender for better user diagnostics
//					  also added MessageBox warnings in SpoutGLDXinterop::CreateInterop
//		09.09.15	- included g_ShareHandle in CheckSpoutPanel
//					- removed bGLDXcompatibleShareInitOK becasue there is no single initialization any more
//		12.09.15	- Incremented application sender name if one already exists with the same name
//					- Finalised revised SpoutMemoryShare class and functions
//		15.09.15	- Disable memoryshare if the 2.005 installer has not set the "MemoryShare" key
//					  to avoid problems with 2.004 apps.
//					- Change logic of OpenSpout so that fails for incompatible hardware
//					  if memoryshare is not set. Only 2.005 apps can set memoryshare.\
//		19.09.15	- Changed GetImageSize to look for NULL sharehandle of a sender to determine
//					  if it is memoryshare. Used by SpoutCam.
//		22.09.15	- Fixed memoryshare sender update in UpdateSender
//		25.09.15	- Changed SetMemoryShareMode for 2.005 - now will only set true for 2.005 and above
//		09.10.15	- DrawToSharedTexture - invert default false instead of true
//		10.10.15	- CreateSender - introduced a temporary DX shared texture for 2.005 memoryshare to prevent
//					  a crash with existing 2.004 apps
//		22.10.15	- Changed CheckSpoutPanel so that function variables are only created if SpoutPanel has been opened
//		26.10.15	- Added bIsSending and bIsReceiving for safety release of sender in destructor.
//		14.11.15	- changed functions to "const char *" where required
//		18.11.15	- added CheckReceiver so that DrawSharedTexture can be used by a receiver
//		24.11.15	- changes to CheckSpoutPanel to favour ActiveSender over the Registry sender name (used by VVVV)
//					- Reintroduced 250msec sleep after SpoutPanel activation
//		29.11.15	- fixed const char problem in ReadPathFromRegistry
//		18.01.16	- added CleanSenders before opening a new sender in case of orphaned sender names in the list
//		10.02.16	- added RemovePathFromRegistry
//		26.02.16	- recompile for Processing library 2.0.5.2 release
//		06.03.16	- added GetSpoutSenderName() and IsSpoutInitialized() for access to globals
//		17.03.16	- removed alignment argument from ReceiveImage
//					  Check for bgra extensions in receiveimage and sendimage
//					  Support only for rgba or bgra
//					  Changed to const unsigned char for Sendimage buffer
//		21.03.16	- Added glFormat and bInvert to SendImage
//					- Included LoadGLextensions in InitSender and InitReceiver for memoryshare mode.
//		24.03.16	- Added HostFBO argument to WriteMemory and ReadMemory function calls.
//		04.04.16	- Added HostFBO argument to SendImage - only used for texture share
//					  Merge from Smokhov https://github.com/leadedge/Spout2/pull/14
//					- Changed default invert flag for SendImage to true.
//		24.04.16	- Added IsPBOavailable to test for PBO support.
//		04.05.16	- SetPBOavailable(true/false) added to enable/disable pbo functions
//		07.05.16	- SetPBOavailable changed to SetBufferMode
//		18.06.16	- Add invert to ReceiveImage
//			2.005 release 23-06-16
//		29.06.16	- Added ReportMemory() for debugging
//					- Changed OpenSpout to fail for DX9 if no hwnd
//					  https://github.com/leadedge/Spout2/issues/18
//		03.07.16	- Fix dwFormat repeat declaration in InitSender
//		15.01.17	- Add GetShareMode, SetShareMode
//		18.01.17	- GetImageSize redundant for 2.006
//		22.01.17	- include zero char in SelectSenderPanel NULL arg checks
//		25.05.17	- corrected SendImage UpdateSender to use passed width and height
//			2.006 release 08-02-17
//
//		VS2015
//
//		02.06.17	- Registry functions moved to SpoutUtils
//		06.06.17	- Added GLDXavailable to OpenSpout
//		09.06.17	- removed g_TexID - not used
//		05.10.17	- https://github.com/leadedge/Spout2/issues/24
//					- OpenReceiver simplify code
//					- CheckSpoutPanel simplify code, remove text file sender retrieval
//					- Add InitReceiver override to include sharehandle and format args
//		10.03.18	- Noted that change to OpenReceiver for offscreen rendering
//					  not needed because hwnd can be null for spoutdx.CreateDX9device
//					  https://github.com/leadedge/Spout2/issues/18
//
//		VS2017
//
//		23.08.18	- Add SendFboTexture - see changes to WriteGLDXtexture in SpoutGLDXinterop.cpp
//		17.10.18	- Retrieve global render window handle in OpenSpout
//		01.11.18	- SendImage bInvert default false to align with SpoutSender.cpp		
//		01.11.18	- Changes to SelectSenderPanel to terminate SpoutPanel if it has crashed.
//		03.11.18	- Texture creation patch for compatibility with 2.004 removed for Spout 2.007
//		13.11.18	- Remove CPU mode
//		24.11.18	- Remove redundant GetImageSize
//		27.11.18	- Add RemovePadding for correction of image stride
//		28.11.18	- Add IsFrameNew and HasNewFrame
//		14.12.18	- Clean up for SpoutLibrary
//		15.12.18	- UpdateSender - release and re-create sender to avoid memory leak
//		17.12.18	- Change Spout dll Project properties to / MT
//		28.12.18	- Check mutex handle before close in CheckSpoutPanel
//		28.12.18	- Rebuild Spout.dll 32 / 64bit - Version 2.007
//		03.01.19	- Changed to revised registry functions in SpoutUtils
//		04.01.19	- Add OpenGL window creation functions for SpoutLibrary
//		05.01.19	- Change names for high level receiver functions for SpoutLibrary
//		16.01.19	- Fix ReceiveTextureData for sender name change
//		22.01.19	- Remove unsused bIsReceiving flag
//		05.03.19	- Add log notice for ReleaseSender
//		05.04.19	- Change GetSenderName to GetSender
//					  Reserve const char * GetSenderName for receiver class
//		17.06.19	- Fix missing log warning argument in UpdateSender
//		26.06.19	- Cleanup changes to UpdateSender
//		13.01.20	- Removed sleep time for SpoutPanel to open
//		19.01.20	- Change SendFboTexture to SendFbo
//		20.01.20	- Corrected SendFbo for width/height < shared texture
//		21.01.20	- Remove auto sender update in send functions
//					  Remove debug print from InitSender
//		25.05.20	- Correct filename case for all #includes throughout
//		14.07.20	- Removed unused bChangeRequested flag
//		18.07.20	- Rebuild binaries Win32 and x64 /MT VS2017
//		04.09.20	- Dynamic switch between memory and texture share modes
//		05.09.20	- OpenReceiver - Switch to memoryshare if receiver and sender use different GPU
//					  See SpoutGLDXinterop to set adapter index to "usage" field in sender shared memory
//		06.09.20	- Do not change share mode flags in SpoutCleanup
//		07.09.20	- Correct receiver switch from memory to texture if texture compatible
//		08.09.20	- OpenReceiver - remove warning log for receiver and sender using a different GPU
//					  InitSender - switch to memoryshare on CreateInterop failure
//		09.09.20	- SetAdapter - reset and perform compatibility test
//		15.09.20	- Remove SpoutMessageBox from OpenSpout()
//					  Failure must be handled by the application
//		17.09.20	- Change GetMemoryShare(const char* sendername) to
//					  GetSenderMemoryShare(const char* sendername) for compatibility with SpoutLibrary
//					  Add GetSenderAdapter
//		18.09.20	- Add SetSenderAdapter
//		22.09.20	- OpenReceiver sender/receiver GPU check
//		23.09.20	- Corrected SetSenderAdapter
//					  Logic corrections
//		24.09.20	- Correction of SetSenderAdapter as bool not void
//		25.09.20	- Remove GetSenderAdapter/SetSenderAdapter - not reliable
//		17.10.20	- Change SetDX9format from D3D_FORMAT to DWORD
//		27.12.20	- Multiple changes for SpoutGL base class - see SpoutSDK.cpp
//					  Remove DX9 support
//					  CPU backup enhanced using dual DirectX staging textures
//					  Auto switch to CPU backup if GL/DX incompatible
//		10.01.21	- SetSenderName - auto increment of sender name if the sender already exists
//		12.01.21	- Release orphaned senders in SelectSenderPanel
//					- CheckSender : write host path to the sender shared memory Description field
//					  in spoutSenderNames::CreateSender
//		13.01.21	- Release orphaned senders in SpoutPanel.exe instead of SelectSenderPanel
//					  Additional checks for un-registered senders
//		18.01.21	- ReceiveSenderData : Check if the name is in the sender list
//		26.02.21	- Add GetSenderGLDXready() for receiver
//		01.03.21	- Add SetSenderID
//		11.03.21	- Rename functions GetSenderCPU and GetSenderGLDX
//		13.03.21	- memoryshare.CloseSenderMemory() in ReleaseSender
//		15.03.21	- IsFrameNew - return frame.IsFrameNew()
//		20.03.21	- memoryshare.CloseSenderMemory() in ReleaseReceiver
//		02.04.21	- Add event functions SetFrameSync/WaitFrameSync
//					- Add data functions WriteMemoryBuffer/ReadMemoryBuffer
//		07.04.21	- Close sync event in ReleaseSender
//		20.04.21	- SendFbo - protect against SendFbo fail for default framebuffer if iconic
//		24.04.21	- ReceiveTexture - return if flagged for update
//					  only if there is a texture to receive into.
//		10.05.21	- ReceiveTexture - allow for the possibility of 2.006 memoryshare sender.
//		22.06.21	- Move code for GetSenderCount and GetSender to SpoutSenderNames class
//		03.07.21	- Use changed SpoutSenderNames "GetSender" function name.
//		04.07.21	- Additional code comments concerning update in ReceiveTexture.
//		12.08.21	- CreateReceiver - Revise CreateReceiver to avoid switch to active
//					  if the selected sender closes.
//		15.10.21	- Allow no argument for SetReceiverName
//		07.11.21	- Remove pbo available flag to use ReadGLDXpixels in ReceiveImage
//					  it is tested within ReadGLDXpixels
//		20.11.21	- Destructor virtual for base class
//		22.11.21	- Use SpoutDirectX ReleaseDX11Texture to release shared texture
//					- Remove adapter gets from constructor
//		17.12.21	- Remove adapter gets from Sender/Receiver init
//					  Adapter index and name are retrieved with Get functions
//		20.12.21	- Restore log notice for ReleaseSender
//		24.02.22	- Restore GetSenderAdpater for testing
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
#include "Spout.h"

// Class: Spout
//
// <https://spout.zeal.co/>
//
// Main class for Spout OpenGL texture sharing
//
// Contains both Sender and Receiver functions.
//
// This class and other source files are included in a project
//
// Files required are (.h and .cpp) :
//
// - Spout
// - SpoutCommon
// - SpoutCopy
// - SpoutDirectX
// - SpoutFramecount
// - SpoutGL
// - SpoutGLextensions
// - SpoutSenderNames
// - SpoutSharedMemory
// - SpoutUtils
//
// Note that Sender and Receiver functions cannot be used within the same object.
// The SpoutSender and SpoutReceiver classes are convenience wrappers which assist
// the programmer by exposing only sender or receiver specific functions.
//
// - SpoutSender
// - SpoutReceiver
//
// You can also use the Spout SDK as a dll. To build the dll, refer to the 
// Visual Studio project in the VS2017 folder and the CMake build documentation.
// Also refer to the SpoutLibrary folder for a C-compatible dll which can be 
// used with compilers other than Visual Studio.
//
// For conversion of existing 2.006 applications, refer to "Porting.txt" in the "Docs" section
// as well as the introductory document *SpoutSDK_2007.pdf*.
//
// More detailed information can be found in the header files for each class.
// Functions for individual classes are documented within the respective source files.
// You can access these from the following objects that are included in the Spout class.
//
// - spoutDirectX spoutdx; (DirectX 11 texture sharing)
// - spoutCopy spoutcopy; (Pixel data copy)
// - spoutSenderNames sendernames; (Spout sender management)
// - spoutFrameCount frame; (Frame counting management)
//
// Details for practical use can be found in the source code for the Openframeworks examples.
// The methods are simple and you should be able to quickly extend to your own application
// or to other frameworks.
//
// Refer to the SpoutGL base class for further documentation and details.
//
Spout::Spout()
{
	// Initialize adapter name global
	// Adapter index and name are retrieved with create sender or receiver
	m_AdapterName[0] = 0;
	m_bAdapt = false; // Receiver adapt to the sender adapter

}

Spout::~Spout()
{
	// ~spoutGL will release dependent objects
}

//
// Group: Sender
//
// SendFbo, SendTexture and SendImage create or update a sender as required.
//
// - If a sender has not been created yet :
//
//    - Make sure Spout has been initialized and OpenGL context is available
//    - Perform a compatibility check for GL/DX interop
//    - If compatible, create interop for GL/DX transfer
//    - If not compatible, create a DirectX 11 shared texture for the sender
//    - Create a sender using the DX11 shared texture handle
//
// - If the sender exists, test for size change :
//
//    - If compatible, update the shared textures and GL/DX interop
//    - If not compatible, re-create the class DirectX shared texture to the new size
//    - Update the sender and class variables	
//

//---------------------------------------------------------
// Function: SetSenderName
// Set name for sender creation
//
//     If no name is specified, the executable name is used. 
//     Thereafter, all sending functions create and update a sender
//     based on the size passed and the name that has been set
void Spout::SetSenderName(const char* sendername)
{
	if (!sendername) {
		// Get executable name as default
		GetModuleFileNameA(NULL, m_SenderName, 256);
		PathStripPathA(m_SenderName);
		PathRemoveExtensionA(m_SenderName);
	}
	else {
		strcpy_s(m_SenderName, 256, sendername);
	}

	// If a sender with this name is already registered, create an incremented name
	int i = 1;
	char name[256];
	strcpy_s(name, 256, m_SenderName);
	if (sendernames.FindSenderName(name)) {
		do {
			sprintf_s(name, 256, "%s_%d", m_SenderName, i);
			i++;
		} while (sendernames.FindSenderName(name));
	}
	// Re-set the global sender name
	strcpy_s(m_SenderName, 256, name);

}

//---------------------------------------------------------
// Function: SetSenderFormat
// Set the sender DX11 shared texture format
//    Compatible formats - see SpoutGL::SetDX11format
void Spout::SetSenderFormat(DWORD dwFormat)
{
	m_dwFormat = dwFormat;
	// Update SpoutGL class global texture format
	SetDX11format((DXGI_FORMAT)dwFormat);
}

//---------------------------------------------------------
// Function: ReleaseSender
// Close receiver and release resources.
//
// A new sender is created or updated by all sending functions
void Spout::ReleaseSender()
{
	SpoutLogNotice("Spout::ReleaseSender(%s)", m_SenderName);

	if (m_bInitialized) {
		sendernames.ReleaseSenderName(m_SenderName);
		frame.CleanupFrameCount();
		frame.CloseAccessMutex();
	}

	// Close shared memory and sync event if used
	memoryshare.Close();
	frame.CloseFrameSync();

	// Release OpenGL resources
	// OpenGL only - do not close DirectX
	CleanupGL();

}

//---------------------------------------------------------
// Function: SendFbo
// Send a framebuffer
//
//   The fbo must be bound for read. 
//
//   The fbo can be larger than the size that the sender is set up for.  
//   For example, if the application is using only a portion of the allocated texture space,  
//   such as for Freeframe plugins. (The 2.006 equivalent is DrawToSharedTexture).
//   The function can also be used with the OpenGL default framebuffer by
//   specifying "0" for the fbo ID.
//
bool Spout::SendFbo(GLuint FboID, unsigned int width, unsigned int height, bool bInvert)
{
	// For texture sharing, the size of the texture attached to the
	// fbo must be equal to or larger than the shared texture
	if (width == 0 || height == 0) {
		return false;
	}

	// Default framebuffer fails if iconic
	if (FboID == 0 && IsIconic(m_hWnd))
		return false;

	// Create or update the sender
	if (!CheckSender(width, height)) {
		return false;
	}

	// All clear to send the fbo texture
	if(m_bTextureShare) {
		// 3840-2160 - 60fps (0.45 msec per frame)
		return WriteGLDXtexture(0, 0, width, height, bInvert, FboID);
	}
	else if (m_bCPUshare) {
		// Auto share enabled for DirectX CPU backup
		// 3840-2160 - 43fps (5-7msec/frame)
		// Create a local class texture if not already
		CheckOpenGLTexture(m_TexID, GL_RGBA, width, height);
		// Copy from the texture attached to the bound fbo to the class texture
		glBindTexture(GL_TEXTURE_2D, m_TexID);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
		glBindTexture(GL_TEXTURE_2D, 0);
		// Copy from the OpenGL class texture to the shared DX11 texture by way of staging texture
		return WriteDX11texture(m_TexID, GL_TEXTURE_2D, width, height, bInvert, FboID);
	}

	return false;

}

//---------------------------------------------------------
// Function: SendTexture
// Send OpenGL texture
//
//     SendTexture creates a shared texture for all receivers to access.
//
//     The invert flag is optional and by default true. This flips the texture
//     in the Y axis, which is necessary because DirectX and OpenGL textures
//     are opposite in Y. If it is set to false no flip occurs and the result
//     may appear upside down.
//
//     The ID of a currently bound fbo should be passed in.
//
bool Spout::SendTexture(GLuint TextureID, GLuint TextureTarget,
	unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	// Quit if no data
	if (TextureID <= 0 || width == 0 || height == 0)
		return false;


	// Create or update the sender
	// < 0.001 msec 
	if (!CheckSender(width, height))
		return false;

	if (m_bTextureShare) {
		// Send OpenGL texture if GL/DX interop compatible
		// 3840-2160 - 60fps (0.45 msec per frame)
		return WriteGLDXtexture(TextureID, TextureTarget, width, height, bInvert, HostFBO);
	}
	else if (m_bCPUshare) {
		// Auto share enabled for DirectX CPU backup
		// 3840-2160 47fps (6-7 msec per frame with PBOs)
		return WriteDX11texture(TextureID, TextureTarget, width, height, bInvert, HostFBO);
	}

	return false;

}

//---------------------------------------------------------
// Function: SendImage
// Send pixel image
//
//     SendImage creates a shared texture using image pixels as the source
//     instead of an OpenGL texture. The format of the image to be sent is RGBA 
//     by default but can be a different OpenGL format, for example GL_RGB or GL_BGRA_EXT.
//
//     The invert flag is optional and false by default.
//
//     The ID of a currently bound fbo should be passed in.
//
bool Spout::SendImage(const unsigned char* pixels, unsigned int width, unsigned int height, GLenum glFormat, bool bInvert, GLuint HostFBO)
{
	// Dimensions should be the same as the sender
	if (!pixels || width == 0 || height == 0)
		return false;

	// Only RGBA, BGRA, RGB, BGR supported
	if (!(glFormat == GL_RGBA || glFormat == GL_BGRA_EXT || glFormat == GL_RGB || glFormat == GL_BGR_EXT))
		return false;

	// Check for BGRA support
	GLenum glformat = glFormat;
	if (!m_bBGRAavailable) {
		// If the bgra extensions are not available and the user
		// provided GL_BGR_EXT or GL_BGRA_EXT do not use them
		if (glFormat == GL_BGR_EXT) glformat = GL_RGB;
		if (glFormat == GL_BGRA_EXT) glformat = GL_RGBA;
	}
	
	// Create or update the sender
	if (!CheckSender(width, height))
		return false;
	//
	// Write pixel data to the rgba shared texture according to pixel format
	//
	if (m_bTextureShare) {
		// Texture share compatible
		return WriteGLDXpixels(pixels, width, height, glformat, bInvert, HostFBO);
	}
	else if (m_bCPUshare) {
		// Auto share enabled for DirectX CPU backup
		return WriteDX11pixels(pixels, width, height, glformat, bInvert);
	}

	return false;

}

//---------------------------------------------------------
// Function: IsInitialized
// Initialization status
bool Spout::IsInitialized()
{
	return m_bInitialized;
}

//---------------------------------------------------------
// Function: GetName
// Sender name
const char * Spout::GetName()
{
	return m_SenderName;
}

//---------------------------------------------------------
// Function: GetWidth
// Sender width
unsigned int Spout::GetWidth()
{
	return m_Width;
}

//---------------------------------------------------------
// Function: GetHeight
// Sender height
unsigned int Spout::GetHeight()
{
	return m_Height;
}

//---------------------------------------------------------
// Function: GetFps
// Sender frame rate
double Spout::GetFps()
{
	return frame.GetSenderFps();
}

//---------------------------------------------------------
// Function: GetFrame
// Sender frame number
long Spout::GetFrame()
{
	return frame.GetSenderFrame();
}

//---------------------------------------------------------
// Function: GetHandle
// Sender share handle
HANDLE Spout::GetHandle()
{
	return m_dxShareHandle;
}

//---------------------------------------------------------
// Function: GetCPU
// Sender sharing method.
//   Returns true if the sender is using CPU methods
bool Spout::GetCPU()
{
	return m_bSenderCPU;
}

//---------------------------------------------------------
// Function: GetGLDX
//Sender sharing compatibility.
//  Returns true if the sender graphics hardware is 
//  compatible with NVIDIA NV_DX_interop2 extension
bool Spout::GetGLDX()
{
	return m_bSenderGLDX;
}

//
// Group: Receiver
//
// Receiving functions
//
// ReceiveTexture and ReceiveImage 
//
//		- Connect to a sender
//
//		- Set class variables for sender name, width and height
//
//		- If the sender has changed size, set a flag for the application to update the receiving texture or image if IsUpdated() returns true.
//
//		- Copy the sender shared texture to the user texture or image.
//
// Any changes to sender size are managed. However, if you are receiving to a local texture or image,
// the application must check for update at every cycle before receiving any data using "IsUpdated()"

//---------------------------------------------------------
// Function: SetReceiverName
// Specify sender for connection
//
//   The if a name is specified, the receiver will not connect to any other unless the user selects one
//   If that sender closes, the receiver will wait for the nominated sender to open 
//   If no name is specified, the receiver will connect to the active sender
void Spout::SetReceiverName(const char * SenderName)
{
	if (SenderName && SenderName[0]) {
		// Connect to the specified sender
		strcpy_s(m_SenderNameSetup, 256, SenderName);
		strcpy_s(m_SenderName, 256, SenderName);
	}
	else {
		// Connect to the active sender
		m_SenderNameSetup[0] = 0;
		m_SenderName[0] = 0;
	}
}

//---------------------------------------------------------
// Function: ReleaseReceiver
// Close receiver and release resources ready to connect to another sender
void Spout::ReleaseReceiver()
{
	if (!m_bInitialized)
		return;

	// Restore the starting sender name if the user specified one in SetReceiverName
	if (m_SenderNameSetup[0]) {
		strcpy_s(m_SenderName, 256, m_SenderNameSetup);
	}
	else {
		m_SenderName[0] = 0;
	}

	// Wait 4 frames in case the same sender opens again
	Sleep(67);

	// Close the named access mutex and frame counting semaphore.
	frame.CloseAccessMutex();
	frame.CleanupFrameCount();

	// Zero width and height so that they are reset when a sender is found
	m_Width = 0;
	m_Height = 0;

	// Reset the received sender texture
	if (m_pSharedTexture)
		spoutdx.ReleaseDX11Texture(GetDX11Device(), m_pSharedTexture);
	m_pSharedTexture = nullptr;
	m_dxShareHandle = nullptr;

	// Reset connected sender share mode and compatibility.
	// Assume texture share and hardware compatible by default.
	m_bSenderCPU = false;
	m_bSenderGLDX = true;

	// Release staging textures if they have been used
	if (m_pStaging[0]) spoutdx.ReleaseDX11Texture(spoutdx.GetDX11Device(), m_pStaging[0]);
	if (m_pStaging[1]) spoutdx.ReleaseDX11Texture(spoutdx.GetDX11Device(), m_pStaging[1]);
	m_pStaging[0] = nullptr;
	m_pStaging[1] = nullptr;
	m_Index = 0;
	m_NextIndex = 0;

	// Close shared memory and sync event if used
	memoryshare.Close();
	frame.CloseFrameSync();
	
	m_bConnected = false;
	m_bInitialized = false;

}

//---------------------------------------------------------
// Function: ReceiveTexture
//     Connect to a sender and retrieve shared texture details
bool Spout::ReceiveTexture()
{
	return ReceiveTexture(0, 0);
}

//---------------------------------------------------------
// Function: ReceiveTexture
//   Receive the sender shared texture
//
//   For a valid OpenGL receiving texture :
//
//   Copy from the sender shared texture if there is a texture to receive into.
//   The receiving OpenGL texture can only be RGBA of dimension (width * height)
//   and must be re-allocated for sender size change. Return if flagged for update.
//   The update flag is reset when the receiving application calls IsUpdated().
//
//   If no arguments are passed :
//
//   Connect to a sender and retrieve shared texture details,
//	 initialize GL/DX interop for OpenGL texture access, and update
//   the sender shared texture, frame count and framerate.
//   The texture can then be accessed using :
//
//		- BindSharedTexture();
//		- UnBindSharedTexture();
//		- GetSharedTextureID();
//
//   As with SendTexture, the host fbo argument is optional (default 0)
//   but an fbo ID is necessary if it is currently bound, then that binding
//   is restored. Otherwise the binding is lost.
//
bool Spout::ReceiveTexture(GLuint TextureID, GLuint TextureTarget, bool bInvert, GLuint HostFbo)
{
	// Return if flagged for update and there is a texture to receive into.
	// The update flag is reset when the receiving application calls IsUpdated().
	if (m_bUpdated && TextureID != 0 && TextureTarget != 0) {
		return true;
	}

	// Make sure OpenGL and DirectX are initialized
	if (!OpenSpout()) {
		return false;
	}

	// Try to receive texture details from a sender
	if (ReceiveSenderData()) {

		// Found a sender
		// The sender name, width, height, format, shared texture handle
		// and shared texture pointer have been retrieved
		// Let the application know
		m_bConnected = true;

		// If the connected sender sharehandle or name is different,
		// the receiver is re-initialized and m_bUpdated is set true
		// so that the application re-allocates the receiving texture.
		if (m_bUpdated) {
			// If the sender is new or changed, reset shared textures
			if (m_bTextureShare) {
				// CreateInterop set "true" for receiver
				if (!CreateInterop(m_Width, m_Height, m_dwFormat, true)) {
					return false;
				}
			}

			// If receiving to a texture, return to update it.
			// The application detects the change with IsUpdated().
			if (TextureID != 0 && TextureTarget != 0) {
				return true;
			}
		}

		// Was the sender's shared texture handle null
		// or has the user set 2.006 memoryshare mode?
		if (!m_dxShareHandle || m_bMemoryShare) {
			// Possible existence of 2.006 memoryshare sender (no texture handle)
			// (ReadMemoryTexture currently only works if texture share compatible)
			if (m_bTextureShare) {
				if (ReadMemoryTexture(m_SenderName, TextureID, TextureTarget, m_Width, m_Height, bInvert, HostFbo))
					return true;
			}
			// ReadMemoryTexture failed, is there is a texture share handle ?
			if (!m_dxShareHandle) {
				return false;
			}
			// This could be a 2.007 sender but the user has set 2.006 memoryshare mode
			// Drop though
		}

		if (m_bTextureShare) {
			// Texture share compatible
			// 3840x2160 60 fps - 0.45 msec/frame
			ReadGLDXtexture(TextureID, TextureTarget, m_Width, m_Height, bInvert, HostFbo);
		}
		else if (m_bCPUshare) {
			// Auto share enabled for DirectX CPU backup
			// 3840x2160 33 fps - 5-7 msec/frame
			ReadDX11texture(TextureID, TextureTarget, m_Width, m_Height, bInvert, HostFbo);
		}

	} // endif sender exists
	else {
		
		// ReceiveSenderData fails if there is no sender or the connected sender closed.
		ReleaseReceiver();
		// Let the application know.
		m_bConnected = false;
		
	}

	return m_bConnected;
}

//---------------------------------------------------------
// Function: ReceiveImage
// Copy the sender texture to image pixels.
//
//    Formats supported are : GL_RGBA, GL_RGB, GL_BGRA_EXT, GL_BGR_EXT.
//    GL_BGRA_EXT and GL_BGR_EXT are dependent on those extensions being supported at runtime.
//    If they are not, the rgba and rgb equivalents are used.
//    The same sender size changes are handled with IsUpdated() as for ReceiveTexture.
//    and the receiving buffer must be re-allocated if IsUpdated() returns true.
//    NOTE : images with padding on each line are not supported.
//    Also the width should be a multiple of 4
//
//    As with ReceiveTexture, the ID of a currently bound fbo should be passed in.
//
bool Spout::ReceiveImage(char* Sendername, unsigned int &width, unsigned int &height,
	unsigned char* pixels, GLenum glFormat, bool bInvert, GLuint HostFBO)
{
	if (ReceiveImage(pixels, glFormat, bInvert, HostFBO)) {
		strcpy_s(Sendername, 256, m_SenderName);
		width = m_Width;
		height = m_Height;
		return true;
	}
	return false;
}

//---------------------------------------------------------
// Function: IsUpdated
// Query whether the sender has changed.
//
//   Must be checked at every cycle before receiving data. 
//   If this is not done, the receiving functions fail.
//
bool Spout::IsUpdated()
{
	bool bRet = m_bUpdated;
	m_bUpdated = false; // Reset the update flag
	return bRet;
}

//---------------------------------------------------------
// Function: IsConnected
// Query sender connection.
//
//   If the sender closes, receiving functions return false,  
//   but connection can be tested at any time.
//
bool Spout::IsConnected()
{
	return m_bConnected;
}

//---------------------------------------------------------
// Function: IsFrameNew
// Query received frame status
//
//   The receiving texture or pixel buffer is refreshed if the sender has produced a new frame  
//   This can be queried to process texture data only for new frames
bool Spout::IsFrameNew()
{
	return frame.IsFrameNew();
}

//---------------------------------------------------------
// Function: GetSenderFormat
// Get sender DirectX texture format
DWORD Spout::GetSenderFormat()
{
	return m_dwFormat;
}

//---------------------------------------------------------
// Function: GetSenderName
// Get sender name
const char * Spout::GetSenderName()
{
	return m_SenderName;
}

//---------------------------------------------------------
// Function: GetSenderWidth
// Get sender width
unsigned int Spout::GetSenderWidth()
{
	return m_Width;
}

//---------------------------------------------------------
// Function: GetSenderHeight
// Get sender height
unsigned int Spout::GetSenderHeight()
{
	return m_Height;

}

//---------------------------------------------------------
// Function: GetSenderFps
// Get sender frame rate
double Spout::GetSenderFps()
{
	return frame.GetSenderFps();
}

//---------------------------------------------------------
// Function: GetSenderFrame
// Get sender frame number
long Spout::GetSenderFrame()
{
	return frame.GetSenderFrame();
}

//---------------------------------------------------------
// Function: GetSenderHandle
// Received sender share handle
HANDLE Spout::GetSenderHandle()
{
	return m_dxShareHandle;
}

//---------------------------------------------------------
// Function: GetSenderCPU
// Received sender sharing method.
//   Returns true if the sender is using CPU methods
bool Spout::GetSenderCPU()
{
	return m_bSenderCPU;
}

//---------------------------------------------------------
// Function: GetSenderGLDX
// Received sender sharing compatibility.
//   Returns true if the sender graphics hardware is 
//   compatible with NVIDIA NV_DX_interop2 extension
bool Spout::GetSenderGLDX()
{
	return m_bSenderGLDX;
}

//---------------------------------------------------------
// Function: SelectSender
// Open sender selection dialog
void Spout::SelectSender()
{
	SelectSenderPanel();
}

//
// Group: Frame counting
//

//---------------------------------------------------------
// Function: SetFrameCount
// Enable or disable frame counting globally
void Spout::SetFrameCount(bool bEnable)
{
	frame.SetFrameCount(bEnable);
}

// Function: DisableFrameCount
// Disable frame counting specifically for this application
void Spout::DisableFrameCount()
{
	frame.DisableFrameCount();
}

//---------------------------------------------------------
// Function: IsFrameCountEnabled
// Return frame count status
bool Spout::IsFrameCountEnabled()
{
	return frame.IsFrameCountEnabled();
}

//---------------------------------------------------------
// Function: HoldFps
// Frame rate control.
//    Desired frames per second.
void Spout::HoldFps(int fps)
{
	frame.HoldFps(fps);
}

// -----------------------------------------------
// Function: SetFrameSync
// Signal sync event.
//   Create a named sync event and set for test
void Spout::SetFrameSync(const char* SenderName)
{
	if (SenderName && SenderName[0] && m_bInitialized)
		frame.SetFrameSync(SenderName);
}

// -----------------------------------------------
// Function: WaitFrameSync
// Wait or test for named sync event.
// Wait until the sync event is signalled or the timeout elapses.
// Events are typically created based on the sender name and are
// effective between a single sender/receiver pair.
//   - For testing for a signal, use a wait timeout of zero.
//   - For synchronization, use a timeout greater than the expected delay
// 
bool Spout::WaitFrameSync(const char *SenderName, DWORD dwTimeout)
{
	if (!SenderName || !SenderName[0] || !m_bInitialized)
		return false;
	return frame.WaitFrameSync(SenderName, dwTimeout);
}

//
// Group: Sender names
//

//---------------------------------------------------------
// Function: GetSenderCount
// Number of senders
int Spout::GetSenderCount()
{
	return sendernames.GetSenderCount();
}

//---------------------------------------------------------
// Function: GetSender
// Sender item name in the sender names set
bool Spout::GetSender(int index, char* sendername, int MaxSize)
{
	return sendernames.GetSender(index, sendername, MaxSize);
}

//---------------------------------------------------------
// Function: GetSenderInfo
// Sender information
bool Spout::GetSenderInfo(const char* sendername, unsigned int &width, unsigned int &height, HANDLE &dxShareHandle, DWORD &dwFormat)
{
	return sendernames.GetSenderInfo(sendername, width, height, dxShareHandle, dwFormat);
}

//---------------------------------------------------------
// Function: GetActiveSender
// Current active sender name
bool Spout::GetActiveSender(char* Sendername)
{
	return sendernames.GetActiveSender(Sendername);
}

//---------------------------------------------------------
// Function: SetActiveSender
// Set sender as active
bool Spout::SetActiveSender(const char* Sendername)
{
	return sendernames.SetActiveSender(Sendername);
}

//
// Group: Graphics adapter
//
// Note that both the Sender and Receiver must use the same graphics adapter.
//

//---------------------------------------------------------
// Function: GetNumAdapters
// The number of graphics adapters in the system
int Spout::GetNumAdapters()
{
	return spoutdx.GetNumAdapters();
}

//---------------------------------------------------------
// Function: GetAdapterName
// Get adapter item name
bool Spout::GetAdapterName(int index, char *adaptername, int maxchars)
{
	char name[256];
	if (spoutdx.GetAdapterName(index, name, 256)) {
		strcpy_s(adaptername, maxchars, name);
		return true;
	}
	return false;
}

//---------------------------------------------------------
// Function: AdapterName
// Return current adapter name
char * Spout::AdapterName()
{
	GetAdapterName(spoutdx.GetAdapter(), m_AdapterName, 256);
	return m_AdapterName;
}

//---------------------------------------------------------
// Function: GetAdapter
// Get current adapter index
int Spout::GetAdapter()
{
	return spoutdx.GetAdapter();
}

//---------------------------------------------------------
// Function: SetAdapter
// Set graphics adapter for output
bool Spout::SetAdapter(int index)
{
	// Set the adapter as requested
	if (!spoutdx.SetAdapter(index)) {
		SpoutLogError("Spout::SetAdapter(%d) failed", index);
		return false;
	}

	// SetAdapter has tested DirectX, but the adapter is different
	// so check again for GL/DX compatibility
	if (!GLDXready()) {
		SpoutLogWarning("Spout::SetAdapter - Graphics not GL/DX compatible. Switching to CPU share mode");
		m_bUseGLDX = false;
	}

	return true;
}

//---------------------------------------------------------
// Function: GetSenderAdapter
// Get sender adapter index and name for a given sender
//
// Testing only.
// Note that OpenDX11shareHandle fails and can crash if the share handle 
// has been created using a different graphics adapter (see spoutDirectX)
// Try/Catch can catch the exception but not recommended for general use.
int Spout::GetSenderAdapter(const char* sendername, char* adaptername, int maxchars)
{
	if (!sendername || !sendername[0])
		return -1;

	int senderadapter = -1;
	ID3D11Texture2D* pSharedTexture = nullptr;
	ID3D11Device* pDummyDevice = nullptr;
	ID3D11DeviceContext* pContext = nullptr;
	IDXGIAdapter* pAdapter = nullptr;

	// Get the current device adapter pointer (could be null default)
	IDXGIAdapter* pCurrentAdapter = spoutdx.GetAdapterPointer();

	SpoutLogNotice("Spout::GetSenderAdapter - testing for sender adapter (%s)", sendername);

	SharedTextureInfo info;
	if (sendernames.getSharedInfo(sendername, &info)) {
		int nAdapters = spoutdx.GetNumAdapters();
		for (int i = 0; i < nAdapters; i++) {
			pAdapter = spoutdx.GetAdapterPointer(i);
			if (pAdapter) {
				SpoutLogNotice("   testing adapter %d", i);
				// Set the adapter pointer for CreateDX11device to use temporarily
				spoutdx.SetAdapterPointer(pAdapter);
				// Create a dummy device using this adapter
				pDummyDevice = spoutdx.CreateDX11device();
				if (pDummyDevice) {
					// Try to open the share handle with the device created from the adapter
					if (spoutdx.OpenDX11shareHandle(pDummyDevice, &pSharedTexture, LongToHandle((long)info.shareHandle))) {
						// break as soon as it succeeds
						SpoutLogNotice("    found sender adapter %d (0x%.7X)", i, PtrToUint(pAdapter));
						senderadapter = i;
						// Return the adapter name
						if (adaptername)
							spoutdx.GetAdapterName(i, adaptername, maxchars);
						pDummyDevice->GetImmediateContext(&pContext);
						if (pContext) pContext->Flush();
						pDummyDevice->Release();
						pAdapter->Release();
						break;
					}
					pDummyDevice->GetImmediateContext(&pContext);
					if (pContext) pContext->Flush();
					pDummyDevice->Release();
				}
				pAdapter->Release();
			}
		}
	}

	// Set the SpoutDirectX class adapter pointer back to what it was
	spoutdx.SetAdapterPointer(pCurrentAdapter);

	return senderadapter;

}


//---------------------------------------------------------
// Function: GetAdapterInfo
// Get the current adapter description
bool Spout::GetAdapterInfo(char *renderdescription, char *displaydescription, int maxchars)
{
	return spoutdx.GetAdapterInfo(renderdescription, displaydescription, maxchars);
}


//
// Group: 2.006 compatibility
//
// These functions are not necessary for Version 2.007
// and should not be used for a new application.
// They are retained for compatibility with existing 2.006 code
// and may be removed in future release.
// For full compatibility with exsiting 2.006 code, the original
// 2.006 SDK is preserved in a separate branch :
//
// https://github.com/leadedge/Spout2/tree/2.006
//

//---------------------------------------------------------
// Function: FindNVIDIA
// Find the index of the NVIDIA adapter in a multi-adapter system
bool Spout::FindNVIDIA(int &nAdapter)
{
	return spoutdx.FindNVIDIA(nAdapter);
}

//---------------------------------------------------------
// Function: GetAdapterInfo
// Get detailed information for the current graphics adapter
// Must be called after DirectX initialization, not before
//
// NOTES : On a “normal” system EnumDisplayDevices and IDXGIAdapter::GetDesc always concur
// i.e. the device that owns the head will be the device that performs the rendering. 
// On an Optimus system IDXGIAdapter::GetDesc will return whichever device has been selected for rendering.
// So on an Optimus system it is possible that IDXGIAdapter::GetDesc will return the dGPU whereas 
// EnumDisplayDevices will return the iGPU.
//
// This function compares the adapter descriptions of the two
// The string "Intel" reveals that it is an Intel device but 
// the Vendor ID could also be used. For example :
//	- 0x10DE NVIDIA
//	- 0x163C Intel
//	- 0x8086 Intel
//	- 0x8087 Intel
// See also the DirectX only version :
// bool spoutDirectX::GetAdapterInfo(char *adapter, char *display, int maxchars)
//
bool Spout::GetAdapterInfo(char* renderadapter,
	char* renderdescription, char* renderversion,
	char* displaydescription, char* displayversion,
	int maxsize, bool &bDX9)
{
	// DirectX9 not supported
	UNREFERENCED_PARAMETER(bDX9);

	IDXGIDevice * pDXGIDevice = nullptr;

	renderadapter[0] = 0; // DirectX adapter
	renderdescription[0] = 0;
	renderversion[0] = 0;
	displaydescription[0] = 0;
	displayversion[0] = 0;
	if (!spoutdx.GetDX11Device()) {
		SpoutLogError("spoutGLDXinterop::GetAdapterInfo - no DX11 device");
		return false;
	}

	spoutdx.GetDX11Device()->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
	IDXGIAdapter * pDXGIAdapter;
	pDXGIDevice->GetAdapter(&pDXGIAdapter);
	DXGI_ADAPTER_DESC adapterinfo;
	pDXGIAdapter->GetDesc(&adapterinfo);
	// WCHAR Description[ 128 ];
	// UINT VendorId;
	// UINT DeviceId;
	// UINT SubSysId;
	// UINT Revision;
	// SIZE_T DedicatedVideoMemory;
	// SIZE_T DedicatedSystemMemory;
	// SIZE_T SharedSystemMemory;
	// LUID AdapterLuid;
	char output[256];
	size_t charsConverted = 0;
	wcstombs_s(&charsConverted, output, 129, adapterinfo.Description, 128);
	// printf("    Description = [%s]\n", output);
	// printf("    VendorId = [%d] [%x]\n", adapterinfo.VendorId, adapterinfo.VendorId);
	// printf("SubSysId = [%d] [%x]\n", adapterinfo.SubSysId, adapterinfo.SubSysId);
	// printf("DeviceId = [%d] [%x]\n", adapterinfo.DeviceId, adapterinfo.DeviceId);
	// printf("Revision = [%d] [%x]\n", adapterinfo.Revision, adapterinfo.Revision);
	strcpy_s(renderadapter, (rsize_t)maxsize, output);

	if (!renderadapter[0])
		return false;

	strcpy_s(renderdescription, (rsize_t)maxsize, renderadapter);

	// Use Windows functions to look for Intel graphics to see if it is
	// the same render adapter that was detected with DirectX
	char driverdescription[256];
	char driverversion[256];
	char regkey[256];

	// Additional info
	DISPLAY_DEVICE DisplayDevice;
	DisplayDevice.cb = sizeof(DISPLAY_DEVICE);

	// Detect the adapter attached to the desktop.
	//
	// To select all display devices in the desktop, use only the display devices
	// that have the DISPLAY_DEVICE_ATTACHED_TO_DESKTOP flag in the DISPLAY_DEVICE structure.
	int nDevices = 0;
	for (int i = 0; i < 10; i++) { // should be much less than 10 adapters
		if (EnumDisplayDevices(NULL, (DWORD)i, &DisplayDevice, 0)) {
			// This will list all the devices
			nDevices++;
			// Get the registry key
			wcstombs_s(&charsConverted, regkey, 129, (const wchar_t *)DisplayDevice.DeviceKey, 128);
			// This is the registry key with all the information about the adapter
			OpenDeviceKey(regkey, 256, driverdescription, driverversion);
			// Is it a render adapter ?
			if (renderadapter && strcmp(driverdescription, renderadapter) == 0) {
				strcpy_s(renderdescription, (rsize_t)maxsize, driverdescription);
				strcpy_s(renderversion, (rsize_t)maxsize, driverversion);
			}
			// Is it a display adapter
			if (DisplayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
				strcpy_s(displaydescription, 256, driverdescription);
				strcpy_s(displayversion, 256, driverversion);
			} // endif attached to desktop

		} // endif EnumDisplayDevices
	} // end search loop

	// The render adapter description
	if (renderdescription) trim(renderdescription);

	if (pDXGIDevice) pDXGIDevice->Release();

	return true;
}

//---------------------------------------------------------
// Function: CreateSender
// Create a sender
bool Spout::CreateSender(const char* name, unsigned int width, unsigned int height, DWORD dwFormat)
{
	// Pass on to CheckSender
	SetSenderName(name);
	if (dwFormat > 0)
		m_dwFormat = dwFormat;

	return CheckSender(width, height);

}

//---------------------------------------------------------
// Function: UpdateSender
// Update a sender
bool Spout::UpdateSender(const char* name, unsigned int width, unsigned int height)
{
	// No update unless already created
	if (!IsInitialized()) {
		return false;
	}

	// For a name change, close the sender and set up again
	if (strcmp(name, m_SenderName) != 0)
		ReleaseSender();

	// CheckSender sets m_Width and m_Height on success
	return CheckSender(width, height);
}

//---------------------------------------------------------
// Function: CreateReceiver
// Create receiver connection
bool Spout::CreateReceiver(char* sendername, unsigned int &width, unsigned int &height, bool bUseActive)
{
	UNREFERENCED_PARAMETER(bUseActive); // no longer used

	if (!OpenSpout())
		return false;
	
	if (ReceiveSenderData()) {
		// The sender name, width, height, format, shared texture handle
		// and shared texture pointer have been retrieved.
		if (m_bUpdated) {
			// If the sender is new or changed, create or re-create interop
			if (m_bTextureShare) {
				// Flag "true" for receive
				if (!CreateInterop(m_Width, m_Height, m_dwFormat, true))
					return false;
			}
			// 2.006 receivers check for changed sender size
			m_bUpdated = false;
		}
		strcpy_s(sendername, 256, m_SenderName);
		width = m_Width;
		height = m_Height;

		return true;
	}

	return false;

}

//---------------------------------------------------------
// Function: CheckReceiver
// Check receiver connection
bool Spout::CheckReceiver(char* name, unsigned int &width, unsigned int &height, bool &bConnected)
{
	if (ReceiveSenderData()) {
		strcpy_s(name, 256, m_SenderName);
		width = m_Width;
		height = m_Height;
		bConnected = m_bConnected;
		return true;
	}
	return false;
}

//---------------------------------------------------------
// Function: ReceiveTexture
// Receive OpenGL texture
bool Spout::ReceiveTexture(char* name, unsigned int &width, unsigned int &height,
	GLuint TextureID, GLuint TextureTarget, bool bInvert, GLuint HostFBO)
{
	if (ReceiveTexture(TextureID, TextureTarget, bInvert, HostFBO)) {

		// 2.006 receivers have to restart for a new sender name
		if (m_SenderName[0] && strcmp(m_SenderName, name) != 0) {
			return false;
		}

		strcpy_s(name, 256, m_SenderName);
		width = m_Width;
		height = m_Height;
		return true;
	}

	return false;

}

//---------------------------------------------------------
// Function: ReceiveImage
// Receive image pixels
bool Spout::ReceiveImage(unsigned char *pixels, GLenum glFormat, bool bInvert, GLuint HostFbo)
{
	// Return if flagged for update
	// The update flag is reset when the receiving application calls IsUpdated()
	if (m_bUpdated) {
		return true;
	}

	// Make sure OpenGL and DirectX are initialized
	if (!OpenSpout())
		return false;

	// Only RGBA, BGRA, RGB, BGR supported
	if (!(glFormat == GL_RGBA || glFormat == GL_BGRA_EXT || glFormat == GL_RGB || glFormat == GL_BGR_EXT))
		return false;

	// Check for BGRA support
	GLenum glformat = glFormat;
	if (!m_bBGRAavailable) {
		// If the bgra extensions are not available and the user
		// provided GL_BGR_EXT or GL_BGRA_EXT do not use them
		if (glFormat == GL_BGR_EXT) glformat = GL_RGB; // GL_BGR_EXT
		if (glFormat == GL_BGRA_EXT) glformat = GL_RGBA; // GL_BGRA_EXT
	}

	// Try to receive texture details from a sender
	if (ReceiveSenderData()) {

		// The sender name, width, height, format, shared texture handle and pointer have been retrieved.
		if (m_bUpdated) {
			// If the sender is new or changed, return to update the receiving texture.
			// The application detects the change with IsUpdated().
			if (m_bTextureShare) {
				// Flag "true" for receive
				if (!CreateInterop(m_Width, m_Height, m_dwFormat, true))
					return false;
			}
			return true;
		}

		// The receiving pixel buffer is created after the first update
		// So check here instead of at the beginning
		if (!pixels)
			return false;

		//
		// Found a sender
		//
		// Read the shared texture into the pixel buffer
		// Copy functions handle the formats supported
		//

		// Was the sender's shared texture handle null ?
		if (!m_dxShareHandle || m_bMemoryShare) {
			// Possible existence of sender memory share map
			// Currently only works for Texture share mode
			if (m_bTextureShare) {
				ReadMemoryPixels(m_SenderName, pixels, m_Width, m_Height, glFormat, bInvert);
			}
		}
		else if (m_bTextureShare) {
			// Texture share compatible
			// Read pixels using OpenGL via PBO
			// PBO (UnloadTexturePixels)
			// 1920x1080 RGB 1.4 msec/frame RGBA 1.6 msec/frame
			// 3840x2160 RGB 5 msec/frame RGBA 6 msec/frame
			// FBO (ReadTextureData) - slower than DirectX method
			// (3840x2160 RGB 30-60 msec/frame RGBA 30-60 msec/frame)
			ReadGLDXpixels(pixels, m_Width, m_Height, glformat, bInvert, HostFbo);
		}
		else if (m_bCPUshare) {
			// Auto share enabled for DirectX CPU backup
			// Read pixels via DX11 staging textures to an rgba or rgb buffer
			// 1920x1080 RGB 7 msec/frame RGBA 2 msec/frame
			// 3840x2160 RGB 30 msec/frame RGBA 9 msec/frame
			ReadDX11pixels(pixels, m_Width, m_Height, glformat, bInvert);
		}
		m_bConnected = true;
	} // sender exists
	else {
		// There is no sender or the connected sender closed.
		ReleaseReceiver();
		// Let the application know.
		m_bConnected = false;
	}

	// ReceiveImage fails if there is no sender or the connected sender closed.
	return m_bConnected;

} // end ReceiveImage

//---------------------------------------------------------
// Function: SelectSenderPanel
// Open dialog for the user to select a sender
//
//  Optional message argument
//
// Replaced by SelectSender for 2.007
bool Spout::SelectSenderPanel(const char *message)
{
	HANDLE hMutex1 = NULL;
	HMODULE module = NULL;
	char path[MAX_PATH], drive[MAX_PATH], dir[MAX_PATH], fname[MAX_PATH];
	char UserMessage[512];

	if (message != NULL && message[0] != 0)
		strcpy_s(UserMessage, 512, message); // could be an arg or a user message
	else
		UserMessage[0] = 0; // make sure SpoutPanel does not see an un-initialized string

	// The selected sender is then the "Active" sender and this receiver switches to it.
	// If Spout is not installed, SpoutPanel.exe has to be in the same folder
	// as this executable. This rather complicated process avoids having to use a dialog
	// which causes problems with host GUI messaging.

	// First find if there has been a Spout installation >= 2.002 with an install path for SpoutPanel.exe
	if (!ReadPathFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutPanel", "InstallPath", path)) {

		// Path not registered so find the path of the host program
		// where SpoutPanel should have been copied
		module = GetModuleHandle(NULL);
		GetModuleFileNameA(module, path, MAX_PATH);
		_splitpath_s(path, drive, MAX_PATH, dir, MAX_PATH, fname, MAX_PATH, NULL, 0);
		_makepath_s(path, MAX_PATH, drive, dir, "SpoutPanel", ".exe");
		// Does SpoutPanel.exe exist in this path ?
		if (!PathFileExistsA(path)) {
			// Try the current working directory
			if (_getcwd(path, MAX_PATH)) {
				strcat_s(path, MAX_PATH, "\\SpoutPanel.exe");
				// Does SpoutPanel exist here?
				if (!PathFileExistsA(path)) {
					SpoutLogWarning("spoutDX::SelectSender - SpoutPanel path not found");
					return false;
				}
			}
		}
	}

	// Check whether the panel is already running
	// Try to open the application mutex.
	hMutex1 = OpenMutexA(MUTEX_ALL_ACCESS, 0, "SpoutPanel");
	if (!hMutex1) {
		// No mutex, so not running, so can open it
		// Use ShellExecuteEx so we can test its return value later
		ZeroMemory(&m_ShExecInfo, sizeof(m_ShExecInfo));
		m_ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
		m_ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		m_ShExecInfo.hwnd = NULL;
		m_ShExecInfo.lpVerb = NULL;
		m_ShExecInfo.lpFile = (LPCSTR)path;
		m_ShExecInfo.lpParameters = UserMessage;
		m_ShExecInfo.lpDirectory = NULL;
		m_ShExecInfo.nShow = SW_SHOW;
		m_ShExecInfo.hInstApp = NULL;
		ShellExecuteExA(&m_ShExecInfo);

		//
		// The flag "m_bSpoutPanelOpened" is set here to indicate that the user
		// has opened the panel to select a sender. This flag is local to 
		// this process so will not affect any other receiver instance
		// Then when the selection panel closes, sender name is tested
		//
		m_bSpoutPanelOpened = true;

	}
	else {
		// The mutex exists, so another instance is already running.
		// Find the SpoutPanel window and bring it to the top.
		// SpoutPanel is opened as topmost anyway but pop it to
		// the front in case anything else has stolen topmost.
		HWND hWnd = FindWindowA(NULL, (LPCSTR)"SpoutPanel");
		if (hWnd && IsWindow(hWnd)) {
			SetForegroundWindow(hWnd);
			// prevent other windows from hiding the dialog
			// and open the window wherever the user clicked
			SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
		}
		else if (path[0]) {
			// If the window was not found but the mutex exists
			// and SpoutPanel is installed, it has crashed.
			// Terminate the process and the mutex or the mutex will remain
			// and SpoutPanel will not be started again.
			PROCESSENTRY32 pEntry;
			pEntry.dwSize = sizeof(pEntry);
			bool done = false;
			// Take a snapshot of all processes and threads in the system
			HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPALL, NULL);
			if (hProcessSnap == INVALID_HANDLE_VALUE) {
				SpoutLogError("spoutDX::OpenSpoutPanel - CreateToolhelp32Snapshot error");
			}
			else {
				// Retrieve information about the first process
				BOOL hRes = Process32First(hProcessSnap, &pEntry);
				if (!hRes) {
					SpoutLogError("spoutDX::OpenSpoutPanel - Process32First error");
					CloseHandle(hProcessSnap);
				}
				else {
					// Look through all processes
					while (hRes && !done) {
						int value = _tcsicmp(pEntry.szExeFile, _T("SpoutPanel.exe"));
						if (value == 0) {
							HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, 0, (DWORD)pEntry.th32ProcessID);
							if (hProcess != NULL) {
								// Terminate SpoutPanel and it's mutex
								TerminateProcess(hProcess, 9);
								CloseHandle(hProcess);
								done = true;
							}
						}
						if (!done)
							hRes = Process32Next(hProcessSnap, &pEntry); // Get the next process
						else
							hRes = NULL; // found SpoutPanel
					}
					CloseHandle(hProcessSnap);
				}
			}
			// Now SpoutPanel will start the next time the user activates it
		} // endif SpoutPanel crashed
	} // endif SpoutPanel already open

	// If we opened the mutex, close it now or it is never released
	if (hMutex1) CloseHandle(hMutex1);

	return true;

} // end SelectSenderPanel


//
// Group: Legacy OpenGL Draw functions
//
// These functions are retained for compatibility with existing 2.006 code.
//
// Enabled for build with "legacyOpenGL" defined in SpoutCommon.h
//
#ifdef legacyOpenGL

//---------------------------------------------------------
// Function: DrawSharedTexture
// Render the sender shared OpenGL texture
bool Spout::DrawSharedTexture(float max_x, float max_y, float aspect, bool bInvert, GLuint HostFBO)
{
	UNREFERENCED_PARAMETER(HostFBO);
	if (!m_hInteropDevice || !m_hInteropObject)
		return false;

	bool bRet = false;

	// Wait for access to the shared texture
	if (frame.CheckTextureAccess(m_pSharedTexture)) {
		// go ahead and access the shared texture to draw it
		if (LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {
			SaveOpenGLstate(m_Width, m_Height);
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, m_glTexture); // bind shared texture
			glColor4f(1.f, 1.f, 1.f, 1.f);
			// Tried to convert to vertex array, but Processing crash
			glBegin(GL_QUADS);
			if (bInvert) {
				glTexCoord2f(0.0, max_y);	glVertex2f(-aspect, -1.0); // lower left
				glTexCoord2f(0.0, 0.0);	glVertex2f(-aspect, 1.0); // upper left
				glTexCoord2f(max_x, 0.0);	glVertex2f(aspect, 1.0); // upper right
				glTexCoord2f(max_x, max_y);	glVertex2f(aspect, -1.0); // lower right
			}
			else {
				glTexCoord2f(0.0, 0.0);	glVertex2f(-aspect, -1.0); // lower left
				glTexCoord2f(0.0, max_y);	glVertex2f(-aspect, 1.0); // upper left
				glTexCoord2f(max_x, max_y);	glVertex2f(aspect, 1.0); // upper right
				glTexCoord2f(max_x, 0.0);	glVertex2f(aspect, -1.0); // lower right
			}
			glEnd();
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);
			RestoreOpenGLstate();
			UnlockInteropObject(m_hInteropDevice, &m_hInteropObject); // unlock dx object
			bRet = true;
		} // lock failed
		// Release mutex and allow access to the texture
		frame.AllowTextureAccess(m_pSharedTexture);
	} // mutex lock failed

	return bRet;

} // end DrawSharedTexture

//---------------------------------------------------------
// Function: DrawToSharedTexture
// Render OpenGL texture to the sender shared OpenGL texture.
bool Spout::DrawToSharedTexture(GLuint TextureID, GLuint TextureTarget,
	unsigned int width, unsigned int height,
	float max_x, float max_y, float aspect,
	bool bInvert, GLuint HostFBO)
{
	GLenum status;
	bool bRet = false;

	if (!m_hInteropDevice || !m_hInteropObject)
		return false;

	if (width != (unsigned  int)m_Width || height != (unsigned  int)m_Height)
		return false;

	// Wait for access to the shared texture
	if (frame.CheckTextureAccess(m_pSharedTexture)) {
		if (LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {
			// Draw the input texture into the shared texture via an fbo
			// Bind our fbo and attach the shared texture to it
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
			glClearColor(0.f, 0.f, 0.f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT);
			glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_glTexture, 0);
			status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
			if (status == GL_FRAMEBUFFER_COMPLETE_EXT) {
				glColor4f(1.f, 1.f, 1.f, 1.f);
				glEnable(TextureTarget);
				glBindTexture(TextureTarget, TextureID);
				GLfloat tc[4][2] = { 0 };
				// Invert texture coord to user requirements
				if (bInvert) {
					tc[0][0] = 0.0;   tc[0][1] = max_y;
					tc[1][0] = 0.0;   tc[1][1] = 0.0;
					tc[2][0] = max_x; tc[2][1] = 0.0;
					tc[3][0] = max_x; tc[3][1] = max_y;
				}
				else {
					tc[0][0] = 0.0;   tc[0][1] = 0.0;
					tc[1][0] = 0.0;   tc[1][1] = max_y;
					tc[2][0] = max_x; tc[2][1] = max_y;
					tc[3][0] = max_x; tc[3][1] = 0.0;
				}
				GLfloat verts[] = {
								-aspect, -1.0,   // bottom left
								-aspect,  1.0,   // top left
								 aspect,  1.0,   // top right
								 aspect, -1.0 }; // bottom right
				glEnableClientState(GL_TEXTURE_COORD_ARRAY);
				glTexCoordPointer(2, GL_FLOAT, 0, tc);
				glEnableClientState(GL_VERTEX_ARRAY);
				glVertexPointer(2, GL_FLOAT, 0, verts);
				glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
				glDisableClientState(GL_VERTEX_ARRAY);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);
				glBindTexture(TextureTarget, 0);
				glDisable(TextureTarget);
				bRet = true; // success
			}
			else {
				PrintFBOstatus(status);
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
				UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
			}
			// restore the previous fbo - default is 0
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
			UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
		} // end interop lock
		// Release mutex and allow access to the texture
		frame.AllowTextureAccess(m_pSharedTexture);
	} // mutex access failed

	return bRet;

} // end DrawToSharedTexture
#endif

//
// Protected functions
//

//---------------------------------------------------------
// If a sender has not been created yet
//    o Make sure Spout has been initialized and OpenGL context is available
//    o Perform a compatibility check for GL/DX interop
//    o If compatible, create interop for GL/DX transfer
//    o If not compatible, create a shared texture for the sender
//    o Create a sender using the DX11 shared texture handle
// If the sender exists, test for size change
//    o If compatible, update the shared textures and GL/DX interop
//    o If not compatible, re-create the class shared texture to the new size
//    o Update the sender and class variables	
bool Spout::CheckSender(unsigned int width, unsigned int height)
{
	if (width == 0 || height == 0) {
		return false;
	}

	// The sender needs a name
	// Default is the executable name
	if (!m_SenderName[0]) {
		SetSenderName();
	}
	
	// If not initialized, create a new sender
	if (!m_bInitialized) {

		// Make sure that Spout has been initialized and an OpenGL context is available
		if (OpenSpout()) {

			if (m_bTextureShare) {
				// Create interop for GL/DX transfer
				// Flag "false" for sender so that a new shared texture is created.
				// For a receiver the shared texture is created from the sender share handle.
				if (!CreateInterop(width, height, m_dwFormat, false))
					return false;
			}
			else {
				// For CPU share with DirectX textures.
				// A sender creates a new shared texture within this class with a new share handle
				m_dxShareHandle = nullptr;
				if (!spoutdx.CreateSharedDX11Texture(spoutdx.GetDX11Device(),
					width, height, (DXGI_FORMAT)m_dwFormat, &m_pSharedTexture, m_dxShareHandle)) {
					return false;
				}
			}

			// Create a sender using the DX11 shared texture handle (m_dxShareHandle)
			if (sendernames.CreateSender(m_SenderName, width, height, m_dxShareHandle, m_dwFormat)) {
				m_Width = width;
				m_Height = height;
				//
				// SetSenderID writes to the sender shared texture memory
				// to set sender CPU sharing mode and hardware GL/DX compatibility
				//
				// Using CPU sharing methods (m_bCPUshare) - set top bit
				// 1000 0000 0000 0000 0000 0000 0000 0000
				//
				// GL/DX compatible hardware (m_bUseGLDX) - set next to top bit
				// 0100 0000 0000 0000 0000 0000 0000 0000
				//
				// Both bits can be set if GL/DX compatible but the user has selected CPU share mode
				//
				SetSenderID(m_SenderName, m_bCPUshare, m_bUseGLDX);

				m_Width = width;
				m_Height = height;

				// Create a sender mutex for access to the shared texture
				frame.CreateAccessMutex(m_SenderName);

				// Enable frame counting so the receiver gets frame number and fps
				frame.EnableFrameCount(m_SenderName);
				
				m_bInitialized = true;
			}
			else {
				ReleaseSender();
				m_SenderName[0] = 0;
				m_Width = 0;
				m_Height = 0;
				m_dwFormat = m_DX11format;
				return false;
			}
		}
	}
	// The sender is initialized but has the sending texture changed size ?
	else if (m_Width != width || m_Height != height) {
		// Update the shared textures and interop
		if (m_bTextureShare) {
			// Flag "false" for sender to create a new shared texture
			if (!CreateInterop(width, height, m_dwFormat, false))
				return false;
		}
		else {
			// Re-create the class shared texture to the new size
			if (m_pSharedTexture)
				spoutdx.ReleaseDX11Texture(GetDX11Device(), m_pSharedTexture);
			m_pSharedTexture = nullptr;
			m_dxShareHandle = nullptr;
			if (!spoutdx.CreateSharedDX11Texture(spoutdx.GetDX11Device(),
				width, height, (DXGI_FORMAT)m_dwFormat, &m_pSharedTexture, m_dxShareHandle)) {
				return false;
			}
		}
		// Update the sender with the new texture and size
		if (!sendernames.UpdateSender(m_SenderName, width, height, m_dxShareHandle, m_dwFormat)) {
			ReleaseSender();
			m_SenderName[0] = 0;
			m_Width = 0;
			m_Height = 0;
			m_dwFormat = m_DX11format;
			return false;
		}

		m_Width = width;
		m_Height = height;
	}

	// endif initialization or size checks

	return true;
}

//---------------------------------------------------------
void Spout::InitReceiver(const char * SenderName, unsigned int width, unsigned int height, DWORD dwFormat)
{
	SpoutLogNotice("Spout::InitReceiver(%s, %d x %d)", SenderName, width, height);

	// Create a named sender mutex for access to the sender's shared texture
	frame.CreateAccessMutex(SenderName);

	// Enable frame counting to get the sender frame number and fps
	frame.EnableFrameCount(SenderName);

	// Set class globals
	strcpy_s(m_SenderName, 256, SenderName);
	m_Width = width;
	m_Height = height;
	m_dwFormat = dwFormat;

	m_bInitialized = true;

}

//---------------------------------------------------------
//	o Connect to a sender and inform the application to update texture dimensions
//	o Check for user sender selection
//  o Receive texture details from the sender for write to the user texture
//  o Retrieve width, height, format, share handle and texture pointer
bool Spout::ReceiveSenderData()
{
	m_bUpdated = false;

	// Initialization is recorded in this class for sender or receiver
	// m_Width or m_Height are established when the receiver connects to a sender

	char sendername[256];
	strcpy_s(sendername, 256, m_SenderName);

	// Check again to see if the sender exists
	if (sendername[0] == 0) {
		// Passed name was null, so find the active sender
		if (!GetActiveSender(sendername))
			return false; // No sender
	}

	// If SpoutPanel has been opened, the active sender name could be different
	if (CheckSpoutPanel(sendername, 256)) {
		// Disable the setup name
		m_SenderNameSetup[0] = 0;
	}

	// Now we have either an existing sender name or the active sender name
	// Save current sender name and dimensions to test for change
	unsigned int width = m_Width;
	unsigned int height = m_Height;
	DWORD dwFormat = m_dwFormat;
	HANDLE dxShareHandle = m_dxShareHandle;

	// Try to get the sender information
	// Retrieve width, height, sharehandle and format.
	SharedTextureInfo info;
	if (sendernames.getSharedInfo(sendername, &info)) {

		width = info.width;
		height = info.height;
		dxShareHandle = (HANDLE)(LongToHandle((long)info.shareHandle));
		dwFormat = info.format;

		// GPU texture share and hardware GL/DX compatible by default
		m_bSenderCPU  = false;
		m_bSenderGLDX = true;
		
		//
		// 32 bit partner ID field
		//
		// Top bit
		//   o Sender is using CPU share methods
		//   o Hardware GL/DX compatibility undefined - assume false
		if (info.partnerId == 0x80000000) {
			m_bSenderCPU = true;
			m_bSenderGLDX = false;
		}

		// Next top bit only
		//   o Sender hardware is GL/DX compatible
		//   o Using texture share methods
		if (info.partnerId == 0x40000000) {
			m_bSenderCPU = false;
			m_bSenderGLDX = true;
		}

		// Both bits set
		//   o Sender is using CPU share methods
		//   o Sender hardware is GL/DX compatible
		if (info.partnerId == 0xC0000000) {
			m_bSenderCPU = true;
			m_bSenderGLDX = true;
		}

		// Compatible DX9 formats
		// 21 =	D3DFMT_A8R8G8B8
		// 22 = D3DFMT_X8R8G8B8
		if (dwFormat == 21 || dwFormat == 21) {
			// Create a DX11 receiving texture with compatible format
			dwFormat = (DWORD)DXGI_FORMAT_B8G8R8A8_UNORM;
		}

		// The shared texture handle will be different
		//   o for texture size or format change
		//   o for a new sender
		if (dxShareHandle != m_dxShareHandle || strcmp(sendername, m_SenderName) != 0) {
		
			// Release everything and start again
			ReleaseReceiver();

			// Update the sender share handle
			m_dxShareHandle = dxShareHandle;

			// We have a valid share handle
			if (m_dxShareHandle) {

				// Get a new shared texture pointer (m_pSharedTexture)
				if (!spoutdx.OpenDX11shareHandle(spoutdx.GetDX11Device(), &m_pSharedTexture, dxShareHandle)) {
					
					// If this fails, something is wrong.
					// The sender graphics adapter might be different.
					// Warning not required here -Error log is generated in OpenDX11shareHandle.

					// Retain the share handle so we don't query the same sender again.
					// m_pSharedTexture is null but will not be used.
					// Wait until another sender is selected or the shared texture handle is valid.

					return true;
				}
			}

			// Now we have a shared texture pointer or a null share handle.

			// For a null share handle from a 2.006 memoryshare sender
			// ReceiveTexture and ReceiveImage will look for the shared memory map

			// Initialize again with the newly connected sender values
			InitReceiver(sendername, width, height, dwFormat);

			m_bUpdated = true; // Return to update the receiving texture or image

		}

		// Connected and intialized
		// Sender name, width, height, format, texture pointer and share handle have been retrieved

		// For debugging
		// printf("    m_dxShareHandle = 0x%7X : m_pSharedTexture = 0x%7X\n", PtrToUint(m_dxShareHandle), PtrToUint(m_pSharedTexture));
		// ID3D11Texture2D * texturePointer = m_pSharedTexture;
		// D3D11_TEXTURE2D_DESC td;
		// texturePointer->GetDesc(&td);
		// printf("td.Format = %d\n", td.Format); // 87
		// printf("td.Width = %d\n", td.Width);
		// printf("td.Height = %d\n", td.Height);
		// printf("td.MipLevels = %d\n", td.MipLevels);
		// printf("td.Usage = %d\n", td.Usage);
		// printf("td.ArraySize = %d\n",  td.ArraySize);
		// printf("td.SampleDesc = %d\n", (int)td.SampleDesc);
		// printf("td.BindFlags = %d\n", td.BindFlags);
		// printf("td.MiscFlags = %d\n", td.MiscFlags); // D3D11_RESOURCE_MISC_SHARED

		// The application can now access and copy the sender texture
		return true;

	} // end find sender

	// There is no sender or the connected sender closed
	return false;

}


//---------------------------------------------------------
// Check whether SpoutPanel opened and return the new sender name
bool Spout::CheckSpoutPanel(char *sendername, int maxchars)
{
	// If SpoutPanel has been activated, test if the user has clicked OK
	if (m_bSpoutPanelOpened) { // User has activated spout panel
		SharedTextureInfo TextureInfo;
		HANDLE hMutex = NULL;
		DWORD dwExitCode;
		char newname[256];
		bool bRet = false;

		// Must find the mutex to signify that SpoutPanel has opened
		// and then wait for the mutex to close
		hMutex = OpenMutexA(MUTEX_ALL_ACCESS, 0, "SpoutPanel");

		// Has it been activated 
		if (!m_bSpoutPanelActive) {
			// If the mutex has been found, set the active flag true and quit
			// otherwise on the next round it will test for the mutex closed
			if (hMutex) m_bSpoutPanelActive = true;
		}
		else if (!hMutex) { // It has now closed
			m_bSpoutPanelOpened = false; // Don't do this part again
			m_bSpoutPanelActive = false;
			// call GetExitCodeProcess() with the hProcess member of
			// global SHELLEXECUTEINFO to get the exit code from SpoutPanel
			if (m_ShExecInfo.hProcess) {
				GetExitCodeProcess(m_ShExecInfo.hProcess, &dwExitCode);
				// Only act if exit code = 0 (OK)
				if (dwExitCode == 0) {
					// SpoutPanel has been activated and OK clicked
					// Test the active sender which should have been set by SpoutPanel
					newname[0] = 0;
					if (!sendernames.GetActiveSender(newname)) {
						// Otherwise the sender might not be registered.
						// SpoutPanel always writes the selected sender name to the registry.
						if (ReadPathFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutPanel", "Sendername", newname)) {
							// Register the sender if it exists
							if (newname[0] != 0) {
								if (sendernames.getSharedInfo(newname, &TextureInfo)) {
									// Register in the list of senders and make it the active sender
									sendernames.RegisterSenderName(newname);
									sendernames.SetActiveSender(newname);
								}
							}
						}
					}
					// Now do we have a valid sender name ?
					if (newname[0] != 0) {
						// Pass back the new name
						strcpy_s(sendername, maxchars, newname);
						bRet = true;
					} // endif valid sender name
				} // endif SpoutPanel OK
			} // got the exit code
		} // endif no mutex so SpoutPanel has closed
		// If we opened the mutex, close it now or it is never released
		if (hMutex) CloseHandle(hMutex);
		return bRet;
	} // SpoutPanel has not been opened


	return false;

}
