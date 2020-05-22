/*

			spoutGLDXinterop.h

		Functions to manage texture sharing using the NVIDIA GL/DX opengl extensions

		https://www.opengl.org/registry/specs/NV/DX_interop.txt


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
#pragma once
#ifndef __spoutGLDXinterop__ // standard way as well
#define __spoutGLDXinterop__

#include "SpoutCommon.h"
#include "SpoutDirectX.h"
#include "SpoutSenderNames.h"
#include "SpoutMemoryShare.h"
#include "SpoutCopy.h"

#include <windowsx.h>
#include <d3d9.h>	// DX9
#include <d3d11.h>	// DX11
#include <gl/gl.h>
#include <gl/glu.h> // For glerror
#include <shlwapi.h> // for path functions

#include "SpoutGLextensions.h" // include last due to redefinition problems with OpenCL


class SPOUT_DLLEXP spoutGLDXinterop {

	public:

		spoutGLDXinterop();
		~spoutGLDXinterop();

		// Spout objects
		spoutSenderNames senders; // Sender management
		spoutDirectX spoutdx; // DirectX functions
		spoutCopy spoutcopy; // Memory copy and rgb-rgba conversion functions
		spoutMemoryShare memoryshare; // Memory sharing

		// Initialization functions
		bool LoadGLextensions(); // Load required opengl extensions
		bool CreateInterop(HWND hWnd, const char* sendername, unsigned int width, unsigned int height, DWORD dwFormat, bool bReceive = true);
		void CleanupInterop(bool bExit = false); // Cleanup with flag to avoid unknown crash bug

		bool getSharedInfo(char* sharedMemoryName, SharedTextureInfo* info);
		bool setSharedInfo(char* sharedMemoryName, SharedTextureInfo* info);

		// Texture functions
		bool WriteTexture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert=true,  GLuint HostFBO=0);
		bool ReadTexture (GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert=false, GLuint HostFBO=0);
		bool WriteTexturePixels(const unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert = false, GLuint HostFBO = 0);
		bool ReadTexturePixels (unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert = false, GLuint HostFBO=0);
		bool DrawSharedTexture (float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = true, GLuint HostFBO = 0);
		bool DrawToSharedTexture (GLuint TexID, GLuint TexTarget, unsigned int width, unsigned int height, float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = false, GLuint HostFBO = 0);
		bool BindSharedTexture();
		bool UnBindSharedTexture();
		
		// DX11 shared texture write and read
		bool WriteTexture(ID3D11Texture2D** texture);
		bool ReadTexture (ID3D11Texture2D** texture);

		// PBO functions for external access
		bool UnloadTexturePixels(GLuint TextureID, GLuint TextureTarget, 
								 unsigned int width, unsigned int height,
								 unsigned char *data, GLenum glFormat = GL_RGBA,
								 bool bInvert = false, GLuint HostFBO = 0);

		bool LoadTexturePixels(GLuint TextureID, GLuint TextureTarget, 
							   unsigned int width, unsigned int height,
							   const unsigned char *data, GLenum glFormat = GL_RGBA, 
							   bool bInvert = false);

		// DX9
		bool m_bUseDX9; // Use DX11 (default) or DX9
		bool GetDX9();
		bool SetDX9(bool bDX9);
		bool UseDX9(bool bDX9); // Includes DX11 compatibility check
		bool isDX9(); // Test for DX11 in case it failed to initialize

		bool m_bUseCPU; // Use CPU texture processing
		bool SetCPUmode(bool bCPU = true);
		bool GetCPUmode();

		bool m_bUseMemory; // Use memoryshare
		bool SetMemoryShareMode(bool bMem = true);
		bool GetMemoryShareMode();

		int  GetShareMode(); // 0 - memory, 1 - cpu, 2 - texture
		bool SetShareMode(int mode);

		bool IsBGRAavailable(); // are the bgra extensions available
		bool IsPBOavailable();  // Are pbo extensions supported
		void SetBufferMode(bool bActive); // Set the pbo availability on or off
		bool GetBufferMode();

		int GetNumAdapters(); // Get the number of graphics adapters in the system
		bool GetAdapterName(int index, char *adaptername, int maxchars); // Get an adapter name
		bool SetAdapter(int index); // Set required graphics adapter for output
		int GetAdapter(); // Get the SpoutDirectX global adapter index
		bool GetHostPath(const char *sendername, char *hostpath, int maxchars); // The path of the host that produced the sender

		// DX9
		D3DFORMAT DX9format; // the DX9 texture format to be used
		void SetDX9format(D3DFORMAT textureformat);
		bool CreateDX9interop(unsigned int width, unsigned int height, DWORD dwFormat, bool bReceive = true);
		bool OpenDirectX9(HWND hWnd); // Initialize and prepare DirectX9
		void CleanupDX9(bool bExit = false);

		// DX11
		DXGI_FORMAT	DX11format; // the DX11 texture format to be used
		void SetDX11format(DXGI_FORMAT textureformat); // set format by user
		bool CreateDX11interop(unsigned int width, unsigned int height, DWORD dwFormat, bool bReceive);
		bool OpenDirectX11(); // Initialize and prepare DirectX11
		bool DX11available(); // Test for DX11 by attempting to open a device
		void CleanupDX11(bool bExit = false);

		// Common
		bool OpenDirectX(HWND hWnd, bool bDX9);
		void CleanupDirectX(bool bExit = false);
		HANDLE LinkGLDXtextures(void* pDXdevice, void* pSharedTexture, HANDLE dxShareHandle, GLuint glTextureID);
		
		// Locks for gl/dx interop functions
		HRESULT LockInteropObject(HANDLE hDevice, HANDLE *hObject);
		HRESULT UnlockInteropObject(HANDLE hDevice, HANDLE *hObject);

		// Utilities
		bool GLDXcompatible();
		bool isOptimus();
		int  GetVerticalSync();
		bool SetVerticalSync(bool bSync = true);
		bool GetAdapterInfo(char *renderadapter, 
						    char *renderdescription, char *renderversion,
							char *displaydescription, char *displayversion,
							int maxsize, bool &bUseDX9);
		DWORD GetSpoutVersion(); // Get Spout version from the registry - starting at 2.005
		
		// OpenGL utilities
		bool InitOpenGL();
		bool CloseOpenGL();
		bool CopyTexture(GLuint SourceID, GLuint SourceTarget, GLuint DestID, GLuint DestTarget,
						 unsigned int width, unsigned int height, bool bInvert, GLuint HostFBO);
		void InitTexture(GLuint &texID, GLenum GLformat, unsigned int width, unsigned int height);
		void CheckOpenGLTexture(GLuint &texID, GLenum GLformat,
								unsigned int newWidth, unsigned int newHeight,
								unsigned int &texWidth, unsigned int &texHeight);
		void SaveOpenGLstate(unsigned int width, unsigned int height, bool bFitWindow = true);
		void RestoreOpenGLstate();
		GLuint GetGLtextureID(); // Get OpenGL shared texture ID
		void GLerror();
		void PrintFBOstatus(GLenum status);

		GLuint m_glTexture; // the OpenGL texture linked to the shared DX texture
		GLuint m_fbo; // General fbo used for texture transfers

		// public for external access
		IDirect3DDevice9Ex* m_pDevice;     // DX9 device
		LPDIRECT3DTEXTURE9  m_dxTexture;   // the shared DX9 texture
		HANDLE m_dxShareHandle;            // the shared DX texture handle
		ID3D11Device* g_pd3dDevice;        // DX11 device
		ID3D11Texture2D* g_pSharedTexture; // The shared DX11 texture

protected:

		bool m_bInitialized;	  // this instance initialized flag
		bool m_bExtensionsLoaded; // extensions have been loaded
		unsigned int m_caps;      // extension capabilities
		bool m_bFBOavailable;     // fbo extensions available
		bool m_bBLITavailable;    // fbo blit extensions available
		bool m_bPBOavailable;     // pbo extensions available
		bool m_bSWAPavailable;    // swap extensions available
		bool m_bBGRAavailable;    // BGRA extensions are supported
		bool m_bGLDXavailable;    // GL/DX interop extensions are supported

		HWND              m_hWnd;          // parent window
		HANDLE            m_hSharedMemory; // handle to the texture info shared memory
		SharedTextureInfo m_TextureInfo;   // local texture info structure

		GLuint            m_TexID;         // Local texture used for memoryshare and CPU functions
		unsigned int      m_TexWidth;      // width and height of local texture
		unsigned int      m_TexHeight;     // height of local texture

		// PBO support
		GLuint m_pbo[2];
		int PboIndex;
		int NextPboIndex;

		// For InitOpenGL and CloseOpenGL
		HDC m_hdc;
		HWND m_hwndButton;
		HGLRC m_hRc;

		// DX11
		ID3D11DeviceContext* g_pImmediateContext;
		D3D_DRIVER_TYPE      g_driverType;
		D3D_FEATURE_LEVEL    g_featureLevel;
		ID3D11Texture2D*     g_pStagingTexture; // A staging texture for CPU access

		// DX9
		IDirect3D9Ex* m_pD3D; // DX9 object
		LPDIRECT3DSURFACE9 g_DX9surface; // A surface for CPU access
	
		// Interop
		HANDLE m_hInteropDevice; // handle to the DX/GL interop device
		HANDLE m_hInteropObject; // handle to the DX/GL interop object (the shared texture)
		HANDLE m_hAccessMutex;   // Texture access mutex lock handle

		bool getSharedTextureInfo(const char* sharedMemoryName);
		bool setSharedTextureInfo(const char* sharedMemoryName);

		// GL/DX interop texture functions
		bool WriteGLDXtexture (GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert=true,  GLuint HostFBO=0);
		bool ReadGLDXtexture  (GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert=false, GLuint HostFBO=0);
		bool WriteGLDXpixels  (const unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert = false, GLuint HostFBO = 0);
		bool ReadGLDXpixels   (unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert = false, GLuint HostFBO=0);
		bool DrawGLDXtexture  (float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = true);
		bool DrawToGLDXtexture(GLuint TexID, GLuint TexTarget, unsigned int width, unsigned int height, float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = false, GLuint HostFBO = 0);

		// DX11 staging texture functions for CPU access
		bool WriteDX11texture (GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert=false, GLuint HostFBO=0);
		bool ReadDX11texture  (GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert=false, GLuint HostFBO=0);
		bool WriteDX11pixels  (const unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert=false);
		bool ReadDX11pixels   (unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert=false);
		bool DrawDX11texture  (float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = false, GLuint HostFBO=0);
		bool DrawToDX11texture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = false, GLuint HostFBO = 0);
		bool CheckStagingTexture(unsigned int width, unsigned int height);
		void FlushWait();

		// DX9 surface functions for CPU access
		bool WriteDX9surface (LPDIRECT3DSURFACE9 source_surface);
		bool WriteDX9texture (GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert=false, GLuint HostFBO=0);
		bool ReadDX9texture  (GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert=false, GLuint HostFBO=0);
		bool WriteDX9pixels  (const unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert=false);
		bool ReadDX9pixels   (unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert=false);
		bool DrawDX9texture  (float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = false, GLuint HostFBO = 0);
		bool DrawToDX9texture(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = false, GLuint HostFBO = 0);
		bool CheckDX9surface (unsigned int width, unsigned int height);

		// Memoryshare functions
		bool WriteMemory (GLuint TexID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert = false,  GLuint HostFBO=0);
		bool ReadMemory  (GLuint TexID, GLuint TextureTarget, unsigned int width, unsigned int height, bool bInvert = false,  GLuint HostFBO=0);
		bool WriteMemoryPixels (const unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert = false);
		bool ReadMemoryPixels  (unsigned char *pixels, unsigned int width, unsigned int height, GLenum glFormat = GL_RGBA, bool bInvert = false);
		bool DrawSharedMemory  (float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = false);
		bool DrawToSharedMemory(GLuint TextureID, GLuint TextureTarget, unsigned int width, unsigned int height, float max_x = 1.0, float max_y = 1.0, float aspect = 1.0, bool bInvert = false, GLuint HostFBO = 0);

		// Utility
		bool OpenDeviceKey(const char* key, int maxsize, char *description, char *version);
		void trim(char * s);

};

#endif
