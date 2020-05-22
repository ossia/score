/**

	spoutGLDXinterop.cpp

	See also - spoutDirectX, spoutSenderNames

	- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

		Copyright (c) 2014-2018, Lynn Jarvis. All rights reserved.

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
		========================

		15-07-14	- ReadTexturePixels - allowed for variable OpenGL format instead of RGB only.
					- Needs testing. 
					- TODO - variable gl format for WriteTexturePixels
		21.07.14	- removed local fbo and replaced with temporary fbo within
					  texture functions due to problems with Max / Jitter
		22-07-14	- added option for DX9 or DX11
		23-07-14	- cleanup of DX9 / DX11 functions
		29-07-14	- pass format 0 for DX9 sender
		31-07-14	- Corrected DrawTexture aspect argument
		13-08-14	- OpenGL texture retained on cleanup
		14-08-14	- Corrected texture delete without context
		16-08-14	- created DrawToSharedTexture
		18-08-14	- debugging with WriteTexture method
		 -- names class revision additions --
		19-08-14	- activated event locks
		01.09.14	- removed temp fbo for texture transfers and returned to use of a common fbo
					- delete texture and fbo on cleanup
					- set texture and fbo to zero on cleanup, otherwise errors in Jitter
					- changed to vertex array draw for DrawToSharedTexture
					- Removed PAINT message from OpenDirectX9 due to crash of sender in Magic
		03.09.14	- Replaced with UpdateWindow and limited to Resolume only.
					- Cleanup
		15.09.14	- corrected access lock for DrawToSharedTexture and ReadTexturePixels
		21.09.14	- mutex texture access locks
		23.09.14	- moved general mutex texture access lock to the SpoutDirectX class
		23.09.14	- test for DirectX 11 support in UseDX9, IsDX9 and OpenDirectX
		24.09.14	- save and restore fbo for read/write/drawto texture
		28.09.14	- Added GL format argument for WriteTexturePixels
					- Added bAlignment  (4 byte alignment) flag for WriteTexturePixels
					- Changed GLformat argument from int to GLenum in ReadTexturePixels
					- Changed default GLformat from GL_RGB to GL_RGBA in ReadTexturePixels
					- Added Host FBO argument for ReadTexture, DrawToSharedTexture, WriteTexture
		12.10.14	- cleaned up CreateInterop for sender updates
		15.10.14	- added safety release of texture in CreateDX9interop in case of previous application crash
		17.10.14	- Directx 11 release context before device
		21.10.14	- removed keyed mutex lock due to reported driver problems
		21.10.14	- Allow for compatible texture formats
					  DirectX 11 format 87, DirectX9 D3DFMT_X8R8G8B8, and the default D3DFMT_A8R8G8B8
		21.10.14	- Allow DirectX texture formats to be registered in CreateInterop
		24.10.14	- Fall back to DirectX 9 if DirectX11 init fails
		24.11.14	- removed context print statement in CleanupInterop
		23.12.14	- added host fbo arg to ReadTexturePixels
					  Changed readback method to glReadPixels
		04.02.15	- Changed header default to DirectX 9 instead of DirectX 11
		09.02.15	- added invert flag to DrawSharedTexture (default true with no args)
		12.02.15	- Changed OpenDirectX to check for Intel graphics and open DirectX 9 if present
		13.02.15	- OpenDirectX9 included SendMessageTimeout before attempting to get the fg window text
		14.02.15	- Used PathStripPath function requiring shlwapi.h - (see SpoutSDK.h)
		--
		21.05.15	- OpenDirectX - Intel auto detection removed and replaced by an installer with option for DX11
					- Programmer must SetDX9(true) for compilation using DirectX 11 functions
		29.05.15	- Included SetAdapter for multiple adapters - Franz Hildgen.
		01.06.15	- Read/Write DX9 mode from registry
		02.06.15	- Added GetAdapter, GetNumAdapters, GetAdapterName
		08.06.15	- removed dx9 flag from setadapter
		20.06.15	- removed Intel / Optimus graphics detecion from GetAdapterInfo
		08.07.15	- Only reads registry for DX9 mode but does not write it
		25.08.15	- moved release texture before release of device - to be checked for a receiver
		26.08.15	- set the executable path to the sender's shared info
					- Added GetHostPath to retrieve the path of the host that produced the sender
		28.08.15	- Introduced RedrawWindow again instead of WM_PAINT due to crash with Windows 10
					  Window invalidate and redraw works OK - Win 7 32bit.
		01.09.15	- added MessageBox error warnings in CreateInterop and SpoutGLextensions::LoadGLextensions
		12.09.15	- Finalised revised SpoutMemoryShare class and functions
		14.09.15	- ReadTexture / WriteTexture change default invert to true in line with Spout class default
		15.09.15	- GetMemoryShare - do not return memoryshare true if the 2.005 installer has not set the 
					  "MemoryShare" key to avoid problems with 2.004 apps.
		21.09.15	- Change SetMemoryShareMode to apply only if there is a 2.005 installation or later
		22.09.15	- fixed source pointer start offset for memoryshare flip vertically between pBuf and pixels
		24.09.15	- Removed Enable/Disable texture target from texture bind. When a texture is first bound,
					  it assumes the specified target and we pass the required target already.
					  Problem noted with Cinder in memorshare mode.
		25.09.15	- Changed SetMemoryShare to allow for true or false.
		09.10.15	- TODO : check invert defaults for WriteTexture, WriteMemory, DrawSharedTexture, DrawToSharedTexture
					- Removed FlipVertical function - now unused with 2.005
		10.10.15	- Created DrawSharedMemory and DrawToSharedMemory - to be tested
					- made m_dxShareHandle public
		11.10.15	- Protect aganst NULL texture in ReadMemory
		12.10.15	- Add glCheckFramebufferStatusEXT to ReadTexture
		15.10.15	- Add PrintFBOstatus function
					- Add glCheckFramebufferStatusEXT and error report to all functions using glFramebufferTexture2DEXT
		22.10.15	- Moved DX11available from the directx class and changed to a test of DX11 device open
					  rather than operating system detection
		14.11.15	- changed functions to "const char *" where required
		20.11.15	- Registry read/write moved to SpoutDirectX class
		25.02.16	- Introduced read of MaxSenders from the registry for 2.005
		10.03.16	- introduced try / catch for memoryshare copymemory function
		16.03.16	- alignment 1 pixel for GL_RGB and GL_BGR_EXT in WriteTexturePixels and ReadTexturePixels
		17.03.16	- added bBGRAavailable flag to indicate whether BGRA extensions are supported
					- added function IsBGRAavailable to retrieve availability flag
					- changed WriteTexturePixels and WriteMemoryPixels to const unsigned char for pixel buffer
		21.03.16	- Added glFormat, bInvert and hostfbo to WriteMemoryPixels
					  and changed to use a local OpenGL texture for data conversion and flip
					- Changed ReadMemory and WriteMemory to use a local OpenGL texture
					- Added CopyTexture function
					- Added buffer flip and format conversion as utilities 
		28.03.16	- Added bGLDXavailable and switch to memoryshare in LoadGLextensions
		04.04.16	- Texture copy functions revised
					  Changed WriteTexture, ReadTexture and CopyTexture
					  to always use fbo blit if blit extensions are available
		19.04.16	- used glTexSubImage2D in WriteTexturePixels and ReadMemory
		27.04.16	- PBO functions for pixels transfer
					  LoadTexturePixels and UnloadTexturePixels used if PBO extensions available
					  in ReadTexturePixels and WriteTexturePixels
		28.04.16	- variable format for LoadTexturePixels
		01.05.16	- pbo functions for memoryshare ReadMemory and WriteMemory
		03.05.16	- SetPBOavailable(true/false) added to enable/disable pbo functions
		07.05.16	- SetPBOavailable changed to SetBufferMode
		22.05.16	- CleanupDirectX in interop cleanup
		09.06.16	- Corrected interop and mutex lock checks for fail in all functions
		16.06.16	- Added WriteDX9surface
		18.06.16	- Add invert to ReadTexturePixels
		23.06.16	- change back to 2.004 logic for mutex and interop lock checks
					- Mutex or interop lock fail does not mean read failure, but just no access
					  The current texture is re-used for a missed frame.
		03.07-16	- Use helper functions for conversion of 64bit HANDLE to unsigned __int32
					  and unsigned __int32 to 64bit HANDLE
					  https://msdn.microsoft.com/en-us/library/aa384267%28VS.85%29.aspx
		09.07-16	- Rebuild with VS2015
		14.07.16	- CreateDX11interop - release the texture not the device
		16.07.16	- Added exit flag to CleanupDirectX to avoid releasing device
					  Added immediatecontext flush to CleanupDX11
					  Restored wglDXUnregisterObjectNV to SpoutCleanup for DX9

		27.07.16	- Assembler memory copy functions for optimum speed for PBO and memoryshare
					  CopyImage instead of memcpy to support assembler functions
		16.08.16	- removed LoadTexturePixels - PBO upload - no performance advantage
		18.08.16	- moved memory copy functions to a separate "SpoutCopy" class
		20.08.16	- introduced SpoutCopy class 
					      CopyPixels with options for assembler functions
						  rgb/rgba <> bgr/bgra conversions moved from this class
		17.09.16	- removed CheckSpout2004()
					- introduced GetSpoutVersion() - returns version number for 2.005 and greater
					- restored LoadTexturePixels for staging texture data copy
					- introduced GetBufferMode and included in constructore
					- SetBufferMode is used by SpoutDXmode and check for PBO extension availability is made then

		04.01.16	- Added invert argument to UnloadTexturePixels and LoadTexturePixels
					- Finalise and test all CPU texture data access functions for DX11 and DX9
					- Included CS_OWNDC in InitOpenGL
					- Reset m_fbo etc to zero in CleanupInterop even if there is no OpenGL context
		05.01.17	- Initialize m_TextureInfo in constructor
					- Return false on fbo error in DrawToSharedMemory
					- Fixed target for texture unbind in CopyTexture
		06.01.17	- Use class variables and properly kill OpenGL window for InitOpenGL/CloseOpenGL
					- Change existing texture function names to GLDX for common selective function
		07.01.17	- Add SetCPUmode.
					- Add registry write to SetMemoryShareMode
		10.01.17	- Add FlushWait() function for Read DX11 texture
					- Add GetShareMode()
		13.01.17	- Remove interop compatibility test from GetMemoryShareMode
					- LoadExtensions - use CPU texture access if not interop compatible
					- GLDXcompatible - use memoryshare if DirectX did not load
					- UseDX9 - write result to registry
					- SetDX9 - return false if registry write failed
		15.01.17	- change GetShareMode to return : 0 - texture, 1 - cpu, 2 - memory
		22.01.17	- use DirectX texture format to create staging texture and surfaces
					- change messagebox errors to identify "SPOUT"
					- CleanupInterop after sender creation fail
					- CleanupDX9 change to prevent crash with Milkdrop
					- add pQuery->Release() to FlushWait
		04.02.17	- corrected test for fbo blit extension
		11.11.18	- Correct release of DX11 immediate context
					  TODO : DX9 leak checking
		12.11.18	- Always release DX9 device. Fix Milkdrop crash.

*/

#include "SpoutGLDXinterop.h"

spoutGLDXinterop::spoutGLDXinterop() {

	m_hWnd           = NULL;
	m_hInteropObject = NULL;
	m_hSharedMemory  = NULL;
	m_hInteropDevice = NULL;
	m_hAccessMutex   = NULL;

	m_glTexture = 0;
	m_fbo       = 0;
	m_TexID     = 0;
	m_TexWidth  = 0;
	m_TexHeight = 0;

	m_TextureInfo.width       = 0;
	m_TextureInfo.height      = 0;
	m_TextureInfo.format      = 0;
	m_TextureInfo.partnerId   = 0;
	m_TextureInfo.usage       = 0;
	m_TextureInfo.shareHandle = 0;

	// For InitOpenGL and CloseOpenGL
	m_hdc = NULL;
	m_hwndButton = NULL;
	m_hRc = NULL;
	
	// DX9
	m_bUseDX9    = false; // Use DX11 (default false) or DX9 (true)
	m_bUseCPU    = false; // CPU texture processing
	m_bUseMemory = false; // Memoryshare
	m_pD3D       = NULL;
	m_pDevice    = NULL;
	m_dxTexture  = NULL;
	DX9format    = D3DFMT_A8R8G8B8; // default format for DX9 (21)
	
	// DX11
	g_pd3dDevice        = NULL;
	g_pSharedTexture    = NULL;
	g_pImmediateContext = NULL;
	g_driverType        = D3D_DRIVER_TYPE_NULL;
	g_featureLevel      = D3D_FEATURE_LEVEL_11_0;
	DX11format          = DXGI_FORMAT_B8G8R8A8_UNORM; // Default compatible with DX9

	g_pStagingTexture   = NULL; // DX11 staging texture
	g_DX9surface        = NULL; // DX9 texture surface in CPU memory

	m_bInitialized      = false;
	m_bBGRAavailable    = false;
	m_bExtensionsLoaded	= false;
	m_bFBOavailable     = false;
	m_bBLITavailable    = false;
	m_bPBOavailable     = false;
	m_bSWAPavailable    = false;
	m_bGLDXavailable    = false;

	// Get mode flags from the registry
	// User can set the modes using SpoutDXmode

	// DX9
	DWORD dwDX9 = 0;
	spoutdx.ReadDwordFromRegistry(&dwDX9, "Software\\Leading Edge\\Spout", "DX9");
	m_bUseDX9 = (dwDX9 == 1); // Set the global bUseDX9 flag in this class

	// CPU texture processing - 2.006 or greater required
	m_bUseCPU = false; 
	DWORD dwCPU = 0;
	if(spoutdx.ReadDwordFromRegistry(&dwCPU, "Software\\Leading Edge\\Spout", "CPU")) {
		// Set the global m_bUseCPU flag in this class to the user setting for 2.006 and greater
		m_bUseCPU = (dwCPU == 1);
	}

	// Memoryshare
	// User selection of Memoryshare depends on 2.004 SpoutDirectX or 2.005 SpoutDXmode.
	// 2.004 apps will not have the registry set by the installer and memoryshare is incompatible.
	// If the hardware is not interop compatible they will fail to work.
	m_bUseMemory = false;
	DWORD dwMemory = 0;
	if(spoutdx.ReadDwordFromRegistry(&dwMemory, "Software\\Leading Edge\\Spout", "MemoryShare")) {
		// Set the global m_bUseMemory flag in this class to the user setting for 2.005 and greater
		m_bUseMemory = (dwMemory == 1);
	}

	// PBO support
	PboIndex     = 0;
	NextPboIndex = 0;
	m_pbo[0]     = NULL;
	m_pbo[1]     = NULL;
	
	// Check the mode currently in the registry
	// PBO extension availability is checked by SetBufferMode 
	// and when the user selects Buffering from SpoutDXmode
	m_bPBOavailable = false; 
	DWORD dwMode = 0;
	if(spoutdx.ReadDwordFromRegistry(&dwMode, "Software\\Leading Edge\\Spout", "Buffering")) {
		m_bPBOavailable = (dwMode == 1);
	}

	// 24.02.16 - max senders - testing only
	// Retrieve max senders from the registry
	// Depends on the 2.005 SpoutDirectX utility "MemoryShare" checkbox and
	// the registry key set by the >2.005 installer which won't be present for 2.004 apps
	DWORD dwSenders = 10;
	if(spoutdx.ReadDwordFromRegistry(&dwSenders, "Software\\Leading Edge\\Spout", "MaxSenders")) {
		// printf("SpoutGLDXinterop - 2.005 - max senders = %d\n", dwSenders);
		senders.SetMaxSenders((int)dwSenders);
	}


}

spoutGLDXinterop::~spoutGLDXinterop() {
	// Because cleanup is not here it has to be specifically called
	// This is because it can crash on exit - see cleanup for details
	m_bInitialized = false;
}


// For external access so that the local global variables are used
bool spoutGLDXinterop::OpenDirectX(HWND hWnd, bool bDX9)
{
	// If user set DX9 then just use it.
	// Otherwise check for DirectX 11 availability.

	// DX11available tests whether DX11 intitalization succeeds
	// and if not will then switch to DirectX 9
	if(bDX9 || !DX11available()) {
		m_bUseDX9 = true;
		return (OpenDirectX9(hWnd));
	}

	// Open DX11	
	if(OpenDirectX11()) {
		m_bUseDX9 = false; // Set to indicate intialized as DX11
		// Return here if OK
		return true;
	}

	return false;
}


bool spoutGLDXinterop::OpenDeviceKey(const char* key, int maxsize, char *description, char *version)
{
	// Extract the subkey from the DeviceKey string
	HKEY hRegKey;
	DWORD dwSize, dwKey;  
	char output[256];
	strcpy_s(output, 256, key);
	string SubKey = strstr(output, "System");

	// Convert all slash to double slash using a C++ string function
	// to get subkey string required to extract registry information
	for (unsigned int i=0; i<SubKey.length(); i++) {
		if (SubKey[i] == '\\') {
			SubKey.insert(i, 1, '\\');
			++i; // Skip inserted char
		}
	}

	// Open the key to find the adapter details
	if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, SubKey.c_str(), NULL, KEY_READ, &hRegKey) == 0) { 
		dwSize = MAX_PATH;
		// Adapter name
		if(RegQueryValueExA(hRegKey, "DriverDesc", NULL, &dwKey, (BYTE*)output, &dwSize) == 0) {
			strcpy_s(description, maxsize, output);
		}
		if(RegQueryValueExA(hRegKey, "DriverVersion", NULL, &dwKey, (BYTE*)output, &dwSize) == 0) {
			// Find the last 6 characters of the version string then
			// convert to a float and multiply to get decimal in the right place
			sprintf_s(output, 256, "%5.2f", atof(output + strlen(output)-6)*100.0);
			strcpy_s(version, maxsize, output);
		} // endif DriverVersion
		RegCloseKey(hRegKey);
	} // endif RegOpenKey

	return true;
}

void spoutGLDXinterop::trim(char * s) {
    char * p = s;
    int l = (int)strlen(p);

    while(isspace(p[l - 1])) p[--l] = 0;
    while(* p && isspace(* p)) ++p, --l;

    memmove(s, p, l + 1);
}


