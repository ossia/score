// ================================================================
//
//		SpoutSDK
//
//		The Main Spout class - used by Sender and Receiver classes
//
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
//					- removed bMemoryShareInitOK becasue there is no single initialization any more
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
//		29.06.16	- Added ReportMemory() for debugging
//					- Changed OpenSpout to fail for DX9 if no hwnd
//					  https://github.com/leadedge/Spout2/issues/18
//		03.07.16	- Fix dwFormat repeat declaration in InitSender
//		15.01.17	- Add GetShareMode, SetShareMode
//		18.01.17	- GetImageSize redundant for 2.006
//		22.01.17	- include zero char in SelectSenderPanel NULL arg checks
//		25.05.17	- corrected SendImage UpdateSender to use passed width and height
//		31.10.17	- CreateReceiver update
//					  https://github.com/leadedge/Spout2/issues/24
//					  temporary changes to allow selection of a sender 
//					  when a name is provided for CreateReceiver
//
// ================================================================
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
#include "SpoutSDK.h"


Spout::Spout()
{

	/*
	// Debug console window
	FILE* pCout;
	AllocConsole();
	freopen_s(&pCout, "CONOUT$", "w", stdout); 
	printf("Spout::Spout()\n");
	*/

	// printf("Spout::Spout()\n");

	g_Width               = 0;
	g_Height              = 0;
	g_ShareHandle         = 0;
	g_Format              = 0;
	g_TexID               = 0;
	g_hWnd                = NULL;   // handle to render window
	g_SharedMemoryName[0] = 0;      // No name to start 
	bDxInitOK             = false;  // Initialized in texture share mode
	bGLDXcompatible       = false;  // Not used
	bUseCPU               = false;  // Use CPU texture processing
	bMemory               = false;  // User or compatibility memoryshare mode
	bInitialized          = false;  // Has initialized or not
	bIsSending            = false;  // A sender
	bIsReceiving          = false;  // A receiver
	bChangeRequested      = true;   // set for initial
	bUseActive            = false;  // Use the active sender for CreateReceiver
	
	bSpoutPanelOpened     = false;  // Selection panel "Spoutpanel.exe" opened
	bSpoutPanelActive     = false;  // The SpoutPanel window has been activated
	ZeroMemory(&m_ShExecInfo, sizeof(m_ShExecInfo));

}


//---------------------------------------------------------
Spout::~Spout()
{
	// Close the sender if it has not been done yet
	if(bInitialized && bIsSending && g_SharedMemoryName[0] > 0) {
		interop.senders.ReleaseSenderName(g_SharedMemoryName);
	}

	// This is the end, so cleanup and close directx or memoryshare
	SpoutCleanUp(true);

	// for debug
	// MessageBoxA(NULL, "~Spout Finished", "Spout", MB_OK);
	
}


// Public functions
bool Spout::CreateSender(const char* sendername, unsigned int width, unsigned int height, DWORD dwFormat)
{
	// printf("Spout::CreateSender [%s] (%dx%d)\n", sendername, width, height);

	// Make sure it has initialized
	// OpenSpout sets : bDxInitOK, bGLDXcompatible, bMemory, and bUseCPU
	if(!OpenSpout()) {
		printf("Spout::CreateSender - OpenSpout failed\n");
		return false;
	}

	// Release any orphaned senders
	// the name exists in the list but the shared memory info does not
	CleanSenders();

	// Set global sender name - TODO : check when
	strcpy_s(g_SharedMemoryName, 256, sendername);

	// Initialize as a sender in either texture, cpu or memoryshare mode
	return(InitSender(g_hWnd, sendername, width, height, dwFormat, bMemory));

} // end CreateSender


// ------------------------------------------
//	Update a sender
//	Used when a sender's texture changes size
//  The DirectX texture or memory map has to be re-created and the sender info updated
// ------------------------------------------
bool Spout::UpdateSender(const char *sendername, unsigned int width, unsigned int height)
{
	HANDLE hSharehandle = NULL;
	DWORD dwFormat = 0;
	unsigned int w, h;

	// Make sure it has initialized
	if(!bInitialized) return false;

	// If it is not the same sendername, quit
	if(strcmp(g_SharedMemoryName, sendername) != 0)
		return false;

	// printf("Spout::UpdateSender [%s] %dx%d\n", sendername, width, height);

	// Is the sender still there? - use local vars
	if(interop.senders.GetSenderInfo(sendername, w, h, hSharehandle, dwFormat)) {
		if(bDxInitOK) {
			// For texture and CPU modes, re-create the sender directX shared texture
			// with the new dimensions and update the sender info
			// No need to re-initialize DirectX, only the GLDX interop
			// which is re-registered for the new texture
			interop.CreateInterop(g_hWnd, sendername, width, height, dwFormat, false); // false means a sender
		}
		else {
			// Memoryshare has to update the sender information as well as the memory map size
			interop.senders.UpdateSender(sendername, width, height, NULL, 0);
			// Only the sender can update the memory map (see SpoutMemoryShare.cpp).
			interop.memoryshare.UpdateSenderMemorySize (sendername, width, height);
		}

		//
		// Get the new sender width, height and share handle into local globals
		//
		interop.senders.GetSenderInfo(g_SharedMemoryName, g_Width, g_Height, g_ShareHandle, g_Format);

		return true;
	}

	return false;
		
} // end UpdateSender



void Spout::ReleaseSender(DWORD dwMsec) 
{
	if(g_SharedMemoryName[0] > 0)
		interop.senders.ReleaseSenderName(g_SharedMemoryName); // if not registered it does not matter

	SpoutCleanUp();
	bInitialized = false; // TODO - needs tracing
	bIsSending = false;
	
	Sleep(dwMsec); // TODO - needed ?

}


// 27.07-14 - change logic to allow an optional user flag to use the active sender
bool Spout::CreateReceiver(char* sendername, unsigned int &width, unsigned int &height, bool bActive)
{

	char UserName[256];
	UserName[0] = 0; // OK to do this internally

	// Use the active sender if the user wants it or the sender name is not set
	if(bActive || sendername[0] == 0) {		
		bUseActive = true;
	}
	else {
		// Try to find the sender with the name sent or over-ride with user flag
		strcpy_s(UserName, 256, sendername);
		bUseActive = false; // set global flag to use the active sender or not
	}

	// printf("Spout::CreateReceiver(%s) %dx%d, bActive = %d\n", UserName, width, height, bActive);

	// Make sure it has been initialized
	// OpenReceiver	checks g_ShareHandle for NULL which indicates memoryshare sender
	// and also sets bGLDXcompatible, bDxInitOK, bUseCPU and bMemory
	if(OpenReceiver(UserName, width, height)) {
		strcpy_s(sendername, 256, UserName); // pass back the sendername used
		return true;
	}

	return false;
}


