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

public:
  explicit CustomMesh(
      const ossia::mesh_list& g, const ossia::geometry_filter_list_ptr& f)
  {
    reload(g, f);
  }

  [[nodiscard]]
  QRhiBuffer* init_vbo(const ossia::geometry::cpu_buffer& buf, QRhi& rhi) const noexcept
  {
    const auto vtx_buf_size = buf.size;
    auto mesh_buf
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, vtx_buf_size);
    mesh_buf->setName("Mesh::mesh_buf");
    mesh_buf->create();

    return mesh_buf;
  }

  [[nodiscard]]
  QRhiBuffer* init_vbo(const ossia::geometry::gpu_buffer& buf, QRhi& rhi) const noexcept
  {
    return static_cast<QRhiBuffer*>(buf.handle);
  }
  [[nodiscard]]
  QRhiBuffer*
  init_index(const ossia::geometry::cpu_buffer& buf, QRhi& rhi) const noexcept
  {
    QRhiBuffer* idx_buf{};
    if(const auto idx_buf_size = buf.size; idx_buf_size > 0)
    {
      idx_buf
          = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer, idx_buf_size);
      idx_buf->setName("Mesh::idx_buf");
      idx_buf->create();
    }

    return idx_buf;
  }

  [[nodiscard]]
  QRhiBuffer*
  init_index(const ossia::geometry::gpu_buffer& buf, QRhi& rhi) const noexcept
  {
    return static_cast<QRhiBuffer*>(buf.handle);
  }

  [[nodiscard]] MeshBuffers init(QRhi& rhi) const noexcept override
  {
    if(geom.meshes.empty())
      return {};
    if(geom.meshes[0].buffers.empty())
      return {};

    MeshBuffers ret;
    ret.mesh = ossia::visit(
        [&](auto& buf) { return init_vbo(buf, rhi); }, geom.meshes[0].buffers[0].data);
    if(geom.meshes[0].buffers.size() > 1)
      ret.index = ossia::visit([&](auto& buf) {
        return init_index(buf, rhi);
      }, geom.meshes[0].buffers[1].data);
    return ret;
  }

  void update_vbo(
      const ossia::geometry::cpu_buffer& vtx_buf, MeshBuffers& meshbuf,
      QRhiResourceUpdateBatch& rb) const noexcept
  {
    if(auto sz = vtx_buf.size; sz != meshbuf.mesh->size())
    {
      meshbuf.mesh->destroy();
      meshbuf.mesh->setSize(sz);
      meshbuf.mesh->create();
    }
    rb.updateDynamicBuffer(meshbuf.mesh, 0, meshbuf.mesh->size(), vtx_buf.data.get());
  }

  void update_vbo(
      const ossia::geometry::gpu_buffer& vtx_buf, MeshBuffers& meshbuf,
      QRhiResourceUpdateBatch& rb) const noexcept
  {
  }

  void update_index(
      const ossia::geometry::cpu_buffer& idx_buf, MeshBuffers& meshbuf,
      QRhiResourceUpdateBatch& rb) const noexcept
  {
    void* idx_buf_data = nullptr;
    if(meshbuf.index)
    {
      if(geom.meshes[0].buffers.size() > 1)
      {
        if(const auto idx_buf_size = idx_buf.size; idx_buf_size > 0)
        {
          idx_buf_data = idx_buf.data.get();
          // FIXME what if index disappears
          if(auto sz = idx_buf.size; sz != meshbuf.index->size())
          {
            meshbuf.index->destroy();
            meshbuf.index->setSize(sz);
            meshbuf.index->create();
          }
          else
          {
          }
        }
      }
    }
    else
    {
      // FIXME what if index appears
    }

    if(meshbuf.index && idx_buf_data)
    {
      rb.updateDynamicBuffer(meshbuf.index, 0, meshbuf.index->size(), idx_buf_data);
    }
  }

  void update_index(
      const ossia::geometry::gpu_buffer& idx_buf, MeshBuffers& meshbuf,
      QRhiResourceUpdateBatch& rb) const noexcept
  {
  }
  void update(MeshBuffers& meshbuf, QRhiResourceUpdateBatch& rb) const noexcept override
  {
    if(geom.meshes.empty())
      return;
    if(geom.meshes[0].buffers.empty())
      return;
    ossia::visit([&](auto& buf) {
      return update_vbo(buf, meshbuf, rb);
    }, geom.meshes[0].buffers[0].data);

    if(geom.meshes[0].buffers.size() > 1)
      ossia::visit([&](auto& buf) {
        return update_index(buf, meshbuf, rb);
      }, geom.meshes[0].buffers[1].data);
  }
  Flags flags() const noexcept override
  {
    Flags f{};
    for(auto& attr : vertexAttributes)
    {
      switch(attr.location())
      {
        case 0:
          f |= HasPosition;
          break;
        case 1:
          f |= HasTexCoord;
          break;
        case 2:
          f |= HasColor;
          break;
        case 3:
          f |= HasNormals;
          break;
        case 4:
          f |= HasTangents;
          break;
      }
    }
    return f;
  }

  void clear()
  {
    vertexBindings.clear();
    vertexAttributes.clear();
  }

  void preparePipeline(QRhiGraphicsPipeline& pip) const noexcept override
  {
    if(cullMode == QRhiGraphicsPipeline::None)
    {
      pip.setDepthTest(false);
      pip.setDepthWrite(false);
    }
    else
    {
      pip.setDepthTest(true);
      pip.setDepthWrite(true);
    }

    pip.setTopology(this->topology);
    pip.setCullMode(this->cullMode);
    pip.setFrontFace(this->frontFace);

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings(this->vertexBindings.begin(), this->vertexBindings.end());
    inputLayout.setAttributes(
        this->vertexAttributes.begin(), this->vertexAttributes.end());
    pip.setVertexInputLayout(inputLayout);
  }

  void reload(const ossia::mesh_list& ml, const ossia::geometry_filter_list_ptr& f)
  {
    this->geom = ml;
    this->filters = f;

    if(this->geom.meshes.size() == 0)
    {
      qDebug() << "Clearing geometry: ";
      //  clear();
      return;
    }

    auto& g = this->geom.meshes[0];

    vertexBindings.clear();
    for(auto& binding : g.bindings)
    {
      vertexBindings.emplace_back(
          binding.stride, (QRhiVertexInputBinding::Classification)binding.classification,
          binding.step_rate);
    }

    vertexAttributes.clear();
    for(auto& attr : g.attributes)
    {
      vertexAttributes.emplace_back(
          attr.binding, attr.location, (QRhiVertexInputAttribute::Format)attr.format,
          attr.offset);
    }

    if(g.buffers.empty())
    {
      qDebug() << "Error: empty buffer !";
      clear();
    }

    topology = (QRhiGraphicsPipeline::Topology)g.topology;
    cullMode = (QRhiGraphicsPipeline::CullMode)g.cull_mode;
    frontFace = (QRhiGraphicsPipeline::FrontFace)g.front_face;
  }

  void draw(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override
  {
    for(auto& g : this->geom.meshes)
    {
      const auto sz = g.input.size();

      QVarLengthArray<QRhiCommandBuffer::VertexInput> bindings(sz);

      int i = 0;
      for(auto& in : g.input)
      {
        bindings[i++] = {bufs.mesh, in.offset};
      }

      if(g.index.buffer >= 0)
      {
        const auto idxFmt = g.index.format == decltype(g.index)::uint16
                                ? QRhiCommandBuffer::IndexUInt32
                                : QRhiCommandBuffer::IndexUInt32;
        cb.setVertexInput(0, sz, bindings.data(), bufs.index, g.index.offset, idxFmt);
      }
      else
      {
        cb.setVertexInput(0, sz, bindings.data());
      }

      if(g.index.buffer > -1)
      {
        cb.drawIndexed(g.indices);
      }
      else
      {
        cb.draw(g.vertices);
      }
    }
  }

  const char* defaultVertexShader() const noexcept override
  {
    return "";
  }
};

}
