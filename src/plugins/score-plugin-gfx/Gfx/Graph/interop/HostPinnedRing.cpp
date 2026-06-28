#include "HostPinnedRing.hpp"

#include <Gfx/Graph/interop/AmdPinnedBuffers.hpp>
#include <Gfx/Graph/interop/CudaFunctions.hpp>
#include <Gfx/Graph/interop/VideoPixelFormat.hpp>

#include <private/qrhi_p.h>

#include <QByteArray>
#include <QDebug>

#if QT_CONFIG(opengl)
  #include <QOpenGLContext>
  #include <QOpenGLExtraFunctions>
#endif

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

// nv_dvp_bridge C API. Pulled in by translation unit only; consumers
// of HostPinnedRing.hpp never see DVP types. The NV_DVP_BRIDGE_INLINE
// define comes from the bridge target's PUBLIC compile definitions.
#include <nv_dvp_bridge.h>

// D3D11 native handles (Windows only) — must come before qrhi.h for
// QRhiD3D11NativeHandles to be available.
#if defined(_WIN32)
  #include <d3d11.h>
#endif

namespace score::gfx::interop
{

namespace
{

// Stride comes from the shared interop::defaultStride(VideoPixelFormat) — the
// V210 (width+47)/48*128 rule and the packed RGB/UYVY rules already live there
// and produce identical values to the former local table.

NvDvpFormat toDvpFormat(VideoPixelFormat f) noexcept
{
  // DVP knows RGBA8 / BGRA8 only. Packed-YUV slots are passed as RGBA8 with a
  // stride that already encodes the packing; the decode shader downstream
  // unpacks. The DVP DMA doesn't care about pixel meaning, only
  // stride × height × bytes-per-pixel.
  return (f == VideoPixelFormat::BGRA8) ? NV_DVP_FORMAT_BGRA8
                                        : NV_DVP_FORMAT_RGBA8;
}

/* Pick the best backend supported by the live system. */
HostPinnedRingBackend pickBackend(const HostPinnedRingConfig& cfg) noexcept
{
  if(!cfg.caps || !cfg.rhi)
    return HostPinnedRingBackend::CpuStaging;

  if(cfg.caps->dvpLoaded)
  {
    switch(cfg.rhi->backend())
    {
      case QRhi::OpenGLES2:
        if(cfg.caps->dvpHaveGl)
          return HostPinnedRingBackend::Dvp;
        break;
      case QRhi::D3D11:
        if(cfg.caps->dvpHaveD3D11)
          return HostPinnedRingBackend::Dvp;
        break;
      default:
        break;
    }
  }

  if(cfg.caps->vendor == GpuVendor::Amd
     && cfg.rhi->backend() == QRhi::OpenGLES2
     && cfg.caps->hasTier2AmdPinned())
    return HostPinnedRingBackend::AmdPinned;

  if(cfg.caps->cudaLoaded
     && (cfg.caps->vendor == GpuVendor::NvidiaConsumer
         || cfg.caps->vendor == GpuVendor::NvidiaProQuadro))
    return HostPinnedRingBackend::CudaHostReg;

  return HostPinnedRingBackend::CpuStaging;
}

/* Extract the GL texture id from a QRhiTexture (when the QRhi backend
 * is GL). Returns 0 on failure. */
uint32_t glIdOf(QRhiTexture* tex) noexcept
{
  if(!tex)
    return 0;
  auto nat = tex->nativeTexture();
  return static_cast<uint32_t>(nat.object);
}

#if defined(_WIN32)
ID3D11Texture2D* d3d11TexOf(QRhiTexture* tex) noexcept
{
  if(!tex)
    return 0;
  auto nat = tex->nativeTexture();
  return reinterpret_cast<ID3D11Texture2D*>(static_cast<uintptr_t>(nat.object));
}
#endif

} // namespace

// =============================================================================
// Impl with per-backend state
// =============================================================================

struct HostPinnedRing::Impl
{
  HostPinnedRingConfig cfg{};
  HostPinnedRingBackend backend{HostPinnedRingBackend::None};
  std::vector<HostPinnedSlot> slots;
  std::size_t writeIndex{};
  uint32_t height{};
  std::size_t stride{};

  // -- DVP state -------------------------------------------------------
  NvDvpContextHandle dvpCtx{};
  std::vector<NvDvpResourceHandle> dvpSlotHandles;
  std::unordered_map<QRhiTexture*, NvDvpResourceHandle> dvpTextureHandles;

  // -- AMD-pinned state -----------------------------------------------
  AmdPinnedBuffers amd{};
  std::vector<unsigned int> amdSlotGlBuffers;

