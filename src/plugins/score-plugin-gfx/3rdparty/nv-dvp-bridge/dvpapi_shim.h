/*
 * NVIDIA "GPUDirect for Video" (DVP) runtime-loaded shim.
 *
 * Adapted from Blender's intern/gpudirect/dvpapi.{h,cpp}:
 *   Copyright (C) 2015 Blender Foundation. All rights reserved.
 *   Licensed under the GNU GPL v2 or later.
 *
 * Modifications for ossia score:
 *   - Replaced BLI_dynlib with bare LoadLibraryA / GetProcAddress (Win32)
 *     and dlopen / dlsym (Linux). Zero Blender dependency.
 *   - Added D3D11 entry points (Windows only) using MSVC mangled names.
 *   - Linux symbol lookup uses GCC Itanium-ABI mangled names —
 *     `libdvp.so.1` is a C++ library too (no `extern "C"`), so
 *     `dlsym(handle, "dvpBegin")` returns NULL; we resolve
 *     `dlsym(handle, "_Z8dvpBeginv")` instead. Names verified against
 *     the libdvp.so.1 shipped with DeckLink SDK 14.x Linux samples
 *     (path: `decklink/Linux/Samples/NVIDIA_GPUDirect/x86_64/libdvp.so.1`).
 *     To re-derive after an ABI change: `nm -D --defined-only libdvp.so.1`.
 *   - The shim makes runtime-load failures explicit (reports which
 *     symbol failed to resolve) so future SDK updates can fix names
 *     individually.
 *
 * Runtime sources:
 *   - Windows: `dvp.dll` from NVIDIA's "GPUDirect for Video" SDK (free,
 *     NVIDIA developer login). The CMakeLists has an opt-in fetch
 *     (`SCORE_FETCH_DVP_DLL=ON`) that downloads v1.70 from
 *     PlusToolkit/PlusLib for development.
 *   - Linux: `libdvp.so.1` shipped inside the Blackmagic DeckLink SDK
 *     (`decklink/Linux/Samples/NVIDIA_GPUDirect/x86_64/libdvp.so.1`) or
 *     packaged with NVIDIA's video-codec SDK. The shipped binary uses
 *     Itanium C++ mangling — see the per-symbol comments below.
 *
 * Without the runtime on the system, `nv_dvp_load_runtime()` returns
 * false and consumers (AJA DVP strategies, planned DeckLink DVP path)
 * refuse to initialize.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * NVIDIA DVP types (subset used by the AJA DVP path).
 * Mirrors the layout in NVIDIA's DVPAPI.h v1.63.
 * ============================================================================ */

typedef uint64_t DVPBufferHandle;
typedef uint64_t DVPSyncObjectHandle;

typedef enum
{
  DVP_STATUS_OK = 0,
  DVP_STATUS_INVALID_PARAMETER = 1,
  DVP_STATUS_UNSUPPORTED = 2,
  DVP_STATUS_END_ENUMERATION = 3,
  DVP_STATUS_INVALID_DEVICE = 4,
  DVP_STATUS_OUT_OF_MEMORY = 5,
  DVP_STATUS_INVALID_OPERATION = 6,
  DVP_STATUS_TIMEOUT = 7,
  DVP_STATUS_INVALID_CONTEXT = 8,
  DVP_STATUS_INVALID_RESOURCE_TYPE = 9,
  DVP_STATUS_INVALID_FORMAT_OR_TYPE = 10,
  DVP_STATUS_DEVICE_UNINITIALIZED = 11,
  DVP_STATUS_UNSIGNALED = 12,
  DVP_STATUS_SYNC_ERROR = 13,
  DVP_STATUS_SYNC_STILL_BOUND = 14,
  DVP_STATUS_ERROR = -1
} DVPStatus;

typedef enum
{
  DVP_BUFFER,
  DVP_DEPTH_COMPONENT,
  DVP_RGBA,
  DVP_BGRA,
  DVP_RED,
  DVP_GREEN,
  DVP_BLUE,
  DVP_ALPHA,
  DVP_RGB,
  DVP_BGR,
  DVP_LUMINANCE,
  DVP_LUMINANCE_ALPHA,
  DVP_CUDA_1_CHANNEL,
  DVP_CUDA_2_CHANNELS,
  DVP_CUDA_4_CHANNELS,
  DVP_RGBA_INTEGER,
  DVP_BGRA_INTEGER,
  DVP_RED_INTEGER,
  DVP_GREEN_INTEGER,
  DVP_BLUE_INTEGER,
  DVP_ALPHA_INTEGER,
  DVP_RGB_INTEGER,
  DVP_BGR_INTEGER,
  DVP_LUMINANCE_INTEGER,
  DVP_LUMINANCE_ALPHA_INTEGER
} DVPBufferFormats;

