#include "IsfBindingsBuilder.hpp"

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/tools/Debug.hpp>

namespace
{
// GLSL type → size in bytes (std430 element size, no padding between elements
// in an unsized runtime array).
static int isf_glsl_type_size_bytes(const std::string& type) noexcept
{
  if(type == "float" || type == "int" || type == "uint" || type == "bool")
    return 4;
  if(type == "vec2" || type == "ivec2" || type == "uvec2")
    return 8;
  if(type == "vec3" || type == "ivec3" || type == "uvec3")
    return 12;
  if(type == "vec4" || type == "ivec4" || type == "uvec4")
    return 16;
  if(type == "mat2")
    return 16;
  if(type == "mat3")
    return 48;
  if(type == "mat4")
    return 64;
  return 16;
}

// Return the size in bytes of one element of a std430-laid-out layout:
// sum of all fields, aligned to 16 bytes per field (std430 rule for arrays
// of structs). Arrays `vec4[N]` parse as a suffix on the field type.
static int64_t isf_ssbo_elem_size(
    const std::vector<isf::storage_input::layout_field>& layout) noexcept
{
  int64_t sz = 0;
  for(const auto& f : layout)
  {
    auto type = f.type;
    int64_t count = 1;
    auto lbr = type.find('[');
    if(lbr != std::string::npos)
    {
      auto rbr = type.find(']', lbr + 1);
      if(rbr != std::string::npos && rbr > lbr + 1)
      {
        auto inner = type.substr(lbr + 1, rbr - lbr - 1);
        if(!inner.empty())
        {
          try { count = std::stoll(inner); } catch(...) { count = 1; }
        }
        // else: empty '[]' means runtime-length — counted as 1 element for
        // sizing the fixed part of the struct; the renderer sizes the buffer
        // based on actual data.
      }
      type = type.substr(0, lbr);
    }
    int element = isf_glsl_type_size_bytes(type);
    // std430: elements align to 16 bytes for vec3/mat arrays; keep it simple
    // and align each field to 16 bytes to match the CSF renderer's convention.
    element = (element + 15) & ~15;
    sz += (int64_t)element * count;
  }
  if(sz == 0)
    sz = 16;
  return sz;
}
}

