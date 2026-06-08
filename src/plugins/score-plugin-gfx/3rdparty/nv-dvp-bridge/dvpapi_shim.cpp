/*
 * NVIDIA "GPUDirect for Video" (DVP) runtime-loaded shim.
 * See dvpapi_shim.h for design + license details.
 *
 * Adapted from Blender's intern/gpudirect/dvpapi.cpp (GPL-2.0+):
 *   Copyright (C) 2015 Blender Foundation. All rights reserved.
 *
 * Cross-platform — BOTH platforms ship libdvp as a C++ library (no
 * `extern "C"`), so the shim resolves mangled symbol names on both:
 *   - Windows: MSVC C++ mangling. Names verified against `dvp.dll` v1.70
 *     (SHA256 602f2cef…), available either from the PlusToolkit/PlusLib
 *     mirror or bundled with the Blackmagic DeckLink SDK at
 *     `decklink/Win/Samples/bin/dvp.dll`. Surface: D3D9 / D3D10 / D3D11,
 *     OpenGL, **CUDA** (dvpInitCUDAContext, dvpMemcpyCuda, …), plus
 *     sync primitives. We currently resolve only the GL + D3D11 subset.
 *   - Linux: GCC Itanium ABI mangling. Names verified against
 *     `libdvp.so.1` (same v1.70 ABI) shipped inside the DeckLink SDK at
 *     `decklink/Linux/Samples/NVIDIA_GPUDirect/x86_64/libdvp.so.1`.
 *     Surface: OpenGL + CUDA + `dvpCreateGPUBufferGL` (no D3D11, which
 *     doesn't exist on Linux). We currently resolve only the GL subset.
 *
 * The CUDA path (`dvpInit/Close/Bind/UnbindFromCUDACtx`, `dvpMemcpyCuda`,
 * `dvpMapBuffer{Wait,End}CUDAStream`) is **cross-platform** on both
 * Windows and Linux — score's existing GPU-direct pipeline uses
 * `CudaP2PBridge` (which is independent of DVP), but the DVP CUDA
 * surface is a viable second path.
 *
 * To dump symbols for cross-checking after an SDK ABI change:
 *   dumpbin -exports dvp.dll                            (Windows)
 *   nm -D --defined-only libdvp.so.1 | sort             (Linux)
 */

#include "dvpapi_shim.h"

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>

/* ============================================================================
 * Global function pointer storage (definitions for the externs in the
 * header). All start nulled; nv_dvp_load_runtime fills them.
 * ============================================================================ */

extern "C" {

PFN_dvpInitGLContext dvpInitGLContext = nullptr;
PFN_dvpCloseGLContext dvpCloseGLContext = nullptr;
PFN_dvpGetLibraryVersion dvpGetLibraryVersion = nullptr;
PFN_dvpBegin dvpBegin = nullptr;
PFN_dvpEnd dvpEnd = nullptr;
PFN_dvpCreateBuffer dvpCreateBuffer = nullptr;
PFN_dvpDestroyBuffer dvpDestroyBuffer = nullptr;
PFN_dvpFreeBuffer dvpFreeBuffer = nullptr;
PFN_dvpMemcpyLined dvpMemcpyLined = nullptr;
PFN_dvpMemcpy dvpMemcpy = nullptr;
PFN_dvpImportSyncObject dvpImportSyncObject = nullptr;
PFN_dvpFreeSyncObject dvpFreeSyncObject = nullptr;
PFN_dvpSyncObjClientWaitPartial dvpSyncObjClientWaitPartial = nullptr;
PFN_dvpMapBufferEndAPI dvpMapBufferEndAPI = nullptr;
PFN_dvpMapBufferWaitDVP dvpMapBufferWaitDVP = nullptr;
PFN_dvpMapBufferEndDVP dvpMapBufferEndDVP = nullptr;
PFN_dvpMapBufferWaitAPI dvpMapBufferWaitAPI = nullptr;
PFN_dvpBindToGLCtx dvpBindToGLCtx = nullptr;
PFN_dvpUnbindFromGLCtx dvpUnbindFromGLCtx = nullptr;
PFN_dvpCreateGPUTextureGL dvpCreateGPUTextureGL = nullptr;
PFN_dvpGetRequiredConstantsGLCtx dvpGetRequiredConstantsGLCtx = nullptr;

PFN_dvpInitCUDAContext dvpInitCUDAContext = nullptr;
PFN_dvpCloseCUDAContext dvpCloseCUDAContext = nullptr;
PFN_dvpBindToCUDACtx dvpBindToCUDACtx = nullptr;
PFN_dvpUnbindFromCUDACtx dvpUnbindFromCUDACtx = nullptr;
PFN_dvpCreateGPUCUDAArray dvpCreateGPUCUDAArray = nullptr;
PFN_dvpCreateGPUCUDADevicePtr dvpCreateGPUCUDADevicePtr = nullptr;
PFN_dvpMapBufferWaitCUDAStream dvpMapBufferWaitCUDAStream = nullptr;
PFN_dvpMapBufferEndCUDAStream dvpMapBufferEndCUDAStream = nullptr;
PFN_dvpGetRequiredConstantsCUDACtx dvpGetRequiredConstantsCUDACtx = nullptr;

PFN_dvpSyncObjClientWaitComplete dvpSyncObjClientWaitComplete = nullptr;
PFN_dvpSyncObjCompletion dvpSyncObjCompletion = nullptr;

#if defined(_WIN32)
PFN_dvpInitD3D11Device dvpInitD3D11Device = nullptr;
PFN_dvpCloseD3D11Device dvpCloseD3D11Device = nullptr;
PFN_dvpCreateGPUD3D11Resource dvpCreateGPUD3D11Resource = nullptr;
PFN_dvpBindToD3D11Device dvpBindToD3D11Device = nullptr;
PFN_dvpUnbindFromD3D11Device dvpUnbindFromD3D11Device = nullptr;
PFN_dvpGetRequiredConstantsD3D11Device dvpGetRequiredConstantsD3D11Device = nullptr;
#endif

} // extern "C"