void Spout::ReleaseReceiver() 
{
	// can be done without a check here
	SpoutCleanUp();
	bInitialized = false; // TODO - needs tracing
	bIsReceiving = false;
	Sleep(100); // Debugging aid, but leave for safety
}


// If the local texure has changed dimensions this will return false
bool Spout::SendTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	// width, g_Width should all be the same
	// (the application resets the size of any texture that is being sent out)
	if(width != g_Width || height != g_Height) 
		return(UpdateSender(g_SharedMemoryName, width, height));
	else
		return(interop.WriteTexture(TextureID, TextureTarget, width, height, bInvert, HostFBO));

} // end SendTexture



// If the local texure has changed dimensions this will return false
bool Spout::SendImage(const unsigned char* pixels, 
					  unsigned int width, 
					  unsigned int height, 
					  GLenum glFormat, 
					  bool bInvert,
					  GLuint HostFBO)
{
	// bool bResult = true;
	GLenum glformat = glFormat;

	// printf("SendImage(%d, %d) - format = %x, invert = %d\n", width, height, glFormat, bInvert);

	// width, g_Width should all be the same
	if(width != g_Width || height != g_Height)
		return(UpdateSender(g_SharedMemoryName, width, height));

	// Only RGBA, BGRA, RGB, BGR supported
	if(!(glformat == GL_RGBA || glFormat == 0x80E1 || glformat == GL_RGB || glFormat == 0x80E0))
		return false;

	// Check for BGRA support
	if(!IsBGRAavailable()) {
		// If the bgra extensions are not available and the user
		// provided GL_BGR_EXT or GL_BGRA_EXT do not use them
		if(glFormat == 0x80E0) glformat = GL_RGB; // GL_BGR_EXT
		if(glFormat == 0x80E1) glformat = GL_RGBA; // GL_BGRA_EXT
	}

	// Write the pixel data to the rgba shared texture from the user pixel format
	return(interop.WriteTexturePixels(pixels, width, height, glformat, bInvert, HostFBO));

} // end SendImage


//
// ReceiveTexture
//
bool Spout::ReceiveTexture(char* name, 
						   unsigned int &width, 
						   unsigned int &height, 
						   GLuint TextureID, 
						   GLuint TextureTarget, 
						   bool bInvert, 
						   GLuint HostFBO)
{
	bool bConnected = true;
	// printf("Spout::ReceiveTexture(%s), %d, %d, [%x], [%x] (bInvert = %d)\n", name, width, height, TextureID, TextureTarget, bInvert);

	//
	// Test for sender change and user selection
	//
	// If not yet initialized, connects to the name provided or the active sender
	//		if connected sets bConnected to true
	//			the caller has to adjust any local textures etc.
	//		if not connected sets bConnected to false
	//		Returns false
	//
	// Calls CheckSpoutPanel to find if the user has selected another sender
	//
	// Checks that the sender identified by the global name is present - the size of that sender is returned
	//		If the global name or the sender width and height have changed they are returned to the caller.
	//		Sets bConnected to true if the sender is OK
	//			the caller has to detect the change and adjust any local textures etc.
	//		Sets bConnected to false if the sender is closed and the global sender name is reset
	//		Returns false
	//
	// Otherwise no changes and returns true
	//
	if(!CheckReceiver(name, width, height, bConnected))
		return bConnected;

	// Sender exists and everything matched.
	// Globals are now all current, so pass back the current name and size
	// so that there is no change found by the host.
	strcpy_s(name, 256, g_SharedMemoryName);
	width  = g_Width;
	height = g_Height;

	if(TextureID > 0 && TextureTarget > 0) {
		// If a valid texture was passed, read the shared texture into it.
		// Otherwise skip it. All the other checks for name and size are already done.
		return(interop.ReadTexture(TextureID, TextureTarget, g_Width, g_Height, bInvert, HostFBO));
	}
	else {
		// Just depend on the shared texture being updated and don't return one
		// e.g. can use DrawSharedTexture to use the shared texture directly
		// ReceiveTexture still does all the check for sender presence and size change etc.
		return true;
	}

} // end ReceiveTexture



bool Spout::ReceiveImage(char* name, 
						 unsigned int &width, 
						 unsigned int &height, 
						 unsigned char* pixels, 
						 GLenum glFormat,
						 bool bInvert, 
						 GLuint HostFBO)
{
	bool bConnected = true;
	GLenum glformat = glFormat;

	// printf("Spout::ReceiveImage (%dx%d) - format = %x\n", width, height, glFormat);

	// Only RGBA, BGRA, RGB and BGR supported
	if(!(glformat == GL_RGBA || glFormat == 0x80E1  || glFormat == GL_RGB || glFormat == 0x80E0))
		return false;

	// Check for BGRA support
	if(!IsBGRAavailable()) {
		// If the bgra extensions are not available and the user
		// provided GL_BGR_EXT or GL_BGRA_EXT do not use them
		if(glFormat == 0x80E0) glformat = GL_RGB; // GL_BGR_EXT
		if(glFormat == 0x80E1) glformat = GL_RGBA; // GL_BGRA_EXT
	}

	// Test for sender change and user selection
	if(!CheckReceiver(name, width, height, bConnected))
		return bConnected;

	// globals are all current, so pass back the current name and size
	strcpy_s(name, 256, g_SharedMemoryName);
	width  = g_Width;
	height = g_Height;

	// Read the shared texture into the pixel buffer
	// Functions handle the formats supported
	return(interop.ReadTexturePixels(pixels, width, height, glformat, bInvert, HostFBO));

}  // end ReceiveImage



