#include "TextureShare.hpp"

#include <QDebug>

#if defined(Q_OS_WIN)
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi1_2.h>
#endif

#include <score/gfx/Vulkan.hpp>
#if QT_HAS_VULKAN
#include <Gfx/Graph/interop/VkExternalMemoryHelpers.hpp>

#include <private/qrhivulkan_p.h>

#include <QVulkanFunctions>
#include <QVulkanInstance>
#if defined(Q_OS_WIN)
#include <vulkan/vulkan_win32.h>
#endif
#endif

#if QT_CONFIG(opengl)
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#endif

namespace score::gfx
{

// ============================================================================
// Triple Buffer Index Management
// ============================================================================

/**
 * Triple buffer state machine for producer/consumer pattern.
 *
 * Three slots: 0, 1, 2
 * - Producer writes to 'write' slot
 * - When done, producer swaps 'write' with 'ready'
 * - Consumer swaps 'ready' with 'read' when it wants new data
 * - Consumer reads from 'read' slot
 *
 * This ensures producer and consumer never access the same slot simultaneously.
 */
class TripleBufferIndex
{
public:
  TripleBufferIndex()
  {
    // Initial state: write=0, ready=1, read=2
    m_state.store(makeState(0, 1, 2, false), std::memory_order_relaxed);
  }

  // Called by producer when starting a new frame
  int acquireWriteIndex() noexcept
  {
    uint32_t state = m_state.load(std::memory_order_acquire);
    return writeIndex(state);
  }

  // Called by producer when frame is complete - swaps write and ready
  void publishWriteIndex() noexcept
  {
    uint32_t oldState, newState;
    do
    {
      oldState = m_state.load(std::memory_order_acquire);
      int w = writeIndex(oldState);
      int r = readyIndex(oldState);
      int rd = readIndex(oldState);
      // Swap write and ready, mark new frame available
      newState = makeState(r, w, rd, true);
    } while(!m_state.compare_exchange_weak(
        oldState, newState, std::memory_order_release, std::memory_order_relaxed));
  }

  // Called by consumer to get latest frame - swaps ready and read if new frame available
  // Returns the read index, or -1 if no new frame
  int acquireReadIndex() noexcept
  {
    uint32_t oldState, newState;
    do
    {
      oldState = m_state.load(std::memory_order_acquire);
      if(!hasNewFrame(oldState))
        return readIndex(oldState); // Return current read index, no swap

      int w = writeIndex(oldState);
      int r = readyIndex(oldState);
      int rd = readIndex(oldState);
      // Swap ready and read, clear new frame flag
      newState = makeState(w, rd, r, false);
    } while(!m_state.compare_exchange_weak(
        oldState, newState, std::memory_order_release, std::memory_order_relaxed));

    return readyIndex(oldState); // Return the old ready (now read) index
  }

  bool hasNewFrameAvailable() const noexcept
  {
    return hasNewFrame(m_state.load(std::memory_order_acquire));
  }

private:
  // State packing: [write:2][ready:2][read:2][hasNew:1] in lowest 7 bits
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

class TextureShareBackendBase : public TextureShareBackend
{
protected:
  // Producer-side resources (3 textures for triple buffering render targets)
  struct ProducerSlot
  {
    QRhiTexture* texture{};
    QRhiTextureRenderTarget* renderTarget{};
  };
  ProducerSlot m_producerSlots[3];
  QRhiRenderPassDescriptor* m_producerRenderPass{};

  // Shared intermediate textures (the actual shared resources)
  // These are created with platform-specific sharing enabled
  QRhiTexture* m_sharedTextures[3]{};

  // Consumer-side resources
  QRhiTexture* m_consumerTexture{};

  QRhi* m_producer{};
  QRhi* m_consumer{};
  QSize m_size;
  QRhiTexture::Format m_format{QRhiTexture::RGBA8};
  std::atomic<bool> m_producerReady{false};
  std::atomic<bool> m_consumerReady{false};

  TripleBufferIndex m_tripleBuffer;
  int m_currentWriteSlot{-1};

  bool createProducerResources()
  {
    if(!m_producer)
      return false;

    // Create render pass descriptor
    QRhiTexture* dummyTex = m_producer->newTexture(
        m_format, m_size, 1,
        QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
    if(!dummyTex->create())
    {
      delete dummyTex;
      return false;
    }

    QRhiTextureRenderTarget* dummyRt
        = m_producer->newTextureRenderTarget({QRhiColorAttachment(dummyTex)});
    m_producerRenderPass = dummyRt->newCompatibleRenderPassDescriptor();
    delete dummyRt;
    delete dummyTex;

    if(!m_producerRenderPass)
      return false;

    // Create triple-buffered producer textures and render targets
    for(int i = 0; i < 3; ++i)
    {
      m_producerSlots[i].texture = m_producer->newTexture(
          m_format, m_size, 1,
          QRhiTexture::RenderTarget | QRhiTexture::UsedAsTransferSource);
      if(!m_producerSlots[i].texture->create())
        return false;

      m_producerSlots[i].renderTarget = m_producer->newTextureRenderTarget(
          {QRhiColorAttachment(m_producerSlots[i].texture)});
      m_producerSlots[i].renderTarget->setRenderPassDescriptor(m_producerRenderPass);
      if(!m_producerSlots[i].renderTarget->create())
        return false;
    }

    return true;
  }

  void destroyProducerResources()
  {
    for(int i = 0; i < 3; ++i)
    {
      delete m_producerSlots[i].renderTarget;
      m_producerSlots[i].renderTarget = nullptr;
      delete m_producerSlots[i].texture;
      m_producerSlots[i].texture = nullptr;
    }
    delete m_producerRenderPass;
    m_producerRenderPass = nullptr;
  }

  void destroyConsumerResources()
  {
    delete m_consumerTexture;
    m_consumerTexture = nullptr;
  }

public:
  QSize size() const noexcept override { return m_size; }

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
    if(m_currentWriteSlot < 0 || m_currentWriteSlot >= 3)
      return nullptr;
    return m_producerSlots[m_currentWriteSlot].renderTarget;
  }

  QRhiTexture* currentProducerTexture() override
  {
    if(m_currentWriteSlot < 0 || m_currentWriteSlot >= 3)
      return nullptr;
    return m_producerSlots[m_currentWriteSlot].texture;
  }

  QRhiRenderPassDescriptor* producerRenderPassDescriptor() const noexcept override
  {
    return m_producerRenderPass;
  }

  bool hasNewFrame() const noexcept override
  {
    return m_tripleBuffer.hasNewFrameAvailable();
  }
};

#if QT_CONFIG(opengl)
class TextureShareOpenGL : public TextureShareBackendBase
{
  // Cached consumer texture wrappers (avoid recreation every frame)
  QRhiTexture* m_consumerTextureWrappers[3]{};

  // GL sync objects for cross-thread synchronization
  GLsync m_fences[3]{};

  // Native texture handles for sharing between threads
  quint64 m_sharedTextureHandles[3]{};

  int m_currentReadSlot{0};

public:
  bool setupProducer(QRhi* producer, QSize size, QRhiTexture::Format format) override
  {
    m_producer = producer;
    m_size = size;
    m_format = format;
    m_producerReady.store(false, std::memory_order_release);

    if(!createProducerResources())
      return false;

    // Create shared textures (intermediate buffers for GPU copy)
    for(int i = 0; i < 3; ++i)
    {
      m_sharedTextures[i] = m_producer->newTexture(
          m_format, m_size, 1, QRhiTexture::UsedAsTransferSource);
      if(!m_sharedTextures[i]->create())
        return false;

      // Store native texture handle for consumer to import
      m_sharedTextureHandles[i] = m_sharedTextures[i]->nativeTexture().object;
      m_fences[i] = nullptr;
    }

    m_producerReady.store(true, std::memory_order_release);
    return true;
  }