// this function initializes and prepares Direct3D
bool spoutGLDXinterop::OpenDirectX9(HWND hWnd)
{
	HWND fgWnd = NULL;
	char fgwndName[MAX_PATH];

	// Already initialized ?
	if(m_pD3D != NULL) {
		return true;
	}

	// Create a IDirect3D9Ex object if not already created
	if(!m_pD3D) {
		m_pD3D = spoutdx.CreateDX9object(); 
	}

	if(m_pD3D == NULL) {
		return false;
	}

	// Create a DX9 device
	if(!m_pDevice) {
		m_pDevice = spoutdx.CreateDX9device(m_pD3D, hWnd); 
	}

	if(m_pDevice == NULL) {
		return false;
	}

	// Problem for FFGL plugins - might be a problem for other FFGL hosts or applications.
	// DirectX 9 device initialization creates black areas and the host window has to be redrawn.
	// But this causes a crash for a sender in Magic when the render window size is changed.
	// Not a problem for DirectX 11.
	// Not needed in Isadora.
	// Needed for Resolume.
	// For now, limit this to Resolume only.

	fgWnd = GetForegroundWindow();
	if(fgWnd) {
		// SMTO_ABORTIFHUNG : The function returns without waiting for the time-out
		// period to elapse if the receiving thread appears to not respond or "hangs."
		if(SendMessageTimeoutA(fgWnd, WM_GETTEXT, MAX_PATH, (LPARAM)fgwndName, SMTO_ABORTIFHUNG, 128, NULL) != 0) {
			// Returns the full path - get just the window name
			PathStripPathA(fgwndName);
			if(fgwndName[0]) {
				if(strstr(fgwndName, "Resolume") != NULL // Is resolume in the window title ?
				&& strstr(fgwndName, "magic") == NULL) { // Make sure it is not a user named magic project.
					// DirectX device initialization needs the window to be redrawn (creates black areas)
					// 03.05.15 - user observation that UpDateWindow does not work and Resolume GUI is still corrupted
					// 28.08.15 - user observation of a crash with Windows 10 
					// try RedrawWindow again (with InvalidateRect as well) - confirmed working with Win 7 32bit
					// https://msdn.microsoft.com/en-us/library/windows/desktop/dd145213%28v=vs.85%29.aspx
					// The WM_PAINT message is generated by the system and should not be sent by an application.					
					// SendMessage(fgWnd, WM_PAINT, NULL, NULL ); // causes problems
					InvalidateRect(fgWnd, NULL, FALSE); // make sure
			        RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASENOW | RDW_INTERNALPAINT);
				}
			}
		}
	}

	return true;
}

// this function initializes and prepares Directx 11
bool spoutGLDXinterop::OpenDirectX11()
{
	// Quit if already initialized
	if(g_pd3dDevice != NULL) return true;

	// Create a DirectX 11 device
	if(!g_pd3dDevice) g_pd3dDevice = spoutdx.CreateDX11device();
	if(g_pd3dDevice == NULL) {
		return false;
	}
	// 11.11.18
	g_pImmediateContext = spoutdx.GetImmediateContext();

	return true;
}


// this function tests for DX11 capability
bool spoutGLDXinterop::DX11available()
{
	// Quit if DX11 is already initialized
	if(g_pd3dDevice != NULL) return true;

	// Try to create a DirectX 11 device
	ID3D11Device* pd3dDevice;
	pd3dDevice = spoutdx.CreateDX11device();
	if(pd3dDevice == NULL)
		return false;

	// 11.11.18
	// Clear state and flush context to prevent deferred device release
	spoutdx.GetImmediateContext()->ClearState();
	spoutdx.GetImmediateContext()->Flush();
	// Release the global immediate context in SpoutDirectX
	spoutdx.GetImmediateContext()->Release();

	// Close it because not initialized yet and is just a test
	ULONG refcount = pd3dDevice->Release();
	// printf("DX11available - device release refcount = %ld\n", refcount);

	return true;
}

// Must be called after DirectX initialization
//
// https://code.google.com/p/chromium/issues/detail?id=106438
//
// NOTES : On a “normal” system EnumDisplayDevices and IDXGIAdapter::GetDesc always concur
// i.e. the device that owns the head will be the device that performs the rendering. 
// On an Optimus system IDXGIAdapter::GetDesc will return whichever device has been selected for rendering.
// So on an Optimus system it is possible that IDXGIAdapter::GetDesc will return the dGPU whereas 
// EnumDisplayDevices will return the iGPU.
//
// This function compares the adapter descriptions of the two
// The string "Intel" reveals that it is an Intel device but 
// the Vendor ID could also be used
//
//	0x10DE	NVIDIA
//	0x163C	intel
//	0x8086  Intel
//	0x8087  Intel
//
bool spoutGLDXinterop::GetAdapterInfo(char *renderadapter, 
									  char *renderdescription, char *renderversion,
									  char *displaydescription, char *displayversion,
									  int maxsize, bool &bDX9)
{
	renderadapter[0] = 0; // DirectX adapter
	renderdescription[0] = 0;
	renderversion[0] = 0;
	displaydescription[0] = 0;
	displayversion[0] = 0;

	if(bDX9) {
		if(m_pDevice == NULL) { printf("No DX9 device\n"); return false; }

		D3DADAPTER_IDENTIFIER9 adapterinfo;
		// char            Driver[MAX_DEVICE_IDENTIFIER_STRING];
		// char            Description[MAX_DEVICE_IDENTIFIER_STRING];
		// char            DeviceName[32];         // Device name for GDI (ex. \\.\DISPLAY1)
		// LARGE_INTEGER   DriverVersion;          // Defined for 32 bit components
		// DWORD           VendorId;
		// DWORD           DeviceId;
		// DWORD           SubSysId;
		// DWORD           Revision;
		// GUID            DeviceIdentifier;
		// DWORD           WHQLLevel;
		m_pD3D->GetAdapterIdentifier (D3DADAPTER_DEFAULT, 0, &adapterinfo);
		// printf("Driver = [%s]\n", adapterinfo.Driver);
		// printf("    Description = [%s]\n", adapterinfo.Description);
		// printf("DeviceName = [%s]\n", adapterinfo.DeviceName);
		// printf("DriverVersion = [%d] [%x]\n", adapterinfo.DriverVersion, adapterinfo.DriverVersion);
		// printf("VendorId = [%d] [%x]\n", adapterinfo.VendorId, adapterinfo.VendorId);
		// printf("DeviceId = [%d] [%x]\n", adapterinfo.DeviceId, adapterinfo.DeviceId);
		// printf("SubSysId = [%d] [%x]\n", adapterinfo.SubSysId, adapterinfo.SubSysId);
		// printf("Revision = [%d] [%x]\n", adapterinfo.Revision, adapterinfo.Revision);
		strcpy_s(renderadapter, maxsize, adapterinfo.Description);

	}
	else {
		if(g_pd3dDevice == NULL) { printf("No DX11 device\n"); return false; }

		IDXGIDevice * pDXGIDevice;
		g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
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
		strcpy_s(renderadapter, maxsize, output);
	}

	// DEBUG - default render adapter is the DirectX one ???
	if(renderadapter) {
		strcpy_s(renderdescription, maxsize, renderadapter);
	}

	// Use Windows functions to look for Intel graphics to see  if it is
	// the same render adapter that was detected with DirectX
	char driverdescription[256];
	char driverversion[256];
	char regkey[256];
	size_t charsConverted = 0;
	
	// Additional info
	DISPLAY_DEVICE DisplayDevice;
	DisplayDevice.cb = sizeof(DISPLAY_DEVICE);

	// 31.10.14 detect the adapter attached to the desktop
	// To query all display devices in the current session, 
	// call this function in a loop, starting with iDevNum set to 0, 
	// and incrementing iDevNum until the function fails. 
	// To select all display devices in the desktop, use only the display devices
	// that have the DISPLAY_DEVICE_ATTACHED_TO_DESKTOP flag in the DISPLAY_DEVICE structure.

	int nDevices = 0;
	for(int i=0; i<10; i++) { // should be much less than 10 adapters
		if(EnumDisplayDevices(NULL, i, &DisplayDevice, 0)) {
			// This will list all the devices
			nDevices++;
			// Get the registry key
			wcstombs_s( &charsConverted, regkey, 129, (const wchar_t *)DisplayDevice.DeviceKey, 128);
			// This is the registry key with all the information about the adapter
			OpenDeviceKey(regkey, 256, driverdescription, driverversion);
			// Is it a render adapter ?
			if(renderadapter && strcmp(driverdescription, renderadapter) == 0) {
				// printf("Windows render adapter matches : [%s] Vers [%s]\n", driverdescription, driverversion);
				strcpy_s(renderdescription, maxsize, driverdescription);
				strcpy_s(renderversion, maxsize, driverversion);
			}
			// Is it a display adapter
			if(DisplayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) {
				// printf("Display adapter : [%s] Vers: %s ", driverdescription, driverversion);
				strcpy_s(displaydescription, 256, driverdescription);
				strcpy_s(displayversion, 256, driverversion);
				// printf("(Attached to desktop)\n");
			} // endif attached to desktop

		} // endif EnumDisplayDevices
	} // end search loop

	// The render adapter
	if(renderdescription) trim(renderdescription);

	return true;
}


//
// TODO - check for memoryshare mode - should not be called
bool spoutGLDXinterop::CreateInterop(HWND hWnd, const char* sendername, unsigned int width, unsigned int height, DWORD dwFormat, bool bReceive)
{
	bool bRet = true;
	DWORD format;

	// Needs an openGL context to work
	if(!wglGetCurrentContext()) {
		// MessageBoxA(NULL, "CreateInterop - no GL context", "SPOUT", MB_OK|MB_ICONEXCLAMATION);
		return false;
	}

	// printf("CreateInterop - %dx%d - dwFormat (%d) \n", width, height, dwFormat);

	//
	// Texture format tests
	//
	// DX9 compatible formats
	// DXGI_FORMAT_R8G8B8A8_UNORM; // default DX11 format - compatible with DX9 (28)
	// DXGI_FORMAT_B8G8R8A8_UNORM; // compatible DX11 format - works with DX9 (87)
	// DXGI_FORMAT_B8G8R8X8_UNORM; // compatible DX11 format - works with DX9 (88)
	//
	// Other formats that work with DX11 but not with DX9
	// DXGI_FORMAT_R16G16B16A16_FLOAT
	// DXGI_FORMAT_R16G16B16A16_SNORM
	// DXGI_FORMAT_R10G10B10A2_UNORM
	//
	// To change any of these you can use :
	//
	// void spoutGLDXinterop::SetDX11format(DXGI_FORMAT textureformat)
	//
	// Allow for compatible DirectX 11 senders (format 87)
	// And compatible DirectX9 senders D3DFMT_X8R8G8B8 - 22
	// and the default D3DFMT_A8R8G8B8 - 21
	if(m_bUseDX9) {
		// printf("CreateInterop - DX9 mode\n"); 
		// DirectX 9
		if(dwFormat > 0) {
			if(dwFormat == 87) {
				// printf("CreateInterop - DX9 mode - compatible DX11 user format (%d), creating DX9 D3DFMT_A8R8G8B8 texture\n", dwFormat);
				format = (DWORD)D3DFMT_A8R8G8B8; // (21)
			}
			else if(dwFormat == D3DFMT_X8R8G8B8 || dwFormat == D3DFMT_A8R8G8B8) {
				// printf("CreateInterop - DX9 mode - compatible DX9 user format (%d) \n", dwFormat); 
				format = (DWORD)dwFormat; // (22)
			}
			else {
				// printf("CreateInterop - DX9 mode - incompatible user format (%d) \n", dwFormat);
				return false;
			}
			SetDX9format((D3DFORMAT)format); // Set the global texture format
		}
		else { // format is passed as zero so we assume a DX9 sender D3DFMT_A8R8G8B8
			format = (DWORD)DX9format;
			// printf("CreateInterop - DX9 mode - DX9format (%d) \n", format);
		}
	}
	else {
		// printf("CreateInterop - DX11 mode\n"); 
		// DirectX 11
		// Is this a DX11 or a DX9 sender texture?
		// A directX 11 receiver accepts DX9 formats
		if(!bReceive && dwFormat > 0) {
			// printf("CreateInterop - DX11 sender - user format %d \n", dwFormat);
			format = (DXGI_FORMAT)dwFormat;
			SetDX11format((DXGI_FORMAT)format); // Set the global texture format
		}
		else {
			// printf("CreateInterop - default DX11 format BGRA - format passed = %d \n", dwFormat);
			format = (DWORD)DX11format; // DXGI_FORMAT_B8G8R8A8_UNORM (87) default compatible with DX9
		}
	}

	// Quit now if the receiver can't access the shared memory info of the sender
	// Otherwise m_dxShareHandle is set by getSharedTextureInfo and is the
	// shared texture handle of the Sender texture
	if (bReceive && !getSharedTextureInfo(sendername)) {
		MessageBoxA(NULL,"Cannot retrieve sender information.","SPOUT",MB_OK|MB_ICONEXCLAMATION);
		printf("CreateInterop error 1\n");
		return false;
	}

	// printf("Sender texture format = %d, sharehanlde = %x \n", m_TextureInfo.format, m_TextureInfo.shareHandle);

	// Check the sender format for a DX9 receiver
	// It can only be from a DX9 sender (format 0, 22, 21)
	// or from a compatible DX11 sender (format 87)
	if(bReceive && m_bUseDX9) {
		if(!(m_TextureInfo.format == 0 
			|| m_TextureInfo.format == 22
			|| m_TextureInfo.format == 21
			|| m_TextureInfo.format == 87)) {
			// printf("Incompatible sender texture format %d \n", m_TextureInfo.format);
			MessageBoxA(NULL,"Incompatible sender texture format.","SPOUT",MB_OK|MB_ICONEXCLAMATION);
			return false;
		}
	}

	// printf("CreateInterop 1\n");

	// Make sure DirectX has been initialized
	// Creates a global pointer to the DirectX device (DX11 g_pd3dDevice or DX9 m_pDevice)
	if(!OpenDirectX(hWnd, m_bUseDX9)) {
		MessageBoxA(NULL,"Cannot open DirectX.","SPOUT",MB_OK|MB_ICONEXCLAMATION);
		printf("CreateInterop error 2\n");
		return false;
	}

	// Create an fbo for copying textures
	if(m_fbo) {
		// Delete the fbo before the texture so that any texture attachment is released
		glDeleteFramebuffersEXT(1, &m_fbo);
		m_fbo = 0;
	}
	glGenFramebuffersEXT(1, &m_fbo); 

	// Create a local opengl texture that will be linked to a shared DirectX texture
	// This is never initialized using OpenGL, but has size and can be accessed when
	// it is linked to the shared DirectX texture with the GL/DX Interop.
	if(m_glTexture) {
		glDeleteTextures(1, &m_glTexture);
		m_glTexture = 0;
	}
	glGenTextures(1, &m_glTexture);

	// printf("CreateInterop 5 (format = %d)\n", format);

	// Create textures and GL/DX interop objects
	// for GL/DX Interop or CPU texture access mode
	if(m_bUseDX9)
		bRet = CreateDX9interop(width, height, format, bReceive);
	else
		bRet = CreateDX11interop(width, height, format, bReceive);

	if(!bRet) {
		printf("CreateInterop error 3\n");
		MessageBoxA(NULL,"Cannot create DirectX/OpenGL interop","SPOUT",MB_OK|MB_ICONEXCLAMATION);
		CleanupInterop(); // 20.11.15 - release everything
		return false;
	}

	// Now the global shared texture handle - m_dxShareHandle - has been set so a sender can be created
	// this creates the sender shared memory map and registers the sender
	if (!bReceive) {
		// We are done with the format
		// So for DirectX 9, set to zero to identify the sender as DirectX 9
		// Allow the sender format to be registered becasue it is tested
		// by SpoutPanel and by the texture formats above
		if(!senders.CreateSender(sendername, width, height, m_dxShareHandle, format)) {
			printf("CreateInterop error 4\n");
			MessageBoxA(NULL,"Cannot create Spout sender.","SPOUT",MB_OK|MB_ICONEXCLAMATION);
			CleanupInterop(); // 22.01.17 - Release
			return false;
		}

	}

	// printf("CreateInterop 6\n");

	// Set up local values for this instance
	// Needed for texture read and write size checks
	m_TextureInfo.width       = (unsigned __int32)width;
	m_TextureInfo.height      = (unsigned __int32)height;
#ifdef _M_X64
	m_TextureInfo.shareHandle = (unsigned __int32)(HandleToLong(m_dxShareHandle));
#else
	m_TextureInfo.shareHandle = (unsigned __int32)m_dxShareHandle;
#endif
	// Additional unused fields available
	// DWORD usage; // texture usage
	// wchar_t description[128]; // Wyhon compatible description
	// unsigned __int32 partnerId; // Wyphon id of partner that shared it with us (not unused)
	// 26.08.15 - set the executable path to the sender's shared info (not documented and could be removed)
	if(!bReceive) {
		SharedTextureInfo info;
		// Access the info directly from the memory map to include the description string
		if(senders.getSharedInfo(sendername, &info)) {
			char exepath[256];
			GetModuleFileNameA(NULL, exepath, sizeof(exepath));
			// Description is defined as wide chars, but the path is stored as byte chars
			strcpy_s((char *)info.description, 256, exepath);
			senders.setSharedInfo(sendername, &info);
		}
	}

	// Initialize general texture transfer sync mutex - either sender or receiver can do this
	spoutdx.CreateAccessMutex(sendername, m_hAccessMutex);

	//
	// Now we have globals for this instance
	//
	// m_TextureInfo.width			- width of the shared DirectX texture
	// m_TextureInfo.height			- height of the shared DirectX texture
	// m_TextureInfo.shareHandle	- handle of the shared DirectX texture
	// m_TextureInfo.format			- format of the shared DirectX texture
	// m_TextureInfo.description    - path of the executable that created the sender

	// m_glTexture					- a linked opengl texture (empty for CPU mode)
	// m_dxTexture					- a linked, shared DirectX texture created here for both GL/DX and CPU mode
	// m_hInteropDevice				- handle to interop device created by wglDXOpenDeviceNV by init
	// m_hInteropObject				- handle to the connected texture created by wglDXRegisterObjectNV
	// m_hAccessMutex				- mutex for texture access lock
	// m_bInitialized				- whether it initialized OK

	// true means the init was OK, not the connection
	return true; 

}