namespace score::gfx
{

QRhiShaderResourceBinding::StageFlags visibilityToStages(std::string_view v) noexcept
{
  using Stage = QRhiShaderResourceBinding;
  if(v == "fragment")
    return Stage::FragmentStage;
  if(v == "vertex")
    return Stage::VertexStage;
  if(v == "vertex+fragment" || v == "both" || v == "graphics" || v == "all")
    return Stage::VertexStage | Stage::FragmentStage;
  if(v == "compute")
    return Stage::ComputeStage;
  if(v == "none")
    return {};
  // Default fallback: fragment visibility (matches the default in isf.hpp).
  return Stage::FragmentStage;
}

void collectGraphicsStorageResources(
    const isf::descriptor& desc, int firstBinding, GraphicsStorageResources& out)
{
  out.ssbos.clear();
  out.images.clear();
  out.indirectDrawBuffer = nullptr;
  out.indirectDrawIndexed = false;
  out.indirectDrawSsboIndex = -1;

  int binding = firstBinding;
  int port_idx = 0;

  for(const auto& inp : desc.inputs)
  {
    if(auto* s = ossia::get_if<isf::storage_input>(&inp.data))
    {
      // Indirect-draw argument buffers don't need a shader-visible binding
      // (the GPU reads them via cb.drawIndirect), but we still track them to
      // refresh pointers from upstream ports.
      if(!s->buffer_usage.empty())
      {
        GraphicsSSBO e;
        e.name = inp.name;
        e.access = s->access;
        e.buffer_usage = s->buffer_usage;
        e.persistent = false;
        e.owned = false; // Pointer comes from upstream
        e.layout = s->layout;
        e.stages = QRhiShaderResourceBinding::StageFlags{}; // No shader binding
        e.binding = -1;
        e.input_port_index = port_idx;
        out.ssbos.push_back(std::move(e));
        out.indirectDrawSsboIndex = (int)out.ssbos.size() - 1;
        out.indirectDrawIndexed = (s->buffer_usage == "indirect_draw_indexed");
      }
      else
      {
        auto stages = visibilityToStages(s->visibility);
        if(stages == QRhiShaderResourceBinding::StageFlags{})
        {
          port_idx++;
          continue;
        }
        GraphicsSSBO e;
        e.name = inp.name;
        e.access = s->access;
        e.persistent = s->persistent;
        e.owned = true;
        e.size = isf_ssbo_elem_size(s->layout);
        e.layout = s->layout;
        e.stages = stages;
        e.binding = binding++;
        e.input_port_index = port_idx;
        if(s->persistent)
          e.prev_binding = binding++;
        out.ssbos.push_back(std::move(e));
      }
    }
    else if(auto* img = ossia::get_if<isf::csf_image_input>(&inp.data))
    {
      auto stages = visibilityToStages(img->visibility);
      if(stages == QRhiShaderResourceBinding::StageFlags{}
         || stages == QRhiShaderResourceBinding::ComputeStage)
      {
        port_idx++;
        continue;
      }
      GraphicsStorageImage e;
      e.name = inp.name;
      e.access = img->access;
      e.format = img->format;
      e.is3D = img->is3D();
      e.persistent = img->persistent;
      e.owned = true;
      e.stages = stages;
      e.binding = binding++;
      e.input_port_index = port_idx;
      if(img->persistent)
        e.prev_binding = binding++;
      out.images.push_back(std::move(e));
    }
    else if(auto* uni = ossia::get_if<isf::uniform_input>(&inp.data))
    {
      auto stages = visibilityToStages(uni->visibility);
      if(stages == QRhiShaderResourceBinding::StageFlags{}
         || stages == QRhiShaderResourceBinding::ComputeStage)
      {
        port_idx++;
        continue;
      }
      GraphicsUBO e;
      e.name = inp.name;
      e.owned = false; // sourced from upstream port each frame
      e.stages = stages;
      e.binding = binding++;
      e.input_port_index = port_idx;
      out.ubos.push_back(std::move(e));
    }
    port_idx++;
  }
}

// --- SSBO allocation ------------------------------------------------------

static QRhiBuffer* allocateSsbo(
    QRhi& rhi, const std::string& name, const std::string& buffer_usage,
    int64_t size)
{
  QRhiBuffer::UsageFlags flags = QRhiBuffer::StorageBuffer;
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
  if(buffer_usage == "indirect_draw" || buffer_usage == "indirect_draw_indexed")
    flags |= QRhiBuffer::IndirectBuffer;
#else
  (void)buffer_usage;
#endif
  auto* buf = rhi.newBuffer(QRhiBuffer::Static, flags, size);
  buf->setName(QByteArray("ISF_SSBO_") + name.c_str());
  if(!buf->create())
  {
    qWarning() << "Failed to create SSBO" << name.c_str();
    delete buf;
    return nullptr;
  }
  return buf;
}

static QRhiTexture::Format parseImageFormat(const std::string& fmt)
{
  std::string f = fmt;
  for(auto& c : f) c = (char)std::tolower((unsigned char)c);
  if(f == "rgba8")   return QRhiTexture::RGBA8;
  if(f == "bgra8")   return QRhiTexture::BGRA8;
  if(f == "r8")      return QRhiTexture::R8;
  if(f == "rg8")     return QRhiTexture::RG8;
  if(f == "r16")     return QRhiTexture::R16;
  if(f == "rg16")    return QRhiTexture::RG16;
  if(f == "r16f")    return QRhiTexture::R16F;
  if(f == "r32f")    return QRhiTexture::R32F;
//  if(f == "rg16f")   return QRhiTexture::RG16F;
//  if(f == "rg32f")   return QRhiTexture::RG32F;
  if(f == "rgba16f") return QRhiTexture::RGBA16F;
  if(f == "rgba32f") return QRhiTexture::RGBA32F;
  if(f == "r8ui"  || f == "r8si"  || f == "r8i")   return QRhiTexture::R8;
  if(f == "r16ui" || f == "r16si" || f == "r16i")  return QRhiTexture::R16;
  if(f == "r32ui" || f == "r32i")                   return QRhiTexture::R32F; // reinterpret
  return QRhiTexture::RGBA8;
}

void ensureStorageResources(
    QRhi& rhi, QRhiResourceUpdateBatch& /*res*/, const RenderList& renderer,
    const isf::descriptor& /*desc*/, GraphicsStorageResources& store,
    QSize renderSize)
{
  // SSBOs
  for(auto& e : store.ssbos)
  {
    if(!e.owned || !e.layout.empty() == false)
    {
      // owned==false: buffer comes from upstream, nothing to allocate here.
      // size derived from layout when persistent; otherwise the user sets
      // it externally (typically matching upstream geometry).
    }
    if(!e.owned)
      continue;
    int64_t target_size = e.size > 0 ? e.size : 16;
    if(!e.buffer)
      e.buffer = allocateSsbo(rhi, e.name, e.buffer_usage, target_size);
    if(e.persistent && !e.prev)
      e.prev = allocateSsbo(rhi, e.name + "_prev", "", target_size);
  }

  // Uniform buffers (UBOs sourced from upstream Buffer ports). The upstream's
  // real buffer is swapped in at runtime by bindUpstreamBuffers — but we need
  // a valid placeholder allocated here so the SRB binding slot exists at
  // pipeline-build time. Without it, Vulkan complains about an invalid
  // descriptor for binding N when the shader reads `camera`.
  for(auto& e : store.ubos)
  {
    if(e.buffer)  // already borrowed from upstream, or previously allocated
      continue;
    // 256 bytes covers the camera UBO (240 B) and most other small UBOs.
    // If the upstream provides a larger buffer we'll replace this at bind time.
    auto* buf = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 256);
    buf->setName(QByteArray("ISF_UBO_placeholder_") + e.name.c_str());
    if(!buf->create())
    {
      qWarning() << "Failed to create placeholder UBO" << e.name.c_str();
      delete buf;
      continue;
    }
    e.buffer = buf;
    e.owned = true;  // we own this placeholder; bindUpstreamBuffers drops ownership when it swaps.
  }

