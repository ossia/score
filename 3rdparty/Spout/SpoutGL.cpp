//
//		SpoutGL
//
//		Base class for OpenGL texture sharing using the NVIDIA GL/DX interop extensions
//
//		See also - spoutDirectX, spoutSenderNames
//
// ====================================================================================
//		Revisions :
//
//		07.10.20	- Started class based on previous work with SpoutGLDXinterop.cpp
//					  for 2.006 and 2.007 beta : 15-07-14 - 03-09-20
//					  with reference to the SpoutDX class for consolidation of global variables.
//					  Compatibility with NVIDIA GL/DX interop is tested with fall-back to CPU share
//					  using DirectX11 staging textures for failure.
//					  MemoryShare is supported for receive only.
//					  DX9 support is removed.
//		09.12.20	- Correct ReadDX11texture for staging texture pitch
//		27.12.20	- Functions allocated to SpoutSDK class where appropriate
//		14.01.21	- Add GetDX11Device() and GetDX11Context()
//					  add bInvert and HostFBO options to WriteTextureReadback
//		04.02.21	- SetHostPath and SetSenderCPUmode public
//					  Add GetCPUshare and SetCPUshare for forced CPU share testing
//		05.02.21	- Introduced m_bTextureShare and m_bCPUshare flags to handle mutiple options
//		08.02.21	- WriteDX11texture, ReadTextureData, OpenSpout, LoadGLextensions cleanup
//					  OpenSpout look for DirectX to prevent repeat
//		26.02.21	- Change m_bSenderCPUmode to m_bSenderCPU
//					- Add m_bSenderGLDX
//					- Change SetSenderCPUmode to include CPU sharing mode and GLDX compatibility
//					- Change SetSenderCPUmode name to SetSenderID
//		13.03.21	- Add m_bMemoryShare for possible older 2.006 apps
//					  Add memoryshare.CreateSenderMemory
//					  memoryshare.CloseSenderMemory() in destructor
//					  Add WriteMemoryPixels
//					  Change ReadMemoryPixels to accept GL_LUMINANCE
//					  Use reinterpret_cast for memoryshare.LockSenderMemory()
//		15.03.21	- Remove m_bNewFrame
//		20.03.21	- Add memoryshare.GetSenderMemoryName()
//		25.03.21	- Disable frame sync in destructor
//		02.04.21	- Change ReadMemory to ReadMemoryTexture
//		04.04.21	- Add GetSenderMemory
//		08.05.21	- Remove ReadMemoryBuffer open error log
//		10.05.21	- Remove memoryshare struct from header
//					  and replace with SpoutSharedMemory object.
//					  Close shared memory and sync event in SpoutGL class destructor
//		27.05.21	- Add GetMemoryBufferSize
//		09.06.21	- Add CreateMemoryBuffer, DeleteMemoryBuffer
//					  Revise and test data functions
//					  All data functions return false if 2.006 memoryshare mode.
//		26.07.21	- Remove memorysize check from GetMemoryBufferSize for receiver
//		10.08.21	- Correct LoadGLextensions to set no PBO availabliity if FBO fails
//					  WriteDX11texture - unmap staging texture if data read fails
//					  ReadTextureData - allow for no FBO support for low end graphics
//		29.09.21	- OpenSpout and LinkGLDXtextures - test for GL/DX extensions
//		15.10.21	- Remove interop object test for repeat from OpenSpout
//		09.11.21	- Revise UnloadTexturePixels
//		12.11.21	- Add OpenGL context check in destructor
//		13.11.21	- Remove code redundancy in destructor
//					  CleanupGL, CleanupInterop - add warning logs if no context
//					  CleanupDX11 - add warning log if no device
//		14.11.21	- Correct ReadTextureData for RGB source
//		16.11.21	- Remove GLerror from destructor
//		18.11.21	- InitTexture - restore current texture binding
//		19.11.21	- LoadGLextensions in constructor as well as OpenSpout
//		22.11.21	- OpenSpout new line for start changed from printf to SpoutLog
//		23.11.21	- Use SpoutDirectX ReleaseDX11Texture to release shared texture
//		11.12.21	- OpenSpout - return false for no OpenGL context or GL extensions
//				      Change CleanupInterop from void to bool
//				      CleanupDX11() - test CleanupInterop before releasing textures
//					  CleanupGL() - release interop objects before releasing shared texture
//					  OpenDirectX and OpenDirectX11 optional device argument
//		14.12.21	- Remove gl texture delete from GLDXready test.
//		15.12.21	- Change no context log warning to notice in CleanupGL and CleanupDX11
//					  LoadGLextensions - warn if pbo extensions not available or user disable
//					  CleanupGL() - release staging textures
//		16.12.21	- Add "No error: case comment to LinkGLDXtextures
//					  Remove un-necessary wglDXSetResourceShareHandle from LinkGLDXtextures
//		17.12.21	- Device argument only for OpenDirectx11
//					  Remove wglDXSetResourceShareHandleNV from LinkGLDXtextures
//					  Remove dxShareHandle argument from LinkGLDXtextures
//		18.12.21	- Restore default draw for all fbo functions
//					  Release interop objects after LinkGLDXtextures in GLDXready
//					  Create m_bGLDXdone flag for GLDXready to avoid repeats
//		27.12.21	- Restore default fbo in SetSharedTextureData if texture ID is zero
//		23.01.22	- Change pointer comparision from >0 to nullptr in OpenSpout (PR #80)
//		25.01.22	- Remove m_hInteropDevice created check in OpenSpout
//					  Clean up logs in LoadGLextensions
//		21.02.22	- Restore glBufferData method for in UnloadTexturePixels
//					  Pending implementation of glFencSync for glMapBufferRange method
//		16.03.22	- Use m_hInteropObject in LinkGLDXtextures so that CleanupInterp releases the imterop object
//					- Allow for success test in GLDXReady();
// ====================================================================================
/*
	Copyright (c) 2021-2022, Lynn Jarvis. All rights reserved.

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

#include "SpoutGL.h"

// Class: spoutGL
// Base class for OpenGL texture sharing using the NVIDIA GL/DX interop extensions.
// This class should not be used directly because it is the base for the Spout, SpoutSender and SpoutReceiver classes.
// Refer to the Spout class for further documentation and details.
spoutGL::spoutGL()
{
	m_SenderName[0] = 0;
	m_SenderNameSetup[0] = 0;
	m_Width = 0;
	m_Height = 0;
	m_dwFormat = (DWORD)DXGI_FORMAT_B8G8R8A8_UNORM; // default sender format

	m_bAuto = true;
	m_bCPU = false;
	m_bUseGLDX = true;
	m_bTextureShare = true;
	m_bCPUshare = false; // Texture share assumed by default
	m_bSenderCPU = false;
	m_bSenderGLDX = true;
	
	m_bConnected = false;
	m_bInitialized = false;
	m_bSpoutPanelOpened = false;
	m_bSpoutPanelActive = false;
	m_bUpdated = false;
	m_bMirror = false;
	m_bSwapRB = false;
	m_bGLDXdone = false; // Compatibility test not done yet

	m_glTexture = 0;
	m_TexID = 0;
	m_TexWidth = 0;
	m_TexHeight = 0;
	m_TexFormat = GL_RGBA;
	m_fbo = 0;

	m_dxShareHandle = nullptr; // Shared texture handle
	m_pSharedTexture = nullptr; // DX11 shared texture
	m_DX11format = DXGI_FORMAT_B8G8R8A8_UNORM; // Default compatible with DX9
	m_pStaging[0] = nullptr; // DX11 staging textures
	m_pStaging[1] = nullptr;
	m_Index = 0;
	m_NextIndex = 0;

	m_hInteropDevice = nullptr;
	m_hInteropObject = nullptr;
	m_hWnd = nullptr;

	// For CreateOpenGL and CloseOpenGL
	m_hdc = nullptr;
	m_hwndButton = nullptr;
	m_hRc = nullptr;

	// OpenGL extensions
	m_caps = 0; // nothing loaded yet
	m_bExtensionsLoaded = false;
	m_bBGRAavailable = false;
	m_bFBOavailable  = false;
	m_bBLITavailable = false;
	m_bSWAPavailable = false;
	m_bGLDXavailable = false;
	m_bCOPYavailable = false;
	m_bPBOavailable = true; // Assume true until tested by LoadGLextensions
	m_bCONTEXTavailable = false;

	// PBO support
	PboIndex = 0;
	NextPboIndex = 0;
	m_pbo[0] = m_pbo[1] = m_pbo[2] = m_pbo[3] = 0;
	m_nBuffers = 2; // default number of buffers used
	
	// Check the user selected Auto share mode
	DWORD dwValue = 0;
	if (ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "Auto", &dwValue))
		m_bAuto = (dwValue == 1);

	// Check for 2.006 registry CPU mode
	if (ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "CPU", &dwValue))
		m_bCPU = (dwValue == 1);

	// Check the user selected buffering mode
	if (ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "Buffering", &dwValue))
		m_bPBOavailable = (dwValue == 1);

	// Number of PBO buffers user selected
	m_nBuffers = 2;
	if(ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "Buffers", &dwValue))
		m_nBuffers = (int)dwValue;

	// Find version number from the registry if Spout is installed (2005, 2006, etc.)
	if (ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "Version", &dwValue))
		m_SpoutVersion = (int)dwValue; // 0 for earlier than 2.005
	else
		m_SpoutVersion = -1; // Spout not installed

	// 2.006 memoryshare mode
	// Only set if 2.006 SpoutSettings has been used
	// Removed by 2.007 SpoutSettings
	m_bMemoryShare = GetMemoryShareMode();

	// Extensions are loaded in OpenSpout() if a context is not available here
	LoadGLextensions();

}

//---------------------------------------------------------
// Function: ~spoutGL()
// Destructor
// Note that an OpenGL context is necessary for release of
// OpenGL objects in CleanupGL and CleanupInterop.
// Similarly, a DirectX device is necessary in CleanupDX11.
// If the context or device is lost, release of memory allocated
// to these objects is handled by the driver.
//
spoutGL::~spoutGL()
{
	if (m_bInitialized) {
		sendernames.ReleaseSenderName(m_SenderName);
		frame.CleanupFrameCount();
		frame.CloseAccessMutex();
	}

	// Close 2.006 or buffer shared memory if used
	memoryshare.Close();

	// Release event if used
	frame.CloseFrameSync();

	// Close shared memory and sync event if used
	memoryshare.Close();
	frame.CloseFrameSync();

	// Release OpenGL resources and interop
	// Releases the DirectX shared texture and Staging textures for CPU share
	CleanupGL();

	// Finally release DirectX resources and device
	CleanupDX11();

}

//
// Group: OpenGL shared texture
//

//---------------------------------------------------------
// Function: BindSharedTexture
// Bind OpenGL shared texture.
bool spoutGL::BindSharedTexture()
{
	// Only for GL/DX interop mode
	if (!m_hInteropDevice || !m_hInteropObject)
		return false;

	bool bRet = false;
	// Wait for access to the shared texture
	if (frame.CheckTextureAccess(m_pSharedTexture)) {
		// lock dx object
		if (LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {
			// Bind our shared OpenGL texture
			glBindTexture(GL_TEXTURE_2D, m_glTexture);
			// Leave interop and mutex both locked for success
			bRet = true;
		}
		else {
			// Release interop lock and allow texture access for fail
			UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
			bRet = false;
		}
		// Release mutex and allow access to the texture
		frame.AllowTextureAccess(m_pSharedTexture);
	}

	return bRet;

} // end BindSharedTexture

//---------------------------------------------------------
// Function: UnBindSharedTexture
// Un-bind OpenGL shared texture.
bool spoutGL::UnBindSharedTexture()
{
	// Only for GL/DX interop mode
	if (!m_hInteropDevice || !m_hInteropObject)
		return false;

	// Unbind our shared OpenGL texture
	glBindTexture(GL_TEXTURE_2D, 0);
	// unlock dx object
	UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
	// Release mutex and allow access to the texture
	frame.AllowTextureAccess(m_pSharedTexture);

	return true;

} // end UnBindSharedTexture

//---------------------------------------------------------
// Function: GetSharedTextureID
// OpenGL shared texture ID.
GLuint spoutGL::GetSharedTextureID()
{
	return m_glTexture;
}

//
// Group: Graphics compatibility
//

//---------------------------------------------------------
// Function: GetAutoShare
// Get auto GPU/CPU share depending on compatibility.
bool spoutGL::GetAutoShare()
{
	return m_bAuto;
}

//---------------------------------------------------------
// Function: SetAutoShare
// Set auto GPU/CPU share depending on compatibility.
void spoutGL::SetAutoShare(bool bAuto)
{
	m_bAuto = bAuto;
}

//---------------------------------------------------------
// Function: GetCPUShare
// Get CPU share for the application.
// This is determined by compatibility or set by the application
bool spoutGL::GetCPUshare()
{
	return m_bCPUshare;
}

//---------------------------------------------------------
// Function: SetCPUshare
// Set CPU share for the application.
// If set false, GL/DX compatibility is re-tested.
// CPU share can also be set globally with "SetCPUmode".
void spoutGL::SetCPUshare(bool bCPU)
{
	m_bCPUshare = bCPU;

	if (m_bCPUshare) {
		m_bTextureShare = false;
	}
	else {
		// Re-test GL/DX compatibility
		SpoutLogNotice("spoutGL::SetCPUshare(%d) - re-testing GL/DX compatibility", bCPU);
		OpenSpout(true);
		if (m_bCPUshare) {
			SpoutLogWarning("    Cannot disable CPU sharing mode. System is not GL/DX compatible");
		}
	}

}

//---------------------------------------------------------
// Function: IsGLDXready
// OpenGL texture share compatibility.
bool spoutGL::IsGLDXready()
{
	return m_bUseGLDX;
}

//
// Group: For direct access if necessary
//

//---------------------------------------------------------
// Function: OpenSpout
//
// Initialize OpenGL and DX11.
//
//     - Open DirectX and check for availability
//     - Load extensions and check for availability and function
//     - Compatibility test for use or GL/DX interop
//     - Optionally re-test compatibility even if already initialized
//
// Failure means that DirectX is not available.
//
// OpenGL GPU texture sharing is used only if GL/DX compatible (m_bUseGLDX = true).
//
// DirectX CPU backup is used if :
//
//     - Graphics is incompatible (m_bUseGLDX = false)
//
//   and
//
//     - The user has selected "Auto" share in SpoutSettings (m_bAuto = false)
//
//   or
//
//     - The user has selected CPU mode (2.006 setting or by registry edit of the CPU entry)
//
// Class flags :
//
//   m_bUseGLDX -       graphics GL/DX compatibility, not the sharing method to be used
//   m_bTextureShare -  use OpenGL GL/DX methods
//   m_bCPUshare -      use DirectX CPU methods
//   Neither method -   do not process at all
//
	
bool spoutGL::OpenSpout(bool bRetest)
{
	// Return if already initialized and not re-testing compatibility
	// Look for DirectX device to prevent repeat
	// m_hInteropDevice is created in CreateInterop 
	if (spoutdx.GetDX11Device() != nullptr && !bRetest)
		return true;

	 // This is the start, so make a new line in the log
	 SpoutLog("");
#ifdef _M_X64
	SpoutLogNotice("spoutGL::OpenSpout - 64bit 2.007 - this 0x%.7X", PtrToUint(this));
#else
	SpoutLogNotice("spoutGL::OpenSpout - 32bit 2.007 - this 0x%.7X", PtrToUint(this));
#endif

	m_bUseGLDX = false;
	m_bTextureShare = false;
	m_bCPUshare = false;

	// DirectX capability is the minimum
	if (!OpenDirectX()) {
		SpoutLogFatal("spoutGL::OpenSpout - Could not initialize DirectX 11");
		return false;
	}

	// DirectX is OK
	// OpenGL device context is needed to go on
	HDC hdc = wglGetCurrentDC();
	if (hdc) {
		// Get a window handle
		m_hWnd = WindowFromDC(hdc);
		// Load extensions
		if (LoadGLextensions()) {
			// If DirectX and OpenGL are both OK - test GLDX compatibility
			// For a re-test, create a new interop device in GLDXReady()
			if (bRetest) {
				CleanupInterop();
			}
			SpoutLogNotice("spoutGL::OpenSpout - GL extensions loaded sucessfully");
			// Drop through for detail notices
		}
		else {
			SpoutLogFatal("spoutGL::OpenSpout - Could not load GL extensions");
			return false;
		}
	}
	else {
		SpoutLogFatal("spoutGL::OpenSpout - Cannot get GL device context");
		// This is OpenGL, but DirectX shared textures might still work OK (see SpoutDX)
		return false;
	}
	
	//
	// OpenGL GPU texture sharing is used if GL/DX compatible (m_bUseGLDX = true)
	//
	// DirectX CPU backup is used if :
	//     Graphics is incompatible (m_bUseGLDX = false)
	//   and
	//     The user has selected "Auto" share in SpoutSettings (m_bAuto = false)
	//   or
	//     The user has selected CPU mode (2.006 setting or by registry edit of the CPU entry)
	//
	//   m_bUseGLDX      - shows graphics GL/DX compatibility, not the sharing method to be used
	//   m_bTextureShare - use OpenGL GL/DX methods
	//   m_bCPUshare     - use DirectX CPU methods
	//   Neither method  - do not process at all
	//

	if (GLDXready()) {
		// GL/DX compatible -  m_bUseGLDX is set true
		SpoutLogNotice("    GL/DX interop compatible");
	}
	else {
		// Not GL/DX compatible -  m_bUseGLDX is set false
		SpoutLogWarning("spoutGL::OpenSpout - system is not compatible with GL/DX interop");
	}

	// Work out sharing methods
	m_bTextureShare = false; // use GL/DX methods
	m_bCPUshare = false; // Use DirectX CPU methods

	// Texture share requires GL/DX compatibility
	m_bTextureShare = m_bUseGLDX;

	// If not compatible, Use CPU share only if Auto has been set in SpoutSettings
	if (!m_bTextureShare && m_bAuto)
		m_bCPUshare = true;

	// Or CPU share over-ride for 2.006 setting or direct registry edit
	if (m_bCPU) {
		m_bTextureShare = false; // Do not use texture share
		m_bCPUshare = true; // Use CPU share
	}

	// Show the sharing method to be used
	if (m_bTextureShare) {
		SpoutLogNotice("    Using GPU OpenGL GL/DX methods");
	}
	else if (m_bCPUshare) {
		SpoutLogWarning("   Using CPU DirectX methods");
	}
	else {
		SpoutLogWarning("   Cannot share textures");
	}
	
	return true;

}

//---------------------------------------------------------
// Function: OpenDirectX
// Initialize DirectX (D3D11 only)
bool spoutGL::OpenDirectX()
{
	SpoutLogNotice("spoutGL::OpenDirectX");
	return spoutdx.OpenDirectX11();
}

//---------------------------------------------------------
// Function: SetDX11format
//   Set sender DX11 shared texture format
//
//   Texture formats compatible with WGL_NV_DX_interop
//   https://www.khronos.org/registry/OpenGL/extensions/NV/WGL_NV_DX_interop.txt
//   https://www.khronos.org/registry/OpenGL/extensions/NV/WGL_NV_DX_interop2.txt
//   D3DFMT_A8R8G8B8                         = 21
//   D3DFMT_X8R8G8B8                         = 22
//   DXGI_FORMAT_R32G32B32A32_FLOAT          = 2
//   DXGI_FORMAT_R16G16B16A16_FLOAT          = 10
//   DXGI_FORMAT_R16G16B16A16_UNORM          = 11
//   DXGI_FORMAT_R16G16B16A16_SNORM          = 13
//   DXGI_FORMAT_R10G10B10A2_UNORM           = 24
//   DXGI_FORMAT_R8G8B8A8_UNORM              = 28
//   DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         = 29
//   DXGI_FORMAT_R8G8B8A8_SNORM              = 31
//   DXGI_FORMAT_B8G8R8A8_UNORM              = 87 (default)
//   DXGI_FORMAT_B8G8R8X8_UNORM              = 88
//
void spoutGL::SetDX11format(DXGI_FORMAT textureformat)
{
	m_DX11format = textureformat;
}

//---------------------------------------------------------
// Function: CloseDirectX
// Close DirectX and free resources
void spoutGL::CloseDirectX()
{
	SpoutLogNotice("spoutGL::CloseDirectX()");

	if (m_pSharedTexture)
		spoutdx.ReleaseDX11Texture(GetDX11Device(), m_pSharedTexture);
	m_pSharedTexture = nullptr;
	spoutdx.CloseDirectX11();

	// Re-set shared texture handle
	m_dxShareHandle = nullptr;

}

//---------------------------------------------------------
// Function: CreateOpenGL
// Create an OpenGL window and context for situations where there is none.
//     Not necessary if an OpenGL context is already available.
//     Always call CloseOpenGL() on application close.
//
// OpenGL support is required.
// Include in your application header file :
//     #include <gl/GL.h>
//     #pragma comment (lib, "opengl32.lib")
//
bool spoutGL::CreateOpenGL()
{
	SpoutLogNotice("spoutGL::CreateOpenGL()");
	
	HGLRC glContext = wglGetCurrentContext();

	if (!glContext) {
		m_hdc = nullptr;
		m_hwndButton = nullptr;
		m_hRc = nullptr;

		// We only need an OpenGL context with no render window because we don't draw to it
		// so create an invisible dummy button window. This is then independent from the host
		// program window (GetForegroundWindow). If SetPixelFormat has been called on the
		// host window it cannot be called again. This caused a problem in Mapio.
		// https://msdn.microsoft.com/en-us/library/windows/desktop/dd369049%28v=vs.85%29.aspx
		//
		// CS_OWNDC allocates a unique device context for each window in the class. 
		//
		if (!m_hwndButton || !IsWindow(m_hwndButton)) {
			m_hwndButton = CreateWindowA("BUTTON",
				"SpoutOpenGL",
				WS_OVERLAPPEDWINDOW | CS_OWNDC,
				0, 0, 32, 32,
				NULL, NULL, NULL, NULL);
		}

		if (!m_hwndButton) {
			SpoutLogError("spoutGL::CreateOpenGL - no hwnd");
			return false;
		}

		m_hdc = GetDC(m_hwndButton);
		if (!m_hdc) {
			SpoutLogError("spoutGL::CreateOpenGL - no hdc");
			CloseOpenGL();
			return false;
		}

		PIXELFORMATDESCRIPTOR pfd;
		ZeroMemory(&pfd, sizeof(pfd));
		pfd.nSize = sizeof(pfd);
		pfd.nVersion = 1;
		pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
		pfd.iPixelType = PFD_TYPE_RGBA;
		pfd.cColorBits = 32;
		pfd.cDepthBits = 16;
		pfd.iLayerType = PFD_MAIN_PLANE;
		int iFormat = ChoosePixelFormat(m_hdc, &pfd);
		if (!iFormat) {
			SpoutLogError("spoutGL::CreateOpenGL - pixel format error");
			CloseOpenGL();
			return false;
		}

		if (!SetPixelFormat(m_hdc, iFormat, &pfd)) {
			DWORD dwError = GetLastError();
			// 2000 (0x7D0) The pixel format is invalid.
			// Caused by repeated call of the SetPixelFormat function
			char temp[128];
			sprintf_s(temp, "spoutGL::CreateOpenGL - SetPixelFormat Error %lu (0x%4.4lX)", dwError, dwError);
			SpoutLogError("%s", temp);
			CloseOpenGL();
			return false;
		}

		m_hRc = wglCreateContext(m_hdc);
		if (!m_hRc) {
			SpoutLogError("spoutGL::CreateOpenGL - could not create OpenGL context");
			CloseOpenGL();
			return false;
		}

		wglMakeCurrent(m_hdc, m_hRc);
		if (!wglGetCurrentContext()) {
			SpoutLogError("spoutGL::CreateOpenGL - no OpenGL context");
			CloseOpenGL();
			return false;
		}
		SpoutLogNotice("    OpenGL window created OK");
	}
	else {
		SpoutLogNotice("    OpenGL context exists");
	}

	return true;
}

//---------------------------------------------------------
// Function: CloseOpenGL
// Close OpenGL window and release resources
bool spoutGL::CloseOpenGL()
{

	SpoutLogNotice("spoutGL::CloseOpenGL() - m_hRc = 0x%.7X : m_hdc = 0x%.7X", PtrToUint(m_hRc), PtrToUint(m_hdc) );

	// Properly kill the OpenGL window
	if (m_hRc) {
		if (!wglMakeCurrent(NULL, NULL)) { // Are We Able To Release The DC And RC Contexts?
			SpoutLogError("spoutGL::CloseOpenGL - release of DC and RC failed");
			return false;
		}
		if (!wglDeleteContext(m_hRc)) { // Are We Able To Delete The RC?
			SpoutLogError("spoutGL::CloseOpenGL - release rendering context failed");
			return false;
		}
		m_hRc = NULL;
	}

	if (m_hdc && !ReleaseDC(m_hwndButton, m_hdc)) { // Are We Able To Release The DC
		SpoutLogError("spoutGL::CloseOpenGL - release device context Failed");
		m_hdc = NULL;
		return false;
	}

	if (m_hwndButton && !DestroyWindow(m_hwndButton)) { // Are We Able To Destroy The Window?
		SpoutLogError("spoutGL::CloseOpenGL - could not release hWnd");
		m_hwndButton = NULL;
		return false;
	}

	SpoutLogNotice("    Closed OpenGL window OK");

	return true;
}

//---------------------------------------------------------
// Class initialization status
bool spoutGL::IsSpoutInitialized()
{
	return m_bInitialized;
}

//
// GLDXready
//
// Hardware compatibility test
//
//  o Check that extensions for GL/DX interop are available
//
//  o GLDXready
//      Checks operation of GL/DX interop functions
//		and creates an interop device for success
//
//	o m_bUseGLDX - true for GL/DX interop availability
//
bool spoutGL::GLDXready()
{
	// === Simulate failure for debugging ===
	// SpoutLogNotice("spoutGL::GLDXready - simulated compatibility failure");
	// m_bUseGLDX = false;
	// m_bTextureShare = false;
	// m_bCPUshare = true;
	// return false;

	// === Simulate success to skip test for debugging ===
	// SpoutLogNotice("spoutGL::GLDXready - simulated compatibility success");
	// m_bUseGLDX = true;
	// m_bTextureShare = true;
	// m_bCPUshare = false;
	// return true;

	
	// Return if the test has already been done
	if (m_bGLDXdone) {
		SpoutLogNotice("spoutGL::GLDXready - test previously completed");
		return m_bUseGLDX;
	}

	//
	// Test whether the NVIDIA OpenGL/DirectX interop extensions function correctly.
	//
	// Create dummy textures and use the interop functions.
	// Must be called after OpenDirectX.
	// Success means the GLDX interop functions can be used.
	// Other errors should not happen if OpenDirectX succeeded
	//
	ID3D11Texture2D* pTexture = nullptr; // the DX11 texture for the test link
	HANDLE dxShareHandle = nullptr; // Shared texture handle
	// HANDLE hInteropObject = nullptr; // handle to the DX/GL interop object
	GLuint glTexture = 0; // the OpenGL texture linked to the shared DX texture

	SpoutLogNotice("spoutGL::GLDXready - testing for GL/DX interop compatibility");

	// Assume not GLDX interop compatible until all tests pass
	m_bUseGLDX = false;

	if (!spoutdx.GetDX11Device()) {
		SpoutLogError("spoutGL::GLDXready - No D3D11 device");
		return false;
	}

	// DirectX is OK but check for availabilty of the GL/DX extensions.
	if (!m_bGLDXavailable) {
		// The extensions required for texture access are not available.
		SpoutLogError("spoutGL::GLDXready - GL/DX interop extensions not available");
		return false;
	}

	SpoutLogNotice("    GL/DX interop extensions available");

	// Create an opengl texture for the test
	glGenTextures(1, &glTexture);
	if (glTexture == 0) {
		SpoutLogError("spoutGL::GLDXready - glGenTextures failed");
		return false;
	}

	// Create a shared texture for the link test
	if (!spoutdx.CreateSharedDX11Texture(spoutdx.GetDX11Device(),
		256, 256, DXGI_FORMAT_B8G8R8A8_UNORM, // default
		&pTexture, dxShareHandle)) {
		glDeleteTextures(1, &glTexture);
		SpoutLogError("spoutGL::GLDXready - CreateSharedDX11Texture failed");
		return false;
	}

	SpoutLogNotice("    Linking test - OpenGL texture (0x%.7X) DX11 texture (0x%.7X)", glTexture, PtrToUint(pTexture));

	// Link the shared DirectX texture to the OpenGL texture
	// USe the global m_hInteropObject so that CleanupInterop works
	m_hInteropObject = LinkGLDXtextures(spoutdx.GetDX11Device(), pTexture, glTexture);
	if (!m_hInteropObject) {
		spoutdx.ReleaseDX11Texture(spoutdx.GetDX11Device(), pTexture);
		glDeleteTextures(1, &glTexture);
		glTexture = 0;
		pTexture = nullptr;
		dxShareHandle = nullptr;
		// It is possible that extensions for the GL/DX interop load OK
		// but that the GL/DX interop functions fail.
		// This has been noted on dual graphics machines with the NVIDIA Optimus driver.
		SpoutLogWarning("spoutGL::GLDXready - GL/DX interop functions failed");
	}
	else {

		// Release the interop objects created for the test
		// They are re-created in CreateInterop
		CleanupInterop();

		// Release the test textures after the interop objects have been released
		spoutdx.ReleaseDX11Texture(spoutdx.GetDX11Device(), pTexture);
		glDeleteTextures(1, &glTexture);
		glTexture = 0;
		pTexture = nullptr;
		dxShareHandle = nullptr;

		// Set compatibility flag
		m_bUseGLDX = true;

		SpoutLogNotice("    Test OpenGL and DX11 textures created and linked OK");

	}

	// Now GLDXready() has set m_bUseGLDX is set to use the GL/DX interop or not.
	
	// Use of texture sharing or CPU backup is assessed from
	// user settings (retrieved with GetAutoShare)
	// and actual GL/DX compatibility (retrieved with GetGLDXready)
	//
	// The result is tested in OpenSpout
	//
	// Texture sharing is used if GL/DX compatible (m_bUseGLDX = true)
	//
	// CPU backup is used if :
	//     Graphics is incompatible (m_bUseGLDX = false)
	//   and
	//     The user has selected "Auto" share in SpoutSettings (m_bAuto = false)
	//   or
	//     The user has forced CPU mode by registry edit of the CPU entry 	
	//
	// Otherwise no sharing is performed.

	// If not GLDX compatible, LinkGLDXtexture will not be called (see CreateDX11interop)
	// ReadDX11Texture and WriteDX11Texture will be used instead via CPU staging textures 

	// Set a class flag so the test is not repeated
	m_bGLDXdone = true;

	return m_bUseGLDX;

}


//---------------------------------------------------------
bool spoutGL::SetHostPath(const char *sendername)
{
	SharedTextureInfo info;
	if (!sendernames.getSharedInfo(sendername, &info)) {
		SpoutLogWarning("spoutGL::SetHostPath(%s) - could not get sender info", sendername);
		return false;
	}
	char exepath[256];
	GetModuleFileNameA(NULL, exepath, sizeof(exepath));
	// Description is defined as wide chars, but the path is stored as byte chars
	strcpy_s((char*)info.description, 256, exepath);
	if (!sendernames.setSharedInfo(sendername, &info)) {
		SpoutLogWarning("spoutGL::SetHostPath(%s) - could not set sender info", sendername);
	}
	return true;

}


//----------------------------------------------------------
// SetSenderID - set top two bits of 32 bit partner ID field
//
//   bCPU  - means "using CPU sharing methods"
//     1000 0000 0000 0000 0000 0000 0000 0000 = 0x80000000
//   bGLDX - means "hardware is compatible with OpenGL/DirectX interop"
//     0100 0000 0000 0000 0000 0000 0000 0000 = 0x40000000
//   Both set - means "hardware is GL/DX compatible but using CPU sharing methods"
//     1100 0000 0000 0000 0000 0000 0000 0000 = 0xC0000000
// 
// 2.006 senders may or may not have these bits set, but will rarely have the exact values.
//
bool spoutGL::SetSenderID(const char *sendername, bool bCPU, bool bGLDX)
{
	// Texture share and hardware compatibility assumed by default
	m_bSenderCPU = false;
	m_bSenderGLDX = true;

	// Set the requested flags to the PartnerID field of sender shared memory.
	// If the method succeeds, set class flags.
	if (sendernames.SetSenderID(sendername, bCPU, bGLDX)) {
			m_bSenderCPU  = bCPU;
			m_bSenderGLDX = bGLDX;
			return true;
	}
	return false;
}


//
// Protected functions
//

// Create shared DirectX texture and OpenGL texture and link with GL/DX interop
bool spoutGL::CreateInterop(unsigned int width, unsigned int height, DWORD dwFormat, bool bReceive)
{
	SpoutLogNotice("spoutGL::CreateInterop");

	// Create or use a shared DirectX texture that will be linked
	// to the OpenGL texture and get it's share handle for sharing textures
	if (bReceive) {
		// A receiver uses a texture already created from the sender share handle
		if (!m_pSharedTexture || !m_dxShareHandle) {
			SpoutLogError("spoutGL::CreateInterop - no receiver texture : device = 0x%.7X, sharehandle = 0x%.7X", PtrToUint(spoutdx.GetDX11Device()), LOWORD(m_dxShareHandle));
			return false;
		}
	}
	else {

		// A sender creates a new texture with a new share handle
		m_dxShareHandle = nullptr;

		// Compatible formats - see SetDX11format
		// A directX 11 receiver accepts DX9 formats
		DWORD format = (DWORD)DXGI_FORMAT_B8G8R8A8_UNORM; // (87) default compatible with DX9
		if (dwFormat > 0) {
			format = dwFormat;
			SetDX11format((DXGI_FORMAT)format); // Set the global texture format
		}
		else {
			format = m_DX11format;
		}

		// Create or re-create the linked DX11 texture
		if (!spoutdx.CreateSharedDX11Texture(spoutdx.GetDX11Device(),
			width, height, (DXGI_FORMAT)format, // default is DXGI_FORMAT_B8G8R8A8_UNORM
			&m_pSharedTexture, m_dxShareHandle)) {
			SpoutLogError("spoutGL::CreateInterop - CreateSharedDX11Texture failed");
			return false;
		}
	}

	//
	// Link the shared DirectX texture to the OpenGL texture
	// This registers for interop and associates the opengl texture with the dx texture
	// by calling wglDXRegisterObjectNV which returns a handle to the interop object
	// (the shared texture) (m_hInteropObject)
	//

	// When a sender size changes, the new texture has to be re-registered
	if (m_hInteropDevice && m_hInteropObject) {
		SpoutLogNotice("    Re-registering interop");
		wglDXUnregisterObjectNV(m_hInteropDevice, m_hInteropObject);
		m_hInteropObject = nullptr;
	}

	// Create or re-create the class OpenGL texture
	// The texture has body after it is linked to the shared DirectX texture
	glGenTextures(1, &m_glTexture);

	m_Width = width;
	m_Height = height;

	// Link the texture using the GL/DX interop
	m_hInteropObject = LinkGLDXtextures((void *)spoutdx.GetDX11Device(), m_pSharedTexture, m_glTexture);
	if (!m_hInteropObject) {
		SpoutLogError("spoutGL::CreateInterop - LinkGLDXtextures failed");
		return false;
	}

	SpoutLogNotice("spoutGL::CreateInterop - m_pSharedTexture [0x%.7X] m_dxShareHandle [0x%.7X]", PtrToUint(m_pSharedTexture), LOWORD(m_dxShareHandle));
	SpoutLogNotice("    m_hInteropObject = 0x%.7X", LOWORD(m_hInteropObject));

	// Create an fbo if not already
	if (m_fbo == 0)
		glGenFramebuffersEXT(1, &m_fbo);

	// Important to reset PBO index
	PboIndex = 0;
	NextPboIndex = 0;

	// Also reset staging texture index
	m_Index = 0;
	m_NextIndex = 0;

	return true;

}

//
//	Link a shared DirectX texture to an OpenGL texture
//	and create a GLDX interop object handle
//
//	IN	pSharedTexture  Pointer to shared the DirectX texture
//	IN	dxShareHandle   Handle of the DirectX texture to be shared
//	IN	glTextureID     ID of the OpenGL texture that is to be linked to the shared DirectX texture
//	Returns             Handle to the GL/DirectX interop object (the shared texture)
//
HANDLE spoutGL::LinkGLDXtextures(void* pDXdevice, void* pSharedTexture,  GLuint glTexture)
{
	HANDLE hInteropObject = nullptr;
	DWORD dwError = 0;
	char tmp[128];

	// Are the GL/DX interop extensions loaded ?
	if (!wglDXOpenDeviceNV
		|| !wglDXSetResourceShareHandleNV
		|| !wglDXRegisterObjectNV
		|| !wglDXCloseDeviceNV) {
		SpoutLogError("spoutGL::LinkGLDXtextures - no GL/DX extensions");
		return nullptr;
	}

	// Prepare the DirectX device for interoperability with OpenGL
	// The return value is a handle to a GL/DirectX interop device.
	if (!m_hInteropDevice) {
		try {
			m_hInteropDevice = wglDXOpenDeviceNV(pDXdevice);
		}
		catch (...) {
			SpoutLogError("spoutGL::LinkGLDXtextures - wglDXOpenDeviceNV exception");
			return NULL;
		}
	}

	// Report the error if wglDXOpenDeviceNV failed
	if (!m_hInteropDevice) {
		dwError = GetLastError();
		sprintf_s(tmp, 128, "spoutGL::LinkGLDXtextures : wglDXOpenDeviceNV(0x%.7X) - error %lu (0x%.X)\n",
			LOWORD(pDXdevice), dwError, LOWORD(dwError));
		// Other errors reported
		// 1008, 0x3F0 - ERROR_NO_TOKEN
		switch (LOWORD(dwError)) {
		case 0:
			strcat_s(tmp, 128, "    No error");
			break;
		case ERROR_OPEN_FAILED:
			strcat_s(tmp, 128, "    Could not open the Direct3D device.");
			break;
		case ERROR_NOT_SUPPORTED:
			// This can be caused either by passing in a device from an unsupported DirectX
			// version, or by passing in a device referencing a display adapter that is
			// not accessible to the GL.
			strcat_s(tmp, 128, "    The dxDevice is not supported.");
			break;
		default:
			strcat_s(tmp, 128, "    Unknown error.");
			break;
		}
		SpoutLogError("%s", tmp);

		return NULL;
	}

	// wglDXSetResourceShareHandle does not need to be called for DirectX
	// version 10 and 11 resources.

	// Prepare the DirectX texture for use by OpenGL
	// register for interop and associate the opengl texture with the dx texture
	// Returns a handle that can be used for sharing functions
	try {
		hInteropObject = wglDXRegisterObjectNV(m_hInteropDevice,
			pSharedTexture,	// DX texture
			glTexture,		// OpenGL texture
			GL_TEXTURE_2D,	// Must be TEXTURE_2D - multisampling not supported
			WGL_ACCESS_READ_WRITE_NV); // We will write and the receiver will read
	}
	catch (...) {
		SpoutLogError("spoutGL::LinkGLDXtextures - wglDXRegisterObjectNV exception");
		return NULL;
	}

	if (!hInteropObject) {
		// Noted C007006E returned on failure.
		// Error codes are 32-bit values, but expected results are in the low word.
		// 006E is ERROR_OPEN_FAILED (110L)
		dwError = GetLastError();
		sprintf_s(tmp, 128, "spoutGL::LinkGLDXtextures - wglDXRegisterObjectNV :error %u, (0x%.X)\n",
			LOWORD(dwError), LOWORD(dwError));
		switch (LOWORD(dwError)) {
		case 0:
			strcat_s(tmp, 128, "    No error");
		break;
		case ERROR_INVALID_HANDLE:
			strcat_s(tmp, 128, "    No GL context is current.");
			break;
		case ERROR_INVALID_DATA:
			strcat_s(tmp, 128, "    Incorrect GL name, type or access parameters.");
			break;
		case ERROR_OPEN_FAILED:
			strcat_s(tmp, 128, "    Failed to open the Direct3D resource.");
			break;
		default:
			strcat_s(tmp, 128, "    Unknown error.");
			break;
		}
		SpoutLogError("%s", tmp);

		// Error so close interop device
		if (m_hInteropDevice) {
			wglDXCloseDeviceNV(m_hInteropDevice);
			m_hInteropDevice = nullptr;
		}

	}

	return hInteropObject;

}

HANDLE spoutGL::GetInteropDevice()
{
	return m_hInteropDevice; // Handle to the GL/DX interop device
}

//
//	GL/DX Interop lock
//
//	A return value of S_OK indicates that all objects were
//    successfully locked.  Other return values indicate an
//    error. If the function returns false, none of the objects will be locked.
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
//	DISCUSSION: The Lock/Unlock calls serve as synchronization points
//    between OpenGL and DirectX. They ensure that any rendering
//    operations that affect the resource on one driver are complete
//    before the other driver takes ownership of it.
//
//	This function assumes only one object
//
//	Must return S_OK (0) - otherwise the error can be checked.
//
HRESULT spoutGL::LockInteropObject(HANDLE hDevice, HANDLE *hObject)
{
	DWORD dwError;
	HRESULT hr;

	if (!hDevice || !hObject || !*hObject) {
		return E_HANDLE;
	}

	// lock dx object
	if (wglDXLockObjectsNV(hDevice, 1, hObject)) {
		return S_OK;
	}
	else {
		dwError = GetLastError();
		switch (LOWORD(dwError)) {
		case ERROR_BUSY:			// One or more of the objects in <hObjects> was already locked.
			hr = E_ACCESSDENIED;	// General access denied error
			SpoutLogError("spoutGL::LockInteropObject - ERROR_BUSY");
			break;
		case ERROR_INVALID_DATA:	// One or more of the objects in <hObjects>
									// does not belong to the interop device
									// specified by <hDevice>.
			hr = E_ABORT;			// Operation aborted
			SpoutLogError("spoutGL::LockInteropObject - ERROR_INVALID_DATA");
			break;
		case ERROR_LOCK_FAILED:	// One or more of the objects in <hObjects> failed to 
			hr = E_ABORT;			// Operation aborted
			SpoutLogError("spoutGL::LockInteropObject - ERROR_LOCK_FAILED");
			break;
		default:
			hr = E_FAIL;			// unspecified error
			SpoutLogError("spoutGL::LockInteropObject - UNKNOWN_ERROR");
			break;
		} // end switch
	} // end false

	return hr;

} // LockInteropObject


//
// Must return S_OK (0) - otherwise the error can be checked.
//
HRESULT spoutGL::UnlockInteropObject(HANDLE hDevice, HANDLE *hObject)
{
	DWORD dwError;
	HRESULT hr;

	if (!hDevice || !hObject || !*hObject) {
		return E_HANDLE;
	}

	if (wglDXUnlockObjectsNV(hDevice, 1, hObject)) {
		return S_OK;
	}
	else {
		dwError = GetLastError();
		switch (LOWORD(dwError)) {
		case ERROR_NOT_LOCKED:
			hr = E_ACCESSDENIED;
			SpoutLogError("spoutGL::UnLockInteropObject - ERROR_NOT_LOCKED");
			break;
		case ERROR_INVALID_DATA:
			SpoutLogError("spoutGL::UnLockInteropObject - ERROR_INVALID_DATA");
			hr = E_ABORT;
			break;
		case ERROR_LOCK_FAILED:
			hr = E_ABORT;
			SpoutLogError("spoutGL::UnLockInteropObject - ERROR_LOCK_FAILED");
			break;
		default:
			hr = E_FAIL;
			SpoutLogError("spoutGL::UnLockInteropObject - UNKNOWN_ERROR");
			break;
		} // end switch
	} // end fail

	return hr;

} // end UnlockInteropObject


// Clean up the gldx interop
bool spoutGL::CleanupInterop()
{
	// Release OpenGL objects etc. even if DirectX has been released
	if (!m_hInteropDevice && !m_hInteropObject)
		return false;

	// These things need an opengl context so check
	if (wglGetCurrentContext()) {
		SpoutLogNotice("spoutGL::CleanupInterop - interop device = 0x%7.7X, interop object = 0x%7.7X", PtrToUint(m_hInteropDevice), PtrToUint(m_hInteropObject));
		if (m_hInteropDevice && m_hInteropObject) {
			SpoutLogNotice("    wglDXUnregisterObjectNV");
			if (!wglDXUnregisterObjectNV(m_hInteropDevice, m_hInteropObject)) {
				SpoutLogWarning("spoutGL::CleanupInterop - could not un-register interop");
			}
			m_hInteropObject = nullptr;
		}
		else {
			if (!m_hInteropDevice)
				SpoutLogWarning("spoutGL::CleanupInterop - null interop device");
			if (!m_hInteropObject)
				SpoutLogWarning("spoutGL::CleanupInterop - null interop object");
		}
		if (m_hInteropDevice) {
			SpoutLogNotice("    wglDXCloseDeviceNV");
			if (!wglDXCloseDeviceNV(m_hInteropDevice)) {
				SpoutLogWarning("spoutGL::CleanupInterop - could not close interop");
			}
			m_hInteropDevice = nullptr;
		}
	}
	else {
		SpoutLogWarning("spoutGL::CleanupInterop() - no context");
	}
	return true;

}

//---------------------------------------------------------
void spoutGL::CleanupGL()
{
	// Release interop objects before releasing shared texture
	// (OpenGL context is tested)
	CleanupInterop();

	// Release OpenGL resources if there is a context
	if (wglGetCurrentContext()) {

		if (m_fbo > 0) {
			// Delete the fbo before the texture so that any texture attachment 
			// is released even though it should have been
			glDeleteFramebuffersEXT(1, &m_fbo);
			m_fbo = 0;
		}

		if (m_glTexture > 0)
			glDeleteTextures(1, &m_glTexture);

		if (m_TexID > 0)
			glDeleteTextures(1, &m_TexID);

		if (m_pbo[0] > 0) {
			glDeleteBuffersEXT(m_nBuffers, m_pbo);
			m_pbo[0] = m_pbo[1] = m_pbo[2] = m_pbo[3] = 0;
		}

	}
	else {
		SpoutLogNotice("spoutGL::CleanupGL() - no context");
	}

	// Release DirectX shared texture
	if (m_pSharedTexture)
		spoutdx.ReleaseDX11Texture(GetDX11Device(), m_pSharedTexture);
	m_pSharedTexture = nullptr;
	m_dxShareHandle = nullptr;

	// Staging textures for CPU share are also released in CleanupDX11
	// But release them here to allow for situations where DirectX is not released
	if (m_pStaging[0]) spoutdx.ReleaseDX11Texture(spoutdx.GetDX11Device(), m_pStaging[0]);
	if (m_pStaging[1]) spoutdx.ReleaseDX11Texture(spoutdx.GetDX11Device(), m_pStaging[1]);
	m_pStaging[0] = nullptr;
	m_pStaging[1] = nullptr;
	m_Index = 0;
	m_NextIndex = 0;

	m_Width = 0;
	m_Height = 0;
	m_SenderName[0] = 0;
	m_bInitialized = false;

	// OpenGL only - do not close DirectX

}

// If a class OpenGL texture has not been created or it is a different size, create a new one
// Typically used for texture copy and invert
void spoutGL::CheckOpenGLTexture(GLuint &texID, GLenum GLformat, unsigned int width,  unsigned int height)
{
	if (texID == 0 || texID != m_TexID || GLformat != m_TexFormat || width != m_TexWidth || height != m_TexHeight) {
		InitTexture(texID, GLformat, width, height);
		m_TexID = texID;
		m_TexWidth  = width;
		m_TexHeight = height;
		m_TexFormat = (DWORD)GLformat;
	}
}

// Initialize OpenGL texture
void spoutGL::InitTexture(GLuint &texID, GLenum GLformat, unsigned int width, unsigned int height)
{

	if (texID != 0) glDeleteTextures(1, &texID);
	glGenTextures(1, &texID);

	// Get current texture binding
	GLint texturebinding;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &texturebinding);

	glBindTexture(GL_TEXTURE_2D, texID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GLformat, GL_UNSIGNED_BYTE, NULL);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, texturebinding);

}


//
// COPY AN OPENGL TEXTURE TO THE SHARED OPENGL TEXTURE
//
// Allows for a texture attached to the fbo
// where the input texture can be larger than the shared texture
// and Width and height are the used portion. Only the used part is copied.
//
bool spoutGL::WriteGLDXtexture(GLuint TextureID, GLuint TextureTarget,
	unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	// Only for GL/DX interop mode
	if (!m_hInteropDevice || !m_hInteropObject)
		return false;

	// Specify greater here because the width/height passed can be smaller
	if (width > m_Width || height > m_Height)
		return false;

	// Create an fbo if not already
	if (m_fbo == 0)
		glGenFramebuffersEXT(1, &m_fbo);


	// Wait for access to the shared texture
	if (frame.CheckTextureAccess(m_pSharedTexture)) {
		// lock dx interop object
		if (LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {
			// Write to the shared texture
			if (SetSharedTextureData(TextureID, TextureTarget, width, height, bInvert, HostFBO)) {
				// Increment the sender frame counter for successful write
				frame.SetNewFrame();
			}
			// unlock dx object
			UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
		}
		// Release mutex and allow access to the texture
		frame.AllowTextureAccess(m_pSharedTexture);
	}

	return true;

} // end WriteGLDXTexture


bool spoutGL::ReadGLDXtexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	// No interop, no copy
	if (!m_hInteropDevice || !m_hInteropObject) {
		return false;
	}

	// width and height must be the same as the shared texture
	// m_TextureInfo is established in CreateDX11interop
	if (width != m_Width || height != m_Height) {
		return false;
	}

	bool bRet = true; // Error only if texture read fails

	// Wait for access to the shared texture
	if (frame.CheckTextureAccess(m_pSharedTexture)) {
		// Read the shared texture if the sender has produced a new frame
		// GetNewFrame updates sender frame count and fps
		if (frame.GetNewFrame()) {
			// No texture read for zero texture - allowed for by ReceiveTexture
			if (TextureID > 0 && TextureTarget > 0) {
				if (LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {
					bRet = GetSharedTextureData(TextureID, TextureTarget, width, height, bInvert, HostFBO);
					UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
				}
			}
		}
		// Release mutex and allow access to the texture
		frame.AllowTextureAccess(m_pSharedTexture);
	}

	return bRet;

} // end ReadGLDXTexture


bool spoutGL::SetSharedTextureData(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	GLenum status = 0;
	bool bRet = false;

	// "TextureID" can be zero if it is attached to the fbo
	// m_fbo is a local FBO, "m_glTexture" is destination texture,
	// width/height are the dimensions of the destination texture.
	// Because two fbos are used, the input texture can be larger than the shared texture
	// Width and height are the used portion and only the used part is copied

	if (TextureID == 0 && HostFBO >= 0 && glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) == GL_FRAMEBUFFER_COMPLETE_EXT) {

		// The input texture is attached to attachment point 0 of the fbo passed in

		// Bind our local fbo for draw
		glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
		// Draw to the first attachment point
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		// Attach the texture we write into (the shared texture)
		glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_glTexture, 0);
		// Check draw fbo for completeness
		status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		if (status == GL_FRAMEBUFFER_COMPLETE_EXT) {
			if (m_bBLITavailable) {
				if (bInvert) {
					// copy from one framebuffer to the other while flipping upside down 
					glBlitFramebufferEXT(0, 0, width, height, 0, height, width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				}
				else {
					// Do not flip during blit
					glBlitFramebufferEXT(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				}
			}
			else {
				// No fbo blit extension
				// Copy from the host fbo (input texture attached) to the shared texture
				glBindTexture(GL_TEXTURE_2D, m_glTexture);
				glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
				glBindTexture(GL_TEXTURE_2D, 0);
			}
			bRet = true;
		}
		else {
			PrintFBOstatus(status);
			bRet = false;
		}
		// restore default fbo
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
	}
	else if (TextureID > 0) {
		// There is a valid texture passed in.
		// Copy the input texture to the destination texture.
		// Both textures must be the same size.
		bRet = CopyTexture(TextureID, TextureTarget, m_glTexture, GL_TEXTURE_2D, width, height, bInvert, HostFBO);
	}

	return bRet;

}

// Copy shared texture via fbo blit
bool spoutGL::GetSharedTextureData(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	GLenum status = 0;
	bool bRet = false;

	// bind the FBO (for both, READ_FRAMEBUFFER_EXT and DRAW_FRAMEBUFFER_EXT)
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);

	// Attach the Input texture (the shared texture) to the color buffer in our frame buffer - note texturetarget 
	glFramebufferTexture2DEXT(READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, m_glTexture, 0);
	glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);

	// Attach target texture (the one we write into and return) to second attachment point
	glFramebufferTexture2DEXT(DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, TextureTarget, TextureID, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);

	// Check read/draw fbo for completeness
	status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
	if (status == GL_FRAMEBUFFER_COMPLETE_EXT) {
		if (m_bBLITavailable) {
			// Flip if the user wants that
			if (bInvert) {
				// copy one texture buffer to the other while flipping upside down
				glBlitFramebufferEXT(0, 0,	// srcX0, srcY0, 
					width, height,	// srcX1, srcY1
					0, height,		// dstX0, dstY0,
					width, 0,		// dstX1, dstY1,
					GL_COLOR_BUFFER_BIT, GL_LINEAR);
			}
			else {
				// Do not flip during blit
				glBlitFramebufferEXT(0, 0,	// srcX0, srcY0, 
					width, height,	// srcX1, srcY1
					0, 0,			// dstX0, dstY0,
					width, height,	// dstX1, dstY1,
					GL_COLOR_BUFFER_BIT, GL_LINEAR);
			}
		}
		else {
			// No fbo blit extension available
			// Copy from the fbo (shared texture attached) to the dest texture
			glBindTexture(TextureTarget, TextureID);
			glCopyTexSubImage2D(TextureTarget, 0, 0, 0, 0, 0, width, height);
			glBindTexture(TextureTarget, 0);
		}
		bRet = true;
	}
	else {
		PrintFBOstatus(status);
		bRet = false;
	}

	// restore the previous fbo - default is 0
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT); // 04.01.16
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);

	return bRet;

}


//
// COPY IMAGE PIXELS TO THE OPENGL SHARED TEXTURE
//
bool spoutGL::WriteGLDXpixels(const unsigned char* pixels,
	unsigned int width, unsigned int height, GLenum glFormat, bool bInvert, GLuint HostFBO)
{
	if (width != m_Width || height != m_Height || !pixels)
		return false;

	// Use a GL texture so that WriteTexture can be used
	GLenum glformat = glFormat;
	bool bRet = true;

	// Create or resize a local OpenGL texture
	CheckOpenGLTexture(m_TexID, glFormat, width, height);

	// Transfer the pixels to the local texture
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // In case of RGB pixel data
	glBindTexture(GL_TEXTURE_2D, m_TexID);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, glformat, GL_UNSIGNED_BYTE, (GLvoid *)pixels);
	glBindTexture(GL_TEXTURE_2D, 0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	// Write the local texture to the shared texture and invert if necessary
	WriteGLDXtexture(m_TexID, GL_TEXTURE_2D, width, height, bInvert, HostFBO);

	return bRet;

} // end WriteGLDXpixels


//
// COPY OPENGL SHARED TEXTURE TO IMAGE PIXELS
//
bool spoutGL::ReadGLDXpixels(unsigned char* pixels,
	unsigned int width, unsigned int height,
	GLenum glFormat, bool bInvert, GLuint HostFBO)
{
	if (!m_hInteropDevice || !m_hInteropObject) {
		return false;
	}

	if (width != m_Width || height != m_Height) {
		return false;
	}

	bool bRet = true; // Error only if pixel read fails

	// retrieve opengl texture data directly to image pixels

	// Wait for access to the shared texture
	if (frame.CheckTextureAccess(m_pSharedTexture)) {
		// read texture for a new frame
		if (frame.GetNewFrame()) {
			// lock gl/dx interop object
			if (LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {
				// Set single pixel alignment in case of rgb source
				glPixelStorei(GL_PACK_ALIGNMENT, 1);
				// Always allow for invert here - only consumes 0.1 msec
				// Create or resize a local OpenGL texture
				CheckOpenGLTexture(m_TexID, glFormat, width, height);
				// Copy the shared texture to the local texture, inverting if necessary
				CopyTexture(m_glTexture, GL_TEXTURE_2D, m_TexID, GL_TEXTURE_2D, width, height, bInvert, HostFBO);
				// Extract the pixels from the local texture - changing to the user passed format
				// Use PBO method for maximum speed. ReadTextureData using glReadPixels is half the
				// speed of using DX11 texture directly (ReadDX11pixels). Note that ReadDX11pixels
				// has texture access and new frame checks and cannot be used if those checks
				// have already nbeen made.
				if (m_bPBOavailable) {
					bRet = UnloadTexturePixels(m_TexID, GL_TEXTURE_2D, width, height, 0, pixels, glFormat, false, HostFBO);
				}
				else {
					bRet = ReadTextureData(m_TexID, GL_TEXTURE_2D, width, height, 0, pixels, glFormat, false, HostFBO);
				}

				// default alignment
				glPixelStorei(GL_PACK_ALIGNMENT, 4);
			} // interop lock failed
			// Unlock interop object
			UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
		} // no new frame
		// Release mutex and allow access to the texture
		frame.AllowTextureAccess(m_pSharedTexture);
	} // mutex access failed

	return bRet;

} // end ReadGLDXpixels 


//
// Asynchronous Read-back from an OpenGL texture
//
// Used by a receiver to read pixels from a shared texture (ReceiveImage)
//
// Adapted from : http://www.songho.ca/opengl/gl_pbo.html
// Also see : https://www.seas.upenn.edu/~pcozzi/OpenGLInsights/OpenGLInsights-AsynchronousBufferTransfers.pdf
//
bool spoutGL::UnloadTexturePixels(GLuint TextureID, GLuint TextureTarget,
	unsigned int width, unsigned int height, unsigned int rowpitch,
	unsigned char* data, GLenum glFormat,
	bool bInvert, GLuint HostFBO)
{
	void *pboMemory = nullptr;
	int channels = 4; // RGBA or RGB

	if (!data) {
		return false;
	}

	if (glFormat == GL_RGB || glFormat == GL_BGR_EXT) {
		channels = 3;
	}

	unsigned int pitch = rowpitch; // row pitch passed in
	if (rowpitch == 0)
		pitch = width * channels; // RGB or RGBA

	if (m_fbo == 0) {
		SpoutLogNotice("spoutGL::UnloadTexturePixels - creating FBO");
		glGenFramebuffersEXT(1, &m_fbo);
	}

	// Create pbos if not already
	if (m_pbo[0] == 0) {
		SpoutLogNotice("spoutGL::UnloadTexturePixels - creating PBO");
		glGenBuffersEXT(m_nBuffers, m_pbo);
		PboIndex = 0;
		NextPboIndex = 0;
	}

	PboIndex = (PboIndex + 1) % m_nBuffers;
	NextPboIndex = (PboIndex + 1) % m_nBuffers;

	// If Texture ID is zero, the texture is already attached to the Host Fbo
	// and we do nothing. If not we need to create an fbo and attach the user texture
	if (TextureID > 0) {
		// Attach the texture to point 0
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, TextureTarget, TextureID, 0);
		// Set the target framebuffer to read
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	}
	else if (HostFBO == 0) {
		// If no texture ID, a Host FBO must be provided
		// testing only - error log will repeat
		return false;
	}

	// Bind the PBO
	glBindBufferEXT(GL_PIXEL_PACK_BUFFER, m_pbo[PboIndex]);

	// Check it's size
	GLint size = 0;
	glGetBufferParameterivEXT(GL_PIXEL_PACK_BUFFER, GL_BUFFER_SIZE_EXT, &size);
	if (size > 0 && size != (int)(pitch * height)) {
		// All PBOs must be re-created
		glBindBufferEXT(GL_PIXEL_PACK_BUFFER, 0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
		glDeleteBuffersEXT(m_nBuffers, m_pbo);
		m_pbo[0] = m_pbo[1] = m_pbo[2] = m_pbo[3] = 0;
		return false;
	}

	// Null existing PBO data to avoid a stall
	// This allocates memory for the PBO pitch*height wide
	glBufferDataEXT(GL_PIXEL_PACK_BUFFER, pitch*height, 0, GL_STREAM_READ);

	// Read pixels from framebuffer to PBO - glReadPixels() should return immediately.
	glPixelStorei(GL_PACK_ROW_LENGTH, pitch / channels); // row length in pixels
	glReadPixels(0, 0, width, height, glFormat, GL_UNSIGNED_BYTE, (GLvoid *)0);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);

	// If there is data in the next pbo from the previous call, read it back
	glBindBufferEXT(GL_PIXEL_PACK_BUFFER, m_pbo[NextPboIndex]);

	// Map the PBO to process its data by CPU
	pboMemory = glMapBufferEXT(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

	// glMapBuffer can return NULL when called the first time
	// when the next pbo has not been filled with data yet
	glGetError(); // remove the last error

	if (pboMemory && data) {
		// Update data directly from the mapped buffer (TODO: RGB)
		spoutcopy.CopyPixels((const unsigned char*)pboMemory, (unsigned char*)data, pitch / channels, height, glFormat, bInvert);
		glUnmapBufferEXT(GL_PIXEL_PACK_BUFFER);
	}
	// skip the copy rather than return false.

	// Back to conventional pixel operation
	glBindBufferEXT(GL_PIXEL_PACK_BUFFER, 0);

	// Restore the previous fbo binding
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);

	return true;

}

/*
//
// glMapBufferRange method
// Requires work using glFenceSync to avoid stall
//
bool spoutGL::UnloadTexturePixels(GLuint TextureID, GLuint TextureTarget,
	unsigned int width, unsigned int height, unsigned int rowpitch,
	unsigned char* data, GLenum glFormat,
	bool bInvert, GLuint HostFBO)
{
	void *pboMemory = nullptr;
	int channels = 4; // RGBA or RGB

	if (!data) {
		return false;
	}

	if (glFormat == GL_RGB || glFormat == GL_BGR_EXT) {
		channels = 3;
	}

	unsigned int pitch = rowpitch; // row pitch passed in
	if (rowpitch == 0)
		pitch = width * channels; // RGB or RGBA

	if (m_fbo == 0) {
		SpoutLogNotice("spoutGL::UnloadTexturePixels - creating FBO");
		glGenFramebuffersEXT(1, &m_fbo);
	}

	// Create pbos if not already
	if (m_pbo[0] == 0) {
		SpoutLogNotice("spoutGL::UnloadTexturePixels - creating %d PBOs", m_nBuffers);
		glGenBuffersEXT(m_nBuffers, m_pbo);
		PboIndex = 0;
		NextPboIndex = 0;
	}

	PboIndex = (PboIndex + 1) % m_nBuffers;
	NextPboIndex = (PboIndex + 1) % m_nBuffers;

	// If Texture ID is zero, the texture is already attached to the Host Fbo
	// and we do nothing. If not we need to create an fbo and attach the user texture.
	if (TextureID > 0) {
		// Attach the texture to point 0
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, TextureTarget, TextureID, 0);
		// Set the target framebuffer to read
		glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
	}
	else if (HostFBO == 0) {
		// If no texture ID, a Host FBO must be provided
		return false;
	}

	// Bind the PBO
	glBindBufferEXT(GL_PIXEL_PACK_BUFFER, m_pbo[PboIndex]);

	// Check it's size
	GLint buffersize = 0;
	glGetBufferParameterivEXT(GL_PIXEL_PACK_BUFFER, GL_BUFFER_SIZE_EXT, &buffersize);
	if (buffersize > 0 && buffersize != (int)(pitch * height)) {
		// For a sender size change, all PBOs must be re-created.
		glBindBufferEXT(GL_PIXEL_PACK_BUFFER, 0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
		glDeleteBuffersEXT(m_nBuffers, m_pbo);
		m_pbo[0] = m_pbo[1] = m_pbo[2] = m_pbo[3] = 0;
		return false;
	}

	// Allocate pbo data buffer with glBufferStorage.
	// The buffer is immutable and size is set for the lifetime of the object.
	if (buffersize == 0) {
		glBufferStorageEXT(GL_PIXEL_PACK_BUFFER, pitch*height, 0, GL_MAP_READ_BIT);
		glBindBufferEXT(GL_PIXEL_PACK_BUFFER, 0);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
		return false; // No more for this round
	}

	// Read pixels from framebuffer to PBO - glReadPixels() should return immediately.
	glPixelStorei(GL_PACK_ROW_LENGTH, pitch / channels); // row length in pixels
	glReadPixels(0, 0, width, height, glFormat, GL_UNSIGNED_BYTE, (GLvoid *)0);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);

	// If there is data in the next pbo from the previous call, read it back.
	glBindBufferEXT(GL_PIXEL_PACK_BUFFER, m_pbo[NextPboIndex]);

	// Map the PBO to process its data by CPU.
	// Map the entire data store into the client's address space.
	// glMapBufferRange may give improved performance over glMapBuffer.
	// GL_MAP_READ_BIT indicates that the returned pointer may be used to read buffer object data.
	pboMemory = glMapBufferRangeEXT(GL_PIXEL_PACK_BUFFER, 0, buffersize, GL_MAP_READ_BIT);

	// glMapBuffer can return NULL when called the first time
	// when the next pbo has not been filled with data yet.
	// Remove the last error
	glGetError();

	// Update data directly from the mapped buffer.
	// If no pbo data, skip the copy rather than return false.
	if (pboMemory) {
		spoutcopy.CopyPixels((const unsigned char*)pboMemory, (unsigned char*)data, pitch / channels, height, glFormat, bInvert);
		glUnmapBufferEXT(GL_PIXEL_PACK_BUFFER);
	}

	// Back to conventional pixel operation
	glBindBufferEXT(GL_PIXEL_PACK_BUFFER, 0);

	// Restore the default
	if (TextureID > 0) glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	// Restore the previous fbo binding
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);

	return true;

}
*/