//
// =================== DX9 ===============================
//
//		CreateDX9interop()
//	
//		bReceive		when receiving a texture from a DX application this must be set to true (default)
//						when sending a texture from GL to the DX application, set to false
//
bool spoutGLDXinterop::CreateDX9interop(unsigned int width, unsigned int height, DWORD dwFormat, bool bReceive) 
{

	// printf("CreateDX9interop(%dx%d, [Format = %d], %d (m_pDevice = %x)\n", width, height, dwFormat, bReceive, m_pDevice);

	// The shared texture handle of the Sender texture "m_dxShareHandle" 
	// is already set by getSharedTextureInfo, but should be NULL for a sender
	if (!bReceive) {
		// printf("    sender - setting m_dxShareHandle to NULL\n");
		// Create a new shared DirectX resource m_dxTexture 
		// with new local handle m_dxShareHandle for a sender
		m_dxShareHandle = NULL; // A sender creates a new texture
	}
	
	// Safety in case an application has crashed
	if (m_dxTexture != NULL) {
		// printf("   releasing texture\n");
		m_dxTexture->Release();
	}
	m_dxTexture = NULL;

	// Create a shared DirectX9 texture - m_dxTexture
	// by giving it a sharehandle variable - m_dxShareHandle
	// For a SENDER : the sharehandle is NULL and a new texture is created
	// For a RECEIVER : the sharehandle is valid and becomes a handle to the existing shared texture
	// USAGE is D3DUSAGE_RENDERTARGET
	if(!spoutdx.CreateSharedDX9Texture(m_pDevice,
									   width,
									   height,
									   (D3DFORMAT)dwFormat,  // default is D3DFMT_A8R8G8B8
									   m_dxTexture,
									   m_dxShareHandle)) {
		printf("    CreateSharedDX9Texture failed\n");								   
		return false;
	}

	// printf("CreateDX9interop : m_dxTexture = [%x]\n");

	//
	// For CPU access to the shared texture a DX9 surface is created
	// and the GL/DX interop is not used
	//
	if(m_bUseCPU) {
		// DX9 texture surface in CPU memory
		if(g_DX9surface != NULL) {
			g_DX9surface->Release();
			g_DX9surface = NULL;
		}
		// Create a CPU accessable surface to get pixels from the shared texture
		HRESULT hr = m_pDevice->CreateOffscreenPlainSurface(width, height, (D3DFORMAT)dwFormat, D3DPOOL_SYSTEMMEM, &g_DX9surface, NULL);
		if(FAILED(hr)) {
			printf("    DX9 create texture surface failed\n");
			return false;
		}
	}
	else {
		// For the GL/DX interop, link the shared DirectX texture to the OpenGL texture
		// This registers for interop and associates the opengl texture with the dx texture
		// by calling wglDXRegisterObjectNV which returns a handle to the interop object
		// (the shared texture) (m_hInteropObject)
		// When a sender size changes, the new texture has to be re-registered
		if(m_hInteropDevice != NULL &&  m_hInteropObject != NULL) {
			// printf("CreateInterop - wglDXUnregisterObjectNV\n");
			wglDXUnregisterObjectNV(m_hInteropDevice, m_hInteropObject);
			m_hInteropObject = NULL;
		}
		m_hInteropObject = LinkGLDXtextures(m_pDevice, m_dxTexture, m_dxShareHandle, m_glTexture); 
		if(!m_hInteropObject) {
			printf("    DX9 LinkGLDXtextures failed\n");	
			return false;
		}
	}

	return true;
}



//
// =================== DX11 ==============================
//
bool spoutGLDXinterop::CreateDX11interop(unsigned int width, unsigned int height, DWORD dwFormat, bool bReceive ) 
{
	
	// printf("CreateDX11interop(%dx%d, [Format = %d], %d\n", width, height, dwFormat, bReceive);

	if (g_pSharedTexture != NULL) g_pSharedTexture->Release();
	g_pSharedTexture = NULL; // Important for checks for NULL

	// Create or use a shared DirectX texture that will be linked to the OpenGL texture
	// and get it's share handle for sharing textures
	if (bReceive) {
		// Retrieve the shared texture pointer via the sharehandle
		// printf("CreateDX11interop %x, %x\n", g_pd3dDevice, m_dxShareHandle);
		if(!spoutdx.OpenDX11shareHandle(g_pd3dDevice, &g_pSharedTexture, m_dxShareHandle)) {
			printf("    CreateDX11interop - error 1\n");
			return false;
		}
	} else {
		// printf("CreateDX11interop - creating texture %dx%d (g_pSharedTexture = %x)\n", width, height, g_pSharedTexture);
		// otherwise create a new shared DirectX resource g_pSharedTexture 
		// with local handle m_dxShareHandle for a sender
		m_dxShareHandle = NULL; // A sender creates a new texture with a new share handle
		if(!spoutdx.CreateSharedDX11Texture(g_pd3dDevice,
											width, height, 
											(DXGI_FORMAT)dwFormat, // default is DXGI_FORMAT_B8G8R8A8_UNORM
											&g_pSharedTexture, m_dxShareHandle)) {
			printf("    CreateDX11interop - error 2\n");
			return false;
		}
	}

	//
	// For CPU access to the shared texture, a staging texture is created
	// and the GL/DX interop is not used
	//
	if(m_bUseCPU) {
		// DX11 staging texture
		if(g_pStagingTexture != NULL) {
			g_pStagingTexture->Release();
			g_pStagingTexture = NULL;
			// Flush is needed or there is a GPU memory leak
			if (g_pImmediateContext)
				g_pImmediateContext->Flush();
		}
		if(!spoutdx.CreateDX11StagingTexture(g_pd3dDevice, width, height, DX11format, &g_pStagingTexture)) {
			printf("    DX11 create staging texture failed\n");	
			return false;
		}
	}
	else {
		// For the GL/DX interop, link the shared DirectX texture to the OpenGL texture
		// This registers for interop and associates the opengl texture with the dx texture
		// by calling wglDXRegisterObjectNV which returns a handle to the interop object
		// (the shared texture) (m_hInteropObject)
		// When a sender size changes, the new texture has to be re-registered
		if(m_hInteropDevice != NULL &&  m_hInteropObject != NULL) {
			// printf("CreateInterop - wglDXUnregisterObjectNV\n");
			wglDXUnregisterObjectNV(m_hInteropDevice, m_hInteropObject);
			m_hInteropObject = NULL;
		}
		m_hInteropObject = LinkGLDXtextures(g_pd3dDevice, g_pSharedTexture, m_dxShareHandle, m_glTexture); 
		if(!m_hInteropObject) {
			printf("    DX11 LinkGLDXtextures failed\n");	
			return false;
		}
	}

	return true;

}

//	Link a shared DirectX texture to an OpenGL texture
//	and create a GLDX interop object handle
//
//	IN	pSharedTexture  Pointer to shared the DirectX texture
//	IN	dxShareHandle   Handle of the DirectX texture to be shared
//	IN	glTextureID     ID of the OpenGL texture that is to be linked to the shared DirectX texture
//	Returns             Handle to the GL/DirectX interop object (the shared texture)
//
HANDLE spoutGLDXinterop::LinkGLDXtextures (	void* pDXdevice,
											void* pSharedTexture,
											HANDLE dxShareHandle,
											GLuint glTexture) 
{

	HANDLE hInteropObject;

	// printf("LinkGLDXtextures (%x, %x, %x, %x)\n", pDXdevice, pSharedTexture, dxShareHandle, glTexture);
	// printf("    m_hInteropDevice = %x\n", m_hInteropDevice);

	// Prepare the DirectX device for interoperability with OpenGL
	// The return value is a handle to a GL/DirectX interop device.
	if(!m_hInteropDevice) {
		// printf("    LinkGLDXtextures creating interop device from %x\n", pDXdevice);
		m_hInteropDevice = wglDXOpenDeviceNV(pDXdevice);
	}

	if (m_hInteropDevice == NULL) {
		printf("    LinkGLDXtextures error 1 : could not create interop device from pDXdevice\n");
		return NULL;
	}

	// prepare shared resource
	// wglDXSetResourceShareHandle does not need to be called for DirectX
	// version 10 and 11 resources. Calling this function for DirectX 10
	// and 11 resources is not an error but has no effect.
	if (!wglDXSetResourceShareHandleNV(pSharedTexture, dxShareHandle)) {
		printf("    LinkGLDXtextures error 2 : wglDXSetResourceShareHandleNV failed\n");
		return NULL;
	}

	// Prepare the DirectX texture for use by OpenGL
	// register for interop and associate the opengl texture with the dx texture
	hInteropObject = wglDXRegisterObjectNV( m_hInteropDevice,
											pSharedTexture,	// DX texture
											glTexture,		// OpenGL texture
											GL_TEXTURE_2D,	// Must be TEXTURE_2D - multisampling not supported
											WGL_ACCESS_READ_WRITE_NV); // We will write and the receiver will read

	if(!hInteropObject) {
		printf("    LinkGLDXtextures error 3 : wglDXRegisterObjectNV failed\n");
		// printf("LinkGLDXtextures (%x, %x, %x, %x)\n", pDXdevice, pSharedTexture, dxShareHandle, glTexture);
		// printf("    m_hInteropDevice = %x\n", m_hInteropDevice);	
	}

	return hInteropObject;

}


void spoutGLDXinterop::CleanupDirectX(bool bExit)
{
	if (m_bUseDX9)
		CleanupDX9(bExit);
	else
		CleanupDX11(bExit);
}

void spoutGLDXinterop::CleanupDX9(bool bExit)
{
	ULONG refcount = 0;

	if (m_pD3D != NULL) {
		// 01.09.14 - texture release was missing for a receiver - caused a VRAM leak
		// If an existing texture exists, CreateTexture can fail with and "unknown error"
		// 25.08.15 - moved before release of device
		if (m_dxTexture != NULL) {
			m_dxTexture->Release();
			m_dxTexture = NULL;
		}

		// DX9 texture surface in CPU memory
		if(g_DX9surface != NULL) {
			g_DX9surface->Release();
			g_DX9surface = NULL;
		}

		// if (bExit) {
			// 25.08.15 - release device before the object !
			// 22.01.17 - will crash if refcount is 1 for MilkDrop. TODO - why
			// 12.11.18 - must always be freed, not only on exit. Device recreated for a new sender.
			if (m_pDevice != NULL) 
				refcount = m_pDevice->Release();

			if (m_pD3D != NULL)
				refcount = m_pD3D->Release();
			// printf("DX9 release - refcount = %d\n", refcount);

			m_pDevice = NULL;
			m_pD3D = NULL;
		// }

	}

}

void spoutGLDXinterop::CleanupDX11(bool bExit)
{

	// printf("CleanupDX11 - g_pd3dDevice = %d\n", g_pd3dDevice);

	if (g_pd3dDevice != NULL) {
		if (g_pSharedTexture != NULL) {
			g_pSharedTexture->Release();
			g_pSharedTexture = NULL;
			// 14.07.16 - flush is needed or there is a GPU memory leak
			if (g_pImmediateContext) 
				g_pImmediateContext->Flush();
		}

		// DX11 staging texture
		if(g_pStagingTexture != NULL) {
			g_pStagingTexture->Release();
			g_pStagingTexture = NULL;
			if (g_pImmediateContext) 
				g_pImmediateContext->Flush();
		}

		// 11.11.18 - release device
		// if (bExit) {
			// Clear state and flush context to prevent deferred device release
			if (g_pImmediateContext) {
				g_pImmediateContext->ClearState();
				g_pImmediateContext->Flush();
				// Release the global immediate context in SpoutDirectX
				g_pImmediateContext->Release();
				g_pImmediateContext = NULL;
			}
			g_pd3dDevice->Release();
			g_pd3dDevice = NULL;
		// }
	}
}


// this is the function that cleans up Direct3D and the gldx interop
// The exit flag is a fix - trouble is with wglDXUnregisterObjectNV
// which crashes on exit to the program but not if called
// while the program is running. Likely due to no GL context on exit
void spoutGLDXinterop::CleanupInterop(bool bExit)
{
	HGLRC ctx = wglGetCurrentContext();

	// Some of these things need an opengl context so check
	if (ctx != NULL) {
		// Problem here on exit, but not on change of resolution while the program is running !?
		if (!bExit) {
			if (m_hInteropDevice != NULL && m_hInteropObject != NULL) {
				wglDXUnregisterObjectNV(m_hInteropDevice, m_hInteropObject);
				m_hInteropObject = NULL;
			}
		}

		if (m_hInteropDevice != NULL) {
			wglDXCloseDeviceNV(m_hInteropDevice);
			m_hInteropDevice = NULL;
		}

		if (m_fbo > 0) {
			// Delete the fbo before the texture so that any texture attachment 
			// is released even though it should have been
			glDeleteFramebuffersEXT(1, &m_fbo);
			m_fbo = 0;
		}

		if (m_pbo[0] > 0) {
			glDeleteBuffersEXT(2, m_pbo);
			m_pbo[0] = 0;
			m_pbo[1] = 0;
		}

		if (m_glTexture > 0) {
			glDeleteTextures(1, &m_glTexture);
			m_glTexture = 0;
		}

		if (m_TexID > 0) {
			glDeleteTextures(1, &m_TexID);
			m_TexID = 0;
			m_TexWidth = 0;
			m_TexHeight = 0;
		}

	} // endif there is an opengl context

	CleanupDirectX(bExit);

	// Close general texture access mutex
	spoutdx.CloseAccessMutex(m_hAccessMutex);
	m_hAccessMutex = NULL; // Double check that the global handle is NULL

	m_bInitialized = false;

}

//
//	Load the Nvidia gl/dx extensions
//
bool spoutGLDXinterop::LoadGLextensions() 
{
	m_caps = loadGLextensions(); // in spoutGLextensions

	char buffer [33];
	_itoa_s(m_caps, buffer, 2);

	if(m_caps == 0) {
		return false;
	}

	// GLEXT_SUPPORT_PBO - set by SetBufferMode()
	if(m_caps & GLEXT_SUPPORT_FBO)       m_bFBOavailable  = true;
	if(m_caps & GLEXT_SUPPORT_FBO_BLIT)  m_bBLITavailable = true;
	if(m_caps & GLEXT_SUPPORT_SWAP)      m_bSWAPavailable = true;
	if(m_caps & GLEXT_SUPPORT_BGRA)      m_bBGRAavailable = true;
	if(m_caps & GLEXT_SUPPORT_NVINTEROP) m_bGLDXavailable = true; // Interop needed for texture sharing

	 // FBO not available is terminal
	if(!m_bFBOavailable)
		return false;

	// If the GL/DX extensions failed to load, then it has to be CPU access
	if(!m_bGLDXavailable) {
		m_bUseCPU = true;
		spoutdx.WriteDwordToRegistry((DWORD)m_bUseCPU, "Software\\Leading Edge\\Spout", "CPU");
	}

	return true;
}


bool spoutGLDXinterop::IsBGRAavailable()
{
	return m_bBGRAavailable;
}

bool spoutGLDXinterop::IsPBOavailable()
{
	return m_bPBOavailable;
}


// Switch pbo functions on or off (default is off).
// Cannot over-ride extension availability.
// Will fail silently if extensions are not loaded
// or PBO functions are not supported
void spoutGLDXinterop::SetBufferMode(bool bActive)
{
	if(!m_bExtensionsLoaded) m_bExtensionsLoaded = LoadGLextensions();
	if(m_bExtensionsLoaded) {
		if(bActive) {
			if(m_caps & GLEXT_SUPPORT_PBO) {
				m_bPBOavailable = true;
			}
		}
		else {
			m_bPBOavailable = false;
		}
	}
	// Write to the registry now - this function is called by the SpoutDirectX utility when it starts
	spoutdx.WriteDwordToRegistry((DWORD)m_bPBOavailable, "Software\\Leading Edge\\Spout", "Buffering");

}

bool spoutGLDXinterop::GetBufferMode()
{
	DWORD dwMode = 0;
	spoutdx.ReadDwordFromRegistry(&dwMode, "Software\\Leading Edge\\Spout", "Buffering");
	return (dwMode == 1);
	
}



// 03.09.14 - MB mods for names map class
bool spoutGLDXinterop::getSharedTextureInfo(const char* sharedMemoryName) {

	unsigned int w, h;
	HANDLE handle;
	DWORD format;
	char name[256];
	strcpy_s(name, 256, sharedMemoryName);

	if (!senders.FindSender(name, w, h, handle, format)) {
		return false;
	}

	m_dxShareHandle = (HANDLE)handle;
	m_TextureInfo.width = w;
	m_TextureInfo.height = h;
#ifdef _M_X64
	m_TextureInfo.shareHandle = (unsigned __int32)(HandleToLong(handle));
#else
	m_TextureInfo.shareHandle = (unsigned __int32)handle;
#endif
	// m_TextureInfo.shareHandle = (__int32)handle;
	m_TextureInfo.format = format;

	return true;

}



// Set texture info to shared memory for the sender init
// width and height must have been set first
// 03.09.14 - MB mods for names map class
bool spoutGLDXinterop::setSharedTextureInfo(const char* sharedMemoryName) {

	return senders.UpdateSender(sharedMemoryName, 
							m_TextureInfo.width,
							m_TextureInfo.height,
							m_dxShareHandle,
							m_TextureInfo.format);


}

// Return current sharing handle, width and height of a Sender
// Note - use the map directly - we must not use getSharedTextureInfo
// which resets the local info structure from shared memory !!!
// A receiver checks this all the time so it has to be compact
// 03.09.14 - MB mods for names map class
bool spoutGLDXinterop::getSharedInfo(char* sharedMemoryName, SharedTextureInfo* info) 
{
	return senders.getSharedInfo(sharedMemoryName, info);
}



// Sets the given info structure to shared memory with the given name
// IMPORTANT: this modifies the local structure
// Used to change the texture dimensions before init
bool spoutGLDXinterop::setSharedInfo(char* sharedMemoryName, SharedTextureInfo* info)
{
	m_TextureInfo.width			= info->width;
	m_TextureInfo.height		= info->height;
#ifdef _M_X64
	m_dxShareHandle = (HANDLE)(LongToHandle((long)info->shareHandle));
#else
	m_dxShareHandle = (HANDLE)info->shareHandle;
#endif	
	// m_dxShareHandle				= (HANDLE)info->shareHandle; 
	// the local info structure handle "m_TextureInfo.shareHandle" gets converted 
	// into (unsigned __int32) from "m_dxShareHandle" by setSharedTextureInfo
	if(setSharedTextureInfo(sharedMemoryName)) {
		return true;
	}
	else {
		return false;
	}
}

