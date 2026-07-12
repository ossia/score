#include <Gfx/Graph/RhiComputeBarrier.hpp>

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

// Pre-condition: cb must NOT have an active render or compute pass.
// Metal allows only one encoder open on a command buffer at a time; calling
// [MTLCommandBuffer blitCommandEncoder] while a render or compute encoder is
// still open will trigger a Metal internal assertion or silent misbehaviour.
// Call this between cb.endPass() and the next cb.beginPass().
//
// Hazard tracking: Metal's default MTLHazardTrackingModeTracked automatically
// inserts a dependency between this blit encoder and any subsequent encoder on
// the same command buffer that accesses the same buffer. No explicit MTLFence
// or MTLBarrier is required for tracked resources.
//
// Note: QRhi's own QRhiResourceUpdateBatch::copyBuffer enforces the
// no-active-pass contract internally. This native-handle path bypasses that
// check, so the caller is responsible for ensuring no encoder is open.
void copyBufferMetal(
    QRhi& rhi, QRhiCommandBuffer& cb,
    QRhiBuffer* src, QRhiBuffer* dst, int size,
    int srcOffset, int dstOffset)
{
  if(!src || !dst || size <= 0 || srcOffset < 0 || dstOffset < 0)
    return;

  const auto* handles
      = static_cast<const QRhiMetalCommandBufferNativeHandles*>(cb.nativeHandles());
  if(!handles || !handles->commandBuffer)
    return;

  auto srcNative = src->nativeBuffer();
  auto dstNative = dst->nativeBuffer();
  if(!srcNative.objects[0] || !dstNative.objects[0])
    return;

  id<MTLCommandBuffer> cmdBuf = (id<MTLCommandBuffer>)handles->commandBuffer;
  // QRhi documents NativeBuffer::objects[i] as a POINTER TO the native
  // handle, not the handle itself. On Metal the handle is an MTLBuffer
  // pointer, so objects[0] is `MTLBuffer * *`. Dereference once to get
  // the actual handle.
  void* const* srcSlot = static_cast<void* const*>(srcNative.objects[0]);
  void* const* dstSlot = static_cast<void* const*>(dstNative.objects[0]);
  id<MTLBuffer> srcBuf = (__bridge id<MTLBuffer>) (*srcSlot);
  id<MTLBuffer> dstBuf = (__bridge id<MTLBuffer>) (*dstSlot);
  if(!srcBuf || !dstBuf)
    return;

  id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
  [blit copyFromBuffer:srcBuf
          sourceOffset:(NSUInteger)srcOffset
              toBuffer:dstBuf
     destinationOffset:(NSUInteger)dstOffset
                  size:(NSUInteger)size];
  [blit endEncoding];
}

// Pre-condition: cb must NOT have an active render or compute pass.
// Same contract as copyBufferMetal above: only one encoder may be open on a
// MTLCommandBuffer at a time. Caller is responsible for ensuring no render or
// compute encoder is currently open before calling this function.
//
// Metal's default hazard tracking inserts the required memory dependency
// between this blit and subsequent encoders on the same command buffer that
// read the destination buffer; no explicit fence is needed.
void copyBufferRegionsMetal(
    QRhi& rhi, QRhiCommandBuffer& cb,
    QRhiBuffer* src, QRhiBuffer* dst,
    const BufferCopyRegion* regions, int count)
{
  if(!src || !dst || !regions || count <= 0)
    return;

  const auto* handles
      = static_cast<const QRhiMetalCommandBufferNativeHandles*>(cb.nativeHandles());
  if(!handles || !handles->commandBuffer)
    return;

  auto srcNative = src->nativeBuffer();
  auto dstNative = dst->nativeBuffer();
  if(!srcNative.objects[0] || !dstNative.objects[0])
    return;

  id<MTLCommandBuffer> cmdBuf = (id<MTLCommandBuffer>)handles->commandBuffer;
  void* const* srcSlot = static_cast<void* const*>(srcNative.objects[0]);
  void* const* dstSlot = static_cast<void* const*>(dstNative.objects[0]);
  id<MTLBuffer> srcBuf = (__bridge id<MTLBuffer>) (*srcSlot);
  id<MTLBuffer> dstBuf = (__bridge id<MTLBuffer>) (*dstSlot);
  if(!srcBuf || !dstBuf)
    return;

  // One blit encoder, N copyFromBuffer calls. Amortizes encoder
  // creation/teardown and any implicit GPU state transitions.
  id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
  for(int i = 0; i < count; ++i)
  {
    [blit copyFromBuffer:srcBuf
            sourceOffset:(NSUInteger)regions[i].src_offset
                toBuffer:dstBuf
       destinationOffset:(NSUInteger)regions[i].dst_offset
                    size:(NSUInteger)regions[i].size];
  }
  [blit endEncoding];
}

}

#else

// No Metal support — provide a no-op stub
namespace score::gfx
{
void copyBufferMetal(
    QRhi&, QRhiCommandBuffer&,
    QRhiBuffer*, QRhiBuffer*, int, int, int)
{
}
void copyBufferRegionsMetal(
    QRhi&, QRhiCommandBuffer&,
    QRhiBuffer*, QRhiBuffer*,
    const BufferCopyRegion*, int)
{
}
}

#endif
