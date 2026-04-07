#pragma once

#include <halp/controls.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <Gfx/Graph/RenderList.hpp>

#include <QtGui/private/qrhi_p.h>

#include <QImage>

namespace Threedim
{

enum class CubemapLayout
{
  Equirectangular,
  HorizontalCross,
  VerticalCross,
  HorizontalStrip,
  VerticalStrip,
};

class CubemapLoader
{
public:
  halp_meta(name, "Cubemap Loader")
  halp_meta(category, "Visuals/3D")
  halp_meta(c_name, "cubemap_loader")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/cubemap-loader.html")
  halp_meta(uuid, "b8d4f9e2-6c3a-4b0f-9e4d-2a7f0c5b3d6e")

  struct ins
  {
    struct : halp::lineedit<"Image", "">
    {
      void update(CubemapLoader& self) { self.m_imageChanged = true; }
    } image;

    struct : halp::enum_t<CubemapLayout, "Layout">
    {
      void update(CubemapLoader& self) { self.m_imageChanged = true; }
    } layout;

    struct : halp::spinbox_i32<"Resolution", halp::range{16, 8192, 512}>
    {
      void update(CubemapLoader& self) { self.m_imageChanged = true; }
    } resolution;
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Cubemap"> cubemap;
  } outputs;

  // GPU resources
  QRhiTexture* m_cubemapTex{};
  QRhiTexture* m_equirectTex{};
  QRhiSampler* m_equirectSampler{};

  // Per-face render targets for equirectangular conversion
  struct FaceRT
  {
    QRhiTextureRenderTarget* renderTarget{};
    QRhiRenderPassDescriptor* renderPass{};
  };
  FaceRT m_faceRTs[6]{};
  QRhiGraphicsPipeline* m_equirectPipeline{};
  QRhiShaderResourceBindings* m_equirectSrb{};
  QRhiBuffer* m_equirectUbo{};
  QRhiBuffer* m_quadVbuf{};

  int m_faceSize{0};
  bool m_imageChanged{true};
  QImage m_loadedImage;

  void operator()() { }

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge);

private:
  void loadImage();
  void createCubemapTexture(QRhi& rhi, int faceSize);
  void releaseCubemapTexture();
  void releaseEquirectResources();

  void uploadCrossOrStrip(QRhiResourceUpdateBatch* res);
  void renderEquirectangular(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res);

  void setupEquirectPipeline(score::gfx::RenderList& renderer);

  // Extract face from cross/strip layout
  QImage extractFace(int faceIndex) const;
};

}