// Utilities
//
// GLDXcompatible
//
bool spoutGLDXinterop::GLDXcompatible()
{
	// printf("GLDXcompatible()\n");
	//
	// ======= Hardware compatibility test =======
	//
	// Call LoadGLextensions for an initial driver compatibilty check
	// for the Nvidia OpenGL/Directx interop extensions.
	// This needs an additional check. 
	// It is possible that the extensions load OK but that initialization will still fail.
	// This occurs when wglDXOpenDeviceNV fails. 
	// This has been noted on dual graphics machines with the NVIDIA Optimus driver.
	// "GLDXcompatible" tests for this by initializing directx and calling wglDXOpenDeviceNV
	// If OK and the debug flag has not been set, all the parameters are available
	// Otherwise it is limited to memory share
	//
	HDC hdc = wglGetCurrentDC(); // OpenGl device context is needed
	if(!hdc) {
		MessageBoxA(NULL, "Spout compatibility test\nCannot get GL device context", "GLDXcompatible", MB_OK);
		return false;
	}
	HWND hWnd = WindowFromDC(hdc); // can be null though
	if(!m_bExtensionsLoaded) {
		// printf("GLDXcompatible() - loading extensions\n");
		// Load extensions and the GL/DX interop
		// Sets CPU texture access if not interop compatible
		m_bExtensionsLoaded = LoadGLextensions();
	}

	if(m_bExtensionsLoaded) {
		// Try to set up DirectX
		if(OpenDirectX(hWnd, m_bUseDX9)) {
			// if it passes here all is well
			return true;
		}
		else {
			// printf("GLDXcompatible() : OpenDirectX failed\n");
			// Failed to open DirectX - use memoryshare if DirectX is not available
			m_bUseCPU = false;
			spoutdx.WriteDwordToRegistry((DWORD)m_bUseCPU, "Software\\Leading Edge\\Spout", "CPU");
			m_bUseMemory = true;
			spoutdx.WriteDwordToRegistry((DWORD)m_bUseMemory, "Software\\Leading Edge\\Spout", "MemoryShare");
			return false; 
		}
	} // end compatibility test
	// else {
		// printf("GLDXcompatible() : Failed to load OpenGL extensions\n");
	// }

	// Extensions failed to load

	return false;

} // end GLDXcompatible


bool spoutGLDXinterop::isOptimus()
{
	// Could be improved
	if(GetModuleHandleA("nvd3d9wrap.dll")
	|| GetModuleHandleA("nvinit.dll")) {
		return true;
	}
	return false;
}



bool spoutGLDXinterop::WriteTexture (GLuint TextureID, GLuint TextureTarget,
									 unsigned int width, unsigned int height,
									 bool bInvert, GLuint HostFBO)
{
	if(m_bUseMemory) { // Memoryshare
		return(WriteMemory(TextureID, TextureTarget, width, height, bInvert, HostFBO));
	}
	else if(m_bUseCPU) { // DirectX CPU
		if(GetDX9()) {
			return(WriteDX9texture(TextureID, TextureTarget, width, height, bInvert, HostFBO));
		}
		else {
			return(WriteDX11texture(TextureID, TextureTarget, width, height, bInvert, HostFBO));
		}
	}
	else if(m_bGLDXavailable) { // GL/DX interop
		return(WriteGLDXtexture(TextureID, TextureTarget, width, height, bInvert, HostFBO));
	}
	else {
		return false;
	}
}

bool spoutGLDXinterop::ReadTexture (GLuint TextureID, GLuint TextureTarget,
									unsigned int width, unsigned int height,
									bool bInvert, GLuint HostFBO)
{
	if(m_bUseMemory) { // Memoryshare
		return(ReadMemory(TextureID, TextureTarget, width, height, bInvert, HostFBO));
	}
	else if(m_bUseCPU) { // DirectX CPU
		if(GetDX9())
			return(ReadDX9texture(TextureID, TextureTarget, width, height, bInvert, HostFBO));
		else
			return(ReadDX11texture(TextureID, TextureTarget, width, height, bInvert, HostFBO));
	}
	else if(m_bGLDXavailable) { // GL/DX interop
		return(ReadGLDXtexture(TextureID, TextureTarget, width, height, bInvert, HostFBO));
		// return true;
	}
	else {
		return false;
	}
}

bool spoutGLDXinterop::WriteTexturePixels (const unsigned char *pixels, 
										   unsigned int width, unsigned int height, 
										   GLenum glFormat, bool bInvert, GLuint HostFBO)
{
	if(m_bUseMemory) { // Memoryshare
		return(WriteMemoryPixels(pixels, width, height, glFormat, bInvert));
	}
	else if(m_bUseCPU) { // DirectX CPU
		if(GetDX9()) {
			return(WriteDX9pixels(pixels, width, height, glFormat, bInvert));
		}
		else {
			return(WriteDX11pixels(pixels, width, height, glFormat, bInvert));
		}
	}
	else if(m_bGLDXavailable) { // GL/DX interop
		return(WriteGLDXpixels(pixels, width, height, glFormat, bInvert, HostFBO));
	}
	else {
		return false;
	}
}

bool spoutGLDXinterop::ReadTexturePixels (unsigned char *pixels,
										  unsigned int width, unsigned int height, 
										  GLenum glFormat, bool bInvert, GLuint HostFBO)
{
	if(m_bUseMemory) { // Memoryshare
		return(ReadMemoryPixels(pixels, width, height, glFormat, bInvert));
	}
	else if(m_bUseCPU) { // DirectX CPU
		if(GetDX9()) 
			return(ReadDX9pixels(pixels, width, height, glFormat, bInvert));
		else
			return(ReadDX11pixels(pixels, width, height, glFormat, bInvert));
	}
	else if(m_bGLDXavailable) { // GL/DX interop
		return(ReadGLDXpixels(pixels, width, height, glFormat, bInvert, HostFBO));
	}
	else {
		return false;
	}
}

bool spoutGLDXinterop::DrawSharedTexture(float max_x, float max_y, float aspect, bool bInvert, GLuint HostFBO)
{
	if(m_bUseMemory) { // Memoryshare
		return(DrawSharedMemory(max_x, max_y, aspect, bInvert));
	}
	else if(m_bUseCPU) { // DirectX CPU
		// Default invert is true for GL/DX interop, but a DirectX texture is already inverted
		if(GetDX9()) {
			return(DrawDX9texture(max_x, max_y, aspect, !bInvert, HostFBO));
		}
		else {
			return(DrawDX11texture(max_x, max_y, aspect, !bInvert, HostFBO));
		}
	}
	else if(m_bGLDXavailable) { // GL/DX interop
		return(DrawGLDXtexture(max_x, max_y, aspect, bInvert));
	}
	else {
		return false;
	}
}

bool spoutGLDXinterop::DrawToSharedTexture(GLuint TextureID, GLuint TextureTarget,
									   unsigned int width, unsigned int height,
									   float max_x, float max_y, float aspect,
									   bool bInvert, GLuint HostFBO)
{
	if(m_bUseMemory) { // Memoryshare
		return(DrawToSharedMemory(TextureID, TextureTarget, width, height, max_x, max_y, aspect, bInvert));
	}
	else if(m_bUseCPU) { // DirectX CPU
		if(GetDX9()) 
			return(DrawToDX9texture(TextureID, TextureTarget, width, height, max_x, max_y, aspect, bInvert, HostFBO));
		else
			return(DrawToDX11texture(TextureID, TextureTarget, width, height, max_x, max_y, aspect, bInvert, HostFBO));
	}
	else if(m_bGLDXavailable) { // GL/DX interop
		return(DrawToGLDXtexture(TextureID, TextureTarget, width, height, max_x, max_y, aspect, bInvert, HostFBO));
	}
	else {
		return false;
	}
}

//
// BIND THE SHARED TEXTURE
//
// for use within an application - this locks the interop object and binds the shared texture
// Locks remain in place, so afterwards a call to UnbindSharedTexture MUST be called
//
bool spoutGLDXinterop::BindSharedTexture()
{
	bool bRet = false;

	// Only for GL/DX interop mode
	if(m_hInteropDevice == NULL || m_hInteropObject == NULL)
		return false;

	// Wait for access to the texture
	if(spoutdx.CheckAccess(m_hAccessMutex)) {
		// lock dx object
		if(LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {
			// Bind our shared OpenGL texture
			glBindTexture(GL_TEXTURE_2D, m_glTexture);
			bRet = true;
		}
		else {
			bRet = false;
		}
	}

	// Leave locked for succcess, release interop lock and allow texture access for fail
	if(!bRet) {
		UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
		spoutdx.AllowAccess(m_hAccessMutex); // Allow access to the texture
	}

	return bRet;

} // end BindSharedTexture


//
// UNBIND THE SHARED TEXTURE
//
// for use within an application - this unbinds the shared texture and unlocks the interop object
//
bool spoutGLDXinterop::UnBindSharedTexture()
{
	// Only for GL/DX interop mode
	if(m_hInteropDevice == NULL || m_hInteropObject == NULL)
		return false;
	
	// Unbind our shared OpenGL texture
	glBindTexture(GL_TEXTURE_2D,0);
	// unlock dx object
	UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
	// Allow access to the texture
	spoutdx.AllowAccess(m_hAccessMutex);
	
	return true;

} // end BindSharedTexture


// ----------------------------------------------------------
//		Access to texture using DX/GL interop functions
// ----------------------------------------------------------

//
// COPY AN OPENGL TEXTURE TO THE SHARED OPENGL TEXTURE
// 
bool spoutGLDXinterop::WriteGLDXtexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	GLenum status;

	if(m_hInteropDevice == NULL || m_hInteropObject == NULL) return false;
	if(width != m_TextureInfo.width || height != m_TextureInfo.height) return false;

	// Wait for access to the texture
	if(spoutdx.CheckAccess(m_hAccessMutex)) {

		// lock dx object
		if(LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {

			// fbo is a  local FBO and width/height are the dimensions of the texture.
			// "TextureID" is the source texture, and "m_glTexture" is destination texture
			// which should have been already created

			// bind the FBO (for both, READ_FRAMEBUFFER_EXT and DRAW_FRAMEBUFFER_EXT)
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

			// Attach the Input texture to the color buffer in our frame buffer - note texturetarget 
			glFramebufferTexture2DEXT(READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, TextureTarget, TextureID, 0);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

			// Attach target texture (the shared texture we write into) to second attachment point
			glFramebufferTexture2DEXT(DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, m_glTexture, 0);
			glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);

			status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
			if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {

				if(m_bBLITavailable) {
					// Default invert flag is false so do the flip to get it the right way up if the user wants that
					if(bInvert) {
						// Blit method with checks - 0.75 - 0.85 msec
						// copy one texture buffer to the other while flipping upside down (OpenGL and DirectX have different texture origins)
						glBlitFramebufferEXT(0, 0,			// srcX0, srcY0, 
											 width, height, // srcX1, srcY1
											 0, height,		// dstX0, dstY0,
											 width, 0,		// dstX1, dstY1,
											 GL_COLOR_BUFFER_BIT, GL_NEAREST); // GLbitfield mask, GLenum filter
					}
					else { 
						// Do not flip during blit
						glBlitFramebufferEXT(0, 0,			// srcX0, srcY0, 
											 width, height,	// srcX1, srcY1
											 0, 0,			// dstX0, dstY0,
											 width, height,	// dstX1, dstY1,
											 GL_COLOR_BUFFER_BIT, GL_NEAREST); // GLbitfield mask, GLenum filter
					}
				}
				else {
					// No fbo blit extension
					// Copy from the fbo (input texture attached) to the shared texture
					glBindTexture(GL_TEXTURE_2D, m_glTexture);
					glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
					glBindTexture(GL_TEXTURE_2D, 0);
				}
			}
			else {
				PrintFBOstatus(status);
			}

			// restore the previous fbo - default is 0
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);

			// unlock dx object
			UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
			spoutdx.AllowAccess(m_hAccessMutex); // Allow access to the texture
			

			return true;
		}
	}

	// There is no reader
	spoutdx.AllowAccess(m_hAccessMutex); // Allow access to the texture

	return false;

} // end WriteGLDXTexture


//
// COPY FROM THE SHARED OPENGL TEXTURE TO AN OPENGL TEXTURE
//
bool spoutGLDXinterop::ReadGLDXtexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	GLenum status;

	if(m_hInteropDevice == NULL || m_hInteropObject == NULL) return false;
	if(width != (unsigned int)m_TextureInfo.width || height != (unsigned int)m_TextureInfo.height) return false;
	if(spoutdx.CheckAccess(m_hAccessMutex)) {

		// lock interop
		if(LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {

			// bind the FBO (for both, READ_FRAMEBUFFER_EXT and DRAW_FRAMEBUFFER_EXT)
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

			// Attach the Input texture (the shared texture) to the color buffer in our frame buffer - note texturetarget 
			glFramebufferTexture2DEXT(READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_glTexture, 0);
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

			// Attach target texture (the one we write into and return) to second attachment point
			glFramebufferTexture2DEXT(DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, TextureTarget, TextureID, 0);
			glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);

			status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
			if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {
				if(m_bBLITavailable) {
					// Flip if the user wants that
					if(bInvert) {
						// copy one texture buffer to the other while flipping upside down
						glBlitFramebufferEXT(0,     0,		// srcX0, srcY0, 
											 width, height, // srcX1, srcY1
											 0,     height,	// dstX0, dstY0,
											 width, 0,		// dstX1, dstY1,
											 GL_COLOR_BUFFER_BIT, GL_LINEAR);
					}
					else { 
						// Do not flip during blit
						glBlitFramebufferEXT(0, 0,			// srcX0, srcY0, 
											 width, height,	// srcX1, srcY1
											 0, 0,			// dstX0, dstY0,
											 width, height,	// dstX1, dstY1,
											 GL_COLOR_BUFFER_BIT, GL_NEAREST); // GLbitfield mask, GLenum filter
					}
				}
				else { 
					// No fbo blit extension available
					// Copy from the fbo (shared texture attached) to the dest texture
					glBindTexture(TextureTarget, TextureID);
					glCopyTexSubImage2D(TextureTarget, 0, 0, 0, 0, 0, width, height);
					glBindTexture(TextureTarget, 0);
				}
			}
			else {
				PrintFBOstatus(status);
			}

			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); // 04.01.16

			// restore the previous fbo - default is 0
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);

			// unlock dx object
			UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
			spoutdx.AllowAccess(m_hAccessMutex);
			return true;
		}
	}

	spoutdx.AllowAccess(m_hAccessMutex);

	return false;

} // end ReadGLDXTexture


//
// COPY IMAGE PIXELS TO THE SHARED TEXTURE
//
bool spoutGLDXinterop::WriteGLDXpixels(const unsigned char *pixels, 
	                                      unsigned int width, 
										  unsigned int height, 
										  GLenum glFormat,
										  bool bInvert,
										  GLuint HostFBO)
{
	GLenum glformat = glFormat;

	if(width != m_TextureInfo.width || height != m_TextureInfo.height) return false;

	// Use a GL texture so that WriteTexture can be used
	
	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);

	// Transfer the pixels to the local texture
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	if(IsPBOavailable()) {
		LoadTexturePixels(m_TexID, GL_TEXTURE_2D, width, height, (const unsigned char *)pixels, glFormat);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, m_TexID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, glformat, GL_UNSIGNED_BYTE, (GLvoid *)pixels);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	// Write the local texture to the shared texture and invert if necessary
	WriteTexture(m_TexID, GL_TEXTURE_2D, width, height, bInvert, HostFBO);

	return true;

} // end WriteGLDXpixels

//
// COPY THE SHARED TEXTURE TO IMAGE PIXELS
//
bool spoutGLDXinterop::ReadGLDXpixels(unsigned char *pixels, 
										 unsigned int width, 
										 unsigned int height, 
										 GLenum glFormat,
										 bool bInvert, 
										 GLuint HostFBO)
{
	GLenum status;
	GLenum glformat = glFormat;

	if(m_hInteropDevice == NULL || m_hInteropObject == NULL) return false;
	if(width != m_TextureInfo.width || height != m_TextureInfo.height) return false;

	// retrieve opengl texture data directly to image pixels rather than via an fbo and texture
	// Wait for access to the texture
	if(spoutdx.CheckAccess(m_hAccessMutex)) {
		
		// lock gl/dx interop object
		if(LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {

			// Set single pixel alignment in case of rgb source
			glPixelStorei(GL_PACK_ALIGNMENT, 1);

			// Create or resize a local OpenGL texture
			CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);

			// Copy the shared texture to the local texture, inverting if necessary
			CopyTexture(m_glTexture, GL_TEXTURE_2D, m_TexID, GL_TEXTURE_2D, width, height, bInvert, HostFBO);

			// Extract the pixels from the local texture - changing to the user passed format
			if(IsPBOavailable()) { // PBO method
				UnloadTexturePixels(m_TexID, GL_TEXTURE_2D, width, height, pixels, glFormat, false, HostFBO);
			}
			else {
				//
				// fbo attachment method - current fbo has to be passed in
				//
				// Bind our local fbo
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo); 
				// Attach the local rgba texture to the color buffer in our frame buffer
				glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_TexID, 0);
				status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
				if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {
					// read the pixels from the framebuffer in the user provided format
					glReadPixels(0, 0, width, height, glformat, GL_UNSIGNED_BYTE, pixels);
				}
				else {
					PrintFBOstatus(status);
				}
				// restore the previous fbo - default is 0
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
			}
	
			glPixelStorei(GL_PACK_ALIGNMENT, 4);

			// Unlock interop object
			UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
			spoutdx.AllowAccess(m_hAccessMutex);
			return true;
		} // interop lock failed
	} // mutex access failed

	spoutdx.AllowAccess(m_hAccessMutex);

	return false;

} // end ReadGLDXpixels 


