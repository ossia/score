#pragma once
#include <Gfx/Graph/Mesh.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QtGui/private/qrhi_p.h>

namespace score::gfx
{

class CustomMesh : public score::gfx::Mesh
{
  ossia::mesh_list geom;

  using pip = QRhiGraphicsPipeline;
  pip::Topology topology = pip::Topology::TriangleStrip;
  pip::CullMode cullMode = pip::CullMode::None;
  pip::FrontFace frontFace = pip::FrontFace::CW;

  ossia::small_vector<QRhiVertexInputBinding, 2> vertexBindings;
  ossia::small_vector<QRhiVertexInputAttribute, 2> vertexAttributes;

  ossia::small_vector<QRhiBuffer*, 2> buffers;
  QRhiBuffer* index{};

public:
  explicit CustomMesh(
      const ossia::mesh_list& g, const ossia::geometry_filter_list_ptr& f);

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

  const char* defaultVertexShader() const noexcept override;
};

}