  // -- CudaHostReg state ----------------------------------------------
  score::gfx::CudaFunctions cu;
  bool cudaSlotsRegistered{false};
  // Per-slot device-side pointer obtained from cuMemHostGetDevicePointer
  // — points into the same registered host memory. Used as the source
  // for cuMemcpy2DAsync when uploading to a CUDA-bound destination.
  std::vector<CUdeviceptr> slotDevicePtrs;
  // Per-destination texture: { CUDA P2P context, target CUarray }.
  // Populated by bindCudaDestination(); when uploadSlotToTexture's
  // dst is in this map, we route via cuMemcpy2DAsync.
  struct CudaBinding
  {
    void* p2p_ctx{};       // CudaP2PContextHandle
    void* cuda_array{};    // CUarray
  };
  std::unordered_map<QRhiTexture*, CudaBinding> cudaDestinations;

  // -- Readback state (CPU + CudaHostReg) -----------------------------
  //
  // Triple-buffer-style: one persistent QRhiReadbackResult per slot,
  // allocated once in createSlots() and reused across frames. The
  // completion lambda captures `this` plus the slot index and writes
  // directly into `slots[i]`.
  //
  // Per-slot pending/ready atomics provide the synchronisation:
  //   - pending[i] = true between enqueue and completion. Re-enqueue
  //     on a slot whose pending flag is set returns false (caller
  //     should wait or rotate slots).
  //   - ready[i] = true once the completion lambda has copied bytes
  //     into slot.host. Vendor strategy polls slotReady() then
  //     resetSlotReady() after consuming.
  //
  // **Teardown contract**: caller must drain (wait until !any pending)
  // before destroying the ring. QRhi keeps a raw pointer to the
  // QRhiReadbackResult between batch submit and completion; freeing
  // the result while QRhi still holds the pointer is UB. destroy()
  // logs a warning if it sees pending readbacks — this is caller
  // error but better than silent UAF.
  struct ReadbackHandle
  {
    QRhiReadbackResult result{};
    std::atomic<bool> pending{false};
  };
  // unique_ptr<[]> rather than vector so the elements don't move on
  // resize (we hand QRhi raw pointers into the array).
  std::unique_ptr<ReadbackHandle[]> readbacks;
  std::unique_ptr<std::atomic<bool>[]> slotReadyFlags;

  // -- Lifecycle -------------------------------------------------------

  bool createSlots()
  {
    slots.resize(cfg.slotCount);
    for(auto& s : slots)
    {
      s.host = ::nv_dvp_aligned_alloc(static_cast<uint64_t>(stride) * height);
      if(!s.host)
        return false;
      s.size = std::size_t(stride) * height;
      s.stride = stride;
    }
    // Per-slot readiness flags. Async backends toggle these from the
    // QRhi completion callback; sync backends set them inline.
    slotReadyFlags = std::make_unique<std::atomic<bool>[]>(cfg.slotCount);
    for(uint32_t i = 0; i < cfg.slotCount; ++i)
      slotReadyFlags[i].store(false, std::memory_order_release);

    // Persistent readback handles, one per slot. Lambdas wired once;
    // they capture `this` and the slot index so the body can find the
    // slot/result/flags at completion time. The teardown contract
    // (drain before destroy) keeps `this` and `&readbacks[i]` valid.
    readbacks = std::make_unique<ReadbackHandle[]>(cfg.slotCount);
    for(uint32_t i = 0; i < cfg.slotCount; ++i)
    {
      auto* self = this;
      const std::size_t slotIdx = i;
      readbacks[i].result.completed = [self, slotIdx]() {
        self->onReadbackComplete(slotIdx);
      };
    }
    return true;
  }

  void freeSlots() noexcept
  {
    // Teardown contract: caller is expected to have drained pending
    // readbacks before reaching here. Warn if violated — destroying
    // QRhiReadbackResults while QRhi still holds raw pointers is UB.
    if(readbacks)
    {
      for(uint32_t i = 0; i < cfg.slotCount; ++i)
      {
        if(readbacks[i].pending.load(std::memory_order_acquire))
        {
          qWarning() << "HostPinnedRing: destroying with pending readback "
                        "for slot"
                     << int(i)
                     << "— caller violated drain contract; UB risk.";
        }
      }
    }
    readbacks.reset();
    slotReadyFlags.reset();

    for(auto& s : slots)
    {
      if(s.host)
        ::nv_dvp_aligned_free(s.host);
      s = {};
    }
    slots.clear();
  }

