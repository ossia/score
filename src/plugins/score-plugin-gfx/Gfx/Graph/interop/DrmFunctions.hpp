#pragma once

/**
 * @file DrmFunctions.hpp
 * @brief dlopen'd libdrm entry points, so KMS output adds no link-time dep.
 *
 * Same shape as CudaFunctions / GbmDmaBufExport: score must build and run on a
 * machine with no libdrm at all, and a missing library has to degrade the
 * output rung rather than fail to load the plugin. Only the symbols the KMS
 * device actually calls are resolved; `load()` returns false if any is absent,
 * so a partial libdrm is caught at init instead of at the first flip.
 *
 * The public libdrm structs are ABI-stable and come from the headers; only the
 * functions are late-bound.
 */

#include <ossia/detail/dylib_loader.hpp>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include <optional>

namespace score::gfx::drm
{

struct DrmFunctions
{
  using FN_drmGetVersion = drmVersionPtr (*)(int);
  using FN_drmFreeVersion = void (*)(drmVersionPtr);
  using FN_drmSetClientCap = int (*)(int, uint64_t, uint64_t);
  using FN_drmIsMaster = int (*)(int);
  using FN_drmSetMaster = int (*)(int);
  using FN_drmDropMaster = int (*)(int);
  using FN_drmPrimeFDToHandle = int (*)(int, int, uint32_t*);
  using FN_drmHandleEvent = int (*)(int, drmEventContextPtr);

  using FN_drmModeGetResources = drmModeResPtr (*)(int);
  using FN_drmModeFreeResources = void (*)(drmModeResPtr);
  using FN_drmModeGetConnector = drmModeConnectorPtr (*)(int, uint32_t);
  using FN_drmModeFreeConnector = void (*)(drmModeConnectorPtr);
  using FN_drmModeGetEncoder = drmModeEncoderPtr (*)(int, uint32_t);
  using FN_drmModeFreeEncoder = void (*)(drmModeEncoderPtr);
  using FN_drmModeGetCrtc = drmModeCrtcPtr (*)(int, uint32_t);
  using FN_drmModeFreeCrtc = void (*)(drmModeCrtcPtr);
  using FN_drmModeSetCrtc
      = int (*)(int, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t*, int,
                drmModeModeInfoPtr);
  using FN_drmModeGetPlaneResources = drmModePlaneResPtr (*)(int);
  using FN_drmModeFreePlaneResources = void (*)(drmModePlaneResPtr);
  using FN_drmModeGetPlane = drmModePlanePtr (*)(int, uint32_t);
  using FN_drmModeFreePlane = void (*)(drmModePlanePtr);
  using FN_drmModeObjectGetProperties
      = drmModeObjectPropertiesPtr (*)(int, uint32_t, uint32_t);
  using FN_drmModeFreeObjectProperties = void (*)(drmModeObjectPropertiesPtr);
  using FN_drmModeGetProperty = drmModePropertyPtr (*)(int, uint32_t);
  using FN_drmModeFreeProperty = void (*)(drmModePropertyPtr);
  using FN_drmModeGetPropertyBlob = drmModePropertyBlobPtr (*)(int, uint32_t);
  using FN_drmModeFreePropertyBlob = void (*)(drmModePropertyBlobPtr);
  using FN_drmModeAddFB2WithModifiers
      = int (*)(int, uint32_t, uint32_t, uint32_t, const uint32_t*,
                const uint32_t*, const uint32_t*, const uint64_t*, uint32_t*,
                uint32_t);
  using FN_drmModeRmFB = int (*)(int, uint32_t);
  using FN_drmModePageFlip = int (*)(int, uint32_t, uint32_t, uint32_t, void*);
  using FN_drmModeCreateDumbBuffer
      = int (*)(int, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t*,
                uint32_t*, uint64_t*);
  using FN_drmModeMapDumbBuffer = int (*)(int, uint32_t, uint64_t*);
  using FN_drmModeDestroyDumbBuffer = int (*)(int, uint32_t);
  using FN_drmModeAddFB
      = int (*)(int, uint32_t, uint32_t, uint8_t, uint8_t, uint32_t, uint32_t,
                uint32_t*);

  // Atomic modesetting: the only way to change several objects in one
  // tear-free commit, and the only way to drive a writeback connector.
  using FN_drmModeAtomicAlloc = drmModeAtomicReqPtr (*)(void);
  using FN_drmModeAtomicFree = void (*)(drmModeAtomicReqPtr);
  using FN_drmModeAtomicAddProperty
      = int (*)(drmModeAtomicReqPtr, uint32_t, uint32_t, uint64_t);
  using FN_drmModeAtomicCommit = int (*)(int, drmModeAtomicReqPtr, uint32_t, void*);
  using FN_drmModeCreatePropertyBlob
      = int (*)(int, const void*, size_t, uint32_t*);
  using FN_drmModeDestroyPropertyBlob = int (*)(int, uint32_t);

  FN_drmGetVersion getVersion{};
  FN_drmFreeVersion freeVersion{};
  FN_drmSetClientCap setClientCap{};
  FN_drmIsMaster isMaster{};
  FN_drmSetMaster setMaster{};
  FN_drmDropMaster dropMaster{};
  FN_drmPrimeFDToHandle primeFDToHandle{};
  FN_drmHandleEvent handleEvent{};

