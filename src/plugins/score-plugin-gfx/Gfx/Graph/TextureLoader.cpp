#include <Gfx/Graph/TextureLoader.hpp>

#include <Gfx/Images/Process.hpp>

#include <QByteArray>
#include <QImage>
#include <QImageReader>

#include <private/qrhi_p.h>

namespace score::gfx
{

// -----------------------------------------------------------------------------
// CPU decode
// -----------------------------------------------------------------------------

std::optional<DecodedImage> decodeImageFromPath(const QString& path)
{
  // Reuse the existing global CPU cache (Gfx/Images/Process.hpp). It's
  // refcounted; we deliberately never call release() — material textures
  // typically stay live for the program lifetime.
  auto cached = Gfx::ImageCache::instance().acquire(path);
  if(!cached || cached->frames.empty())
    return std::nullopt;

  DecodedImage out;
  out.image = cached->frames.front();
  // Cache stores Format_ARGB32 (BGRA-swizzled by Qt). Convert to a
  // canonical RGBA8888 layout so QRhi's RGBA8 textures sample correctly.
  if(out.image.format() != QImage::Format_RGBA8888)
    out.image.convertTo(QImage::Format_RGBA8888);
  out.debug_name = path;
  return out;
}

std::optional<DecodedImage> decodeImageFromMemory(
    const QByteArray& bytes, const QString& mime_hint)
{
  QImage img;
  // QImage::loadFromData accepts a format hint as a const char* (e.g. "PNG").
  // Strip the "image/" prefix from the MIME type if present, then upper-case.
  QByteArray fmt;
  if(!mime_hint.isEmpty())
  {
    QString s = mime_hint;
    if(s.startsWith("image/"))
      s = s.mid(6);
    fmt = s.toUpper().toLatin1();
  }
  if(!img.loadFromData(bytes, fmt.isEmpty() ? nullptr : fmt.constData()))
    return std::nullopt;

  DecodedImage out;
  out.image = std::move(img);
  if(out.image.format() != QImage::Format_RGBA8888)
    out.image.convertTo(QImage::Format_RGBA8888);
  out.debug_name = QStringLiteral("blob:") + mime_hint;
  return out;
}

// -----------------------------------------------------------------------------
// GPU upload
// -----------------------------------------------------------------------------

QRhiTexture* uploadImageToTexture(
    QRhi& rhi, QRhiResourceUpdateBatch& batch, const QImage& img, bool srgb,
    const QString& debug_name)
{
  if(img.isNull())
    return nullptr;

  // sRGB is a Flag bit (not a separate format) — Qt RHI allocates an RGBA8
  // texture with sRGB sampling semantics when the flag is present.
  // MipMapped + UsedWithGenerateMips: required for the generateMips() call
  // below. Without a mip chain, sampling a high-resolution material texture
  // (Sponza floor at distance, etc.) point-samples the base level at sub-
  // pixel rate → uniform noise / TV-static aliasing.
  QRhiTexture::Flags flags
      = QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips;
  if(srgb)
    flags |= QRhiTexture::sRGB;
  // sampleCount=1 (no MSAA on a sampled material texture). The mip count
  // itself is implicit — set by the MipMapped flag and floor(log2(max(w,h)))+1.
  auto* tex = rhi.newTexture(QRhiTexture::RGBA8, img.size(), 1, flags);
  if(!tex)
    return nullptr;
  if(!debug_name.isEmpty())
    tex->setName(debug_name.toUtf8());
  if(!tex->create())
  {
    delete tex;
    return nullptr;
  }
  // QRhi accepts QImage directly; format conversion is handled internally.
  batch.uploadTexture(tex, img);
  // Filter the base level into the mip chain. Cheap (one-shot, on first
  // upload) and unblocks min-filter-linear-mipmap-linear sampling on the
  // material samplers — kills the floor-noise aliasing.
  batch.generateMips(tex);
  return tex;
}

// -----------------------------------------------------------------------------
// One-shot helpers
// -----------------------------------------------------------------------------

QRhiTexture* loadAndUploadTexture(
    QRhi& rhi, QRhiResourceUpdateBatch& batch, const QString& path, bool srgb)
{
  auto decoded = decodeImageFromPath(path);
  if(!decoded)
    return nullptr;
  return uploadImageToTexture(
      rhi, batch, decoded->image, srgb, decoded->debug_name);
}

QRhiTexture* loadAndUploadTexture(
    QRhi& rhi, QRhiResourceUpdateBatch& batch, const QByteArray& bytes,
    const QString& mime_hint, bool srgb)
{
  auto decoded = decodeImageFromMemory(bytes, mime_hint);
  if(!decoded)
    return nullptr;
  return uploadImageToTexture(
      rhi, batch, decoded->image, srgb, decoded->debug_name);
}

// -----------------------------------------------------------------------------
// TextureCache
// -----------------------------------------------------------------------------

std::size_t TextureCache::KeyHash::operator()(const Key& k) const noexcept
{
  std::size_t h = qHash(k.origin);
  // Mix the sRGB bit. Use a constant of decent dispersion.
  h ^= (k.srgb ? 0x9E3779B97F4A7C15ull : 0xBF58476D1CE4E5B9ull);
  return h;
}

TextureCache::~TextureCache()
{
  clear();
}

void TextureCache::clear()
{
  for(auto& [key, tex] : m_textures)
    if(tex)
      tex->deleteLater();
  m_textures.clear();
}

QRhiTexture* TextureCache::acquireFromPath(
    QRhi& rhi, QRhiResourceUpdateBatch& batch, const QString& path, bool srgb)
{
  if(path.isEmpty())
    return nullptr;
  Key k{path, srgb};
  if(auto it = m_textures.find(k); it != m_textures.end())
    return it->second;

  auto* tex = loadAndUploadTexture(rhi, batch, path, srgb);
  // Insert even if nullptr — avoids retrying decode every frame for a missing
  // file. Caller can detect failure via the nullptr return.
  m_textures.emplace(std::move(k), tex);
  return tex;
}

QRhiTexture* TextureCache::acquireFromMemory(
    QRhi& rhi, QRhiResourceUpdateBatch& batch, const QByteArray& bytes,
    const QString& mime_hint, uint64_t content_hash, bool srgb)
{
  Key k{
      QStringLiteral("blob:") + QString::number(content_hash, 16),
      srgb};
  if(auto it = m_textures.find(k); it != m_textures.end())
    return it->second;

  auto* tex = loadAndUploadTexture(rhi, batch, bytes, mime_hint, srgb);
  m_textures.emplace(std::move(k), tex);
  return tex;
}

}  // namespace score::gfx