  /* QRhi completion callback for slot `i`. Copies the readback bytes
   * into the slot's host buffer, handles stride mismatch, then clears
   * pending and sets ready. */
  void onReadbackComplete(std::size_t i) noexcept
  {
    if(i >= slots.size() || !slotReadyFlags || !readbacks)
      return;
    auto& rb = readbacks[i];
    const QByteArray& bytes = rb.result.data;
    const int h = rb.result.pixelSize.height();
    auto& slot = slots[i];
    if(h > 0 && !bytes.isEmpty() && slot.host && slot.size > 0)
    {
      const std::size_t srcRowStride
          = static_cast<std::size_t>(bytes.size()) / std::size_t(h);
      if(srcRowStride == slot.stride
         && std::size_t(bytes.size()) <= slot.size)
      {
        std::memcpy(slot.host, bytes.constData(),
                    static_cast<std::size_t>(bytes.size()));
      }
      else
      {
        const std::size_t copyRowBytes
            = srcRowStride < slot.stride ? srcRowStride : slot.stride;
        auto* dst = static_cast<char*>(slot.host);
        const char* s = bytes.constData();
        for(int r = 0; r < h; ++r)
        {
          if(std::size_t(r + 1) * srcRowStride > std::size_t(bytes.size()))
            break;
          if(std::size_t(r + 1) * slot.stride > slot.size)
            break;
          std::memcpy(dst + std::size_t(r) * slot.stride,
                      s + std::size_t(r) * srcRowStride, copyRowBytes);
        }
      }
    }
    rb.pending.store(false, std::memory_order_release);
    slotReadyFlags[i].store(true, std::memory_order_release);
  }

  // -- CPU backend (already worked; preserved here) -------------------
  bool initCpuStaging() noexcept { return true; }

  // -- DVP backend ----------------------------------------------------

  bool initDvp() noexcept
  {
    const auto rhiBackend = cfg.rhi->backend();
    if(rhiBackend == QRhi::OpenGLES2)
    {
      // GL context must be current. Caller is responsible for ensuring
      // that — typically the QRhi render thread.
      if(::nv_dvp_init_gl(&dvpCtx) != NV_DVP_SUCCESS || !dvpCtx)
      {
        qWarning() << "HostPinnedRing: nv_dvp_init_gl failed";
        return false;
      }
    }
#if defined(_WIN32)
    else if(rhiBackend == QRhi::D3D11)
    {
      // Extract ID3D11Device* from QRhi's native handles.
      auto* nh
          = static_cast<const QRhiD3D11NativeHandles*>(cfg.rhi->nativeHandles());
      if(!nh || !nh->dev)
      {
        qWarning() << "HostPinnedRing: QRhi D3D11 native handles missing";
        return false;
      }
      if(::nv_dvp_init_d3d11(&dvpCtx, nh->dev) != NV_DVP_SUCCESS || !dvpCtx)
      {
        qWarning() << "HostPinnedRing: nv_dvp_init_d3d11 failed";
        return false;
      }
    }
#endif
    else
    {
      qWarning() << "HostPinnedRing: DVP backend not implemented for QRhi "
                    "backend"
                 << int(rhiBackend);
      return false;
    }

    // Register each slot's host buffer with DVP.
    dvpSlotHandles.resize(slots.size(), nullptr);
    const NvDvpFormat fmt = toDvpFormat(cfg.format);
    for(std::size_t i = 0; i < slots.size(); ++i)
    {
      NvDvpResourceHandle h{};
      if(::nv_dvp_register_sysmem_buffer(
             dvpCtx, slots[i].host, fmt, cfg.width, cfg.height,
             uint32_t(stride), &h)
             != NV_DVP_SUCCESS
         || !h)
      {
        qWarning() << "HostPinnedRing: nv_dvp_register_sysmem_buffer failed "
                      "for slot"
                   << int(i);
        return false;
      }
      dvpSlotHandles[i] = h;
      slots[i].backendOpaque = h;
    }
    return true;
  }

  /** Look up (or register-and-cache) the DVP handle for a destination
   *  texture. The cache key is the QRhiTexture pointer; assumes QRhi
   *  preserves texture identity across frames (which it does — the
   *  native handle may rebuild but the QRhiTexture* stays stable until
   *  the consumer calls destroy on it). */
  NvDvpResourceHandle dvpHandleFor(QRhiTexture* tex) noexcept
  {
    auto it = dvpTextureHandles.find(tex);
    if(it != dvpTextureHandles.end())
      return it->second;

    const NvDvpFormat fmt = toDvpFormat(cfg.format);
    NvDvpResourceHandle h{};
    int rc = 0;
    if(cfg.rhi->backend() == QRhi::OpenGLES2)
    {
      const uint32_t glId = glIdOf(tex);
      if(glId == 0)
        return nullptr;
      rc = ::nv_dvp_register_gl_texture(
          dvpCtx, glId, fmt, cfg.width, cfg.height, &h);
    }
#if defined(_WIN32)
    else if(cfg.rhi->backend() == QRhi::D3D11)
    {
      auto* d3d = d3d11TexOf(tex);
      if(!d3d)
        return nullptr;
      rc = ::nv_dvp_register_d3d11_texture(
          dvpCtx, d3d, fmt, cfg.width, cfg.height, &h);
    }
#endif
    else
    {
      return nullptr;
    }
    if(rc != NV_DVP_SUCCESS || !h)
    {
      qWarning() << "HostPinnedRing: DVP texture register failed";
      return nullptr;
    }
    dvpTextureHandles.emplace(tex, h);
    return h;
  }

