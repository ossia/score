#pragma once
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/detail/mutex.hpp>
#include <ossia/detail/pod_vector.hpp>

#include <Threedim/Ply.hpp>
#include <Threedim/TinyObj.hpp>
#include <halp/buffer.hpp>
#include <halp/controls.hpp>
#include <halp/file_port.hpp>
#include <halp/geometry.hpp>
#include <halp/meta.hpp>

namespace Threedim
{

class SplatLoader
{
public:
  halp_meta(name, "Splat loader")
  halp_meta(category, "Visuals/Meshes")
  halp_meta(c_name, "buffer_loader")
  halp_meta(authors, "Jean-Michaël Celerier, miniPLY authors")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/meshes.html#splat-loader")
  halp_meta(description, "Loads gaussian splats from a PLY model")
  halp_meta(uuid, "bab30770-d6d7-4727-ad43-38eacdd910a7")

  struct ins
  {
    struct obj_t : halp::file_port<"3DGS PLY file", halp::mmap_file_view>
    {
      halp_meta(extensions, "3DGS files (*.ply)");
      static std::function<void(SplatLoader&)> process(file_type data)
      {
        auto loaded = Threedim::GaussianSplatsFromPly(data.filename);
        return [l = std::move(loaded)](SplatLoader& self) mutable {
          std::swap(self.m_splat_data.buffer, l.buffer);
          self.m_splat_data.splatCount = l.splatCount;
          self.m_splat_data.shRestCount = l.shRestCount;
          self.m_changed = true;
        };
      }
    } obj;
  } inputs;

  struct
  {
    halp::gpu_buffer_output<"Output"> buffer;
  } outputs;

  void init(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res);

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::Edge* e);

  void release(score::gfx::RenderList& r);

  void runInitialPasses(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& commands,
      QRhiResourceUpdateBatch*& res, score::gfx::Edge& edge);

  void operator()();
  QRhiBuffer* m_last_buffer{};
  GaussianSplatData m_splat_data;
  bool m_changed{};
};

}