  bool setupConsumer(QRhi* consumer) override
  {
    if(!m_producerReady.load(std::memory_order_acquire))
    {
      qWarning() << "TextureShareOpenGL: setupConsumer called before producer is ready";
      return false;
    }

    m_consumer = consumer;

    // Pre-create consumer texture wrappers (avoids per-frame allocation)
    for(int i = 0; i < 3; ++i)
    {
      m_consumerTextureWrappers[i] = m_consumer->newTexture(
          m_format, m_size, 1, QRhiTexture::UsedAsTransferSource);

      // Import the shared texture using the stored handle
      QRhiTexture::NativeTexture nativeTex{m_sharedTextureHandles[i], 0};
      if(!m_consumerTextureWrappers[i]->createFrom(nativeTex))
      {
        // Fallback: create standalone texture
        m_consumerTextureWrappers[i]->create();
      }
    }

    m_consumerTexture = m_consumerTextureWrappers[0];
    m_consumerReady.store(true, std::memory_order_release);
    return true;
  }

  bool
  setup(QRhi* producer, QRhi* consumer, QSize size, QRhiTexture::Format format) override
  {
    // Convenience method - sets up both sides from calling thread
    if(!setupProducer(producer, size, format))
      return false;
    return setupConsumer(consumer);
  }

  void resize(QSize newSize) override
  {
    if(m_size == newSize)
      return;

    cleanup();
    setup(m_producer, m_consumer, newSize, m_format);
  }

  void cleanup() override
  {
    m_producerReady.store(false, std::memory_order_release);
    m_consumerReady.store(false, std::memory_order_release);

    // Delete fences
    if(m_producer && m_producer->makeThreadLocalNativeContextCurrent())
    {
      auto* gl = QOpenGLContext::currentContext();
      if(gl)
      {
        auto* f = gl->extraFunctions();
        for(int i = 0; i < 3; ++i)
        {
          if(m_fences[i])
          {
            f->glDeleteSync(m_fences[i]);
            m_fences[i] = nullptr;
          }
        }
      }
    }

    // Delete consumer wrappers
    for(int i = 0; i < 3; ++i)
    {
      delete m_consumerTextureWrappers[i];
      m_consumerTextureWrappers[i] = nullptr;
      m_sharedTextureHandles[i] = 0;
    }
    m_consumerTexture = nullptr;

    for(int i = 0; i < 3; ++i)
    {
      delete m_sharedTextures[i];
      m_sharedTextures[i] = nullptr;
    }
    destroyProducerResources();
  }

  void endProducerFrame(QRhiCommandBuffer* cb) override
  {
    if(m_currentWriteSlot < 0)
      return;

    // GPU copy: producer render target → shared intermediate texture
    QRhiResourceUpdateBatch* batch = m_producer->nextResourceUpdateBatch();
    QRhiTextureCopyDescription copyDesc;
    batch->copyTexture(
        m_sharedTextures[m_currentWriteSlot],
        m_producerSlots[m_currentWriteSlot].texture, copyDesc);
    cb->resourceUpdate(batch);

    // Create GL fence for synchronization (must be done after commands are flushed)
    // Note: The fence is created here but becomes valid after the command buffer is submitted.
    // For proper sync, we should create the fence after endFrame(), but since QRhi doesn't
    // expose that, we create it here and the consumer will wait on it.
    if(m_producer->makeThreadLocalNativeContextCurrent())
    {
      auto* gl = QOpenGLContext::currentContext();
      if(gl)
      {
        auto* f = gl->extraFunctions();
        // Delete old fence if exists
        if(m_fences[m_currentWriteSlot])
          f->glDeleteSync(m_fences[m_currentWriteSlot]);

        // Create new fence - this will be signaled when prior commands complete
        m_fences[m_currentWriteSlot] = f->glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        f->glFlush(); // Ensure fence is in the command stream
      }
    }

    m_tripleBuffer.publishWriteIndex();
    m_currentWriteSlot = -1;
  }

  QRhiTexture* acquireConsumerTexture(QRhiCommandBuffer* cb) override
  {
    int readSlot = m_tripleBuffer.acquireReadIndex();
    if(readSlot < 0 || readSlot >= 3)
      return m_consumerTextureWrappers[m_currentReadSlot];

    // Wait on the producer's fence before reading
    if(m_fences[readSlot] && m_consumer->makeThreadLocalNativeContextCurrent())
    {
      auto* gl = QOpenGLContext::currentContext();
      if(gl)
      {
        auto* f = gl->extraFunctions();
        // GPU-side wait: consumer GPU waits for producer GPU to finish
        f->glWaitSync(m_fences[readSlot], 0, GL_TIMEOUT_IGNORED);
      }
    }

    m_currentReadSlot = readSlot;
    return m_consumerTextureWrappers[readSlot];
  }
};
#endif

#if defined(Q_OS_WIN)
class TextureShareD3D11 : public TextureShareBackendBase
{
  // D3D11 shared texture handles
  ID3D11Texture2D* m_sharedD3D11Textures[3]{};
  HANDLE m_sharedHandles[3]{};
  IDXGIKeyedMutex* m_keyedMutexes[3]{};

  // Consumer-side opened textures
  ID3D11Texture2D* m_consumerD3D11Textures[3]{};
  IDXGIKeyedMutex* m_consumerKeyedMutexes[3]{};

  // Cached consumer QRhi texture wrappers (avoid per-frame recreation)
  QRhiTexture* m_consumerTextureWrappers[3]{};

  ID3D11Device* m_producerDevice{};
  ID3D11DeviceContext* m_producerContext{};
  ID3D11Device* m_consumerDevice{};
  ID3D11DeviceContext* m_consumerContext{};

  int m_currentReadSlot{0};
  int m_heldConsumerMutexSlot{-1}; // Slot whose mutex is currently held by consumer

