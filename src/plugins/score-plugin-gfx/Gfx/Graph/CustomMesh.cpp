#include "CustomMesh.hpp"

#include <Gfx/Graph/Utils.hpp>

// TODO: extend MeshBufs to hold multiple buffers
// TODO: check that rendering e.g. sponza still works
namespace score::gfx{

CustomMesh::CustomMesh(const ossia::mesh_list &g, const ossia::geometry_filter_list_ptr &f)
{
  reload(g, f);
}

QRhiBuffer *CustomMesh::init_vbo(const ossia::geometry::cpu_buffer &buf, QRhi &rhi) const noexcept
{
  static std::atomic_int idx = 0;
  const auto vtx_buf_size = buf.byte_size;
  auto mesh_buf
      = rhi.newBuffer(QRhiBuffer::Static, QRhiBuffer::VertexBuffer, vtx_buf_size);
  mesh_buf->setName(QString("Mesh::mesh_buf.%1").arg(idx.load(std::memory_order_relaxed)).toLatin1());
  mesh_buf->create();

  return mesh_buf;
}

QRhiBuffer *CustomMesh::init_vbo(const ossia::geometry::gpu_buffer &buf, QRhi &rhi) const noexcept
{
  return static_cast<QRhiBuffer*>(buf.handle);
}

QRhiBuffer *CustomMesh::init_index(const ossia::geometry::cpu_buffer &buf, QRhi &rhi) const noexcept
{
  QRhiBuffer* idx_buf{};
  if(const auto idx_buf_size = buf.byte_size; idx_buf_size > 0)
  {
    idx_buf = rhi.newBuffer(QRhiBuffer::Static, QRhiBuffer::IndexBuffer, idx_buf_size);
    idx_buf->setName("Mesh::idx_buf");
    idx_buf->create();
  }

  return idx_buf;
}

QRhiBuffer *CustomMesh::init_index(const ossia::geometry::gpu_buffer &buf, QRhi &rhi) const noexcept
{
  return static_cast<QRhiBuffer*>(buf.handle);
}

MeshBuffers CustomMesh::init(QRhi &rhi) const noexcept
{
  if(geom.meshes.empty())
  {
    return {};
  }
  if(geom.meshes[0].buffers.empty())
  {
    return {};
  }

  MeshBuffers ret;
  // FIXME multi-mesh
  auto& mesh = geom.meshes[0];

  // 1. Null check
  bool any_is_null = false;
  for(const auto& buf : mesh.buffers)
  {
    any_is_null |= ossia::visit([&]<typename Buffer>(Buffer& buf) {
      if constexpr(std::is_same_v<Buffer, ossia::geometry::cpu_buffer>)
      {
        return buf.byte_size == 0 || buf.data == nullptr;
      }
      else if constexpr(std::is_same_v<Buffer, ossia::geometry::gpu_buffer>)
      {
        return buf.handle == nullptr;
      }
      return false;
    }, buf.data);
  }

  if(any_is_null)
  {
    return {};
  }

  int i = 0;
  int index_i = mesh.index.buffer;

  for(const auto& buf : mesh.buffers)
  {
    if(i != index_i)
    {
      auto rhi_buf
          = ossia::visit([&](auto& buf) { return init_vbo(buf, rhi); }, buf.data);
      ret.buffers.emplace_back(rhi_buf, 0, 0);
    }
    else
    {
      auto rhi_buf
          = ossia::visit([&](auto& buf) { return init_index(buf, rhi); }, buf.data);
      ret.buffers.emplace_back(rhi_buf, 0, 0);
    }
    i++;
  }
  return ret;
}

void CustomMesh::update_vbo(
    int buffer_index, const ossia::geometry::cpu_buffer& vtx_buf, MeshBuffers& meshbuf,
    QRhiResourceUpdateBatch& rb) const noexcept
{
  if(meshbuf.buffers.size() <= buffer_index)
    return;

  auto buffer = meshbuf.buffers[buffer_index].handle; // FIXME use offset here?
  if(auto sz = vtx_buf.byte_size; sz != buffer->size())
  {
    buffer->destroy();
    buffer->setSize(sz);
    buffer->create();
  }
  // FIXME support offset
  uploadStaticBufferWithStoredData(
      &rb, buffer, 0, buffer->size(), (const char*)vtx_buf.raw_data.get());
}

void CustomMesh::update_vbo(
    int buffer_index, const ossia::geometry::gpu_buffer& vtx_buf, MeshBuffers& meshbuf,
    QRhiResourceUpdateBatch& rb) const noexcept
{
  if(meshbuf.buffers.size() <= buffer_index)
    return;

  // FIXME offset, size ?
  // FIXME check if memory of previous buffer gets freed?
  meshbuf.buffers[buffer_index] = {static_cast<QRhiBuffer*>(vtx_buf.handle), 0, 0};
}

void CustomMesh::update_index(
    int buffer_index, const ossia::geometry::cpu_buffer& idx_buf, MeshBuffers& meshbuf,
    QRhiResourceUpdateBatch& rb) const noexcept
{
  if(meshbuf.buffers.size() <= buffer_index)
    return;

  void* idx_buf_data = nullptr;
  auto buffer = meshbuf.buffers[buffer_index].handle; // FIXME use offset here?
  if(buffer)
  {
    if(geom.meshes[0].buffers.size() > 1)
    {
      if(const auto idx_buf_size = idx_buf.byte_size; idx_buf_size > 0)
      {
        idx_buf_data = idx_buf.raw_data.get();
        // FIXME what if index disappears
        if(auto sz = idx_buf.byte_size; sz != buffer->size())
        {
          buffer->destroy();
          buffer->setSize(sz);
          buffer->create();
        }
        else
        {
        }
      }
    }
    else
    {
      // FIXME what if index appears
    }
  }
  else
  {
    // FIXME what if index appears
  }

  if(buffer && idx_buf_data)
  {
    // FIXME support offset
    uploadStaticBufferWithStoredData(
        &rb, buffer, 0, buffer->size(), (const char*)idx_buf_data);
  }
}

void CustomMesh::update_index(
    int buffer_index, const ossia::geometry::gpu_buffer& idx_buf, MeshBuffers& meshbuf,
    QRhiResourceUpdateBatch& rb) const noexcept
{
  SCORE_ASSERT(meshbuf.buffers.size() > buffer_index);
}

void CustomMesh::update(
    QRhi& rhi, MeshBuffers& output_meshbuf, QRhiResourceUpdateBatch& rb) const noexcept
{
  if(geom.meshes.empty())
    return;

  // FIXME multi-mesh
  auto& input_mesh = geom.meshes[0];
  if(input_mesh.buffers.empty())
  {
    return;
  }
  if(output_meshbuf.buffers.empty())
  {
    output_meshbuf = init(rhi);
  }
  if(output_meshbuf.buffers.empty())
  {
    return;
  }

  int i = 0;
  int index_i = input_mesh.index.buffer;

  for(const auto& buf : input_mesh.buffers)
  {
    if(i != index_i)
    {
      ossia::visit(
          [&](auto& buf) { return update_vbo(i, buf, output_meshbuf, rb); }, buf.data);
    }
    else
    {
      ossia::visit(
          [&](auto& buf) { return update_index(i, buf, output_meshbuf, rb); }, buf.data);
    }
    i++;
  }
}

Mesh::Flags CustomMesh::flags() const noexcept
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

void CustomMesh::clear()
{
  vertexBindings.clear();
  vertexAttributes.clear();
}

void CustomMesh::preparePipeline(QRhiGraphicsPipeline &pip) const noexcept
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

void CustomMesh::reload(const ossia::mesh_list &ml, const ossia::geometry_filter_list_ptr &f)
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
        binding.byte_stride,
        (QRhiVertexInputBinding::Classification)binding.classification,
        binding.step_rate);
  }

  vertexAttributes.clear();
  for(auto& attr : g.attributes)
  {
    vertexAttributes.emplace_back(
        attr.binding, attr.location, (QRhiVertexInputAttribute::Format)attr.format,
        attr.byte_offset);
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

void CustomMesh::draw(const MeshBuffers &bufs, QRhiCommandBuffer &cb) const noexcept
{
  for(auto& g : this->geom.meshes)
  {
    const auto sz = g.input.size();

    QVarLengthArray<QRhiCommandBuffer::VertexInput> draw_inputs(sz);

    int i = 0;
    for(auto& in : g.input)
    {
      // FIXME buffer offset? input offset?
      if(bufs.buffers.size() <= in.buffer)
        return;

      auto buf = bufs.buffers[in.buffer].handle;
      if(!buf)
        return;
      draw_inputs[i++] = {buf, in.byte_offset};
    }

    if(g.index.buffer >= 0)
    {
      auto buf = bufs.buffers[g.index.buffer].handle;
      const auto idxFmt = g.index.format == decltype(g.index)::uint16
                              ? QRhiCommandBuffer::IndexUInt32
                              : QRhiCommandBuffer::IndexUInt32;
      cb.setVertexInput(0, sz, draw_inputs.data(), buf, g.index.byte_offset, idxFmt);
    }
    else
    {
      cb.setVertexInput(0, sz, draw_inputs.data());
    }

    if(g.index.buffer > -1)
    {
      cb.drawIndexed(g.indices, g.instances);
    }
    else
    {
      cb.draw(g.vertices, g.instances);
    }
  }
}

const char *CustomMesh::defaultVertexShader() const noexcept
{
  return "";
}

}
