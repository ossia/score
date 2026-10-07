#pragma once
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/VertexFallbackPlan.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QDebug>
#include <QtGui/private/qrhi_p.h>

#include <optional>
#include <span>

namespace score::gfx
{

// Opt-in (SCORE_BUFTRACE=1) trace of CustomMesh::reload, the point where a
// geometry change reaches the GPU vertex / index buffers.
SCORE_PLUGIN_GFX_EXPORT bool buftrace_enabled();
#define BUFTRACE()                        \
  if(!::score::gfx::buftrace_enabled()) \
  {                                       \
  }                                       \
  else                                    \
    qDebug().nospace() << "[BUFTRACE] "

//! The commands [first, first + count) of an indirect draw's command list,
//! clamped to it.
struct DrawCommandRange
{
  uint32_t first{0};
  uint32_t count{0xFFFFFFFFu};
};

class CustomMesh : public score::gfx::Mesh
{
  ossia::mesh_list geom;

  using pip = QRhiGraphicsPipeline;
  pip::Topology topology = pip::Topology::TriangleStrip;
  pip::CullMode cullMode = pip::CullMode::None;
  pip::FrontFace frontFace = pip::FrontFace::CW;

  ossia::small_vector<QRhiVertexInputBinding, 2> vertexBindings;
  ossia::small_vector<QRhiVertexInputAttribute, 2> vertexAttributes;
  ossia::small_vector<ossia::attribute_semantic, 2> attributeSemantics;

  ossia::small_vector<QRhiBuffer*, 2> buffers;
  QRhiBuffer* index{};

public:
  explicit CustomMesh(
      const ossia::mesh_list& g, const ossia::geometry_filter_list_ptr& f);

  const ossia::mesh_list& meshList() const noexcept { return geom; }

  [[nodiscard]]
  QRhiBuffer* init_vbo(const ossia::geometry::cpu_buffer& buf, QRhi& rhi) const noexcept;

  [[nodiscard]]
  QRhiBuffer* init_vbo(const ossia::geometry::gpu_buffer& buf, QRhi& rhi) const noexcept;
  [[nodiscard]]
  QRhiBuffer*
  init_index(const ossia::geometry::cpu_buffer& buf, QRhi& rhi) const noexcept;

  [[nodiscard]]
  QRhiBuffer*
  init_index(const ossia::geometry::gpu_buffer& buf, QRhi& rhi) const noexcept;

  [[nodiscard]] MeshBuffers init(QRhi& rhi) const noexcept override;

  void update_vbo(
      int buffer_index, const ossia::geometry::cpu_buffer& vtx_buf, MeshBuffers& meshbuf,
      QRhi& rhi, QRhiResourceUpdateBatch& rb) const noexcept;

  void update_vbo(
      int buffer_index, const ossia::geometry::gpu_buffer& vtx_buf, MeshBuffers& meshbuf,
      QRhi& rhi, QRhiResourceUpdateBatch& rb) const noexcept;

  void update_index(
      int buffer_index, const ossia::geometry::cpu_buffer& idx_buf, MeshBuffers& meshbuf,
      QRhi& rhi, QRhiResourceUpdateBatch& rb) const noexcept;

  void update_index(
      int buffer_index, const ossia::geometry::gpu_buffer& idx_buf, MeshBuffers& meshbuf,
      QRhi& rhi, QRhiResourceUpdateBatch& rb) const noexcept;
  void update(QRhi& rhi, MeshBuffers& output_meshbuf, QRhiResourceUpdateBatch& rb)
      const noexcept override;
  Flags flags() const noexcept override;

  void clear();

  void preparePipeline(QRhiGraphicsPipeline& pip) const noexcept override;
  [[nodiscard]] bool hasGeometry() const noexcept override;

  void reload(const ossia::mesh_list& ml, const ossia::geometry_filter_list_ptr& f);

  void draw(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;

  // Fallback-aware variant: appends each `FallbackBindingPlan::Slot`
  // buffer to the vertex-input array before issuing the draw. Used by
  // raw-raster pipelines whose shaders declared "REQUIRED: false"
  // VERTEX_INPUTS the upstream geometry doesn't provide. Non-virtual on
  // purpose — only CustomMesh participates in the fallback path.
  void drawWithFallbackBindings(
      const MeshBuffers& bufs, QRhiCommandBuffer& cb,
      const FallbackBindingPlan& plan) const noexcept;

  // Draw a single sub-mesh (geom.meshes[mesh_index]) using the portion of
  // `bufs.buffers` starting at `buffer_offset`. `buffer_offset` must match
  // init()'s flat-concat layout: sum of geom.meshes[0..mesh_index-1].buffers.size().
  // Returns true if a draw call was issued.
  //
  // Exposed so consumers that need per-sub-mesh state (e.g. RawRaster
  // swapping the per_draw SSBO between meshes) can iterate sub-meshes
  // themselves instead of invoking the fire-and-forget `draw()` above.
  //
  // `plan` (default empty) says which of the mesh's own bindings the
  // pipeline kept and in what order, and carries the fallback buffers to
  // merge in at their binding_index — those land past the surviving mesh
  // bindings, so their indices are always contiguous after them. An
  // un-compacted plan means "bind every geometry input in order", which
  // is what the pipeline builders that do not emit a plan expect.
  //
  // `range` restricts an indirect draw to some of its commands.
  using CommandRange = DrawCommandRange;
  bool drawSingleMesh(
      std::size_t mesh_index, std::size_t buffer_offset,
      const MeshBuffers& bufs, QRhiCommandBuffer& cb,
      const FallbackBindingPlan& plan = {},
      CommandRange range = {}) const noexcept;

  //! The commands of the alpha-blended draws in a single-mesh indirect draw,
  //! from the ScenePreprocessor's _blend_draw_cmds auxiliary; empty without.
  std::optional<CommandRange> blendCommandRange() const noexcept;

  //! True when sub-mesh i shares meshes[0]'s vertex layout — the only
  //! layout the pipeline was built for; mismatching sub-meshes are
  //! skipped by the default draw paths.
  bool subMeshLayoutMatchesFirst(std::size_t i) const noexcept;

  const char* defaultVertexShader() const noexcept override;

  const ossia::geometry* semanticGeometry() const noexcept override
  {
    if(!geom.meshes.empty())
      return &geom.meshes[0];
    return nullptr;
  }
};

}
