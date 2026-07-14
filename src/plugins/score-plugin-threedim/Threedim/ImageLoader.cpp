#include "ImageLoader.hpp"

namespace Threedim
{

void ImageLoader::init(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
{
}

void ImageLoader::update(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
    score::gfx::Edge* e)
{
  if(!m_changed || m_pendingImage.isNull())
    return;

  auto& rhi = *renderer.state.rhi;
  const QSize sz = m_pendingImage.size();

  // (Re)create texture when the stored one's size doesn't match.
  // RGBA8 — LDR loader. The HDR variant lives in a sibling plug-in
  // that links against OpenImageIO and produces RGBA16F/RGBA32F.
  if(!m_tex || m_tex->pixelSize() != sz)
  {
    if(m_tex)
      m_tex->deleteLater();
    m_tex = rhi.newTexture(QRhiTexture::RGBA8, sz, 1, QRhiTexture::Flag{});
    if(!m_tex || !m_tex->create())
    {
      if(m_tex)
      {
        m_tex->deleteLater();
        m_tex = nullptr;
      }
      return;
    }
  }

  res.uploadTexture(m_tex, m_pendingImage);

  outputs.texture.texture.handle = m_tex;
  outputs.texture.texture.width = sz.width();
  outputs.texture.texture.height = sz.height();
  // Format defaults to RGBA8 on construction; explicit for clarity.
  outputs.texture.texture.format = halp::gpu_texture::RGBA8;

  m_pendingImage = QImage{};   // drop CPU copy once uploaded
  m_changed = false;
}

void ImageLoader::release(score::gfx::RenderList& r)
{
  if(m_tex)
  {
    m_tex->deleteLater();
    m_tex = nullptr;
  }
  outputs.texture.texture.handle = nullptr;
  outputs.texture.texture.width = 0;
  outputs.texture.texture.height = 0;
}

void ImageLoader::runInitialPasses(
    score::gfx::RenderList&, QRhiCommandBuffer&,
    QRhiResourceUpdateBatch*&, score::gfx::Edge&)
{
}

} // namespace Threedim