  // Storage images
  for(auto& e : store.images)
  {
    if(!e.owned)
      continue;

    QSize sz = renderSize.isValid() ? renderSize : QSize(256, 256);
    QRhiTexture::Format fmt = parseImageFormat(e.format);
    QRhiTexture::Flags flags = QRhiTexture::UsedWithLoadStore;
    if(e.is3D)
      flags |= QRhiTexture::ThreeDimensional;

    auto make_tex = [&](const char* suffix) -> QRhiTexture* {
      auto* t = e.is3D
          ? rhi.newTexture(fmt, sz.width(), sz.height(), sz.width(), 1, flags)
          : rhi.newTexture(fmt, sz, 1, flags);
      t->setName(
          QByteArray("ISF_StorageImage_") + e.name.c_str() + suffix);
      if(!t->create())
      {
        qWarning() << "Failed to create storage image" << e.name.c_str() << suffix;
        delete t;
        return nullptr;
      }
      return t;
    };

    if(!e.texture)
      e.texture = make_tex("");
    if(e.persistent && !e.prev)
      e.prev = make_tex("_prev");
  }
}

QVarLengthArray<QRhiShaderResourceBinding, 8> buildExtraBindings(
    const GraphicsStorageResources& store)
{
  QVarLengthArray<QRhiShaderResourceBinding, 8> out;

  for(const auto& e : store.ssbos)
  {
    if(!e.buffer || e.binding < 0)
      continue;

    const auto stages = e.stages;
    if(stages == QRhiShaderResourceBinding::StageFlags{})
      continue;

    if(e.access == "read_only")
    {
      out.append(QRhiShaderResourceBinding::bufferLoad(e.binding, stages, e.buffer));
    }
    else if(e.access == "write_only")
    {
      out.append(QRhiShaderResourceBinding::bufferStore(e.binding, stages, e.buffer));
    }
    else
    {
      out.append(QRhiShaderResourceBinding::bufferLoadStore(e.binding, stages, e.buffer));
    }

    if(e.persistent && e.prev && e.prev_binding >= 0)
    {
      out.append(
          QRhiShaderResourceBinding::bufferLoad(e.prev_binding, stages, e.prev));
    }
  }

  for(const auto& e : store.images)
  {
    if(!e.texture || e.binding < 0)
      continue;
    const auto stages = e.stages;
    if(stages == QRhiShaderResourceBinding::StageFlags{})
      continue;

    if(e.access == "read_only")
      out.append(QRhiShaderResourceBinding::imageLoad(e.binding, stages, e.texture, 0));
    else if(e.access == "write_only")
      out.append(QRhiShaderResourceBinding::imageStore(e.binding, stages, e.texture, 0));
    else
      out.append(QRhiShaderResourceBinding::imageLoadStore(e.binding, stages, e.texture, 0));

    if(e.persistent && e.prev && e.prev_binding >= 0)
    {
      out.append(
          QRhiShaderResourceBinding::imageLoad(e.prev_binding, stages, e.prev, 0));
    }
  }

  for(const auto& e : store.ubos)
  {
    if(!e.buffer || e.binding < 0)
      continue;
    const auto stages = e.stages;
    if(stages == QRhiShaderResourceBinding::StageFlags{})
      continue;
    out.append(QRhiShaderResourceBinding::uniformBuffer(e.binding, stages, e.buffer));
  }

  return out;
}