//
// Copy OpenGL to DirectX 11 texture via CPU where the GL/DX interop is not available
//
// GPU read is from OpenGL.
// Use multiple PBOs instead of glReadPixels for best speed.
//
bool spoutGL::WriteDX11texture(GLuint TextureID, GLuint TextureTarget,
	unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	D3D11_MAPPED_SUBRESOURCE mappedSubResource;

	// Only for DX11 mode
	if (!spoutdx.GetDX11Context()) {
		return false;
	}

	// If a staging texture has not been created or a different size create a new one
	// Only one staging texture is required. Buffering read from GPU is done by OpenGL PBO.
	if (!CheckStagingTextures(width, height, 1))
		return false;

	// Map the DX11 staging texture and write the sender OpenGL texture pixels to it
	if (SUCCEEDED(spoutdx.GetDX11Context()->Map(m_pStaging[0], 0, D3D11_MAP_WRITE, 0, &mappedSubResource))) {

		// Staging texture width is multiples of 16 and pitch can be greater that width*4
		// Copy OpenGL texture pixelsto the staging texture taking account of the destination row pitch
		if (m_bPBOavailable) {
			if (!UnloadTexturePixels(TextureID, TextureTarget, width, height,
				mappedSubResource.RowPitch, (unsigned char *)mappedSubResource.pData,
				GL_BGRA_EXT, bInvert, HostFBO)) {
					spoutdx.GetDX11Context()->Unmap(m_pStaging[0], 0);
					return false;
			}
		}
		else {
			if (!ReadTextureData(TextureID, TextureTarget, // OpenGL source texture
				width, height, // width and height of OpenGL texture
				mappedSubResource.RowPitch, // bytes per line of staging texture
				(unsigned char *)mappedSubResource.pData, // staging texture pixels
				GL_BGRA_EXT, bInvert, HostFBO)) {
					spoutdx.GetDX11Context()->Unmap(m_pStaging[0], 0);
					return false;
			}
		}
		spoutdx.GetDX11Context()->Unmap(m_pStaging[0], 0);

		// The staging texture is updated with the OpenGL texture data
		// Write it to the sender's shared texture
		return WriteTexture(&m_pStaging[0]);

	}

	return false;

} // end WriteDX11texture