//
// DRAW A TEXTURE INTO THE THE SHARED TEXTURE VIA AN FBO
//
bool spoutGLDXinterop::DrawToGLDXtexture(GLuint TextureID, GLuint TextureTarget, 
										   unsigned int width, unsigned int height, 
										   float max_x, float max_y, float aspect, 
										   bool bInvert, GLuint HostFBO)
{
	GLenum status;

	if(m_hInteropDevice == NULL || m_hInteropObject == NULL) return false;
	if(width != (unsigned  int)m_TextureInfo.width || height != (unsigned  int)m_TextureInfo.height) return false;

	// printf("Draw To Shared Texture - invert = %d\n", bInvert);

	// Wait for access to the texture
	if(spoutdx.CheckAccess(m_hAccessMutex)) {

		if(LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {

			// Draw the input texture into the shared texture via an fbo

			// Bind our fbo and attach the shared texture to it
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
			
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_glTexture, 0);
			
			status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
			if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {

				glColor4f(1.f, 1.f, 1.f, 1.f);
				glEnable(TextureTarget);
				glBindTexture(TextureTarget, TextureID);

				GLfloat tc[4][2] = {0};

				// Invert texture coord to user requirements
				if(bInvert) {
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

				GLfloat verts[] =  {
								-aspect, -1.0,   // bottom left
								-aspect,  1.0,   // top left
								 aspect,  1.0,   // top right
								 aspect, -1.0 }; // bottom right

				glEnableClientState( GL_TEXTURE_COORD_ARRAY );
				glTexCoordPointer(2, GL_FLOAT, 0, tc );
				glEnableClientState(GL_VERTEX_ARRAY);		
				glVertexPointer(2, GL_FLOAT, 0, verts );
				glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
				glDisableClientState(GL_VERTEX_ARRAY);
				glDisableClientState(GL_TEXTURE_COORD_ARRAY);

				glBindTexture(TextureTarget, 0);
				glDisable(TextureTarget);
			}
			else {
				PrintFBOstatus(status);
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
				UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
				spoutdx.AllowAccess(m_hAccessMutex); // Allow access to the texture
				return false;
			}
			// restore the previous fbo - default is 0
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
			UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
		}
	}
	spoutdx.AllowAccess(m_hAccessMutex); // Allow access to the texture

	return true;

} // end DrawToGLDXtexture

//
// DRAW THE SHARED TEXTURE
//
bool spoutGLDXinterop::DrawGLDXtexture(float max_x, float max_y, float aspect, bool bInvert)
{
	if(m_hInteropDevice == NULL || m_hInteropObject == NULL)
		return false;

	// Wait for access to the texture
	if(spoutdx.CheckAccess(m_hAccessMutex)) {

		// go ahead and access the shared texture to draw it
		if(LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {

			SaveOpenGLstate(m_TextureInfo.width, m_TextureInfo.height);
			glEnable(GL_TEXTURE_2D);
			glBindTexture(GL_TEXTURE_2D, m_glTexture); // bind shared texture
			glColor4f(1.f, 1.f, 1.f, 1.f);
			// Tried to convert to vertex array, but Processing crash
			glBegin(GL_QUADS);
			if(bInvert) {
				glTexCoord2f(0.0,	max_y);	glVertex2f(-aspect,-1.0); // lower left
				glTexCoord2f(0.0,	0.0);	glVertex2f(-aspect, 1.0); // upper left
				glTexCoord2f(max_x, 0.0);	glVertex2f( aspect, 1.0); // upper right
				glTexCoord2f(max_x, max_y);	glVertex2f( aspect,-1.0); // lower right
			}
			else {
				glTexCoord2f(0.0,   0.0);	glVertex2f(-aspect,-1.0); // lower left
				glTexCoord2f(0.0,   max_y);	glVertex2f(-aspect, 1.0); // upper left
				glTexCoord2f(max_x, max_y);	glVertex2f( aspect, 1.0); // upper right
				glTexCoord2f(max_x, 0.0);	glVertex2f( aspect,-1.0); // lower right
			}
			glEnd();
			glBindTexture(GL_TEXTURE_2D, 0);
			glDisable(GL_TEXTURE_2D);
			RestoreOpenGLstate();

			UnlockInteropObject(m_hInteropDevice, &m_hInteropObject); // unlock dx object
			spoutdx.AllowAccess(m_hAccessMutex); // Allow access to the texture
			return true;

		} // lock failed
	} // mutex lock failed

	spoutdx.AllowAccess(m_hAccessMutex); // Allow access to the texture

	return false;

} // end DrawGLDXTexture



//
// DX11 versions - https://github.com/DashW/Spout2
//

//
// COPY A DX11 TEXTURE TO THE SHARED DX11 TEXTURE
//
bool spoutGLDXinterop::WriteTexture(ID3D11Texture2D** texture)
{
	// Only for DX11 mode
	if(!texture || !*texture || GetDX9())
		return false;

	D3D11_TEXTURE2D_DESC desc = { 0 };
	(*texture)->GetDesc(&desc);
	if(desc.Width != (unsigned int)m_TextureInfo.width || desc.Height != (unsigned int)m_TextureInfo.height) {
		return false;
	}

	// Wait for access to the texture
	if(spoutdx.CheckAccess(m_hAccessMutex)) {
		if(g_pImmediateContext)
			g_pImmediateContext->CopyResource(g_pSharedTexture, *texture);
		// Wait for access to the shared texture sor the receiver can read it straight away
		FlushWait();
		spoutdx.AllowAccess(m_hAccessMutex); // Allow access to the texture
		return true;
	}

	// Cannot access
	spoutdx.AllowAccess(m_hAccessMutex); // Allow access to the texture

	return false;
}

//
// COPY FROM THE SHARED DX11 TEXTURE TO A DX11 TEXTURE
//
bool spoutGLDXinterop::ReadTexture(ID3D11Texture2D** texture)
{
	// Only for DX11 mode
	if(!texture || !*texture || GetDX9())
		return false;

	D3D11_TEXTURE2D_DESC desc = { 0 };
	(*texture)->GetDesc(&desc);
	if(desc.Width != (unsigned int)m_TextureInfo.width || desc.Height != (unsigned int)m_TextureInfo.height) {
		return false;
	}

	if(spoutdx.CheckAccess(m_hAccessMutex)) {
		if(g_pImmediateContext)
			g_pImmediateContext->CopyResource(*texture, g_pSharedTexture);
		spoutdx.AllowAccess(m_hAccessMutex); // Allow access to the texture
		return true;
	}

	// Cannot access
	spoutdx.AllowAccess(m_hAccessMutex);

	return false;

} // end ReadTexture


void spoutGLDXinterop::FlushWait()
{
	D3D11_QUERY_DESC queryDesc;
	ID3D11Query * pQuery = NULL;

	// =====================================================================
	// Tests confirm that for a sender the following code eliminates jerky
	// texture access by a receiver and a receiver is faster with the flush.
	// =====================================================================

	// CopyResource is an asynchronous call.
	// https://msdn.microsoft.com/en-us/library/windows/desktop/bb205132%28v=vs.85%29.aspx#Performance_Considerations
	// For ReadTexture, "If the application tries to map the resource that was the target of
	// the copy call before the command buffer has been flushed, a pipeline stall will occur."
	// The receiver will attempt to read the staging texture immediately after it and so
	// the flush and wait is necessary. For WriteTexture "the copy has not necessarily executed
	// by the time the method returns". The sender has to ensure that the copy is complete so
	// that the receiver will get the new frame.
	if(g_pImmediateContext) {
		g_pImmediateContext->Flush();

		// For a receiver, make sure that the GPU is finished processing commands before accessing the 
		// staging texture and before the sender fills the shared texture again on the next frame.
		// For a sender, make sure the CopyResource fnction has completed before the receiver application
		// accesses the shared texture.
		// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476578%28v=vs.85%29.aspx
		ZeroMemory(&queryDesc, sizeof(queryDesc));
		queryDesc.Query = D3D11_QUERY_EVENT; 
		// When the GPU is finished, ID3D11DeviceContext::GetData will return S_OK.
		// When using this type of query, ID3D11DeviceContext::Begin is disabled.
		ZeroMemory(&queryDesc, sizeof(queryDesc));
		queryDesc.Query = D3D11_QUERY_EVENT; 
		g_pd3dDevice->CreateQuery(&queryDesc, &pQuery);
		g_pImmediateContext->End(pQuery);
		while( S_OK != g_pImmediateContext->GetData(pQuery, NULL, 0, 0));
		pQuery->Release();
	}

}

//
// COPY IMAGE PIXELS TO A TEXTURE
//
//
// Streaming Texture Upload
//
// From : http://www.songho.ca/opengl/gl_pbo.html
//
// No FBO used so none has to be passed
//
bool spoutGLDXinterop::LoadTexturePixels(GLuint TextureID, GLuint TextureTarget, 
										 unsigned int width, unsigned int height, 
										 const unsigned char *data, 
										 GLenum glFormat, bool bInvert)
{
	void *pboMemory = NULL;
	int channels = 4; // RGBA or RGB

	if(TextureID == 0 || data == NULL)
		return false;

	if(glFormat == GL_RGB || glFormat == GL_BGR_EXT) 
		channels = 3;

	if(m_fbo == 0) {
		glGenFramebuffersEXT(1, &m_fbo); 
	}

	// Create pbos if not already
	if(!m_pbo[0]) glGenBuffersEXT(2, m_pbo);

	PboIndex = (PboIndex + 1) % 2;
	NextPboIndex = (PboIndex + 1) % 2;

	// Bind the texture and PBO
	glBindTexture(TextureTarget, TextureID);
	glBindBufferEXT(GL_PIXEL_UNPACK_BUFFER, m_pbo[PboIndex]);

	// Copy pixels from PBO to the texture - use offset instead of pointer.
	glTexSubImage2D(TextureTarget, 0, 0, 0, width, height, glFormat, GL_UNSIGNED_BYTE, 0);

	// Bind PBO to update the texture
	glBindBufferEXT(GL_PIXEL_UNPACK_BUFFER, m_pbo[NextPboIndex]);

	// Call glBufferData() with a NULL pointer to clear the PBO data and avoid a stall.
	glBufferDataEXT(GL_PIXEL_UNPACK_BUFFER, width*height*channels, 0, GL_STREAM_DRAW);

	// Map the buffer object into client's memory
	pboMemory = (void *)glMapBufferEXT(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	GLerror(); // soak up the error for Processing - only happens once
	if(pboMemory) {
		// Update data directly on the mapped buffer
		spoutcopy.CopyPixels((const unsigned char *)data, (unsigned char *)pboMemory, width, height, glFormat, bInvert);
		glUnmapBufferEXT(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
	}
	else {
		glBindBufferEXT(GL_PIXEL_UNPACK_BUFFER, 0);
		return false;
	}

	// Release PBOs
	glBindBufferEXT(GL_PIXEL_UNPACK_BUFFER, 0);

	return true;

}

//
// Asynchronous Read-back from a texture
//
// Adapted from : http://www.songho.ca/opengl/gl_pbo.html
//
bool spoutGLDXinterop::UnloadTexturePixels(GLuint TextureID, GLuint TextureTarget, 
										   unsigned int width, unsigned int height, 
										   unsigned char *data, GLenum glFormat, 
										   bool bInvert, GLuint HostFBO)
{
	void *pboMemory = NULL;
	int channels = 4; // RGBA or RGB

	if(TextureID == 0 || data == NULL)
		return false;

	if(glFormat == GL_RGB || glFormat == GL_BGR_EXT) 
		channels = 3;

	// Create pbos if not already
	if(m_pbo[0] == 0 || m_pbo[1] == 0) {
		glGenBuffersEXT(2, m_pbo);
	}

	if(m_fbo == 0) {
		glGenFramebuffersEXT(1, &m_fbo); 
	}
	
	PboIndex = (PboIndex + 1) % 2;
	NextPboIndex = (PboIndex + 1) % 2;

	// Attach the texture to an FBO
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, TextureTarget, TextureID, 0);

	// Set the target framebuffer to read
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

	// Bind the PBO
	glBindBufferEXT(GL_PIXEL_PACK_BUFFER, m_pbo[PboIndex]);

	// Null existing data to avoid a stall
	glBufferDataEXT(GL_PIXEL_PACK_BUFFER, width*height*channels, 0, GL_STREAM_READ);

	// Read pixels from framebuffer to PBO - glReadPixels() should return immediately.
	glReadPixels(0, 0, width, height, glFormat, GL_UNSIGNED_BYTE, (GLvoid *)0);

	// Map the PBO to process its data by CPU
	glBindBufferEXT(GL_PIXEL_PACK_BUFFER, m_pbo[NextPboIndex]);

	// TODO : For some reason, glMapBuffer returns NULL when called the first time
	// when used with Processing. Not resolved - but it only happens once.
	pboMemory = glMapBufferEXT(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
	if(pboMemory) {
		// Update data directly on the mapped buffer
		spoutcopy.CopyPixels((const unsigned char *)pboMemory, (unsigned char *)data, width, height, glFormat, bInvert);
		glUnmapBufferEXT(GL_PIXEL_PACK_BUFFER);
	}
	else {
		GLerror(); // soak up the error for Processing
		glBindBufferEXT(GL_PIXEL_PACK_BUFFER, 0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
		return false;
	}
	
	// Back to conventional pixel operation
	glBindBufferEXT(GL_PIXEL_PACK_BUFFER, 0);
	
	// Restore the previous fbo binding
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);


	return true;

}


// ===================================================================
// DirectX texture CPU access where the GL/DX interop is not available
// ===================================================================


// ==============================
// DX11 staging texture functions
// ==============================

//
// Create a new global staging texture if it has changed size or does not exist yet
//
bool spoutGLDXinterop::CheckStagingTexture(unsigned int width, unsigned int height)
{
	D3D11_TEXTURE2D_DESC desc = { 0 };

	if(g_pStagingTexture) {
		g_pStagingTexture->GetDesc(&desc);
		if(desc.Width != width || desc.Height != height) {
			g_pStagingTexture->Release();
			g_pStagingTexture = NULL;
		}
		else
			return true;
	}

	if(!g_pStagingTexture) {
		if(spoutdx.CreateDX11StagingTexture(g_pd3dDevice, width, height, DX11format, &g_pStagingTexture)) {
			return true;
		}
	}

	return false;
}


//
// COPY FROM A USER OPENGL TEXTURE TO THE SHARED DIRECTX TEXTURE BY WAY OF A DX11 STAGING TEXTURE 
//
bool spoutGLDXinterop::WriteDX11texture (GLuint TextureID, GLuint TextureTarget, 
										 unsigned int width, unsigned int height, 
										 bool bInvert, GLuint HostFBO)
{
	D3D11_MAPPED_SUBRESOURCE mappedSubResource;
	HRESULT hr;
	// void * dataPointer = NULL;

	// Only for DX11 mode
	if(GetDX9())
		return false;

	if(width != m_TextureInfo.width || height != m_TextureInfo.height)
		return false;

	// If a staging texture has not been created or is a different size, create a new one
	if(!CheckStagingTexture(width, height))
		return false;

	//
	// Copy OpenGL texture pixels to the staging texture
	//

	if(g_pImmediateContext) {
		// Get a pointer to the staging texture data
		hr = g_pImmediateContext->Map(g_pStagingTexture, 0, D3D11_MAP_WRITE, 0, &mappedSubResource);
		if(SUCCEEDED(hr)) {
			// Copy the user OpenGL texture data directly to the staging texture
			if(IsPBOavailable()) { // PBO method
				UnloadTexturePixels(TextureID, TextureTarget, width, height, (unsigned char *)mappedSubResource.pData, GL_BGRA_EXT, bInvert, HostFBO);
			}
			else {
				if(bInvert) {
					// Create or resize a local OpenGL texture
					CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);
					// Copy the user texture to the local texture - necessary for inversion
					CopyTexture(TextureID, TextureTarget, m_TexID, GL_TEXTURE_2D, width, height, bInvert, HostFBO);
					// Bind our local fbo - current fbo has to be passed in
					glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo); 
					// Attach the local rgba texture to the color buffer in our frame buffer
					glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_TexID, 0);
					GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
					if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {
						// read the pixels from the OpenGL framebuffer into the BGRA DX11 texture
						glReadPixels(0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, mappedSubResource.pData);
					}
					else {
						PrintFBOstatus(status);
					}
					// restore the previous fbo - default is 0
					glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
				}
				else {
					// No invert so use the user texture
					glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo); 
					// Attach the user rgba texture to the color buffer in our frame buffer
					glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, TextureTarget, TextureID, 0);
					GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
					if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {
						// read the pixels from the OpenGL framebuffer into the BGRA DX11 texture
						glReadPixels(0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, mappedSubResource.pData);
					}
					else {
						PrintFBOstatus(status);
					}
				}
			}
			g_pImmediateContext->Unmap(g_pStagingTexture, 0);

			// Write the staging texture to the shared texture
			return WriteTexture(&g_pStagingTexture);
		}

	} // endif DX11 map OK

	return false;

} // end WriteDX11texture



//
// COPY FROM THE SHARED DIRECTX TEXTURE TO A USER OPENGL TEXTURE BY WAY OF A DX11 STAGING TEXTURE 
//
bool spoutGLDXinterop::ReadDX11texture (GLuint TextureID,
										GLuint TextureTarget, 
										unsigned int width, 
										unsigned int height, 
										bool bInvert,
										GLuint HostFBO)
{
	D3D11_MAPPED_SUBRESOURCE mappedSubResource;
	HRESULT hr;
	void * dataPointer = NULL;

	// Only for DX11 mode
	if(GetDX9())
		return false;

	if(width != m_TextureInfo.width || height != m_TextureInfo.height)
		return false;

	// If a staging texture has not been created or it is a different size, create a new one
	if(!CheckStagingTexture(width, height))
		return false;

	// Read the shared texture data into the staging texture so it can be accessed
	if(ReadTexture(&g_pStagingTexture)) {
		FlushWait(); // Wait for access to the staging texture
		// Map the staging texture resource to access the pixels
		if(g_pImmediateContext) {
			hr = g_pImmediateContext->Map(g_pStagingTexture, 0, D3D11_MAP_READ, 0, &mappedSubResource);
			if(SUCCEEDED(hr)) {
				// Get a pointer to the staging texture data
				dataPointer = mappedSubResource.pData;
				if(dataPointer) {
					if(IsPBOavailable()) { // PBO method
						LoadTexturePixels(TextureID, TextureTarget, width, height, (const unsigned char *)dataPointer, GL_BGRA_EXT, bInvert);
					}
					else {
						if(bInvert) {
							// Create or resize a local OpenGL texture
							CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);
							// Copy the DX11 pixels to it
							glBindTexture(GL_TEXTURE_2D, m_TexID);
							glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dataPointer);
							glBindTexture(GL_TEXTURE_2D, 0);
							// Copy the local texture to the user texture and invert as necessary
							CopyTexture(m_TexID, GL_TEXTURE_2D, TextureID, TextureTarget, width, height, bInvert, HostFBO);
						}
						else {
							// Copy the DX11 pixels to the user texture
							glBindTexture(TextureTarget, TextureID);
							glTexSubImage2D(TextureTarget, 0, 0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dataPointer);
							glBindTexture(TextureTarget, 0);
						}
					}
				}
				g_pImmediateContext->Unmap(g_pStagingTexture, 0);
				return true;
			}
		} // endif DX11 map OK
	} // endif ReadTexture OK
	return false;
} // end ReadDX11texture