//
// CheckReceiver
//
// If not yet inititalized, conects to the name provided or the active sender
//		if connected sets bConnected to true
//		if not connected sets bConnected to false
//		returns false
//
// Calls CheckSpoutPanel to find if the user has selected another sender
//		If so, changes globals g_SharedMemoryName, g_Width, g_Height, g_Format.
//		and the sender name will be different to that passed 
//		If not, no changes to global name and size
//
// Checks that the sender identified by the global name is present
//		the size of that sender is returned
//		If the global name or the sender width and height have changed they are returned to the caller.
//		Sets bConnected to true if the sender is OK
//			the caller has to detect the change and adjust any local textures etc.
//		Sets bConnected to false if the sender is closed and the global sender name is reset
//		Returns false
//
// Otherwise drops through and returns true
//
bool Spout::CheckReceiver(char* name, unsigned int &width, unsigned int &height, bool &bConnected)
{
	char newname[256];
	unsigned int newWidth, newHeight;
	DWORD dwFormat;
	HANDLE hShareHandle;	
	
	// Has it initialized yet ?
	if(!bInitialized) {
		// The name passed is the name to try to connect to unless the bUseActive flag is set
		// or the name is not initialized in which case it will try to find the active sender
		// Width and height are passed back as well
		if(name[0] != 0)
			strcpy_s(newname, 256, name);
		else
			newname[0] = 0;

		if(OpenReceiver(newname, newWidth, newHeight)) {
			// OpenReceiver will also set the global name, width, height and format
			// Pass back the new name, width and height to the caller
			// The change has to be detected by the application
			strcpy_s(name, 256, newname);
			width  = newWidth;
			height = newHeight;
			bConnected = true; // user needs to check
			return false;
		}
		else {
			// Initialization failure - the sender is not there 
			// Quit to let the app try again
			bConnected = false;
			return false;
		}
	} // endif not initialized


	// Check to see if SpoutPanel has been opened
	// If it has been opened, the globals are reset
	// (g_SharedMemoryName, g_Width, g_Height, g_Format)
	// and the sender name will be different to that passed 
	CheckSpoutPanel();

	// Set initial values to current globals to check for change with those passed in
	strcpy_s(newname, 256, g_SharedMemoryName);
	newWidth = g_Width; // width;
	newHeight = g_Height; // height;
	hShareHandle = g_ShareHandle;
	dwFormat = g_Format;

	// Is the sender there ?
	if(interop.senders.CheckSender(newname, newWidth, newHeight, hShareHandle, dwFormat)) {
		// The sender exists, but has the width, height, texture format changed from those passed in
		if(newWidth > 0 && newHeight > 0) {
			if(newWidth  != width
			|| newHeight != height
			|| dwFormat  != g_Format
			|| strcmp(name, g_SharedMemoryName) != 0 ) { // test of original name allows for CheckSpoutPanel above
				// Re-initialize the receiver
				// OpenReceiver will also set the global name, width, height and format
				if(OpenReceiver(g_SharedMemoryName, newWidth, newHeight)) {				
					g_Width = newWidth;
					g_Height = newHeight;
					g_ShareHandle = hShareHandle; // 09.09.15
					g_Format = dwFormat; // 09.09.15
					// Return the new sender name and dimensions
					// The change has to be detected by the application
					strcpy_s(name, 256, g_SharedMemoryName);
					width  = g_Width;
					height = g_Height;
					bConnected = true; // user needs to check for changes
					return false;
				} // OpenReceiver OK
				else {
					// need what here
					bConnected = false;
					return false;
				}
			} // width, height, format or name have changed
			// The sender exists and there are no changes
			// Drop through to return true
		} // width and height are zero
		else {
			// need what here
			bConnected = false;
			return false;
		}
	} // endif CheckSender found a sender
	else {
		g_SharedMemoryName[0] = 0; // sender no longer exists
		// 01.06.15 - safety
		ReleaseReceiver(); // Start again
		bConnected = false;
		return false;
	} // CheckSender did not find the sender - probably closed

	// The sender exists and there are no changes
	bConnected = true;
	return true;

}


// Can be used without OpenGL context 
// Use before OpenReceiver and should not be called repeatedly
// Redundant for Spout 2.006 - may be removed for future releases
// Spout::GetSenderInfo is equivalent
bool Spout::GetImageSize(char* name, unsigned int &width, unsigned int &height, bool &bMemoryMode)
{
	char newname[256];
	SharedTextureInfo TextureInfo;

	// Was initialized so get the sender details
	// Test to see whether the current sender is still there
	if(!interop.getSharedInfo(name, &TextureInfo)) {
		// Try the active sender
		if(interop.senders.GetActiveSender(newname)) {
			if(interop.getSharedInfo(newname, &TextureInfo)) {
				// Pass back the new name and size
				strcpy_s(name, 256, newname);
				width  = TextureInfo.width;
				height = TextureInfo.height;
				// Check the sharehandle - if it is null, the sender is memoryshare
				if(TextureInfo.shareHandle == NULL)
					bMemoryMode = true;
				else
					bMemoryMode = false;
				return true;
			}
		}
	} // sender was running

	return false;

} // end GetImageSize


//---------------------------------------------------------
bool Spout::BindSharedTexture()
{
	return(interop.BindSharedTexture());
}


//---------------------------------------------------------
bool Spout::UnBindSharedTexture()
{
	return(interop.UnBindSharedTexture());
}


//---------------------------------------------------------
bool Spout::DrawSharedTexture(float max_x, float max_y, float aspect, bool bInvert, GLuint HostFBO)
{
	return(interop.DrawSharedTexture(max_x, max_y, aspect, bInvert, HostFBO));
}



//---------------------------------------------------------
// 
bool Spout::DrawToSharedTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, float max_x, float max_y, float aspect, bool bInvert, GLuint HostFBO)
{
	// Allow for change of sender size, even though the draw is independent of the 
	// shared texture size, otherwise receivers will get a constant size for this sender
	if(!bMemory) {
		// width, g_Width should all be the same
		// width and height are the size of the texture that is being drawn to.
		if(width != g_Width || height != g_Height) {
			return(UpdateSender(g_SharedMemoryName, width, height));
		}
	}
	return(interop.DrawToSharedTexture(TextureID, TextureTarget, width, height, max_x, max_y, aspect, bInvert, HostFBO));

}


//---------------------------------------------------------
bool Spout::SetCPUmode(bool bCPU)
{
	return (interop.SetCPUmode(bCPU));
}

//---------------------------------------------------------
bool Spout::GetCPUmode()
{
	return (interop.GetCPUmode());
}

//---------------------------------------------------------
bool Spout::SetMemoryShareMode(bool bMem)
{
	return (interop.SetMemoryShareMode(bMem));
}

//---------------------------------------------------------
bool Spout::GetMemoryShareMode()
{
	// Gets interop class global memoryshare flag and sets a flag in this class
	bMemory = interop.GetMemoryShareMode(); // set global flag - TODO : rename globals
	return bMemory;
}

//---------------------------------------------------------
int Spout::GetShareMode()
{
	return interop.GetShareMode();
}

//---------------------------------------------------------
bool Spout::SetShareMode(int mode)
{
	return (interop.SetShareMode(mode));
}