//
// Copy from the shared DX11 texture to an OpenGL texture via CPU staging texture
// GPU write is to OpenGL
//
bool spoutGL::ReadDX11texture(GLuint TextureID, GLuint TextureTarget,
	unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	D3D11_MAPPED_SUBRESOURCE mappedSubResource;

	// Quit for zero texture
	if (TextureID == 0 || TextureTarget == 0) {
		return false;
	}

	// Only for DX11 mode
	if (!spoutdx.GetDX11Context())
		return false;

	// Only one staging texture is required because GPU write is to OpenGL
	if (!CheckStagingTextures(width, height, 1)) {
		return false;
	}

	// Read from from the sender shared texture to a staging texture
	if (!ReadTexture(&m_pStaging[0])) {
		return false;
	}

	// Update the application receiving OpenGL texture from the DX11 staging texture
	// Default format is BGRA. Change if the sender is RGBA.
	GLenum glFormat = GL_BGRA_EXT;
	if (m_dwFormat == 28) glFormat = GL_RGBA;

	// Make sure the GPU is ready to access the staging texture 
	spoutdx.GetDX11Context()->Flush();

	// Map the staging texture to access the sender pixels
	if (SUCCEEDED(spoutdx.GetDX11Context()->Map(m_pStaging[0], 0, D3D11_MAP_READ, 0, &mappedSubResource))) {

		// TODO : format testing for invert if m_TexID exists
		if (bInvert) {
			// Create or resize a local OpenGL texture
			CheckOpenGLTexture(m_TexID, glFormat, width, height);
			// Copy the DX11 pixels to it
			glBindTexture(GL_TEXTURE_2D, m_TexID);
		}
		else {
			// Copy the DX11 pixels to the user texture
			glBindTexture(TextureTarget, TextureID);
		}

		// Allow for the staging texture for row pitch
		glPixelStorei(GL_UNPACK_ROW_LENGTH, mappedSubResource.RowPitch / 4); // row length in pixels

		// Get the pixels from the staging texture
		glTexSubImage2D(TextureTarget, 0, 0, 0, width, height,
			glFormat, GL_UNSIGNED_BYTE, (const GLvoid *)mappedSubResource.pData);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		glBindTexture(GL_TEXTURE_2D, 0);
		
		// Copy the local texture to the user texture and invert as necessary
		if(bInvert)	
			CopyTexture(m_TexID, GL_TEXTURE_2D, TextureID, TextureTarget, width, height, bInvert, HostFBO);

		spoutdx.GetDX11Context()->Unmap(m_pStaging[0], 0);

		return true;
	}

	return false;

} // end ReadDX11texture


