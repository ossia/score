#pragma once

#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <Gfx/Graph/RenderList.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QtGui/private/qrhi_p.h>

#include <QDebug>
#include <QImage>

#include <cstdint>
#include <functional>
#include <memory>

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
    // File-port boilerplate — same pattern as ImageLoader. process()
    // runs on the file-load worker thread, decodes the image off the
    // render thread, returns a lambda that stages the decoded QImage
    // onto the node from the execution thread. See diagnostic 041 —
    // the previous lineedit<…> path called QImage(qpath) from update()
    // on the render thread, blocking command recording for many frames
    // on a large cube cross / equirect HDR.
    struct image_t : halp::file_port<"Image", halp::mmap_file_view>
    {
      // Only LDR formats are advertised: decode goes through QImage, which
      // has no stock Radiance-HDR / OpenEXR handler, and both the equirect
      // source and cube textures are RGBA8. .hdr/.exr were previously
      // listed but would silently decode to null (no cube, no error) or be
      // truncated to 8-bit — no float decoder is vendored in this tree, so
      // they are dropped rather than silently mishandled.
      halp_meta(extensions,
          "Images (*.png *.jpg *.jpeg *.bmp *.tga *.webp *.tif *.tiff)");
      static std::function<void(CubemapLoader&)> process(file_type data)
      {
        QImage img;
        if(!data.bytes.empty())
        {
          img.loadFromData(
              reinterpret_cast<const uchar*>(data.bytes.data()),
              (int)data.bytes.size());
        }
        if(img.isNull() && !data.filename.empty())
        {
          img = QImage(data.filename.data());
        }
        if(img.isNull())
        {
          // Never fail silently — surface unsupported/corrupt inputs
          // (e.g. an HDR/EXR file QImage cannot decode).
          qWarning() << "CubemapLoader: failed to decode environment image"
                     << data.filename.data()
                     << "- unsupported or corrupt format (HDR/EXR float"
                        " formats are not supported)";
        }
        else if(img.format() != QImage::Format_RGBA8888)
          img = img.convertToFormat(QImage::Format_RGBA8888);
        return [img = std::move(img)](CubemapLoader& self) mutable {
          self.m_loadedImage = std::move(img);
          self.m_imageChanged = true;
        };
      }
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
    // Raw cube texture — kept for consumers that want the handle
    // directly (e.g. a bare-skybox rendering shader). Tagged via the
    // new halp::gpu_cubemap_output so sinks know to grab-from-source
    // rather than allocate a 2D render target.
    halp::gpu_cubemap_output<"Cubemap"> cubemap;

    // Scene-graph output: a scene_spec whose scene_environment has only
    // skybox_texture.native_handle populated (no ambient / fog / etc.,
    // no roots). Lets users wire the cubemap into a scene without a
    // side-channel cable — merge_scenes's per-field env overlay folds
    // it together with an EnvironmentLoader's params independent of
    // wiring order.
    struct
    {
      halp_meta(name, "Scene");
      ossia::scene_spec scene;
      uint8_t dirty{0};
    } scene_out;
  } outputs;

  // Stable scene_state identity so downstream scene-identity caches
  // (ScenePreprocessor, merge_scenes passthrough) stay hot across frames.
  std::shared_ptr<ossia::scene_state> m_sceneState;
  int64_t m_sceneVersion{0};
  void* m_lastPublishedHandle{};

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

  // Per-face stride into m_equirectUbo. The 6 equirect->cube face passes
  // each need their own FaceInfo slot bound via a dynamic offset, so the
  // UBO holds 6 blocks spaced by aligned(UniformBufferOffsetAlignment).
  quint32 m_equirectUboStride{0};

  int m_faceSize{0};
  bool m_imageChanged{true};
  QImage m_loadedImage;

  void operator()() { }

  // Dtor safety net: if the renderer framework's release(RenderList&)
  // path was skipped (e.g. a reconcile path that deletes the renderer
  // without first calling release — or any future code that drops the
  // GfxRenderer's shared_ptr<CubemapLoader> without going through
  // CpuFilterNode::releaseState), any still-live textures and GPU
  // resources go to deleteLater here so QRhi's destructor can collect
  // them before vkDestroyDevice. Without this the Vulkan validation
  // layer flags "VkImage has not been destroyed" on app exit.
  ~CubemapLoader();

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge);

private:
  bool createCubemapTexture(QRhi& rhi, int faceSize);
  void releaseCubemapTexture();
  // `renderer` is optional: when non-null QRhiBuffers go through
  // RenderList::releaseBuffer (the project-wide lifetime invariant);
  // when null (dtor fallback, after the RenderList itself may have
  // already been destroyed) we fall back to direct deleteLater.
  // Textures always deleteLater directly — they're not tracked in
  // RenderList::m_vertexBuffers, so the double-free risk only applies
  // to buffers.
  void releaseEquirectResources(score::gfx::RenderList* renderer = nullptr);

  void uploadCrossOrStrip(QRhiResourceUpdateBatch* res);
  void renderEquirectangular(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res);

  bool setupEquirectPipeline(score::gfx::RenderList& renderer);

  // Extract face from cross/strip layout
  QImage extractFace(int faceIndex) const;
};

}
