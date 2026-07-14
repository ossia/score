#include "ImageLoader.hpp"

namespace Threedim
{

void ImageLoader::init(
    score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  // RenderList rebuild (e.g. viewport resize) calls release() which
  // drops m_tex, then init() against the new RenderList. Without this
  // re-stage the user would have to re-trigger the file-port to get
  // their texture back. Stage the kept CPU image into m_pendingImage
  // so the next update() pass uploads it to the freshly-allocated
  // QRhiTexture against the new rhi.
  if(!m_keptImage.isNull())
  {
    m_pendingImage = m_keptImage;
    m_changed = true;
  }
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

  // Persist the CPU copy across RenderList rebuilds so init() can
  // re-stage on the next resize. Move-from m_pendingImage to keep
  // the upload's already-detached QImage data without copying.
  m_keptImage = std::move(m_pendingImage);
  m_pendingImage = QImage{};
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