//
// Maximum sender functions - for development testing only
//
int Spout::GetMaxSenders()
{
	//
	// Gets the maximum senders allowed from the sendernames class
	//
	return(interop.senders.GetMaxSenders());
}

void Spout::SetMaxSenders(int maxSenders)
{
	//
	// Sets the maximum senders allowed
	//
	interop.senders.SetMaxSenders(maxSenders);
}

// Get the global sender name for this instance
bool Spout::GetSpoutSenderName(char * sendername, int maxchars)
{
	if(g_SharedMemoryName && g_SharedMemoryName[0] > 0) {
		strcpy_s(sendername, maxchars, g_SharedMemoryName);
		return true;
	}
	else {
		return false;
	}
}

// has the class been initialized
bool Spout::IsSpoutInitialized()
{
	return bInitialized;

}

// Are BGRA extensions supported
bool Spout::IsBGRAavailable()
{
	return interop.IsBGRAavailable();

}

// Are PBO extensions supported
bool Spout::IsPBOavailable()
{
	return interop.IsPBOavailable();

}

// Switch pbo functions on or off (default is off).
void Spout::SetBufferMode(bool bActive)
{
	interop.SetBufferMode(bActive);
}

bool Spout::GetBufferMode()
{
	return interop.GetBufferMode();
}


// SelectSenderPanel - used by a receiver
// Optional message argument
bool Spout::SelectSenderPanel(const char *message)
{
	HANDLE hMutex1;
	HMODULE module;
	char UserMessage[512];
	char path[MAX_PATH], drive[MAX_PATH], dir[MAX_PATH], fname[MAX_PATH];

	if(message != NULL && message[0] != 0)
		strcpy_s(UserMessage, 512, message); // could be an arg or a user message
	else
		UserMessage[0] = 0; // make sure SpoutPanel does not see an un-initialized string

	// For a texture share receiver pop up SpoutPanel to allow the user to select a sender
	// The selected sender is then the "Active" sender and this receiver switches to it.
	// SpoutPanel.exe has to be in the same folder as this executable
	// This rather complicated process avoids having to use a dialog within a dll
	// which causes problems with FreeFrameGL plugins and Max eternals

	// First check whether the panel is already running
	// Try to open the application mutex.
	hMutex1 = OpenMutexA(MUTEX_ALL_ACCESS, 0, "SpoutPanel");
	if (!hMutex1) {
		// No mutex, so not running, so can open it
		// See if there has been a Spout installation >= 2.002 with an install path for SpoutPanel.exe
		if(!ReadPathFromRegistry(path, "Software\\Leading Edge\\SpoutPanel", "InstallPath")) {
			// Path not registered so find the path of the host program
			// where SpoutPanel should have been copied
			module = GetModuleHandle(NULL);
			GetModuleFileNameA(module, path, MAX_PATH);
			_splitpath_s(path, drive, MAX_PATH, dir, MAX_PATH, fname, MAX_PATH, NULL, 0);
			_makepath_s(path, MAX_PATH, drive, dir, "SpoutPanel", ".exe");
			// Does SpoutPanel.exe exist in this path ?
			if(!PathFileExistsA(path) ) {
				// Try the current working directory
				if(_getcwd(path, MAX_PATH)) {
					strcat_s(path, MAX_PATH, "\\SpoutPanel.exe");
					// printf("SpoutPanel cwd [%s]\n", path);
					// Does SpoutPanel exist here?
					if(!PathFileExistsA(path) ) {
						return false;
					}
				}
			}
		}

		// printf("SpoutPanel path [%s]\n", path); // Spoutpanel exists

		// 
		// Use  ShellExecuteEx so we can test its return value later
		//
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

		Sleep(125); // allow time for SpoutPanel to open 0.125s

		// Returns straight away here but multiple instances of SpoutPanel
		// are prevented in it's WinMain procedure by the mutex.
		// An infinite wait here causes problems.
		// The flag "bSpoutPanelOpened" is set here to indicate that the user
		// has opened the panel to select a sender. This flag is local to 
		// this process so will not affect any other receiver instance
		// Then when the selection panel closes, sender name is tested
		bSpoutPanelOpened = true;
	}
	else {
		// We opened it so close it, otherwise it is never released
		CloseHandle(hMutex1);
	}

	// The mutex exists, so another instance is already running
	// Find the dialog window and bring it to the top
	// the spout dll dialog is opened as topmost anyway but pop it to
	// the front in case anything else has stolen topmost
	HWND hWnd = FindWindowA(NULL, (LPCSTR)"SpoutPanel");
	if(IsWindow(hWnd)) {
		SetForegroundWindow(hWnd); 
		// prevent other windows from hiding the dialog
		// and open the window wherever the user clicked
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_ASYNCWINDOWPOS | SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
	}

	return true;

} // end selectSenderPanel


// 22.02.15 - find the SpoutPanel version
// http://stackoverflow.com/questions/940707/how-do-i-programatically-get-the-version-of-a-dll-or-exe-file
//
bool Spout::FindFileVersion(const char *FilePath, DWORD &versMS, DWORD &versLS)
{
    DWORD               dwSize              = 0;
    unsigned char       *pbVersionInfo      = NULL;
    VS_FIXEDFILEINFO    *pFileInfo          = NULL;
    UINT                puLenFileInfo       = 0;

    // get the version info for the file requested
    dwSize = GetFileVersionInfoSizeA(FilePath, NULL );
    if ( dwSize == 0 ) {
        printf("Error in GetFileVersionInfoSize: %d\n", GetLastError() );
        return false;
    }

    pbVersionInfo = new BYTE[ dwSize ];

    if ( !GetFileVersionInfoA( FilePath, 0, dwSize, pbVersionInfo ) )  {
        printf("Error in GetFileVersionInfo: %d\n", GetLastError() );
        delete[] pbVersionInfo;
        return false;
    }

    if ( !VerQueryValueA( pbVersionInfo, "\\", (LPVOID*) &pFileInfo, &puLenFileInfo ) ) {
        printf("Error in VerQueryValue: %d\n", GetLastError() );
        delete[] pbVersionInfo;
        return false;
    }

	versMS = pFileInfo->dwFileVersionMS;
	versLS = pFileInfo->dwFileVersionLS;

    /*
	printf("File Version: %d.%d.%d.%d\n",
		( pFileInfo->dwFileVersionMS >> 16 ) & 0xffff,
        ( pFileInfo->dwFileVersionMS >>  0 ) & 0xffff,
        ( pFileInfo->dwFileVersionLS >> 16 ) & 0xffff,
        ( pFileInfo->dwFileVersionLS >>  0 ) & 0xffff
        );

    printf("Product Version: %d.%d.%d.%d\n",
        ( pFileInfo->dwProductVersionMS >> 24 ) & 0xffff,
        ( pFileInfo->dwProductVersionMS >> 16 ) & 0xffff,
        ( pFileInfo->dwProductVersionLS >>  8 ) & 0xffff,
        ( pFileInfo->dwProductVersionLS >>  0 ) & 0xffff
        );
	*/

	return true;

}
// ======================