typedef enum
{
  DVP_UNSIGNED_BYTE,
  DVP_BYTE,
  DVP_UNSIGNED_SHORT,
  DVP_SHORT,
  DVP_UNSIGNED_INT,
  DVP_INT,
  DVP_FLOAT,
  DVP_HALF_FLOAT,
  DVP_UNSIGNED_BYTE_3_3_2,
  DVP_UNSIGNED_BYTE_2_3_3_REV,
  DVP_UNSIGNED_SHORT_5_6_5,
  DVP_UNSIGNED_SHORT_5_6_5_REV,
  DVP_UNSIGNED_SHORT_4_4_4_4,
  DVP_UNSIGNED_SHORT_4_4_4_4_REV,
  DVP_UNSIGNED_SHORT_5_5_5_1,
  DVP_UNSIGNED_SHORT_1_5_5_5_REV,
  DVP_UNSIGNED_INT_8_8_8_8,
  DVP_UNSIGNED_INT_8_8_8_8_REV,
  DVP_UNSIGNED_INT_10_10_10_2,
  DVP_UNSIGNED_INT_2_10_10_10_REV
} DVPBufferTypes;

typedef struct DVPSysmemBufferDescRec
{
  uint32_t width;
  uint32_t height;
  uint32_t stride;
  uint32_t size;
  DVPBufferFormats format;
  DVPBufferTypes type;
  void* bufAddr;
} DVPSysmemBufferDesc;

#define DVP_SYNC_OBJECT_FLAGS_USE_EVENTS 0x00000001

typedef struct DVPSyncObjectDescRec
{
  uint32_t* sem;
  uint32_t flags;
  DVPStatus (*externalClientWaitFunc)(
      DVPSyncObjectHandle sync, uint32_t value, bool GEQ, uint64_t timeout);
} DVPSyncObjectDesc;

#define DVP_TIMEOUT_IGNORED 0xFFFFFFFFFFFFFFFFull

/* Forward declarations of D3D11 types — we don't include d3d11.h from a
 * vendored shim. Translation units that actually call the D3D11 entry
 * points include d3d11.h before this header (or after; either order
 * works because the function pointer types take struct pointers and
 * struct redeclarations are legal). Windows-only — the D3D11 entry
 * points themselves are gated below. */
#if defined(_WIN32)
struct ID3D11Device;
struct ID3D11Resource;
#endif

/* Forward declarations of CUDA driver-API types. Identical to the
 * declarations in Gfx/Graph/interop/CudaFunctions.hpp; C++ permits
 * multiple typedef-name declarations naming the same type, so including
 * both headers in the same translation unit is safe. */
typedef struct CUstream_st* CUstream;
typedef struct CUarray_st* CUarray;
#if defined(_WIN64) || defined(__LP64__)
typedef unsigned long long CUdeviceptr;
#else
typedef unsigned int CUdeviceptr;
#endif

/* ============================================================================
 * Function pointers (resolved at runtime by nv_dvp_load_runtime).
 *
 * Macros redirect the SDK-style names (dvpBegin etc.) to the underlying
 * function pointers - this keeps consumer code looking like it links
 * against the SDK while in fact dispatching through dlsym/GetProcAddress.
 * ============================================================================ */

typedef DVPStatus (*PFN_dvpInitGLContext)(uint32_t flags);
typedef DVPStatus (*PFN_dvpCloseGLContext)(void);
typedef DVPStatus (*PFN_dvpGetLibraryVersion)(uint32_t* major, uint32_t* minor);
typedef DVPStatus (*PFN_dvpBegin)(void);
typedef DVPStatus (*PFN_dvpEnd)(void);
typedef DVPStatus (*PFN_dvpCreateBuffer)(
    DVPSysmemBufferDesc* desc, DVPBufferHandle* hBuf);
typedef DVPStatus (*PFN_dvpDestroyBuffer)(DVPBufferHandle hBuf);
typedef DVPStatus (*PFN_dvpFreeBuffer)(DVPBufferHandle hBuf);
typedef DVPStatus (*PFN_dvpMemcpyLined)(
    DVPBufferHandle src, DVPSyncObjectHandle srcSync, uint32_t srcAcq,
    uint64_t timeout, DVPBufferHandle dst, DVPSyncObjectHandle dstSync,
    uint32_t dstRel, uint32_t startLine, uint32_t numLines);
typedef DVPStatus (*PFN_dvpMemcpy)(
    DVPBufferHandle src, DVPSyncObjectHandle srcSync, uint32_t srcAcq,
    uint64_t timeout, DVPBufferHandle dst, DVPSyncObjectHandle dstSync,
    uint32_t dstRel, uint32_t srcOffset, uint32_t dstOffset, uint32_t count);
