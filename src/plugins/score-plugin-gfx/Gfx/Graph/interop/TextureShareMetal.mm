#include "TextureShare.hpp"

#if __has_include(<Metal/Metal.h>)
#include <Metal/Metal.h>
#include <IOSurface/IOSurface.h>

namespace score::gfx
{

class TripleBufferIndexMetal
{
public:
  TripleBufferIndexMetal()
  {
    m_state.store(makeState(0, 1, 2, false), std::memory_order_relaxed);
  }

  int acquireWriteIndex() noexcept
  {
    uint32_t state = m_state.load(std::memory_order_acquire);
    return writeIndex(state);
  }

  void publishWriteIndex() noexcept
  {
    uint32_t oldState, newState;
    do
    {
      oldState = m_state.load(std::memory_order_acquire);
      int w = writeIndex(oldState);
      int r = readyIndex(oldState);
      int rd = readIndex(oldState);
      newState = makeState(r, w, rd, true);
    } while(!m_state.compare_exchange_weak(
        oldState, newState, std::memory_order_release, std::memory_order_relaxed));
  }

  int acquireReadIndex() noexcept
  {
    uint32_t oldState, newState;
    do
    {
      oldState = m_state.load(std::memory_order_acquire);
      if(!hasNewFrame(oldState))
        return readIndex(oldState);

      int w = writeIndex(oldState);
      int r = readyIndex(oldState);
      int rd = readIndex(oldState);
      newState = makeState(w, rd, r, false);
    } while(!m_state.compare_exchange_weak(
        oldState, newState, std::memory_order_release, std::memory_order_relaxed));

    return readyIndex(oldState);
  }

  bool hasNewFrameAvailable() const noexcept
  {
    return hasNewFrame(m_state.load(std::memory_order_acquire));
  }

private:
  static constexpr uint32_t makeState(int w, int r, int rd, bool hasNew) noexcept
  {
    return (uint32_t(w) << 5) | (uint32_t(r) << 3) | (uint32_t(rd) << 1)
           | (hasNew ? 1 : 0);
  }
  static constexpr int writeIndex(uint32_t state) noexcept { return (state >> 5) & 3; }
  static constexpr int readyIndex(uint32_t state) noexcept { return (state >> 3) & 3; }
  static constexpr int readIndex(uint32_t state) noexcept { return (state >> 1) & 3; }
  static constexpr bool hasNewFrame(uint32_t state) noexcept { return state & 1; }

  std::atomic<uint32_t> m_state;
};

/**
 * @brief Metal backend using IOSurface for zero-copy GPU texture sharing.
 *
 * IOSurface is a macOS kernel-level abstraction that allows multiple
 * processes and GPU contexts to share GPU memory without any copies.
 *
 * Architecture:
 * - 3 IOSurfaces for triple buffering
 * - Each IOSurface backs both producer and consumer Metal textures
 * - Producer renders → IOSurface ← Consumer samples
 * - MTLSharedEvent for GPU synchronization between contexts
 */
class TextureShareMetal : public TextureShareBackend
{
  // Producer-side resources
  struct ProducerSlot
  {
    QRhiTexture* texture{};
    QRhiTextureRenderTarget* renderTarget{};
    id<MTLTexture> mtlTexture{};
  };
  ProducerSlot m_producerSlots[3];
  QRhiRenderPassDescriptor* m_producerRenderPass{};

  // IOSurfaces (the actual shared memory)
  IOSurfaceRef m_surfaces[3]{};

  // Consumer-side textures (backed by same IOSurfaces)
  QRhiTexture* m_consumerTextures[3]{};
  id<MTLTexture> m_consumerMtlTextures[3]{};

  // Synchronization
  id<MTLSharedEvent> m_sharedEvents[3]{};
  uint64_t m_eventValues[3]{};

  // Devices
  QRhi* m_producer{};
  QRhi* m_consumer{};
  id<MTLDevice> m_producerDevice{};
  id<MTLDevice> m_consumerDevice{};
  id<MTLCommandQueue> m_producerQueue{};
  id<MTLCommandQueue> m_consumerQueue{};