int Spout::GetSenderCount() {
	std::set<string> SenderNameSet;
	if(interop.senders.GetSenderNames(&SenderNameSet)) {
		return((int)SenderNameSet.size());
	}
	return 0;
}


//
// Get a sender name given an index and knowing the sender count
// index             - in
// sendername        - out
// sendernameMaxSize - in
bool Spout::GetSenderName(int index, char* sendername, int sendernameMaxSize)
{
	std::set<string> SenderNameSet;
	std::set<string>::iterator iter;
	string namestring;
	char name[256];
	int i;

	if(interop.senders.GetSenderNames(&SenderNameSet)) {
		if(SenderNameSet.size() < (unsigned int)index) {
			return false;
		}
		i = 0;
		for(iter = SenderNameSet.begin(); iter != SenderNameSet.end(); iter++) {
			namestring = *iter; // the name string
			strcpy_s(name, 256, namestring.c_str()); // the 256 byte name char array
			if(i == index) {
				strcpy_s(sendername, sendernameMaxSize, name); // the passed name char array
				break;
			}
			i++;
		}
		return true;
	}
	return false;
}


// All of these can be directly in the Receiver class . TODO - Change/Test
//---------------------------------------------------------
bool Spout::GetActiveSender(char* Sendername)
{
	return interop.senders.GetActiveSender(Sendername);
}


//---------------------------------------------------------
bool Spout::SetActiveSender(const char* Sendername)
{
	return interop.senders.SetActiveSender(Sendername);
}


bool Spout::GetSenderInfo(const char* sendername, unsigned int &width, unsigned int &height, HANDLE &dxShareHandle, DWORD &dwFormat)
{
	return interop.senders.GetSenderInfo(sendername, width, height, dxShareHandle, dwFormat);
}



int Spout::GetVerticalSync()
{
	return interop.GetVerticalSync();
}


bool Spout::SetVerticalSync(bool bSync)
{
	return interop.SetVerticalSync(bSync);
}



// ========================================================== //
//                      LOCAL FUNCTIONS                       //
// ========================================================== //
bool Spout::OpenReceiver (char* theName, unsigned int& theWidth, unsigned int& theHeight)
{
	char Sendername[256]; // user entered Sender name
	DWORD dwFormat = 0;
	HANDLE sharehandle = NULL;
	unsigned int width;
	unsigned int height;

	// printf("OpenReceiver (%s, %d , %d) - bUseActive = %d\n", theName, theWidth, theHeight, bUseActive);

	// If the name begins with a null character, or the bUseActive flag has been set
	if(theName[0] != 0 && !bUseActive) { // A valid name is sent and the user does not want to use the active sender
		strcpy_s(Sendername, 256, theName);
	}
	else {
		Sendername[0] = 0;
	}

	// Set initial size to that passed in
	width  = theWidth;
	height = theHeight;

	// Find if the sender exists
	// Or, if a null name given, return the active sender if that exists
	if(!interop.senders.FindSender(Sendername, width, height, sharehandle, dwFormat)) {
		// Given name not found ? - has SpoutPanel been opened ?
		// the globals are reset if it has been
		if(CheckSpoutPanel()) {
			// set vars for below
			strcpy_s(Sendername, 256, g_SharedMemoryName);
			width    = g_Width;
			height   = g_Height;
			dwFormat = g_Format;
			// 31.10.17
			sharehandle = g_ShareHandle;
		}
		else {
		    return false;
		}
	}

	// Make sure it has been initialized
	// OpenSpout sets bDxInitOK and bMemory if not compatible
	if(!OpenSpout()) {
		return false;
	}

	// Texture mode - sharehandle must not be NULL
	if(!bMemory && !sharehandle)
		return false;

	g_ShareHandle = sharehandle;

	if(bDxInitOK) {

		// Render window must be visible for initSharing to work
		// Safety in case no opengl context
		if(wglGetCurrentContext() == NULL || wglGetCurrentDC() == NULL) {
			return false;
		}
	
		g_hWnd = WindowFromDC(wglGetCurrentDC()); 
		// Suggested : https://github.com/leadedge/Spout2/issues/18
		// if(g_hWnd == NULL && interop.m_bUseDX9) {
		if(g_hWnd == NULL) {
			return false;
		}

	}

	// Set the global name, width, height and format
	strcpy_s(g_SharedMemoryName, 256, Sendername);
	g_Width  = width;
	g_Height = height;
	g_Format = dwFormat;

	// Initialize a receiver in either memoryshare or texture mode
	// Use the global memory mode flag
	if(InitReceiver(g_hWnd, g_SharedMemoryName, g_Width, g_Height, bMemory)) {
		// InitReceiver can reset the globals so pass them back
		strcpy_s(theName, 256, g_SharedMemoryName);
		theWidth  = g_Width;
		theHeight = g_Height;
		return true;
	}

	return false;

} // end OpenReceiver



void Spout::CleanSenders()
{
	char name[512];
	std::set<std::string> Senders;
	std::set<std::string>::iterator iter;
	std::string namestring;
	SharedTextureInfo info;

	// MessageBoxA(NULL,"Spout::CleanSenders()","ERROR",MB_OK|MB_ICONEXCLAMATION);

	// get the sender name list in shared memory into a local list
	interop.senders.GetSenderNames(&Senders);

	// Now we have a local set of names "Senders"
	// 27.12.13 - noted that if a Processing sketch is stopped by closing the window
	// all is OK and either the "stop" or "dispose" overrides work, but if STOP is used, 
	// or the sketch is closed, neither the exit or dispose functions are called and
	// the sketch does not release the sender.
	// So here we run through again and check whether the sender exists and if it does not
	// release the sender from the local sender list
	if(Senders.size() > 0) {
		for(iter = Senders.begin(); iter != Senders.end(); iter++) {
			namestring = *iter; // the Sender name string
			strcpy_s(name, namestring.c_str());
			// we have the name already, so look for it's info
			if(!interop.senders.getSharedInfo(name, &info)) {
				// Sender does not exist any more
				interop.senders.ReleaseSenderName(name); // release from the shared memory list
			}
		}
	}

	// Now we have cleaned up the list in shared memory
	Senders.clear();

}


