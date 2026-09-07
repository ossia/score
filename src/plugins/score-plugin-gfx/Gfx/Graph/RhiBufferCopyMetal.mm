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

#include <QDebug>

namespace
{
// On the Metal backend, QRhiResourceUpdateBatch::uploadStaticBuffer does NOT
// write the MTLBuffer when the batch is submitted: the bytes are stashed in
// QMetalBufferData::pendingUpdates and only memcpy'd into [buf contents] the
// next time the buffer is used through QRhi's own binding machinery
// (setShaderResources, setVertexInput, draw*Indirect, readBackBuffer).
// QMetalBuffer::nativeBuffer() performs that flush for slotted (Dynamic)
// buffers only; for Immutable/Static buffers it returns &d->buf[0] WITHOUT
// executing the pending host writes (qrhimetal.mm, still true in Qt 6.12).
//
// This native-handle blit path reads the MTLBuffer directly, so a buffer that
// is consumed ONLY through these copies — never bound through QRhi — keeps
// its uploads stuck in pendingUpdates forever and the blit reads stale zeros.
// Measured symptom: every per-instance translation copied out of the
// world-transform buffer read back as (0,0,0) on Metal while Vulkan and
// OpenGL rendered correctly (test_gfx_instancer_shrink).
//
// Force the flush through public API: enqueueing a buffer readback executes
// the buffer's pending host writes immediately at enqueue time
// (QRhiMetal::enqueueResourceUpdates, BufferOp::Read path). The 1-byte
// readback result is a throwaway that deletes itself on completion.
//
// Must be called outside any render/compute pass — the same contract the
// copy functions below already impose on their callers.
void flushPendingHostWritesMetal(QRhi& rhi, QRhiCommandBuffer& cb, QRhiBuffer* buf)
{
  // nextResourceUpdateBatch() returns nullptr when QRhi's fixed batch pool is
  // exhausted -- RenderList.cpp:1312 already handles that case explicitly, so
  // it is reachable, and this path is a MULTIPLIER on it: issuePendingGpuCopies
  // loops over every queued copy and each one flushes both src and dst, i.e.
  // two batches per copy op per frame. Degrade instead of dereferencing null:
  // skipping the flush can leave a copy reading stale bytes (the bug this
  // function exists to prevent) but that is strictly better than a crash, and
  // the warning names it rather than leaving it silent.
  QRhiResourceUpdateBatch* batch = rhi.nextResourceUpdateBatch();
  if(!batch)
  {
    static bool warned = false;
    if(!warned)
    {
      warned = true;
      qWarning("copyBuffer(Metal): resource update batch pool exhausted; "
               "pending host writes could not be flushed and this copy may "
               "read stale data");
    }
    return;
  }
  auto* result = new QRhiReadbackResult;
  result->completed = [result] { delete result; };
  batch->readBackBuffer(buf, 0, 1, result);
  cb.resourceUpdate(batch);
}
}

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

  // Land any uploadStaticBuffer data still parked in Qt's pendingUpdates
  // before reading/writing the MTLBuffers natively; see the comment on
  // flushPendingHostWritesMetal.
  flushPendingHostWritesMetal(rhi, cb, src);
  flushPendingHostWritesMetal(rhi, cb, dst);

  auto srcNative = src->nativeBuffer();
  auto dstNative = dst->nativeBuffer();
  if(!srcNative.objects[0] || !dstNative.objects[0])
    return;

  // Slot 0 unconditionally, which is only right for an UNSLOTTED buffer.
  //
  // Qt's Metal backend keeps QMTL_FRAMES_IN_FLIGHT copies of most buffers --
  // qrhimetal.mm: "writing to a Managed buffer (which is what Immutable and
  // Static maps to on macOS) is not safe when another frame reading from the
  // same buffer is still in flight" -- and excludes exactly one usage:
  //     d->slotted = !m_usage.testFlag(QRhiBuffer::StorageBuffer);
  // Every caller of this helper copies storage buffers, so slotCount is 1 and
  // objects[0] is the only slot there is. Hand it a slotted buffer and it
  // would copy from or into whichever frame happens to sit at slot 0 --
  // silently, and only sometimes wrong. (N2.)
  //
  // The precondition is checkable, so check it rather than trusting the
  // caller list to stay this way.
  if(srcNative.slotCount > 1 || dstNative.slotCount > 1)
  {
    static bool warned = false;
    if(!warned)
    {
      warned = true;
      qWarning() << "score.gfx: copyBufferMetal on a SLOTTED buffer (src slots"
                 << srcNative.slotCount << ", dst slots" << dstNative.slotCount
                 << ") -- this helper only ever addresses slot 0, so the copy "
                    "would touch the wrong frame's buffer. Refusing it.";
    }
    return;
  }

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

  // Land any uploadStaticBuffer data still parked in Qt's pendingUpdates
  // before reading/writing the MTLBuffers natively; see the comment on
  // flushPendingHostWritesMetal.
  flushPendingHostWritesMetal(rhi, cb, src);
  flushPendingHostWritesMetal(rhi, cb, dst);

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