  bool dvpUpload(std::size_t i, QRhiTexture* dst) noexcept
  {
    auto* texH = dvpHandleFor(dst);
    if(!texH)
      return false;
    // Release the texture for DVP DMA (signals API queue done writing).
    if(::nv_dvp_release_texture(dvpCtx, texH) != NV_DVP_SUCCESS)
      return false;
    // Run the DMA from the slot's sysmem buffer to the texture.
    if(::nv_dvp_copy_buffer_to_texture(dvpCtx, dvpSlotHandles[i], texH)
       != NV_DVP_SUCCESS)
      return false;
    // Re-acquire for the API queue.
    return ::nv_dvp_acquire_texture(dvpCtx, texH) == NV_DVP_SUCCESS;
  }

  bool dvpDownload(std::size_t i, QRhiTexture* src) noexcept
  {
    auto* texH = dvpHandleFor(src);
    if(!texH)
      return false;
    if(::nv_dvp_release_texture(dvpCtx, texH) != NV_DVP_SUCCESS)
      return false;
    if(::nv_dvp_copy_texture_to_buffer(dvpCtx, texH, dvpSlotHandles[i])
       != NV_DVP_SUCCESS)
      return false;
    return ::nv_dvp_acquire_texture(dvpCtx, texH) == NV_DVP_SUCCESS;
  }

  void destroyDvp() noexcept
  {
    if(!dvpCtx)
      return;
    for(auto& [tex, h] : dvpTextureHandles)
    {
      if(h)
        ::nv_dvp_unregister(dvpCtx, h);
    }
    dvpTextureHandles.clear();
    for(auto h : dvpSlotHandles)
    {
      if(h)
        ::nv_dvp_unregister(dvpCtx, h);
    }
    dvpSlotHandles.clear();
    ::nv_dvp_shutdown(dvpCtx);
    dvpCtx = nullptr;
  }

  // -- AMD-pinned backend ---------------------------------------------

#if QT_CONFIG(opengl)
  bool initAmdPinned() noexcept
  {
    if(!amd.tryInit())
    {
      qWarning() << "HostPinnedRing: AMD GL extensions not present";
      return false;
    }
    // For capture (sysmem → texture) we want UNPACK; for output PACK.
    const unsigned int glTarget = cfg.direction
                                          == HostPinnedDirection::CaptureToTexture
                                      ? 0x88EC /*GL_PIXEL_UNPACK_BUFFER*/
                                      : 0x88EB /*GL_PIXEL_PACK_BUFFER*/;
    const unsigned int glUsage = cfg.direction
                                         == HostPinnedDirection::CaptureToTexture
                                     ? 0x88E8 /*GL_DYNAMIC_DRAW*/
                                     : 0x88E9 /*GL_DYNAMIC_READ*/;
    amdSlotGlBuffers.resize(slots.size(), 0);
    for(std::size_t i = 0; i < slots.size(); ++i)
    {
      const unsigned int buf = amd.createPinnedBuffer(
          glTarget, slots[i].size, slots[i].host, glUsage);
      if(buf == 0)
      {
        qWarning() << "HostPinnedRing: AmdPinnedBuffers.createPinnedBuffer "
                      "failed for slot"
                   << int(i);
        return false;
      }
      amdSlotGlBuffers[i] = buf;
      slots[i].backendOpaque
          = reinterpret_cast<void*>(static_cast<uintptr_t>(buf));
    }
    return true;
  }

