/*

	spoutDirectX.h

	Functions to manage DirectX 11 texture sharing

	Copyright (c) 2014 - 2022, Lynn Jarvis. All rights reserved.

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
#include <d3d11.h>

#pragma comment (lib, "d3d11.lib")// the Direct3D 11 Library file
#pragma comment (lib, "DXGI.lib") // for CreateDXGIFactory1

using namespace spoututils;

class SPOUT_DLLEXP spoutDirectX {

	public:

		spoutDirectX();
		~spoutDirectX();
		
		// Initialize and prepare DirectX 11
		bool OpenDirectX11(ID3D11Device* pDevice = nullptr);
		// Release DirectX 11 device and context
		void CloseDirectX11();
		// Set the DirectX11 device
		bool SetDX11Device(ID3D11Device* pDevice);
		// Create a DirectX11 device
		ID3D11Device* CreateDX11device();
		// Create a DirectX11 shared texture
		bool CreateSharedDX11Texture(ID3D11Device* pDevice, unsigned int width, unsigned int height, DXGI_FORMAT format, ID3D11Texture2D** ppSharedTexture, HANDLE &dxShareHandle);
		// Create a DirectX11 texture
		bool CreateDX11Texture(ID3D11Device* pDevice, unsigned int width, unsigned int height, DXGI_FORMAT format, ID3D11Texture2D** ppTexture);
		// Create a DirectX11 staging texture
		bool CreateDX11StagingTexture(ID3D11Device* pDevice, unsigned int width, unsigned int height, DXGI_FORMAT format, ID3D11Texture2D** pStagingTexture);
		// Return the pointer of a DirectX11 shared texture
		bool OpenDX11shareHandle(ID3D11Device* pDevice, ID3D11Texture2D** ppSharedTexture, HANDLE dxShareHandle);

		//
		// Output graphics adapter
		//

		// Get the number of graphics adapters in the system
		int GetNumAdapters();
		// Get an adapter name
		bool GetAdapterName(int index, char *adaptername, int maxchars);
		// Get the current adapter index
		int GetAdapter();
		// Set graphics adapter for CreateDX11device from an index
		bool SetAdapter(int index = -1); 
		// Get the current adapter description
		bool GetAdapterInfo(char *renderdescription, char *displaydescription, int maxchars);
		// Get adapter pointer for a given adapter (-1 means current)
		IDXGIAdapter* GetAdapterPointer(int index = -1);
		// Set required graphics adapter for CreateDX11device
		void SetAdapterPointer(IDXGIAdapter* pAdapter);
		// Find the index of the NVIDIA adapter in a multi-adapter system
		bool FindNVIDIA(int &nAdapter);

		//
		// DirectX11 utiities
		//

		// Release a texture resource created with a class device1`
		unsigned long ReleaseDX11Texture(ID3D11Texture2D* pTexture);
		// Release a texture resource
		unsigned long ReleaseDX11Texture(ID3D11Device* pd3dDevice, ID3D11Texture2D* pTexture);
		// Release a device
		unsigned long ReleaseDX11Device(ID3D11Device* pd3dDevice);
		// Return the class device
		ID3D11Device* GetDX11Device();

		// Return the device immediate context
		ID3D11DeviceContext* GetDX11Context();
		// Flush immediate context command queue and wait for copleteion
		void FlushWait(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pImmediateContext);
		// Wait for completion after flush
		void Wait(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pImmediateContext);

	protected:

		void DebugLog(ID3D11Device* pd3dDevice, const char* format, ...);
		int						m_AdapterIndex; // Adapter index
		IDXGIAdapter*			m_pAdapterDX11; // Adapter pointer
		ID3D11Device*           m_pd3dDevice;   // DX11 device
		ID3D11DeviceContext*	m_pImmediateContext;
		bool					m_bClassDevice;
		D3D_DRIVER_TYPE			m_driverType;
		D3D_FEATURE_LEVEL		m_featureLevel;

};

#endif