//
// Group: Data sharing
//
//   General purpose data exchange functions using shared memory.
//   These functions can be used in addition to texture sharing.
//   Typical uses will be for data attached to the video frame,
//   commonly referred to as "per frame Metadata".
//
//   Notes for synchronisation.
//
//   If used before sending and after receiving, the data will be 
//   associated with the same video frame, but frames may be missed 
//   if the receiver has a lower frame rate than the sender.
//
//   If strict synchronization is required, the data sharing functions
//   should be used in combination with event signal functions. The sender
//   frame rate will be matched exactly to that of the receiver and the 
//   receiver will not miss any frames.
//
//      - void SetFrameSync(const char* SenderName);
//      - bool WaitFrameSync(const char *SenderName, DWORD dwTimeout = 0);
//
//   WaitFrameSync
//   A sender should use this before rendering or sending texture or data and
//   wait for a signal from the receiver that it is ready to read another frame.
//
//   SetFrameSync
//   After receiving a texture, rendering the result and reading data
//   a receiver should signal that it is ready to read another. 
//

//---------------------------------------------------------
// Function: WriteMemoryBuffer
// Write buffer to shared memory.
//
//    If shared memory has not been created in advance, it will be
//    created on the first call to this function at the length specified.
//
//    This is acceptable if the data to send is fixed in length.
//    Otherwise the shared memory should be created in advance of sufficient
//    size to contain the maximum length expected (see CreateMemoryBuffer).
//
//    The map is closed when the sender is released.
//
bool spoutGL::WriteMemoryBuffer(const char *name, const char* data, int length)
{
	// Quit if 2.006 memoryshare mode
	if (m_bMemoryShare)
		return false;

	if (!name || !name[0]) {
		SpoutLogError("spoutGL::WriteMemoryBuffer - no name");
		return false;
	}

	if (!data) {
		SpoutLogError("spoutGL::WriteMemoryBuffer - no data");
		return false;
	}

	// Create a shared memory map if it does not exist.
	if (memoryshare.Size() == 0) {
		// Create a name for the map
		std::string namestring = name;
		namestring += "_map";
		// Create a shared memory map
		if (!CreateMemoryBuffer(namestring.c_str(), length))
			return false;
	}

	char* pBuffer = memoryshare.Lock();
	if (!pBuffer) {
		SpoutLogError("spoutGL::WriteMemoryBuffer - no buffer lock");
		return false;
	}

	// Write user data to shared memory (skip the first 16 bytes containing the map size)
	memcpy(reinterpret_cast<void *>(pBuffer + 16), reinterpret_cast<const void *>(data), length);

	// Terminate the shared memory data with a null.
	// The map is created larger in advance to allow for it.
	if (memoryshare.Size() > (16 + length))
		*(pBuffer + 16 + length) = 0;

	memoryshare.Unlock();

	return true;

}

