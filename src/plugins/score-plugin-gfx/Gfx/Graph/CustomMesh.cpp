#include "CustomMesh.hpp"
#include <score/tools/Debug.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <QDebug>

#include <cstdlib>

// TODO: extend MeshBufs to hold multiple buffers
// TODO: check that rendering e.g. sponza still works
namespace score::gfx{

// [BUFTRACE] implementation — see CustomMesh.hpp. Turn off at runtime
// by setting SCORE_BUFTRACE=0.
bool buftrace_enabled()
{
  static const bool on = [] {
    const char* v = std::getenv("SCORE_BUFTRACE");
    return !v || v[0] != '0';
  }();
  return on;
}

CustomMesh::CustomMesh(const ossia::mesh_list &g, const ossia::geometry_filter_list_ptr &f)
{
  reload(g, f);
}

QRhiBuffer *CustomMesh::init_vbo(const ossia::geometry::cpu_buffer &buf, QRhi &rhi) const noexcept
{
  static std::atomic_int idx = 0;
  const auto vtx_buf_size = buf.byte_size;
  auto mesh_buf = rhi.newBuffer(
      QRhiBuffer::Static, QRhiBuffer::StorageBuffer | QRhiBuffer::VertexBuffer,
      vtx_buf_size);
  mesh_buf->setName(
      QString("Mesh::vtx_buf.%1").arg(idx.load(std::memory_order_relaxed)).toLatin1());
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

  MeshBuffers ret;

  // Multi-mesh: concatenate every mesh's buffers into ret.buffers in order.
  // Each sub-mesh's local `input[].buffer` / `index.buffer` indices are
  // remapped at draw time by adding the sub-mesh's starting offset in
  // ret.buffers. The first sub-mesh's layout drives the pipeline
  // (vertex bindings / attributes) in reload() — sub-meshes with a
  // different layout are not supported today and will draw incorrectly.
  for(std::size_t mi = 0; mi < geom.meshes.size(); ++mi)
  {
    const auto& mesh = geom.meshes[mi];
    if(mesh.buffers.empty())
      continue;

    // Null check — skip a sub-mesh whose data isn't ready yet.
    bool any_is_null = false;
    for(const auto& buf : mesh.buffers)
    {
      any_is_null |= ossia::visit([&]<typename Buffer>(Buffer& buf) {
        if constexpr(std::is_same_v<Buffer, ossia::geometry::cpu_buffer>)
          return buf.byte_size == 0 || buf.data == nullptr;
        else if constexpr(std::is_same_v<Buffer, ossia::geometry::gpu_buffer>)
          return buf.handle == nullptr;
        return false;
      }, buf.data);
    }
    if(any_is_null)
    {
      // Emit null placeholders so indexing stays aligned with geom.meshes.
      for(std::size_t k = 0; k < mesh.buffers.size(); ++k)
        ret.buffers.emplace_back(nullptr, 0, 0);
      continue;
    }

    int i = 0;
    const int index_i = mesh.index.buffer;
    for(const auto& buf : mesh.buffers)
    {
      QRhiBuffer* rhi_buf = (i != index_i)
          ? ossia::visit([&](auto& b) { return init_vbo(b, rhi); }, buf.data)
          : ossia::visit([&](auto& b) { return init_index(b, rhi); }, buf.data);
      // Ownership follows the source variant: cpu_buffer paths allocate
      // fresh QRhiBuffers (owned), gpu_buffer paths borrow an upstream
      // handle (unowned — the original producer still owns it).
      const bool owned = ossia::visit(
          []<typename Buffer>(const Buffer&) {
            return std::is_same_v<Buffer, ossia::geometry::cpu_buffer>;
          }, buf.data);
      BufferView bv{};
      bv.handle = rhi_buf;
      bv.owned = owned;
      ret.buffers.emplace_back(bv);
      i++;
    }
  }

  if(ret.buffers.empty())
    return {};

  // Indirect draw / cpu_draw_commands: only meaningful when a single output
  // mesh carries them (ScenePreprocessor's MDI mode). Pick them up from mesh[0].
  const auto& first_mesh = geom.meshes[0];
  if(first_mesh.indirect_count.handle)
  {
    ret.indirectDrawBuffer = static_cast<QRhiBuffer*>(first_mesh.indirect_count.handle);
    ret.useIndirectDraw = true;
    ret.indirectDrawIndexed = (first_mesh.index.buffer >= 0);
    ret.indirectDrawCount
        = first_mesh.indirect_count.byte_size / (5 * sizeof(uint32_t));
    ret.indirectDrawStride = 5 * sizeof(uint32_t);
    if(ret.indirectDrawCount == 0)
      ret.indirectDrawCount = 1;
  }
  if(!first_mesh.cpu_draw_commands.empty())
    ret.cpuDrawCommands.assign(
        first_mesh.cpu_draw_commands.begin(), first_mesh.cpu_draw_commands.end());

  return ret;
}

void CustomMesh::update_vbo(
    int buffer_index, const ossia::geometry::cpu_buffer& vtx_buf, MeshBuffers& meshbuf,
    QRhiResourceUpdateBatch& rb) const noexcept
{
  if(meshbuf.buffers.size() <= buffer_index)
    return;

  auto buffer = meshbuf.buffers[buffer_index].handle; // FIXME use offset here?
  if(!buffer)
    return;
  if(auto sz = vtx_buf.byte_size; sz != buffer->size())
  {
    qDebug() << "CustomMesh::update_vbo: resizing buffer from"
             << buffer->size() << "to" << sz
             << "buffer=" << (void*)buffer;
    buffer->setSize(sz);
    if(!buffer->create())
      qWarning() << "CustomMesh::update_vbo: buffer->create() FAILED after resize!";
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
  auto* old_buf = meshbuf.buffers[buffer_index].handle;
  auto* new_buf = static_cast<QRhiBuffer*>(vtx_buf.handle);
  if(old_buf != new_buf)
  {
    BUFTRACE() << "update_vbo(gpu) mesh=" << (void*)this
               << " slot=" << buffer_index
               << " old=" << (void*)old_buf
               << " new=" << (void*)new_buf
               << " size=" << (qint64)vtx_buf.byte_size
               << " (old handle abandoned without deleteLater — upstream "
                  "owner must still hold it, ASan will flag if not)";
  }
  // Replacement entry must carry owned=false: the handle belongs to the
  // upstream gpu_buffer producer. Default-constructed BufferView has
  // owned=true → RenderList::release would `delete` a borrowed handle.
  BufferView bv{};
  bv.handle = new_buf;
  bv.owned = false;
  meshbuf.buffers[buffer_index] = bv;
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
  auto* old_buf = meshbuf.buffers[buffer_index].handle;
  auto* new_buf = static_cast<QRhiBuffer*>(idx_buf.handle);
  if(old_buf != new_buf)
  {
    BUFTRACE() << "update_index(gpu) mesh=" << (void*)this
               << " slot=" << buffer_index
               << " old=" << (void*)old_buf
               << " new=" << (void*)new_buf
               << " size=" << (qint64)idx_buf.byte_size
               << " (old handle abandoned — if ASan fires on this slot "
                  "on next bind, the owner freed it too early)";
    BufferView bv{};
    bv.handle = new_buf;
    bv.owned = false;
    meshbuf.buffers[buffer_index] = bv;
  }
}

void CustomMesh::update(
    QRhi& rhi, MeshBuffers& output_meshbuf, QRhiResourceUpdateBatch& rb) const noexcept
{
  if(geom.meshes.empty())
    return;

  // Grow output_meshbuf.buffers when the geometry has added more
  // buffers than mb has slots for (e.g. a model swap from Box.gltf →
  // Duck.gltf where Duck has more vertex buffers, or
  // ScenePreprocessor appending instance + scene-aux entries beyond
  // the existing slot count). Without this, update_vbo's
  // `if(meshbuf.buffers.size() <= buffer_index) return;` silently
  // drops writes for new high-index buffers, stale handles persist,
  // and the next setVertexInput binds them as vertex inputs —
  // validation flags `pBuffers[N] is INDEX_BUFFER / STORAGE_BUFFER,
  // requires VERTEX_BUFFER`.
  //
  // We *grow* rather than re-init: re-initialising forces init()
  // through its any-buffer-null bail-out (which emits null placeholders
  // for the WHOLE sub-mesh whenever any single buffer is null), which
  // breaks scenes where a conditional aux buffer transiently goes
  // null. Growing preserves the live handles already bound to
  // populated slots; new slots get null placeholders and the
  // update_vbo / update_index loop below fills them in.
  //
  // Shrinking is intentionally not done: extra trailing slots beyond
  // what g.input / g.index reference are harmless (the draw path
  // never indexes into them), and shrinking would require explicit
  // release of the truncated owned buffers.
  std::size_t total_geom_buffers = 0;
  for(const auto& m : geom.meshes)
    total_geom_buffers += m.buffers.size();
  if(output_meshbuf.buffers.size() < total_geom_buffers)
  {
    BUFTRACE() << "CustomMesh::update: growing MeshBuffers from "
               << (qsizetype)output_meshbuf.buffers.size()
               << " to " << (qsizetype)total_geom_buffers
               << " slots (preserving existing handles)";
    output_meshbuf.buffers.resize(
        total_geom_buffers, BufferView{nullptr, 0, 0});
  }

  if(output_meshbuf.buffers.empty())
    output_meshbuf = init(rhi);
  if(output_meshbuf.buffers.empty())
    return;

  // Upload each sub-mesh's buffers, remapping local indices to the flat
  // offset in output_meshbuf.buffers built by init().
  std::size_t base = 0;
  for(const auto& input_mesh : geom.meshes)
  {
    if(input_mesh.buffers.empty())
      continue;
    if(base + input_mesh.buffers.size() > output_meshbuf.buffers.size())
      break;

    int i = 0;
    const int index_i = input_mesh.index.buffer;
    for(const auto& buf : input_mesh.buffers)
    {
      const int flat = int(base) + i;
      if(i != index_i)
      {
        ossia::visit(
            [&](auto& buf) { return update_vbo(flat, buf, output_meshbuf, rb); },
            buf.data);
      }
      else
      {
        ossia::visit(
            [&](auto& buf) { return update_index(flat, buf, output_meshbuf, rb); },
            buf.data);
      }
      i++;
    }
    base += input_mesh.buffers.size();
  }

  // Indirect draw / cpu_draw_commands: same single-mesh scoping as init().
  const auto& first_mesh = geom.meshes[0];
  if(first_mesh.indirect_count.handle)
  {
    output_meshbuf.indirectDrawBuffer
        = static_cast<QRhiBuffer*>(first_mesh.indirect_count.handle);
    output_meshbuf.useIndirectDraw = true;
    output_meshbuf.indirectDrawIndexed = (first_mesh.index.buffer >= 0);
  }
  else
  {
    output_meshbuf.indirectDrawBuffer = nullptr;
    output_meshbuf.useIndirectDraw = false;
    output_meshbuf.indirectDrawIndexed = false;
  }

  if(!first_mesh.cpu_draw_commands.empty())
  {
    output_meshbuf.cpuDrawCommands.assign(
        first_mesh.cpu_draw_commands.begin(), first_mesh.cpu_draw_commands.end());
  }

  // Note: GPU readback for the indirect draw fallback is handled
  // synchronously in RenderedRawRasterPipelineNode::runInitialPasses,
  // which has access to both the command buffer and QRhi::finish().
}

Mesh::Flags CustomMesh::flags() const noexcept
{
  Flags f{};
  for(auto sem : attributeSemantics)
  {
    switch(sem)
    {
      case ossia::attribute_semantic::position:
        f |= HasPosition;
        break;
      case ossia::attribute_semantic::texcoord0:
        f |= HasTexCoord;
        break;
      case ossia::attribute_semantic::color0:
        f |= HasColor;
        break;
      case ossia::attribute_semantic::normal:
        f |= HasNormals;
        break;
      case ossia::attribute_semantic::tangent:
        f |= HasTangents;
        break;
      default:
        break;
    }
  }
  return f;
}

void CustomMesh::clear()
{
  vertexBindings.clear();
  vertexAttributes.clear();
  attributeSemantics.clear();
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
    // Reverse-Z project rule.
    pip.setDepthOp(QRhiGraphicsPipeline::Greater);
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
  BUFTRACE() << "CustomMesh::reload mesh=" << (void*)this
             << " meshes=" << (qsizetype)ml.meshes.size()
             << " first_buf_count="
             << (ml.meshes.empty() ? (qsizetype)-1
                                    : (qsizetype)ml.meshes[0].buffers.size());
  this->geom = ml;
  this->filters = f;

  if(this->geom.meshes.size() == 0)
  {
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
  attributeSemantics.clear();

  // Assign linear locations (0, 1, 2, ...) in order of appearance.
  // The actual semantic-to-shader-location mapping is done at pipeline
  // creation time by remapPipelineVertexInputs(), which matches shader
  // input variable names to geometry attribute semantics.
  int location = 0;
  for(auto& attr : g.attributes)
  {
    vertexAttributes.emplace_back(
        attr.binding, location++, (QRhiVertexInputAttribute::Format)attr.format,
        attr.byte_offset);
    attributeSemantics.push_back(attr.semantic);
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

bool CustomMesh::drawSingleMesh(
    std::size_t mesh_index, std::size_t base, const MeshBuffers& bufs,
    QRhiCommandBuffer& cb,
    std::span<const FallbackBindingPlan::Slot> fallback_slots) const noexcept
{
  if(mesh_index >= geom.meshes.size())
    return false;
  const auto& g = geom.meshes[mesh_index];

  // Total vertex-input count = mesh bindings + fallback bindings. The
  // fallback slots' binding_index values were allocated sequentially
  // past the mesh's own bindings when the pipeline was built
  // (remapPipelineVertexInputs); they land at indices sz, sz+1, ... here.
  const auto mesh_input_count = g.input.size();
  const auto total = mesh_input_count + fallback_slots.size();
  QVarLengthArray<QRhiCommandBuffer::VertexInput> draw_inputs(total);

  int i = 0;
  for(auto& in : g.input)
  {
    const std::size_t flat = base + (std::size_t)in.buffer;
    if(flat >= bufs.buffers.size())
      return false;
    auto buf = bufs.buffers[flat].handle;
    if(!buf)
      return false;
    draw_inputs[i++] = {buf, in.byte_offset};
  }

  // Fallback slots. Each Slot::binding_index is expressed in the global
  // binding-index space; for a single-sub-mesh raw-raster draw it's
  // always `mesh_input_count + k` for the k'th slot, so we place the
  // buffers by index.
  for(const auto& slot : fallback_slots)
  {
    const std::size_t idx = (std::size_t)slot.binding_index;
    if(idx >= total || !slot.buffer)
      continue;   // defensive: skip malformed plans rather than dropping the draw
    draw_inputs[idx] = {slot.buffer, 0};
  }

  if(g.index.buffer >= 0)
  {
    const std::size_t flat_idx = base + (std::size_t)g.index.buffer;
    if(flat_idx >= bufs.buffers.size())
      return false;
    auto buf = bufs.buffers[flat_idx].handle;
    const auto idxFmt = g.index.format == decltype(g.index)::uint16
                            ? QRhiCommandBuffer::IndexUInt16
                            : QRhiCommandBuffer::IndexUInt32;
    // If this bind crashes with a dangling buffer, the `buf` pointer
    // logged here will match ASan's freed-at report. The mesh= and
    // slot= fields tell us which CustomMesh and which MeshBuffers
    // entry retained it.
    BUFTRACE() << "bindIndexBuffer mesh=" << (void*)this
               << " sub=" << mesh_index << " slot=" << flat_idx
               << " buf=" << (void*)buf
               << " offset=" << (qint64)g.index.byte_offset
               << " bufs.size=" << (qsizetype)bufs.buffers.size();
    cb.setVertexInput(
        0, (int)total, draw_inputs.data(), buf, g.index.byte_offset, idxFmt);
  }
  else
  {
    cb.setVertexInput(0, (int)total, draw_inputs.data());
  }

  // Per-mesh indirect override: when THIS submesh carries its own
  // `indirect_count` handle (different from bufs.indirectDrawBuffer),
  // use it instead. Required for multi-batch MDI (opaque + transparent
  // split emitted by ScenePreprocessor) where each sub-mesh drives a
  // separate indirect-cmd list. Same rule for `cpu_draw_commands`.
  QRhiBuffer* effIndirectBuf = bufs.indirectDrawBuffer;
  quint32     effIndirectCount = bufs.indirectDrawCount;
  const auto* effCpuCmds = &bufs.cpuDrawCommands;
  std::decay_t<decltype(bufs.cpuDrawCommands)> perMeshCmds;
  if(auto* h = static_cast<QRhiBuffer*>(g.indirect_count.handle))
  {
    effIndirectBuf = h;
    effIndirectCount
        = (quint32)(g.indirect_count.byte_size / (5 * sizeof(uint32_t)));
    if(effIndirectCount == 0)
      effIndirectCount = 1;
  }
  if(!g.cpu_draw_commands.empty())
  {
    perMeshCmds.assign(g.cpu_draw_commands.begin(), g.cpu_draw_commands.end());
    effCpuCmds = &perMeshCmds;
  }

  // Multi-draw indirect: runtime capability check, not compile-time.
  // Only meaningful for single-sub-mesh MDI-mode geometries.
  if(bufs.useIndirectDraw && effIndirectBuf)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    if(bufs.gpuIndirectSupported)
    {
      if(bufs.indirectDrawIndexed)
        cb.drawIndexedIndirect(
            effIndirectBuf, bufs.indirectDrawOffset,
            effIndirectCount, bufs.indirectDrawStride);
      else
        cb.drawIndirect(
            effIndirectBuf, bufs.indirectDrawOffset,
            effIndirectCount, bufs.indirectDrawStride);
      return true;
    }
#endif

    // CPU fallback: iterate draw commands with correct firstInstance /
    // baseVertex so each sub-draw gets its own per-draw data via
    // gl_BaseInstance. Commands come from either the producer
    // (ScenePreprocessor) or GPU readback (CSF).
    if(!effCpuCmds->empty())
    {
      const bool indexed = (g.index.buffer >= 0);
      for(const auto& cmd : *effCpuCmds)
      {
        if(indexed)
          cb.drawIndexed(
              cmd.index_or_vertex_count, cmd.instance_count,
              cmd.first_index_or_vertex, cmd.base_vertex, cmd.first_instance);
        else
          cb.draw(
              cmd.index_or_vertex_count, cmd.instance_count,
              cmd.first_index_or_vertex, cmd.first_instance);
      }
      return true;
    }
    // No CPU commands yet (readback pending or first frame) — skip.
    return false;
  }

  if(g.index.buffer > -1)
    cb.drawIndexed(g.indices, g.instances);
  else
    cb.draw(g.vertices, g.instances);
  return true;
}

void CustomMesh::draw(const MeshBuffers &bufs, QRhiCommandBuffer &cb) const noexcept
{
  // Default draw path: iterate sub-meshes without any per-mesh state swap.
  // Works for single-mesh geometries and for MDI mode (one sub-mesh with an
  // indirect buffer). For multi-sub-mesh + per-mesh SRB auxes (classic
  // per-mesh ScenePreprocessor output), the caller should instead iterate
  // drawSingleMesh() itself and rebind the SRB between sub-meshes.
  std::size_t base = 0;
  for(std::size_t i = 0; i < geom.meshes.size(); ++i)
  {
    drawSingleMesh(i, base, bufs, cb);
    base += geom.meshes[i].buffers.size();
  }
}

void CustomMesh::drawWithFallbackBindings(
    const MeshBuffers& bufs, QRhiCommandBuffer& cb,
    std::span<const FallbackBindingPlan::Slot> fallback_slots) const noexcept
{
  // Same as draw() but with the caller's fallback-binding plan threaded
  // down to drawSingleMesh so the extra PerInstance identity buffers
  // land in the vertex-input array at the indices the pipeline
  // allocated for them.
  std::size_t base = 0;
  for(std::size_t i = 0; i < geom.meshes.size(); ++i)
  {
    drawSingleMesh(i, base, bufs, cb, fallback_slots);
    base += geom.meshes[i].buffers.size();
  }
}

const char *CustomMesh::defaultVertexShader() const noexcept
{
  return "";
}

}