typedef DVPStatus (*PFN_dvpImportSyncObject)(
    DVPSyncObjectDesc* desc, DVPSyncObjectHandle* syncObject);
typedef DVPStatus (*PFN_dvpFreeSyncObject)(DVPSyncObjectHandle syncObject);
typedef DVPStatus (*PFN_dvpSyncObjClientWaitPartial)(
    DVPSyncObjectHandle sync, uint32_t value, uint64_t timeout);
typedef DVPStatus (*PFN_dvpMapBufferEndAPI)(DVPBufferHandle hBuf);
typedef DVPStatus (*PFN_dvpMapBufferWaitDVP)(DVPBufferHandle hBuf);
typedef DVPStatus (*PFN_dvpMapBufferEndDVP)(DVPBufferHandle hBuf);
typedef DVPStatus (*PFN_dvpMapBufferWaitAPI)(DVPBufferHandle hBuf);
typedef DVPStatus (*PFN_dvpBindToGLCtx)(DVPBufferHandle hBuf);
typedef DVPStatus (*PFN_dvpUnbindFromGLCtx)(DVPBufferHandle hBuf);
typedef DVPStatus (*PFN_dvpCreateGPUTextureGL)(
    uint32_t glTexId, DVPBufferHandle* hBuf);
typedef DVPStatus (*PFN_dvpGetRequiredConstantsGLCtx)(
    uint32_t* bufferAddrAlignment, uint32_t* bufferGPUStrideAlignment,
    uint32_t* semaphoreAddrAlignment, uint32_t* semaphoreAllocSize,
    uint32_t* semaphorePayloadOffset, uint32_t* semaphorePayloadSize);

/* CUDA — cross-platform (Windows v1.70 dvp.dll and Linux libdvp.so.1
 * both export the same surface). Used for sysmem↔CUDA-resource DMA
 * with CUstream-side synchronisation. */
typedef DVPStatus (*PFN_dvpInitCUDAContext)(uint32_t flags);
typedef DVPStatus (*PFN_dvpCloseCUDAContext)(void);
typedef DVPStatus (*PFN_dvpBindToCUDACtx)(DVPBufferHandle hBuf);
typedef DVPStatus (*PFN_dvpUnbindFromCUDACtx)(DVPBufferHandle hBuf);
typedef DVPStatus (*PFN_dvpCreateGPUCUDAArray)(
    CUarray array, DVPBufferHandle* hBuf);
typedef DVPStatus (*PFN_dvpCreateGPUCUDADevicePtr)(
    CUdeviceptr devPtr, DVPBufferHandle* hBuf);
typedef DVPStatus (*PFN_dvpMapBufferWaitCUDAStream)(
    DVPBufferHandle hBuf, CUstream stream);
typedef DVPStatus (*PFN_dvpMapBufferEndCUDAStream)(
    DVPBufferHandle hBuf, CUstream stream);
typedef DVPStatus (*PFN_dvpGetRequiredConstantsCUDACtx)(
    uint32_t* bufferAddrAlignment, uint32_t* bufferGPUStrideAlignment,
    uint32_t* semaphoreAddrAlignment, uint32_t* semaphoreAllocSize,
    uint32_t* semaphorePayloadOffset, uint32_t* semaphorePayloadSize);

/* Additional sync primitives (cross-platform, exported by v1.70):
 * SyncObjClientWaitComplete blocks until the sync completes (vs the
 * partial variant we already use); SyncObjCompletion returns the
 * current completion value. */
typedef DVPStatus (*PFN_dvpSyncObjClientWaitComplete)(
    DVPSyncObjectHandle sync, uint64_t timeout);
typedef DVPStatus (*PFN_dvpSyncObjCompletion)(
    DVPSyncObjectHandle sync, uint64_t* completionValue);

/* D3D11 — Windows-only extension over Blender's GL-only shim. */
#if defined(_WIN32)
typedef DVPStatus (*PFN_dvpInitD3D11Device)(
    struct ID3D11Device* dev, uint32_t flags);
typedef DVPStatus (*PFN_dvpCloseD3D11Device)(struct ID3D11Device* dev);
typedef DVPStatus (*PFN_dvpCreateGPUD3D11Resource)(
    struct ID3D11Resource* res, DVPBufferHandle* hBuf);
typedef DVPStatus (*PFN_dvpBindToD3D11Device)(
    DVPBufferHandle hBuf, struct ID3D11Device* dev);
typedef DVPStatus (*PFN_dvpUnbindFromD3D11Device)(
    DVPBufferHandle hBuf, struct ID3D11Device* dev);