//---------------------------------------------------------
// Function: ReadMemoryBuffer
// Read shared memory to a buffer.
//
//    Open a memory map and retain the handle.
//    The map is closed when the receiver is released.
int spoutGL::ReadMemoryBuffer(const char* name, char* data, int maxlength)
{
	// Quit if 2.006 memoryshare mode
	if (m_bMemoryShare)
		return 0;

	if (!name || !name[0]) {
		SpoutLogError("spoutGL::ReadMemoryBuffer - no name");
		return 0;
	}

	if (!data) {
		SpoutLogError("spoutGL::ReadMemoryBuffer - no data");
		return 0;
	}

	// Open a shared memory map for read if not done already
	if (!memoryshare.Name()) {
		// Create a name for the map
		std::string namestring = name;
		namestring += "_map";
		// Open the shared memory. This also creates a mutex
		// for the reader to lock and unlock the map for reads.
		if (!memoryshare.Open(namestring.c_str())) {
			return 0;
		}
		SpoutLogNotice("spoutGL::ReadMemoryBuffer - opened memory map [%s]", memoryshare.Name());
	}

	char* pBuffer = memoryshare.Lock();
	if (!pBuffer) {
		SpoutLogError("spoutGL::ReadMemoryBuffer - no buffer lock");
		return 0;
	}

	// The memory map includes it's size, saved as the first 16 bytes
	*(pBuffer + 15) = 0; // End for atoi

	// Number of bytes available for data transfer
	int nbytes = atoi(reinterpret_cast<char *>(pBuffer));

	// Reduce if the user buffer max length is less
	if (maxlength < nbytes)
		nbytes = maxlength;

	// Copy bytes from shared memory to the user buffer
	if (nbytes > 0)
		memcpy(reinterpret_cast<void *>(data), reinterpret_cast<const void *>(pBuffer + 16), nbytes);

	// Done with the shared memory pointer
	memoryshare.Unlock();

	return nbytes;
}

//---------------------------------------------------------
// Function: CreateMemoryBuffer
// Create a shared memory buffer.
//
//    Create a memory map and retain the handle.
//    This function should be called before any buffer write
//    if the length of the data to send will vary.
//    The map is closed when the sender is released (see Spout.cpp ReleaseReceiver).
bool spoutGL::CreateMemoryBuffer(const char *name, int length)
{
	// Quit if 2.006 memoryshare mode
	if (m_bMemoryShare)
		return false;

	if (!name || !name[0]) {
		SpoutLogError("spoutGL::CreateMemoryBuffer - no name");
		return false;
	}

	if (memoryshare.Size() > 0) {
		SpoutLogError("spoutGL::CreateMemoryBuffer - shared memory already exists");
		return false;
	}

	// Create a name for the map from the sender name
	std::string namestring = name;
	namestring += "_map";

	// The first 16 bytes are reserved to record the number of bytes available
	// for data transfer. Make the map 16 bytes larger to compensate. 
	// Add another 16 bytes to allow for a null terminator.
	// (Use multiples of 16 for alignment to allow for SSE copy : TODO).
	if (!memoryshare.Create(namestring.c_str(), length + 32)) {
		SpoutLogError("spoutGL::CreateMemoryBuffer - could not create shared memory");
		return false;
	}

	// The length requested is the number of bytes to be
	// available for data transfer (map data size).
	char* pBuffer = memoryshare.Lock();
	if (!pBuffer) {
		SpoutLogError("spoutGL::CreateMemoryBuffer - no buffer lock");
		return false;
	}

	// Convert the map data size to decimal digit chars
	// directly to the first 16 bytes of the shared memory.
	_itoa_s(length, reinterpret_cast<char *>(pBuffer), 16, 10);

	memoryshare.Unlock();
	SpoutLogNotice("spoutGL::CreateMemoryBuffer - created shared memory buffer %d bytes", length);

	return true;
}