bool Spout::InitSender (HWND hwnd, const char* theSendername, 
						unsigned int theWidth, unsigned int theHeight, 
						DWORD theFormat, bool bMemoryMode) 
{
	char sendername[256];

	// printf("Spout::Initsender [%s] (%dx%d) (bGLDXcompatible = %d, memorymode = %d)\n", theSendername, theWidth, theHeight, bGLDXcompatible, bMemoryMode);

	// Quit if there is no image size to initialize with
	if(theWidth == 0 || theHeight == 0) {
		MessageBoxA(NULL,"Cannot initialize sender with zero size.","ERROR",MB_OK|MB_ICONEXCLAMATION);
		return false;
	}

	// Does the sender already exist ?
	int i = 1;
	strcpy_s(sendername, 256, theSendername);
	if(interop.senders.FindSenderName(sendername)) {
		do {
			sprintf_s(sendername, 256, "%s_%d", theSendername, i);
			i++;
		} while (interop.senders.FindSenderName(sendername));
	}

	// only try dx if the memory mode flag is not set
	if(!bMemoryMode) {
		// Initialize the GL/DX interop and create a new shared texture (false = sender)
		if(!interop.CreateInterop(hwnd, sendername, theWidth, theHeight, theFormat, false)) {  // False for a sender
			printf("Spout::InitSender error 2\n");
			return false;
		}
		bDxInitOK = true;
		bMemory	= false;
	}
	else {

		//
		// Memoryshare mode
		//

		// If there is an OpenGL context, load the extensions now so that the fbo extensions work
		// From 2.005, OpenGL is used for Memoryshare as well, so if extensions fail to load,
		// nothing will not work anyway, so quit now. TODO : trace global flags for memoryshare
		if(!wglGetCurrentContext())
			return false;

		if(!interop.LoadGLextensions())
			return false;

		//
		// LJ DEBUG - temporary patch
		//
		// To prevent a crash with a 2.004 receiver, create an empty DirectX texture
		//
		// DirectX will be released by CleaunpInterop.
		// This is just a prevention in case with 2.005, memoryshare is set by the user or the
		// hardware is incompatible and then the user starts up an app that has been developed
		// with the Spout 2.004 SDK. Then the result will be black as it would have been anyway
		// until the mode is set back to texture share.
		//
		// This patch can be removed whan all apps convert to 2.005 or later.
		//

		HDC hdc;
		hdc = wglGetCurrentDC(); // OpenGl device context is needed
		if(!hdc) return false;
		g_hWnd = WindowFromDC(hdc);

		// OpenDirectX does not set any initialization flags for Texture or Memory share
		if(!interop.OpenDirectX(g_hWnd, GetDX9())) {
			return false;
		}

		// Now we have created the DirectX device so create an empty texture
		interop.m_dxShareHandle = NULL; // A sender creates a new texture with a new share handle
		DWORD dwFormat = 0;
		if(interop.GetDX9()) {
			dwFormat = (DWORD)D3DFMT_A8R8G8B8;
			if(!interop.spoutdx.CreateSharedDX9Texture(interop.m_pDevice,
													   theWidth,
													   theHeight,
													   D3DFMT_A8R8G8B8,
													   interop.m_dxTexture,
													   interop.m_dxShareHandle)) {
				return false;
			}
		}
		else {
			dwFormat = (DWORD)DXGI_FORMAT_B8G8R8A8_UNORM;
			if(!interop.spoutdx.CreateSharedDX11Texture(interop.g_pd3dDevice,
														theWidth, theHeight, 
														DXGI_FORMAT_B8G8R8A8_UNORM,
														&interop.g_pSharedTexture,
														interop.m_dxShareHandle)) {
				return false;
			}
		}

		// Now create a sender with a valid texture handle and format
		// For a 2.004 receiver the result will just be black
		// Memoryshare needs to create a sender separately
		if(!interop.senders.CreateSender(sendername, theWidth, theHeight, interop.m_dxShareHandle, dwFormat))
			return false;

		if(!interop.memoryshare.CreateSenderMemory(sendername, theWidth, theHeight))
			return false;
		
		bDxInitOK = false;
		bMemory = true;

	}

	// Set global name
	strcpy_s(g_SharedMemoryName, 256, sendername);
				
	// Get the sender width, height and share handle into local copy
	interop.senders.GetSenderInfo(g_SharedMemoryName, g_Width, g_Height, g_ShareHandle, g_Format);

	bInitialized = true;
	bIsSending   = true;

	return true;


} // end InitSender


bool Spout::InitReceiver (HWND hwnd, char* theSendername, unsigned int theWidth, unsigned int theHeight, bool bMemoryMode) 
{

	char sendername[256];
	unsigned int width = 0;
	unsigned int height = 0;
	DWORD format = 0;
	HANDLE sharehandle = NULL;

	UNREFERENCED_PARAMETER(bMemoryMode);

	// printf("InitReceiver (%s, %d, %d) bGLDXcompatible = %d, bMemoryMode = %d\n", theSendername, theWidth, theHeight, bGLDXcompatible, bMemoryMode);
	// Quit if there is no image size to initialize with
	if(theWidth == 0 || theHeight == 0)
		return false;

	//
	// ============== Set up for a RECEIVER ============
	//
	if(theSendername[0] != 0) {
		strcpy_s(sendername, 256, theSendername); // working name local to this function
	}
	else {
		sendername[0] = 0;
	}


	// bChangeRequested is set when the Sender name, image size or share handle changes
	// or the user selects another Sender - everything has to be reset if already initialized
	if(bChangeRequested) {
		SpoutCleanUp();
		bDxInitOK        = false;
		// bMemory is Registry or user setting - do not touch it
		bInitialized     = false;
		bChangeRequested = false; // only do it once
	}

	// Find the requested sender and return the name, width, height, sharehandle and format
	if(!interop.senders.FindSender(sendername, width, height, sharehandle, format)) {
		return false;
	}

	// only try dx if the memory mode flag is not set and sharehandle is not NULL
	if(!bMemory && sharehandle) {
		// Initialize the receiver interop (this will create globals local to the interop class)
		if(!interop.CreateInterop(hwnd, sendername, width, height, format, true)) // true meaning receiver
			return false;
		bDxInitOK = true;
	}
	else {
		// If there is an OpenGL context, load the extensions now
		// so that the fbo extensions work (See InitSender)
		if(!wglGetCurrentContext())
			return false;

		if(!interop.LoadGLextensions())
			return false;

		if(!interop.memoryshare.CreateSenderMemory(sendername, width, height))
			return false;

		bDxInitOK = false;
	}

	// Set globals here
	g_Width       = width;
	g_Height      = height;
	g_ShareHandle = sharehandle;
	g_Format      = format;
	strcpy_s(g_SharedMemoryName, 256, sendername);

	bInitialized = true;
	bIsReceiving = true;

	return true;

} // end InitReceiver


