//
//
//			spoutDirectX.cpp
//
//		DirectX functions to manage DirectX9 and DirectX11 texture sharing
//
// ====================================================================================
//		Revisions :
//
//		22.07.14	- added option for DX9 or DX11
//		21.09.14	- included keyed mutex texture access lock in CreateSharedDX11Texture
//		23.09.14	- moved general mutex texture access lock to this class
//		23.09.14	- added DX11available() to verify operating system support for DirectX 11
//		15.10.14	- added debugging aid for texture access locks
//		17.10.14	- flush before release immediate context in CloseDX11
//		21.10.14	- removed keyed mutex lock due to reported driver problems
//					  TODO - cleanup all functions using it
//		10.02.15	- removed functions relating to DirectX 11 keyed mutex lock
//		14.02.15	- added UNREFERENCED_PARAMETER(pSharedTexture) to CheckAceess and AllowAccess
//		29.05.15	- Included SetAdapter for multiple adapters - Franz Hildgen.
//		02.06.15	- Added GetAdapter, GetNumAdapters, GetAdapterName
//		08.06.15	- removed dx9 flag from setadapter
//		04.08.15	- cleanup
//		11.08.15	- removed GetAdapterName return if Intel. For use with Intel HD4400/5000 graphics
//		22.10.15	- removed DX11available and shifted it to the interop class
//		19.11.15	- fixed return value in CreateDX9device after caps error (was false instead of NULL)
//		20.11.15	- Registry read/write moved from SpoutGLDXinterop class
//		16.02.16	- IDXGIFactory release - from https://github.com/jossgray/Spout2
//		29.02.16	- cleanup
//		05.04.16	- removed unused texture pointer from mutex access functions
//		16.06.16	- fixed null device release in SetAdapter - https://github.com/leadedge/Spout2/issues/17
//		01.07.16	- restored hFocusWindow in CreateDX9device (was set to NULL for testing)
//		04.09.16	- Add create DX11 staging texture
//		16.01.17	- Add WriteDX9surface
//		23.01.17	- pEventQuery->Release() for writeDX9surface
//		24.04.17	- Add MessageBox error warnings in CreateSharedDX11Texture
//		11.11.18	- Add GetImmediateContext()
//
// ====================================================================================
/*

		Copyright (c) 2014-2018. Lynn Jarvis. All rights reserved.

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

#include "SpoutDirectX.h"

spoutDirectX::spoutDirectX() {

	// DX11
	g_pImmediateContext = NULL;
	g_driverType		= D3D_DRIVER_TYPE_NULL;
	g_featureLevel		= D3D_FEATURE_LEVEL_11_0;

	// For debugging only - to toggle texture access locks disable/enable
	bUseAccessLocks     = true; // use texture access locks by default

	// Output graphics adapter
	// Programmer can set for an application
	g_AdapterIndex  = D3DADAPTER_DEFAULT; // DX9
	g_pAdapterDX11  = nullptr; // DX11


}

spoutDirectX::~spoutDirectX() {

}

//
// =========================== DX9 ================================
//

// Create a DX9 object
IDirect3D9Ex* spoutDirectX::CreateDX9object()
{
	HRESULT res;
	IDirect3D9Ex* pD3D;

	// MessageBoxA(NULL, "CreateDX9object", "Spout", MB_OK);
	res = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D);
	if ( res != D3D_OK ) return NULL;

	return pD3D;
}

// Create a DX9 device
IDirect3DDevice9Ex* spoutDirectX::CreateDX9device(IDirect3D9Ex* pD3D, HWND hWnd)
{
	HRESULT res;
	IDirect3DDevice9Ex* pDevice;
    D3DPRESENT_PARAMETERS d3dpp;
	D3DCAPS9 d3dCaps;
	int AdapterIndex = g_AdapterIndex;

	// printf("CreateDX9device : g_AdapterIndex = %d\n", g_AdapterIndex);

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed		= TRUE;						// windowed and not full screen
    d3dpp.SwapEffect	= D3DSWAPEFFECT_DISCARD;	// discard old frames
    d3dpp.hDeviceWindow	= hWnd;						// set the window to be used by D3D

	// D3DFMT_UNKNOWN can be specified for the BackBufferFormat while in windowed mode. 
	// This tells the runtime to use the current display-mode format and eliminates
	// the need to call GetDisplayMode. 
	d3dpp.BackBufferFormat		 = D3DFMT_UNKNOWN;

	// Set a dummy resolution - we don't render anything
    d3dpp.BackBufferWidth		 = 1920;
    d3dpp.BackBufferHeight		 = 1080;
	d3dpp.EnableAutoDepthStencil = FALSE;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.BackBufferCount		 = 1;

	// Test for hardware vertex processing capability and set up as needed
	// D3DCREATE_MULTITHREADED required by interop spec
	if(pD3D->GetDeviceCaps( AdapterIndex, D3DDEVTYPE_HAL, &d3dCaps) != S_OK ) {
		printf("SpoutDirectX::CreateDX9device - GetDeviceCaps error\n");
		return NULL;
	}

	// | D3DCREATE_NOWINDOWCHANGES
	DWORD dwBehaviorFlags = D3DCREATE_PUREDEVICE | D3DCREATE_MULTITHREADED; 
	if ( d3dCaps.VertexProcessingCaps != 0 )
		dwBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	else
		dwBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	// Create a DirectX9 device - we use directx only for accessing the handle
	// LJ notes - hwnd seems to have no effect - maybe because we do not render anything.
	// Note here that we are setting up for Windowed mode but it seems not to be affected
	// by fullscreen, probably because we are not rendering to it.
    res = pD3D->CreateDeviceEx(	AdapterIndex, // D3DADAPTER_DEFAULT
								D3DDEVTYPE_HAL, // Hardware rasterization. 
								hWnd,			// hFocusWindow (can be NULL)
								dwBehaviorFlags,
								&d3dpp,			// d3dpp.hDeviceWindow should be valid if hFocusWindow is NULL
								NULL,			// pFullscreenDisplayMode must be NULL for windowed mode
								&pDevice);
	
	if ( res != D3D_OK ) {
		printf("SpoutDirectX::CreateDX9device - CreateDeviceEx returned error %d (%x)\n", res, res);
		return NULL;
	}

	return pDevice;

} // end CreateDX9device


// Create a shared DirectX9 texture
// by giving it a sharehandle variable - dxShareHandle
// For a SENDER : the sharehanlde is NULL and a new texture is created
// For a RECEIVER : the sharehandle is valid and a handle to the existing shared texture is created
bool spoutDirectX::CreateSharedDX9Texture(IDirect3DDevice9Ex* pDevice, unsigned int width, unsigned int height, D3DFORMAT format, LPDIRECT3DTEXTURE9 &dxTexture, HANDLE &dxShareHandle)
{
	if(dxTexture != NULL) dxTexture->Release();

	HRESULT res = pDevice->CreateTexture(width,
										 height,
										 1,
										 D3DUSAGE_RENDERTARGET, 
										 format,	// default is D3DFMT_A8R8G8B8 - may be set externally
										 D3DPOOL_DEFAULT,	// Required by interop spec
										 &dxTexture,
										 &dxShareHandle);	// local share handle to allow type casting for 64bit

	// USAGE may also be D3DUSAGE_DYNAMIC and pay attention to format and resolution!!!
	// USAGE, format and size for sender and receiver must all match
	if ( res != D3D_OK ) {
		printf("SpoutDirectX::CreateSharedDX9Texture error : ");
		switch (res) {
			case D3DERR_INVALIDCALL:
				printf("    D3DERR_INVALIDCALL \n");
				break;
			case D3DERR_OUTOFVIDEOMEMORY:
				printf("    D3DERR_OUTOFVIDEOMEMORY \n");
				break;
			case E_OUTOFMEMORY:
				printf("    E_OUTOFMEMORY \n");
				break;
			default :
				printf("    Unknown error\n");
				break;
		}
		return false;
	}

	return true;

} // end CreateSharedDX9Texture


bool spoutDirectX::WriteDX9surface(IDirect3DDevice9Ex* pDevice, LPDIRECT3DTEXTURE9 dxTexture, LPDIRECT3DSURFACE9 source_surface)
{
	IDirect3DSurface9* texture_surface = NULL;
	IDirect3DQuery9* pEventQuery=NULL;
	HRESULT hr = 0;
	hr = dxTexture->GetSurfaceLevel(0, &texture_surface); // shared texture surface
	if(SUCCEEDED(hr)) {
		// UpdateSurface
		// https://msdn.microsoft.com/en-us/library/windows/desktop/bb205857%28v=vs.85%29.aspx
		//    The source surface must have been created with D3DPOOL_SYSTEMMEM.
		//    The destination surface must have been created with D3DPOOL_DEFAULT.
		//    Neither surface can be locked or holding an outstanding device context.
		hr = pDevice->UpdateSurface(source_surface, NULL, texture_surface, NULL);
		if(SUCCEEDED(hr)) {
			// It is necessary to flush the command queue 
			// or the data is not ready for the receiver to read.
			// Adapted from : https://msdn.microsoft.com/en-us/library/windows/desktop/bb172234%28v=vs.85%29.aspx
			// Also see : http://www.ogre3d.org/forums/viewtopic.php?f=5&t=50486
			pDevice->CreateQuery(D3DQUERYTYPE_EVENT, &pEventQuery) ;
			if(pEventQuery!=NULL) {
				pEventQuery->Issue(D3DISSUE_END) ;
				while(S_FALSE == pEventQuery->GetData(NULL, 0, D3DGETDATA_FLUSH)) ;
				pEventQuery->Release(); // Must be released or causes a leak and reference count increment
			}
			return true;
		}
	}
	return false;
} // end WriteDX9surface

// =========================== end DX9 =============================


//
// =========================== DX11 ================================
//

//
// Notes for DX11 : https://www.opengl.org/registry/specs/NV/DX_interop2.txt
//
// Valid device types for the <dxDevice> parameter of wglDXOpenDeviceNV and associated restrictions
// DirectX device type : ID3D11Device - can only be used on WDDM operating systems; XXX Must be multithreaded
// TEXTURE_2D - ID3D11Texture2D - Usage flags must be D3D11_USAGE_DEFAULT
// wglDXSetResourceShareHandle does not need to be called for DirectX
// version 10 and 11 resources. Calling this function for DirectX 10
// and 11 resources is not an error but has no effect.

// Create DX11 device
ID3D11Device* spoutDirectX::CreateDX11device()
{
	ID3D11Device* pd3dDevice = NULL;
	HRESULT hr = S_OK;
	UINT createDeviceFlags = 0;
	IDXGIAdapter* pAdapterDX11 = g_pAdapterDX11;
	// printf("CreateDX11device : g_AdapterIndex = %d, pAdapterDX11 = [%x]\n", g_AdapterIndex, g_pAdapterDX11);

	// GL/DX interop Spec
	// ID3D11Device can only be used on WDDM operating systems : Must be multithreaded
	// D3D11_CREATE_DEVICE_FLAG createDeviceFlags
	D3D_DRIVER_TYPE driverTypes[] =	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE,
	};

	UINT numDriverTypes = ARRAYSIZE( driverTypes );

	// These are the feature levels that we will accept.
	// g_featureLevel is the feature level used
	// 11.0 = 0xb000
	// 11.1 = 0xb001
	// TODO - check for 11.1 and multiple passes if feature level fails
	D3D_FEATURE_LEVEL featureLevels[] =	{
		// D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	UINT numFeatureLevels = ARRAYSIZE( featureLevels );

	// To allow for multiple graphics cards we will use g_pAdapterDX11
	// Which is set by SetAdapter before initializing DirectX
	// printf("CreateDX11device : pAdapterDX11 = %x\n", pAdapterDX11);
	if(pAdapterDX11) {
			hr = D3D11CreateDevice( pAdapterDX11,
									D3D_DRIVER_TYPE_UNKNOWN,
									NULL,
									createDeviceFlags,
									featureLevels,
									numFeatureLevels,
									D3D11_SDK_VERSION,
									&pd3dDevice,
									&g_featureLevel,
									&g_pImmediateContext );
	} // endif adapter set
	else {
		
		// Possible Optimus problem : is the default adapter (NULL) always Intel ?
		// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476082%28v=vs.85%29.aspx
		// pAdapter : a pointer to the video adapter to use when creating a device. 
		// Pass NULL to use the default adapter, which is the first adapter that is
		// enumerated by IDXGIFactory1::EnumAdapters. 
		// http://www.gamedev.net/topic/645920-d3d11createdevice-returns-wrong-feature-level/

		for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ ) {

			g_driverType = driverTypes[driverTypeIndex];

			hr = D3D11CreateDevice(	NULL,
									g_driverType,
									NULL,
									createDeviceFlags,
									featureLevels,
									numFeatureLevels,
									D3D11_SDK_VERSION, 
									&pd3dDevice,
									&g_featureLevel,
									&g_pImmediateContext);

			// Break as soon as something passes
			if(SUCCEEDED(hr))
				break;
		}
	} // endif no adapter set
	
	// Quit if nothing worked
	if( FAILED(hr))
		return NULL;

	// All OK
	return pd3dDevice;

} // end CreateDX11device

ID3D11DeviceContext* spoutDirectX::GetImmediateContext()
{
	return g_pImmediateContext;
}


bool spoutDirectX::CreateSharedDX11Texture(ID3D11Device* pd3dDevice, 
											unsigned int width, 
											unsigned int height, 
											DXGI_FORMAT format, 
											ID3D11Texture2D** pSharedTexture,
											HANDLE &dxShareHandle)
{
	ID3D11Texture2D* pTexture;
	
	if(pd3dDevice == NULL) {
		MessageBoxA(NULL, "CreateSharedDX11Texture NULL device", "SpoutDirectX", MB_OK);
		return false;
	}

	//
	// Create a new shared DX11 texture
	//

	pTexture = *pSharedTexture; // The texture pointer

	// if(pTexture == NULL) MessageBoxA(NULL, "CreateSharedDX11Texture NULL texture", "SpoutSender", MB_OK);

	// Textures being shared from D3D9 to D3D11 have the following restrictions (LJ - D3D11 to D3D9 ?).
	//		Textures must be 2D
	//		Only 1 mip level is allowed
	//		Texture must have default usage
	//		Texture must be write only	- ?? LJ ??
	//		MSAA textures are not allowed
	//		Bind flags must have SHADER_RESOURCE and RENDER_TARGET set
	//		Only R10G10B10A2_UNORM, R16G16B16A16_FLOAT and R8G8B8A8_UNORM formats are allowed - ?? LJ ??
	//		If a shared texture is updated on one device ID3D11DeviceContext::Flush must be called on that device **

	// http://msdn.microsoft.com/en-us/library/windows/desktop/ff476903%28v=vs.85%29.aspx
	// To share a resource between two Direct3D 11 devices the resource must have been created
	// with the D3D11_RESOURCE_MISC_SHARED flag, if it was created using the ID3D11Device interface.
	//
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width				= width;
	desc.Height				= height;
	desc.BindFlags			= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	desc.MiscFlags			= D3D11_RESOURCE_MISC_SHARED; // This texture will be shared
	// A DirectX 11 texture with D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX is not compatible with DirectX 9
	// so a general named mutex is used for all texture types
	desc.Format				= format;
	desc.Usage				= D3D11_USAGE_DEFAULT;
	// Multisampling quality and count
	// The default sampler mode, with no anti-aliasing, has a count of 1 and a quality level of 0.
	desc.SampleDesc.Quality = 0;
	desc.SampleDesc.Count	= 1;
	desc.MipLevels			= 1;
	desc.ArraySize			= 1;

	HRESULT res = pd3dDevice->CreateTexture2D(&desc, NULL, &pTexture); // pSharedTexture);

	if (res != S_OK) {
		// http://msdn.microsoft.com/en-us/library/windows/desktop/ff476174%28v=vs.85%29.aspx
		// printf("SpoutDirectX::CreateSharedDX11Texture ERROR : [0x%x]\n", res);
		char temp[256];
		sprintf_s(temp, 256, "CreateSharedDX11Texture ERROR : [0x%x]\n", res);

		switch (res) {
			case D3DERR_INVALIDCALL:
				// printf("    D3DERR_INVALIDCALL \n");
				strcat_s(temp, 256, "    The method call is invalid.");
				break;
			case E_INVALIDARG:
				// printf("    E_INVALIDARG \n");
				strcat_s(temp, 256, "    An invalid parameter was passed.");
				break;
			case E_OUTOFMEMORY:
				// printf("    E_OUTOFMEMORY \n");
				strcat_s(temp, 256, "    Direct3D could not allocate sufficient memory.");
				break;
			default :
				// printf("    Unlisted error\n");
				strcat_s(temp, 256, "    Unlisted error");
				break;
		}
		MessageBoxA(NULL, temp, "SpoutDirectX", MB_OK);
		return false;
	}

	// The DX11 texture is created OK
	// Get the texture share handle so it can be saved in shared memory for receivers to pick up
	// When sharing a resource between two Direct3D 10/11 devices the unique handle 
	// of the resource can be obtained by querying the resource for the IDXGIResource 
	// interface and then calling GetSharedHandle.
	IDXGIResource* pOtherResource(NULL);
	if(pTexture->QueryInterface( __uuidof(IDXGIResource), (void**)&pOtherResource) != S_OK) {
		// printf("    QueryInterface error\n");
		MessageBoxA(NULL, "CreateSharedDX11Texture : QueryInterface error", "SpoutDirectX", MB_OK);
		return false;
	}

	// Return the shared texture handle
	pOtherResource->GetSharedHandle(&dxShareHandle); 
	pOtherResource->Release();

	*pSharedTexture = pTexture;

	return true;

}


// Create a DirectX 11 staging texture for read and write
bool spoutDirectX::CreateDX11StagingTexture(ID3D11Device* pd3dDevice, 
											unsigned int width, 
											unsigned int height, 
											DXGI_FORMAT format, 
											ID3D11Texture2D** pStagingTexture)
{
	ID3D11Texture2D* pTexture = NULL;
	if(pd3dDevice == NULL) return false;

	pTexture = *pStagingTexture; // The texture pointer
	if(pTexture) {
		pTexture->Release();
	}

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory( &desc, sizeof(desc) );
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = format;
	desc.SampleDesc.Count = 1;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
    desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;

	HRESULT res = pd3dDevice->CreateTexture2D(&desc, NULL, &pTexture);

	if (res != S_OK) {
		// http://msdn.microsoft.com/en-us/library/windows/desktop/ff476174%28v=vs.85%29.aspx
		printf("CreateTexture2D ERROR : [0x%x]\n", res);
		switch (res) {
			case D3DERR_INVALIDCALL:
				printf("    D3DERR_INVALIDCALL \n");
				break;
			case E_INVALIDARG:
				printf("    E_INVALIDARG \n");
				break;
			case E_OUTOFMEMORY:
				printf("    E_OUTOFMEMORY \n");
				break;
			default :
				printf("    Unlisted error\n");
				break;
		}
		MessageBoxA(NULL, "CreateTexture2D ERROR", "info", MB_OK);
		return false;
	}

	*pStagingTexture = pTexture;

	return true;

}

bool spoutDirectX::OpenDX11shareHandle(ID3D11Device* pDevice, ID3D11Texture2D** ppSharedTexture, HANDLE dxShareHandle)
{
	HRESULT hr;

	// To share a resource between a Direct3D 9 device and a Direct3D 11 device 
	// the texture must have been created using the pSharedHandle argument of CreateTexture.
	// The shared Direct3D 9 handle is then passed to OpenSharedResource in the hResource argument.
	// printf("OpenDX11shareHandle - pDevice [%x] %x, %x\n", pDevice, dxShareHandle, ppSharedTexture);
	hr = pDevice->OpenSharedResource(dxShareHandle, __uuidof(ID3D11Resource), (void**)(ppSharedTexture));
	if(hr != S_OK) {
		return false;
	}
	
	// Can get sender format here
	/*
	ID3D11Texture2D * texturePointer = *ppSharedTexture;
	D3D11_TEXTURE2D_DESC td;
	texturePointer->GetDesc(&td);
	printf("td.Format = %d\n", td.Format); // 87
	printf("td.Width = %d\n", td.Width);
	printf("td.Height = %d\n", td.Height);
	printf("td.MipLevels = %d\n", td.MipLevels);
	printf("td.Usage = %d\n", td.Usage);
	printf("td.ArraySize = %d\n", td.ArraySize);
	printf("td.SampleDesc = %d\n", td.SampleDesc);
	printf("td.BindFlags = %d\n", td.BindFlags);
	printf("td.MiscFlags = %d\n", td.MiscFlags); // D3D11_RESOURCE_MISC_SHARED
	*/

	return true;

}