  QSize m_size;
  QRhiTexture::Format m_format{QRhiTexture::RGBA8};
  std::atomic<bool> m_producerReady{false};
  std::atomic<bool> m_consumerReady{false};

  TripleBufferIndexMetal m_tripleBuffer;
  int m_currentWriteSlot{-1};
  int m_currentReadSlot{0};

  MTLPixelFormat toMTLPixelFormat(QRhiTexture::Format format) const
  {
    switch (format)
    {
      case QRhiTexture::RGBA8:
        return MTLPixelFormatRGBA8Unorm;
      case QRhiTexture::BGRA8:
        return MTLPixelFormatBGRA8Unorm;
      case QRhiTexture::RGBA16F:
        return MTLPixelFormatRGBA16Float;
      case QRhiTexture::RGBA32F:
        return MTLPixelFormatRGBA32Float;
      case QRhiTexture::R8:
        return MTLPixelFormatR8Unorm;
      case QRhiTexture::R16:
        return MTLPixelFormatR16Unorm;
      case QRhiTexture::R16F:
        return MTLPixelFormatR16Float;
      case QRhiTexture::R32F:
        return MTLPixelFormatR32Float;
      default:
        return MTLPixelFormatRGBA8Unorm;
    }
  }

  size_t bytesPerPixel(QRhiTexture::Format format) const
  {
    switch (format)
    {
      case QRhiTexture::RGBA8:
      case QRhiTexture::BGRA8:
        return 4;
      case QRhiTexture::RGBA16F:
        return 8;
      case QRhiTexture::RGBA32F:
        return 16;
      case QRhiTexture::R8:
        return 1;
      case QRhiTexture::R16:
      case QRhiTexture::R16F:
        return 2;
      case QRhiTexture::R32F:
        return 4;
      default:
        return 4;
    }
  }

public:
  bool setupProducer(QRhi* producer, QSize size, QRhiTexture::Format format) override
  {
    @autoreleasepool {
      m_producer = producer;
      m_size = size;
      m_format = format;

      // Get Metal device from QRhi
      auto* producerHandles
          = static_cast<const QRhiMetalNativeHandles*>(producer->nativeHandles());

      if (!producerHandles)
        return false;

      m_producerDevice = (__bridge id<MTLDevice>)(__bridge void*)producerHandles->dev;
      m_producerQueue = (__bridge id<MTLCommandQueue>)(__bridge void*)producerHandles->cmdQueue;

      if (!m_producerDevice)
        return false;

      // Create render pass descriptor
      QRhiTexture* dummyTex = m_producer->newTexture(
          m_format, m_size, 1,
          QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
      if (!dummyTex->create())
      {
        delete dummyTex;
        return false;
      }

      QRhiTextureRenderTarget* dummyRt
          = m_producer->newTextureRenderTarget({QRhiColorAttachment(dummyTex)});
      m_producerRenderPass = dummyRt->newCompatibleRenderPassDescriptor();
      delete dummyRt;
      delete dummyTex;

      if (!m_producerRenderPass)
        return false;

      // Create IOSurfaces and Metal textures
      MTLPixelFormat mtlFormat = toMTLPixelFormat(format);
      size_t bpp = bytesPerPixel(format);

      for (int i = 0; i < 3; ++i)
      {
        // Create IOSurface
        NSDictionary* properties = @{
          (id)kIOSurfaceWidth: @(size.width()),
          (id)kIOSurfaceHeight: @(size.height()),
          (id)kIOSurfaceBytesPerElement: @(bpp),
          (id)kIOSurfacePixelFormat: @(mtlFormat),
        };

        m_surfaces[i] = IOSurfaceCreate((__bridge CFDictionaryRef)properties);
        if (!m_surfaces[i])
          return false;

        // Create producer Metal texture backed by IOSurface
        MTLTextureDescriptor* texDesc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:mtlFormat
                                         width:size.width()
                                        height:size.height()
                                     mipmapped:NO];
        texDesc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        texDesc.storageMode = MTLStorageModeShared; // Required for IOSurface

        m_producerSlots[i].mtlTexture =
            [m_producerDevice newTextureWithDescriptor:texDesc
                                             iosurface:m_surfaces[i]
                                                 plane:0];
        if (!m_producerSlots[i].mtlTexture)
          return false;

        // Create QRhi wrapper for producer texture
        m_producerSlots[i].texture
            = m_producer->newTexture(m_format, m_size, 1,
                QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
        if (!m_producerSlots[i].texture->createFrom(
                {quint64((__bridge void*)m_producerSlots[i].mtlTexture), 0}))
          return false;

        // Create render target
        m_producerSlots[i].renderTarget = m_producer->newTextureRenderTarget(
            {QRhiColorAttachment(m_producerSlots[i].texture)});
        m_producerSlots[i].renderTarget->setRenderPassDescriptor(m_producerRenderPass);
        if (!m_producerSlots[i].renderTarget->create())
          return false;

        // Create shared event for synchronization
        m_sharedEvents[i] = [m_producerDevice newSharedEvent];
        if (!m_sharedEvents[i])
          return false;

        m_eventValues[i] = 0;
      }

      m_producerReady.store(true, std::memory_order_release);
      return true;
    }
  }

  bool setupConsumer(QRhi* consumer) override
  {
    @autoreleasepool {
      m_consumer = consumer;

      auto* consumerHandles
          = static_cast<const QRhiMetalNativeHandles*>(consumer->nativeHandles());

      if (!consumerHandles)
        return false;

      m_consumerDevice = (__bridge id<MTLDevice>)(__bridge void*)consumerHandles->dev;
      m_consumerQueue = (__bridge id<MTLCommandQueue>)(__bridge void*)consumerHandles->cmdQueue;

      if (!m_consumerDevice)
        return false;

      MTLPixelFormat mtlFormat = toMTLPixelFormat(m_format);

      for (int i = 0; i < 3; ++i)
      {
        // Create consumer Metal texture backed by same IOSurface (zero-copy!)
        MTLTextureDescriptor* consumerTexDesc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:mtlFormat
                                         width:m_size.width()
                                        height:m_size.height()
                                     mipmapped:NO];
        consumerTexDesc.usage = MTLTextureUsageShaderRead;
        consumerTexDesc.storageMode = MTLStorageModeShared;

        m_consumerMtlTextures[i] =
            [m_consumerDevice newTextureWithDescriptor:consumerTexDesc
                                             iosurface:m_surfaces[i]
                                                 plane:0];
        if (!m_consumerMtlTextures[i])
          return false;

        // Create QRhi wrapper for consumer texture
        m_consumerTextures[i] = m_consumer->newTexture(m_format, m_size, 1, {});
        if (!m_consumerTextures[i]->createFrom(
                {quint64((__bridge void*)m_consumerMtlTextures[i]), 0}))
          return false;
      }

      m_consumerReady.store(true, std::memory_order_release);
      return true;
    }
  }

