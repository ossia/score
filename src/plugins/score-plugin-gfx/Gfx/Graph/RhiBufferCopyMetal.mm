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
    QRhiBuffer* src, QRhiBuffer* dst, int size)
{
  if(!src || !dst || size <= 0)
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
  id<MTLBuffer> srcBuf = (__bridge id<MTLBuffer>)(void*)srcNative.objects[0];
  id<MTLBuffer> dstBuf = (__bridge id<MTLBuffer>)(void*)dstNative.objects[0];

  id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
  [blit copyFromBuffer:srcBuf
          sourceOffset:0
              toBuffer:dstBuf
     destinationOffset:0
                  size:(NSUInteger)size];
  [blit endEncoding];
}

}

#else

// No Metal support — provide a no-op stub
namespace score::gfx
{
void copyBufferMetal(
    QRhi&, QRhiCommandBuffer&,
    QRhiBuffer*, QRhiBuffer*, int)
{
}
}

#endif