// =================================================================
// Texture access mutex locks
//
// A general mutex lock
//
// =================================================================
bool spoutDirectX::CreateAccessMutex(const char *name, HANDLE &hAccessMutex)
{
	DWORD errnum;
	char szMutexName[300]; // name of the mutex

	// Create the mutex name to control access to the shared texture
	sprintf_s((char*)szMutexName, 300, "%s_SpoutAccessMutex", name);

	// Create or open mutex depending, on whether it already exists or not
    hAccessMutex = CreateMutexA ( NULL,   // default security
						  FALSE,  // No initial owner
						  (LPCSTR)szMutexName);

	if (hAccessMutex == NULL) {
        return false;
	}
	else {
		errnum = GetLastError();
		if(errnum == ERROR_INVALID_HANDLE) {
			printf("access mutex [%s] invalid handle\n", szMutexName);
		}
	}

	return true;

}

void spoutDirectX::CloseAccessMutex(HANDLE &hAccessMutex)
{
	if(hAccessMutex) CloseHandle(hAccessMutex);
	hAccessMutex = NULL; // makes sure the passed handle is set to NULL
}


//
// Checks whether any other process is holding the lock and waits for access for 4 frames if so.
// For receiving from Version 1 apps with no mutex lock, a reader will have created the mutex and
// will have sole access and rely on the interop locks
bool spoutDirectX::CheckAccess(HANDLE hAccessMutex)
{
	DWORD dwWaitResult;

	// For debugging
	if(!bUseAccessLocks) return true;

	// General mutex lock
	// Don't block if no mutex for Spout1 apps
	if(!hAccessMutex) {
		// printf("No access mutex\n");
		return true; 
	}

	dwWaitResult = WaitForSingleObject(hAccessMutex, 67); // 4 frames at 60fps
	if (dwWaitResult == WAIT_OBJECT_0 ) {
		// The state of the object is signalled.
		return true;
	}
	else {
		switch(dwWaitResult) {
			case WAIT_ABANDONED : // Could return here
				printf("CheckAccess : WAIT_ABANDONED\n");
				break;
			case WAIT_TIMEOUT : // The time-out interval elapsed, and the object's state is nonsignaled.
				printf("CheckAccess : WAIT_TIMEOUT\n");
				break;
			case WAIT_FAILED : // Could use call GetLastError
				printf("CheckAccess : WAIT_FAILED\n");
				break;
			default :
				printf("CheckAccess : unknown error\n");
				break;
		}
	}
	return false;

}