void bindUpstreamBuffers(
    RenderList& renderer, const std::vector<Port*>& inputPorts,
    GraphicsStorageResources& store,
    QRhiShaderResourceBindings* srb)
{
  // Upstream renderers (halp-based nodes like ExtractBuffer2, RenderedCSFNode,
  // ScenePreprocessorNode aux extractors, ...) publish their output buffer via
  // the virtual NodeRenderer::bufferForOutput() — never by writing
  // Port::value. RenderList::bufferForInput(edge) is the right lookup: it
  // resolves the source node's renderer and calls bufferForOutput on it.
  auto fetchUpstream = [&](Port* port) -> QRhiBuffer* {
    for(Edge* edge : port->edges)
    {
      if(!edge || !edge->source)
        continue;
      if(edge->source->type != Types::Buffer)
        continue;
      if(auto view = renderer.bufferForInput(*edge); view.handle)
        return view.handle;
    }
    return nullptr;
  };
  // For each SSBO that has an input_port_index and is either read-only or an
  // indirect-draw buffer, try to fetch the buffer from the upstream port.
  for(auto& e : store.ssbos)
  {
    if(e.input_port_index < 0)
      continue;
    if(e.input_port_index >= (int)inputPorts.size())
      continue;

    Port* port = inputPorts[e.input_port_index];
    if(!port)
      continue;

    // Only ports of Type::Buffer carry SSBO pointers.
    if(port->type != Types::Buffer)
      continue;

    if(auto* buf = fetchUpstream(port))
    {
      if(!e.owned)
      {
        e.buffer = buf;
      }
      else if(e.access == "read_only")
      {
        if(e.owned && e.buffer)
          e.buffer->deleteLater();
        e.owned = false;
        e.buffer = buf;
      }
    }
  }

  // UBOs: borrow the upstream buffer when one is published on the Buffer port.
  // If the SRB is provided, patch its binding to point at the new buffer so
  // the draw call binds the right descriptor. A per-frame "placeholder" UBO
  // was allocated in ensureStorageResources so the binding slot exists even
  // when no upstream is connected.
  bool ubo_srb_changed = false;
  for(auto& e : store.ubos)
  {
    if(e.input_port_index < 0)
      continue;
    if(e.input_port_index >= (int)inputPorts.size())
      continue;
    Port* port = inputPorts[e.input_port_index];
    if(!port || port->type != Types::Buffer)
      continue;
    QRhiBuffer* found = fetchUpstream(port);
    if(!found || found == e.buffer)
      continue;  // unchanged — nothing to do

    // An upstream is now providing a different buffer than what's currently
    // bound. Drop any placeholder we owned and retarget the binding.
    if(e.owned && e.buffer)
      e.buffer->deleteLater();
    e.owned = false;
    e.buffer = found;

    if(srb && e.binding >= 0)
    {
      replaceBuffer(*srb, e.binding, found);
      ubo_srb_changed = true;
    }
  }
  // No trailing srb->create() — replaceBuffer() now uses the
  // updateResources() fast path, which already rebuilds the backend
  // descriptor set. Re-creating here would tear down the pool slot
  // we just refreshed.
  (void)ubo_srb_changed;
}

