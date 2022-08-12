#pragma once
#include <Gfx/Graph/Mesh.hpp>

#include <ossia/dataflow/geometry_port.hpp>

#include <QtGui/private/qrhi_p.h>

namespace score::gfx
{

class CustomMesh : public score::gfx::Mesh
{
  ossia::mesh_list geom;
  tcb::span<const unsigned int> indexArray;
  int indexCount{};

public:
  explicit CustomMesh(const ossia::mesh_list& g) { reload(g); }

#include <Gfx/Qt5CompatPush> // clang-format: keep
  [[nodiscard]] MeshBuffers init(QRhi& rhi) const noexcept override
  {
    auto mesh_buf = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer,
        vertexArray.size() * sizeof(float));
    mesh_buf->setName("Mesh::mesh_buf");
    mesh_buf->create();

    QRhiBuffer* idx_buf{};
    if(!indexArray.empty())
    {
      // FIXME use sizeof(unsigned short) if uint16
      idx_buf = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::IndexBuffer,
          indexArray.size() * sizeof(unsigned int));
      idx_buf->setName("Mesh::idx_buf");
      idx_buf->create();
    }

    MeshBuffers ret{mesh_buf, idx_buf};
    return ret;
  }

  void update(MeshBuffers& meshbuf, QRhiResourceUpdateBatch& rb) const noexcept override
  {
    auto& mesh = *this;
    if(auto sz = mesh.vertexArray.size() * sizeof(float); sz != meshbuf.mesh->size())
    {
      meshbuf.mesh->destroy();
      meshbuf.mesh->setSize(sz);
      meshbuf.mesh->create();
    }

    if(meshbuf.index)
    {
      // FIXME what if index disappears
      if(auto sz = mesh.indexArray.size() * sizeof(unsigned int);
         sz != meshbuf.index->size())
      {
        meshbuf.index->destroy();
        meshbuf.index->setSize(sz);
        meshbuf.index->create();
      }
      else
      {
      }
    }
    else
    {
      // FIXME what if index appears
    }

    rb.updateDynamicBuffer(
        meshbuf.mesh, 0, meshbuf.mesh->size(), mesh.vertexArray.data());
    if(meshbuf.index)
    {
      rb.updateDynamicBuffer(
          meshbuf.index, 0, meshbuf.index->size(), mesh.indexArray.data());
    }
  }
#include <Gfx/Qt5CompatPop> // clang-format: keep

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
    vertexArray = {};
    vertexCount = 0;
    indexArray = {};
    indexCount = 0;
  }

  void reload(const ossia::mesh_list& ml)
  {
    this->geom = ml;

    qDebug() << "Replacing geometry: " << this->geom.meshes.size();
    if(this->geom.meshes.size() == 0)
    {
      clear();
      return;
    }

    auto& g = this->geom.meshes[0];
    qDebug() << g.vertices << g.bindings.size() << g.attributes.size() << g.input.size();

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

    if(!g.buffers.empty())
    {
      vertexArray = tcb::span<const float>(
          (float*)g.buffers[0].data.get(), g.buffers[0].size / sizeof(float));
      vertexCount = g.vertices;

      if(g.index.buffer >= 0 && g.index.buffer < g.buffers.size())
      {
        auto& index_buf = g.buffers[g.index.buffer];
        indexArray = tcb::span<const uint32_t>(
            (uint32_t*)index_buf.data.get(), index_buf.size / sizeof(uint32_t));
        indexCount = g.indices;
      }
    }
    else
    {
      qDebug() << "Error: empty buffer !";
      clear();
    }

    topology = (QRhiGraphicsPipeline::Topology)g.topology;
    cullMode = (QRhiGraphicsPipeline::CullMode)g.cull_mode;
    frontFace = (QRhiGraphicsPipeline::FrontFace)g.front_face;
  }

  void
  setupBindings(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override
  {
    SCORE_ASSERT(this->geom.meshes.size() > 0);
    auto& g = this->geom.meshes[0];
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
      cb.setVertexInput(0, sz, bindings.data());
  }

  void draw(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override
  {
    setupBindings(bufs, cb);

    if(indexCount > 0)
    {
      cb.drawIndexed(indexCount);
    }
    else
    {
      cb.draw(vertexCount);
    }
  }

  const char* defaultVertexShader() const noexcept override
  {
    return "";
  }
};

}