void spoutDirectX::AllowAccess(HANDLE hAccessMutex)
{

	// For debugging
	if(!bUseAccessLocks) return;

	if(hAccessMutex) ReleaseMutex(hAccessMutex);

}


// Set required graphics adapter for output
bool spoutDirectX::SetAdapter(int index)
{
	char adaptername[128];
	IDXGIAdapter* pAdapter = nullptr;

	g_AdapterIndex = D3DADAPTER_DEFAULT; // DX9
	g_pAdapterDX11 = nullptr; // DX11

	// Reset
	if(index == -1) {
		return true;
	}

	// printf("SpoutDirectX::SetAdapter(%d)\n", index);

	// Is the requested adapter available
	if(index > GetNumAdapters()-1) {
		// printf("Index greater than number of adapters\n");
		return false;
	}

	if(!GetAdapterName(index, adaptername, 128)) {
		// printf("Incompatible adapter\n");
		return false;
	}

	// Set the global adapter pointer for DX11
	pAdapter = GetAdapterPointer(index);
	if(pAdapter == nullptr) {
		// printf("Could not get pointer for adapter %d\n", index);
		return false;
	}

	// Set the global adapter pointer for DX11
	g_pAdapterDX11 = pAdapter;
	// Set the global adapter index for DX9
	g_AdapterIndex = index;

	// In case of incompatibility - test everything here

	// 2.005 what is the directX mode ?
	DWORD dwDX9 = 0;
	ReadDwordFromRegistry(&dwDX9, "Software\\Leading Edge\\Spout", "DX9");

	if(dwDX9 == 1) {

		// Try to create a DX9 object and device
		IDirect3D9Ex* pD3D; // DX9 object
		IDirect3DDevice9Ex* pDevice;     // DX9 device
		pD3D = CreateDX9object(); 
		if(pD3D == NULL) {
			// printf("SetAdapter - could not create DX9 object\n");
			g_AdapterIndex = D3DADAPTER_DEFAULT; // DX9
			g_pAdapterDX11 = nullptr; // DX11
			return false;
		}
		pDevice = CreateDX9device(pD3D, NULL); 
		if(pDevice == NULL) {
			// printf("SetAdapter - could not create DX9 device\n");
			pD3D->Release();
			g_AdapterIndex = D3DADAPTER_DEFAULT; // DX9
			g_pAdapterDX11 = nullptr; // DX11
			return false;
		}
		pD3D->Release();
		pDevice->Release();
		// printf("SetAdapter - created DX9 device OK\n");
	}
	else {
		// Try to create a DirectX 11 device
		ID3D11Device* pd3dDevice;
		pd3dDevice = CreateDX11device();
		if(pd3dDevice == NULL) {
			// printf("SetAdapter - could not create DX11 device\n");
			g_AdapterIndex = D3DADAPTER_DEFAULT; // DX9
			g_pAdapterDX11 = nullptr; // DX11
			return false;
		}
		// Close it because not initialized yet and is just a test
		// See : https://github.com/leadedge/Spout2/issues/17
		pd3dDevice->Release();
		// printf("SetAdapter - created DX11 device OK\n");
	}


	return true;

}