  DXGI_FORMAT toDxgiFormat(QRhiTexture::Format format) const
  {
    switch(format)
    {
      case QRhiTexture::BGRA8:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
      case QRhiTexture::RGBA16F:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
      case QRhiTexture::RGBA32F:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
      default:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
  }

public:
  bool setupProducer(QRhi* producer, QSize size, QRhiTexture::Format format) override
  {
    m_producer = producer;
    m_size = size;
    m_format = format;
    m_producerReady.store(false, std::memory_order_release);

    // Get D3D11 device from QRhi
    auto* producerHandles
        = static_cast<const QRhiD3D11NativeHandles*>(producer->nativeHandles());
    if(!producerHandles)
      return false;

    m_producerDevice = static_cast<ID3D11Device*>(producerHandles->dev);
    m_producerDevice->GetImmediateContext(&m_producerContext);

    if(!createProducerResources())
      return false;

    // Create shared D3D11 textures with keyed mutex
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = size.width();
    desc.Height = size.height();
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = toDxgiFormat(format);
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    for(int i = 0; i < 3; ++i)
    {
      HRESULT hr
          = m_producerDevice->CreateTexture2D(&desc, nullptr, &m_sharedD3D11Textures[i]);
      if(FAILED(hr))
        return false;

      // Get shared handle
      IDXGIResource* dxgiResource{};
      hr = m_sharedD3D11Textures[i]->QueryInterface(
          __uuidof(IDXGIResource), (void**)&dxgiResource);
      if(FAILED(hr))
        return false;

      hr = dxgiResource->GetSharedHandle(&m_sharedHandles[i]);
      dxgiResource->Release();
      if(FAILED(hr))
        return false;

      // Get keyed mutex for producer
      hr = m_sharedD3D11Textures[i]->QueryInterface(
          __uuidof(IDXGIKeyedMutex), (void**)&m_keyedMutexes[i]);
      if(FAILED(hr))
        return false;

      // Initialize the keyed mutex to a known state (key 0)
      // The first acquire on a fresh keyed mutex should succeed with any key
      hr = m_keyedMutexes[i]->AcquireSync(0, 0);
      if(hr == S_OK)
      {
        m_keyedMutexes[i]->ReleaseSync(0);
        qDebug() << "D3D11: Initialized mutex" << i << "to key 0";
      }
      else
      {
        qWarning() << "D3D11: Failed to initialize mutex" << i << "hr=" << hr;
      }

      // Create QRhi wrapper for shared texture
      m_sharedTextures[i] = m_producer->newTexture(m_format, m_size, 1, {});
      m_sharedTextures[i]->createFrom({quint64(m_sharedD3D11Textures[i]), 0});
    }

    m_producerReady.store(true, std::memory_order_release);
    return true;
  }

  bool setupConsumer(QRhi* consumer) override
  {
    if(!m_producerReady.load(std::memory_order_acquire))
    {
      qWarning() << "TextureShareD3D11: setupConsumer called before producer is ready";
      return false;
    }

    m_consumer = consumer;

    auto* consumerHandles
        = static_cast<const QRhiD3D11NativeHandles*>(consumer->nativeHandles());
    if(!consumerHandles)
      return false;

    m_consumerDevice = static_cast<ID3D11Device*>(consumerHandles->dev);
    m_consumerDevice->GetImmediateContext(&m_consumerContext);

    for(int i = 0; i < 3; ++i)
    {
      // Open shared texture on consumer device using the shared handle
      HRESULT hr = m_consumerDevice->OpenSharedResource(
          m_sharedHandles[i], __uuidof(ID3D11Texture2D),
          (void**)&m_consumerD3D11Textures[i]);
      if(FAILED(hr))
        return false;

      // Get keyed mutex for consumer
      hr = m_consumerD3D11Textures[i]->QueryInterface(
          __uuidof(IDXGIKeyedMutex), (void**)&m_consumerKeyedMutexes[i]);
      if(FAILED(hr))
        return false;

      // Pre-create consumer texture wrappers (avoids per-frame recreation)
      m_consumerTextureWrappers[i] = m_consumer->newTexture(m_format, m_size, 1, {});
      m_consumerTextureWrappers[i]->createFrom({quint64(m_consumerD3D11Textures[i]), 0});
    }

    m_consumerTexture = m_consumerTextureWrappers[0];
    m_consumerReady.store(true, std::memory_order_release);
    return true;
  }

  bool
  setup(QRhi* producer, QRhi* consumer, QSize size, QRhiTexture::Format format) override
  {
    // Convenience method - sets up both sides from calling thread
    if(!setupProducer(producer, size, format))
      return false;
    return setupConsumer(consumer);
  }

  void resize(QSize newSize) override
  {
    if(m_size == newSize)
      return;
    cleanup();
    setup(m_producer, m_consumer, newSize, m_format);
  }

  void cleanup() override
  {
    m_producerReady.store(false, std::memory_order_release);
    m_consumerReady.store(false, std::memory_order_release);

    // Release any held consumer mutex before cleanup
    if(m_heldConsumerMutexSlot >= 0 && m_consumerKeyedMutexes[m_heldConsumerMutexSlot])
    {
      m_consumerKeyedMutexes[m_heldConsumerMutexSlot]->ReleaseSync(0);
      m_heldConsumerMutexSlot = -1;
    }

    // Delete consumer texture wrappers first
    for(int i = 0; i < 3; ++i)
    {
      delete m_consumerTextureWrappers[i];
      m_consumerTextureWrappers[i] = nullptr;
    }
    m_consumerTexture = nullptr;

    destroyProducerResources();

    for(int i = 0; i < 3; ++i)
    {
      if(m_consumerKeyedMutexes[i])
        m_consumerKeyedMutexes[i]->Release();
      m_consumerKeyedMutexes[i] = nullptr;

      if(m_consumerD3D11Textures[i])
        m_consumerD3D11Textures[i]->Release();
      m_consumerD3D11Textures[i] = nullptr;

      if(m_keyedMutexes[i])
        m_keyedMutexes[i]->Release();
      m_keyedMutexes[i] = nullptr;

      if(m_sharedD3D11Textures[i])
        m_sharedD3D11Textures[i]->Release();
      m_sharedD3D11Textures[i] = nullptr;

      delete m_sharedTextures[i];
      m_sharedTextures[i] = nullptr;
    }

    if(m_producerContext)
      m_producerContext->Release();
    m_producerContext = nullptr;
    if(m_consumerContext)
      m_consumerContext->Release();
    m_consumerContext = nullptr;
  }

  void endProducerFrame(QRhiCommandBuffer* cb) override
  {
    if(m_currentWriteSlot < 0)
      return;

    // Use beginExternal to flush QRhi's command buffer to the D3D11 context
    // This ensures render commands execute before our CopyResource
    cb->beginExternal();

    // Handle the case where consumer skipped this slot (triple buffering allows this).
    // If we previously released this slot with key 1 but consumer never acquired it,
    // the slot is stuck at key 1. Reset it to key 0 first.
    HRESULT hrReset = m_keyedMutexes[m_currentWriteSlot]->AcquireSync(1, 0);
    if(hrReset == S_OK)
    {
      // Consumer never touched this slot, reset to key 0
      m_keyedMutexes[m_currentWriteSlot]->ReleaseSync(0);
    }

    // Now acquire with key 0 (either from consumer release or our reset above)
    // Note: Must check for S_OK specifically, not SUCCEEDED(), because
    // SUCCEEDED(WAIT_TIMEOUT) is true but the mutex wasn't acquired!
    HRESULT hr = m_keyedMutexes[m_currentWriteSlot]->AcquireSync(0, 1000);
    if(hr == WAIT_TIMEOUT)
    {
      qWarning() << "D3D11 Producer: AcquireSync timeout on slot" << m_currentWriteSlot;
      cb->endExternal();
      m_currentWriteSlot = -1;
      return;
    }
    if(hr == S_OK)
    {
      // Direct GPU copy on the D3D11 context (now after render commands)
      auto srcNative = m_producerSlots[m_currentWriteSlot].texture->nativeTexture();
      ID3D11Texture2D* srcTex = reinterpret_cast<ID3D11Texture2D*>(srcNative.object);
      m_producerContext->CopyResource(m_sharedD3D11Textures[m_currentWriteSlot], srcTex);

      // Release mutex
      m_keyedMutexes[m_currentWriteSlot]->ReleaseSync(1);
      qDebug() << "D3D11 Producer: wrote slot" << m_currentWriteSlot;
    }
    else
    {
      qWarning() << "D3D11 Producer: AcquireSync failed with hr" << hr << "on slot"
                 << m_currentWriteSlot;
    }

    // Restore QRhi state
    cb->endExternal();

    m_tripleBuffer.publishWriteIndex();
    m_currentWriteSlot = -1;
  }

  QRhiTexture* acquireConsumerTexture(QRhiCommandBuffer* cb) override
  {
    // Check if consumer is ready
    if(!m_consumerReady.load(std::memory_order_acquire))
      return nullptr;

    int readSlot = m_tripleBuffer.acquireReadIndex();
    if(readSlot < 0 || readSlot >= 3)
    {
      // Return texture only if we have its mutex held
      if(m_heldConsumerMutexSlot >= 0)
        return m_consumerTextureWrappers[m_currentReadSlot];
      return nullptr;
    }

    // If we're requesting the same slot we already hold, just return it
    if(readSlot == m_heldConsumerMutexSlot)
      return m_consumerTextureWrappers[m_currentReadSlot];

    // Try to acquire mutex for the new slot (non-blocking)
    // Note: Must check for S_OK specifically, not SUCCEEDED(), because
    // SUCCEEDED(WAIT_TIMEOUT) is true but the mutex wasn't acquired!
    HRESULT hr = m_consumerKeyedMutexes[readSlot]->AcquireSync(1, 0);
    if(hr == S_OK)
    {
      qDebug() << "D3D11 Consumer: acquired slot" << readSlot << ", releasing old slot"
               << m_heldConsumerMutexSlot;
      // Successfully acquired new slot - now release the old one
      if(m_heldConsumerMutexSlot >= 0)
      {
        m_consumerKeyedMutexes[m_heldConsumerMutexSlot]->ReleaseSync(0);
      }

      m_currentReadSlot = readSlot;
      m_heldConsumerMutexSlot = readSlot;
    }
    else
    {
      static int failCount = 0;
      if(++failCount % 60 == 1) // Log once per second at 60fps
        qDebug() << "D3D11 Consumer: AcquireSync failed for slot" << readSlot
                 << ", keeping slot" << m_heldConsumerMutexSlot;
    }

    // Only return texture if we have a mutex held
    if(m_heldConsumerMutexSlot >= 0)
      return m_consumerTextureWrappers[m_currentReadSlot];
    qDebug() << "D3D11 Consumer: no mutex held, returning nullptr";
    return nullptr;
  }
};

class TextureShareD3D12 : public TextureShareBackendBase
{
  // D3D12 shared resources
  ID3D12Resource* m_sharedD3D12Resources[3]{};
  HANDLE m_sharedHandles[3]{};

