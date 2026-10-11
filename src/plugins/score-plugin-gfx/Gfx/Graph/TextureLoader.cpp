#include <Gfx/Graph/TextureLoader.hpp>
#include <Gfx/Graph/MipGeneration.hpp>
#include <Gfx/Hashes.hpp>

#include <ossia/detail/hash.hpp>

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QImageReader>

#include <private/qrhi_p.h>

#include <algorithm>
#include <cstring>
#include <vector>

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
  // Decode straight off disk rather than through Gfx::ImageCache: that cache is
  // refcounted and this loader has no release point, so every unique path would
  // hold a QImage for the program lifetime. The TextureCache below already
  // de-duplicates per-renderer GPU uploads and AssetTable handles cross-output
  // dedup keyed on content hash.
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

bool bleedTransparentColor(QImage& image)
{
  if(image.isNull())
    return false;
  if(image.format() != QImage::Format_RGBA8888)
    image.convertTo(QImage::Format_RGBA8888);

  const int w = image.width();
  const int h = image.height();
  const auto texel = [&](const QImage& img, int x, int y) {
    return img.constScanLine(y) + 4 * x;
  };

  bool anyClear = false, anyCovered = false;
  for(int y = 0; y < h && !(anyClear && anyCovered); ++y)
  {
    const uchar* row = image.constScanLine(y);
    for(int x = 0; x < w; ++x)
    {
      if(row[4 * x + 3] == 0)
        anyClear = true;
      else
        anyCovered = true;
    }
  }
  if(!anyClear || !anyCovered)
    return false;

  // Push: alpha-weighted sums per 2x2 block, level 1 up to 1x1. Each entry is
  // (sum a*r, sum a*g, sum a*b, sum a); after the pull below it holds the
  // un-weighted colour in rgb.
  struct Level
  {
    int w, h;
    std::vector<float> v;
  };
  std::vector<Level> levels;
  {
    int lw = w, lh = h;
    while(lw > 1 || lh > 1)
    {
      const int pw = lw, ph = lh;
      lw = std::max(1, (lw + 1) / 2);
      lh = std::max(1, (lh + 1) / 2);
      Level lv{lw, lh, std::vector<float>(std::size_t(lw) * lh * 4, 0.f)};
      for(int y = 0; y < ph; ++y)
      {
        for(int x = 0; x < pw; ++x)
        {
          float* dst = &lv.v[(std::size_t(y / 2) * lw + x / 2) * 4];
          if(levels.empty())
          {
            const uchar* p = texel(image, x, y);
            const float a = p[3];
            dst[0] += a * p[0];
            dst[1] += a * p[1];
            dst[2] += a * p[2];
            dst[3] += a;
          }
          else
          {
            const auto& prev = levels.back();
            const float* src = &prev.v[(std::size_t(y) * pw + x) * 4];
            for(int c = 0; c < 4; ++c)
              dst[c] += src[c];
          }
        }
      }
      levels.push_back(std::move(lv));
    }
  }

  // Pull: from the 1x1 top down, a covered block keeps its weighted mean and
  // an empty one inherits its parent's colour.
  for(int k = (int)levels.size() - 1; k >= 0; --k)
  {
    auto& lv = levels[k];
    const Level* parent = k + 1 < (int)levels.size() ? &levels[k + 1] : nullptr;
    for(int y = 0; y < lv.h; ++y)
    {
      for(int x = 0; x < lv.w; ++x)
      {
        float* t = &lv.v[(std::size_t(y) * lv.w + x) * 4];
        if(t[3] > 0.f)
        {
          t[0] /= t[3];
          t[1] /= t[3];
          t[2] /= t[3];
        }
        else if(parent)
        {
          const float* p = &parent->v[(std::size_t(y / 2) * parent->w + x / 2) * 4];
          t[0] = p[0];
          t[1] = p[1];
          t[2] = p[2];
        }
      }
    }
  }

  // Level 0. A clear texel touching covered ones takes their alpha-weighted
  // mean (read from the source, so the fill does not chain); any other takes
  // its level-1 block colour.
  const QImage src = image.copy();
  const auto& l1 = levels.front();
  for(int y = 0; y < h; ++y)
  {
    uchar* row = image.scanLine(y);
    for(int x = 0; x < w; ++x)
    {
      uchar* p = row + 4 * x;
      if(p[3] != 0)
        continue;
      float acc[4]{};
      for(int dy = -1; dy <= 1; ++dy)
      {
        const int yy = y + dy;
        if(yy < 0 || yy >= h)
          continue;
        for(int dx = -1; dx <= 1; ++dx)
        {
          const int xx = x + dx;
          if(xx < 0 || xx >= w)
            continue;
          const uchar* q = texel(src, xx, yy);
          const float a = q[3];
          acc[0] += a * q[0];
          acc[1] += a * q[1];
          acc[2] += a * q[2];
          acc[3] += a;
        }
      }
      const float* fill = acc;
      float mean[3];
      if(acc[3] > 0.f)
      {
        for(int c = 0; c < 3; ++c)
          mean[c] = acc[c] / acc[3];
        fill = mean;
      }
      else
      {
        fill = &l1.v[(std::size_t(y / 2) * l1.w + x / 2) * 4];
      }
      for(int c = 0; c < 3; ++c)
        p[c] = (uchar)std::clamp(int(fill[c] + 0.5f), 0, 255);
    }
  }
  return true;
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
  score::gfx::generateMipsIfAny(batch, tex);
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
