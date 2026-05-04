#pragma once
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/VertexFallbackPlan.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QDebug>
#include <QtGui/private/qrhi_p.h>

#include <span>

namespace score::gfx
{

// [BUFTRACE] — diagnostic logging around QRhiBuffer lifetime during
// live graph edits (defined in CustomMesh.cpp). Exposed so other TUs
// (RenderList, ScenePreprocessorNode, RenderedRawRasterPipelineNode) can
// use BUFTRACE() with the same env-var gating.
SCORE_PLUGIN_GFX_EXPORT bool buftrace_enabled();
#define BUFTRACE() if(::score::gfx::buftrace_enabled()) qDebug().nospace() << "[BUFTRACE] "


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
      QRhiResourceUpdateBatch& rb) const noexcept;

  void update_vbo(
      int buffer_index, const ossia::geometry::gpu_buffer& vtx_buf, MeshBuffers& meshbuf,
      QRhiResourceUpdateBatch& rb) const noexcept;

  void update_index(
      int buffer_index, const ossia::geometry::cpu_buffer& idx_buf, MeshBuffers& meshbuf,
      QRhiResourceUpdateBatch& rb) const noexcept;

  void update_index(
      int buffer_index, const ossia::geometry::gpu_buffer& idx_buf, MeshBuffers& meshbuf,
      QRhiResourceUpdateBatch& rb) const noexcept;
  void update(QRhi& rhi, MeshBuffers& output_meshbuf, QRhiResourceUpdateBatch& rb)
      const noexcept override;
  Flags flags() const noexcept override;

  void clear();

  void preparePipeline(QRhiGraphicsPipeline& pip) const noexcept override;

  void reload(const ossia::mesh_list& ml, const ossia::geometry_filter_list_ptr& f);

  void draw(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;

  // Fallback-aware variant: appends each `FallbackBindingPlan::Slot`
  // buffer to the vertex-input array before issuing the draw. Used by
  // raw-raster pipelines whose shaders declared "REQUIRED: false"
  // VERTEX_INPUTS the upstream geometry doesn't provide. Non-virtual on
  // purpose — only CustomMesh participates in the fallback path.
  void drawWithFallbackBindings(
      const MeshBuffers& bufs, QRhiCommandBuffer& cb,
      std::span<const FallbackBindingPlan::Slot> fallback_slots) const noexcept;

  // Draw a single sub-mesh (geom.meshes[mesh_index]) using the portion of
  // `bufs.buffers` starting at `buffer_offset`. `buffer_offset` must match
  // init()'s flat-concat layout: sum of geom.meshes[0..mesh_index-1].buffers.size().
  // Returns true if a draw call was issued.
  //
  // Exposed so consumers that need per-sub-mesh state (e.g. RawRaster
  // swapping the per_draw SSBO between meshes) can iterate sub-meshes
  // themselves instead of invoking the fire-and-forget `draw()` above.
  //
  // `fallback_slots` (default empty) is merged into the vertex-input
  // array at each slot's binding_index — bindings appended by the
  // fallback-aware remap land past the mesh's own bindings, so slot
  // indices are always contiguous after geom.meshes[mesh_index].input.
  bool drawSingleMesh(
      std::size_t mesh_index, std::size_t buffer_offset,
      const MeshBuffers& bufs, QRhiCommandBuffer& cb,
      std::span<const FallbackBindingPlan::Slot> fallback_slots = {}) const noexcept;

  const char* defaultVertexShader() const noexcept override;

  const ossia::geometry* semanticGeometry() const noexcept override
  {
    if(!geom.meshes.empty())
      return &geom.meshes[0];
    return nullptr;
  }
};

}