  // Single shared fence for all slots (simpler than per-slot fences)
  ID3D12Fence* m_sharedFence{};
  HANDLE m_sharedFenceHandle{};
  UINT64 m_producerFenceValue{0};
  UINT64 m_lastPublishedFenceValue[3]{0, 0, 0};

  // Consumer-side opened resources
  ID3D12Resource* m_consumerD3D12Resources[3]{};
  ID3D12Fence* m_consumerFence{};

  // Cached consumer texture wrappers (avoid per-frame recreation)
  QRhiTexture* m_consumerTextureWrappers[3]{};

  ID3D12Device* m_producerDevice{};
  ID3D12CommandQueue* m_producerQueue{};
  ID3D12Device* m_consumerDevice{};
  ID3D12CommandQueue* m_consumerQueue{};

  int m_currentReadSlot{0};
  int m_pendingSignalSlot{-1};

  DXGI_FORMAT toDxgiFormat(QRhiTexture::Format format) const
  {
    switch(format)
    {
      case QRhiTexture::BGRA8:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
      case QRhiTexture::RGBA16F:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
      case QRhiTexture::RGBA32F:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
      default:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    }
  }

public:
  bool setupProducer(QRhi* producer, QSize size, QRhiTexture::Format format) override
  {
    m_producer = producer;
    m_size = size;
    m_format = format;
    m_producerReady.store(false, std::memory_order_release);

    // Get D3D12 device from QRhi
    auto* producerHandles
        = static_cast<const QRhiD3D12NativeHandles*>(producer->nativeHandles());
    if(!producerHandles)
      return false;

    m_producerDevice = static_cast<ID3D12Device*>(producerHandles->dev);
    m_producerQueue = static_cast<ID3D12CommandQueue*>(producerHandles->commandQueue);

    if(!m_producerDevice)
      return false;

    if(!createProducerResources())
      return false;

    // Create single shared fence for synchronization
    HRESULT hr = m_producerDevice->CreateFence(
        0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_sharedFence));
    if(FAILED(hr))
      return false;

    hr = m_producerDevice->CreateSharedHandle(
        m_sharedFence, nullptr, GENERIC_ALL, nullptr, &m_sharedFenceHandle);
    if(FAILED(hr))
      return false;

    // Create shared D3D12 textures
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = size.width();
    resourceDesc.Height = size.height();
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = toDxgiFormat(format);
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    for(int i = 0; i < 3; ++i)
    {
      // Create shared texture
      hr = m_producerDevice->CreateCommittedResource(
          &heapProps, D3D12_HEAP_FLAG_SHARED, &resourceDesc, D3D12_RESOURCE_STATE_COMMON,
          nullptr, IID_PPV_ARGS(&m_sharedD3D12Resources[i]));
      if(FAILED(hr))
        return false;

      // Create shared handle for the texture
      hr = m_producerDevice->CreateSharedHandle(
          m_sharedD3D12Resources[i], nullptr, GENERIC_ALL, nullptr, &m_sharedHandles[i]);
      if(FAILED(hr))
        return false;

      // Create QRhi wrappers for producer
      m_sharedTextures[i] = m_producer->newTexture(m_format, m_size, 1, {});
      m_sharedTextures[i]->createFrom(
          {quint64(m_sharedD3D12Resources[i]), D3D12_RESOURCE_STATE_COMMON});

      m_lastPublishedFenceValue[i] = 0;
    }

    m_producerReady.store(true, std::memory_order_release);
    return true;
  }

  bool setupConsumer(QRhi* consumer) override
  {
    if(!m_producerReady.load(std::memory_order_acquire))
    {
      qWarning() << "TextureShareD3D12: setupConsumer called before producer is ready";
      return false;
    }

    m_consumer = consumer;

    auto* consumerHandles
        = static_cast<const QRhiD3D12NativeHandles*>(consumer->nativeHandles());
    if(!consumerHandles)
      return false;

    m_consumerDevice = static_cast<ID3D12Device*>(consumerHandles->dev);
    m_consumerQueue = static_cast<ID3D12CommandQueue*>(consumerHandles->commandQueue);

    if(!m_consumerDevice)
      return false;

    // Open shared fence on consumer device
    HRESULT hr = m_consumerDevice->OpenSharedHandle(
        m_sharedFenceHandle, IID_PPV_ARGS(&m_consumerFence));
    if(FAILED(hr))
      return false;

    for(int i = 0; i < 3; ++i)
    {
      // Open shared texture on consumer device using the shared handle
      hr = m_consumerDevice->OpenSharedHandle(
          m_sharedHandles[i], IID_PPV_ARGS(&m_consumerD3D12Resources[i]));
      if(FAILED(hr))
        return false;

      // Pre-create consumer texture wrappers (avoids per-frame recreation)
      m_consumerTextureWrappers[i] = m_consumer->newTexture(m_format, m_size, 1, {});
      m_consumerTextureWrappers[i]->createFrom(
          {quint64(m_consumerD3D12Resources[i]), D3D12_RESOURCE_STATE_COMMON});
    }

    m_consumerTexture = m_consumerTextureWrappers[0];
    m_consumerReady.store(true, std::memory_order_release);
    return true;
  }

  bool
  setup(QRhi* producer, QRhi* consumer, QSize size, QRhiTexture::Format format) override
  {
    // Convenience method - sets up both sides from calling thread
    if(!setupProducer(producer, size, format))
      return false;
    return setupConsumer(consumer);
  }

  void resize(QSize newSize) override
  {
    if(m_size == newSize)
      return;
    cleanup();
    setup(m_producer, m_consumer, newSize, m_format);
  }

  void cleanup() override
  {
    m_producerReady.store(false, std::memory_order_release);
    m_consumerReady.store(false, std::memory_order_release);

    // Delete consumer texture wrappers
    for(int i = 0; i < 3; ++i)
    {
      delete m_consumerTextureWrappers[i];
      m_consumerTextureWrappers[i] = nullptr;
    }
    m_consumerTexture = nullptr;

    destroyProducerResources();

    for(int i = 0; i < 3; ++i)
    {
      if(m_consumerD3D12Resources[i])
        m_consumerD3D12Resources[i]->Release();
      m_consumerD3D12Resources[i] = nullptr;

      if(m_sharedHandles[i])
        CloseHandle(m_sharedHandles[i]);
      m_sharedHandles[i] = nullptr;

      if(m_sharedD3D12Resources[i])
        m_sharedD3D12Resources[i]->Release();
      m_sharedD3D12Resources[i] = nullptr;

      delete m_sharedTextures[i];
      m_sharedTextures[i] = nullptr;
    }

    if(m_consumerFence)
      m_consumerFence->Release();
    m_consumerFence = nullptr;

    if(m_sharedFenceHandle)
      CloseHandle(m_sharedFenceHandle);
    m_sharedFenceHandle = nullptr;

    if(m_sharedFence)
      m_sharedFence->Release();
    m_sharedFence = nullptr;
  }

  void endProducerFrame(QRhiCommandBuffer* cb) override
  {
    if(m_currentWriteSlot < 0)
      return;

    // First, signal fence for any pending slot from PREVIOUS frame
    // This ensures the signal happens AFTER QRhi submitted the previous frame's commands
    if(m_pendingSignalSlot >= 0 && m_producerQueue)
    {
      m_producerFenceValue++;
      m_producerQueue->Signal(m_sharedFence, m_producerFenceValue);
      m_lastPublishedFenceValue[m_pendingSignalSlot] = m_producerFenceValue;
      m_pendingSignalSlot = -1;
    }

    // Record GPU copy: producer render target → shared texture
    QRhiResourceUpdateBatch* batch = m_producer->nextResourceUpdateBatch();
    QRhiTextureCopyDescription copyDesc;
    batch->copyTexture(
        m_sharedTextures[m_currentWriteSlot],
        m_producerSlots[m_currentWriteSlot].texture, copyDesc);
    cb->resourceUpdate(batch);

    // Mark this slot as pending signal (will be signaled NEXT frame)
    m_pendingSignalSlot = m_currentWriteSlot;

    m_tripleBuffer.publishWriteIndex();
    m_currentWriteSlot = -1;
  }

