#include <Gfx/Graph/RhiClearBuffer.hpp>

#include <QtGui/private/qrhi_p.h>

#if __has_include(<Metal/Metal.h>)
#include <Metal/Metal.h>
#if __has_include(<QtGui/rhi/qrhi_platform.h>)
#include <QtGui/rhi/qrhi_platform.h>
#else
#include <QtGui/private/qrhimetal_p.h>
#endif

namespace score::gfx
{

// Pre-condition: cb must NOT have an active render or compute pass —
// same contract as copyBufferMetal in RhiBufferCopyMetal.mm. Metal allows
// only one encoder open on a command buffer at a time; opening a blit
// encoder while a render/compute encoder is live triggers an internal
// assertion or silent misbehaviour.
//
// Hazard tracking: the default MTLHazardTrackingModeTracked inserts a
// dependency between this blit encoder and any subsequent encoder on
// the same command buffer that touches the same buffer, so no explicit
// MTLFence / MTLBarrier is needed.
//
// fillBuffer:range:value: takes a single byte value (uint8_t), not a
// 4-byte word. We map 4-byte patterns to a Metal fill ONLY when all
// four bytes are equal — the common case (pattern == 0 or pattern ==
// 0xFFFFFFFF). For arbitrary patterns Metal would need a manual
// stage-via-MTLBuffer + copyFromBuffer; we return false and let the
// caller fall back to QRhi's update batch, which is the right vehicle
// for general-purpose host writes anyway.
bool clearBufferMetal(
    QRhi& rhi,
    QRhiCommandBuffer& cb,
    QRhiBuffer* buf,
    quint32 offset,
    quint32 size,
    quint32 pattern)
{
  (void)rhi;
  if(!buf || size == 0)
    return false;

  const uint8_t b0 = static_cast<uint8_t>(pattern & 0xFFu);
  const uint8_t b1 = static_cast<uint8_t>((pattern >> 8) & 0xFFu);
  const uint8_t b2 = static_cast<uint8_t>((pattern >> 16) & 0xFFu);
  const uint8_t b3 = static_cast<uint8_t>((pattern >> 24) & 0xFFu);
  // fillBuffer: takes a single uint8_t. Refuse non-uniform-byte patterns.
  if(b0 != b1 || b0 != b2 || b0 != b3)
    return false;

  const auto* handles
      = static_cast<const QRhiMetalCommandBufferNativeHandles*>(cb.nativeHandles());
  if(!handles || !handles->commandBuffer)
    return false;

  auto bufNative = buf->nativeBuffer();
  if(!bufNative.objects[0])
    return false;

  id<MTLCommandBuffer> cmdBuf = (id<MTLCommandBuffer>)handles->commandBuffer;
  // QRhi NativeBuffer convention (Metal): objects[i] is `id<MTLBuffer> *`,
  // i.e. a POINTER TO the handle. Dereference once to obtain the handle.
  // For Dynamic buffers QRhi presents N slots; the CB variant doesn't
  // currently target Dynamic buffers (they fall back to the batch path)
  // but if it ever does we'd want to clear all slots — same as Vulkan's
  // Dynamic guard in RhiClearBuffer.cpp.
  void* const* slot = static_cast<void* const*>(bufNative.objects[0]);
  id<MTLBuffer> mtlBuf = (__bridge id<MTLBuffer>)(*slot);
  if(!mtlBuf)
    return false;

  cb.beginExternal();
  id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
  [blit fillBuffer:mtlBuf
             range:NSMakeRange((NSUInteger)offset, (NSUInteger)size)
             value:b0];
  [blit endEncoding];
  cb.endExternal();
  return true;
}

}  // namespace score::gfx

#endif