  /* AMD path: bind the slot's pinned GL buffer to PIXEL_UNPACK and
   * issue glTexSubImage2D against the destination texture's GL id.
   * The GL driver page-locks the host memory and DMAs from it. */
  bool amdUpload(std::size_t i, QRhiTexture* dst) noexcept
  {
    auto* ctx = QOpenGLContext::currentContext();
    if(!ctx)
      return false;
    auto* funcs = ctx->extraFunctions();
    if(!funcs)
      return false;

    const uint32_t texId = glIdOf(dst);
    if(texId == 0)
      return false;

    // GL token values — local constants to avoid pulling in <GL/gl.h>
    // here (which is already indirectly included via QOpenGLExtraFunctions
    // and may have the macros defined). Names are deliberately suffixed
    // with _v to avoid colliding with the system macros.
    constexpr unsigned int GL_PIXEL_UNPACK_BUFFER_v = 0x88EC;
    constexpr unsigned int GL_TEXTURE_2D_v = 0x0DE1;
    constexpr unsigned int GL_RGBA_v = 0x1908;
    constexpr unsigned int GL_BGRA_v = 0x80E1;
    constexpr unsigned int GL_UNSIGNED_BYTE_v = 0x1401;
    const unsigned int glFmt
        = (cfg.format == VideoPixelFormat::BGRA8) ? GL_BGRA_v : GL_RGBA_v;

    funcs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER_v, amdSlotGlBuffers[i]);
    funcs->glBindTexture(GL_TEXTURE_2D_v, texId);
    funcs->glTexSubImage2D(
        GL_TEXTURE_2D_v, 0, 0, 0, int(cfg.width), int(cfg.height), glFmt,
        GL_UNSIGNED_BYTE_v, nullptr); // offset 0 reads from PUB
    funcs->glBindTexture(GL_TEXTURE_2D_v, 0);
    funcs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER_v, 0);
    return true;
  }

  /* AMD download path: bind slot's pinned GL buffer as PACK, glGetTexImage
   * the source texture into it. The driver DMAs into the pinned host
   * pointer. */
  bool amdDownload(std::size_t i, QRhiTexture* src) noexcept
  {
    auto* ctx = QOpenGLContext::currentContext();
    if(!ctx)
      return false;
    auto* funcs = ctx->extraFunctions();
    if(!funcs)
      return false;

    const uint32_t texId = glIdOf(src);
    if(texId == 0)
      return false;

    constexpr unsigned int GL_PIXEL_PACK_BUFFER_v = 0x88EB;
    constexpr unsigned int GL_TEXTURE_2D_v = 0x0DE1;
    constexpr unsigned int GL_RGBA_v = 0x1908;
    constexpr unsigned int GL_BGRA_v = 0x80E1;
    constexpr unsigned int GL_UNSIGNED_BYTE_v = 0x1401;
    const unsigned int glFmt
        = (cfg.format == VideoPixelFormat::BGRA8) ? GL_BGRA_v : GL_RGBA_v;

    funcs->glBindBuffer(GL_PIXEL_PACK_BUFFER_v, amdSlotGlBuffers[i]);
    funcs->glBindTexture(GL_TEXTURE_2D_v, texId);
    // QOpenGLExtraFunctions doesn't expose glGetTexImage (it's not in
    // GLES). Fall back to glGetTexImage via direct resolution; this
    // path is desktop-GL only anyway since AMD-pinned needs desktop GL.
    using FN_glGetTexImage
        = void(*)(unsigned int, int, unsigned int, unsigned int, void*);
    auto pfn = reinterpret_cast<FN_glGetTexImage>(
        ctx->getProcAddress("glGetTexImage"));
    if(pfn)
      pfn(GL_TEXTURE_2D_v, 0, glFmt, GL_UNSIGNED_BYTE_v, nullptr);
    funcs->glBindTexture(GL_TEXTURE_2D_v, 0);
    funcs->glBindBuffer(GL_PIXEL_PACK_BUFFER_v, 0);
    return pfn != nullptr;
  }

  void destroyAmdPinned() noexcept
  {
    if(amdSlotGlBuffers.empty())
      return;
    if(auto* ctx = QOpenGLContext::currentContext())
    {
      if(auto* funcs = ctx->extraFunctions())
      {
        funcs->glDeleteBuffers(
            int(amdSlotGlBuffers.size()), amdSlotGlBuffers.data());
      }
    }
    amdSlotGlBuffers.clear();
  }
#else
  bool initAmdPinned() noexcept { return false; }
  bool amdUpload(std::size_t, QRhiTexture*) noexcept { return false; }
  bool amdDownload(std::size_t, QRhiTexture*) noexcept { return false; }
  void destroyAmdPinned() noexcept {}