// Get the global adapter index
int spoutDirectX::GetAdapter()
{
	return g_AdapterIndex;
}


// FOR DEBUGGING 
bool spoutDirectX::FindNVIDIA(int &nAdapter)
{
	IDXGIFactory1* _dxgi_factory1;
	IDXGIAdapter* adapter1_ptr = nullptr;
	DXGI_ADAPTER_DESC desc;
	// DXGI_OUTPUT_DESC desc_out;
	UINT32 i;
	bool bFound = false;

	if ( FAILED( CreateDXGIFactory1( __uuidof(IDXGIFactory1), (void**)&_dxgi_factory1 ) ) )
		return false;

	for ( i = 0; _dxgi_factory1->EnumAdapters( i, &adapter1_ptr ) != DXGI_ERROR_NOT_FOUND; i++ )	{
		adapter1_ptr->GetDesc( &desc );
		printf( "Adapter(%d) : %S\n", i, desc.Description );
		/*
		IDXGIOutput* p_output = nullptr;
		if(adapter1_ptr->EnumOutputs(0, &p_output ) == DXGI_ERROR_NOT_FOUND) {
			printf("  No outputs\n");
			continue;
		}

		for ( UINT32 j = 0; adapter1_ptr->EnumOutputs( j, &p_output ) != DXGI_ERROR_NOT_FOUND; j++ ) {
			p_output->GetDesc( &desc_out );
			// printf( "  Output : %d\n", j );
			// printf( "    Name %S\n", desc_out.DeviceName );
			// printf( "    Attached to desktop : (%d) %s\n", desc_out.AttachedToDesktop, desc_out.AttachedToDesktop ? "yes" : "no" );
			// printf( "    Rotation : %d\n", desc_out.Rotation );
			// printf( "    Left     : %d\n", desc_out.DesktopCoordinates.left );
			// printf( "    Top      : %d\n", desc_out.DesktopCoordinates.top );
			// printf( "    Right    : %d\n", desc_out.DesktopCoordinates.right );
			// printf( "    Bottom   : %d\n", desc_out.DesktopCoordinates.bottom );
			if( p_output )
				p_output->Release();
		}
		*/
		if(wcsstr(desc.Description, L"NVIDIA")) {
			// printf("Found NVIDIA adapter %d (%S)\n", i, desc.Description);
			bFound = true;
			break;
		}

	}

	_dxgi_factory1->Release();

	if(bFound) {
		printf("Found NVIDIA adapter %d (%S)\n", i, desc.Description);
		nAdapter = i;
		//	0x10DE	NVIDIA
		//	0x163C	intel
		//	0x8086  Intel
		//	0x8087  Intel
		// printf("Vendor    = %d [0x%X]\n", desc.VendorId, desc.VendorId);
		// printf("Revision  = %d [0x%X]\n", desc.Revision, desc.Revision);
		// printf("Device ID = %d [0x%X]\n", desc.DeviceId, desc.DeviceId);
		// printf("SubSys ID = %d [0x%X]\n", desc.SubSysId, desc.SubSysId);

		return true;
	}

	return false;

}