//
// COPY FROM A USER PIXEL BUFFER TO THE SHARED DIRECTX TEXTURE BY WAY OF A DX11 STAGING TEXTURE 
//
bool spoutGLDXinterop::WriteDX11pixels (const unsigned char *pixels,
										unsigned int width,
										unsigned int height,
										GLenum glFormat,
										bool bInvert)
{
	D3D11_MAPPED_SUBRESOURCE mappedSubResource;
	HRESULT hr;
	void * dataPointer = NULL;

	// Only for DX11 mode
	if(GetDX9())
		return false;

	if(width != m_TextureInfo.width || height != m_TextureInfo.height)
		return false;

	// If a staging texture has not been created or it is a different size, create a new one
	if(!CheckStagingTexture(width, height))
		return false;
	
	// Copy the pixels to the staging texture
	if(g_pImmediateContext) {
		hr = g_pImmediateContext->Map(g_pStagingTexture, 0, D3D11_MAP_WRITE, 0, &mappedSubResource);
		if(SUCCEEDED(hr)) {
			// Get a pointer to the staging texture data
			dataPointer = mappedSubResource.pData;
			if(dataPointer) {
				// Write the user pixel buffer to the staging texture
				switch(glFormat) {
					case GL_BGRA_EXT: // direct copy
						spoutcopy.CopyPixels(pixels, (unsigned char *)dataPointer, width, height, GL_RGBA, bInvert);
						break;
					case GL_RGBA: // Convert the rgba pixels to bgra for the DX11 texture
						spoutcopy.rgba2bgra((void *)pixels, dataPointer, width, height, bInvert);
						break;
					case GL_RGB: // Convert the rgb pixels to bgra for the DX11 texture
						spoutcopy.rgb2bgra((void *)pixels, dataPointer, width, height, bInvert);
						break;
					case GL_BGR_EXT: // Convert the bgr pixels to bgra for the DX11 texture
						spoutcopy.bgr2bgra((void *)pixels, dataPointer, width, height, bInvert);
						break;
					default:
						break;
				}
				// Unmap and copy the staging texture resource to the shared texture
				g_pImmediateContext->Unmap(g_pStagingTexture, 0);
				return WriteTexture(&g_pStagingTexture);
			}
		} // endif pointer OK
	} // endif DX11 map OK
	return false;
} // end WriteDX11pixels


//
// COPY FROM THE SHARED DIRECTX TEXTURE TO A USER PIXEL BUFFER BY WAY OF A DX11 STAGING TEXTURE 
//
bool spoutGLDXinterop::ReadDX11pixels (unsigned char *pixels, 
									   unsigned int width, 
									   unsigned int height,
									   GLenum glFormat, 
									   bool bInvert)
{
	D3D11_MAPPED_SUBRESOURCE mappedSubResource;
	HRESULT hr;
	void * dataPointer = NULL;

	// Only for DX11 mode
	if(GetDX9())
		return false;

	if(width != m_TextureInfo.width || height != m_TextureInfo.height)
		return false;

	// If a staging texture has not been created or it is a different size, create a new one
	if(!CheckStagingTexture(width, height))
		return false;

	// Read the shared texture data into the staging texture so it can be accessed
	if(ReadTexture(&g_pStagingTexture)) {
		FlushWait(); // Wait for access to the staging texture
		// Map the resource so we can access the pixels
		if(g_pImmediateContext) {
			hr = g_pImmediateContext->Map(g_pStagingTexture, 0, D3D11_MAP_READ, 0, &mappedSubResource);
			if(SUCCEEDED(hr)) {
				// Get a pointer to the staging texture data
				dataPointer = mappedSubResource.pData;
				// Write the the bgra staging texture to the user pixel buffer
				switch(glFormat) {
					case GL_BGRA_EXT: // direct copy
						spoutcopy.CopyPixels((unsigned char *)dataPointer, pixels, width, height, GL_RGBA, bInvert);
						break;
					case GL_RGBA:
						spoutcopy.bgra2rgba(dataPointer, (void *)pixels, width, height, bInvert);
						break;
					case GL_RGB:
						spoutcopy.bgra2rgb(dataPointer, (void *)pixels, width, height, bInvert);
						break;
					case GL_BGR_EXT:
						spoutcopy.bgra2bgr(dataPointer, (void *)pixels, width, height, bInvert);
						break;
					default:
						break;
				}
				g_pImmediateContext->Unmap(g_pStagingTexture, 0);
				return true;
			}
		} // endif DX11 map OK
	} // endif ReadTexture OK

	return false;
} // end ReadDX11pixels


//
// Draw the shared DirectX 11 texture
// equivalent to DrawSharedTexture for the shared OpenGL texture
//
bool spoutGLDXinterop::DrawDX11texture(float max_x, float max_y, float aspect, bool bInvert, GLuint HostFBO)
{
	D3D11_MAPPED_SUBRESOURCE mappedSubResource;
	HRESULT hr;
	void * dataPointer = NULL;
	unsigned int width, height;

	UNREFERENCED_PARAMETER(HostFBO);

	// Only for DX11 mode
	if(GetDX9())
		return false;

	width  = m_TextureInfo.width;
	height = m_TextureInfo.height;

	// If a staging texture has not been created or it is a different size, create a new one
	if(!CheckStagingTexture(width, height))
		return false;

	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);

	// Read the shared texture data into the staging texture so it can be accessed
	if(ReadTexture(&g_pStagingTexture)) {
		FlushWait(); // Wait for access to the staging texture
		// Map the staging texture resource to access the pixels
		if(g_pImmediateContext) {
			hr = g_pImmediateContext->Map(g_pStagingTexture, 0, D3D11_MAP_READ, 0, &mappedSubResource);
			if(SUCCEEDED(hr)) {
				// Get a pointer to the staging texture data
				dataPointer = mappedSubResource.pData;
				if(dataPointer) {

					// Copy the DX11 pixels to it
					glBindTexture(GL_TEXTURE_2D, m_TexID);
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, dataPointer);
					glBindTexture(GL_TEXTURE_2D, 0);

					// Draw the local texture and invert as necessary
					SaveOpenGLstate(width, height);
					glEnable(GL_TEXTURE_2D);
					glBindTexture(GL_TEXTURE_2D, m_TexID); // bind texture
					glColor4f(1.f, 1.f, 1.f, 1.f);
					glBegin(GL_QUADS);
					if(bInvert) {
						// DirectX coordinates are already inverted
						glTexCoord2f(0.0,   0.0);	glVertex2f(-aspect,-1.0); // lower left
						glTexCoord2f(0.0,   max_y);	glVertex2f(-aspect, 1.0); // upper left
						glTexCoord2f(max_x, max_y);	glVertex2f( aspect, 1.0); // upper right
						glTexCoord2f(max_x, 0.0);	glVertex2f( aspect,-1.0); // lower right
					}
					else {
						glTexCoord2f(0.0,	max_y);	glVertex2f(-aspect,-1.0); // lower left
						glTexCoord2f(0.0,	0.0);	glVertex2f(-aspect, 1.0); // upper left
						glTexCoord2f(max_x, 0.0);	glVertex2f( aspect, 1.0); // upper right
						glTexCoord2f(max_x, max_y);	glVertex2f( aspect,-1.0); // lower right
					}
					glEnd();
					glBindTexture(GL_TEXTURE_2D, 0); // unbind shared texture
					glDisable(GL_TEXTURE_2D);
					RestoreOpenGLstate();
				}
				g_pImmediateContext->Unmap(g_pStagingTexture, 0);
				return true;
			}
		} // endif DX11 map OK
	} // endif ReadTexture OK

	return false;

} // end DrawDX11texture

//
// DRAW AN OPENGL TEXTURE INTO THE SHARED DX11 texture
// equivalent to DrawToSharedTexture for the shared OpenGL texture
//
bool spoutGLDXinterop::DrawToDX11texture(GLuint TextureID, GLuint TextureTarget, 
										 unsigned int width, unsigned int height, 
										 float max_x, float max_y, 
										 float aspect, bool bInvert, GLuint HostFBO)
{
	GLenum status;

	// Only for DX11 mode
	if(GetDX9())
		return false;

	if(width != m_TextureInfo.width || height != m_TextureInfo.height)
		return false;

	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);

	// Create an fbo if not already
	if(m_fbo == 0) glGenFramebuffersEXT(1, &m_fbo); 

	// Draw the shared texture into the user texture via an fbo
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

	// Destination is the fbo with local texture attached
	glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_TexID, 0);

	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);

	if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {
	
		// Draw the input texture
		glColor4f(1.f, 1.f, 1.f, 1.f);
		glEnable(TextureTarget);
		glBindTexture(TextureTarget, TextureID);

		GLfloat tc[4][2] = {0};

		// Invert texture coord to user requirements
		if(bInvert) {
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

		GLfloat verts[] =  {
						-aspect, -1.0,   // bottom left
						-aspect,  1.0,   // top left
						 aspect,  1.0,   // top right
						 aspect, -1.0 }; // bottom right
	
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		glTexCoordPointer(2, GL_FLOAT, 0, tc );
		glEnableClientState(GL_VERTEX_ARRAY);		
		glVertexPointer(2, GL_FLOAT, 0, verts );
		glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		glBindTexture(TextureTarget, 0);
		glDisable(TextureTarget);

	}
	else {
		PrintFBOstatus(status);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
		return false;
	}

	// restore the previous fbo - default is 0
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);

	// Copy the result in the local OpenGL texture to the DX11 shared texture
	return(WriteDX11texture(m_TexID, GL_TEXTURE_2D, width, height, false, HostFBO));

} // end DrawToDX11texture



// =============================
// DX9 texture surface functions
// =============================

//
// If a DX9 CPU texture surface has not been created  or is a different size, create a new one
//
bool spoutGLDXinterop::CheckDX9surface(unsigned int width, unsigned int height)
{
	HRESULT hr;
	D3DSURFACE_DESC desc; // Surface description

	if(g_DX9surface) {
		g_DX9surface->GetDesc(&desc);
		if(desc.Width != width || desc.Height != height) {
			g_DX9surface->Release();
			g_DX9surface = NULL;
		}
		else
			return true;
	}

	if(!g_DX9surface) {
		// Create a CPU accessable surface to get pixels from the shared texture
		hr = m_pDevice->CreateOffscreenPlainSurface(width, height, DX9format, D3DPOOL_SYSTEMMEM, &g_DX9surface, NULL);
		if(SUCCEEDED(hr))
			return true;
	}

	return false;
}


//
// Write a DirectX 9 system memory surface to the shared texture (sizes must be the same)
//
bool spoutGLDXinterop::WriteDX9surface(LPDIRECT3DSURFACE9 source_surface)
{
	// Only for DX9 mode
	if(!source_surface || !GetDX9())
		return false;

	if(spoutdx.CheckAccess(m_hAccessMutex)) {
		if(spoutdx.WriteDX9surface(m_pDevice, m_dxTexture, source_surface)) {
			spoutdx.AllowAccess(m_hAccessMutex);
			return true;
		}
	}

	spoutdx.AllowAccess(m_hAccessMutex);

	return false;

} // end WriteDX9surface



//
// COPY FROM A USER OPENGL TEXTURE TO THE SHARED DIRECTX TEXTURE BY WAY OF A DX9 SURFACE
//
bool spoutGLDXinterop::WriteDX9texture (GLuint TextureID,
										GLuint TextureTarget, 
										unsigned int width, 
										unsigned int height, 
										bool bInvert,
										GLuint HostFBO)
{
	D3DLOCKED_RECT d3dlr; // LockRect for data transfer
	HRESULT hr;

	// Only for DX9 mode
	if(!GetDX9())
		return false;

	if(width != m_TextureInfo.width || height != m_TextureInfo.height)
		return false;

	// If a CPU surface has not been created or it is a different size, create a new one
	if(!CheckDX9surface(width, height))
		return false;

	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);

	// Copy the user texture to the local texture - necessary for inversion
	CopyTexture(TextureID, TextureTarget, m_TexID, GL_TEXTURE_2D, width, height, bInvert, HostFBO);

	// Lock the system memory surface for write
	hr = g_DX9surface->LockRect(&d3dlr, NULL, D3DLOCK_DISCARD);
	if(SUCCEEDED(hr)) {
		// Extract the pixels from the local OpenGL texture to the BGRA staging texture buffer
		if(IsPBOavailable()) { // PBO method
			UnloadTexturePixels(m_TexID, GL_TEXTURE_2D, width, height, (unsigned char *)d3dlr.pBits, GL_BGRA_EXT, false, HostFBO);
		}
		else { 
			// Bind our local fbo - current fbo has to be passed in
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo); 
			// Attach the local rgba texture to the color buffer in our frame buffer
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_TexID, 0);
			GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
			if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {
				// read the pixels from the framebuffer
				glReadPixels(0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, d3dlr.pBits);
			}
			else {
				PrintFBOstatus(status);
			}
			// restore the previous fbo - default is 0
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
		}
		g_DX9surface->UnlockRect();

		// Copy the DX9 surface to the shared texture
		return WriteDX9surface(g_DX9surface);
	}

	return false;

} // end WriteDX9texture


//
// COPY FROM THE SHARED DIRECTX TEXTURE TO A USER OPENGL TEXTURE BY WAY OF A DX9 SURFACE
//
bool spoutGLDXinterop::ReadDX9texture (GLuint TextureID,
									   GLuint TextureTarget, 
									   unsigned int width, 
									   unsigned int height, 
									   bool bInvert,
									   GLuint HostFBO)
{
	D3DLOCKED_RECT d3dlr; // LockRect for data transfer
	HRESULT hr;
	IDirect3DSurface9 * SharedTextureSurface = NULL;

	// Only for DX9 mode
	if(!GetDX9())
		return false;

	if(width != m_TextureInfo.width || height != m_TextureInfo.height)
		return false;

	// If a CPU surface has not been created or it is a different size, create a new one
	if(!CheckDX9surface(width, height))
		return false;

	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);

	if(spoutdx.CheckAccess(m_hAccessMutex)) {
		// Create a local shared texture surface
		hr = m_dxTexture->GetSurfaceLevel(0, &SharedTextureSurface);
		if(SUCCEEDED(hr)) {
			// Use GetRenderTargetData to copy the shared texture data from device memory to system memory
			hr = m_pDevice->GetRenderTargetData(SharedTextureSurface, g_DX9surface);
			if(SUCCEEDED(hr)) {
				// Lock the system memory surface for read
				hr = g_DX9surface->LockRect(&d3dlr, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_READONLY);
				if(SUCCEEDED(hr)) {
					// Copy the surface pixels to the local OpenGL texture
					if(IsPBOavailable()) {
						LoadTexturePixels(m_TexID, GL_TEXTURE_2D, width, height, (const unsigned char *)d3dlr.pBits, GL_BGRA_EXT);
					}
					else {
						glBindTexture(GL_TEXTURE_2D, m_TexID);
						glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, d3dlr.pBits);
						glBindTexture(GL_TEXTURE_2D, 0);
					}
					// Copy the local texture to the user texture and invert as necessary
					CopyTexture(m_TexID, GL_TEXTURE_2D, TextureID, TextureTarget, width, height, bInvert, HostFBO);
					g_DX9surface->UnlockRect();
				}
				if(SharedTextureSurface) SharedTextureSurface->Release();
				spoutdx.AllowAccess(m_hAccessMutex);
				return true;
			} // endif GetRenderTargetData OK
		} // endif GetSurfaceLevel OK
	} // endif mutex access OK

	if(SharedTextureSurface) SharedTextureSurface->Release();
	spoutdx.AllowAccess(m_hAccessMutex);

	return false;
} // end ReadDX9texture



//
// COPY FROM A USER PIXEL BUFFER TO THE SHARED DIRECTX TEXTURE BY WAY OF A DX9 SURFACE
//
bool spoutGLDXinterop::WriteDX9pixels (const unsigned char *pixels,
									   unsigned int width,
									   unsigned int height,
									   GLenum glFormat,
									   bool bInvert)
{
	D3DLOCKED_RECT d3dlr; // LockRect for data transfer
	HRESULT hr;
	// IDirect3DSurface9 * SharedTextureSurface = NULL;

	// Only for DX9 mode
	if(!GetDX9())
		return false;

	if(width != m_TextureInfo.width || height != m_TextureInfo.height)
		return false;

	// If a CPU surface has not been created or it is a different size, create a new one
	if(!CheckDX9surface(width, height))
		return false;

	// Lock the system memory surface for write
	hr = g_DX9surface->LockRect(&d3dlr, NULL, D3DLOCK_DISCARD);
	if(SUCCEEDED(hr)) {
		// Write the user buffer to the bgra DX9 surface
		switch(glFormat) {
			case GL_BGRA_EXT: // direct copy
				spoutcopy.CopyPixels(pixels, (unsigned char *)d3dlr.pBits, width, height, GL_RGBA, bInvert);
				break;
			case GL_RGBA: // Convert the rgba pixels to bgra
				spoutcopy.rgba2bgra((void *)pixels, d3dlr.pBits, width, height, bInvert);
				break;
			case GL_RGB: // Convert the rgb pixels to bgra
				spoutcopy.rgb2bgra((void *)pixels, d3dlr.pBits, width, height, bInvert);
				break;
			case GL_BGR_EXT: // Convert the bgr pixels to bgra
				spoutcopy.bgr2bgra((void *)pixels, d3dlr.pBits, width, height, bInvert);
				break;
			default:
				break;
		}
		// Done writing to the surface
		g_DX9surface->UnlockRect();
		// Copy the surface to the shared texture
		return WriteDX9surface(g_DX9surface);
	}

	return false;

} // end WriteDX9pixels


//
// COPY FROM THE SHARED DIRECTX TEXTURE TO A USER PIXEL BUFFER BY WAY OF A DX9 SURFACE
//
bool spoutGLDXinterop::ReadDX9pixels (unsigned char *pixels,
									  unsigned int width, 
									  unsigned int height,
									  GLenum glFormat,
									  bool bInvert)
{
	D3DLOCKED_RECT d3dlr; // LockRect for data transfer
	HRESULT hr;
	IDirect3DSurface9 * SharedTextureSurface = NULL;

	// Only for DX9 mode
	if(!GetDX9())
		return false;

	if(width != m_TextureInfo.width || height != m_TextureInfo.height)
		return false;

	// If a CPU surface has not been created or it is a different size, create a new one
	if(!CheckDX9surface(width, height))
		return false;

	if(spoutdx.CheckAccess(m_hAccessMutex)) {
		// Create a local shared texture surface
		hr = m_dxTexture->GetSurfaceLevel(0, &SharedTextureSurface);
		if(SUCCEEDED(hr)) {
			// Use GetRenderTargetData to copy the shared texture data from device memory to system memory
			hr = m_pDevice->GetRenderTargetData(SharedTextureSurface, g_DX9surface);
			if(SUCCEEDED(hr)) {
				// Lock the system memory surface for read
				hr = g_DX9surface->LockRect(&d3dlr, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_READONLY);
				if(SUCCEEDED(hr)) {
					// Write BGRA pixels to the user buffer
					switch(glFormat) {
						case GL_BGRA_EXT: // direct copy
							spoutcopy.CopyPixels((const unsigned char *)d3dlr.pBits, pixels, width, height, GL_RGBA, bInvert);
							break;
						case GL_RGBA: // Convert bgra to rgba
							spoutcopy.bgra2rgba(d3dlr.pBits, (void *)pixels, width, height, bInvert);
							break;
						case GL_RGB: // Convert bgra to rgb
							spoutcopy.bgra2rgb(d3dlr.pBits, (void *)pixels, width, height, bInvert);
							break;
						case GL_BGR_EXT: // Convert bgra to bgr
							spoutcopy.bgra2bgr(d3dlr.pBits, (void *)pixels, width, height, bInvert);
							break;
						default:
							break;
					}
					g_DX9surface->UnlockRect();
				}
				if(SharedTextureSurface) SharedTextureSurface->Release();
				spoutdx.AllowAccess(m_hAccessMutex);
				return true;
			} // endif GetRenderTargetData OK
		} // endif GetSurfaceLevel OK
	} // endif mutex access OK

	if(SharedTextureSurface) SharedTextureSurface->Release();
	spoutdx.AllowAccess(m_hAccessMutex);

	return false;
} // end ReadDX9pixels