#endif

  // -- CudaHostReg backend --------------------------------------------
  //
  // Page-locks each slot via cuMemHostRegister. The QRhi-side transfer
  // is still per-backend: for a CUDA-imported destination (Vulkan QRhi
  // with cuda_p2p_import_vulkan_image), the caller-supplied import
  // gives us a CUarray to cuMemcpy2D into. For other QRhi backends the
  // page-locking is "preparation" only — the actual upload routes
  // through CPU staging since there's no cheap path from registered
  // host memory to a non-CUDA QRhi texture without an intermediate
  // copy. We honour that by demoting upload to CPU and exposing the
  // pinning as a perf-only benefit.

  bool initCudaHostReg() noexcept
  {
    if(!cu.load())
    {
      qWarning() << "HostPinnedRing: CudaFunctions.load failed";
      return false;
    }
    // cuInit(0) is idempotent across multiple consumers of CudaFunctions
    // in the same process.
    cu.init(0);

    constexpr unsigned int CU_MEMHOSTREGISTER_PORTABLE = 0x01;
    constexpr unsigned int CU_MEMHOSTREGISTER_DEVICEMAP = 0x02;
    using FN_cuMemHostRegister
        = CUresult (*)(void*, std::size_t, unsigned int);

    // cuMemHostRegister isn't in the curated CudaFunctions struct
    // (it's used only by this backend). Resolve directly.
#if defined(_WIN32)
    auto pfn = reinterpret_cast<FN_cuMemHostRegister>(
        (void*)GetProcAddress((HMODULE)cu.lib, "cuMemHostRegister"));
#else
    auto pfn
        = reinterpret_cast<FN_cuMemHostRegister>(dlsym(cu.lib, "cuMemHostRegister"));
#endif
    if(!pfn)
    {
      qWarning() << "HostPinnedRing: cuMemHostRegister unresolved";
      return false;
    }
    for(auto& s : slots)
    {
      // PORTABLE | DEVICEMAP — readable from CUDA streams on any
      // device + addressable via cuMemHostGetDevicePointer.
      const unsigned int flags
          = CU_MEMHOSTREGISTER_PORTABLE | CU_MEMHOSTREGISTER_DEVICEMAP;
      if(pfn(s.host, s.size, flags) != CUDA_SUCCESS)
      {
        qWarning() << "HostPinnedRing: cuMemHostRegister failed";
        return false;
      }
    }
    cudaSlotsRegistered = true;

    // Resolve the device-pointer accessor. Each slot's page-locked
    // host pointer is mapped into the CUDA address space; we use the
    // resulting CUdeviceptr as the source for cuMemcpy2DAsync.
    using FN_cuMemHostGetDevicePointer
        = CUresult (*)(CUdeviceptr*, void*, unsigned int);
#if defined(_WIN32)
    auto getDevPtr = reinterpret_cast<FN_cuMemHostGetDevicePointer>(
        (void*)GetProcAddress((HMODULE)cu.lib, "cuMemHostGetDevicePointer_v2"));
#else
    auto getDevPtr = reinterpret_cast<FN_cuMemHostGetDevicePointer>(
        dlsym(cu.lib, "cuMemHostGetDevicePointer_v2"));
#endif
    if(getDevPtr)
    {
      slotDevicePtrs.resize(slots.size(), 0);
      for(std::size_t i = 0; i < slots.size(); ++i)
      {
        CUdeviceptr dp{};
        if(getDevPtr(&dp, slots[i].host, 0) == CUDA_SUCCESS)
          slotDevicePtrs[i] = dp;
      }
    }
    // If getDevPtr unavailable, slotDevicePtrs stays empty and the
    // CUDA-direct upload path won't engage — falls back to QRhi
    // staging like the CPU backend. Still functional, just slower.
    return true;
  }

  /* CudaHostReg upload. Two paths:
   *
   *   1. Zero-copy direct: when the caller has bound a CUDA-imported
   *      destination via bindCudaDestination(), do a cuMemcpy2DAsync
   *      from the slot's (registered, device-mapped) host pointer
   *      straight to the CUarray. The Vulkan/QRhi side sees the
   *      updated VkImage on its next bind — no pixel copy through CPU
   *      or QRhi staging.
   *
   *   2. Fallback: same as CPU backend — QRhi uploadTexture from the
   *      page-locked slot. The pinning is perf-neutral here (the GL
   *      driver doesn't recognise CUDA-page-locked memory as faster).
   *      Acceptable as a transparent fallback when CUDA-imported
   *      destinations aren't wired. */
  bool cudaHostRegUpload(std::size_t i, QRhiTexture* dst,
                        QRhiResourceUpdateBatch* batch) noexcept
  {
    // Look up CUDA binding for this destination.
    auto it = cudaDestinations.find(dst);
    if(it != cudaDestinations.end() && it->second.cuda_array
       && i < slotDevicePtrs.size() && slotDevicePtrs[i] != 0
       && cu.loaded() && cu.memcpy2DAsync)
    {
      // Zero-copy CUDA path. The CudaP2P bridge manages its own
      // CUstream + semaphore handshake; we just hand it a CUDA_MEMCPY2D
      // describing source (registered host → device-mapped CUdeviceptr)
      // and destination (CUarray).
      CUDA_MEMCPY2D op{};
      op.srcMemoryType = CU_MEMORYTYPE_DEVICE;
      op.srcDevice = slotDevicePtrs[i];
      op.srcPitch = slots[i].stride;
      op.dstMemoryType = CU_MEMORYTYPE_ARRAY;
      op.dstArray = static_cast<CUarray>(it->second.cuda_array);
      op.WidthInBytes = slots[i].stride;
      op.Height = height;
      // Stream=null (default stream). CudaP2PBridge consumers can
      // wrap the call with their own stream + sync if they need
      // pipelining; for the typical use case (single render thread)
      // null-stream serial is correct.
      if(cu.memcpy2DAsync(&op, nullptr) == CUDA_SUCCESS)
        return true;
      qWarning() << "HostPinnedRing: cuMemcpy2DAsync failed — falling "
                    "back to QRhi staging";
      // fall through to staging path
    }

    // QRhi staging path (no CUDA binding, or memcpy2D failed).
    if(!batch)
      return false;
    const auto& s = slots[i];
    QRhiTextureSubresourceUploadDescription sub;
    sub.setData(QByteArray::fromRawData(
        static_cast<const char*>(s.host), int(s.size)));
    sub.setDataStride(quint32(s.stride));
    QRhiTextureUploadEntry entry{0, 0, sub};
    QRhiTextureUploadDescription upload({entry});
    batch->uploadTexture(dst, upload);
    return true;
  }

  bool cudaHostRegDownload(std::size_t i, QRhiTexture* src,
                          QRhiResourceUpdateBatch* batch) noexcept
  {
    // CudaHostReg's pinning doesn't help texture→host without a CUDA-
    // imported source texture (which the consumer would have to set up
    // separately). For the moment, route through the same QRhi readback
    // path as the CPU backend — the pinning is a future perf hook for
    // a CUDA-aware consumer that reads from slot.host straight into a
    // CUDA stream.
    return cpuReadback(i, src, batch);
  }

  /* CPU / CudaHostReg readback. Reuses the persistent slot's
   * QRhiReadbackResult; copy + flag-flip happen in onReadbackComplete().
   *
   * Returns false if a previous readback for the same slot hasn't
   * completed (`pending[i] == true`) — caller should wait on slotReady
   * or rotate to a different slot. */
  bool cpuReadback(std::size_t i, QRhiTexture* src,
                  QRhiResourceUpdateBatch* batch) noexcept
  {
    if(!batch || i >= slots.size() || !readbacks)
      return false;

    auto& rb = readbacks[i];
    bool expected = false;
    if(!rb.pending.compare_exchange_strong(
           expected, true, std::memory_order_acq_rel,
           std::memory_order_acquire))
    {
      // Already in flight; caller must drain before re-issuing.
      return false;
    }

    // Clear any stale data from the previous use; QRhi will overwrite
    // but explicit reset makes debugging easier.
    rb.result.data.clear();
    rb.result.pixelSize = {};

    QRhiReadbackDescription desc(src);
    batch->readBackTexture(desc, &rb.result);
    return true;
  }

  void destroyCudaHostReg() noexcept
  {
    if(!cudaSlotsRegistered || !cu.loaded())
      return;
    using FN_cuMemHostUnregister = CUresult (*)(void*);
#if defined(_WIN32)
    auto pfn = reinterpret_cast<FN_cuMemHostUnregister>(
        (void*)GetProcAddress((HMODULE)cu.lib, "cuMemHostUnregister"));
#else
    auto pfn = reinterpret_cast<FN_cuMemHostUnregister>(
        dlsym(cu.lib, "cuMemHostUnregister"));
#endif
    if(pfn)
    {
      for(auto& s : slots)
      {
        if(s.host)
          pfn(s.host);
      }
    }
    cudaSlotsRegistered = false;
  }
};