// Get the number of graphics adapters in the system
int spoutDirectX::GetNumAdapters()
{
	IDXGIFactory1* _dxgi_factory1;
	IDXGIAdapter* adapter1_ptr = nullptr;
	UINT32 i;

	// Enum Adapters first : multiple video cards
	if ( FAILED( CreateDXGIFactory1( __uuidof(IDXGIFactory1), (void**)&_dxgi_factory1 ) ) )
		return 0;

	for ( i = 0; _dxgi_factory1->EnumAdapters( i, &adapter1_ptr ) != DXGI_ERROR_NOT_FOUND; i++ )	{
		DXGI_ADAPTER_DESC	desc;
		adapter1_ptr->GetDesc( &desc );
		// printf( "Adapter(%d) : %S\n", i, desc.Description );
		// printf( "  Vendor Id : %d\n", desc.VendorId );
		// printf( "  Dedicated System Memory : %.0f MiB\n", (float)desc.DedicatedSystemMemory / (1024.f * 1024.f) );
		// printf( "  Dedicated Video Memory : %.0f MiB\n", (float)desc.DedicatedVideoMemory / (1024.f * 1024.f) );
		// printf( "  Shared System Memory : %.0f MiB\n", (float)desc.SharedSystemMemory / (1024.f * 1024.f) );
		IDXGIOutput* p_output = nullptr;
		if(adapter1_ptr->EnumOutputs(0, &p_output ) == DXGI_ERROR_NOT_FOUND) {
			printf("  No outputs\n");
		}

		for ( UINT32 j = 0; adapter1_ptr->EnumOutputs( j, &p_output ) != DXGI_ERROR_NOT_FOUND; j++ ) {
			DXGI_OUTPUT_DESC	desc_out;
			p_output->GetDesc( &desc_out );
			// printf( "  Output : %d\n", j );
			// printf( "    Name %S\n", desc_out.DeviceName );
			// printf( "    Attached to desktop : (%d) %s\n", desc_out.AttachedToDesktop, desc_out.AttachedToDesktop ? "yes" : "no" );
			// printf( "    Rotation : %d\n", desc_out.Rotation );
			// printf( "    Left     : %d\n", desc_out.DesktopCoordinates.left );
			// printf( "    Top      : %d\n", desc_out.DesktopCoordinates.top );
			// printf( "    Right    : %d\n", desc_out.DesktopCoordinates.right );
			// printf( "    Bottom   : %d\n", desc_out.DesktopCoordinates.bottom );
			if( p_output )
				p_output->Release();
		}
	}

	_dxgi_factory1->Release();

	return (int)i;

}

