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
//		24.02.22	- Restore GetSenderAdapter for testing
//		10.04.22	- ReceiveSenderData() - correct duplication of DX9 formats
//		16.04.22	- Add more log notices to GetSenderAdapter
//		18.04.22	- Change default invert from true to false for fbo sending functions
//		28.04.22	- SelectSenderPanel - if SpoutPanel is not found,
//					  show a MessageBox and direct to the Spout home page
//		05.05.22	- SendFbo - mods for default framebuffer
//		30.07.22	- SendFbo - avoid using glGetIntegerv if width and height are passed in
//				      Revert to default invert true after further testing with default framebuffer.
//		28.10.22	- Add GetPerformancePreference, SetPerformancePreference, GetPreferredAdapterName
//		01.11.22	- Add SetPreferredAdapter
//		03.11.22	- Add IsPreferenceAvailable
//					  SetAdapter - GL/DX compatibility is re-tested in OpenSpout
//		07.11.22	- Add IsApplicationPath
//					  Update ReceiveSenderData code comments for Windows Graphics Preferences
//		28.11.22	- SelectSenderPanel - correct warning if SpoutPanel is not found instead of the 
//					  ShellExecute Windows error and allow user the option to open the Spout home page.
//		05.12.22	- Add CleanSenders to SetSenderName
//		07.12.22	- SelectSender return bool
//		12.12.22	- SetSenderName - return if the same name
//		13.12.22	- SetSenderName revise for null name passed
//		14.12.22	- Remove SetAdapter. Requires OpenGL setup.
//		18.12.22	- Change unused bDX9 argument to const with default
//		20.12.22	- More checks for null arguments
//		22.12.22	- Compiler compatibility check
//				      Conditional compile of preference functions
//		06.01.23	- UIntToPtr for cast of uint32_t to HANDLE
//					  cast unsigned int array for glGetIntegerv instead of result
//					  Avoid c-style cast where possible
//		08.01.23	- Code review - Use Microsoft Native Recommended rules
//		08.03.23	- GetSenderAdapter use SetAdpater instead of SetAdapterPointer
//		21.03.23	- ReceiveSenderData - use the format of the D3D11 texture generated
//					  by OpenDX11shareHandle for incorrect sender information.
//	Version 2.007.011
//		22.04.23	- Minor code comments cleanup
//		29.04.23	- ReceiveSenderData - test for incorrect sender dimensions
//		07.05.23	- CheckSender - release interop device and object to re-create
//		17.05.23	- ReleaseSender - add m_bInitialized = false and remove from SpoutGL::GleanupGL
//		18.05.23	- ReleaseSender - clear m_SenderName
//		28.06.23	- Remove bDX9 option from GetAdapterInfo
//		03.07.23	- CreateReceiver - remove unused bUseActive flag
//					  and UNREFERENCED_PARAMETER (#PR93  Fix MinGW error(beta branch)
//		22.07.23	- ReceiveSenderData -
//					  ensure m_pSharedTexture is null if OpenSharedResource failed.
//		03.08.23	- InitReceiver - set m_DX11format
//		07.08.23	- Add frame sync option functions
//	Version 2.007.012
//		09.10.23	- SelectSenderPanel -if SpoutPanel.exe is not found
//					  show a SpoutMessageBox with Spout releases page url
//		18.10.23	- ReceiveSenderData - check for texture format supported
//					  by OpenGL/DirectX interop
//		07.12.23	- use _access in place of shlwapi Path functions
//	Version 2.007.013
//		14.01.24	- CheckSender - return false if OpenSpout fails
//		13.02.24	- SelectSenderPanel
//					  m_ShExecInfo.lpParameters : receiver graphics adapter index by default
//		19.04.24	- SelectSenderPanel - allow for unicode build for PROCESSENTRY32
//		24.04.24	- ReceiveImage - allow for multiple OpenGL formats
//		06.05.24	- SelectSenderPanel - removed unused "value" variable
//		21.05.24	- SetSenderName - move sender name increment to SenderNames class
//		22.05.24	- Add GetReceiverName
//					  CheckSpoutPanel - do not to register twice if already registered
//		25.05.24	- SetSenderName - remove name increment
//		08.06.24	- Add GetSenderList
//		09.06.24	- SelectSender - use SpoutMessageBox if SelectSenderPanel fails
//					  Add hwnd argument to centre MessageBox dialog if used.
//	Version 2.007.014
//		13.06.24	- SelectSender - open SpoutMessagebox for no senders.
//					  Add OK/CANCEL and test for empty senderlist.
//		21.06.24	- Add GetSenderIndex
//					  Modify SelectSender to show the active sender as current
//		03.07.24	- SelectSender - pass hWnd to SpoutPanel command line
//					  for it to open centred on the window
//		04.07.24	- CheckSpoutPanel - allow for use of SpoutMessageBox
//		15.07.24	- SelectSender - after cast of window handle to long 
//					  convert to a string of 8 characters without new line
//		16.07.24	- Add receiver ID3D11Texture2D* GetSenderTexture()
//		21.08.24	- SetPerformancePreference - remove null path test
//		23.08.24	- SelectSender - if no SpoutPanel and SpoutMessageBox is used :
//					  test for successful open of the sender share handle
//					   - Warn if NT share handle
//					   - Warn for open failure
//					   - Allow setting preferences for laptop
//					   - Allow sender adapter test for desktop
//					   - Refer to Spout settings if no resolution or a desktop system
//					  Also warn in SpoutPanel
//		03-09-24	- Graphics preference functions available but disabled if not NTDDI_WIN10_RS4
//		10.09.24	- SelectSenderPanel - test for exe file name for terminate
//		25.09.24	- Revise ReceiveTexture and extend code comments for sender update
//		08.01.25	- Add empty senderlist check in SelectSender()
//		05.03.25	- Add m_bSender flag for sender/receiver
//					  Set by Spout::CheckSender and also by Spout::InitReceiver
//					  SetFrameSync/WaitFrameSync - do not block of not initialized
//		18.05.25	- SelectSender - if SpoutPanel was not found, remove the
//					  empty sender list return to still display a sender list box
//		09.08.25	- SetSenderName - add !*sendername check
//		16.08.25	- Add CloseFrameSync
//		11.10.25	- SelectSenderPanel - CreateToolhelp32Snapshot
//					  change NULL argument to 0, Change hRes = NULL to hRes = 0
//		21.10.25	- Remove initial update check from ReceiveTexture and ReceiveImage
//					  The update flag is reset on the next call to ReceiveSenderData.
//					  Calling IsUpdated is optional if receiving to a pre-allocated
//					  texture or accessing the sender texture directly.
//
// ====================================================================================
/*

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
// If no name is specified, the executable name is used. 
// Thereafter, all sending functions create and update a sender
// based on the size passed and the name that has been set.
// If a sender with this name is already registered,
// create an incremented name : sender_1, sender_2 etc.
// and update the passed name
void Spout::SetSenderName(const char* sendername)
{
	if (!sendername || !*sendername) {
		// Get executable name as default
		strcpy_s(m_SenderName, 256, GetExeName().c_str());
	}
	else {
		strcpy_s(m_SenderName, 256, sendername);
	}

	// Create an incremented name if a sender with this name is already registered,
	// Although this function precedes SpoutSenderNames::RegisterSenderName,
	// a further increment is not applied when a sender with the new name is created.
	char name[256]{};
	strcpy_s(name, 256, m_SenderName);
	if (sendernames.FindSenderName(name)) {
		int i = 1;
		do {
			sprintf_s(name, 256, "%s_%d", m_SenderName, i);
			i++;
		} while (sendernames.FindSenderName(name));

		// Re-set the global sender name
		strcpy_s(m_SenderName, 256, name);
	}

	// Remove the sender from the names list if it's
	// shared memory information does not exist.
	// This can happen if the sender has crashed or if a
	// console window was closed instead of the main program.
	sendernames.CleanSenders();

}

//---------------------------------------------------------
// Function: SetSenderFormat
// Set the sender DX11 shared texture format
//    Compatible formats - see SpoutGL::SetDX11format(DXGI_FORMAT textureformat)
void Spout::SetSenderFormat(DWORD dwFormat)
{
	m_dwFormat = dwFormat;
	// Update SpoutGL class global texture format
	SetDX11format(static_cast<DXGI_FORMAT>(dwFormat));
}

//---------------------------------------------------------
// Function: ReleaseSender
// Close sender and release resources.
//
// A new sender is created or updated by all sending functions
void Spout::ReleaseSender()
{
	SpoutLogNotice("Spout::ReleaseSender(%s)", m_SenderName);

	if (m_bInitialized) {
		sendernames.ReleaseSenderName(m_SenderName);
		m_SenderName[0]=0;
		frame.CleanupFrameCount();
		frame.CloseAccessMutex();
		m_bInitialized = false;
	}

	// Close 2.006 or buffer shared memory if used
	memoryshare.Close();

	// Release sync event if used
	frame.CloseFrameSync();

	// Interop objects must be released before releasing shared texture
	CleanupInterop();

	// Release OpenGL resources
	CleanupGL();

}

//---------------------------------------------------------
// Function: SendFbo
// Send texture attached to fbo
//
//   The fbo must be bound for read. 
//
//   The sending texture attached to the fbo can be larger than the sender
//   if the application is using only a portion of the allocated texture space,  
//   such as for FreeframeGL plugins. (The 2.006 equivalent is DrawToSharedTexture)
//
//   To send the default OpenGL framebuffer, specify FboID = 0. 
//   If width and height are also 0, the function determines the viewport size. 
//
bool Spout::SendFbo(GLuint FboID, unsigned int fbowidth, unsigned int fboheight, bool bInvert)
{
	// The size of the texture attached to the fbo must be
	// equal to or larger than the sending texture
	// Establish local width and height in case the viewport size is used
	unsigned int width  = fbowidth;
	unsigned int height = fboheight;

	// For the default framebuffer, specify FboID = 0
	if (FboID == 0) {
		// Default framebuffer fails if iconic
		if (IsIconic(m_hWnd))
			return false;
		// Get the viewport size if width and height are passed as zero.
		// Use this only when necessary to prevent repeated calls to
		// glGetIntegerv because performance can be affected.
		if (width == 0 || height == 0) {
			// Get the viewport size
			unsigned int vpdims[4]{0};
			glGetIntegerv(GL_VIEWPORT, (GLint*)vpdims);
			width  = vpdims[2];
			height = vpdims[3];
		}
	}

	// Create or update the sender
	if (!CheckSender(width, height)) {
		return false;
	}

	// All clear to send the fbo texture
	if(m_bTextureShare) {
		// Specify 0 for texture ID and target
		// to write the texture attached to the fbo.
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
	if (!CheckSender(width, height))
		return false;

	if (m_bTextureShare) { // if GL/DX interop compatible
		// Send OpenGL texture 
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
//     SendImage creates a shared texture using image pixels as the source.
//     RGBA, BGRA, RGB, BGR formats are supported.
//     The invert flag is optional and false by default.
//     The ID of a currently bound fbo should be passed in.
//
bool Spout::SendImage(const unsigned char* pixels, unsigned int width, unsigned int height, GLenum glFormat, bool bInvert, GLuint HostFBO)
{
	// Dimensions should be the same as the sender
	if (!pixels || width == 0 || height == 0)
		return false;

	// Only RGBA, BGRA, RGB, BGR are supported
	// (DX11 format DXGI_FORMAT_B8G8R8A8_UNORM)
	GLenum glformat = glFormat;
	if (!(glformat == GL_RGBA || glformat == GL_BGRA_EXT
	   || glformat == GL_RGB  || glformat == GL_BGR_EXT)) {
		SpoutLogError("Spout::SendImage - unsupported format 0x%X\n", glformat);
		return false;
	}

	// Check for BGRA support
	if (!m_bBGRAavailable) {
		SpoutLogWarning("Spout::SendImage - BGRA extensions not available");
		// If the bgra extensions are not available and the user
		// provided GL_BGR_EXT or GL_BGRA_EXT do not use them
		if (glformat == GL_BGR_EXT)  glformat = GL_RGB;
		if (glformat == GL_BGRA_EXT) glformat = GL_RGBA;
	}

	// Create or update the sender
	if (!CheckSender(width, height))
		return false;

	//
	// Write pixel data to the rgba shared texture
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
// Sender sharing compatibility.
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
	if (SenderName) {
		if (*SenderName) {
			// Connect to the specified sender
			strcpy_s(m_SenderNameSetup, 256, SenderName);
			strcpy_s(m_SenderName, 256, SenderName);
			return;
		}
	}

	// Connect to the active sender
	m_SenderNameSetup[0] = 0;
	m_SenderName[0] = 0;

}

//---------------------------------------------------------
// Function: GetReceiverName
// Get sender for connection
bool Spout::GetReceiverName(char* sendername, int maxchars)
{
	if (m_SenderNameSetup[0]) {
		strcpy_s(sendername, maxchars, m_SenderNameSetup);
		return true;
	}
	return false;
}

//---------------------------------------------------------
// Function: ReleaseReceiver
// Close receiver and release resources ready to connect to another sender
void Spout::ReleaseReceiver()
{
	if (!m_bInitialized)
		return;

	SpoutLogNotice("Spout::ReleaseReceiver");

	// Release interop
	CleanupInterop();

	// Release OpenGL resources.
	CleanupGL();

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
//   The update flag is reset on the next call to ReceiveSenderData.
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
	// Make sure OpenGL and DirectX are initialized
	if (!OpenSpout())
		return false;

	// Try to receive texture details from a sender
	if (ReceiveSenderData()) {

		// Found a sender
		// The sender name, width, height, format, shared texture handle
		// and shared texture pointer have been retrieved
		// Let the application know
		m_bConnected = true;

		// If the connected sender sharehandle or name is different, the receiver
		// is re-initialized and m_bUpdated is set true by ReceiveSenderData so that
		// IsUpdated() returns true and the application re-allocates the receiving texture.
		if (m_bUpdated) {
			// If the sender is new or changed, reset shared/linked textures
			// Set "true" for receiver
			if (!CreateInterop(m_Width, m_Height, m_dwFormat, true)) {
				return false;
			}
			//
			// Return now for the application to test IsUpdated() and 
			// re-allocate the receiving texture.
			//
			// m_bUpdated is reset to false on the next call to 
			// ReceiveSenderData until the sender changes size again.
			//
			return true;
		}

		// Was the sender's shared texture handle null
		// or has the user set 2.006 memoryshare mode?
		if (!m_dxShareHandle || m_bMemoryShare) {
			// Possible existence of 2.006 memoryshare sender (no texture handle)
			// (ReadMemoryTexture currently only works if texture share compatible)
			if (m_bTextureShare) {
				if (ReadMemoryTexture(m_SenderName, TextureID, TextureTarget, m_Width, m_Height, bInvert, HostFbo)) {
					return true;
				}
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
	if (!pixels || !Sendername)
		return false;

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
	const bool bRet = m_bUpdated;
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
// Function: GetSenderTexture
// Received sender texture
ID3D11Texture2D* Spout::GetSenderTexture()
{
	return m_pSharedTexture;
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
bool Spout::SelectSender(HWND hwnd)
{
	//
	// Use SpoutPanel if available
	//
	// SpoutPanel opens either centred on the cursor position 
	// or on the application window if the handle is passed in.

	// For a valid window handle, convert hwnd to chars
	// for the SpoutPanel command line
	char * msg = nullptr;
	if (hwnd) {
		// Window handle is an 32 bit unsigned value
		// Cast to long of 8 characters without new line
		msg = new char[256];
		sprintf_s(msg, 256, "%8.8ld", HandleToLong(hwnd));
	}

	if (!SelectSenderPanel(msg)) {

		// If SpoutPanel is not available, use a SpoutMessageBox for sender selection.
		// Note that SpoutMessageBox is modal and will interrupt the host program.

		// create a local sender list
		std::vector<std::string> senderlist = GetSenderList();
	
		// Get the active sender index "selected".
		// The index is passed in to SpoutMessageBox and used as the current combobox item.
		int selected = 0;
		char sendername[256]{};
		if (GetActiveSender(sendername))
			selected = GetSenderIndex(sendername);

		// SpoutMessageBox opens either centered on the cursor position 
		// or on the application window if the handle is passed in.
		if (!hwnd) {
			POINT pt{};
			GetCursorPos(&pt);
			SpoutMessageBoxPosition(pt);
		}

		// Show the SpoutMessageBox even if the list is empty.
		// This makes it clear to the user that no senders are running.
		if (SpoutMessageBox(hwnd, NULL, "Select sender", MB_OKCANCEL, senderlist, selected) == IDOK && !senderlist.empty()) {

			// Release the receiver
			ReleaseReceiver();

			// Set the selected sender as active for the next receive
			SetActiveSender(senderlist[selected].c_str());

			//
			// Test for successful open of the sender share handle
			//
			// - Warn if NT share handle
			// - Warn for open failure
			// - Allow setting preferences for laptop
			// - Allow sender adapter test for desktop
			// - Refer to Spout settings if no resolution or a desktop system
			SharedTextureInfo info{};
			if (sendernames.getSharedInfo(senderlist[selected].c_str(), &info)) {
				std::string str;
				ID3D11Texture2D* pTexture = nullptr;
				if (spoutdx.OpenDX11shareHandle(spoutdx.GetDX11Device(), &pTexture, LongToHandle((long)info.shareHandle))) {
					// If OpenDX11shareHandle succeeded, sender and receiver use the same adapter
					// Check for an NT handle
					D3D11_TEXTURE2D_DESC desc{};
					pTexture->GetDesc(&desc);
					if (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX) {
						if (desc.MiscFlags & D3D11_RESOURCE_MISC_SHARED_NTHANDLE) {
							str = "NT handle shared texture not supported\n";
							SpoutMessageBox(hwnd, str.c_str(), "Warning", MB_OK | MB_ICONWARNING);
						}
					}
					spoutdx.ReleaseDX11Texture(pTexture);
				}
				else {
					str ="WARNING - failed to open texture share handle\n\n";
					if (IsLaptop()) {
						str += "Laptop system detected.\n\n";
#ifdef NTDDI_WIN10_RS4
						str += "<a href=\"ms-settings:display-advancedgraphics\">Windows Graphics Performance Preferences</a>\n";
						// Windows preferences for laptop power saving graphics
						//     -1 - No preference
						//      0 - Default
						//      1 - Power saving
						//      2 - High performance
						char senderpath[512]{};
						strcpy_s(senderpath, 512, (char*)info.description);
						if (_access(senderpath, 0) != -1) {
							// Sender
							int senderpref = GetPerformancePreference(senderpath);
							// Receiver is this application
							int receiverpref = GetPerformancePreference();
							// If both are already set to high performance, there is another problem
							if (senderpref == 2 && receiverpref == 2) {
								str += "Both sender and receiver are already set\n";
								str += "to prefer High performance.\n\n";
								// General
								str += "Open \"SpoutSettings\" and \"Reset\" for default settings\n";
								str += "Click \"Diagnostics\" to show system details, and \"Logs\"\n";
								str += "to explore application logs. If sender or receiver log files\n";
								str += "are available, examine for Warnings and Errors.\n\n";
								str += "Seek further assistance on the <a href=\"https://spout.discourse.group/\">Spout Discourse Group</a>\n\n";
								SpoutMessageBox(hwnd, str.c_str(), "Warning", MB_OK | MB_ICONWARNING);
							}
							else {
								str += "Both sender and receiver applications\n";
								str += "must be set to prefer High performance.\n";
								if (senderpref == -1 && receiverpref == -1) {
									str += "    No preferences set\n";
								}
								else {
									str += "    Sender    - ";
									if (senderpref == -1) str += "  No preference set\n";
									if (senderpref ==  0) str += "  Let Windows decide\n";
									if (senderpref ==  1) str += "  Power saving\n";
									if (senderpref ==  2) str += "  High performance\n";
									str += "    Receiver - ";
									if (receiverpref == -1) str += "  No preference set\n";
									if (receiverpref ==  0) str += "  Let Windows decide\n";
									if (receiverpref ==  1) str += "  Power saving\n";
									if (receiverpref ==  2) str += "  High performance\n";
									str += "\n";
								}
								str += "Change both sender and receiver\nto prefer High Performance now?\n\n";
								if (SpoutMessageBox(hwnd, str.c_str(), "Warning", MB_YESNO | MB_ICONWARNING) == IDYES) {
									// Sender
									SetPerformancePreference(2, senderpath);
									// Receiver
									SetPerformancePreference(2);
									SpoutMessageBox(hwnd, "Restart sender and receiver\nfor changes to take effect", "Information", MB_OK | MB_ICONINFORMATION);
								}
							}
						}
#else
						str += "No graphics preferences available\n";
#endif
					} // endif laptop system
					else {
						int nadapters = GetNumAdapters();
						str += "Desktop system with ";
						str += std::to_string(nadapters);
						if(nadapters == 1)
							str += " graphics adapter.\n";
						else
							str += " graphics adapters.\n";
						// List multiple graphics adapters
						char adaptername[256]{};
						for (int i=0; i<nadapters; i++) {
							GetAdapterName(i, adaptername);
							str += " ("; str += std::to_string(i); str += ") ";
							str += adaptername; str += "\n";
						}
						str += "\n";
						if (nadapters > 1) {
							str += "Both sender and receiver applications\n";
							str += "must use the same graphics adapter.\n";
							// Get the receiver adapter name
							int adapterindex = GetAdapter();
							GetAdapterName(adapterindex, adaptername);
							str += "Receiver - ";
							str += " ("; str += std::to_string(adapterindex); str += ") ";
							str += adaptername; str += "\n\n";
							// Optional test for sender adapter
							str += "Click \"Test\" below to find an adapter that opens the\n";
							str += "texture share handle and compare with the receiver.\n";
							str += "Note that this process can fail for some systems.\n\n";
							SpoutMessageBoxButton(1000, L"Test");
							if (SpoutMessageBox(hwnd, str.c_str(), "Warning", MB_OK | MB_ICONWARNING) == 1000) {
								// Loop though adapters and get the Sender adapter index and name
								char senderadaptername[256]{};
								int senderadapter = GetSenderAdapter(senderlist[selected].c_str(), senderadaptername);
								str = "Sender    - ";
								str += " ("; str += std::to_string(senderadapter); str += ") ";
								str += senderadaptername; str += "\n";
								// Receiver index and adapter name
								adapterindex = GetAdapter();
								GetAdapterName(adapterindex, adaptername);
								str += "Receiver - ";
								str += " ("; str += std::to_string(adapterindex); str += ") ";
								str += adaptername; str += "\n";
								if (senderadapter == adapterindex) {
									str += "\nReceiver and sender use the same graphics adapter\n";
									SpoutMessageBox(hwnd, str.c_str(), "Graphics", MB_OK | MB_ICONINFORMATION);
								}
								else {
									str += "\nReceiver and sender use different graphics adapters\n";
									str += "Refer to the sender application documentation\n";
									SpoutMessageBox(hwnd, str.c_str(), "Warning", MB_OK | MB_ICONWARNING);
								}
							}
							str = ""; // Clear first message if multiple adapters
						}

						// General
						str += "Open \"SpoutSettings\" and \"Reset\" for default settings\n";
						str += "Click \"Diagnostics\" to show system details, and \"Logs\"\n";
						str += "to explore application logs. If sender or receiver log files\n";
						str += "are available, examine for Warnings and Errors.\n\n";
						str += "Seek further assistance on the <a href=\"https://spout.discourse.group/\">Spout Discourse Group</a>\n\n";
						SpoutMessageBox(hwnd, str.c_str(), "Settings", MB_OK | MB_ICONINFORMATION, "Spout settings");

					} // endif desktop system
				} // endif opensharehandle failed
			} // endif senderinfo

			// Set the opened flag in the same way as for SelectSenderPanel
			// to indicate that the user has selected a sender.
			// This is tested in CheckSpoutPanel.
			m_bSpoutPanelOpened = true;
		}
	}
	
	if (msg) delete[] msg;

	return true;


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

//
// Group: Frame synchronization
//
//
//   Notes for synchronisation.
//
//
// In cases where the receiver or the sender have different processing or cycle rates
// it is often necessary to synchronize one with the other to avoid missed or duplicate frames
// and possible visible hesitations.
//
// This can be achieved using event functions "SetFrameSync" and "WaitFrameSync".
//
//      - void SetFrameSync(const char* SenderName);
//      - bool WaitFrameSync(const char *SenderName, DWORD dwTimeout = 0);
//
//   WaitFrameSync
//   A sender or receiver should use this before rendering wait for a signal from
//   the other process that it is ready to send or to read another frame.
//
//   SetFrameSync
//   After processing, a sender or receiver should signal that it is ready to
//   either send or read another frame. 
//
// EXAMPLES
//
// 1) If the sender is faster, the slower receiver will miss frames.
//
//    Sender
//    Before processing, the sender waits for a signal from the receiver that it is ready to receive a new frame.
//        WaitFrameSync(const char* sendername, DORD dwTimeout);'
//    Receiver
//    After processing, signals the sender to produce a new frame.
//        SetFrameSync(const char* sendername);
//
// 2) If the sender is slower, the faster receiver will duplicate frames.
//
//    Receiver
//    Before processing, the receiver waits for a signal from the sender that a new frame is ready.
//        WaitFrameSync(const char* sendername, DORD dwTimeout);
//    Sender
//        After processing, signals the receiver that a new frame is ready.
//        SetFrameSync(const char* sendername);
//

// -----------------------------------------------
// Function: SetFrameSync
// Signal sync event.
//   Create a named sync event and set for test
void Spout::SetFrameSync(const char* name)
{
	// Do not block of not initialized
	if (!m_bInitialized)
		return;

	if (!name)
		frame.SetFrameSync(m_SenderName);
	else
		frame.SetFrameSync(name);

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
	if (!SenderName || !*SenderName || !m_bInitialized)
		return false;
	return frame.WaitFrameSync(SenderName, dwTimeout);
}

// -----------------------------------------------
// Function: EnableFrameSync
// Enable / disable frame sync
void Spout::EnableFrameSync(bool bSync)
{
	frame.EnableFrameSync(bSync);
}

// -----------------------------------------------
// Function: CloseFrameSync
// Close frame sync
void Spout::CloseFrameSync()
{
	frame.CloseFrameSync();
}

// -----------------------------------------------
// Function: IsFrameSyncEnabled
// Check for frame sync option
bool Spout::IsFrameSyncEnabled()
{
	return frame.IsFrameSyncEnabled();
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
	if (!sendername)
		return false;
	return sendernames.GetSender(index, sendername, MaxSize);
}

//---------------------------------------------------------
// Function: GetSenderList
// Return a list of current senders
std::vector<std::string> Spout::GetSenderList()
{
	std::vector<std::string> list;
	int nSenders = GetSenderCount();
	if (nSenders > 0) {
		char sendername[256]{};
		for (int i=0; i<nSenders; i++) {
			if (GetSender(i, sendername))
				list.push_back(sendername);
		}
	}
	return list;
}

//---------------------------------------------------------
// Function: GetSenderIndex
// Sender index into the set of names
int Spout::GetSenderIndex(const char* sendername)
{
	return sendernames.GetSenderIndex(sendername);
}

//---------------------------------------------------------
// Function: GetSenderInfo
// Sender information
bool Spout::GetSenderInfo(const char* sendername, unsigned int &width, unsigned int &height, HANDLE &dxShareHandle, DWORD &dwFormat)
{
	if (!sendername)
		return false;
	return sendernames.GetSenderInfo(sendername, width, height, dxShareHandle, dwFormat);
}

//---------------------------------------------------------
// Function: GetActiveSender
// Current active sender name
bool Spout::GetActiveSender(char* Sendername)
{
	if (!Sendername)
		return false;
	return sendernames.GetActiveSender(Sendername);
}

//---------------------------------------------------------
// Function: SetActiveSender
// Set sender as active
bool Spout::SetActiveSender(const char* Sendername)
{
	if (!Sendername)
		return false;
	return sendernames.SetActiveSender(Sendername);
}

//
// Group: Graphics adapter
//
// Return graphics adapter number and names.
// Refer to the SpoutDirectX class for details.
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
	char name[256]{};
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
// Function: GetSenderAdapter
// Get adapter index and name for a given sender
//
// OpenDX11shareHandle will fail if the share handle has been created 
// using a different graphics adapter (see spoutDirectX).
//
// This function loops though all graphics adapters in the system
// until OpenDX11shareHandle is successful and the same adapter
// index as the sender is established. 
//
// This adapter can then be used by CreateDX11device when the Spout 
// DirectX device is created. This can be done in DirectX applications
// (see examples for the SpoutDX class), but not for OpenGL because both
// OpenGL and DirectX must use the same adapter.
//
// The function is included here for diagnostic purposes.
//
int Spout::GetSenderAdapter(const char* sendername, char* adaptername, int maxchars)
{
	if (!sendername || !adaptername)
		return -1;

	int senderadapter = -1;
	ID3D11Texture2D* pSharedTexture = nullptr;
	ID3D11Device* pDummyDevice = nullptr;
	ID3D11DeviceContext* pContext = nullptr;
	IDXGIAdapter* pAdapter = nullptr;
	SpoutLogNotice("Spout::GetSenderAdapter - testing for sender adapter (%s)", sendername);

	SharedTextureInfo info{};
	if (!sendernames.getSharedInfo(sendername, &info))
		return -1;

	// Get the current device adapter
	const int adapterIndex = spoutdx.GetAdapter();
	const int nAdapters = spoutdx.GetNumAdapters();
	for (int i = 0; i < nAdapters; i++) {
		pAdapter = spoutdx.GetAdapterPointer(i);
		if (pAdapter) {
			// Set the adapter for CreateDX11device to use temporarily
			spoutdx.SetAdapter(i);
			// Create a dummy device using this adapter
			pDummyDevice = spoutdx.CreateDX11device();
			if (pDummyDevice) {
				// Try to open the share handle with the device created from the adapter
				if (spoutdx.OpenDX11shareHandle(pDummyDevice, &pSharedTexture, UIntToPtr(info.shareHandle))) {
					// break as soon as it succeeds
					SpoutLogNotice("  found sender adapter %d(0x%.7X)[%s]", i, PtrToUint(pAdapter), adaptername);
					senderadapter = i;
					// Return the adapter name
					spoutdx.GetAdapterName(i, adaptername, maxchars);
					pDummyDevice->GetImmediateContext(&pContext);
					if (pContext) pContext->Flush();
					pDummyDevice->Release();
					pAdapter->Release();
					break;
				}
				SpoutLogNotice("    Could not open sender shared texture share handle for adapter %d", i);
				pDummyDevice->GetImmediateContext(&pContext);
				if (pContext) pContext->Flush();
				pDummyDevice->Release();
			}
			SpoutLogNotice("    Could not create DX11 device for test");
			pAdapter->Release();
		}
	}

	// Set the SpoutDirectX class adapter back to what it was
	spoutdx.SetAdapter(adapterIndex);

	return senderadapter;

}

//---------------------------------------------------------
// Function: GetAdapterInfo
// Get the description and output name of the current adapter
bool Spout::GetAdapterInfo(char* description, char* output, int maxchars)
{
	return spoutdx.GetAdapterInfo(description, output, maxchars);
}

//---------------------------------------------------------
// Function: GetAdapterInfo
// Get the description and output display name for a given adapter
bool Spout::GetAdapterInfo(int index, char* description, char* output, int maxchars)
{
	return spoutdx.GetAdapterInfo(index, description, output, maxchars);
}


//
// Group: Graphics performance
//
// Windows Graphics performance preferences.
// Refer to the SpoutDirectX class for details.
//
// Performance prefrence settings are available from Windows 10
// April 2018 update "Redstone 4" (Version 1803, build 17134) and later.
// Windows 10 SDK required included in Visual Studio 2017 ver.15.7 
//

//---------------------------------------------------------
// Function: GetPerformancePreference
// Get the Windows graphics preference for an application
//
//	-1 - Not registered
//
//	 0 - DXGI_GPU_PREFERENCE_UNSPECIFIED
//
//	 1 - DXGI_GPU_PREFERENCE_MINIMUM_POWER
//
//	 2 - DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
//
int Spout::GetPerformancePreference(const char* path)
{
	return spoutdx.GetPerformancePreference(path);
}

//---------------------------------------------------------
// Function: SetPerformancePreference
// Set the Windows graphics preference for an application
//
//     -1 - No preference
//
//      0 - Default
//
//      1 - Power saving
//
//      2 - High performance
//
bool Spout::SetPerformancePreference(int preference, const char* path)
{
	return spoutdx.SetPerformancePreference(preference, path);
}

//---------------------------------------------------------
// Function: GetPreferredAdapterName
//
// Get the graphics adapter name for a Windows preference
// This is the first adapter for the given preference :
//
//    DXGI_GPU_PREFERENCE_UNSPECIFIED - (0) Equivalent to EnumAdapters1
//
//    DXGI_GPU_PREFERENCE_MINIMUM_POWER - (1) Integrated GPU
//
//    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE - (2) External GPU / Discrete GPU
//
bool Spout::GetPreferredAdapterName(int preference, char* adaptername, int maxchars)
{
	return spoutdx.GetPreferredAdapterName(preference, adaptername, maxchars);
}

//---------------------------------------------------------
// Function: SetPreferredAdapter
//
// Set graphics adapter index for a Windows preference
//
// This index is used by CreateDX11device when DirectX is intitialized
//
//    DXGI_GPU_PREFERENCE_UNSPECIFIED - (0) Equivalent to EnumAdapters1
//
//    DXGI_GPU_PREFERENCE_MINIMUM_POWER - (1) Integrated GPU
//
//    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE - (2) External GPU / Discrete GPU
//
bool Spout::SetPreferredAdapter(int preference)
{
	return spoutdx.SetPreferredAdapter(preference);
}

//---------------------------------------------------------
// Function: IsPreferenceAvailable()
// Availability of Windows graphics preference settings.
//
bool Spout::IsPreferenceAvailable()
{
	return spoutdx.IsPreferenceAvailable();
}

//---------------------------------------------------------
// Function: IsApplicationPath
//
// Is the path a valid application
//
// A valid application path will have a drive letter and terminate with ".exe"
bool Spout::IsApplicationPath(const char* path)
{
	if (!path)
		return false;

	return spoutdx.IsApplicationPath(path);
}

//
// Group: 2.006 compatibility
//
// These functions are not necessary for Version 2.007
// and should not be used for a new application.
// They are retained for compatibility with existing 2.006 code
// and may be removed in future release.
// For full compatibility with exsiting 2.006 code, the original
// 2.006 SDK is preserved in a <separate branch. at https://github.com/leadedge/Spout2/tree/2.006>
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
// NOTES : On a “normal” system the Windows function EnumDisplayDevices and the DirectX function
// IDXGIAdapter::GetDesc always concur. i.e. the device that owns the head will be the device that
// performs the rendering. 
//
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
//
// See also the DirectX only version :
// bool spoutDirectX::GetAdapterInfo(char *adapter, char *display, int maxchars)
// DirectX9 not supported
//
bool Spout::GetAdapterInfo(char* renderadapter,
	char* renderdescription, char* renderversion,
	char* displaydescription, char* displayversion,
	int maxsize)
{
	if(!renderadapter
	|| !renderdescription
	|| !renderversion
	|| !displaydescription
	|| !displayversion)
		return false;

	IDXGIDevice * pDXGIDevice = nullptr;

	*renderadapter = 0; // DirectX adapter
	*renderdescription = 0;
	*renderversion = 0;
	*displaydescription = 0;
	*displayversion = 0;

	if (!spoutdx.GetDX11Device()) {
		SpoutLogError("Spout::GetAdapterInfo - no DX11 device");
		return false;
	}

	spoutdx.GetDX11Device()->QueryInterface(__uuidof(IDXGIDevice), (void**)(&pDXGIDevice));
	if (!pDXGIDevice) return false;

	IDXGIAdapter * pDXGIAdapter = nullptr;
	pDXGIDevice->GetAdapter(&pDXGIAdapter);
	if (!pDXGIAdapter) return false;

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
	char output[256]{};
	size_t charsConverted = 0;
	wcstombs_s(&charsConverted, output, 129, adapterinfo.Description, 128);
	// printf("    Description = [%s]\n", output);
	// printf("    VendorId = [%d] [%x]\n", adapterinfo.VendorId, adapterinfo.VendorId);
	// printf("SubSysId = [%d] [%x]\n", adapterinfo.SubSysId, adapterinfo.SubSysId);
	// printf("DeviceId = [%d] [%x]\n", adapterinfo.DeviceId, adapterinfo.DeviceId);
	// printf("Revision = [%d] [%x]\n", adapterinfo.Revision, adapterinfo.Revision);
	strcpy_s(renderadapter, maxsize, output);
	if (!*renderadapter) return false;

	strcpy_s(renderdescription, maxsize, renderadapter);

	//
	// Use Windows functions to look for Intel graphics to see if it is
	// the same render adapter that was detected with DirectX
	//
	char driverdescription[256]{};
	char driverversion[256]{};
	char regkey[256]{};

	// Additional info
	DISPLAY_DEVICE DisplayDevice;
	DisplayDevice.cb = sizeof(DISPLAY_DEVICE);

	// Detect the adapter attached to the desktop.
	//
	// To select all display devices in the desktop, use only the display devices
	// that have the DISPLAY_DEVICE_ATTACHED_TO_DESKTOP flag in the DISPLAY_DEVICE structure.
	int nDevices = 0;
	for (DWORD i = 0; i < 10; i++) { // should be much less than 10 adapters
		if (EnumDisplayDevices(NULL, i, &DisplayDevice, 0)) {
			// This will list all the devices
			nDevices++;
			// Get the registry key
			wcstombs_s(&charsConverted, regkey, 129, (const wchar_t *)DisplayDevice.DeviceKey, 128);
			// This is the registry key with all the information about the adapter
			OpenDeviceKey(regkey, 256, driverdescription, driverversion);
			if (!driverdescription || !driverversion) {
				pDXGIDevice->Release();
				return false;
			}

			// Is it a render adapter ?
			if (renderadapter && strcmp(driverdescription, renderadapter) == 0) {
				strcpy_s(renderdescription, maxsize, driverdescription);
				strcpy_s(renderversion, maxsize, driverversion);
			}

			// Is it a display adapter
			if (DisplayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
				strcpy_s(displaydescription, 256, driverdescription);
				strcpy_s(displayversion, 256, driverversion);
			} // endif attached to desktop

		} // endif EnumDisplayDevices
	} // end search loop

	pDXGIDevice->Release();

	// The render adapter description
	trim(renderdescription);

	// The display adapter description
	trim(renderdescription);

	return true;
}

//---------------------------------------------------------
// Function: CreateSender
// Create a sender
bool Spout::CreateSender(const char* name, unsigned int width, unsigned int height, DWORD dwFormat)
{
	if (!name)
		return false;

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
	if (!name)
		return false;

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
bool Spout::CreateReceiver(char* sendername, unsigned int &width, unsigned int &height)
{
	if (!sendername)
		return false;

	if (!OpenSpout())
		return false;
	
	if (ReceiveSenderData()) {
		// The sender name, width, height, format, shared texture handle
		// and shared texture pointer have been retrieved.
		if (m_bUpdated) {
			// If the sender is new or changed, create or re-create interop
			if (m_bTextureShare) {
				// CreateInterop set "true" for receiver
				if (!CreateInterop(m_Width, m_Height, m_dwFormat, true)) {
					return false;
				}
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
	if (!name)
		return false;

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
	if (!name)
		return false;

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
// Format can be GL_RGBA, GL_BGRA, GL_RGB or GL_BGR for the receving buffer
bool Spout::ReceiveImage(unsigned char* pixels, GLenum glFormat, bool bInvert, GLuint HostFbo)
{
	// The receiving pixel buffer is created after the first update
	// so the pixel pointer can be NULL here

	// Make sure OpenGL and DirectX are initialized
	if (!OpenSpout())
		return false;

	// Check for BGRA support
	GLenum glformat = glFormat;
	if (!m_bBGRAavailable) {
		// If the bgra extensions are not available and the user
		// provided GL_BGR_EXT or GL_BGRA_EXT do not use them
		if (glFormat == GL_BGR_EXT)  glformat = GL_RGB; // GL_BGR_EXT
		if (glFormat == GL_BGRA_EXT) glformat = GL_RGBA; // GL_BGRA_EXT
	}

	// Try to receive texture details from a sender
	if (ReceiveSenderData()) {

		// The sender name, width, height, format, shared texture handle and pointer have been retrieved.
		// m_Width, m_Height, m_dwFormat are updated
		if (m_bUpdated) {
			//
			// Return now for the application to test IsUpdated() and 
			// re-allocate the receiving texture.
			//
			// m_bUpdated is reset to false on the next call to 
			// ReceiveSenderData until the sender changes size again.
			//
			// CreateInterop set "true" for receiver
			if (!CreateInterop(m_Width, m_Height, m_dwFormat, true)) {
				return false;
			}
			return true;
		}

		// The receiving pixel buffer is created after the first update
		// So check here instead of at the beginning
		if (!pixels) {
			return false;
		}

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
//
bool Spout::SelectSenderPanel(const char* message)
{
	HANDLE hMutex1 = NULL;
	HMODULE module = NULL;
	char path[MAX_PATH]{};
	char drive[MAX_PATH]{};
	char dir[MAX_PATH]{};
	char fname[MAX_PATH]{};
	char UserMessage[512]{};

	if (message && *message) {
		strcpy_s(UserMessage, 512, message); // could be an arg or a user message
	}
	else {
		// Send the receiver graphics adapter index by default
		strcpy_s(UserMessage, MAX_PATH, std::to_string(GetAdapter()).c_str());
	}

	// The selected sender is then the "Active" sender and this receiver switches to it.
	// If Spout is not installed, SpoutPanel.exe has to be in the same folder
	// as this executable. This rather complicated process avoids having to use a dialog
	// which causes problems with host GUI messaging.

	// First find if there has been a Spout installation >= 2.002 with an install path for SpoutPanel.exe
	path[0] = 0;
	if (!ReadPathFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutPanel", "InstallPath", path)) {
		// Path not registered so find the path of the host program
		// where SpoutPanel can be copied
		module = GetModuleHandle(NULL);
		GetModuleFileNameA(module, path, MAX_PATH);
		_splitpath_s(path, drive, MAX_PATH, dir, MAX_PATH, fname, MAX_PATH, NULL, 0);
		_makepath_s(path, MAX_PATH, drive, dir, "SpoutPanel", ".exe");
	}

	if (path[0]) {
		// Does SpoutPanel.exe exist in this path ?
		if(_access(path, 0) == -1) {
			// Try the current working directory
			if (_getcwd(path, MAX_PATH)) {
				strcat_s(path, MAX_PATH, "\\SpoutPanel.exe");
				// Does SpoutPanel exist here?
				if (_access(path, 0) == -1) {
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
			PROCESSENTRY32 pEntry{};
			pEntry.dwSize = sizeof(pEntry);
			bool done = false;
			// Take a snapshot of all processes and threads in the system
			HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPALL, 0);
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
					// Look through all processes to find SpoutPanel
					while (hRes && !done) {
#ifdef UNICODE
						int iRet = _wcsicmp(pEntry.szExeFile, L"SpoutPanel.exe");
#else
						int iRet = _tcsicmp(pEntry.szExeFile, _T("SpoutPanel.exe"));
#endif
						// Terminate if the file name is SpoutPanel.exe
						if (iRet == 0) {
							HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, 0, pEntry.th32ProcessID);
							if (hProcess != NULL) {
								// Terminate SpoutPanel and it's mutex if it opened
								TerminateProcess(hProcess, 9);
								CloseHandle(hProcess);
								done = true;
							}
						}

						if (!done)
							hRes = Process32Next(hProcessSnap, &pEntry); // Get the next process
						else
							hRes = 0; // found SpoutPanel
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
				glTexCoord2f(0.0, 0.0);		glVertex2f(-aspect,  1.0); // upper left
				glTexCoord2f(max_x, 0.0);	glVertex2f( aspect,  1.0); // upper right
				glTexCoord2f(max_x, max_y);	glVertex2f( aspect, -1.0); // lower right
			}
			else {
				glTexCoord2f(0.0, 0.0);		glVertex2f(-aspect, -1.0); // lower left
				glTexCoord2f(0.0, max_y);	glVertex2f(-aspect,  1.0); // upper left
				glTexCoord2f(max_x, max_y);	glVertex2f( aspect,  1.0); // upper right
				glTexCoord2f(max_x, 0.0);	glVertex2f( aspect, -1.0); // lower right
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
				GLfloat tc[4][2]{};
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
		SpoutLogWarning("Spout::CheckSender - zero width or height");
		return false;
	}

	// The sender needs a name. Default is the executable name
	// If a sender with this name is already registered,
	// SetSenderName increments the name : sender_1, sender_2 etc.
	if (!m_SenderName[0])
		SetSenderName();

	// If not initialized, create a new sender
	if (!m_bInitialized) {

		// Make sure that Spout has been initialized and an OpenGL context is available
		if (!OpenSpout()) {
			SpoutLogWarning("Spout::CheckSender - OpenSpout failed");
			return false;
		}

		if (m_bTextureShare) {
			// Create interop for GL/DX transfer
			//   Flag "false" for sender so that a new shared texture and handle are created.
			//   For a receiver the shared texture is created from the sender share handle.
			if (!CreateInterop(width, height, m_dwFormat, false)) {
				SpoutLogWarning("Spout::CheckSender(%s, %dx%d) - create - CreateInterop failed", m_SenderName, width, height);
				return false;
			}
		}
		else {
			// For CPU share with DirectX textures.
			// A sender creates a new shared texture within this class with a new share handle
			m_dxShareHandle = nullptr;
			if (!spoutdx.CreateSharedDX11Texture(spoutdx.GetDX11Device(),
				width, height, (DXGI_FORMAT)m_dwFormat, &m_pSharedTexture, m_dxShareHandle)) {
				SpoutLogWarning("Spout::CheckSender(%s, %dx%d) - create cpu - CreateSharedDX11Texture failed", m_SenderName, width, height);
				return false;
			}
		}

		// Create a sender using the DX11 shared texture handle (m_dxShareHandle)
		// If the sender already exists, the name is incremented
		// name, name_1, name_2 etc
		if (sendernames.CreateSender(m_SenderName, width, height, m_dxShareHandle, m_dwFormat)) {

			m_Width = width;
			m_Height = height;

			//
			// SetSenderID writes to the sender shared texture memory
			// to set sender CPU sharing mode and hardware GL/DX compatibility
			//
			// GL/DX compatible hardware (m_bUseGLDX) - set top bit
			//     1000 0000 0000 0000 0000 0000 0000 0000
			// CPU sharing methods (m_bCPUshare) - set next bit
			//     0100 0000 0000 0000 0000 0000 0000 0000
			//
			// Both top two bits can be set if GL/DX compatible
			// but the user has selected CPU share mode
			//
			SetSenderID(m_SenderName, m_bCPUshare, m_bUseGLDX);

			m_Width = width;
			m_Height = height;

			// Create a sender mutex for access to the shared texture
			frame.CreateAccessMutex(m_SenderName);

			// Enable frame counting so the receiver gets frame number and fps
			frame.EnableFrameCount(m_SenderName);

			m_bInitialized = true;

			// Sender rather than receiver
			m_bSender = true;

		}
		else {
			SpoutLogWarning("Spout::CheckSender(%s, %dx%d) - create - CreateSender failed", m_SenderName, width, height);
			ReleaseSender();
			m_SenderName[0] = 0;
			m_Width = 0;
			m_Height = 0;
			m_dwFormat = m_DX11format;
			return false;
		}
	}
	// The sender is initialized but has the sending texture changed size ?
	else if (m_Width != width || m_Height != height) {

		// Update the shared textures and interop
		if (m_bTextureShare) {
			// The linked textures cannot be re-sized so have to
			// be re-created. The interop object handle is then
			// re-created from the linking of the new textures.
			// Flag "false" for sender to create a new shared texture.
			// Release interop device/object and OpenGL objects and re-create
			CleanupInterop();
			CleanupGL();
			if (!CreateInterop(width, height, m_dwFormat, false)) {
				SpoutLogWarning("Spout::CheckSender(%s, %dx%d) - update CreateInterop failed", m_SenderName, width, height);
				return false;
			}
		}
		else {
			// For CPU share, the DirectX texture is not linked to OpenGL
			// Re-create the class shared texture to the new size
			if (m_pSharedTexture)
				spoutdx.ReleaseDX11Texture(GetDX11Device(), m_pSharedTexture);
			m_pSharedTexture = nullptr;
			m_dxShareHandle = nullptr;

			// Flush context to avoid deferred release
			spoutdx.Flush();

			if (!spoutdx.CreateSharedDX11Texture(spoutdx.GetDX11Device(),
				width, height, (DXGI_FORMAT)m_dwFormat, &m_pSharedTexture, m_dxShareHandle)) {
				SpoutLogWarning("Spout::CheckSender(%s, %dx%d) - cpu - CreateSharedDX11Texture failed", m_SenderName, width, height);
				return false;
			}
		}

		// Update the sender with the new texture and size
		if (!sendernames.UpdateSender(m_SenderName, width, height, m_dxShareHandle, m_dwFormat)) {
			SpoutLogWarning("Spout::CheckSender(%s, %dx%d) - UpdateSender failed", m_SenderName, width, height);
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
	if (!SenderName)
		return;

	SpoutLogNotice("Spout::InitReceiver(%s, %dx%d)", SenderName, width, height);

	// Create a named sender mutex for access to the sender's shared texture
	frame.CreateAccessMutex(SenderName);

	// Enable frame counting to get the sender frame number and fps
	frame.EnableFrameCount(SenderName);

	// Set class globals
	strcpy_s(m_SenderName, 256, SenderName);

	m_Width = width;
	m_Height = height;
	m_dwFormat = dwFormat;
	m_DX11format = (DXGI_FORMAT)m_dwFormat;
	m_bInitialized = true;

	// Receiver rather than sender
	m_bSender = false;

}

//---------------------------------------------------------
bool Spout::ReceiveSenderData()
{
	m_bUpdated = false;

	// Initialization is recorded in this class for sender or receiver
	// m_Width or m_Height are established when the receiver connects to a sender

	// An existing connection will have a sender name
	// Or a name may be set for the receiver to connect to
	char sendername[256]{};
	strcpy_s(sendername, 256, m_SenderName);

	// Find the active sender if the global sender name is null
	if (sendername[0] == 0) {
		if (!GetActiveSender(sendername)) {
			return false; // No sender
		}
	}

	// If SpoutPanel or SpoutMessgeBox have been opened and a 
	// sender selected, the active sender name could be different.
	// Retrieve the new name but do not clear the receiver setup name.
	CheckSpoutPanel(sendername, 256);

	// Now we have either an existing sender name or the active sender name
	// Save current sender name and dimensions to test for change
	unsigned int width = m_Width;
	unsigned int height = m_Height;
	DWORD dwFormat = m_dwFormat;
	HANDLE dxShareHandle = m_dxShareHandle;

	// Retrieve the sender information : width, height, sharehandle and format.
	SharedTextureInfo info;
	if (sendernames.getSharedInfo(sendername, &info)) {

		width = info.width;
		height = info.height;
		dxShareHandle = UIntToPtr(info.shareHandle);
		dwFormat = info.format;

		// The following flags are informative only
		// and may be removed in future release

		//
		// 32 bit partner ID field
		//
		// GPU texture share and hardware GL/DX compatible by default
		m_bSenderCPU  = false;
		m_bSenderGLDX = true;

		// Top bit
		//   o Sender is using CPU share methods
		if (info.partnerId & 0x80000000) {
			m_bSenderCPU = true;
		}

		//
		// Next top bit
		//   o Sender hardware is GL/DX compatible
		//   o Using texture share methods
		if (info.partnerId & 0x40000000) {
			m_bSenderGLDX = true;
		}

		//
		// Both bits set (and none other)
		//   o Sender is using CPU share methods
		//   o Sender hardware is GL/DX compatible
		if (info.partnerId == 0xC0000000) {
			m_bSenderCPU = true;
			m_bSenderGLDX = true;
		}

		// Compatible DX9 formats
		// 21 =	D3DFMT_A8R8G8B8
		// 22 = D3DFMT_X8R8G8B8
		if (dwFormat == 21 || dwFormat == 22) {
			// Create a DX11 receiving texture with compatible format
			dwFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
		}

		// The shared texture handle will be different
		//   o for texture size or format change
		//   o for a new sender
		// Open the sender share handle to produce a new received texture
		if (dxShareHandle != m_dxShareHandle || strcmp(sendername, m_SenderName) != 0) {

			// Release everything to start again
			ReleaseReceiver();

			// Update the sender share handle
			m_dxShareHandle = dxShareHandle;

			// Get a new shared texture pointer from the share handle
			if (m_dxShareHandle) {

				if(spoutdx.OpenDX11shareHandle(spoutdx.GetDX11Device(), &m_pSharedTexture, dxShareHandle)) {

					// Get the texture details
					D3D11_TEXTURE2D_DESC desc{};
					m_pSharedTexture->GetDesc(&desc);

					// Check for zero size
					if (desc.Width == 0 || desc.Height == 0) {
						return false;
					}

					// For incorrect sender information, use dimensions and format
					// of the D3D11 texture generated by OpenDX11shareHandle
					if (width    != (DWORD)desc.Width)	width = (DWORD)desc.Width;
					if (height   != (DWORD)desc.Height) height = (DWORD)desc.Height;
					if (dwFormat != (DWORD)desc.Format) dwFormat = (DWORD)desc.Format;

					// Check for texture format supported by OpenGL/DirectX interop
					if (!(dwFormat == D3DFMT_A8R8G8B8						// 21
						|| dwFormat == D3DFMT_X8R8G8B8						// 22
						|| dwFormat == DXGI_FORMAT_B8G8R8X8_UNORM			// 88
						|| dwFormat == DXGI_FORMAT_B8G8R8A8_UNORM			// 22
						|| dwFormat == DXGI_FORMAT_R8G8B8A8_SNORM			// 31
						|| dwFormat == DXGI_FORMAT_R8G8B8A8_UNORM			// 28
						|| dwFormat == DXGI_FORMAT_R10G10B10A2_UNORM		// 24
						|| dwFormat == DXGI_FORMAT_R16G16B16A16_SNORM		// 13
						|| dwFormat == DXGI_FORMAT_R16G16B16A16_UNORM		// 11
						|| dwFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB		// 29
						|| dwFormat == DXGI_FORMAT_R16G16B16A16_FLOAT		// 10
						|| dwFormat == DXGI_FORMAT_R32G32B32A32_FLOAT)) {	// 2
						SpoutLogError("Spout::ReceiveSenderData - texture %dx%d incompatible texture format 0x%X (%d)",
							width, height, dwFormat, dwFormat);
						return false;
					}

					// If the received texture is successfully updated, initialize again
					// with the new sender name, width, height and format
					InitReceiver(sendername, width, height, dwFormat);

					// The application can now access and copy the sender texture.
					// Signal the application to update the receiving texture or image
					m_bUpdated = true;

				} // endif OpenDX11shareHandle succeeded
				else {
					//
					// Error log generated in OpenDX11shareHandle.
					//
					// OpenDX11shareHandle uses OpenSharedResource, which can fail if the sender and receiver 
					// applications are using different graphics adapters, which is possible if the user has 
					// specified different performance preferences using Windows Display Graphics settings.
					//
					// Graphics performance preference is only effective for a laptop with multiple graphics.
					// Performance settings can be accessed directly from SpoutSettings Version 1.046 and later.
					//
					// Do not inform the application of texture update, but retain the share handle so we don't 
					// query the same sender again.
					//
					// Return true and wait until another sender is selected or the shared texture handle is valid.
					// m_pSharedTexture is then null but make sure so that it is not used.
					if (m_pSharedTexture) 
						spoutdx.ReleaseDX11Texture(GetDX11Device(), m_pSharedTexture);
					m_pSharedTexture = nullptr;

				} // endif OpenDX11shareHandle fail

			} // endif m_dxShareHandle valid
			// For a null share handle from a 2.006 memoryshare sender
			// ReceiveTexture and ReceiveImage will look for the shared memory map
		} // endif changed share handle or sender name
		return true;
	} // endif find sender

	// There is no sender or the connected sender closed
	return false;

}

//---------------------------------------------------------
// Check whether SpoutPanel opened and return the new sender name
bool Spout::CheckSpoutPanel(char *sendername, int maxchars)
{
	if (!sendername)
		return false;

	// If SpoutPanel has been activated, test if the user has clicked OK
	if (m_bSpoutPanelOpened) { // User has activated spout panel

		char newname[256]{};

		// Is SpoutPanel registered ?
		char path[MAX_PATH]{};
		if (!ReadPathFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\SpoutPanel", "InstallPath", path)) {
			// If not registered, SpoutMessageBox has been used
			// Pass back the active name
			if (sendernames.GetActiveSender(newname)) {
				strcpy_s(sendername, maxchars, newname);
				m_bSpoutPanelOpened = false;
				return true;
			}
			return false;
		}

		//
		// SpoutPanel
		//

		SharedTextureInfo TextureInfo{};
		HANDLE hMutex = NULL;
		DWORD dwExitCode = 0;
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
									// If not already registered
									if (!sendernames.FindSenderName(newname)) {
										// Register in the list of senders and make it the active sender
										sendernames.RegisterSenderName(newname);
										sendernames.SetActiveSender(newname);
									}
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
