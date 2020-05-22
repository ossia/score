/*

	spoutDirectX.h

	DirectX functions to manage DirectX 11 texture sharing


		Copyright (c) 2014 - 2017, Lynn Jarvis. All rights reserved.

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
#ifndef __spoutDirectX__ 
#define __spoutDirectX__

#include "SpoutCommon.h"
#include <windowsx.h>
#include <d3d9.h>
#include <d3d11.h>
// #include <DXGI.h> // LJ DEBUG
#include <string>
#include <iostream>

#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "DXGI.lib")

using namespace std;

class SPOUT_DLLEXP spoutDirectX {

	public:

		spoutDirectX();
		~spoutDirectX();

		// DX9
		IDirect3D9Ex* CreateDX9object(); // Create a DirectX9 object
		IDirect3DDevice9Ex* CreateDX9device(IDirect3D9Ex* pD3D, HWND hWnd);	// Create a DirectX9 device
		bool CreateSharedDX9Texture(IDirect3DDevice9Ex* pDevice, unsigned int width, unsigned int height, D3DFORMAT format, LPDIRECT3DTEXTURE9 &dxTexture, HANDLE &dxShareHandle);
		bool WriteDX9surface(IDirect3DDevice9Ex* pDevice, LPDIRECT3DTEXTURE9 dxTexture, LPDIRECT3DSURFACE9 source_surface);

		// DX11
		ID3D11Device* CreateDX11device(); // Create a DX11 device
		ID3D11DeviceContext* GetImmediateContext();
		bool CreateSharedDX11Texture(ID3D11Device* pDevice, unsigned int width, unsigned int height, DXGI_FORMAT format, ID3D11Texture2D** pSharedTexture, HANDLE &dxShareHandle);
		bool CreateDX11StagingTexture(ID3D11Device* pDevice, unsigned int width, unsigned int height, DXGI_FORMAT format, ID3D11Texture2D** pStagingTexture);
		bool OpenDX11shareHandle(ID3D11Device* pDevice, ID3D11Texture2D** ppSharedTexture, HANDLE dxShareHandle);

		// Output adapter selection
		int GetNumAdapters(); // Get the number of graphics adapters in the system
		bool GetAdapterName(int index, char *adaptername, int maxchars); // Get an adapter name
		bool SetAdapter(int index); // Set required graphics adapter for output
		int GetAdapter(); // Get the current adapter index
		bool GetAdapterInfo(char *renderdescription, char *displaydescription, int maxchars);
		bool FindNVIDIA(int &nAdapter); // Find the index of the NVIDIA adapter in a multi-adapter system

		// Registry read/write - 20.11.15 - moved from interop class
		bool ReadDwordFromRegistry(DWORD *pValue, const char *subkey, const char *valuename);
		bool WriteDwordToRegistry(DWORD dwValue, const char *subkey, const char *valuename);

		// Mutex locks for shared texture access
		bool CreateAccessMutex(const char *name, HANDLE &hAccessMutex);
		void CloseAccessMutex(HANDLE &hAccessMutex);
		bool CheckAccess(HANDLE hAccessMutex);
		void AllowAccess(HANDLE hAccessMutex);

		// For debugging only - to toggle texture access locks disable/enable
		bool bUseAccessLocks;



	protected:

		IDXGIAdapter* GetAdapterPointer(int index); // Get adapter pointer for DirectX 11
		int						g_AdapterIndex; // Used for DX9
		IDXGIAdapter*			g_pAdapterDX11;
		ID3D11DeviceContext*	g_pImmediateContext;
		D3D_DRIVER_TYPE			g_driverType;
		D3D_FEATURE_LEVEL		g_featureLevel;

};

#endif