// Get an adapter name
bool spoutDirectX::GetAdapterName(int index, char *adaptername, int maxchars)
{
	IDXGIFactory1* _dxgi_factory1;
	IDXGIAdapter* adapter1_ptr = nullptr;
	UINT32 i;

	if ( FAILED( CreateDXGIFactory1( __uuidof(IDXGIFactory1), (void**)&_dxgi_factory1 ) ) )
		return false;
	
	for ( i = 0; _dxgi_factory1->EnumAdapters( i, &adapter1_ptr ) != DXGI_ERROR_NOT_FOUND; i++ ) {
		if((int)i == index) {
			DXGI_ADAPTER_DESC	desc;
			adapter1_ptr->GetDesc( &desc );
			adapter1_ptr->Release();
			size_t charsConverted = 0;
			wcstombs_s(&charsConverted, adaptername, maxchars, desc.Description, maxchars-1);
			// Is the adapter compatible ? TODO
			_dxgi_factory1->Release();
			return true;
		}
	}

	_dxgi_factory1->Release();

	return false;
}


IDXGIAdapter* spoutDirectX::GetAdapterPointer(int index)
{
	// Enum Adapters first : multiple video cards
	IDXGIFactory1*	_dxgi_factory1;
	if ( FAILED( CreateDXGIFactory1( __uuidof(IDXGIFactory1), (void**)&_dxgi_factory1 ) ) )	{
		printf( "    Could not create CreateDXGIFactory1\n" );
		return nullptr;
	}

	IDXGIAdapter* adapter1_ptr = nullptr;
	for ( UINT32 i = 0; _dxgi_factory1->EnumAdapters( i, &adapter1_ptr ) != DXGI_ERROR_NOT_FOUND; i++ )	{
		if ( index == (int)i ) {
			// Now we have the requested adapter, but does it support the required extensions
			_dxgi_factory1->Release();
			return adapter1_ptr;
		}
		adapter1_ptr->Release();
	}
	_dxgi_factory1->Release();

	return nullptr;
}