//---------------------------------------------------------
// Function: DeleteMemoryBuffer
// Delete a sender shared memory buffer.
//
bool spoutGL::DeleteMemoryBuffer()
{
	// Quit if 2.006 memoryshare mode
	if (m_bMemoryShare)
		return false;

	// Only the application that creates a map can close it.
	// The writer creates the map and records the size (memoryshare.Size()).
	// A reader must open a map to find the size and does not record it.
	if (memoryshare.Size() == 0) {
		SpoutLogError("spoutGL::DeleteMemoryBuffer - no shared memory size");
		return false;
	}

	memoryshare.Close();

	return true;

}


//---------------------------------------------------------
// Function: GetMemoryBufferSize
// Get the number of bytes available for data transfer.
//
int spoutGL::GetMemoryBufferSize(const char *name)
{
	// A writer has created the map (Create) and set the map size.
	// The data length is recorded in the first 16 bytes.
	// Another 16 bytes is added to allow for a terminating NULL. (See CreateMemoryBuffer)
	// The remaining length is the number of bytes available for data transfer.
	if (memoryshare.Size() > 32) {
		return memoryshare.Size()-32;
	}

	// A reader must read the map to get the size.
	// The size is not set in the SpoutSharedMemory class.
	// Open a shared memory map for the buffer if it not already.
	// (after a map is opened, the name is saved in the SpoutSharedMemory class).
	if (!memoryshare.Name()) {
		// Create a name for the map
		std::string namestring = name;
		namestring += "_map";
		// Open the shared memory map.
		// This also creates a mutex for the receiver to lock and unlock the map for reads.
		if (!memoryshare.Open(namestring.c_str())) {
			return 0;
		}
		SpoutLogNotice("spoutGL::GetMemoryBufferSize - opened sender memory map [%s]", memoryshare.Name());
	}

	// The map is open and a name for it has been recorded
	char* pBuffer = memoryshare.Lock();
	if (!pBuffer) {
		SpoutLogError("spoutGL::GetMemoryBufferSize - no buffer lock");
		return 0;
	}

	// The number of bytes of the memory map available for data transfer
	// is saved in the first 16 bytes.
	*(pBuffer + 15) = 0; // End for atoi
	int nbytes = atoi(reinterpret_cast<char *>(pBuffer));

	memoryshare.Unlock();

	return nbytes;

}


// Copy OpenGL texture data to a pixel buffer via fbo
bool spoutGL::ReadTextureData(GLuint SourceID, GLuint SourceTarget,
	unsigned int width, unsigned int height, unsigned int pitch,
	unsigned char* dest, GLenum GLformat, bool bInvert, GLuint HostFBO)
{
	if (!m_bFBOavailable || GLformat == GL_RGB || GLformat == GL_BGR_EXT) {
		if (bInvert) {
			// Copy to intermediate buffer
			unsigned char* src = nullptr;
			if(GLformat == GL_RGB || GLformat == GL_BGR_EXT)
				src = new unsigned char[width * height * 3];
			else
				src = new unsigned char[width * height * 4];
			glBindTexture(SourceTarget, SourceID);
			glGetTexImage(SourceTarget, 0, GLformat, GL_UNSIGNED_BYTE, (void *)src);
			glBindTexture(SourceTarget, 0);
			// Flip the buffer
			spoutcopy.FlipBuffer(src, dest, width, height, GLformat);
			delete src;
		}
		else {
			// dest must be RGBA or RGB width x height
			glBindTexture(SourceTarget, SourceID);
			glGetTexImage(SourceTarget, 0, GLformat, GL_UNSIGNED_BYTE, (void *)dest);
			glBindTexture(SourceTarget, 0);
		}
		return true;
	}
	else {
		
		//
		// RGBA only
		//

		GLenum status = 0;

		// Create or resize a local OpenGL texture
		CheckOpenGLTexture(m_TexID, GL_RGBA, width, height);

		// Create a local fbo if not already
		if (m_fbo == 0)	glGenFramebuffersEXT(1, &m_fbo);

		// If texture ID is zero, assume the source texture is attached
		// to the host fbo which is bound for read and write
		if (SourceID == 0 && HostFBO > 0) {
			// Bind our local fbo for draw only
			glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT, m_fbo);
			// Source texture is already attached to point 0 for read
		}
		else {
			// bind the local fbo for read and write
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, m_fbo);
			// Read from attachment point 0
			glReadBuffer(GL_COLOR_ATTACHMENT0_EXT);
			// Attach the Source texture to point 0 for read
			glFramebufferTexture2DEXT(READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, SourceTarget, SourceID, 0);
		}

		// Draw to attachment point 1
		glDrawBuffer(GL_COLOR_ATTACHMENT1_EXT);

		// Attach the texture we write into (the local texture) to attachment point 1
		glFramebufferTexture2DEXT(DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, m_TexID, 0);

		// Check read/draw fbo for completeness
		status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
		if (status == GL_FRAMEBUFFER_COMPLETE_EXT) {
			if (bInvert && m_bBLITavailable) {
				// copy the source texture (0) to the local texture (1) while flipping upside down 
				glBlitFramebufferEXT(0, 0, width, height, 0, height, width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
				// Bind local fbo for read
				glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, m_fbo);
				// Read from attachment point 1
				glReadBuffer(GL_COLOR_ATTACHMENT1_EXT);
				// Read pixels from it
				glPixelStorei(GL_PACK_ROW_LENGTH, pitch / 4); // row length in pixels
				glReadPixels(0, 0, width, height, GLformat, GL_UNSIGNED_BYTE, (GLvoid *)dest);
				glPixelStorei(GL_PACK_ROW_LENGTH, 0);

			}
			else {
				// No invert or no fbo blit extension
				// Read from the source texture attachment point 0
				// This will be the local fbo if a texture ID was passed in
				// Pitch is destination line length in bytes. Divide by 4 to get the width in rgba pixels.
				glPixelStorei(GL_PACK_ROW_LENGTH, pitch / 4); // row length in pixels
				glReadPixels(0, 0, width, height, GLformat, GL_UNSIGNED_BYTE, (GLvoid *)dest);
				glPixelStorei(GL_PACK_ROW_LENGTH, 0);
			}
		}
		else {
			PrintFBOstatus(status);
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
			return false;
		}

		// restore the previous fbo - default is 0
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
	}

	return true;

} // end ReadTextureData

//
// COPY IMAGE PIXELS TO THE SHARED DX11 TEXTURE VIA STAGING TEXTURES
// RGBA/RGB/BGRA/BGR supported
//
// GPU write is to DX11
// Use staging texture to support RGBA/RGB
//
bool spoutGL::WriteDX11pixels(const unsigned char* pixels,
	unsigned int width, unsigned int height, GLenum glFormat, bool bInvert)
{
	if (width != m_Width || height != m_Height || !pixels)
		return false;

	// if(!CheckStagingTextures(width, height, 1))
	if (!CheckStagingTextures(width, height, 2))
		return false;

	// 1) pixels (RGBA or RGB) -> staging texture (RGBA) - CPU
	// 2) staging texture -> DX11 sender texture (RGBA) CopyResource - GPU
	//
	// RGBA :
	//   6.2 msec @ 3840 x 2160
	//   1.6 msec @ 1920 x 1080
	// RGBA Using UpdateSubresource (pixels > texture) instead of pixels > staging > texture
	//   5.8 msec @ 3840 x 2160
	//   1.5 msec @ 1920 x 1080
	// RGB :
	//   10 msec @ 3840 x 2160
	//   2.7 msec @ 1920 x 1080
	//
	// Access the sender shared texture
	if (frame.CheckTextureAccess(m_pSharedTexture)) {
		// Map the staging texture and write pixels to it (CPU)
		WritePixelData(pixels, m_pStaging[0], width, height, glFormat, bInvert);
		// Copy from the staging texture to the sender shared texture (GPU)
		spoutdx.GetDX11Context()->CopyResource(m_pSharedTexture, m_pStaging[0]);
		spoutdx.GetDX11Context()->Flush();
		frame.SetNewFrame();
		frame.AllowTextureAccess(m_pSharedTexture);
		return true;
	}
	return false;

} // end WriteDX11pixels


// Receive from a sender via DX11 staging textures to an rgba or rgb buffer of variable size
// A new shared texture pointer (m_pSharedTexture) is retrieved if the sender changed
bool spoutGL::ReadDX11pixels(unsigned char * pixels, unsigned int width, unsigned int height, GLenum glFormat, bool bInvert)
{
	if (!CheckStagingTextures(width, height, 2)) {
		return false;
	}

	// Access the sender shared texture
	if (frame.CheckTextureAccess(m_pSharedTexture)) {

		// Check if the sender has produced a new frame.
		if (frame.GetNewFrame()) {

			// Read from the sender GPU texture to CPU pixels via two staging textures
			m_Index = (m_Index + 1) % 2;
			m_NextIndex = (m_Index + 1) % 2;
			// Copy from the sender's shared texture to the first staging texture
			spoutdx.GetDX11Context()->CopyResource(m_pStaging[m_Index], m_pSharedTexture);
			
			// Map and read from the second while the first is occupied
			ReadPixelData(m_pStaging[m_NextIndex], pixels, m_Width, m_Height, glFormat, bInvert);
		}
		// Allow access to the shared texture
		frame.AllowTextureAccess(m_pSharedTexture);
		return true;
	}

	return false;

}


// RGBA/RGB/BGRA/BGR supported
bool spoutGL::WritePixelData(const unsigned char* pixels, ID3D11Texture2D* pStagingTexture,
	unsigned int width, unsigned int height, GLenum glFormat, bool bInvert)
{
	if (!spoutdx.GetDX11Context() || !pStagingTexture || !pixels)
		return false;

	// glFormat = GL_RGB      0x1907
	// glFormat = GL_RGBA     0x1908
	// glFormat = GL_BGR_EXT  0x80E0
	// glFormat = GL_BGRA_EXT 0x80E1
	//
	// m_dwFormat = 28 RGBA staging textures
	// m_dwFormat = 87 BGRA staging textures

	// Map the resource so we can access the pixels
	D3D11_MAPPED_SUBRESOURCE mappedSubResource;
	// Make sure all commands are done before mapping the staging texture
	spoutdx.GetDX11Context()->Flush();
	// Map waits for GPU access
	HRESULT hr = spoutdx.GetDX11Context()->Map(pStagingTexture, 0, D3D11_MAP_READ, 0, &mappedSubResource);
	if (SUCCEEDED(hr)) {
		//
		// Copy from the pixel buffer to the staging texture
		//
		// The shared texture format is BGRA or RGBA and the staging textures are the same format.
		// If the texture format is BGRA and the receiving pixel buffer is RGBA/RGB or vice-versa,
		// the data has to be converted from BGRA to RGBA/RGB or RGBA to BGRA/BGR during the pixel copy.
		//
		if (glFormat == GL_RGBA) { // RGBA pixel buffer
			if (m_dwFormat == 28) // RGBA staging textures
				spoutcopy.rgba2rgba((const void *)pixels, mappedSubResource.pData,
					width, height, width*4, mappedSubResource.RowPitch, bInvert);
			else
				spoutcopy.rgba2bgra((const void *)pixels, mappedSubResource.pData, 
					width, height, width*4, mappedSubResource.RowPitch, bInvert);
		}
		else if (glFormat == GL_BGRA_EXT) { // BGRA pixel buffer
			if (m_dwFormat == 28)
				spoutcopy.rgba2bgra((const void *)pixels, mappedSubResource.pData,
					width, height, width * 4, mappedSubResource.RowPitch, bInvert);
			else
				spoutcopy.rgba2rgba((const void *)pixels, mappedSubResource.pData,
					width, height, width*4, mappedSubResource.RowPitch, bInvert);
		}
		else if (glFormat == GL_RGB) { // RGB pixel buffer
			if (m_dwFormat == 28)
				spoutcopy.rgb2rgba((const void *)pixels, mappedSubResource.pData, 
					width, height, mappedSubResource.RowPitch, bInvert);
			else
				spoutcopy.rgb2bgra((const void *)pixels, mappedSubResource.pData,
					width, height, mappedSubResource.RowPitch, bInvert);
		}
		else if (glFormat == GL_BGR_EXT) { // BGR pixel buffer
			if (m_dwFormat == 28)
				spoutcopy.bgr2rgba((const void *)pixels, mappedSubResource.pData,
					width, height, mappedSubResource.RowPitch, bInvert);
			else
				spoutcopy.rgb2rgba((const void *)pixels, mappedSubResource.pData,
					width, height, mappedSubResource.RowPitch, bInvert);
		}
		spoutdx.GetDX11Context()->Unmap(pStagingTexture, 0);

		return true;

	} // endif DX11 map OK

	return false;

} // end WritePixelData


//
// COPY FROM A DX11 STAGING TEXTURE TO A USER RGBA/RGB/BGR PIXEL BUFFER
//
bool spoutGL::ReadPixelData(ID3D11Texture2D* pStagingTexture, unsigned char* pixels,
	unsigned int width, unsigned int height, GLenum glFormat, bool bInvert)
{
	if (!spoutdx.GetDX11Context() || !pStagingTexture || !pixels)
		return false;

	// glFormat = GL_RGB      0x1907
	// glFormat = GL_RGBA     0x1908
	// glFormat = GL_BGR_EXT  0x80E0
	// glFormat = GL_BGRA_EXT 0x80E1
	//
	// m_dwFormat = 28 RGBA staging textures
	// m_dwFormat = 87 BGRA staging textures

	// Map the resource so we can access the pixels
	D3D11_MAPPED_SUBRESOURCE mappedSubResource;
	// Make sure all commands are done before mapping the staging texture
	spoutdx.GetDX11Context()->Flush();
	// Map waits for GPU access
	HRESULT hr = spoutdx.GetDX11Context()->Map(pStagingTexture, 0, D3D11_MAP_READ, 0, &mappedSubResource);
	if (SUCCEEDED(hr)) {
		//
		// Copy from staging texture to the pixel buffer
		//
		// The shared texture format is BGRA or RGBA and the staging textures are the same format.
		// If the texture format is BGRA and the receiving pixel buffer is RGBA/RGB or vice-versa,
		// the data has to be converted from BGRA to RGBA/RGB or RGBA to BGRA/BGR during the pixel copy.
		//
		if (glFormat == GL_RGBA) { // RGBA pixel buffer
			if (m_dwFormat == 28) // RGBA staging textures
				spoutcopy.rgba2rgba(mappedSubResource.pData, pixels, width, height, mappedSubResource.RowPitch, bInvert);
			else
				spoutcopy.rgba2bgra(mappedSubResource.pData, pixels, width, height, mappedSubResource.RowPitch, bInvert);
		}
		else if (glFormat == GL_BGRA_EXT) { // BGRA pixel buffer
			if (m_dwFormat == 28)
				spoutcopy.rgba2bgra(mappedSubResource.pData, pixels, width, height, mappedSubResource.RowPitch, bInvert);
			else
				spoutcopy.rgba2rgba(mappedSubResource.pData, pixels, width, height, mappedSubResource.RowPitch, bInvert);
		}
		else if (glFormat == GL_RGB) { // RGB pixel buffer
			if (m_dwFormat == 28)
				spoutcopy.rgba2rgb(mappedSubResource.pData, pixels, m_Width, m_Height, mappedSubResource.RowPitch, bInvert);
			else
				spoutcopy.rgba2bgr(mappedSubResource.pData, pixels, m_Width, m_Height, mappedSubResource.RowPitch, bInvert);
		}
		else if (glFormat == GL_BGR_EXT) { // BGR pixel buffer
			if (m_dwFormat == 28)
				spoutcopy.rgba2bgr(mappedSubResource.pData, pixels, m_Width, m_Height, mappedSubResource.RowPitch, bInvert);
			else
				spoutcopy.rgba2rgb(mappedSubResource.pData, pixels, m_Width, m_Height, mappedSubResource.RowPitch, bInvert);
		}

		spoutdx.GetDX11Context()->Unmap(pStagingTexture, 0);

		return true;
	} // endif DX11 map OK

	return false;

} // end ReadPixelData


