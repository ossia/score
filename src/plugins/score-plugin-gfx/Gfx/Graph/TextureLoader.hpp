#pragma once
#include <score_plugin_gfx_export.h>

#include <QImage>
#include <QString>

#include <cstdint>
#include <optional>
#include <unordered_map>

class QRhi;
class QRhiTexture;
class QRhiResourceUpdateBatch;
class QByteArray;

namespace score::gfx
{

// =============================================================================
// CPU-side decode result.
//
// The default implementation produces RGBA8888 data via QImageReader. The
// `srgb` flag is metadata only — it does NOT alter the pixel bytes, it just
// records whether the caller intends those bytes to be interpreted as sRGB
// when the texture is sampled (set the QRhiTexture format to RGBA8 vs sRGB8A8
// at upload time accordingly).
//
// Future swap-in candidates: OIIO (HDR/EXR), KTX2 (transcoded BCn), AVIF.
// =============================================================================
struct DecodedImage
{
  QImage image;        // QImage::Format_RGBA8888 (no premul)
  QString debug_name;  // For QRhiTexture::setName()
};

// =============================================================================
// Decode helpers — synchronous, called on the render thread.
//
// Path-based decode goes through Gfx::ImageCache (Gfx/Images/Process.hpp) for
// cross-process CPU sharing. Memory-based decode bypasses the cache (the
// caller already owns the bytes).
// =============================================================================

SCORE_PLUGIN_GFX_EXPORT
std::optional<DecodedImage> decodeImageFromPath(const QString& path);

SCORE_PLUGIN_GFX_EXPORT
std::optional<DecodedImage> decodeImageFromMemory(
    const QByteArray& bytes, const QString& mime_hint);

// =============================================================================
// GPU upload — pure RHI, no I/O. Allocates a freshly-sized QRhiTexture
// (RGBA8 or sRGB8_ALPHA8 depending on `srgb`), records the upload into
// `batch`. Caller owns the returned pointer (delete via deleteLater()).
//
// Returns nullptr on QRhi allocation failure.
// =============================================================================

SCORE_PLUGIN_GFX_EXPORT
QRhiTexture* uploadImageToTexture(
    QRhi& rhi, QRhiResourceUpdateBatch& batch, const QImage& img, bool srgb,
    const QString& debug_name = {});

// =============================================================================
// One-shot decode + upload helpers. Convenience for callers that don't need
// to reuse the decoded CPU bytes.
// =============================================================================

SCORE_PLUGIN_GFX_EXPORT
QRhiTexture* loadAndUploadTexture(
    QRhi& rhi, QRhiResourceUpdateBatch& batch, const QString& path, bool srgb);

SCORE_PLUGIN_GFX_EXPORT
QRhiTexture* loadAndUploadTexture(
    QRhi& rhi, QRhiResourceUpdateBatch& batch, const QByteArray& bytes,
    const QString& mime_hint, bool srgb);

// =============================================================================
// Per-renderer GPU texture cache.
//
// QRhiTexture* is bound to one QRhi instance, so this cache MUST live on the
// render-side node (e.g. ScenePreprocessorNode), not as a global singleton. Owns
// the textures it returns; clear() (also runs in the dtor) schedules each via
// deleteLater().
//
// Keys: a file path OR a stable content hash (for embedded glTF/FBX blobs).
// Two entries with the same origin but different sRGB flags coexist.
// =============================================================================
class SCORE_PLUGIN_GFX_EXPORT TextureCache
{
public:
  TextureCache() = default;
  ~TextureCache();

  TextureCache(const TextureCache&) = delete;
  TextureCache& operator=(const TextureCache&) = delete;
  TextureCache(TextureCache&&) noexcept = default;
  TextureCache& operator=(TextureCache&&) noexcept = default;

  // First call decodes + uploads via `batch`; later calls hit the cache.
  // Returns nullptr if the file can't be decoded.
  QRhiTexture* acquireFromPath(
      QRhi& rhi, QRhiResourceUpdateBatch& batch, const QString& path, bool srgb);

  // Same, for embedded blobs. `content_hash` is supplied by the caller — its
  // identity (not its value) is what guards re-upload.
  QRhiTexture* acquireFromMemory(
      QRhi& rhi, QRhiResourceUpdateBatch& batch, const QByteArray& bytes,
      const QString& mime_hint, uint64_t content_hash, bool srgb);

  // Schedule deleteLater() on every owned texture and drop the map.
  void clear();

  std::size_t size() const noexcept { return m_textures.size(); }

private:
  struct Key
  {
    QString origin;  // file path, or "blob:<hash-hex>" for memory blobs
    bool srgb{};
    bool operator==(const Key&) const noexcept = default;
  };
  struct KeyHash
  {
    std::size_t operator()(const Key& k) const noexcept;
  };

  std::unordered_map<Key, QRhiTexture*, KeyHash> m_textures;
};

}  // namespace score::gfx