HostPinnedRing::HostPinnedRing() noexcept = default;
HostPinnedRing::~HostPinnedRing() { destroy(); }

HostPinnedRing::HostPinnedRing(HostPinnedRing&&) noexcept = default;
HostPinnedRing& HostPinnedRing::operator=(HostPinnedRing&&) noexcept = default;

bool HostPinnedRing::create(const HostPinnedRingConfig& cfg)
{
  destroy();
  if(!cfg.rhi || cfg.width == 0 || cfg.height == 0 || cfg.slotCount == 0)
    return false;

  m_impl = std::make_unique<Impl>();
  m_impl->cfg = cfg;
  m_impl->height = cfg.height;
  m_impl->stride
      = cfg.stride > 0 ? cfg.stride : defaultStride(cfg.format, cfg.width);

  if(!m_impl->createSlots())
  {
    destroy();
    return false;
  }

  m_impl->backend = pickBackend(cfg);
  bool ok = false;
  switch(m_impl->backend)
  {
    case HostPinnedRingBackend::Dvp:
      ok = m_impl->initDvp();
      if(!ok)
      {
        m_impl->destroyDvp();
        // Demote to CPU on failure rather than rejecting the create.
        m_impl->backend = HostPinnedRingBackend::CpuStaging;
        ok = m_impl->initCpuStaging();
      }
      break;
    case HostPinnedRingBackend::AmdPinned:
      ok = m_impl->initAmdPinned();
      if(!ok)
      {
        m_impl->destroyAmdPinned();
        m_impl->backend = HostPinnedRingBackend::CpuStaging;
        ok = m_impl->initCpuStaging();
      }
      break;
    case HostPinnedRingBackend::CudaHostReg:
      ok = m_impl->initCudaHostReg();
      if(!ok)
      {
        m_impl->destroyCudaHostReg();
        m_impl->backend = HostPinnedRingBackend::CpuStaging;
        ok = m_impl->initCpuStaging();
      }
      break;
    case HostPinnedRingBackend::CpuStaging:
      ok = m_impl->initCpuStaging();
      break;
    case HostPinnedRingBackend::None:
      ok = false;
      break;
  }

  if(!ok)
  {
    destroy();
    return false;
  }
  return true;
}

void HostPinnedRing::destroy() noexcept
{
  if(!m_impl)
    return;
  // Backend-specific cleanup BEFORE freeing the host buffers, since
  // some backends hold references to them.
  m_impl->destroyDvp();
  m_impl->destroyAmdPinned();
  m_impl->destroyCudaHostReg();
  m_impl->freeSlots();
  m_impl.reset();
}