  QRhiTexture* acquireConsumerTexture(QRhiCommandBuffer* cb) override
  {
    int readSlot = m_tripleBuffer.acquireReadIndex();
    if(readSlot < 0 || readSlot >= 3)
      return m_consumerTextureWrappers[m_currentReadSlot];

    // Check if this slot has been signaled (from a previous producer frame)
    UINT64 requiredValue = m_lastPublishedFenceValue[readSlot];
    if(requiredValue > 0)
    {
      UINT64 completedValue = m_consumerFence->GetCompletedValue();
      if(completedValue < requiredValue)
      {
        // GPU wait - consumer queue waits for producer fence
        if(m_consumerQueue)
        {
          m_consumerQueue->Wait(m_consumerFence, requiredValue);
        }
      }
    }

    m_currentReadSlot = readSlot;
    return m_consumerTextureWrappers[readSlot];
  }
};
#endif // Q_OS_WIN

#if QT_HAS_VULKAN
// Qt < 6.6 does not expose the QVulkanInstance in QRhiVulkanNativeHandles;
// score always creates its Vulkan QRhi from staticVulkanInstance().
static QVulkanInstance* rhiVulkanInstance(const QRhiVulkanNativeHandles* h)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  if(h->inst)
    return h->inst;
#endif
  return score::gfx::staticVulkanInstance(false);
}

class TextureShareVulkan : public TextureShareBackendBase
{
  // Helper handle type selection (matches the original platform ifdef)
#if defined(Q_OS_WIN)
  static constexpr VkExternalMemoryHandleTypeFlagBits k_extMemHandleType
      = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
  static constexpr VkExternalMemoryHandleTypeFlagBits k_extMemHandleType
      = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

  // Producer-side Vulkan resources (image + memory bundled by helper)
  vkinterop::ExternalImage m_sharedImages[3]{};
  VkSemaphore m_producerSemaphore{};

  // External memory handles for sharing (one per slot)
  vkinterop::ExternalHandle m_memoryHandles[3]{};

  // Semaphore handle for sharing
#if defined(Q_OS_WIN)
  HANDLE m_semaphoreHandle{};
#else
  int m_semaphoreFd{-1};
#endif

  // Consumer-side imported resources
  vkinterop::ExternalImage m_consumerImages[3]{};
  VkSemaphore m_consumerSemaphore{};

  // Cached consumer texture wrappers
  QRhiTexture* m_consumerTextureWrappers[3]{};

  // Device handles
  VkDevice m_producerVkDevice{};
  VkDevice m_consumerVkDevice{};
  VkPhysicalDevice m_producerPhysDev{};
  VkPhysicalDevice m_consumerPhysDev{};
  VkQueue m_producerQueue{};
  VkQueue m_consumerQueue{};

  // Vulkan contexts for the helper (one per device)
  vkinterop::VulkanCtx m_producerCtx{};
  vkinterop::VulkanCtx m_consumerCtx{};

  // Vulkan functions from QVulkanInstance
  QVulkanDeviceFunctions* m_producerFuncs{};
  QVulkanDeviceFunctions* m_consumerFuncs{};
  QVulkanFunctions* m_instFuncs{};

  // Semaphore timeline values
  uint64_t m_semaphoreValue{0};
  uint64_t m_lastPublishedValue[3]{0, 0, 0};

  // Flag to track if external semaphores are supported
  bool m_hasExternalSemaphores{false};

  int m_currentReadSlot{0};
  int m_pendingSignalSlot{-1};

  // Function pointers for semaphore extensions (memory PFNs are handled by the helper)
#if defined(Q_OS_WIN)
  PFN_vkGetSemaphoreWin32HandleKHR pfnGetSemaphoreWin32HandleKHR{};
  PFN_vkImportSemaphoreWin32HandleKHR pfnImportSemaphoreWin32HandleKHR{};
#else
  PFN_vkGetSemaphoreFdKHR pfnGetSemaphoreFdKHR{};
  PFN_vkImportSemaphoreFdKHR pfnImportSemaphoreFdKHR{};
#endif

  VkFormat toVkFormat(QRhiTexture::Format format) const
  {
    switch(format)
    {
      case QRhiTexture::RGBA8:
        return VK_FORMAT_R8G8B8A8_UNORM;
      case QRhiTexture::BGRA8:
        return VK_FORMAT_B8G8R8A8_UNORM;
      case QRhiTexture::RGBA16F:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
      case QRhiTexture::RGBA32F:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
      case QRhiTexture::R8:
        return VK_FORMAT_R8_UNORM;
      case QRhiTexture::R16:
        return VK_FORMAT_R16_UNORM;
      case QRhiTexture::R16F:
        return VK_FORMAT_R16_SFLOAT;
      case QRhiTexture::R32F:
        return VK_FORMAT_R32_SFLOAT;
      default:
        return VK_FORMAT_R8G8B8A8_UNORM;
    }
  }