//
// SpoutCleanup
//
void Spout::SpoutCleanUp(bool bExit)
{
	// LJ DEBUG - should be OK for memoryshare because all handles will be NULL 
	// This allows a dummy shared texture to be created for memoryshare to prevent
	// a crash with 2.004 receivers.
	// if(bDxInitOK) 
	// printf("Spout::SpoutCleanUp\n");
	interop.CleanupInterop(bExit); // true means it is the exit so don't call wglDXUnregisterObjectNV
	bDxInitOK = false;

	// 04.11.15 - Close memoryshare if created for data transfer
	// Has no effect if not created
	interop.memoryshare.CloseSenderMemory();
	if(bMemory) interop.memoryshare.ReleaseSenderMemory(); // destroys sendermem object
	// bMemory - Registry or user setting - do not change it
	g_ShareHandle = NULL;
	g_Width	= 0;
	g_Height= 0;
	g_Format = 0;

	// important - we no longer want the global shared memory name and need to reset it
	g_SharedMemoryName[0] = 0; 

	// Set default for CreateReceiver
	bUseActive = false;

	// Important - everything is reset (see ReceiveTexture)
	bInitialized = false;
	bIsSending = false;
	bIsReceiving = false;

}


//
// ========= USER SELECTION PANEL TEST =====
//
//	This is necessary because the exit code needs to be tested
//
bool Spout::CheckSpoutPanel()
{
	// MessageBoxA(NULL, "CheckSpoutPanel()", "SpoutSDK", MB_OK);

	// If SpoutPanel has been activated, test if the user has clicked OK
	if(bSpoutPanelOpened) { // User has activated spout panel

		SharedTextureInfo TextureInfo;
		HANDLE hMutex = NULL;
		DWORD dwExitCode;
		char newname[256];
		char activename[256];
		bool bRet = false;
		
		// Must find the mutex to signify that SpoutPanel has opened
		// and then wait for the mutex to close
		hMutex = OpenMutexA(MUTEX_ALL_ACCESS, 0, "SpoutPanel");

		// Has it been activated 
		if(!bSpoutPanelActive) {
			// If the mutex has been found, set the active flag true and quit
			// otherwise on the next round it will test for the mutex closed
			if(hMutex) bSpoutPanelActive = true;
		}
		else if (!hMutex) { // It has now closed
			bSpoutPanelOpened = false; // Don't do this part again
			bSpoutPanelActive = false;
			
			// call GetExitCodeProcess() with the hProcess member of SHELLEXECUTEINFO
			// to get the exit code from SpoutPanel
			if(m_ShExecInfo.hProcess) {
				GetExitCodeProcess(m_ShExecInfo.hProcess, &dwExitCode);
				// Only act if exit code = 0 (OK)
				if(dwExitCode == 0) {
					//
					// SpoutPanel has been activated and OK clicked
					//
					// Sender name entry
					//
					// Check for an unregistered sender first because this will not have been set as active yet
					// Try to get the current sender name from the registry (24.05.15 instead of text file)
					// Text file method does not work if SpoutPanel is in the Program Files folder without Admin privileges
					// SpoutPanel now always writes the selected sender name to the registry
					// so this first check should always work
					newname[0] = 0;
					if(!ReadPathFromRegistry(newname, "Software\\Leading Edge\\SpoutPanel", "Sendername")) {
						// Otherwise try the text file method
						string line;
						HMODULE module;
						char path[MAX_PATH], drive[MAX_PATH], dir[MAX_PATH], fname[MAX_PATH];
					
						// Find the path of the host program where SpoutPanel should have been copied
						module = GetModuleHandle(NULL);
						GetModuleFileNameA(module, path, MAX_PATH);
						_splitpath_s(path, drive, MAX_PATH, dir, MAX_PATH, fname, MAX_PATH, NULL, 0);
						_makepath_s(path, MAX_PATH, drive, dir, "Spoutpanel", ".txt");
						
						ifstream infile(path, ios::in);
						if (infile.is_open()) {
							if(getline(infile, line)) {
								strcpy_s(newname, 256, line.c_str());
							}
							infile.close();
							remove(path);
						}

						// 24.11.15
						// Does the sender exist - if so register it
						if(newname[0] != 0) {
							if(interop.senders.getSharedInfo(newname, &TextureInfo)) {
								// Register in the list of senders and make it the active sender
								interop.senders.RegisterSenderName(newname);
								interop.senders.SetActiveSender(newname);
							}
						}
					}

					// Do we have a sender name from the registry or a text file ?
					if(newname[0] != 0) {

						// Here we can test the active sender which should have been set by SpoutPanel
						// instead of depending on the registry flush which might have returned the old name
						// They should both be the same - so most reliable might be the active sender
						if(interop.senders.GetActiveSender(activename)) {
							if(strcmp(activename, newname) != 0) { // different names
									strcpy_s(newname, activename); // use the acitev sender
							}
						}
						// Does the sender exist ?
						if(interop.senders.getSharedInfo(newname, &TextureInfo)) {
							strcpy_s(g_SharedMemoryName, 256, newname);
							g_Width  = (unsigned int)TextureInfo.width;
							g_Height = (unsigned int)TextureInfo.height;
							g_Format = TextureInfo.format;
							// 31.10.17
							g_ShareHandle = (HANDLE)TextureInfo.shareHandle;
							// 24.11.15 - not needed if the sender exists - and it is already checked as active
							// Register in the list of senders and make it the active sender
							// interop.senders.RegisterSenderName(newname);
							// interop.senders.SetActiveSender(newname);
							bRet = true; // will pass on next call to receivetexture
						}
					}
					else {
						// No name in registry or text file, so get the active sender which is set by spoutpanel
						if(interop.senders.GetActiveSender(newname)) { // returns the active sender name
							if(interop.getSharedInfo(newname, &TextureInfo)) {
								strcpy_s(g_SharedMemoryName, 256, newname);
								g_Width  = (unsigned int)TextureInfo.width;
								g_Height = (unsigned int)TextureInfo.height;
								g_Format = TextureInfo.format;
								// 31.10.17
								g_ShareHandle = (HANDLE)TextureInfo.shareHandle;
								bRet = true; // will pass on next call to receivetexture
							}
						} // no active sender
					} // no active sender or unregistered sender
				} // endif SpoutPanel OK
			} // got the exit code
		} // endif no mutex so SpoutPanel has closed
		CloseHandle(hMutex);
		return bRet;
	} // SpoutPanel has not been opened

	return false;

} // ========= END USER SELECTION PANEL =====



