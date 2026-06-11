#include <Gfx/Graph/RhiClearBuffer.hpp>

#include <QtGui/private/qrhi_p.h>

// Vulkan
#if QT_HAS_VULKAN || (QT_CONFIG(vulkan) && __has_include(<vulkan/vulkan.h>))
#include <score/gfx/Vulkan.hpp>
#if __has_include(<QtGui/rhi/qrhi_platform.h>)
#include <QtGui/rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhivulkan_p.h>
#endif
#include <QVulkanInstance>
#define SCORE_HAS_VULKAN 1
#endif

#include <algorithm>
#include <cstring>
#include <vector>

// On non-Apple, provide a no-op stub for clearBufferMetal
// (the real implementation lives in RhiClearBufferMetal.mm)
#if !defined(Q_OS_MACOS) && !defined(Q_OS_IOS)
namespace score::gfx
{
bool clearBufferMetal(
    QRhi&, QRhiCommandBuffer&, QRhiBuffer*, quint32, quint32, quint32)
{
  return false;
}
}
#endif

namespace score::gfx
{
namespace
{

// Thread-local zero-buffer pool. Amortises the std::vector<char>(N, 0)
// allocation across every clearBuffer call site — at steady state the
// vector grows once to the max requested size and is reused for every
// subsequent call, so the per-call cost is just a memset of the
// requested range (already zero, so the access is touched-page free
// for the prefix that survived the last clear).
//
// Pattern != 0 hits a side path that materialises the requested
// 4-byte pattern into a separate vector. The default-pattern (0) path
// is the one every current call site uses.
const char* getZeroBuffer(quint32 size)
{
  thread_local std::vector<char> zero_pool;
  if(zero_pool.size() < size)
    zero_pool.assign(size, 0);
  return zero_pool.data();
}

// Pattern path — used when pattern != 0. Replicates the 4-byte pattern
// across the requested size. The buffer is sticky per-thread so a hot
// pattern (e.g. 0xFFFFFFFF for "invalid slot" sentinels) reuses the
// same memory. Switching patterns rewrites the buffer.
const char* getPatternBuffer(quint32 size, quint32 pattern)
{
  thread_local std::vector<char> pattern_pool;
  thread_local quint32 last_pattern = 0u;
  thread_local quint32 last_filled = 0u;
  const bool grow = pattern_pool.size() < size;
  if(grow)
    pattern_pool.resize(size);
  if(grow || last_pattern != pattern || last_filled < size)
  {
    auto* p = pattern_pool.data();
    const quint32 n = size / 4u;
    for(quint32 i = 0; i < n; ++i)
      std::memcpy(p + i * 4u, &pattern, 4u);
    // Tail bytes (size not 4-aligned). vkCmdFillBuffer requires
    // 4-aligned size so this only matters for the batch fallback.
    const quint32 tail = size - n * 4u;
    if(tail)
      std::memcpy(p + n * 4u, &pattern, tail);
    last_pattern = pattern;
    last_filled = size;
  }
  return pattern_pool.data();
}

const char* getSourceBytes(quint32 size, quint32 pattern)
{
  return pattern == 0u ? getZeroBuffer(size) : getPatternBuffer(size, pattern);
}

// Route a clear into a QRhiResourceUpdateBatch the way QRhi expects:
// uploadStaticBuffer for Static, updateDynamicBuffer for Dynamic UBOs
// (chunked at 65535 bytes — QRhi's documented maximum per call for
// the host-coherent path).
void clearViaBatch(
    QRhiResourceUpdateBatch& batch, QRhiBuffer* buf,
    quint32 offset, quint32 size, quint32 pattern)
{
  if(!buf || size == 0)
    return;
  const char* src = getSourceBytes(size, pattern);
  if(buf->type() == QRhiBuffer::Dynamic)
  {
    quint32 off = 0;
    while(off < size)
    {
      const quint32 chunk = std::min<quint32>(size - off, 65535u);
      batch.updateDynamicBuffer(buf, offset + off, chunk, src + off);
      off += chunk;
    }
  }
  else
  {
    batch.uploadStaticBuffer(buf, offset, size, src);
  }
}

}  // namespace

// Returns true on success (native path took it), false to request the
// shared fallback. Backend-specific helper to keep clearBuffer() free
// of forward-flow control hazards.
static bool clearBufferNative(
    QRhi& rhi,
    QRhiCommandBuffer& cb,
    QRhiBuffer* buf,
    quint32 offset,
    quint32 size,
    quint32 pattern)
{
  switch(rhi.backend())
  {
#if SCORE_HAS_VULKAN
    case QRhi::Vulkan: {
      // vkCmdFillBuffer is only legal on buffers with
      // VK_BUFFER_USAGE_TRANSFER_DST_BIT. QRhi's QVkBuffer::create adds
      // that bit only for non-Dynamic buffers (see qrhivulkan.cpp ~line
      // 7212). Dynamic UBOs would trip the validation layer if we
      // called vkCmdFillBuffer on them — fall back to the deferred
      // path. (In practice none of the current call sites pass a
      // Dynamic buffer through the CB variant; this is defence in
      // depth.)
      if(buf->type() == QRhiBuffer::Dynamic)
        return false;

      auto* inst = score::gfx::staticVulkanInstance();
      if(!inst)
        return false;

      auto fn = reinterpret_cast<PFN_vkCmdFillBuffer>(
          inst->getInstanceProcAddr("vkCmdFillBuffer"));
      if(!fn)
        return false;

      auto* native
          = static_cast<const QRhiVulkanCommandBufferNativeHandles*>(cb.nativeHandles());
      if(!native || !native->commandBuffer)
        return false;

      auto bufNative = buf->nativeBuffer();
      if(!bufNative.objects[0])
        return false;

      // QRhi NativeBuffer convention (Vulkan): objects[i] is `VkBuffer *`,
      // i.e. a POINTER TO the handle. Dereference to obtain the actual
      // VkBuffer. See the long comment in RhiComputeBarrier.cpp's copyBuffer
      // for the per-backend convention table.
      VkBuffer vkbuf = *static_cast<const VkBuffer*>(bufNative.objects[0]);
      if(vkbuf == VK_NULL_HANDLE)
        return false;

      cb.beginExternal();
      // vkCmdFillBuffer signature: (cb, buffer, offset, size, data).
      // - offset and size MUST be multiples of 4. Caller is required to
      //   honour this; we don't silently round here because doing so
      //   would clear bytes the caller didn't request.
      // - data is a uint32_t replicated across the range (exactly the
      //   contract the abstraction exposes via @p pattern).
      // - The buffer must NOT be in a render pass; this path is
      //   intended for resource setup / runInitialPasses-style sites
      //   that have a CB but no active pass.
      fn(native->commandBuffer, vkbuf,
         static_cast<VkDeviceSize>(offset),
         static_cast<VkDeviceSize>(size),
         pattern);
      cb.endExternal();
      return true;
    }
#endif

    case QRhi::Metal:
      return clearBufferMetal(rhi, cb, buf, offset, size, pattern);

#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    case QRhi::D3D12:
#endif
    case QRhi::D3D11:
    case QRhi::OpenGLES2:
    default:
      // No native fast path wired yet.
      return false;
  }
}

void RhiClearBuffer::clearBuffer(
    QRhi& rhi,
    QRhiCommandBuffer& cb,
    QRhiBuffer* buf,
    quint32 offset,
    quint32 size,
    quint32 pattern)
{
  if(!buf || size == 0)
    return;

  if(clearBufferNative(rhi, cb, buf, offset, size, pattern))
    return;

  // No native path available. Allocate a one-shot QRhiResourceUpdateBatch
  // and submit it to the rhi via the standard route. We deliberately do
  // NOT borrow the caller's batch here (the caller doesn't have one in
  // scope by definition — they passed us a CB). The cost: one batch
  // allocation + queue insertion. Still much cheaper than a per-call
  // std::vector<char>(size, 0) allocation thanks to the zero pool.
  if(auto* batch = rhi.nextResourceUpdateBatch())
  {
    clearViaBatch(*batch, buf, offset, size, pattern);
    cb.resourceUpdate(batch);
  }
}

void RhiClearBuffer::clearBuffer(
    QRhi& rhi,
    QRhiResourceUpdateBatch& batch,
    QRhiBuffer* buf,
    quint32 offset,
    quint32 size,
    quint32 pattern)
{
  // Backend is not relevant here — every backend's update batch is a
  // straight CPU→GPU upload, so the only thing the abstraction buys us
  // is the zero pool (eliminating the per-call vector allocation that
  // motivated this whole exercise). A future revision could record a
  // pending native fill and apply it in the next CB-recording op, but
  // that's a deeper refactor than the current bug warrants.
  (void)rhi;
  clearViaBatch(batch, buf, offset, size, pattern);
}

}  // namespace score::gfx
