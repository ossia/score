#include <Gfx/Graph/TextureLoader.hpp>
#include <Gfx/Hashes.hpp>

#include <ossia/detail/hash.hpp>

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QImageReader>

#include <private/qrhi_p.h>

#include <cstring>

namespace score::gfx
{
// Qt's tga handler only accepts TGA 2.0 files ending in the
// "TRUEVISION-XFILE." footer; original-spec TGAs (still produced by common
// tools) are rejected. When the header plausibly describes a TGA, retry the
// decode with the footer appended.
static QImage tryFooterlessTga(const QByteArray& bytes)
{
  if(bytes.size() < 18)
    return {};
  const auto* d = reinterpret_cast<const unsigned char*>(bytes.constData());
  const bool colormap_ok = d[1] <= 1;
  const bool type_ok
      = d[2] == 1 || d[2] == 2 || d[2] == 3 || d[2] == 9 || d[2] == 10 || d[2] == 11;
  const int bpp = d[16];
  const bool bpp_ok = bpp == 8 || bpp == 15 || bpp == 16 || bpp == 24 || bpp == 32;
  const int w = d[12] | (d[13] << 8);
  const int h = d[14] | (d[15] << 8);
  if(!colormap_ok || !type_ok || !bpp_ok || w <= 0 || h <= 0)
    return {};

  QByteArray footered = bytes;
  footered.append(26, '\0');
  std::memcpy(footered.data() + footered.size() - 18, "TRUEVISION-XFILE.", 18);
  QImage img;
  img.loadFromData(footered, "tga");
  return img;
}

// -----------------------------------------------------------------------------
// CPU decode
// -----------------------------------------------------------------------------

std::optional<DecodedImage> decodeImageFromPath(const QString& path)
{
  // Decode straight off disk. We previously reused Gfx::ImageCache here, but
  // that cache is refcounted and TextureLoader never released its acquisition,
  // so every unique path ever decoded leaked one QImage for the program
  // lifetime (drag-drop reloads, library scans, image_input swaps all bled
  // memory). The TextureCache below already de-duplicates per-renderer GPU
  // uploads, and AssetTable handles cross-output dedup keyed on content hash,
  // so the extra CPU-side cache layer wasn't pulling its weight.
  QImage img(path);
  if(img.isNull())
  {
    QFile f(path);
    if(f.open(QIODevice::ReadOnly))
      img = tryFooterlessTga(f.readAll());
    if(img.isNull())
      return std::nullopt;
  }

  DecodedImage out;
  out.image = std::move(img);
  // Canonical RGBA8888 layout so QRhi's RGBA8 textures sample correctly.
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
  {
    img = tryFooterlessTga(bytes);
    if(img.isNull())
      return std::nullopt;
  }

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
  std::size_t seed = hash_qstring(k.origin);
  ossia::hash_combine(seed, (uint8_t)(k.srgb ? 1 : 0));
  return seed;
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
  if(tex)
    m_textures.emplace(std::move(k), tex);
  // Decode failures are not cached — let the next call retry. Caller
  // handles the nullptr return as the "missing texture" fallback.
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
  if(tex)
    m_textures.emplace(std::move(k), tex);
  return tex;
}

}  // namespace score::gfx