//
// Draw the shared DirectX 9 texture
// equivalent to DrawSharedTexture for the shared OpenGL texture
//
bool spoutGLDXinterop::DrawDX9texture(float max_x, float max_y, float aspect, bool bInvert, GLuint HostFBO)
{
	IDirect3DSurface9 * SharedTextureSurface = NULL;
	D3DLOCKED_RECT d3dlr; // LockRect for data transfer
	HRESULT hr;
	unsigned int width, height;

	UNREFERENCED_PARAMETER(HostFBO);

	// Only for DX9 mode
	if(!GetDX9())
		return false;

	width  = m_TextureInfo.width;
	height = m_TextureInfo.height;

	// If a CPU surface has not been created or it is a different size, create a new one
	if(!CheckDX9surface(width, height))
		return false;

	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);

	if(spoutdx.CheckAccess(m_hAccessMutex)) {
		// Create a local shared texture surface
		hr = m_dxTexture->GetSurfaceLevel(0, &SharedTextureSurface);
		if(SUCCEEDED(hr)) {
			// Use GetRenderTargetData to copy the shared texture data from device memory to system memory
			hr = m_pDevice->GetRenderTargetData(SharedTextureSurface, g_DX9surface);
			if(SUCCEEDED(hr)) {
				// Lock the system memory surface for read
				hr = g_DX9surface->LockRect(&d3dlr, NULL, D3DLOCK_NO_DIRTY_UPDATE | D3DLOCK_READONLY);
				if(SUCCEEDED(hr)) {

					// Copy the surface pixels to the local OpenGL texture
					glBindTexture(GL_TEXTURE_2D, m_TexID);
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, d3dlr.pBits);
					glBindTexture(GL_TEXTURE_2D, 0);
					g_DX9surface->UnlockRect();

					// Draw the local texture and invert as necessary
					SaveOpenGLstate(width, height);
					glEnable(GL_TEXTURE_2D);
					glBindTexture(GL_TEXTURE_2D, m_TexID);
					glColor4f(1.f, 1.f, 1.f, 1.f);
					glBegin(GL_QUADS);
					if(bInvert) {
						// DirectX coordinates are already inverted
						glTexCoord2f(0.0,   0.0);	glVertex2f(-aspect,-1.0); // lower left
						glTexCoord2f(0.0,   max_y);	glVertex2f(-aspect, 1.0); // upper left
						glTexCoord2f(max_x, max_y);	glVertex2f( aspect, 1.0); // upper right
						glTexCoord2f(max_x, 0.0);	glVertex2f( aspect,-1.0); // lower right
					}
					else {
						glTexCoord2f(0.0,	max_y);	glVertex2f(-aspect,-1.0); // lower left
						glTexCoord2f(0.0,	0.0);	glVertex2f(-aspect, 1.0); // upper left
						glTexCoord2f(max_x, 0.0);	glVertex2f( aspect, 1.0); // upper right
						glTexCoord2f(max_x, max_y);	glVertex2f( aspect,-1.0); // lower right
					}
					glEnd();
					glBindTexture(GL_TEXTURE_2D, 0);
					glDisable(GL_TEXTURE_2D);
					RestoreOpenGLstate();
				}
				if(SharedTextureSurface) SharedTextureSurface->Release();
				spoutdx.AllowAccess(m_hAccessMutex);
				return true;
			} // endif GetRenderTargetData OK
		} // endif GetSurfaceLevel OK
	} // endif mutex access OK

	if(SharedTextureSurface) SharedTextureSurface->Release();
	spoutdx.AllowAccess(m_hAccessMutex);

	return false;

} // end DrawDX9texture


//
// DRAW AN OPENGL TEXTURE INTO THE SHARED DX9 texture
// equivalent to DrawToSharedTexture for the shared OpenGL texture
//
bool spoutGLDXinterop::DrawToDX9texture(GLuint TextureID, GLuint TextureTarget, 
										unsigned int width, unsigned int height, 
										float max_x, float max_y, float aspect, 
										bool bInvert, GLuint HostFBO)
{
	// bool bRet = false;
	GLenum status;

	// Only for DX9 mode
	if(!GetDX9())
		return false;

	if(width != m_TextureInfo.width || height != m_TextureInfo.height)
		return false;

	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);

	// Draw the input texture into the local texture via an fbo
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
	// Destination is the fbo with local texture attached
	glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_TexID, 0);
	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {

		// Draw the input texture
		glColor4f(1.f, 1.f, 1.f, 1.f);

		glEnable(TextureTarget);
		glBindTexture(TextureTarget, TextureID);
		GLfloat tc[4][2] = {0};
		// Invert texture coord to user requirements
		if(bInvert) {
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
		GLfloat verts[] = { -aspect, -1.0,   // bottom left
							-aspect,  1.0,   // top left
							 aspect,  1.0,   // top right
							 aspect, -1.0 }; // bottom right
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		glTexCoordPointer(2, GL_FLOAT, 0, tc );
		glEnableClientState(GL_VERTEX_ARRAY);		
		glVertexPointer(2, GL_FLOAT, 0, verts );
		glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		glBindTexture(TextureTarget, 0);
		glDisable(TextureTarget);

	}
	else {
		PrintFBOstatus(status);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
		return false;
	}

	// restore the previous fbo - default is 0
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);

	// Copy the result in the local OpenGL texture to the shared DX9 texture
	return(WriteDX9texture (m_TexID, GL_TEXTURE_2D, width, height, false, HostFBO));

} // end DrawToDX9texture


//
//	GL/DX Interop lock
//
//	A return value of TRUE indicates that all objects were
//    successfully locked.  A return value of FALSE indicates an
//    error. If the function returns FALSE, none of the objects will be locked.
//
//	Attempting to access an interop object via GL when the object is
//    not locked, or attempting to access the DirectX resource through
//    the DirectX API when it is locked by GL, will result in undefined
//    behavior and may result in data corruption or program
//    termination. Likewise, passing invalid interop device or object
//    handles to this function has undefined results, including program
//    termination.
//
//	Note that only one GL context may hold the lock on the
//    resource at any given time --- concurrent access from multiple GL
//    contexts is not currently supported.
//
//	http://halogenica.net/sharing-resources-between-directx-and-opengl/
//
//	This lock triggers the GPU to perform the necessary flushing and stalling
//	to guarantee that the surface has finished being written to before reading from it. 
//
//	DISCUSSION: The Lock/Unlock calls serve as synchronization points
//    between OpenGL and DirectX. They ensure that any rendering
//    operations that affect the resource on one driver are complete
//    before the other driver takes ownership of it.
//
//	This function assumes only one object to 
//
//	Must return S_OK (0) - otherwise the error can be checked.
//
HRESULT spoutGLDXinterop::LockInteropObject(HANDLE hDevice, HANDLE *hObject)
{
	DWORD dwError;
	HRESULT hr;

	if(hDevice == NULL || hObject == NULL || *hObject == NULL) {
		return E_HANDLE;
	}

	// lock dx object
	if(wglDXLockObjectsNV(hDevice, 1, hObject) == TRUE) {
		return S_OK;
	}
	else {
		dwError = GetLastError();
		switch (dwError) {
			case ERROR_BUSY :			// One or more of the objects in <hObjects> was already locked.
				hr = E_ACCESSDENIED;	// General access denied error
				// printf("	spoutGLDXinterop::LockInteropObject ERROR_BUSY\n");
				break;
			case ERROR_INVALID_DATA :	// One or more of the objects in <hObjects>
										// does not belong to the interop device
										// specified by <hDevice>.
				hr = E_ABORT;			// Operation aborted
				// printf("	spoutGLDXinterop::LockInteropObject ERROR_INVALID_DATA\n");
				break;
			case ERROR_LOCK_FAILED :	// One or more of the objects in <hObjects> failed to 
				hr = E_ABORT;			// Operation aborted
				// printf("	spoutGLDXinterop::LockInteropObject ERROR_LOCK_FAILED\n");
				break;
			default:
				hr = E_FAIL;			// unspecified error
				// printf("	spoutGLDXinterop::LockInteropObject UNKNOWN_ERROR\n");
				break;
		} // end switch
	} // end false

	return hr;

} // LockInteropObject


//
// Must return S_OK (0) - otherwise the error can be checked.
//
HRESULT spoutGLDXinterop::UnlockInteropObject(HANDLE hDevice, HANDLE *hObject)
{
	DWORD dwError;
	HRESULT hr;

	if(hDevice == NULL || hObject == NULL || *hObject == NULL) {
		return E_HANDLE;
	}

	if (wglDXUnlockObjectsNV(hDevice, 1, hObject) == TRUE) {
		return S_OK;
	}
	else {
		dwError = GetLastError();
		switch (dwError) {
			case ERROR_NOT_LOCKED :
				hr = E_ACCESSDENIED;
				// printf("	spoutGLDXinterop::UnLockInteropObject ERROR_NOT_LOCKED\n");
				break;
			case ERROR_INVALID_DATA :
				// printf("	spoutGLDXinterop::UnLockInteropObject ERROR_INVALID_DATA\n");
				hr = E_ABORT;
				break;
			case ERROR_LOCK_FAILED :
				hr = E_ABORT;
				// printf("	spoutGLDXinterop::UnLockInteropObject ERROR_LOCK_FAILED\n");
				break;
			default:
				hr = E_FAIL;
				// printf("	spoutGLDXinterop::UnLockInteropObject UNKNOWN_ERROR\n");
				break;
		} // end switch
	} // end fail

	return hr;

} // end UnlockInteropObject


// Set DX9 off or no with a DX11 compatibility check
// Returns false if SetDX9(false) failed due to DX11 copmaptibility check
bool spoutGLDXinterop::UseDX9(bool bDX9)
{
	bool bRet = false;
	if(bDX9 == true) {
		// Set to DirectX 9
		// DirectX 11 is the default but is checked by OpenDirectX.
		m_bUseDX9 = bDX9;
		bRet = true;
	}
	// Check for DirectX 11 availability if the user requested it
	else if(DX11available()) {
		m_bUseDX9 = false;
		bRet = true;
	}
	else {
		// Set to use DirectX 9 if DirectX 11 is not available
		m_bUseDX9 = true;
		bRet = false;
	}

	if(!spoutdx.WriteDwordToRegistry((DWORD)m_bUseDX9, "Software\\Leading Edge\\Spout", "DX9")) {
		return false;
	}

	return bRet;
}

// Test function
// Check support for DirectX 11
// It is checked with OpenDirectX but this might not have been called yet.
// The user can call this after the UseDX9 call as well as testing for it's false return.
bool spoutGLDXinterop::isDX9()
{
	if(!DX11available()) {
		m_bUseDX9 = true;
	}
	// Otherwise return what has been set
	// This can be checked again after directX initialization
	// to find out if DirectX 11 initialization failed
	return m_bUseDX9;
}

// Set flag - does not require OpenGL context - UseDX9 does
bool spoutGLDXinterop::SetDX9(bool bDX9)
{
	if(spoutdx.WriteDwordToRegistry((DWORD)m_bUseDX9, "Software\\Leading Edge\\Spout", "DX9")) {
		m_bUseDX9 = bDX9;
		return true;
	}
	return false;
}

// Return existing flag - does not require OpenGL context - isDX9 does
bool spoutGLDXinterop::GetDX9()
{
	return m_bUseDX9;
}


bool spoutGLDXinterop::SetCPUmode(bool bCPU)
{
	if(spoutdx.WriteDwordToRegistry((DWORD)bCPU, "Software\\Leading Edge\\Spout", "CPU")) {
		m_bUseCPU = bCPU;
		return true;
	}
	return false;
}


// Return CPU texture processing flag from the registry
// It is set by the user via the "SpoutDirectX" utility for 2.006 and later
// and it is also picked up when the SpoutGLDXinterop class initializes
// including a compatibility check and set to true for incompatible hardware.
bool spoutGLDXinterop::GetCPUmode()
{
	DWORD dwCPU = 0;
	if(spoutdx.ReadDwordFromRegistry(&dwCPU, "Software\\Leading Edge\\Spout", "CPU")) {
		m_bUseCPU = (dwCPU == 1);
	}
	return m_bUseCPU;
}


bool spoutGLDXinterop::GetMemoryShareMode()
{
	DWORD dwMem = 0;
	if(spoutdx.ReadDwordFromRegistry(&dwMem, "Software\\Leading Edge\\Spout", "MemoryShare")) {
		m_bUseMemory = (dwMem == 1);
	}	
	return m_bUseMemory;
}


// User set by the SpoutDXmode utility
bool spoutGLDXinterop::SetMemoryShareMode(bool bMem)
{
	if(spoutdx.WriteDwordToRegistry((DWORD)bMem, "Software\\Leading Edge\\Spout", "MemoryShare")) {
		m_bUseMemory = bMem;
		return true;
	}
	return false;
}

//
// Return sharing mode set by user or by an application
// Reads the registry - avoid repeated use every frame.
//
// 0 - texture, 1 - cpu, 2 - memory
//
int spoutGLDXinterop::GetShareMode()
{
	if(GetMemoryShareMode()) {
			return 2;
	}
	else if (GetCPUmode()) {
			return 1;
	}
	else {
		return 0;
	}
}


//---------------------------------------------------------
// 0 - texture, 1 - cpu, 2 - memory
bool spoutGLDXinterop::SetShareMode(int mode)
{
	switch (mode) {

		case 2 : // Shared memory
			if(spoutdx.WriteDwordToRegistry(1, "Software\\Leading Edge\\Spout", "MemoryShare")) {
				if(spoutdx.WriteDwordToRegistry(0, "Software\\Leading Edge\\Spout", "CPU")) {
					m_bUseMemory = true;
					m_bUseCPU = false;
					return true;
				}
			}
			break;

		case 1: // CPU DX texture
			if(spoutdx.WriteDwordToRegistry(1, "Software\\Leading Edge\\Spout", "CPU")) {
				if(spoutdx.WriteDwordToRegistry(0, "Software\\Leading Edge\\Spout", "MemoryShare")) {
					m_bUseCPU = true;
					m_bUseMemory = false;
					return true;
				}
			}
			break;

		default : // GL/DX texture
			if(spoutdx.WriteDwordToRegistry(0, "Software\\Leading Edge\\Spout", "CPU")) {
				if(spoutdx.WriteDwordToRegistry(0, "Software\\Leading Edge\\Spout", "MemoryShare")) {
					m_bUseCPU = false;
					m_bUseMemory = false;
					return true;
				}
			}
			break;
	}

	return false;
}


GLuint spoutGLDXinterop::GetGLtextureID()
{
	return m_glTexture;
}


void spoutGLDXinterop::SetDX11format(DXGI_FORMAT textureformat)
{
	DX11format = textureformat;
}

void spoutGLDXinterop::SetDX9format(D3DFORMAT textureformat)
{
	DX9format = textureformat;
}

// Set graphics adapter for Spout output
bool spoutGLDXinterop::SetAdapter(int index) 
{
	if(spoutdx.SetAdapter(index)) {
		// printf("SpoutGLDXinterop::SetAdapter(%d) OK\n", index);
		return true;
	}

	spoutdx.SetAdapter(-1); // make sure globals are reset to default

	return false;
}

// Get current adapter index
int spoutGLDXinterop::GetAdapter() 
{
	return spoutdx.GetAdapter();
}


// Get the path of the host that produced the sender
// from the description string in the sender info memory map
// Description is defined as wide chars, but the path is stored as byte chars
// Not a permanent thing - just for testing.
// The description string could be used for other things in future
bool spoutGLDXinterop::GetHostPath(const char *sendername, char *hostpath, int maxchars)
{
	SharedTextureInfo info;
	int n;

	if(!senders.getSharedInfo(sendername, &info))
		return false;

	n = maxchars;
	if(n > 256) n = 256; // maximum field width in shared memory

	strcpy_s(hostpath, n, (char *)info.description);

	return true;
}


// Get the number of graphics adapters in the system
int spoutGLDXinterop::GetNumAdapters()
{
	return spoutdx.GetNumAdapters();
}

// Get an adapter name
bool spoutGLDXinterop::GetAdapterName(int index, char *adaptername, int maxchars)
{
	return spoutdx.GetAdapterName(index, adaptername, maxchars);
}


// Needs OpenGL context
int spoutGLDXinterop::GetVerticalSync()
{
	if(!m_bExtensionsLoaded) m_bExtensionsLoaded = LoadGLextensions();

	// needed for both sender and receiver
	if(m_bSWAPavailable) {
		return(wglGetSwapIntervalEXT());
	}
	return 0;
}


bool spoutGLDXinterop::SetVerticalSync(bool bSync)
{
	if(!m_bExtensionsLoaded) m_bExtensionsLoaded = LoadGLextensions();

	if(m_bSWAPavailable) {
		if(bSync)
			wglSwapIntervalEXT(1); // lock to monitor vsync
		else
			wglSwapIntervalEXT(0); // unlock from monitor vsync
		return true;
	}
	return false;
}

// For debugging only - disable console out for release
void spoutGLDXinterop::GLerror() {
	GLenum err;
	while ((err = glGetError()) != GL_NO_ERROR) {
		// printf("GL error = %d (0x%x)\n", err, err);
		// gluErrorString needs glu32.lib
		// printf("GL error = %d (0x%x) %s\n", err, err, gluErrorString(err));
	}
}	


void spoutGLDXinterop::PrintFBOstatus(GLenum status)
{
	// printf("FBO error status = %d (0x%x)\n", status, status);
	if (status == GL_FRAMEBUFFER_UNSUPPORTED_EXT)
		printf("GL_FRAMEBUFFER_UNSUPPORTED_EXT\n");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT)
		printf("GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT\n");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT)
		printf("GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT\n");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT)
		printf("GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT - width-height problems?\n");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT)
		printf("GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT\n");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT)
		printf("GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT\n");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT)
		printf("GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT\n");
	// else if (status == GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT)
	// 	printf("GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT\n");
	else 
		printf("\n Unknown Code: glCheckFramebufferStatusEXT returned %i (%x)\n",status, status);
	GLerror();
}
	

// =======================================================
//               2.005 Memoryshare functions
// =======================================================