typedef DVPStatus (*PFN_dvpGetRequiredConstantsD3D11Device)(
    uint32_t* bufferAddrAlignment, uint32_t* bufferGPUStrideAlignment,
    uint32_t* semaphoreAddrAlignment, uint32_t* semaphoreAllocSize,
    uint32_t* semaphorePayloadOffset, uint32_t* semaphorePayloadSize,
    struct ID3D11Device* dev);
#endif

extern PFN_dvpInitGLContext dvpInitGLContext;
extern PFN_dvpCloseGLContext dvpCloseGLContext;
extern PFN_dvpGetLibraryVersion dvpGetLibraryVersion;
extern PFN_dvpBegin dvpBegin;
extern PFN_dvpEnd dvpEnd;
extern PFN_dvpCreateBuffer dvpCreateBuffer;
extern PFN_dvpDestroyBuffer dvpDestroyBuffer;
extern PFN_dvpFreeBuffer dvpFreeBuffer;
extern PFN_dvpMemcpyLined dvpMemcpyLined;
extern PFN_dvpMemcpy dvpMemcpy;
extern PFN_dvpImportSyncObject dvpImportSyncObject;
extern PFN_dvpFreeSyncObject dvpFreeSyncObject;
extern PFN_dvpSyncObjClientWaitPartial dvpSyncObjClientWaitPartial;
extern PFN_dvpMapBufferEndAPI dvpMapBufferEndAPI;
extern PFN_dvpMapBufferWaitDVP dvpMapBufferWaitDVP;
extern PFN_dvpMapBufferEndDVP dvpMapBufferEndDVP;
extern PFN_dvpMapBufferWaitAPI dvpMapBufferWaitAPI;
extern PFN_dvpBindToGLCtx dvpBindToGLCtx;
extern PFN_dvpUnbindFromGLCtx dvpUnbindFromGLCtx;
extern PFN_dvpCreateGPUTextureGL dvpCreateGPUTextureGL;
extern PFN_dvpGetRequiredConstantsGLCtx dvpGetRequiredConstantsGLCtx;

extern PFN_dvpInitCUDAContext dvpInitCUDAContext;
extern PFN_dvpCloseCUDAContext dvpCloseCUDAContext;
extern PFN_dvpBindToCUDACtx dvpBindToCUDACtx;
extern PFN_dvpUnbindFromCUDACtx dvpUnbindFromCUDACtx;
extern PFN_dvpCreateGPUCUDAArray dvpCreateGPUCUDAArray;
extern PFN_dvpCreateGPUCUDADevicePtr dvpCreateGPUCUDADevicePtr;
extern PFN_dvpMapBufferWaitCUDAStream dvpMapBufferWaitCUDAStream;
extern PFN_dvpMapBufferEndCUDAStream dvpMapBufferEndCUDAStream;
extern PFN_dvpGetRequiredConstantsCUDACtx dvpGetRequiredConstantsCUDACtx;

extern PFN_dvpSyncObjClientWaitComplete dvpSyncObjClientWaitComplete;
extern PFN_dvpSyncObjCompletion dvpSyncObjCompletion;

#if defined(_WIN32)
extern PFN_dvpInitD3D11Device dvpInitD3D11Device;
extern PFN_dvpCloseD3D11Device dvpCloseD3D11Device;
extern PFN_dvpCreateGPUD3D11Resource dvpCreateGPUD3D11Resource;
extern PFN_dvpBindToD3D11Device dvpBindToD3D11Device;
extern PFN_dvpUnbindFromD3D11Device dvpUnbindFromD3D11Device;
extern PFN_dvpGetRequiredConstantsD3D11Device dvpGetRequiredConstantsD3D11Device;
#endif

/* ============================================================================
 * Runtime loader
 * ============================================================================ */

/** Try to load dvp.dll and resolve required GL + D3D11 entry points.
 *  Returns true on full success. On partial failure (e.g. dvp.dll loaded
 *  but a D3D11 symbol couldn't be resolved), the failed function pointer
 *  remains null and nv_dvp_get_runtime_error() reports which one. */
bool nv_dvp_load_runtime(void);

/** Are GL DVP entry points all resolved? Cheap; returns cached result. */
bool nv_dvp_have_gl(void);

/** Are D3D11 DVP entry points all resolved? Windows-only; always false
 *  on Linux. */
bool nv_dvp_have_d3d11(void);

/** Are CUDA DVP entry points all resolved? Cross-platform; both v1.70
 *  dvp.dll and libdvp.so.1 export the CUDA suite. */
bool nv_dvp_have_cuda(void);

/** Last loader-time error message, NUL-terminated. Empty if no error. */
const char* nv_dvp_get_runtime_error(void);

#ifdef __cplusplus
}
#endif
