#pragma once

/**
 * @file AVHWFrameToQRhi.hpp
 * @brief Small inline helpers for the AVFrame opaque → QRhiTexture pattern.
 *
 * Every hardware decoder in `Gfx/Graph/decoders/` follows the same outer
 * shape:
 *   1. Extract a native API handle from `AVFrame::data[*]` (per pixfmt).
 *   2. Do API-specific transformation (CopySubresourceRegion, av_hwframe_map,
 *      cuMemcpy2DAsync, CVMetalTextureCacheCreateTextureFromImage, ...).
 *   3. Wrap the resulting native handle in a non-owning `QRhiTexture` via
 *      `newTexture + createFrom`.
 *
 * Steps 1 and 3 are the parts that recur identically across all six
 * decoders. This header standardizes them as inline helpers so the
 * decoders converge on the same shape and any future signature change in
 * QRhi::createFrom is a one-place edit.
 *
 * Step 2 stays per-decoder — it's irreducibly API-specific.
 *
 * Existing decoders (`HWD3D11.hpp`, `HWD3D12.hpp`, `HWCUDA.hpp`,
 * `HWVulkan.hpp`, `HWVAAPI.hpp`, `HWVideoToolbox.hpp`) are not migrated
 * by this change.
 */

#include <QtGui/private/qrhi_p.h>

#include <cstdint>
#include <memory>

extern "C" {
#include <libavutil/frame.h>
}

namespace score::gfx::interop
{

/**
 * @brief Wrap a native graphics-API handle in a non-owning QRhiTexture.
 *
 * @param rhi          the target QRhi.
 * @param format       texture format the consumer will sample with.
 * @param size         logical dimensions.
 * @param nativeHandle e.g. (quint64)ID3D11Texture2D*, (quint64)VkImage,
 *                     (quint64)id<MTLTexture>, (quint64)ID3D12Resource*.
 * @param layout       Vulkan: VkImageLayout. D3D11/D3D12/Metal: pass 0.
 * @param flags        additional QRhiTexture::Flags (e.g. RenderTarget).
 *
 * Returns a freshly-created QRhiTexture wrapping `nativeHandle`. The
 * caller is responsible for the underlying native resource's lifetime —
 * QRhiTexture::createFrom is non-owning.
 */
inline std::unique_ptr<QRhiTexture> wrapNativeTexture(
    QRhi& rhi,
    QRhiTexture::Format format,
    QSize size,
    quint64 nativeHandle,
    int layout = 0,
    QRhiTexture::Flags flags = {}) noexcept
{
  auto tex = std::unique_ptr<QRhiTexture>(rhi.newTexture(format, size, 1, flags));
  if(!tex)
    return nullptr;
  // createFrom replaces the QRhi-owned native handle with `nativeHandle`.
  // No explicit create() call: createFrom() is the constructor here.
  if(!tex->createFrom({nativeHandle, layout}))
    return nullptr;
  return tex;
}

// =============================================================================
// AVFrame opaque extractors — type-safe wrappers around frame.data[i].
//
// Existing decoders open-code these as `(Type*)frame.data[i]` casts; using
// these helpers makes the intent obvious at the call site and centralizes
// the data[i] / linesize[i] index conventions per pixel format. The
// conventions are documented in FFmpeg's pixfmt.h for each AV_PIX_FMT_*.
// =============================================================================

/** @brief D3D11VA: data[0] = ID3D11Texture2D*, data[1] = array index. */
struct AVFrameD3D11
{
  void* texture;     // ID3D11Texture2D*
  uint32_t subresource;
};

inline AVFrameD3D11 avframeD3D11(const AVFrame& f) noexcept
{
  return {
      f.data[0],
      static_cast<uint32_t>(reinterpret_cast<intptr_t>(f.data[1]))};
}

/** @brief D3D12VA: data[0] = AVD3D12VAFrame*. */
inline void* avframeD3D12(const AVFrame& f) noexcept
{
  return f.data[0];
}

/** @brief CUDA (NVDEC): data[plane] = CUdeviceptr cast to uint8_t*. */
inline uint64_t avframeCudaPlane(const AVFrame& f, int plane) noexcept
{
  return reinterpret_cast<uintptr_t>(f.data[plane]);
}

/**
 * @brief Vulkan-native: data[0] = AVVkFrame*. After av_hwframe_map(DRM_PRIME)
 *        you get a separate AVFrame whose data[0] points to an AVDRMFrameDescriptor.
 */
inline void* avframeVulkanFrame(const AVFrame& f) noexcept
{
  return f.data[0];
}

inline void* avframeDRMDescriptor(const AVFrame& mapped) noexcept
{
  return mapped.data[0]; // AVDRMFrameDescriptor*
}

/** @brief VideoToolbox: data[3] = CVPixelBufferRef. */
inline void* avframeVideoToolbox(const AVFrame& f) noexcept
{
  return f.data[3];
}

} // namespace score::gfx::interop