// Create class staging textures for changed size or if they do not exist yet
// Two are available but only one can be allocated to save memory
// Format is the same as the shared texture - m_dwFormat
bool spoutGL::CheckStagingTextures(unsigned int width, unsigned int height, int nTextures)
{
	if (!spoutdx.GetDX11Device()) {
		return false;
	}

	D3D11_TEXTURE2D_DESC desc = { 0 };

	if (m_pStaging[0]) {

		// Get the size to test for change
		m_pStaging[0]->GetDesc(&desc);
		if (desc.Width != width || desc.Height != height) {
			// Staging textures must not be mapped before release
			if (m_pStaging[0]) spoutdx.ReleaseDX11Texture(spoutdx.GetDX11Device(), m_pStaging[0]);
			if (m_pStaging[1]) spoutdx.ReleaseDX11Texture(spoutdx.GetDX11Device(), m_pStaging[1]);
			m_pStaging[0] = nullptr;
			m_pStaging[1] = nullptr;
			// Drop through to create new textures
		}
		else {
			return true;
		}
	}

	if (!spoutdx.CreateDX11StagingTexture(spoutdx.GetDX11Device(), width, height, (DXGI_FORMAT)m_dwFormat, &m_pStaging[0]))
		return false;

	if (nTextures > 1) {
		if (!spoutdx.CreateDX11StagingTexture(spoutdx.GetDX11Device(), width, height, (DXGI_FORMAT)m_dwFormat, &m_pStaging[1]))
			return false;
	}

	// Update class width and height
	m_Width = width;
	m_Height = height;

	// Reset staging texture index
	m_Index = 0;
	m_NextIndex = 0;

	// Also reset PBO index
	PboIndex = 0;
	NextPboIndex = 0;

	// Did something go wrong somehow
	if (!m_pStaging[0])
		return false;
	if (nTextures > 1 && !m_pStaging[1])
		return false;

	return true;

} // end CheckStagingTextures


//
// Memoryshare functions - receive only
//

//
// Read rgba shared memory to texture pixel data
//
bool spoutGL::ReadMemoryTexture(const char* sendername, GLuint TexID, GLuint TextureTarget,
	unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO)
{
	// Open a shared memory map if it not already
	if (!memoryshare.Name()) {
		// Create a name for the map from the sender name
		std::string namestring = sendername;
		namestring += "_map";
		// Open the shared memory map.
		if (!memoryshare.Open(namestring.c_str())) {
			return false;
		}
		SpoutLogNotice("SpoutSharedMemory::ReadMemoryTexture - opened sender memory map [%s]", memoryshare.Name());
	}

	char* pBuffer = memoryshare.Lock();
	if (!pBuffer) {
		SpoutLogError("SpoutSharedMemory::ReadMemoryTexture - no buffer lock");
		return false;
	}

	bool bRet = true; // Error only if pixel read fails

	// Query a new frame and read pixels while the buffer is locked
	if (frame.GetNewFrame()) {
		if (bInvert) {
			// Create or resize a local OpenGL texture
			CheckOpenGLTexture(m_TexID, GL_RGBA, width, height);
			// Read the memory pixels into it
			glBindTexture(GL_TEXTURE_2D, m_TexID);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, (GLvoid *)pBuffer);
			glBindTexture(GL_TEXTURE_2D, 0);
			// Copy to the user texture, inverting at the same time
			bRet = CopyTexture(m_TexID, GL_TEXTURE_2D, TexID, TextureTarget, width, height, true, HostFBO);
		}
		else {
			// No invert - copy memory pixels directly to the user texture
			glBindTexture(TextureTarget, TexID);
			glTexSubImage2D(TextureTarget, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)pBuffer);
			glBindTexture(TextureTarget, 0);
		}
	} // No new frame

	memoryshare.Unlock();

	return bRet;

}

//
// Read shared memory to image pixels
//
bool spoutGL::ReadMemoryPixels(const char* sendername, unsigned char* pixels,
	unsigned int width, unsigned int height, GLenum glFormat, bool bInvert)
{
	if (!pixels || glFormat != GL_RGBA) {
		SpoutLogError("spoutGLDXinterop::ReadMemoryPixels - no data or incorrect format");
		return false;
	}

	// Open a shared memory map if it not already
	if (!memoryshare.Name()) {
		// Create a name for the map from the sender name
		std::string namestring = sendername;
		namestring += "_map";
		// Open the shared memory map.
		if (!memoryshare.Open(namestring.c_str())) {
			return false;
		}
		SpoutLogNotice("SpoutSharedMemory::ReadMemoryPixels - opened sender memory map [%s]", memoryshare.Name());
	}

	char* pBuffer = memoryshare.Lock();
	if (!pBuffer) {
		SpoutLogError("SpoutSharedMemory::ReadMemoryPixels - no buffer lock");
		return false;
	}

	// Query a new frame and read pixels while the buffer is locked
	if (frame.GetNewFrame()) {
		// Read pixels from shared memory
		spoutcopy.CopyPixels(reinterpret_cast<unsigned char *>(pBuffer), pixels, width, height, glFormat, bInvert);
	}

	memoryshare.Unlock();

	return true;

}

//
// Write image pixels to shared memory
//
bool spoutGL::WriteMemoryPixels(const char *sendername, const unsigned char* pixels, unsigned int width, unsigned int height, GLenum glFormat, bool bInvert)
{
	if (!pixels || glFormat != GL_RGBA) {
		SpoutLogError("spoutGLDXinterop::WriteMemoryPixels - no data or incorrect format");
		return false;
	}

	// Create a shared memory map if it does not exist yet
	char* pBuffer = nullptr;
	if (memoryshare.Size() == 0) {
		// Create a name for the map from the sender name
		std::string namestring = sendername;
		namestring += "_map";
		if (!memoryshare.Create(sendername, width*4*height)) {
			SpoutLogError("SpoutSharedMemory::WriteMemoryPixels - could not create shared memory");
			return false;
		}
		pBuffer = memoryshare.Lock();
		if (!pBuffer) {
			SpoutLogError("SpoutSharedMemory::WriteMemoryPixels - no buffer lock");
			return false;
		}
	}

	// Write pixel data to shared memory
	spoutcopy.CopyPixels(pixels, reinterpret_cast<unsigned char *>(pBuffer), width, height, glFormat, bInvert);

	memoryshare.Unlock();

	return true;

}

//
// Directx 11
//

//---------------------------------------------------------
bool spoutGL::OpenDirectX11(ID3D11Device* pDevice)
{
	return spoutdx.OpenDirectX11(pDevice);
}

//---------------------------------------------------------
ID3D11Device* spoutGL::GetDX11Device()
{
	return spoutdx.GetDX11Device();
}

//---------------------------------------------------------
ID3D11DeviceContext* spoutGL::GetDX11Context()
{
	return spoutdx.GetDX11Context();
}

//---------------------------------------------------------
void spoutGL::CleanupDirectX()
{
	// DirectX 9 not supported >= 2.007
	CleanupDX11();
}

//---------------------------------------------------------
void spoutGL::CleanupDX11()
{
	if (spoutdx.GetDX11Device()) {

		SpoutLogNotice("spoutGL::CleanupDX11()");

		// Reference count warnings are in the SpoutDirectX class

		if (m_pSharedTexture) {
			SpoutLogNotice("    Releasing shared texture");
			// Release interop link before releasing the texture
			// Requires openGL context
			if (m_hInteropDevice && m_hInteropObject) {
				if (!CleanupInterop()) {
					SpoutLogWarning("    GL/DX Interop could not be released");
				}
			}
			spoutdx.ReleaseDX11Texture(spoutdx.GetDX11Device(), m_pSharedTexture);
		}

		// Important to set pointer to NULL or it will crash if released again
		m_pSharedTexture = nullptr;

		// Re-set shared texture handle
		m_dxShareHandle = nullptr;

		// Release staging texture if they have been used
		if (m_pStaging[0]) spoutdx.ReleaseDX11Texture(spoutdx.GetDX11Device(), m_pStaging[0]);
		if (m_pStaging[1]) spoutdx.ReleaseDX11Texture(spoutdx.GetDX11Device(), m_pStaging[1]);
		m_pStaging[0] = nullptr;
		m_pStaging[1] = nullptr;
		m_Index = 0;
		m_NextIndex = 0;

		// 12.11.18 - To avoid memory leak with dynamic objects
		//            must always be freed, not only on exit.
		//            Device recreated for a new sender.
		// Releases immediate context and device in the SpoutDirectX class
		// spoutdx.GetDX11Context() and spoutdx.GetDX11Device() are copies of these
		spoutdx.CloseDirectX11();
	}
	else {
		SpoutLogNotice("spoutGL::CleanupDX11() - device closed");
	}

}

//
// Extensions and availability
//

//---------------------------------------------------------
bool spoutGL::LoadGLextensions()
{
	// Return if already loaded
	if (m_caps > 0) {
		SpoutLogNotice("spoutGL::LoadGLextensions - already loaded");
		return true;
	}

	// Needs an OpenGL context
	if (!wglGetCurrentContext()) {
		SpoutLogWarning("spoutGL::LoadGLextensions - no OpenGL context");
		return false;
	}

	m_bFBOavailable = false;
	m_bGLDXavailable = false;
	m_bBLITavailable = false;
	m_bSWAPavailable = false;
	m_bBGRAavailable = false;
	m_bCOPYavailable = false;
	m_bCONTEXTavailable = false;

	m_caps = loadGLextensions(); // in spoutGLextensions

	if (m_caps == 0) {
		SpoutLogError("spoutGL::LoadGLextensions failed");
		m_bPBOavailable = false;
		return false;
	}

	if (m_caps & GLEXT_SUPPORT_FBO)
		m_bFBOavailable = true;

	// FBO not available is terminal
	if (!m_bFBOavailable) {
		SpoutLogError("spoutGL::LoadGLextensions - no FBO extensions available");
		m_bPBOavailable = false; // No PBO support either - over-ride user selection
		return false;
	}

	if (m_caps & GLEXT_SUPPORT_NVINTEROP) m_bGLDXavailable = true; // Interop needed for texture sharing
	if (m_caps & GLEXT_SUPPORT_FBO_BLIT)  m_bBLITavailable = true;
	if (m_caps & GLEXT_SUPPORT_SWAP)      m_bSWAPavailable = true;
	if (m_caps & GLEXT_SUPPORT_BGRA)      m_bBGRAavailable = true;
	if (m_caps & GLEXT_SUPPORT_COPY)      m_bCOPYavailable = true;
	if (m_caps & GLEXT_SUPPORT_CONTEXT)   m_bCONTEXTavailable = true;

	// Test PBO availability unless user has selected buffering off
	// m_bPBOavailable also set by SetBufferMode()
	if (m_bPBOavailable) {
		if (!(m_caps && GLEXT_SUPPORT_PBO))
			m_bPBOavailable = false;
	}

	// Show status
	if (!m_bPBOavailable) {
		if (!(m_caps && GLEXT_SUPPORT_PBO))
			SpoutLogWarning("spoutGL::LoadGLextensions - pbo extensions not available");
		else
			SpoutLogWarning("spoutGL::LoadGLextensions - pbo functions disabled by settings");
	}
	else if (!m_bGLDXavailable)
		SpoutLogWarning("spoutGL::LoadGLextensions - interop extensions not available");
	else if (!m_bBLITavailable)
		SpoutLogWarning("spoutGL::LoadGLextensions - fbo blit extension not available");
	else if (!m_bSWAPavailable)
		SpoutLogWarning("spoutGL::LoadGLextensions - sync control extensions not available");
	else if (!m_bBGRAavailable)
		SpoutLogWarning("spoutGL::LoadGLextensions - bgra extension not available");
	else if (!m_bCOPYavailable)
		SpoutLogWarning("spoutGL::LoadGLextensions - copy extensions not available");
	else if (!m_bCONTEXTavailable)
		SpoutLogWarning("spoutGL::LoadGLextensions - context extension not available");
	else
		SpoutLogNotice("spoutGL::LoadGLextensions - all extensions available");

	m_bExtensionsLoaded = true;

	return true;
}

//---------------------------------------------------------
bool spoutGL::IsGLDXavailable()
{
	return m_bGLDXavailable;
}

//---------------------------------------------------------
bool spoutGL::IsBLITavailable()
{
	return m_bBLITavailable;
}

//---------------------------------------------------------
bool spoutGL::IsSWAPavailable()
{
	return m_bSWAPavailable;
}

//---------------------------------------------------------
bool spoutGL::IsBGRAavailable()
{
	return m_bBGRAavailable;
}

//---------------------------------------------------------
bool spoutGL::IsCOPYavailable()
{
	return m_bCOPYavailable;
}

//---------------------------------------------------------
bool spoutGL::IsPBOavailable()
{
	return m_bPBOavailable;
}

//---------------------------------------------------------
bool spoutGL::IsCONTEXTavailable()
{
	return m_bCONTEXTavailable;
}

// 
// Legacy OpenGL functions
//

#ifdef legacyOpenGL

//---------------------------------------------------------
void spoutGL::SaveOpenGLstate(unsigned int width, unsigned int height, bool bFitWindow)
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
	if (bFitWindow) {
		// Scale both width and height to the current viewport size
		vpScaleX = dim[2] / (float)width;
		vpScaleY = dim[3] / (float)height;
		vpWidth = (float)width  * vpScaleX;
		vpHeight = (float)height * vpScaleY;
		vpx = vpy = 0;
	}
	else {
		// Preserve aspect ratio of the sender
		// and fit to the width or the height
		vpWidth = dim[2];
		vpHeight = ((float)height / (float)width)*vpWidth;
		if (vpHeight > dim[3]) {
			vpHeight = dim[3];
			vpWidth = ((float)width / (float)height)*vpHeight;
		}
		vpx = (int)(dim[2] - vpWidth) / 2;;
		vpy = (int)(dim[3] - vpHeight) / 2;
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


void spoutGL::RestoreOpenGLstate()
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

#endif

//
// Utility
//

//---------------------------------------------------------
// Given a DeviceKey string from a DisplayDevice
// read all the information about the adapter.
// Only used by this class.
bool spoutGL::OpenDeviceKey(const char* key, int maxsize, char* description, char* version)
{
	if (!key)
		return false;

	// Extract the subkey from the DeviceKey string
	HKEY hRegKey = nullptr;
	DWORD dwSize = 0;
	DWORD dwKey = 0;

	char output[256];
	strcpy_s(output, 256, key);
	char *found = strstr(output, "System");
	if (!found)
		return false;
	std::string SubKey = found;

	// Convert all slash to double slash using a C++ string function
	// to get subkey string required to extract registry information
	for (unsigned int i = 0; i < SubKey.length(); i++) {
		if (SubKey[i] == '\\') {
			SubKey.insert(i, 1, '\\');
			++i; // Skip inserted char
		}
	}

	// Open the key to find the adapter details
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, SubKey.c_str(), NULL, KEY_READ, &hRegKey) == 0) {
		dwSize = MAX_PATH;
		// Adapter name
		if (RegQueryValueExA(hRegKey, "DriverDesc", NULL, &dwKey, (BYTE*)output, &dwSize) == 0) {
			strcpy_s(description, (rsize_t)maxsize, output);
		}
		if (RegQueryValueExA(hRegKey, "DriverVersion", NULL, &dwKey, (BYTE*)output, &dwSize) == 0) {
			// Find the last 6 characters of the version string then
			// convert to a float and multiply to get decimal in the right place
			sprintf_s(output, 256, "%5.2f", atof(output + strlen(output) - 6)*100.0);
			strcpy_s(version, (rsize_t)maxsize, output);
		} // endif DriverVersion
		RegCloseKey(hRegKey);
	} // endif RegOpenKey

	return true;
}