  FN_drmModeGetResources getResources{};
  FN_drmModeFreeResources freeResources{};
  FN_drmModeGetConnector getConnector{};
  FN_drmModeFreeConnector freeConnector{};
  FN_drmModeGetEncoder getEncoder{};
  FN_drmModeFreeEncoder freeEncoder{};
  FN_drmModeGetCrtc getCrtc{};
  FN_drmModeFreeCrtc freeCrtc{};
  FN_drmModeSetCrtc setCrtc{};
  FN_drmModeGetPlaneResources getPlaneResources{};
  FN_drmModeFreePlaneResources freePlaneResources{};
  FN_drmModeGetPlane getPlane{};
  FN_drmModeFreePlane freePlane{};
  FN_drmModeObjectGetProperties objectGetProperties{};
  FN_drmModeFreeObjectProperties freeObjectProperties{};
  FN_drmModeGetProperty getProperty{};
  FN_drmModeFreeProperty freeProperty{};
  FN_drmModeGetPropertyBlob getPropertyBlob{};
  FN_drmModeFreePropertyBlob freePropertyBlob{};
  FN_drmModeAddFB2WithModifiers addFB2WithModifiers{};
  FN_drmModeRmFB rmFB{};
  FN_drmModePageFlip pageFlip{};
  FN_drmModeCreateDumbBuffer createDumb{};
  FN_drmModeMapDumbBuffer mapDumb{};
  FN_drmModeDestroyDumbBuffer destroyDumb{};
  FN_drmModeAddFB addFB{};
  FN_drmModeAtomicAlloc atomicAlloc{};
  FN_drmModeAtomicFree atomicFree{};
  FN_drmModeAtomicAddProperty atomicAddProperty{};
  FN_drmModeAtomicCommit atomicCommit{};
  FN_drmModeCreatePropertyBlob createPropertyBlob{};
  FN_drmModeDestroyPropertyBlob destroyPropertyBlob{};

  bool available() const noexcept { return m_lib.has_value(); }

  bool load() noexcept
  {
    if(m_lib)
      return true;
    try
    {
      m_lib.emplace(std::vector<std::string_view>{"libdrm.so.2", "libdrm.so"});
    }
    catch(...)
    {
      return false;
    }

    bool ok = true;
    auto sym = [&]<typename T>(T& out, const char* name) {
      out = m_lib->symbol<T>(name);
      if(!out)
        ok = false;
    };

    sym(getVersion, "drmGetVersion");
    sym(freeVersion, "drmFreeVersion");
    sym(setClientCap, "drmSetClientCap");
    sym(isMaster, "drmIsMaster");
    sym(setMaster, "drmSetMaster");
    sym(dropMaster, "drmDropMaster");
    sym(primeFDToHandle, "drmPrimeFDToHandle");
    sym(handleEvent, "drmHandleEvent");

    sym(getResources, "drmModeGetResources");
    sym(freeResources, "drmModeFreeResources");
    sym(getConnector, "drmModeGetConnector");
    sym(freeConnector, "drmModeFreeConnector");
    sym(getEncoder, "drmModeGetEncoder");
    sym(freeEncoder, "drmModeFreeEncoder");
    sym(getCrtc, "drmModeGetCrtc");
    sym(freeCrtc, "drmModeFreeCrtc");
    sym(setCrtc, "drmModeSetCrtc");
    sym(getPlaneResources, "drmModeGetPlaneResources");
    sym(freePlaneResources, "drmModeFreePlaneResources");
    sym(getPlane, "drmModeGetPlane");
    sym(freePlane, "drmModeFreePlane");
    sym(objectGetProperties, "drmModeObjectGetProperties");
    sym(freeObjectProperties, "drmModeFreeObjectProperties");
    sym(getProperty, "drmModeGetProperty");
    sym(freeProperty, "drmModeFreeProperty");
    sym(getPropertyBlob, "drmModeGetPropertyBlob");
    sym(freePropertyBlob, "drmModeFreePropertyBlob");
    sym(addFB2WithModifiers, "drmModeAddFB2WithModifiers");
    sym(rmFB, "drmModeRmFB");
    sym(pageFlip, "drmModePageFlip");
    sym(createDumb, "drmModeCreateDumbBuffer");
    sym(mapDumb, "drmModeMapDumbBuffer");
    sym(destroyDumb, "drmModeDestroyDumbBuffer");
    sym(addFB, "drmModeAddFB");
    sym(atomicAlloc, "drmModeAtomicAlloc");
    sym(atomicFree, "drmModeAtomicFree");
    sym(atomicAddProperty, "drmModeAtomicAddProperty");
    sym(atomicCommit, "drmModeAtomicCommit");
    sym(createPropertyBlob, "drmModeCreatePropertyBlob");
    sym(destroyPropertyBlob, "drmModeDestroyPropertyBlob");

    if(!ok)
      m_lib.reset();
    return ok;
  }

private:
  std::optional<ossia::dylib_loader> m_lib;
};

} // namespace score::gfx::drm