namespace
{

constexpr uint32_t kRequiredMajor = 1;
constexpr uint32_t kRequiredMinor = 63;

#if defined(_WIN32)
HMODULE g_module = nullptr;
#else
void* g_module = nullptr;
#endif

char g_error[512] = {};
std::atomic<bool> g_glOk{false};
std::atomic<bool> g_d3d11Ok{false};
std::atomic<bool> g_cudaOk{false};
std::once_flag g_onceFlag;

// Lookup the symbol `name` in the loaded library; report a fix-it
// message on failure. Naming convention differs per platform — see
// the two-arg overload that picks the right name.
bool resolveSym(const char* name, void** outFn)
{
#if defined(_WIN32)
  FARPROC p = ::GetProcAddress(g_module, name);
#else
  void* p = ::dlsym(g_module, name);
#endif
  if(!p)
  {
    if(g_error[0] == '\0')
    {
#if defined(_WIN32)
      std::snprintf(g_error, sizeof(g_error),
                    "GetProcAddress failed: %s", name);
#else
      const char* errMsg = ::dlerror();
      std::snprintf(g_error, sizeof(g_error),
                    "dlsym failed: %s (%s)", name,
                    errMsg ? errMsg : "no detail");
#endif
    }
    return false;
  }
  *outFn = reinterpret_cast<void*>(p);
  return true;
}

// Resolve a symbol given its two mangled forms. Both Windows and Linux
// ship libdvp as a C++ library, so each platform has its own mangling
// scheme:
//   - Windows: MSVC C++ mangling (`?dvpBegin@@YA?AW4DVPStatus@@XZ` style).
//   - Linux: GCC Itanium ABI mangling (`_Z8dvpBeginv` style).
// Caller passes both; the active-platform name is used.
bool resolve(const char* mangledWin, const char* mangledLinux, void** outFn)
{
#if defined(_WIN32)
  (void)mangledLinux;
  return resolveSym(mangledWin, outFn);
#else
  (void)mangledWin;
  return resolveSym(mangledLinux, outFn);
#endif
}

void doLoad()
{
#if defined(_WIN32)
  g_module = ::LoadLibraryA("dvp.dll");
  if(!g_module)
  {
    std::snprintf(g_error, sizeof(g_error),
                  "LoadLibraryA(\"dvp.dll\") failed (Win32 error %lu)",
                  ::GetLastError());
    return;
  }
#else
  // Try the versioned soname first, then unversioned fallback.
  g_module = ::dlopen("libdvp.so.1", RTLD_NOW);
  if(!g_module)
    g_module = ::dlopen("libdvp.so", RTLD_NOW);
  if(!g_module)
  {
    const char* errMsg = ::dlerror();
    std::snprintf(g_error, sizeof(g_error),
                  "dlopen(\"libdvp.so[.1]\") failed: %s",
                  errMsg ? errMsg : "no detail");
    return;
  }
#endif

  /* === Common (non-API-specific) DVP entry points. ===
   * Windows MSVC mangling (left arg) vs Linux Itanium mangling (right arg).
   * Linux names verified against the libdvp.so.1 in DeckLink SDK 14.x's
   * `NVIDIA_GPUDirect/x86_64/`. Re-derive after ABI changes with:
   *   nm -D --defined-only libdvp.so.1 | sort
   */
  bool common = true;
  common &= resolve("?dvpGetLibrayVersion@@YA?AW4DVPStatus@@PEAI0@Z",
                    "_Z19dvpGetLibrayVersionPjS_",
                    reinterpret_cast<void**>(&dvpGetLibraryVersion));
  common &= resolve("?dvpBegin@@YA?AW4DVPStatus@@XZ",
                    "_Z8dvpBeginv",
                    reinterpret_cast<void**>(&dvpBegin));
  common &= resolve("?dvpEnd@@YA?AW4DVPStatus@@XZ",
                    "_Z6dvpEndv",
                    reinterpret_cast<void**>(&dvpEnd));
  common &= resolve(
      "?dvpCreateBuffer@@YA?AW4DVPStatus@@PEAUDVPSysmemBufferDescRec@@PEA_K@Z",
      "_Z15dvpCreateBufferP22DVPSysmemBufferDescRecPm",
      reinterpret_cast<void**>(&dvpCreateBuffer));
  common &= resolve("?dvpDestroyBuffer@@YA?AW4DVPStatus@@_K@Z",
                    "_Z16dvpDestroyBufferm",
                    reinterpret_cast<void**>(&dvpDestroyBuffer));
  common &= resolve("?dvpFreeBuffer@@YA?AW4DVPStatus@@_K@Z",
                    "_Z13dvpFreeBufferm",
                    reinterpret_cast<void**>(&dvpFreeBuffer));
  common &= resolve("?dvpMemcpyLined@@YA?AW4DVPStatus@@_K0I000III@Z",
                    "_Z14dvpMemcpyLinedmmjmmmjjj",
                    reinterpret_cast<void**>(&dvpMemcpyLined));
  /* dvpMemcpy is exported as dvpMemcpy2D in both dvp.dll and libdvp.so.1;
     same signature so we re-target the symbol. Blender's shim does the
     same on Windows. */
  common &= resolve("?dvpMemcpy2D@@YA?AW4DVPStatus@@_K0I000IIIII@Z",
                    "_Z11dvpMemcpy2Dmmjmmmjjjjj",
                    reinterpret_cast<void**>(&dvpMemcpy));
  common &= resolve(
      "?dvpImportSyncObject@@YA?AW4DVPStatus@@PEAUDVPSyncObjectDescRec@@PEA_K@Z",
      "_Z19dvpImportSyncObjectP20DVPSyncObjectDescRecPm",
      reinterpret_cast<void**>(&dvpImportSyncObject));
  common &= resolve("?dvpFreeSyncObject@@YA?AW4DVPStatus@@_K@Z",
                    "_Z17dvpFreeSyncObjectm",
                    reinterpret_cast<void**>(&dvpFreeSyncObject));
  common &= resolve(
      "?dvpSyncObjClientWaitPartial@@YA?AW4DVPStatus@@_KI0@Z",
      "_Z27dvpSyncObjClientWaitPartialmjm",
      reinterpret_cast<void**>(&dvpSyncObjClientWaitPartial));
  common &= resolve("?dvpMapBufferEndAPI@@YA?AW4DVPStatus@@_K@Z",
                    "_Z18dvpMapBufferEndAPIm",
                    reinterpret_cast<void**>(&dvpMapBufferEndAPI));
  common &= resolve("?dvpMapBufferWaitDVP@@YA?AW4DVPStatus@@_K@Z",
                    "_Z19dvpMapBufferWaitDVPm",
                    reinterpret_cast<void**>(&dvpMapBufferWaitDVP));
  common &= resolve("?dvpMapBufferEndDVP@@YA?AW4DVPStatus@@_K@Z",
                    "_Z18dvpMapBufferEndDVPm",
                    reinterpret_cast<void**>(&dvpMapBufferEndDVP));
  common &= resolve("?dvpMapBufferWaitAPI@@YA?AW4DVPStatus@@_K@Z",
                    "_Z19dvpMapBufferWaitAPIm",
                    reinterpret_cast<void**>(&dvpMapBufferWaitAPI));
  /* Additional sync primitives — present in v1.70 but not used by
   * Blender's shim. dvpSyncObjClientWaitComplete is the blocking
   * counterpart to *Partial; dvpSyncObjCompletion returns the current
   * value. Optional; resolution failure isn't fatal. */
  (void)resolve("?dvpSyncObjClientWaitComplete@@YA?AW4DVPStatus@@_K0@Z",
                "_Z28dvpSyncObjClientWaitCompletemm",
                reinterpret_cast<void**>(&dvpSyncObjClientWaitComplete));
  (void)resolve("?dvpSyncObjCompletion@@YA?AW4DVPStatus@@_KPEA_K@Z",
                "_Z20dvpSyncObjCompletionmPm",
                reinterpret_cast<void**>(&dvpSyncObjCompletion));

  /* Verify the library version - matches Blender's check. */
  if(common && dvpGetLibraryVersion)
  {
    uint32_t major = 0, minor = 0;
    DVPStatus s = dvpGetLibraryVersion(&major, &minor);
    if(s != DVP_STATUS_OK
       || major != kRequiredMajor || minor < kRequiredMinor)
    {
      if(g_error[0] == '\0')
        std::snprintf(g_error, sizeof(g_error),
                      "DVP runtime version mismatch: got %u.%u, need %u.%u+",
                      major, minor, kRequiredMajor, kRequiredMinor);
      return;
    }
  }

  /* === OpenGL entry points === */
  bool gl = common;
  gl &= resolve("?dvpInitGLContext@@YA?AW4DVPStatus@@I@Z",
                "_Z16dvpInitGLContextj",
                reinterpret_cast<void**>(&dvpInitGLContext));
  gl &= resolve("?dvpCloseGLContext@@YA?AW4DVPStatus@@XZ",
                "_Z17dvpCloseGLContextv",
                reinterpret_cast<void**>(&dvpCloseGLContext));
  gl &= resolve("?dvpBindToGLCtx@@YA?AW4DVPStatus@@_K@Z",
                "_Z14dvpBindToGLCtxm",
                reinterpret_cast<void**>(&dvpBindToGLCtx));
  gl &= resolve("?dvpUnbindFromGLCtx@@YA?AW4DVPStatus@@_K@Z",
                "_Z18dvpUnbindFromGLCtxm",
                reinterpret_cast<void**>(&dvpUnbindFromGLCtx));
  gl &= resolve("?dvpCreateGPUTextureGL@@YA?AW4DVPStatus@@IPEA_K@Z",
                "_Z21dvpCreateGPUTextureGLjPm",
                reinterpret_cast<void**>(&dvpCreateGPUTextureGL));
  gl &= resolve(
      "?dvpGetRequiredConstantsGLCtx@@YA?AW4DVPStatus@@PEAI00000@Z",
      "_Z28dvpGetRequiredConstantsGLCtxPjS_S_S_S_S_",
      reinterpret_cast<void**>(&dvpGetRequiredConstantsGLCtx));

  g_glOk.store(gl, std::memory_order_release);

  /* === CUDA entry points (cross-platform — v1.70 dvp.dll v1.70 and
   * libdvp.so.1 v1.70 both export the full CUDA surface). === */
  bool cuda = common;
  cuda &= resolve("?dvpInitCUDAContext@@YA?AW4DVPStatus@@I@Z",
                  "_Z18dvpInitCUDAContextj",
                  reinterpret_cast<void**>(&dvpInitCUDAContext));
  cuda &= resolve("?dvpCloseCUDAContext@@YA?AW4DVPStatus@@XZ",
                  "_Z19dvpCloseCUDAContextv",
                  reinterpret_cast<void**>(&dvpCloseCUDAContext));
  cuda &= resolve("?dvpBindToCUDACtx@@YA?AW4DVPStatus@@_K@Z",
                  "_Z16dvpBindToCUDACtxm",
                  reinterpret_cast<void**>(&dvpBindToCUDACtx));
  cuda &= resolve("?dvpUnbindFromCUDACtx@@YA?AW4DVPStatus@@_K@Z",
                  "_Z20dvpUnbindFromCUDACtxm",
                  reinterpret_cast<void**>(&dvpUnbindFromCUDACtx));
  cuda &= resolve(
      "?dvpCreateGPUCUDAArray@@YA?AW4DVPStatus@@PEAUCUarray_st@@PEA_K@Z",
      "_Z21dvpCreateGPUCUDAArrayP10CUarray_stPm",
      reinterpret_cast<void**>(&dvpCreateGPUCUDAArray));
  cuda &= resolve(
      "?dvpCreateGPUCUDADevicePtr@@YA?AW4DVPStatus@@_KPEA_K@Z",
      "_Z25dvpCreateGPUCUDADevicePtryPm",
      reinterpret_cast<void**>(&dvpCreateGPUCUDADevicePtr));
  cuda &= resolve(
      "?dvpMapBufferWaitCUDAStream@@YA?AW4DVPStatus@@_KPEAUCUstream_st@@@Z",
      "_Z26dvpMapBufferWaitCUDAStreammP11CUstream_st",
      reinterpret_cast<void**>(&dvpMapBufferWaitCUDAStream));
  cuda &= resolve(
      "?dvpMapBufferEndCUDAStream@@YA?AW4DVPStatus@@_KPEAUCUstream_st@@@Z",
      "_Z25dvpMapBufferEndCUDAStreammP11CUstream_st",
      reinterpret_cast<void**>(&dvpMapBufferEndCUDAStream));
  cuda &= resolve(
      "?dvpGetRequiredConstantsCUDACtx@@YA?AW4DVPStatus@@PEAI00000@Z",
      "_Z30dvpGetRequiredConstantsCUDACtxPjS_S_S_S_S_",
      reinterpret_cast<void**>(&dvpGetRequiredConstantsCUDACtx));
  g_cudaOk.store(cuda, std::memory_order_release);

#if defined(_WIN32)
  /* === D3D11 entry points (Windows only — MSVC mangled names) === */
  bool d3d11 = common;
  d3d11 &= resolveSym(
      "?dvpInitD3D11Device@@YA?AW4DVPStatus@@PEAUID3D11Device@@I@Z",
      reinterpret_cast<void**>(&dvpInitD3D11Device));
  d3d11 &= resolveSym(
      "?dvpCloseD3D11Device@@YA?AW4DVPStatus@@PEAUID3D11Device@@@Z",
      reinterpret_cast<void**>(&dvpCloseD3D11Device));
  d3d11 &= resolveSym(
      "?dvpCreateGPUD3D11Resource@@YA?AW4DVPStatus@@PEAUID3D11Resource@@PEA_K@Z",
      reinterpret_cast<void**>(&dvpCreateGPUD3D11Resource));
  d3d11 &= resolveSym(
      "?dvpBindToD3D11Device@@YA?AW4DVPStatus@@_KPEAUID3D11Device@@@Z",
      reinterpret_cast<void**>(&dvpBindToD3D11Device));
  d3d11 &= resolveSym(
      "?dvpUnbindFromD3D11Device@@YA?AW4DVPStatus@@_KPEAUID3D11Device@@@Z",
      reinterpret_cast<void**>(&dvpUnbindFromD3D11Device));
  d3d11 &= resolveSym(
      "?dvpGetRequiredConstantsD3D11Device@@YA?AW4DVPStatus@@PEAI00000PEAUID3D11Device@@@Z",
      reinterpret_cast<void**>(&dvpGetRequiredConstantsD3D11Device));
  g_d3d11Ok.store(d3d11, std::memory_order_release);
#endif
}

} // namespace

extern "C" {

bool nv_dvp_load_runtime(void)
{
  std::call_once(g_onceFlag, doLoad);
  return g_glOk.load(std::memory_order_acquire)
         || g_d3d11Ok.load(std::memory_order_acquire)
         || g_cudaOk.load(std::memory_order_acquire);
}

bool nv_dvp_have_gl(void)
{
  return g_glOk.load(std::memory_order_acquire);
}

bool nv_dvp_have_d3d11(void)
{
  return g_d3d11Ok.load(std::memory_order_acquire);
}

bool nv_dvp_have_cuda(void)
{
  return g_cudaOk.load(std::memory_order_acquire);
}

const char* nv_dvp_get_runtime_error(void)
{
  return g_error;
}

} // extern "C"