bool Spout::OpenSpout()
{
	HDC hdc;

	// From 2.005 OpenGL is used for Memoryshare as well, so load the extensions.
	// If extensions fail to load, FBO extensions are not available and nothing
	// will not work anyway, so quit now
	if(!interop.LoadGLextensions())	{
		// printf("OpenSpout : Extensions not loaded\n");
		return false;
	}

	// LoadGLextensions has a check for availabilty of the GL/DX extensions
	// and switches to memoryshare mode if not supported

	// Retrieve memoryshare mode from interop class
	// This reads the user setting from the registry and
	// if GLDX extensions are available and makes an additional
	// compatibility test.
	// bMemory will be false for < 2.005
	bool bMemoryShare = interop.GetMemoryShareMode();

	// printf("OpenSpout - bGLDXcompatible = %d, bDxInitOK = %d, bMemoryShare = %d\n", bGLDXcompatible, bDxInitOK, bMemoryShare);

	// Safety return if already initialized
	if( (bDxInitOK && !bMemory) || (bMemory && !bDxInitOK) ) {
		// printf("OpenSpout : already initialized\n");
		return true;
	}

	if(bMemoryShare) {
		// Memoryshare was user set, so return to use shared memory
		bDxInitOK = false;
		bMemory = true;
	}
	else {
		// If not memoryshare, initialize DirectX and prepare GLDX interop
		hdc = wglGetCurrentDC(); // OpenGl device context is needed
		if(!hdc) {
			MessageBoxA(NULL, "Cannot get GL device context", "OpenSpout", MB_OK);
			return false;
		}
	
		g_hWnd = WindowFromDC(hdc); // can be null for DX11
		if(g_hWnd == NULL && GetDX9()) {
			//
			// DX9 device creation needs hwnd
			// https://github.com/leadedge/Spout2/issues/18
			//
			MessageBoxA(NULL, "Cannot get hwnd to create DirectX 9 device", "OpenSpout", MB_OK);
			return false;
		}

		if(!interop.OpenDirectX(g_hWnd, GetDX9())) { // did the NVIDIA open interop extension work ?
			bDxInitOK = false; // DirectX initialization failed
			bMemory = true; // Default to memoryshare
		}
	}

	return true;

}


// This is a request from within a program and Spout might not have initialized yet.
// If set OFF the DX9 setting is returned false only after a DX11 compatibility check
bool Spout::SetDX9(bool bDX9)
{
	return(interop.UseDX9(bDX9));
}


// Just return the flag that has been set
bool Spout::GetDX9()
{
	return interop.GetDX9();
}


// Set graphics adapter for Spout output
bool Spout::SetAdapter(int index)
{
	bool bRet = interop.SetAdapter(index);
	return bRet;

}


// Get current adapter index
int Spout::GetAdapter()
{
	return interop.GetAdapter();
}


// Get the number of graphics adapters in the system
int Spout::GetNumAdapters()
{
	return interop.GetNumAdapters();
}


// Get an adapter name
bool Spout::GetAdapterName(int index, char *adaptername, int maxchars)
{
	return interop.GetAdapterName(index, adaptername, maxchars);
}


// Get the path of the host that produced the sender
bool Spout::GetHostPath(const char *sendername, char *hostpath, int maxchars)
{
	return interop.GetHostPath(sendername, hostpath, maxchars);
}



bool Spout::WritePathToRegistry(const char *filepath, const char *subkey, const char *valuename)
{
	HKEY  hRegKey;
	LONG  regres;
	char  mySubKey[512];

	// The required key
	strcpy_s(mySubKey, 512, subkey);

	// Does the key already exist ?
	regres = RegOpenKeyExA(HKEY_CURRENT_USER, mySubKey, NULL, KEY_ALL_ACCESS, &hRegKey);
	if(regres != ERROR_SUCCESS) { 
		// Create a new key
		regres = RegCreateKeyExA(HKEY_CURRENT_USER, mySubKey, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,NULL, &hRegKey, NULL);
	}

	if(regres == ERROR_SUCCESS && hRegKey != NULL) {
		// Write the path
		regres = RegSetValueExA(hRegKey, valuename, 0, REG_SZ, (BYTE*)filepath, ((DWORD)strlen(filepath) + 1)*sizeof(unsigned char));
		RegCloseKey(hRegKey);
    }

	if(regres == ERROR_SUCCESS)
		return true;
	else
		return false;

}


bool Spout::ReadPathFromRegistry(char *filepath, const char *subkey, const char *valuename)
{
	HKEY  hRegKey;
	LONG  regres;
	DWORD  dwSize, dwKey;  

	dwSize = MAX_PATH;

	// Does the key exist
	regres = RegOpenKeyExA(HKEY_CURRENT_USER, subkey, NULL, KEY_READ, &hRegKey);
	if(regres == ERROR_SUCCESS) {
		// Read the key Filepath value
		regres = RegQueryValueExA(hRegKey, valuename, NULL, &dwKey, (BYTE*)filepath, &dwSize);
		RegCloseKey(hRegKey);
		if(regres == ERROR_SUCCESS)
			return true;
	}

	// Just quit if the key does not exist
	return false;

}

bool Spout::RemovePathFromRegistry(const char *subkey, const char *valuename)
{
	HKEY  hRegKey;
	LONG  regres;

	regres = RegOpenKeyExA(HKEY_CURRENT_USER, subkey, NULL, KEY_ALL_ACCESS, &hRegKey);
	if(regres == ERROR_SUCCESS) {
		regres = RegDeleteValueA(hRegKey, valuename);
		RegCloseKey(hRegKey);
		return true;
	}

	return false;
}

// For debugging only
void Spout::UseAccessLocks(bool bUseLocks)
{
	interop.spoutdx.bUseAccessLocks = bUseLocks;
}


int Spout::ReportMemory()
{
	int nTotalAvailMemoryInKB = 0;
	int nCurAvailMemoryInKB = 0;

	glGetIntegerv(0x9048, &nTotalAvailMemoryInKB);
	glGetIntegerv(0x9049, &nCurAvailMemoryInKB);
	// printf("Memory used : Total [%i], Available [%i]\n", nTotalAvailMemoryInKB, nCurAvailMemoryInKB);

	return nCurAvailMemoryInKB;

}