  bool setup(
      QRhi* producer, QRhi* consumer, QSize size,
      QRhiTexture::Format format) override
  {
    // Convenience method - sets up both sides from calling thread
    if (!setupProducer(producer, size, format))
      return false;
    return setupConsumer(consumer);
  }

  void resize(QSize newSize) override
  {
    if (m_size == newSize)
      return;
    cleanup();
    setup(m_producer, m_consumer, newSize, m_format);
  }

  void cleanup() override
  {
    @autoreleasepool {
      m_producerReady.store(false, std::memory_order_release);
      m_consumerReady.store(false, std::memory_order_release);

      for (int i = 0; i < 3; ++i)
      {
        delete m_consumerTextures[i];
        m_consumerTextures[i] = nullptr;

        m_consumerMtlTextures[i] = nil;

        delete m_producerSlots[i].renderTarget;
        m_producerSlots[i].renderTarget = nullptr;

        delete m_producerSlots[i].texture;
        m_producerSlots[i].texture = nullptr;

        m_producerSlots[i].mtlTexture = nil;

        m_sharedEvents[i] = nil;

        if (m_surfaces[i])
        {
          CFRelease(m_surfaces[i]);
          m_surfaces[i] = nullptr;
        }
      }

      delete m_producerRenderPass;
      m_producerRenderPass = nullptr;
    }
  }