  // Check if external timeline semaphores are supported
  bool checkExternalSemaphoreSupport(VkPhysicalDevice physDev, QVulkanInstance* vkInst)
  {
    auto pfnGetPhysicalDeviceExternalSemaphoreProperties
        = (PFN_vkGetPhysicalDeviceExternalSemaphoreProperties)vkInst
              ->getInstanceProcAddr("vkGetPhysicalDeviceExternalSemaphoreProperties");

    if(!pfnGetPhysicalDeviceExternalSemaphoreProperties)
      return false;

    VkPhysicalDeviceExternalSemaphoreInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_SEMAPHORE_INFO;
#if defined(Q_OS_WIN)
    semInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
    semInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

    VkExternalSemaphoreProperties semProps{};
    semProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_SEMAPHORE_PROPERTIES;

    pfnGetPhysicalDeviceExternalSemaphoreProperties(physDev, &semInfo, &semProps);

    // Check if exportable and importable
    bool supported = (semProps.externalSemaphoreFeatures
                      & VK_EXTERNAL_SEMAPHORE_FEATURE_EXPORTABLE_BIT)
                     && (semProps.externalSemaphoreFeatures
                         & VK_EXTERNAL_SEMAPHORE_FEATURE_IMPORTABLE_BIT);

    if(!supported)
    {
      qDebug(
          "TextureShare: External semaphores not supported, using triple buffering "
          "only");
    }

    return supported;
  }

public:
  bool setupProducer(QRhi* producer, QSize size, QRhiTexture::Format format) override
  {
    m_producer = producer;
    m_size = size;
    m_format = format;
    m_producerReady.store(false, std::memory_order_release);

    // Get Vulkan handles from QRhi
    auto* producerHandles
        = static_cast<const QRhiVulkanNativeHandles*>(producer->nativeHandles());
    if(!producerHandles)
      return false;

    m_producerVkDevice = producerHandles->dev;
    m_producerPhysDev = producerHandles->physDev;
    m_producerQueue = producerHandles->gfxQueue;

    if(!m_producerVkDevice)
      return false;

    // Get QVulkanInstance and device functions
    QVulkanInstance* vkInst = rhiVulkanInstance(producerHandles);
    if(!vkInst)
      return false;

    m_instFuncs = vkInst->functions();
    m_producerFuncs = vkInst->deviceFunctions(m_producerVkDevice);

    if(!m_instFuncs || !m_producerFuncs)
      return false;

    // Build the helper context for producer-side operations
    m_producerCtx.instance = vkInst->vkInstance();
    m_producerCtx.physDev = m_producerPhysDev;
    m_producerCtx.dev = m_producerVkDevice;
    m_producerCtx.qInst = vkInst;

    // Load producer-side semaphore extension function pointers
    // (memory PFNs are resolved internally by the helper)
#if defined(Q_OS_WIN)
    pfnGetSemaphoreWin32HandleKHR
        = (PFN_vkGetSemaphoreWin32HandleKHR)m_instFuncs->vkGetDeviceProcAddr(
            m_producerVkDevice, "vkGetSemaphoreWin32HandleKHR");
    if(!pfnGetSemaphoreWin32HandleKHR)
    {
      qWarning("TextureShare: Vulkan external semaphore extensions not available");
      return false;
    }
#else
    pfnGetSemaphoreFdKHR = (PFN_vkGetSemaphoreFdKHR)m_instFuncs->vkGetDeviceProcAddr(
        m_producerVkDevice, "vkGetSemaphoreFdKHR");
    // Semaphore functions are optional - we can still work without them
#endif

    if(!createProducerResources())
      return false;

    VkFormat vkFormat = toVkFormat(format);

    // Check if external semaphores are supported and create them if so
    // This is optional - without semaphores we rely on triple buffering only
    m_hasExternalSemaphores = checkExternalSemaphoreSupport(m_producerPhysDev, vkInst);

#if defined(Q_OS_WIN)
    if(m_hasExternalSemaphores && pfnGetSemaphoreWin32HandleKHR)
#else
    if(m_hasExternalSemaphores && pfnGetSemaphoreFdKHR)
#endif
    {
      // Create timeline semaphore for synchronization
      VkSemaphoreTypeCreateInfo timelineInfo{};
      timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
      timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
      timelineInfo.initialValue = 0;

      VkExportSemaphoreCreateInfo exportSemInfo{};
      exportSemInfo.sType = VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO;
      exportSemInfo.pNext = &timelineInfo;
#if defined(Q_OS_WIN)
      exportSemInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
#else
      exportSemInfo.handleTypes = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif

      VkSemaphoreCreateInfo semInfo{};
      semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      semInfo.pNext = &exportSemInfo;

      if(m_producerFuncs->vkCreateSemaphore(
             m_producerVkDevice, &semInfo, nullptr, &m_producerSemaphore)
         != VK_SUCCESS)
      {
        qDebug("TextureShare: Failed to create external semaphore, continuing without");
        m_hasExternalSemaphores = false;
      }
      else
      {
        // Export semaphore handle
#if defined(Q_OS_WIN)
        VkSemaphoreGetWin32HandleInfoKHR getSemHandleInfo{};
        getSemHandleInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_WIN32_HANDLE_INFO_KHR;
        getSemHandleInfo.semaphore = m_producerSemaphore;
        getSemHandleInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
        if(pfnGetSemaphoreWin32HandleKHR(
               m_producerVkDevice, &getSemHandleInfo, &m_semaphoreHandle)
           != VK_SUCCESS)
        {
          qDebug("TextureShare: Failed to export semaphore handle, continuing without");
          m_producerFuncs->vkDestroySemaphore(
              m_producerVkDevice, m_producerSemaphore, nullptr);
          m_producerSemaphore = VK_NULL_HANDLE;
          m_hasExternalSemaphores = false;
        }
#else
        VkSemaphoreGetFdInfoKHR getSemFdInfo{};
        getSemFdInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR;
        getSemFdInfo.semaphore = m_producerSemaphore;
        getSemFdInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        if(pfnGetSemaphoreFdKHR(m_producerVkDevice, &getSemFdInfo, &m_semaphoreFd)
           != VK_SUCCESS)
        {
          qDebug("TextureShare: Failed to export semaphore fd, continuing without");
          m_producerFuncs->vkDestroySemaphore(
              m_producerVkDevice, m_producerSemaphore, nullptr);
          m_producerSemaphore = VK_NULL_HANDLE;
          m_hasExternalSemaphores = false;
        }
#endif
      }
    }

    // Create shared images with external memory via the helper
    vkinterop::ExternalImageDesc imgDesc{};
    imgDesc.format = vkFormat;
    imgDesc.extent = {(uint32_t)size.width(), (uint32_t)size.height(), 1};
    imgDesc.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                    | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgDesc.handleType = k_extMemHandleType;
    imgDesc.dedicated = true;
    imgDesc.preferDeviceLocal = true;

    for(int i = 0; i < 3; ++i)
    {
      auto img = vkinterop::createExportableImage(m_producerCtx, imgDesc);
      if(!img)
        return false;
      m_sharedImages[i] = *img;

      auto handle = vkinterop::exportMemoryHandle(
          m_producerCtx, m_sharedImages[i].memory, k_extMemHandleType);
      if(!handle)
        return false;
      m_memoryHandles[i] = *handle;

      // Create QRhi wrapper for producer shared texture
      m_sharedTextures[i] = m_producer->newTexture(m_format, m_size, 1, {});
      if(!m_sharedTextures[i]->createFrom(
             {quint64(m_sharedImages[i].image), VK_IMAGE_LAYOUT_UNDEFINED}))
        return false;

      m_lastPublishedValue[i] = 0;
    }

    m_producerReady.store(true, std::memory_order_release);
    return true;
  }