//---------------------------------------------------------
void spoutGL::trim(char* s) {
	char* p = s;
	int l = (int)strlen(p);

	while (isspace(p[l - 1])) p[--l] = 0;
	while (*p && isspace(*p)) ++p, --l;

	memmove(s, p, (size_t)(l + 1));
}

//
// Errors
//

void spoutGL::PrintFBOstatus(GLenum status)
{
	char tmp[256];
	sprintf_s(tmp, 256, "FBO status error %u (0x%.7X) - ", status, status);
	if (status == GL_FRAMEBUFFER_UNSUPPORTED_EXT)
		strcat_s(tmp, 256, "GL_FRAMEBUFFER_UNSUPPORTED_EXT");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT)
		strcat_s(tmp, 256, "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT)
		strcat_s(tmp, 256, "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT)
		strcat_s(tmp, 256, "GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT - width-height problems?");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT)
		strcat_s(tmp, 256, "GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT)
		strcat_s(tmp, 256, "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT");
	else if (status == GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT)
		strcat_s(tmp, 256, "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT");
	// else if (status == GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT)
	// 	strcat_s(tmp, 256, "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE_EXT\n");
	else
		strcat_s(tmp, 256, "Unknown Code");
	SpoutLogError("%s", tmp);
	GLerror();
}

bool spoutGL::GLerror() {
	GLenum err = GL_NO_ERROR;
	bool bError = false;
	while ((err = glGetError()) != GL_NO_ERROR) {
		// SpoutLogError("    GLerror - OpenGL error = %u (0x%.7X)", err, err);
		printf("    GLerror - OpenGL error = %u (0x%.7X)\n", err, err);
		bError = true;
		// gluErrorString needs Glu.h and glu32.lib (or glew)
		// printf("GL error = %d (0x%.7X) %s\n", err, err, gluErrorString(err));
	}

	return bError;
}

//
// Group: User registry settings recorded by "SpoutSettings"
//
// User settings are retrieved in constructor and can be 
// set for the application (except max senders which must be global)
//

//---------------------------------------------------------
// Function: GetBufferMode
// Get user buffering mode
//
bool spoutGL::GetBufferMode()
{
	return m_bPBOavailable;
}

//---------------------------------------------------------
// Function: SetBufferMode
// Set application buffering mode
void spoutGL::SetBufferMode(bool bActive)
{
	if (m_bExtensionsLoaded) {
		if (bActive) {
			if (m_caps & GLEXT_SUPPORT_PBO) {
				m_bPBOavailable = true;
			}
		}
		else {
			m_bPBOavailable = false;
		}
	}
	else {
		m_bPBOavailable = false;
	}
}

//---------------------------------------------------------
// Function: GetBuffers
// Get user number of pixel buffers
int spoutGL::GetBuffers()
{
	return m_nBuffers;
}

//---------------------------------------------------------
// Function: SetBuffers
// Set application number of pixel buffers
void spoutGL::SetBuffers(int nBuffers)
{
	m_nBuffers = nBuffers;
}

//---------------------------------------------------------
// Function: GetMaxSenders
// Get user Maximum senders allowed
int spoutGL::GetMaxSenders()
{
	return sendernames.GetMaxSenders();
}

//---------------------------------------------------------
// Function: SetMaxSenders
// Set user Maximum senders allowed
void spoutGL::SetMaxSenders(int maxSenders)
{
	// Setting must be global for all applications
	sendernames.SetMaxSenders(maxSenders);
}

//
// Group: Retained for 2.006 compatibility
//

//---------------------------------------------------------
// Function: GetDX9
// Get user DX9 mode
bool spoutGL::GetDX9()
{
	DWORD dwDX9 = 0;
	ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "DX9", &dwDX9);
	return (dwDX9 == 1);
}

//---------------------------------------------------------
// Function: SetDX9
// Set user DX9 mode
bool spoutGL::SetDX9(bool bDX9)
{
	return WriteDwordToRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "DX9", (DWORD)bDX9);
}

//---------------------------------------------------------
// Function: GetMemoryShareMode
// Get user memory share mode
bool spoutGL::GetMemoryShareMode()
{
	DWORD dwMem = 0;
	ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "MemoryShare", &dwMem);
	return (dwMem == 1);
}

//---------------------------------------------------------
// Function: SetMemoryShareMode
// Set user memory share mode
bool spoutGL::SetMemoryShareMode(bool bMem)
{
	return WriteDwordToRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "MemoryShare", (DWORD)bMem);
}

//---------------------------------------------------------
// Function: GetCPUmode
// Get user CPU mode
bool spoutGL::GetCPUmode()
{
	DWORD dwCpu = 0;
	ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "CPU", &dwCpu);
	return (dwCpu == 1);
}

//---------------------------------------------------------
// Function: SetCPUmode
// Set user CPU mode
bool spoutGL::SetCPUmode(bool bCPU)
{
	return WriteDwordToRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "CPU", (DWORD)bCPU);
}

// Function: GetShareMode
// Get user share mode
//  0 - texture, 1 - memory, 2 - CPU
int spoutGL::GetShareMode()
{
	DWORD dwMem = 0;
	DWORD dwCPU = 0;
	ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "MemoryShare", &dwMem);
	ReadDwordFromRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "CPU", &dwCPU);

	if (dwCPU > 0) {
		return 2;
	}
	if (dwMem > 0) {
		return 1;
	}

	// 0 : Texture share default
	return 0;

}

//---------------------------------------------------------
// Function: SetShareMode
// Set user share mode
//  0 - texture, 1 - memory, 2 - CPU
void spoutGL::SetShareMode(int mode)
{
	switch (mode) {

	case 1: // Memory
		WriteDwordToRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "MemoryShare", 1);
		WriteDwordToRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "CPU", 0);
		break;
	case 2: // CPU
		WriteDwordToRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "MemoryShare", 0);
		WriteDwordToRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "CPU", 1);
		break;
	default: // 0 - Texture
		WriteDwordToRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "MemoryShare", 0);
		WriteDwordToRegistry(HKEY_CURRENT_USER, "Software\\Leading Edge\\Spout", "CPU", 0);
		break;
	}
}

//
// Group: Information
//

//---------------------------------------------------------
// Function: GetHostPath
// The path of the host that produced the sender
//
// Retrieved from the description string in the sender info memory map
bool spoutGL::GetHostPath(const char* sendername, char* hostpath, int maxchars)
{
	SharedTextureInfo info;
	int n;

	if (!sendernames.getSharedInfo(sendername, &info)) {
		// Just quit if the key does not exist
		SpoutLogWarning("spoutGL::GetHostPath - could not get sender info [%s]", sendername);
		return false;
	}

	n = maxchars;
	if (n > 256) n = 256; // maximum field width in shared memory (128 wide chars)
	strcpy_s(hostpath, n, (char*)info.description);

	return true;
}

//---------------------------------------------------------
// Function: GetVerticalSync
// Vertical sync status
int spoutGL::GetVerticalSync()
{
	// Needs OpenGL context
	if (wglGetCurrentContext()) {
		// needed for both sender and receiver
		if (m_bSWAPavailable) {
			return(wglGetSwapIntervalEXT());
		}
	}
	return 0;
}

//---------------------------------------------------------
// Function: SetVerticalSync
// Lock to monitor vertical sync
bool spoutGL::SetVerticalSync(bool bSync)
{
	// wglSwapIntervalEXT specifies the minimum number
	// of video frame periods per buffer swap
	if (wglGetCurrentContext()) {
		if (m_bSWAPavailable) {
			if (bSync) {
				wglSwapIntervalEXT(1); // lock to monitor vsync
			}
			else {
				// buffer swaps are not synchronized to a video frame.
				wglSwapIntervalEXT(0); // unlock from monitor vsync
			}
			return true;
		}
	}

	return false;
}

//---------------------------------------------------------
// Function: GetSpoutVersion
// Get Spout version
int spoutGL::GetSpoutVersion()
{
	// Version number is retrieved from the registry at class initialization
	// Integer number 2005, 2006, 2007 etc.
	// 0 for earlier than 2.005
	// Set by the Spout installer for 2.005/2.006, or by SpoutSettings
	return m_SpoutVersion;
}

//
// Group: Utilities
//

//---------------------------------------------------------
// Function: CopyTexture
// Copy OpenGL texture with optional invert
//   Textures must be the same size
bool spoutGL::CopyTexture(GLuint SourceID, GLuint SourceTarget,
	GLuint DestID, GLuint DestTarget, unsigned int width, unsigned int height,
	bool bInvert, GLuint HostFBO)
{
	GLenum status;

	// Create an fbo if not already
	if (m_fbo == 0)
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
	if (status == GL_FRAMEBUFFER_COMPLETE_EXT) {

		if (m_bBLITavailable) {
			if (bInvert) {
				// Blit method with checks - 0.75 - 0.85 msec
				// copy one texture buffer to the other while flipping upside down 
				// (OpenGL and DirectX have different texture origins)
				glBlitFramebufferEXT(0, 0, // srcX0, srcY0, 
					width, height,         // srcX1, srcY1
					0, height,             // dstX0, dstY0,
					width, 0,              // dstX1, dstY1,
					GL_COLOR_BUFFER_BIT, GL_NEAREST);
			}
			else {
				// Do not flip during blit
				glBlitFramebufferEXT(0, 0, // srcX0, srcY0, 
					width, height,         // srcX1, srcY1
					0, 0,                  // dstX0, dstY0,
					width, height,         // dstX1, dstY1,
					GL_COLOR_BUFFER_BIT, GL_NEAREST);
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
		glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);
		return false;
	}

	// Restore default draw
	glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
	// restore the previous fbo - default is 0
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, HostFBO);

	return true;

} // end CopyTexture

//---------------------------------------------------------
// Function: RemovePadding
// Remove line padding from a source image and crerate a destination image without padding
void spoutGL::RemovePadding(const unsigned char *source, unsigned char *dest,
	unsigned int width, unsigned int height, unsigned int stride, GLenum glFormat)
{
	spoutcopy.RemovePadding(source, dest, width, height, stride, glFormat);
}


//
// Group : DX11 texture copy versions
//
// - https://github.com/DashW/Spout2
//

//---------------------------------------------------------
// Function: ReadTexture
// Copy the sender DirectX shared texture
bool spoutGL::ReadTexture(ID3D11Texture2D** texture)
{
	// Only for DX11 mode
	if (!texture || !*texture || !spoutdx.GetDX11Context()) {
		return false;
	}

	D3D11_TEXTURE2D_DESC desc = { 0 };
	(*texture)->GetDesc(&desc);
	if (desc.Width != m_Width || desc.Height != m_Height) {
		return false;
	}
	if (frame.CheckTextureAccess(m_pSharedTexture)) {
		// Copy the shared texture if the sender has produced a new frame
		if (frame.GetNewFrame()) {
			spoutdx.GetDX11Context()->CopyResource(*texture, m_pSharedTexture);
		}
		// Release mutex and allow access to the texture
		frame.AllowTextureAccess(m_pSharedTexture);
	}

	return true;

} // end ReadTexture

//---------------------------------------------------------
// Function: WriteTexture
// Copy to the sender DirectX shared texture
bool spoutGL::WriteTexture(ID3D11Texture2D** texture)
{
	// Only for DX11 mode
	if (!texture || !spoutdx.GetDX11Context()) {
		SpoutLogWarning("spoutGL::WriteTexture(ID3D11Texture2D** texture) failed");
		if (!texture)
			SpoutLogWarning("    ID3D11Texture2D** NULL");
		if (!spoutdx.GetDX11Context())
			SpoutLogVerbose("    pImmediateContext NULL");
		return false;
	}

	bool bRet = false;
	D3D11_TEXTURE2D_DESC desc = { 0 };

	(*texture)->GetDesc(&desc);
	if (desc.Width != m_Width || desc.Height != m_Height) {
		SpoutLogWarning("spoutGL::WriteTexture(ID3D11Texture2D** texture) sizes do not match");
		SpoutLogWarning("    texture (%dx%d) : sender (%dx%d)", desc.Width, desc.Height, m_Width, m_Height);
		return false;
	}

	// Wait for access to the shared texture
	if (frame.CheckTextureAccess(m_pSharedTexture)) {
		spoutdx.GetDX11Context()->CopyResource(m_pSharedTexture, *texture);
		// Flush after update of the shared texture on this device
		spoutdx.GetDX11Context()->Flush();
		// Increment the sender frame counter
		frame.SetNewFrame();
		// Release mutex and allow access to the texture
		frame.AllowTextureAccess(m_pSharedTexture);
		bRet = true;
	}

	return bRet;
}


//---------------------------------------------------------
bool spoutGL::WriteTextureReadback(ID3D11Texture2D** texture,
	GLuint TextureID, GLuint TextureTarget,
	unsigned int width, unsigned int height,
	bool bInvert, GLuint HostFBO)
{
	// Only for DX11 mode
	if (!texture || !spoutdx.GetDX11Context()) {
		SpoutLogWarning("spoutGL::WriteTextureReadback(ID3D11Texture2D** texture) failed");
		if (!texture)
			SpoutLogWarning("    ID3D11Texture2D** NULL");
		if (!spoutdx.GetDX11Context())
			SpoutLogVerbose("    pImmediateContext NULL");
		return false;
	}

	if (!m_hInteropDevice || !m_hInteropObject) {
		SpoutLogWarning("spoutGL::WriteTextureReadback() no GL/DX interop\n	m_hInteropObject = 0x%7.7X - m_hInteropDevice = 0x%7.7X", PtrToUint(m_hInteropDevice), PtrToUint(m_hInteropObject));
		return false;
	}

	bool bRet = false;
	D3D11_TEXTURE2D_DESC desc = { 0 };

	(*texture)->GetDesc(&desc);
	if (desc.Width != m_Width || desc.Height != m_Height) {
		SpoutLogWarning("spoutGL::WriteTextureReadback(ID3D11Texture2D** texture) sizes do not match");
		SpoutLogWarning("    texture (%dx%d) : sender (%dx%d)", desc.Width, desc.Height, m_Width, m_Height);
		return false;
	}

	// Wait for access to the shared texture
	if (frame.CheckTextureAccess(m_pSharedTexture)) {
		bRet = true;
		// Copy the DirectX texture to the shared texture
		spoutdx.GetDX11Context()->CopyResource(m_pSharedTexture, *texture);
		// Flush after update of the shared texture on this device
		spoutdx.GetDX11Context()->Flush();
		// Copy the linked OpenGL texture back to the user texture
		if (width != m_Width || height != m_Height) {
			SpoutLogWarning("spoutGL::WriteTextureReadback(ID3D11Texture2D** texture) sizes do not match");
			SpoutLogWarning("    OpenGL texture (%dx%d) : sender (%dx%d)", desc.Width, desc.Height, m_Width, m_Height);
			bRet = false;
		}
		else if (LockInteropObject(m_hInteropDevice, &m_hInteropObject) == S_OK) {
			bRet = GetSharedTextureData(TextureID, TextureTarget, width, height, bInvert, HostFBO);
			UnlockInteropObject(m_hInteropDevice, &m_hInteropObject);
			if (!bRet)
				SpoutLogWarning("spoutGL::WriteTextureReadback(ID3D11Texture2D** texture) readback failed");
		}

		// Increment the sender frame counter
		frame.SetNewFrame();
		// Release mutex and allow access to the texture
		frame.AllowTextureAccess(m_pSharedTexture);
	}

	return bRet;
}