  bool isValid() const noexcept override
  {
    return m_producerReady.load(std::memory_order_acquire)
           && m_consumerReady.load(std::memory_order_acquire);
  }

  bool isProducerReady() const noexcept override
  {
    return m_producerReady.load(std::memory_order_acquire);
  }

  QRhiTextureRenderTarget* beginProducerFrame() override
  {
    m_currentWriteSlot = m_tripleBuffer.acquireWriteIndex();
    if (m_currentWriteSlot < 0 || m_currentWriteSlot >= 3)
      return nullptr;
    return m_producerSlots[m_currentWriteSlot].renderTarget;
  }

  QRhiTexture* currentProducerTexture() override
  {
    if(m_currentWriteSlot < 0 || m_currentWriteSlot >= 3)
      return nullptr;
    return m_producerSlots[m_currentWriteSlot].texture;
  }

  void endProducerFrame(QRhiCommandBuffer* cb) override
  {
    @autoreleasepool {
      if (m_currentWriteSlot < 0)
        return;

      // With IOSurface, no GPU copy is needed - producer and consumer
      // already share the same GPU memory!
      //
      // However, we need to signal the shared event so the consumer
      // knows when rendering is complete.
      //
      // Get the native command buffer and encode event signal
      auto* cbHandles = static_cast<const QRhiMetalCommandBufferNativeHandles*>(
          cb->nativeHandles());
      if (cbHandles && cbHandles->commandBuffer)
      {
        id<MTLCommandBuffer> mtlCb = (__bridge id<MTLCommandBuffer>)(__bridge void*)cbHandles->commandBuffer;

        // Increment event value and signal
        m_eventValues[m_currentWriteSlot]++;
        [mtlCb encodeSignalEvent:m_sharedEvents[m_currentWriteSlot]
                           value:m_eventValues[m_currentWriteSlot]];
      }

      m_tripleBuffer.publishWriteIndex();
      m_currentWriteSlot = -1;
    }
  }

  QRhiRenderPassDescriptor* producerRenderPassDescriptor() const noexcept override
  {
    return m_producerRenderPass;
  }

  QRhiTexture* acquireConsumerTexture(QRhiCommandBuffer* cb) override
  {
    @autoreleasepool {
      int readSlot = m_tripleBuffer.acquireReadIndex();
      if (readSlot < 0 || readSlot >= 3)
        return m_consumerTextures[m_currentReadSlot];

      // Wait for producer to complete rendering to this slot
      // Get the native command buffer and encode event wait
      auto* cbHandles = static_cast<const QRhiMetalCommandBufferNativeHandles*>(
          cb->nativeHandles());
      if (cbHandles && cbHandles->commandBuffer)
      {
        id<MTLCommandBuffer> mtlCb = (__bridge id<MTLCommandBuffer>)(__bridge void*)cbHandles->commandBuffer;

        // GPU wait for producer's signal
        [mtlCb encodeWaitForEvent:m_sharedEvents[readSlot]
                            value:m_eventValues[readSlot]];
      }

      m_currentReadSlot = readSlot;

      // No copy needed - IOSurface provides zero-copy sharing!
      // Just return the consumer texture that's backed by the same IOSurface
      return m_consumerTextures[readSlot];
    }
  }

  bool hasNewFrame() const noexcept override
  {
    return m_tripleBuffer.hasNewFrameAvailable();
  }

  QSize size() const noexcept override { return m_size; }
};

// Factory function for Metal backend
std::unique_ptr<TextureShareBackend>
createTextureShareBackendMetal(QRhi* producer, QRhi* consumer)
{
  return std::make_unique<TextureShareMetal>();
}

} // namespace score::gfx

#endif
