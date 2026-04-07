#pragma once

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <Gfx/Graph/RenderList.hpp>

#include <QtGui/private/qrhi_p.h>

namespace Threedim
{

class CubemapComposer
{
public:
  halp_meta(name, "Cubemap Composer")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "cubemap_composer")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/cubemap-composer.html")
  halp_meta(uuid, "a7c3e8f1-5b2d-4a9e-8f3c-1d6e9b4a2c5f")

  struct ins
  {
    halp::texture_input<"+X", halp::custom_variable_texture> pos_x;
    halp::texture_input<"-X", halp::custom_variable_texture> neg_x;
    halp::texture_input<"+Y", halp::custom_variable_texture> pos_y;
    halp::texture_input<"-Y", halp::custom_variable_texture> neg_y;
    halp::texture_input<"+Z", halp::custom_variable_texture> pos_z;
    halp::texture_input<"-Z", halp::custom_variable_texture> neg_z;
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Cubemap"> cubemap;
  } outputs;

  QRhiTexture* m_cubemapTex{};
  int m_faceSize{0};
  bool m_dirty{true};

  void operator()() { }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res)
  {
    m_dirty = true;
  }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e)
  {
    // Determine face size from the largest input
    int maxSize = 0;
    auto checkFace = [&](const auto& tex) {
      if(tex.texture.bytes && tex.texture.width > 0 && tex.texture.height > 0)
      {
        int s = std::max(tex.texture.width, tex.texture.height);
        maxSize = std::max(maxSize, s);
      }
    };
    checkFace(inputs.pos_x);
    checkFace(inputs.neg_x);
    checkFace(inputs.pos_y);
    checkFace(inputs.neg_y);
    checkFace(inputs.pos_z);
    checkFace(inputs.neg_z);

    if(maxSize <= 0)
      return;

    // Recreate texture if size changed
    if(maxSize != m_faceSize)
    {
      if(m_cubemapTex)
      {
        m_cubemapTex->deleteLater();
        m_cubemapTex = nullptr;
      }
      m_faceSize = maxSize;
      m_dirty = true;
    }

    if(!m_cubemapTex && m_faceSize > 0)
    {
      auto& rhi = *renderer.state.rhi;
      m_cubemapTex = rhi.newTexture(
          QRhiTexture::RGBA8, QSize{m_faceSize, m_faceSize}, 1,
          QRhiTexture::CubeMap | QRhiTexture::MipMapped
              | QRhiTexture::UsedWithGenerateMips);
      m_cubemapTex->create();
      outputs.cubemap.texture.handle = m_cubemapTex;
      m_dirty = true;
    }
  }

  void release(score::gfx::RenderList& r)
  {
    if(m_cubemapTex)
    {
      m_cubemapTex->deleteLater();
      m_cubemapTex = nullptr;
    }
    m_faceSize = 0;
    outputs.cubemap.texture.handle = nullptr;
  }

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge)
  {
    if(!m_cubemapTex)
      return;

    if(!m_dirty)
      return;

    bool anyUploaded = false;

    // Upload each face that has valid data
    auto uploadFace = [&](const auto& tex, int layer) {
      if(!tex.texture.bytes || tex.texture.width <= 0 || tex.texture.height <= 0)
        return;

      // Convert to QImage, scale to face size if needed
      QImage img(
          tex.texture.bytes, tex.texture.width, tex.texture.height,
          QImage::Format_RGBA8888);

      if(img.width() != m_faceSize || img.height() != m_faceSize)
        img = img.scaled(
            m_faceSize, m_faceSize, Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation);

      img = img.convertToFormat(QImage::Format_RGBA8888);

      QRhiTextureSubresourceUploadDescription subresDesc(img);
      QRhiTextureUploadEntry entry(layer, 0, subresDesc);
      QRhiTextureUploadDescription desc({entry});
      res->uploadTexture(m_cubemapTex, desc);
      anyUploaded = true;
    };

    uploadFace(inputs.pos_x, 0); // +X
    uploadFace(inputs.neg_x, 1); // -X
    uploadFace(inputs.pos_y, 2); // +Y
    uploadFace(inputs.neg_y, 3); // -Y
    uploadFace(inputs.pos_z, 4); // +Z
    uploadFace(inputs.neg_z, 5); // -Z

    if(anyUploaded)
    {
      res->generateMips(m_cubemapTex);
    }

    m_dirty = false;
  }
};

}