  bool setupConsumer(QRhi* consumer) override
  {
    if(!m_producerReady.load(std::memory_order_acquire))
    {
      qWarning() << "TextureShareVulkan: setupConsumer called before producer is ready";
      return false;
    }

    m_consumer = consumer;

    auto* consumerHandles
        = static_cast<const QRhiVulkanNativeHandles*>(consumer->nativeHandles());
    if(!consumerHandles)
      return false;

    m_consumerVkDevice = consumerHandles->dev;
    m_consumerPhysDev = consumerHandles->physDev;
    m_consumerQueue = consumerHandles->gfxQueue;

    if(!m_consumerVkDevice)
      return false;

    // Get device functions for consumer
    QVulkanInstance* vkInst = rhiVulkanInstance(consumerHandles);
    if(!vkInst)
      return false;

    m_consumerFuncs = vkInst->deviceFunctions(m_consumerVkDevice);
    if(!m_consumerFuncs)
      return false;

    // Build the helper context for consumer-side operations
    m_consumerCtx.instance = vkInst->vkInstance();
    m_consumerCtx.physDev = m_consumerPhysDev;
    m_consumerCtx.dev = m_consumerVkDevice;
    m_consumerCtx.qInst = vkInst;

    // Only setup semaphores if external semaphores are supported
    if(m_hasExternalSemaphores)
    {
      // Load consumer extension function pointer
#if defined(Q_OS_WIN)
      pfnImportSemaphoreWin32HandleKHR
          = (PFN_vkImportSemaphoreWin32HandleKHR)m_instFuncs->vkGetDeviceProcAddr(
              m_consumerVkDevice, "vkImportSemaphoreWin32HandleKHR");
      if(!pfnImportSemaphoreWin32HandleKHR)
      {
        qDebug("TextureShare: Vulkan semaphore import extension not available");
        m_hasExternalSemaphores = false;
      }
#else
      pfnImportSemaphoreFdKHR
          = (PFN_vkImportSemaphoreFdKHR)m_instFuncs->vkGetDeviceProcAddr(
              m_consumerVkDevice, "vkImportSemaphoreFdKHR");
      if(!pfnImportSemaphoreFdKHR)
      {
        qDebug("TextureShare: Vulkan semaphore import extension not available");
        m_hasExternalSemaphores = false;
      }
#endif
    }

    if(m_hasExternalSemaphores)
    {
      // Create and import semaphore on consumer device
      VkSemaphoreTypeCreateInfo consumerTimelineInfo{};
      consumerTimelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
      consumerTimelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
      consumerTimelineInfo.initialValue = 0;

      VkSemaphoreCreateInfo consumerSemInfo{};
      consumerSemInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
      consumerSemInfo.pNext = &consumerTimelineInfo;

      if(m_consumerFuncs->vkCreateSemaphore(
             m_consumerVkDevice, &consumerSemInfo, nullptr, &m_consumerSemaphore)
         != VK_SUCCESS)
      {
        qDebug("TextureShare: Failed to create consumer semaphore");
        m_hasExternalSemaphores = false;
      }
      else
      {
#if defined(Q_OS_WIN)
        VkImportSemaphoreWin32HandleInfoKHR importSemInfo{};
        importSemInfo.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_WIN32_HANDLE_INFO_KHR;
        importSemInfo.semaphore = m_consumerSemaphore;
        importSemInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_WIN32_BIT;
        importSemInfo.handle = m_semaphoreHandle;
        if(pfnImportSemaphoreWin32HandleKHR(m_consumerVkDevice, &importSemInfo)
           != VK_SUCCESS)
        {
          qDebug("TextureShare: Failed to import semaphore");
          m_consumerFuncs->vkDestroySemaphore(
              m_consumerVkDevice, m_consumerSemaphore, nullptr);
          m_consumerSemaphore = VK_NULL_HANDLE;
          m_hasExternalSemaphores = false;
        }
#else
        VkImportSemaphoreFdInfoKHR importSemInfo{};
        importSemInfo.sType = VK_STRUCTURE_TYPE_IMPORT_SEMAPHORE_FD_INFO_KHR;
        importSemInfo.semaphore = m_consumerSemaphore;
        importSemInfo.handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
        importSemInfo.fd = m_semaphoreFd;
        if(pfnImportSemaphoreFdKHR(m_consumerVkDevice, &importSemInfo) != VK_SUCCESS)
        {
          qDebug("TextureShare: Failed to import semaphore fd");
          m_consumerFuncs->vkDestroySemaphore(
              m_consumerVkDevice, m_consumerSemaphore, nullptr);
          m_consumerSemaphore = VK_NULL_HANDLE;
          m_hasExternalSemaphores = false;
        }
        else
        {
          m_semaphoreFd = -1; // FD consumed by import
        }
#endif
      }
    }

    VkFormat vkFormat = toVkFormat(m_format);

    // Import memory on consumer device and create images via the helper
    vkinterop::ExternalImageDesc consumerImgDesc{};
    consumerImgDesc.format = vkFormat;
    consumerImgDesc.extent
        = {(uint32_t)m_size.width(), (uint32_t)m_size.height(), 1};
    consumerImgDesc.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    consumerImgDesc.tiling = VK_IMAGE_TILING_OPTIMAL;
    consumerImgDesc.handleType = k_extMemHandleType;
    consumerImgDesc.dedicated = true;
    consumerImgDesc.preferDeviceLocal = true;

    for(int i = 0; i < 3; ++i)
    {
      auto img = vkinterop::importExternalImage(
          m_consumerCtx, consumerImgDesc, m_memoryHandles[i]);
      if(!img)
        return false;
      m_consumerImages[i] = *img;

#if !defined(Q_OS_WIN)
      // On Linux, OPAQUE_FD ownership transferred to the consumer device on import.
      m_memoryHandles[i].fd = -1;
#endif
    }

    // Transition all consumer images to SHADER_READ_ONLY_OPTIMAL
    // This requires a command buffer submission
    {
      auto* consumerHandles
          = static_cast<const QRhiVulkanNativeHandles*>(m_consumer->nativeHandles());

      VkCommandPoolCreateInfo poolInfo{};
      poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
      poolInfo.queueFamilyIndex = consumerHandles->gfxQueueFamilyIdx;
      poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

      VkCommandPool cmdPool;
      if(m_consumerFuncs->vkCreateCommandPool(
             m_consumerVkDevice, &poolInfo, nullptr, &cmdPool)
         != VK_SUCCESS)
        return false;

      VkCommandBufferAllocateInfo allocInfo{};
      allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocInfo.commandPool = cmdPool;
      allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocInfo.commandBufferCount = 1;

      VkCommandBuffer cmdBuf;
      if(m_consumerFuncs->vkAllocateCommandBuffers(
             m_consumerVkDevice, &allocInfo, &cmdBuf)
         != VK_SUCCESS)
      {
        m_consumerFuncs->vkDestroyCommandPool(m_consumerVkDevice, cmdPool, nullptr);
        return false;
      }

      VkCommandBufferBeginInfo beginInfo{};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
      m_consumerFuncs->vkBeginCommandBuffer(cmdBuf, &beginInfo);

      // Transition all 3 images
      VkImageMemoryBarrier barriers[3]{};
      for(int i = 0; i < 3; ++i)
      {
        barriers[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[i].srcAccessMask = 0;
        barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barriers[i].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barriers[i].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[i].image = m_consumerImages[i].image;
        barriers[i].subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barriers[i].subresourceRange.baseMipLevel = 0;
        barriers[i].subresourceRange.levelCount = 1;
        barriers[i].subresourceRange.baseArrayLayer = 0;
        barriers[i].subresourceRange.layerCount = 1;
      }

      m_consumerFuncs->vkCmdPipelineBarrier(
          cmdBuf, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 3, barriers);

      m_consumerFuncs->vkEndCommandBuffer(cmdBuf);

      VkSubmitInfo submitInfo{};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.commandBufferCount = 1;
      submitInfo.pCommandBuffers = &cmdBuf;

      m_consumerFuncs->vkQueueSubmit(m_consumerQueue, 1, &submitInfo, VK_NULL_HANDLE);
      m_consumerFuncs->vkQueueWaitIdle(m_consumerQueue);

      m_consumerFuncs->vkDestroyCommandPool(m_consumerVkDevice, cmdPool, nullptr);
    }

    // Create consumer texture wrappers (images are now in correct layout)
    for(int i = 0; i < 3; ++i)
    {
      m_consumerTextureWrappers[i] = m_consumer->newTexture(m_format, m_size, 1, {});
      if(!m_consumerTextureWrappers[i]->createFrom(
             {quint64(m_consumerImages[i].image),
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}))
        return false;
    }

    m_consumerTexture = m_consumerTextureWrappers[0];
    m_consumerReady.store(true, std::memory_order_release);
    return true;
  }

  bool
  setup(QRhi* producer, QRhi* consumer, QSize size, QRhiTexture::Format format) override
  {
    // Convenience method - sets up both sides from calling thread
    if(!setupProducer(producer, size, format))
      return false;
    return setupConsumer(consumer);
  }

  void resize(QSize newSize) override
  {
    if(m_size == newSize)
      return;
    cleanup();
    setup(m_producer, m_consumer, newSize, m_format);
  }

  void cleanup() override
  {
    m_producerReady.store(false, std::memory_order_release);
    m_consumerReady.store(false, std::memory_order_release);

    // Delete consumer texture wrappers
    for(int i = 0; i < 3; ++i)
    {
      delete m_consumerTextureWrappers[i];
      m_consumerTextureWrappers[i] = nullptr;
    }
    m_consumerTexture = nullptr;

    destroyProducerResources();

    // Cleanup consumer Vulkan resources
    if(m_consumerFuncs)
    {
      for(int i = 0; i < 3; ++i)
      {
        vkinterop::destroyExternal(m_consumerCtx, m_consumerImages[i]);
      }

      if(m_consumerSemaphore)
        m_consumerFuncs->vkDestroySemaphore(
            m_consumerVkDevice, m_consumerSemaphore, nullptr);
      m_consumerSemaphore = VK_NULL_HANDLE;
    }

    // Cleanup producer Vulkan resources
    if(m_producerFuncs)
    {
      for(int i = 0; i < 3; ++i)
      {
#if defined(Q_OS_WIN)
        if(m_memoryHandles[i].handle)
          CloseHandle(m_memoryHandles[i].handle);
#endif
        m_memoryHandles[i] = {};

        vkinterop::destroyExternal(m_producerCtx, m_sharedImages[i]);

        delete m_sharedTextures[i];
        m_sharedTextures[i] = nullptr;
      }

#if defined(Q_OS_WIN)
      if(m_semaphoreHandle)
        CloseHandle(m_semaphoreHandle);
      m_semaphoreHandle = nullptr;
#endif

      if(m_producerSemaphore)
        m_producerFuncs->vkDestroySemaphore(
            m_producerVkDevice, m_producerSemaphore, nullptr);
      m_producerSemaphore = VK_NULL_HANDLE;
    }
  }

  void endProducerFrame(QRhiCommandBuffer* cb) override
  {
    if(m_currentWriteSlot < 0)
      return;

    // Signal semaphore for previous slot (deferred signal pattern, like D3D12)
    // Only if external semaphores are supported
    if(m_hasExternalSemaphores && m_pendingSignalSlot >= 0 && m_producerFuncs)
    {
      m_semaphoreValue++;

      // Submit a semaphore signal via timeline semaphore
      VkTimelineSemaphoreSubmitInfo timelineInfo{};
      timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
      timelineInfo.signalSemaphoreValueCount = 1;
      timelineInfo.pSignalSemaphoreValues = &m_semaphoreValue;

      VkSubmitInfo submitInfo{};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
      submitInfo.pNext = &timelineInfo;
      submitInfo.signalSemaphoreCount = 1;
      submitInfo.pSignalSemaphores = &m_producerSemaphore;

      m_producerFuncs->vkQueueSubmit(m_producerQueue, 1, &submitInfo, VK_NULL_HANDLE);

      m_lastPublishedValue[m_pendingSignalSlot] = m_semaphoreValue;
      m_pendingSignalSlot = -1;
    }

    // GPU copy: producer render target → shared texture
    QRhiResourceUpdateBatch* batch = m_producer->nextResourceUpdateBatch();
    QRhiTextureCopyDescription copyDesc;
    batch->copyTexture(
        m_sharedTextures[m_currentWriteSlot],
        m_producerSlots[m_currentWriteSlot].texture, copyDesc);
    cb->resourceUpdate(batch);

    // Mark this slot for deferred signal (only if using semaphores)
    if(m_hasExternalSemaphores)
      m_pendingSignalSlot = m_currentWriteSlot;

    m_tripleBuffer.publishWriteIndex();
    m_currentWriteSlot = -1;
  }

  QRhiTexture* acquireConsumerTexture(QRhiCommandBuffer* cb) override
  {
    int readSlot = m_tripleBuffer.acquireReadIndex();
    if(readSlot < 0 || readSlot >= 3)
      return m_consumerTextureWrappers[m_currentReadSlot];

    // GPU wait only if external semaphores are supported
    if(m_hasExternalSemaphores)
    {
      uint64_t requiredValue = m_lastPublishedValue[readSlot];
      if(requiredValue > 0 && m_consumerFuncs)
      {
        // GPU wait: consumer waits on producer's semaphore
        VkTimelineSemaphoreSubmitInfo timelineInfo{};
        timelineInfo.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
        timelineInfo.waitSemaphoreValueCount = 1;
        timelineInfo.pWaitSemaphoreValues = &requiredValue;

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pNext = &timelineInfo;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &m_consumerSemaphore;
        submitInfo.pWaitDstStageMask = &waitStage;

        m_consumerFuncs->vkQueueSubmit(m_consumerQueue, 1, &submitInfo, VK_NULL_HANDLE);
      }
    }
    // Without semaphores, we rely on triple buffering to avoid major sync issues.
    // There may be occasional visual artifacts but it will still work.

    m_currentReadSlot = readSlot;
    return m_consumerTextureWrappers[readSlot];
  }
};
#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
// Forward declaration - implementation in TextureShareMetal.mm
std::unique_ptr<TextureShareBackend>
createTextureShareBackendMetal(QRhi* producer, QRhi* consumer);
#endif

struct TextureShare::Impl
{
  std::unique_ptr<TextureShareBackend> backend;
  std::atomic<bool> backendCreated{false};
};

TextureShare::TextureShare()
    : m_impl{std::make_unique<Impl>()}
{
}

TextureShare::~TextureShare()
{
  cleanup();
}

bool TextureShare::setup(
    QRhi* producer, QRhi* consumer, QSize size, QRhiTexture::Format format)
{
  if(!producer || !consumer)
    return false;

  // Both must use the same backend
  if(producer->backend() != consumer->backend())
  {
    qWarning()
        << "TextureShare: producer and consumer must use the same graphics backend";
    return false;
  }

  m_impl->backend = createTextureShareBackend(producer, consumer);
  if(!m_impl->backend)
    return false;

  bool result = m_impl->backend->setup(producer, consumer, size, format);

  // Signal that backend is fully initialized and safe to access from other threads
  // This MUST be after setup() completes to avoid races on backend members
  if(result)
    m_impl->backendCreated.store(true, std::memory_order_release);

  return result;
}

bool TextureShare::setupProducer(QRhi* producer, QSize size, QRhiTexture::Format format)
{
  if(!producer)
    return false;

  m_impl->backend = createTextureShareBackend(producer, nullptr);
  if(!m_impl->backend)
    return false;

  bool result = m_impl->backend->setupProducer(producer, size, format);

  // Signal that backend is fully initialized and safe to access from other threads
  // This MUST be after setupProducer() completes to avoid races on backend members
  if(result)
    m_impl->backendCreated.store(true, std::memory_order_release);

  return result;
}

bool TextureShare::setupConsumer(QRhi* consumer)
{
  if(!consumer)
    return false;

  if(!m_impl->backend)
  {
    qWarning() << "TextureShare::setupConsumer: setupProducer must be called first";
    return false;
  }

  return m_impl->backend->setupConsumer(consumer);
}

void TextureShare::resize(QSize newSize)
{
  if(m_impl->backend)
    m_impl->backend->resize(newSize);
}

void TextureShare::cleanup()
{
  // First signal that backend is no longer safe to access
  m_impl->backendCreated.store(false, std::memory_order_release);

  if(m_impl->backend)
  {
    m_impl->backend->cleanup();
    m_impl->backend.reset();
  }
}

bool TextureShare::isValid() const noexcept
{
  if(!m_impl->backendCreated.load(std::memory_order_acquire))
    return false;
  return m_impl->backend && m_impl->backend->isValid();
}

QSize TextureShare::size() const noexcept
{
  if(!m_impl->backendCreated.load(std::memory_order_acquire))
    return QSize{};
  return m_impl->backend ? m_impl->backend->size() : QSize{};
}

QRhiTextureRenderTarget* TextureShare::beginProducerFrame()
{
  return m_impl->backend ? m_impl->backend->beginProducerFrame() : nullptr;
}

QRhiTexture* TextureShare::currentProducerTexture()
{
  return m_impl->backend ? m_impl->backend->currentProducerTexture() : nullptr;
}

void TextureShare::endProducerFrame(QRhiCommandBuffer* cb)
{
  if(m_impl->backend)
    m_impl->backend->endProducerFrame(cb);
}

QRhiRenderPassDescriptor* TextureShare::producerRenderPassDescriptor() const noexcept
{
  return m_impl->backend ? m_impl->backend->producerRenderPassDescriptor() : nullptr;
}

QRhiTexture* TextureShare::acquireConsumerTexture(QRhiCommandBuffer* cb)
{
  if(!m_impl->backendCreated.load(std::memory_order_acquire))
    return nullptr;
  return m_impl->backend ? m_impl->backend->acquireConsumerTexture(cb) : nullptr;
}

bool TextureShare::hasNewFrame() const noexcept
{
  if(!m_impl->backendCreated.load(std::memory_order_acquire))
    return false;
  return m_impl->backend && m_impl->backend->hasNewFrame();
}

std::unique_ptr<TextureShareBackend>
createTextureShareBackend(QRhi* producer, QRhi* consumer)
{
  switch(producer->backend())
  {
#if QT_CONFIG(opengl)
    case QRhi::OpenGLES2:
      return std::make_unique<TextureShareOpenGL>();
#endif

#if defined(Q_OS_WIN)
    case QRhi::D3D11:
      return std::make_unique<TextureShareD3D11>();

    case QRhi::D3D12:
      return std::make_unique<TextureShareD3D12>();
#endif

#if QT_HAS_VULKAN
    case QRhi::Vulkan:
      return std::make_unique<TextureShareVulkan>();
#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
    case QRhi::Metal:
      return createTextureShareBackendMetal(producer, consumer);
#else
    case QRhi::Metal:
      qWarning() << "TextureShare: Metal backend not available on this platform";
      return nullptr;
#endif

    default:
      qWarning() << "TextureShare: unsupported backend" << producer->backend();
      return nullptr;
  }
}

} // namespace score::gfx