//
// Write user texture pixel data to shared memory
// rgba textures only
//
bool spoutGLDXinterop::WriteMemory (GLuint TexID, 
									GLuint TextureTarget, 
									unsigned int width, 
									unsigned int height, 
									bool bInvert,
									GLuint HostFBO)
{

	unsigned char *pBuffer = memoryshare.LockSenderMemory();
	if(!pBuffer) {
		return false;
	}

	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);

	// Copy the user texture to the local rgba texture and invert as necessary
	// There is not much speed gain bypassing the intermediate texture
	// so create it and then it will always be RGBA for the functions to follow
	CopyTexture(TexID, TextureTarget, m_TexID, GL_TEXTURE_2D, width, height, bInvert, HostFBO);

	// Read the local opengl texture into the rgba memory map buffer
	// Use PBO if supported
	if(IsPBOavailable()) {
		UnloadTexturePixels(m_TexID, GL_TEXTURE_2D, width, height, pBuffer, GL_RGBA, false, HostFBO);
	}
	else {
		// printf("glGetTexImage\n");
		glBindTexture(GL_TEXTURE_2D, m_TexID);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)pBuffer);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	memoryshare.UnlockSenderMemory();

	return true;
}

//
// Read shared memory to texture pixel data
// rgba textures only
//
bool spoutGLDXinterop::ReadMemory(GLuint TexID, 
								  GLuint TextureTarget,
								  unsigned int width,
								  unsigned int height,
								  bool bInvert,
								  GLuint HostFBO)
{
	unsigned char *pBuffer = memoryshare.LockSenderMemory();
	if(!pBuffer) return false;

	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);
	
	// Copy the rgba memory map pixels to the local rgba opengl texture
	if(IsPBOavailable()) {
		LoadTexturePixels(m_TexID, GL_TEXTURE_2D, width, height, (const unsigned char *)pBuffer, GL_RGBA);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, m_TexID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)pBuffer);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	// Copy the local rgba texture to the user texture and invert as necessary
	CopyTexture(m_TexID, GL_TEXTURE_2D, TexID, TextureTarget, width, height, bInvert, HostFBO);

	memoryshare.UnlockSenderMemory();
	
	return true;

}

//
// Write image pixels to shared memory
// rgba, bgra, rgb, bgr source buffers supported
//
bool spoutGLDXinterop::WriteMemoryPixels(const unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat, bool bInvert)
{
	unsigned char *pBuffer = memoryshare.LockSenderMemory();

	if(!pBuffer)
		return false;

	// Write pixels to shared memory
	if(glFormat == GL_RGBA) {
		spoutcopy.CopyPixels(pixels, pBuffer, width, height, GL_RGBA, bInvert);
	}
	else if(glFormat == 0x80E1) { // GL_BGRA_EXT if supported
		spoutcopy.bgra2rgba((void *)pixels, (void *)pBuffer, width, height, bInvert);
	}
	else if(glFormat == 0x80E0) { // GL_BGR_EXT if supported
		spoutcopy.bgr2rgba((void *)pixels, (void *)pBuffer, width, height, bInvert);
	}
	else if(glFormat == GL_RGB) {
		spoutcopy.rgb2rgba((void *)pixels, (void *)pBuffer, width, height, bInvert);
	}

	memoryshare.UnlockSenderMemory();

	return true;

}


//
// Read shared memory to image pixels
// rgba, bgra, rgb, bgr destination buffers supported
// Most efficient if the receiving buffer is rgba
// Invert currently not used
//
bool spoutGLDXinterop::ReadMemoryPixels(unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat, bool bInvert)
{
	unsigned char *pBuffer = memoryshare.LockSenderMemory();

	if(!pBuffer)
		return false;

	// Read pixels from shared memory
	if(glFormat == GL_RGBA) {
		spoutcopy.CopyPixels(pBuffer, pixels, width, height, GL_RGBA, bInvert);
	}
	else if(glFormat == 0x80E1) { // GL_BGRA_EXT if supported
		spoutcopy.rgba2bgra((void *)pBuffer, (void *)pixels, width, height, bInvert);
	}
	else if(glFormat == 0x80E0) { // GL_BGR_EXT if supported
		spoutcopy.rgba2bgr((void *)pBuffer, (void *)pixels, width, height, bInvert);
	}
	else if(glFormat == GL_RGB) {
		spoutcopy.rgba2rgb((void *)pBuffer, (void *)pixels, width, height, bInvert);
	}

	memoryshare.UnlockSenderMemory();

	return true;

}


// DRAW A TEXTURE INTO SHARED MEMORY - equivalent to DrawToSharedTexture
bool spoutGLDXinterop::DrawToSharedMemory(GLuint TexID, GLuint TextureTarget, 
										  unsigned int width, unsigned int height, 
										  float max_x, float max_y, float aspect, 
										  bool bInvert, GLuint HostFBO)
{
	unsigned int memWidth, memHeight;
	GLenum status;


	// Get the memoryshare size
	if(!memoryshare.GetSenderMemorySize(memWidth, memHeight))
		return false;

	// Sender size check - quit if not equal
	if(width != memWidth || height != memHeight) 
		return false;

	unsigned char *pBuffer = memoryshare.LockSenderMemory();
	if(!pBuffer) {
		return false;
	}

	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);

	// Create an fbo if not already
	if(m_fbo == 0) glGenFramebuffersEXT(1, &m_fbo); 

	//
	// Draw the input texture into the local texture via an fbo
	//
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

	// Destination is the fbo with local texture attached
	glFramebufferTexture2DEXT(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_TexID, 0);

	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {
	
		// Draw the input texture
		glColor4f(1.f, 1.f, 1.f, 1.f);
		glEnable(TextureTarget);
		glBindTexture(TextureTarget, TexID);

		GLfloat tc[4][2] = {0};

		// Invert texture coord to user requirements
		if(bInvert) {
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

		GLfloat verts[] =  {
						-aspect, -1.0,   // bottom left
						-aspect,  1.0,   // top left
						 aspect,  1.0,   // top right
						 aspect, -1.0 }; // bottom right
	
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		glTexCoordPointer(2, GL_FLOAT, 0, tc );
		glEnableClientState(GL_VERTEX_ARRAY);		
		glVertexPointer(2, GL_FLOAT, 0, verts );
		glDrawArrays( GL_TRIANGLE_FAN, 0, 4 );
		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);

		glBindTexture(TextureTarget, 0);
		glDisable(TextureTarget);

	}
	else {
		PrintFBOstatus(status);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
		memoryshare.UnlockSenderMemory();
		return false;
	}

	// restore the previous fbo - default is 0
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);

	// Now read the local opengl texture into the memory map buffer
	// Use PBO if supported
	if(IsPBOavailable()) {
		UnloadTexturePixels(m_TexID, GL_TEXTURE_2D, width, height, pBuffer, GL_RGBA, false, HostFBO);
	}
	else {
		glBindTexture(GL_TEXTURE_2D, m_TexID);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)pBuffer);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	memoryshare.UnlockSenderMemory();


	return true;
}


//
// Draw shared memory via texture - equivalent to DrawSharedTexture
//
bool spoutGLDXinterop::DrawSharedMemory(float max_x, float max_y, float aspect, bool bInvert)
{
	unsigned int width, height;

	// Get the memoryshare size
	if(!memoryshare.GetSenderMemorySize(width, height))
		return false;

	// Find the shared memory buffer pointer
	unsigned char *pBuffer = memoryshare.LockSenderMemory();
	if(!pBuffer) return false;

	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, GL_RGBA, width, height, m_TexWidth, m_TexHeight);

	// if(IsPBOavailable()) {
		// LoadTexturePixels(m_TexID, GL_TEXTURE_2D, width, height, (const unsigned char *)pBuffer, GL_RGBA);
	// }
	// else {
		glBindTexture(GL_TEXTURE_2D, m_TexID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)pBuffer);
		glBindTexture(GL_TEXTURE_2D, 0);
	// }

	// Draw the texture
	SaveOpenGLstate(width, height);
	glColor4f(1.f, 1.f, 1.f, 1.f);
	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, m_TexID);
	glBegin(GL_QUADS);
	if(bInvert) {
		glTexCoord2f(0.0,	max_y);	glVertex2f(-aspect,-1.0); // lower left
		glTexCoord2f(0.0,	0.0);	glVertex2f(-aspect, 1.0); // upper left
		glTexCoord2f(max_x, 0.0);	glVertex2f( aspect, 1.0); // upper right
		glTexCoord2f(max_x, max_y);	glVertex2f( aspect,-1.0); // lower right
	}
	else {
		glTexCoord2f(0.0,   0.0);	glVertex2f(-aspect,-1.0); // lower left
		glTexCoord2f(0.0,   max_y);	glVertex2f(-aspect, 1.0); // upper left
		glTexCoord2f(max_x, max_y);	glVertex2f( aspect, 1.0); // upper right
		glTexCoord2f(max_x, 0.0);	glVertex2f( aspect,-1.0); // lower right
	}
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_TEXTURE_2D);
	RestoreOpenGLstate();

	memoryshare.UnlockSenderMemory();

	return true;
}


//
// OpenGL utilities
//

//
// COPY AN OPENGL TEXTURE TO ANOTHER OPENGL TEXTURE OF THE SAME SIZE
// 
bool spoutGLDXinterop::CopyTexture(	GLuint SourceID,
									GLuint SourceTarget,
									GLuint DestID,
									GLuint DestTarget,
									unsigned int width, 
									unsigned int height, 
									bool bInvert, 
									GLuint HostFBO)
{
	GLenum status;

	// Create an fbo if not already
	if(m_fbo == 0)
		glGenFramebuffersEXT(1, &m_fbo); 

	// bind the FBO (for both, READ_FRAMEBUFFER_EXT and DRAW_FRAMEBUFFER_EXT)
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

	// Attach the Source texture to the color buffer in our frame buffer
	glFramebufferTexture2DEXT(READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, SourceTarget, SourceID, 0);
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

	// Attach destination texture (the texture we write into) to second attachment point
	glFramebufferTexture2DEXT(DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, DestTarget, DestID, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);

	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if(status == GL_FRAMEBUFFER_COMPLETE_EXT) {
		if(m_bBLITavailable) {
			if(bInvert) {
				// Blit method with checks - 0.75 - 0.85 msec
				// copy one texture buffer to the other while flipping upside down 
				// (OpenGL and DirectX have different texture origins)
				glBlitFramebufferEXT(0, 0,			// srcX0, srcY0, 
									 width, height, // srcX1, srcY1
									 0, height,		// dstX0, dstY0,
									 width, 0,		// dstX1, dstY1,
									 GL_COLOR_BUFFER_BIT, GL_NEAREST); // GLbitfield mask, GLenum filter
			}
			else { 
				// Do not flip during blit
				glBlitFramebufferEXT(0, 0,			// srcX0, srcY0, 
									 width, height,	// srcX1, srcY1
									 0, 0,			// dstX0, dstY0,
									 width, height,	// dstX1, dstY1,
									 GL_COLOR_BUFFER_BIT, GL_NEAREST); // GLbitfield mask, GLenum filter
			}
		}
		else {
			// No fbo blit extension
			// Copy from the fbo (source texture attached) to the dest texture
			glBindTexture(DestTarget, DestID);
			glCopyTexSubImage2D(DestTarget, 0, 0, 0, 0, 0, width, height);
			glBindTexture(DestTarget, 0);
		}
	}
	else {
		PrintFBOstatus(status);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
		return false;
	}

	// restore the previous fbo - default is 0
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);

	return true;

} // end CopyTexture


// If an OpenGL texture has not been created or it is a different size, create a new one
void spoutGLDXinterop::CheckOpenGLTexture(GLuint &texID, GLenum GLformat,
										  unsigned int newWidth, unsigned int newHeight,
										  unsigned int &texWidth, unsigned int &texHeight)
{
	if(texID == 0 || newWidth != texWidth || newHeight != texHeight) {
		InitTexture(texID, GLformat, newWidth, newHeight);
		texWidth = newWidth;
		texHeight = newHeight;
	}
}


// Initialize local OpenGL texture
void spoutGLDXinterop::InitTexture(GLuint &texID, GLenum GLformat, unsigned int width, unsigned int height)
{
	if(texID != 0) glDeleteTextures(1, &texID);	
	glGenTextures(1, &texID);

	glBindTexture(GL_TEXTURE_2D, texID);
	glTexImage2D(GL_TEXTURE_2D, 0, GLformat, width, height, 0, GLformat, GL_UNSIGNED_BYTE, NULL); 
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

}


void spoutGLDXinterop::SaveOpenGLstate(unsigned int width, unsigned int height, bool bFitWindow)
{
	float dim[4];
	float vpScaleX, vpScaleY, vpWidth, vpHeight;
	int vpx, vpy;

	// save texture state, client state, etc.
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);

	glMatrixMode(GL_TEXTURE);
	glPushMatrix();
	glLoadIdentity();

	glPushAttrib(GL_TRANSFORM_BIT);

	// find the current viewport dimensions in order to scale to the aspect ratio required
	glGetFloatv(GL_VIEWPORT, dim);

	// Fit to window
	if(bFitWindow) {
		// Scale both width and height to the current viewport size
		vpScaleX = dim[2]/(float)width;
		vpScaleY = dim[3]/(float)height;
		vpWidth  = (float)width  * vpScaleX;
		vpHeight = (float)height * vpScaleY;
		vpx = vpy = 0;
	}
	else {
		// Preserve aspect ratio of the sender
		// and fit to the width or the height
		vpWidth = dim[2];
		vpHeight = ((float)height/(float)width)*vpWidth;
		if(vpHeight > dim[3]) {
			vpHeight = dim[3];
			vpWidth = ((float)width/(float)height)*vpHeight;
		}
		vpx = (int)(dim[2]-vpWidth)/2;;
		vpy = (int)(dim[3]-vpHeight)/2;
	}

	glViewport((int)vpx, (int)vpy, (int)vpWidth, (int)vpHeight);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity(); // reset the current matrix back to its default state
	glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0f, 1.0f);

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
}


void spoutGLDXinterop::RestoreOpenGLstate()
{
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
		
	glPopAttrib();
		
	glMatrixMode(GL_TEXTURE);
	glPopMatrix();

	glPopClientAttrib();			
	glPopAttrib();

}




// Get Spout version from the registry if the key exists
// Set by the Spout installer for 2.005 and greater
DWORD spoutGLDXinterop::GetSpoutVersion()
{
	DWORD dwVersion = 0;
	if(spoutdx.ReadDwordFromRegistry(&dwVersion, "Software\\Leading Edge\\Spout", "Version")) {
		// Return the version number (2005, 2006, etc.)
		return dwVersion; 
	}
	// Otherwise return 0 for earlier than 2.005
	return 0;
}


//
// Create an OpenGL window and context for situations where there is none
// Not used if applications already have an OpenGL context
// Always call CloseOpenGL afterwards.
//
bool spoutGLDXinterop::InitOpenGL()
{
	// For InitOpenGL and CloseOpenGL
	m_hdc = NULL;
	m_hwndButton = NULL;
	m_hRc = NULL;

	HGLRC glContext = wglGetCurrentContext();

	if(glContext == NULL) {

		// We only need an OpenGL context with no render window because we don't draw to it
		// so create an invisible dummy button window. This is then independent from the host
		// program window (GetForegroundWindow). If SetPixelFormat has been called on the
		// host window it cannot be called again. This caused a problem in Mapio.
		// https://msdn.microsoft.com/en-us/library/windows/desktop/dd369049%28v=vs.85%29.aspx
		//
		// CS_OWNDC allocates a unique device context for each window in the class. 
		//
		if(!m_hwndButton || !IsWindow(m_hwndButton)) {
			m_hwndButton = CreateWindowA("BUTTON",
				            "SpoutOpenGL",
							WS_OVERLAPPEDWINDOW | CS_OWNDC,
							0, 0, 32, 32,
							NULL, NULL, NULL, NULL);
		}

		if(!m_hwndButton) { 
			MessageBoxA(NULL, "Error 1", "InitOpenGL", MB_OK);
			return false; 
		}

		m_hdc = GetDC(m_hwndButton);
		if(!m_hdc) { 
			MessageBoxA(NULL, "Error 2", "InitOpenGL", MB_OK); 
			return false; 
		}
			
		PIXELFORMATDESCRIPTOR pfd;
		ZeroMemory( &pfd, sizeof( pfd ) );
		pfd.nSize = sizeof( pfd );
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cDepthBits = 16;
		pfd.iLayerType = PFD_MAIN_PLANE;
		int iFormat = ChoosePixelFormat(m_hdc, &pfd);
		if(!iFormat) { 
			MessageBoxA(NULL, "Error 3", "InitOpenGL", MB_OK);
			return false; 
		}

		if(!SetPixelFormat(m_hdc, iFormat, &pfd)) {
			DWORD dwError = GetLastError();
			// 2000 (0x7D0) The pixel format is invalid.
			// Caused by repeated call of  the SetPixelFormat function
			char temp[128];
			sprintf_s(temp, "InitOpenGL Error 4\nSetPixelFormat\nError %d (%x)", dwError, dwError);
			MessageBoxA(NULL, temp, "InitOpenGL", MB_OK); 
			return false; 
		}

		m_hRc = wglCreateContext(m_hdc);
		if(!m_hRc) { 
			MessageBoxA(NULL, "Error 5", "InitOpenGL", MB_OK); 
			return false; 
		}

		wglMakeCurrent(m_hdc, m_hRc);
		if(wglGetCurrentContext() == NULL) {
			MessageBoxA(NULL, "Error 6", "InitOpenGL", MB_OK);
			return false; 
		}
	}

	return true;
}


bool spoutGLDXinterop::CloseOpenGL()
{		
	// Properly kill the OpenGL window
	if (m_hRc) {
		if (!wglMakeCurrent(NULL,NULL))	{ // Are We Able To Release The DC And RC Contexts?
			MessageBoxA(NULL,"Release Of DC And RC Failed.","CloseOpenGL", MB_OK | MB_ICONINFORMATION);
			return false; 
		}

		if (!wglDeleteContext(m_hRc)) { // Are We Able To Delete The RC?
			MessageBoxA(NULL,"Release Rendering Context Failed.","CloseOpenGL",MB_OK | MB_ICONINFORMATION);
			return false; 
		}
		m_hRc=NULL;
	}

	if (m_hdc && !ReleaseDC(m_hwndButton, m_hdc)) { // Are We Able To Release The DC
		MessageBoxA(NULL,"Release Device Context Failed.", "CloseOpenGL", MB_OK | MB_ICONINFORMATION);
		m_hdc=NULL;
		return false; 
	}

	if (m_hwndButton && !DestroyWindow(m_hwndButton)) { // Are We Able To Destroy The Window?
		MessageBoxA(NULL,"Could Not Release hWnd.","CloseOpenGL",MB_OK | MB_ICONINFORMATION);
		m_hwndButton=NULL;
		return false; 
	}

	return true;
}