bool HostPinnedRing::valid() const noexcept
{
  return m_impl && !m_impl->slots.empty();
}

HostPinnedRingBackend HostPinnedRing::backend() const noexcept
{
  return m_impl ? m_impl->backend : HostPinnedRingBackend::None;
}

std::size_t HostPinnedRing::slotCount() const noexcept
{
  return m_impl ? m_impl->slots.size() : 0;
}

HostPinnedSlot& HostPinnedRing::slot(std::size_t i) noexcept
{
  return m_impl->slots[i];
}

const HostPinnedSlot& HostPinnedRing::slot(std::size_t i) const noexcept
{
  return m_impl->slots[i];
}

std::size_t HostPinnedRing::writeIndex() const noexcept
{
  return m_impl ? m_impl->writeIndex : 0;
}

std::size_t HostPinnedRing::advance() noexcept
{
  if(!m_impl || m_impl->slots.empty())
    return 0;
  m_impl->writeIndex = (m_impl->writeIndex + 1) % m_impl->slots.size();
  return m_impl->writeIndex;
}

bool HostPinnedRing::uploadSlotToTexture(
    std::size_t i, QRhiTexture* dst, QRhiResourceUpdateBatch* batch)
{
  if(!valid() || !dst || i >= m_impl->slots.size())
    return false;

  switch(m_impl->backend)
  {
    case HostPinnedRingBackend::Dvp:
      return m_impl->dvpUpload(i, dst);
    case HostPinnedRingBackend::AmdPinned:
      return m_impl->amdUpload(i, dst);
    case HostPinnedRingBackend::CudaHostReg:
      return m_impl->cudaHostRegUpload(i, dst, batch);
    case HostPinnedRingBackend::CpuStaging:
    {
      if(!batch)
        return false;
      const auto& s = m_impl->slots[i];
      QRhiTextureSubresourceUploadDescription sub;
      sub.setData(QByteArray::fromRawData(
          static_cast<const char*>(s.host), int(s.size)));
      sub.setDataStride(quint32(s.stride));
      QRhiTextureUploadEntry entry{0, 0, sub};
      QRhiTextureUploadDescription upload({entry});
      batch->uploadTexture(dst, upload);
      return true;
    }
    case HostPinnedRingBackend::None:
      return false;
  }
  return false;
}

bool HostPinnedRing::downloadTextureToSlot(
    QRhiTexture* src, std::size_t i, QRhiResourceUpdateBatch* batch)
{
  if(!valid() || !src || i >= m_impl->slots.size())
    return false;

  // Sync backends own their own DMA completion: mark the slot ready
  // inline on success so the polling caller sees it immediately.
  auto markReady = [this, i]() {
    m_impl->slotReadyFlags[i].store(true, std::memory_order_release);
  };

  switch(m_impl->backend)
  {
    case HostPinnedRingBackend::Dvp:
    {
      m_impl->slotReadyFlags[i].store(false, std::memory_order_release);
      if(m_impl->dvpDownload(i, src))
      {
        markReady();
        return true;
      }
      return false;
    }
    case HostPinnedRingBackend::AmdPinned:
    {
      m_impl->slotReadyFlags[i].store(false, std::memory_order_release);
      if(m_impl->amdDownload(i, src))
      {
        markReady();
        return true;
      }
      return false;
    }
    case HostPinnedRingBackend::CudaHostReg:
      m_impl->slotReadyFlags[i].store(false, std::memory_order_release);
      return m_impl->cudaHostRegDownload(i, src, batch);
    case HostPinnedRingBackend::CpuStaging:
      m_impl->slotReadyFlags[i].store(false, std::memory_order_release);
      return m_impl->cpuReadback(i, src, batch);
    case HostPinnedRingBackend::None:
      return false;
  }
  return false;
}

bool HostPinnedRing::slotReady(std::size_t i) const noexcept
{
  if(!m_impl || !m_impl->slotReadyFlags || i >= m_impl->slots.size())
    return false;
  return m_impl->slotReadyFlags[i].load(std::memory_order_acquire);
}

void HostPinnedRing::resetSlotReady(std::size_t i) noexcept
{
  if(!m_impl || !m_impl->slotReadyFlags || i >= m_impl->slots.size())
    return;
  m_impl->slotReadyFlags[i].store(false, std::memory_order_release);
}

void HostPinnedRing::bindCudaDestination(
    QRhiTexture* dst, void* cuda_ctx, void* cuda_array) noexcept
{
  if(!m_impl || !dst)
    return;
  if(cuda_array == nullptr)
  {
    m_impl->cudaDestinations.erase(dst);
    return;
  }
  m_impl->cudaDestinations[dst]
      = Impl::CudaBinding{cuda_ctx, cuda_array};
}

} // namespace score::gfx::interop
