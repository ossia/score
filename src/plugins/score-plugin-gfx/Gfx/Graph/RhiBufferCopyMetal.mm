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