bool spoutDirectX::GetAdapterInfo(char *adapter, char *display, int maxchars)
{
	IDXGIFactory1* _dxgi_factory1;
	IDXGIAdapter* adapter1_ptr = nullptr;
	UINT32 i;
	size_t charsConverted = 0;
	
	// Enum Adapters first : multiple video cards
	if ( FAILED( CreateDXGIFactory1( __uuidof(IDXGIFactory1), (void**)&_dxgi_factory1 ) ) )
		return false;

	for ( i = 0; _dxgi_factory1->EnumAdapters( i, &adapter1_ptr ) != DXGI_ERROR_NOT_FOUND; i++ )	{

		DXGI_ADAPTER_DESC	desc;
		adapter1_ptr->GetDesc( &desc );

		// Return the current adapter - max of 2 assumed
		wcstombs_s(&charsConverted, adapter, maxchars, desc.Description, maxchars-1);

		IDXGIOutput*	p_output = nullptr;
		for ( UINT32 j = 0; adapter1_ptr->EnumOutputs( j, &p_output ) != DXGI_ERROR_NOT_FOUND; j++ ) {
			DXGI_OUTPUT_DESC	desc_out;
			p_output->GetDesc( &desc_out );
			if(desc_out.AttachedToDesktop)
				wcstombs_s(&charsConverted, display, maxchars, desc.Description, maxchars-1);
			if( p_output )
				p_output->Release();
		}
	}

	_dxgi_factory1->Release();

	return true;
}

