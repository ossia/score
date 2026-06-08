#pragma once

#include <score_plugin_gfx_export.h>

#include <QSize>
#include <QtGui/private/qrhi_p.h>

#include <atomic>
#include <memory>
#include <mutex>

namespace score::gfx
{

/**
 * @brief GPU-only triple-buffered texture sharing between two QRhi instances in different threads.
 */
class SCORE_PLUGIN_GFX_EXPORT TextureShare
{
public:
  TextureShare();
  ~TextureShare();

  TextureShare(const TextureShare&) = delete;
  TextureShare& operator=(const TextureShare&) = delete;
  TextureShare(TextureShare&&) = delete;
  TextureShare& operator=(TextureShare&&) = delete;

  /**
   * @brief Initialize the texture sharing system (single-thread convenience method).
   *
   * @param producer The QRhi instance that will produce/render textures
   * @param consumer The QRhi instance that will consume/display textures
   * @param size The texture size
   * @param format The texture format (default RGBA8)
   * @return true if setup succeeded
   *
   * Note: Both QRhi instances must use the same graphics backend.
   *
   * WARNING: This method calls both QRhi instances from the calling thread.
   * Per QRhi threading rules, each QRhi should only be used from the thread
   * that created it. Use setupProducer() and setupConsumer() for correct
   * multi-threaded operation.
   */
  bool setup(
      QRhi* producer, QRhi* consumer, QSize size,
      QRhiTexture::Format format = QRhiTexture::RGBA8);

  /**
   * @brief Initialize producer-side resources (call from producer thread).
   *
   * @param producer The QRhi instance that will produce/render textures
   * @param size The texture size
   * @param format The texture format (default RGBA8)
   * @return true if producer setup succeeded
   *
   * This method creates all producer-side resources using the producer QRhi.
   * After this returns, call setupConsumer() from the consumer thread.
   *
   * Thread Safety: Must be called from the producer thread (the thread
   * that created the producer QRhi instance).
   */
  bool setupProducer(
      QRhi* producer, QSize size,
      QRhiTexture::Format format = QRhiTexture::RGBA8);

  /**
   * @brief Initialize consumer-side resources (call from consumer thread).
   *
   * @param consumer The QRhi instance that will consume/display textures
   * @return true if consumer setup succeeded
   *
   * This method creates all consumer-side resources using the consumer QRhi.
   * Must be called after setupProducer() has completed.
   *
   * Thread Safety: Must be called from the consumer thread (the thread
   * that created the consumer QRhi instance).
   */
  bool setupConsumer(QRhi* consumer);

  /**
   * @brief Resize the shared textures.
   *
   * Must be called when both threads are synchronized.
   */
  void resize(QSize newSize);

  /**
   * @brief Clean up all resources.
   *
   * Must be called before destroying the QRhi instances.
   */
  void cleanup();

  /**
   * @brief Check if the system is properly set up.
   */
  bool isValid() const noexcept;

  /**
   * @brief Get the current texture size.
   */
  QSize size() const noexcept;

  // === Producer Thread API ===

  /**
   * @brief Begin a producer frame.
   *
   * @return The render target to render to, or nullptr if not ready.
   *
   * The returned render target is owned by TextureShare.
   * Call endProducerFrame() when rendering is complete.
   *
   * Must be called from the producer thread.
   */
  QRhiTextureRenderTarget* beginProducerFrame();

  /**
   * @brief Get the current producer texture.
   *
   * @return The texture being rendered to, or nullptr if not in a frame.
   *
   * Must be called after beginProducerFrame() and before endProducerFrame().
   */
  QRhiTexture* currentProducerTexture();

  /**
   * @brief End a producer frame and submit GPU copy to shared buffer.
   *
   * @param cb The command buffer to record the copy command into.
   *
   * This records a GPU-to-GPU copy from the render target to the shared
   * intermediate texture. The copy happens entirely on the GPU.
   *
   * Must be called from the producer thread.
   */
  void endProducerFrame(QRhiCommandBuffer* cb);

  /**
   * @brief Get a render pass descriptor compatible with producer textures.
   */
  QRhiRenderPassDescriptor* producerRenderPassDescriptor() const noexcept;

  // === Consumer Thread API ===

  /**
   * @brief Acquire a texture for consumption/display.
   *
   * @param cb The command buffer for any required GPU synchronization/copy.
   * @return The most recently completed texture, or nullptr if none ready.
   *
   * The texture remains valid until the next call to acquireConsumerTexture().
   *
   * Must be called from the consumer thread.
   */
  QRhiTexture* acquireConsumerTexture(QRhiCommandBuffer* cb);

  /**
   * @brief Check if a new frame is available for consumption.
   */
  bool hasNewFrame() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

/**
 * @brief Backend-specific implementation interface for GPU-only texture sharing.
 */
class TextureShareBackend
{
public:
  virtual ~TextureShareBackend() = default;

  // Single-thread setup (convenience, may violate QRhi threading rules)
  virtual bool setup(
      QRhi* producer, QRhi* consumer, QSize size, QRhiTexture::Format format)
      = 0;

  // Thread-safe split setup
  virtual bool setupProducer(QRhi* producer, QSize size, QRhiTexture::Format format) = 0;
  virtual bool setupConsumer(QRhi* consumer) = 0;

  virtual void resize(QSize newSize) = 0;
  virtual void cleanup() = 0;
  virtual bool isValid() const noexcept = 0;
  virtual bool isProducerReady() const noexcept = 0;

  virtual QRhiTextureRenderTarget* beginProducerFrame() = 0;
  virtual QRhiTexture* currentProducerTexture() = 0;
  virtual void endProducerFrame(QRhiCommandBuffer* cb) = 0;
  virtual QRhiRenderPassDescriptor* producerRenderPassDescriptor() const noexcept = 0;

  virtual QRhiTexture* acquireConsumerTexture(QRhiCommandBuffer* cb) = 0;
  virtual bool hasNewFrame() const noexcept = 0;
  virtual QSize size() const noexcept = 0;
};

/**
 * @brief Factory function to create the appropriate backend implementation.
 */
SCORE_PLUGIN_GFX_EXPORT
std::unique_ptr<TextureShareBackend>
createTextureShareBackend(QRhi* producer, QRhi* consumer);

} // namespace score::gfx
