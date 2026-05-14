#pragma once
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/meta.hpp>
#include <halp/texture.hpp>

#include <QImage>

namespace Threedim
{

// Lightweight LDR image-to-GPU-texture loader. Sibling to BufferLoader
// but for 2D textures. Sits alongside the main OpenImageIO-backed
// ImageLoader in a sibling plug-in, usable when OIIO isn't linked in
// and the image is a plain QImage-supported format (PNG / JPG / BMP /
// …). HDR formats (.hdr / .exr) require the OIIO path.
//
// Primary use: feeds the pure-shader cubemap pipeline
//   ImageLoader(path) → cubemap_from_source → SceneResourceRoute(Skybox)
// superseding the bespoke equirect/cross/strip code in CubemapLoader.
class ImageLoader
{
public:
  halp_meta(name, "Image loader (LDR)")
  halp_meta(category, "Visuals")
  halp_meta(c_name, "image_loader_ldr")
  halp_meta(authors, "ossia team")
  halp_meta(
      manual_url,
      "https://ossia.io/score-docs/processes/image-loader.html")
  halp_meta(description,
      "Loads a 2D image file (PNG / JPG / BMP / …) to a GPU RGBA8 texture")
  halp_meta(uuid, "e6b2c1d8-3f45-4a92-8b17-9c4e0d5a6f3b")

  struct ins
  {
    // File-port boilerplate — same pattern as SplatLoader's obj_t.
    // process() runs on the file-load thread, decodes the image,
    // returns a lambda that stages the result onto the node from the
    // execution thread.
    struct image_t : halp::file_port<"Image", halp::mmap_file_view>
    {
      halp_meta(extensions,
          "Images (*.png *.jpg *.jpeg *.bmp *.tga *.webp *.tif *.tiff)");
      static std::function<void(ImageLoader&)> process(file_type data)
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
        if(!img.isNull() && img.format() != QImage::Format_RGBA8888)
          img = img.convertToFormat(QImage::Format_RGBA8888);
        return [img = std::move(img)](ImageLoader& self) mutable {
          self.m_pendingImage = std::move(img);
          self.m_changed = true;
        };
      }
    } image;
  } inputs;

  struct
  {
    halp::gpu_texture_output<"Texture"> texture;
  } outputs;

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res);
  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);
  void release(score::gfx::RenderList& r);
  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge);

  void operator()() { }

  QImage m_pendingImage;
  // Persistent CPU copy of the last successfully uploaded image. Kept
  // alive across RenderList rebuilds (resize) so that init() can
  // re-upload to the freshly allocated QRhiTexture without needing the
  // user to re-trigger the file-port. Without this, release() drops
  // m_tex AND clears m_pendingImage in update() — the next init() has
  // nothing to upload, the texture port stays bound to the empty
  // placeholder for the rest of the session.
  QImage m_keptImage;
  QRhiTexture* m_tex{};
  bool m_changed{};
};

}