// 20.11.15 - moved from interop class
bool spoutDirectX::ReadDwordFromRegistry(DWORD *pValue, const char *subkey, const char *valuename)
{
	HKEY  hRegKey;
	LONG  regres;
	DWORD  dwSize, dwKey;  

	dwSize = MAX_PATH;

	// Does the key exist
	regres = RegOpenKeyExA(HKEY_CURRENT_USER, subkey, NULL, KEY_READ, &hRegKey);
	if(regres == ERROR_SUCCESS) {
		// Read the key DWORD value
		regres = RegQueryValueExA(hRegKey, valuename, NULL, &dwKey, (BYTE*)pValue, &dwSize);
		RegCloseKey(hRegKey);
		if(regres == ERROR_SUCCESS)
			return true;
	}

	// Just quit if the key does not exist
	return false;

}

bool spoutDirectX::WriteDwordToRegistry(DWORD dwValue, const char *subkey, const char *valuename)
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
		// Write the DWORD value
		regres = RegSetValueExA(hRegKey, valuename, 0, REG_DWORD, (BYTE*)&dwValue, 4);
		// For immediate read after write - necessary here because the app might set the values 
		// and read the registry straight away and it might not be available yet
		// The key must have been opened with the KEY_QUERY_VALUE access right (included in KEY_ALL_ACCESS)
		RegFlushKey(hRegKey); // needs an open key
		RegCloseKey(hRegKey); // Done with the key
    }

	if(regres == ERROR_SUCCESS)
		return true;
	else
		return false;

}