void swapPersistentSSBOsState(GraphicsStorageResources& store)
{
  for(auto& e : store.ssbos)
    if(e.persistent && e.buffer && e.prev)
      std::swap(e.buffer, e.prev);
  for(auto& e : store.images)
    if(e.persistent && e.texture && e.prev)
      std::swap(e.texture, e.prev);
}

void reapplyStorageBindings(
    const GraphicsStorageResources& store, QRhiShaderResourceBindings& srb)
{
  for(const auto& e : store.ssbos)
  {
    if(!e.persistent || !e.buffer || !e.prev)
      continue;
    replaceBuffer(srb, e.binding, e.buffer);
    replaceBuffer(srb, e.prev_binding, e.prev);
  }
  for(const auto& e : store.images)
  {
    if(!e.persistent || !e.texture || !e.prev)
      continue;
    replaceTexture(srb, e.binding, e.texture);
    replaceTexture(srb, e.prev_binding, e.prev);
  }
  // No trailing srb.create() — the replace*() helpers use updateResources()
  // which already refreshes the backend descriptor state. A create() here
  // would re-allocate the descriptor set pool slot and defeat the
  // fast-path swap (qrhivulkan.cpp:8707, updateResources).
}

void swapPersistentSSBOs(
    GraphicsStorageResources& store, QRhiShaderResourceBindings& srb)
{
  swapPersistentSSBOsState(store);
  reapplyStorageBindings(store, srb);
}

bool refreshIndirectDrawBuffer(
    RenderList& renderer, const std::vector<Port*>& inputPorts,
    GraphicsStorageResources& store)
{
  if(store.indirectDrawSsboIndex < 0
     || store.indirectDrawSsboIndex >= (int)store.ssbos.size())
    return false;

  auto& e = store.ssbos[store.indirectDrawSsboIndex];
  if(e.input_port_index < 0 || e.input_port_index >= (int)inputPorts.size())
    return false;

  Port* port = inputPorts[e.input_port_index];
  if(!port)
    return false;

  QRhiBuffer* latest = nullptr;
  for(Edge* edge : port->edges)
  {
    if(!edge || !edge->source)
      continue;
    if(auto view = renderer.bufferForInput(*edge); view.handle)
    {
      latest = view.handle;
      break;
    }
  }

  if(latest && latest != store.indirectDrawBuffer)
  {
    store.indirectDrawBuffer = latest;
    e.buffer = latest;
    return true;
  }
  return false;
}

}
