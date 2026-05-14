#include "IsfBindingsBuilder.hpp"

#include <Gfx/Graph/ISFVisitors.hpp>
#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RhiClearBuffer.hpp>

#include <score/tools/Debug.hpp>

namespace score::gfx
{

// Centralized GLSL type → size table; see header comment for conventions.
int64_t glslTypeSizeBytes(std::string_view type) noexcept
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

int64_t std430ArrayStride(std::string_view type) noexcept
{
  // std430 keeps the vec4-aligned base alignment for vec3 array elements,
  // so the per-element stride is 16 (4 bytes of trailing padding). Every
  // other primitive shrinks to its packed size in std430.
  if(type == "vec3" || type == "ivec3" || type == "uvec3")
    return 16;
  return glslTypeSizeBytes(type);
}

}

namespace score::gfx
{

int64_t std430LayoutSize(
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
    int64_t element = glslTypeSizeBytes(type);
    // std430: elements align to 16 bytes for vec3/mat arrays; keep it simple
    // and align each field to 16 bytes to match the CSF renderer's convention.
    element = (element + 15) & ~15;
    sz += element * count;
  }
  if(sz == 0)
    sz = 16;
  return sz;
}

int64_t glslTypeSizeBytes(std::string_view type, const isf::descriptor& d) noexcept
{
  // Built-in primitives go through the authoritative size table.
  if(type == "float" || type == "int" || type == "uint" || type == "bool")
    return 4;
  if(type == "vec2" || type == "ivec2" || type == "uvec2")
    return 8;
  if(type == "vec3" || type == "ivec3" || type == "uvec3")
    return 12;
  if(type == "vec4" || type == "ivec4" || type == "uvec4")
    return 16;
  if(type == "mat2") return 16;
  if(type == "mat3") return 48;
  if(type == "mat4") return 64;

  // User-defined struct from the descriptor's TYPES section. We sum
  // each field's natural size (no per-field 16-byte padding) so the
  // result matches the actual GLSL std430 size of the emitted struct
  // for scalar/vector-only layouts. This is what producers compare
  // against when binding a struct-typed ATTRIBUTE (the AUXILIARY path
  // uses `std430LayoutSize` instead, which over-pads each field for
  // legacy reasons). For mixed-alignment layouts the producer should
  // populate `element_byte_size` explicitly; the runtime trusts that
  // value over this estimate.
  for(const auto& tdef : d.types)
  {
    if(tdef.name != type)
      continue;
    int64_t sz = 0;
    for(const auto& f : tdef.layout)
    {
      auto fty = f.type;
      int64_t count = 1;
      auto lbr = fty.find('[');
      if(lbr != std::string::npos)
      {
        auto rbr = fty.find(']', lbr + 1);
        if(rbr != std::string::npos && rbr > lbr + 1)
        {
          auto inner = fty.substr(lbr + 1, rbr - lbr - 1);
          if(!inner.empty())
          {
            try { count = std::stoll(inner); } catch(...) { count = 1; }
          }
        }
        fty = fty.substr(0, lbr);
      }
      sz += glslTypeSizeBytes(fty) * count;
    }
    return sz > 0 ? sz : 16;
  }

  // Unknown — match the lenient default of the no-descriptor overload.
  return 16;
}

int64_t std430ArrayStride(std::string_view type, const isf::descriptor& d) noexcept
{
  // Only built-in vec3 needs the std430 padding promotion; user-defined
  // structs already pad their fields at declaration time and their array
  // stride is just the struct's std430 size.
  if(type == "vec3" || type == "ivec3" || type == "uvec3")
    return 16;
  return glslTypeSizeBytes(type, d);
}

}

namespace
{
// Internal alias for the existing AUXILIARY size sites that imported the old
// name from this translation unit; defer to the public helper.
inline int64_t isf_ssbo_elem_size(
    const std::vector<isf::storage_input::layout_field>& layout) noexcept
{
  return score::gfx::std430LayoutSize(layout);
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

  // walk_descriptor_inputs() advances port_idx in lockstep with
  // isf_input_port_vis (ISFNode.cpp / ISFVisitors.hpp). Pre-refactor, this
  // function had its own bookkeeping that did `port_idx++` for every
  // desc.inputs entry — wrong for write-only storage_input (no input port
  // unless flex-array sizing) and for write-only csf_image_input (no
  // input port at all). Now port_idx == cur.inlets, which matches the
  // actual ports created by ISFNode.
  walk_descriptor_inputs(
      desc, [&](const isf::input& inp, const port_counts& cur, const port_counts&) {
        const int port_idx = cur.inlets;
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
            // Only read-only indirect-draw buffers come from an upstream
            // input port; write variants are produced by an output port.
            e.input_port_index = (s->access == "read_only") ? port_idx : -1;
            out.ssbos.push_back(std::move(e));
            out.indirectDrawSsboIndex = (int)out.ssbos.size() - 1;
            out.indirectDrawIndexed = (s->buffer_usage == "indirect_draw_indexed");
            return;
          }
          auto stages = visibilityToStages(s->visibility);
          if(stages == QRhiShaderResourceBinding::StageFlags{})
            return;
          GraphicsSSBO e;
          e.name = inp.name;
          e.access = s->access;
          e.persistent = s->persistent;
          e.owned = true;
          e.size = isf_ssbo_elem_size(s->layout);
          e.layout = s->layout;
          e.stages = stages;
          e.binding = binding++;
          // Only read-only storage_inputs have a matching input port; write
          // variants put the buffer on an OUTPUT port (no upstream rebind).
          e.input_port_index = (s->access == "read_only") ? port_idx : -1;
          if(s->persistent)
            e.prev_binding = binding++;
          out.ssbos.push_back(std::move(e));
        }
        else if(auto* img = ossia::get_if<isf::csf_image_input>(&inp.data))
        {
          auto stages = visibilityToStages(img->visibility);
          if(stages == QRhiShaderResourceBinding::StageFlags{}
             || stages == QRhiShaderResourceBinding::ComputeStage)
            return;
          GraphicsStorageImage e;
          e.name = inp.name;
          e.access = img->access;
          e.format = img->format;
          e.is3D = img->is3D();
          // Cubemap / array shape flags must propagate from the parser to
          // the runtime allocator AND to the GLSL emit; otherwise the
          // descriptor type bound at SRB-create disagrees with the GLSL
          // declaration (parser accepts CUBEMAP / IS_ARRAY at isf.cpp:1411
          // / :1426 but earlier versions of this collector kept only is3D,
          // forcing the allocator into a flat 2D texture and the emit into
          // `image2D`, triggering Vulkan VUID-VkGraphicsPipelineCreateInfo-
          // layout-07990 at pipeline build).
          e.cubemap = img->isCube();
          e.is_array = img->is_array;
          e.persistent = img->persistent;
          if(e.is3D && !img->depth_expression.empty())
          {
            try
            {
              e.depth = std::stoi(img->depth_expression);
            }
            catch(...)
            {
              // Non-literal expression (e.g. "$DEPTH"): leave 0, use default at alloc time
            }
          }
          if(e.is_array && !img->layers_expression.empty())
          {
            try
            {
              e.layers = std::stoi(img->layers_expression);
            }
            catch(...)
            {
              // Non-literal expression (e.g. "$LAYERS"): leave 0; allocator picks default
            }
          }
          e.owned = true;
          e.stages = stages;
          e.binding = binding++;
          // Only read-only csf_image_inputs have a matching input port.
          e.input_port_index = (img->access == "read_only") ? port_idx : -1;
          if(img->persistent)
            e.prev_binding = binding++;
          out.images.push_back(std::move(e));
        }
        else if(auto* uni = ossia::get_if<isf::uniform_input>(&inp.data))
        {
          auto stages = visibilityToStages(uni->visibility);
          if(stages == QRhiShaderResourceBinding::StageFlags{}
             || stages == QRhiShaderResourceBinding::ComputeStage)
            return;
          GraphicsUBO e;
          e.name = inp.name;
          e.owned = false; // sourced from upstream port each frame
          e.stages = stages;
          e.binding = binding++;
          e.input_port_index = port_idx;
          out.ubos.push_back(std::move(e));
        }
      });
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

  // Integer storage image formats — required for atomic image ops
  // (imageAtomicOr / Add / Min / Max / Exchange / CompareExchange).
  // Aliasing an integer SPIR-V OpTypeImage Format operand onto a float
  // QRhiTexture::Format violates VUID-RuntimeSpirv-OpTypeImage-07752
  // and VUID-RuntimeSpirv-OpImageWrite-04469 (numeric-class mismatch
  // between Sampled operand and the bound storage image's format).
  // Mirror RenderedCSFNode.cpp's pattern: gate on Qt 6.10+ (when
  // QRhiTexture exposed R{8,32}{UI,SI} and {RG,RGBA}{32}{UI,SI}).
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  if(f == "r8ui")                   return QRhiTexture::R8UI;
  if(f == "r32ui")                  return QRhiTexture::R32UI;
  if(f == "rg32ui")                 return QRhiTexture::RG32UI;
  if(f == "rgba32ui")               return QRhiTexture::RGBA32UI;
  if(f == "r8si" || f == "r8i")     return QRhiTexture::R8SI;
  if(f == "r32si" || f == "r32i")   return QRhiTexture::R32SI;
  if(f == "rg32si")                 return QRhiTexture::RG32SI;
  if(f == "rgba32si")               return QRhiTexture::RGBA32SI;
#endif
  // r16ui / r16si / r16i are not exposed by QRhiTexture::Format at all,
  // and on older Qt the wider integer formats are also absent. Refuse
  // the binding rather than silently aliasing onto a float/UNORM format
  // — atomics and integer image ops have undefined behaviour otherwise.
  if(f == "r8ui"   || f == "r8si"  || f == "r8i"
     || f == "r16ui" || f == "r16si" || f == "r16i"
     || f == "r32ui" || f == "r32si" || f == "r32i"
     || f == "rg32ui" || f == "rg32si"
     || f == "rgba32ui" || f == "rgba32si")
  {
    qWarning() << "ISF storage image format" << fmt.c_str()
               << "requires Qt 6.10+ integer formats; refusing binding "
                  "(VUID-RuntimeSpirv-OpTypeImage-07752).";
    return QRhiTexture::UnknownFormat;
  }
  return QRhiTexture::RGBA8;
}

// Sentinel zero-buffer used when an upstream SSBO/UBO port disconnects
// mid-session. Vulkan requires every SRB binding to point at a valid
// resource — without a sentinel, a disconnect leaves the binding
// pointing at a deleteLater'd QRhiBuffer (the prior upstream's, freed
// when its owning node was destroyed), and the next setShaderResources
// dereferences the freed pointer.
//
// 64 KiB is generous for any realistic UBO/SSBO layout that a graphics
// shader actually reads from (Vulkan UBO max is at least 16 KiB; SSBOs
// can be larger but disconnect-fallback reads return zeros regardless of
// the buffer's actual size, only its descriptor validity matters). One
// buffer covers both SSBO and UBO disconnects since QRhi accepts both
// usage flags on a single buffer; the descriptor type is set on the
// SRB binding side, not the buffer side.
static constexpr uint32_t kSentinelBufferSize = 64u * 1024u;

// Allocate (and zero-fill) the sentinel disconnect-fallback buffer.
// Called from ensureStorageResources so the resource-update batch is in
// hand. Idempotent — store.sentinelBuffer is non-null after first call.
static void ensureSentinelBuffer(
    QRhi& rhi, QRhiResourceUpdateBatch& res, GraphicsStorageResources& store)
{
  if(store.sentinelBuffer)
    return;
  auto* buf = rhi.newBuffer(
      QRhiBuffer::Static,
      QRhiBuffer::StorageBuffer | QRhiBuffer::UniformBuffer,
      kSentinelBufferSize);
  buf->setName("ISF_SentinelDisconnectBuffer");
  if(!buf->create())
  {
    qWarning() << "Failed to create sentinel disconnect buffer";
    delete buf;
    return;
  }
  // Zero-fill so disconnected SSBO/UBO reads return predictable zeros
  // rather than uninitialised memory.
  static const std::vector<char> zeros(kSentinelBufferSize, 0);
  res.uploadStaticBuffer(buf, 0, kSentinelBufferSize, zeros.data());
  store.sentinelBuffer = buf;
  store.sentinelSize = kSentinelBufferSize;
}

void ensureStorageResources(
    QRhi& rhi, QRhiResourceUpdateBatch& res, const RenderList& renderer,
    const isf::descriptor& /*desc*/, GraphicsStorageResources& store,
    QSize renderSize)
{
  // Sentinel disconnect-fallback buffer: only allocate when the node has
  // at least one upstream-bound SSBO or UBO. ensureSentinelBuffer is
  // idempotent, so subsequent calls (per-frame ensure passes) are
  // no-ops once the sentinel exists. Allocating here (rather than
  // lazily inside bindUpstreamBuffers) lets us fold the zero-fill upload
  // into the same resource-update batch as the rest of the storage
  // initialisation, instead of needing a per-call res in the bind path.
  bool needsSentinel = false;
  for(const auto& s : store.ssbos)
    if(s.input_port_index >= 0) { needsSentinel = true; break; }
  if(!needsSentinel)
    for(const auto& u : store.ubos)
      if(u.input_port_index >= 0) { needsSentinel = true; break; }
  if(needsSentinel)
    ensureSentinelBuffer(rhi, res, store);
  // SSBOs
  for(auto& e : store.ssbos)
  {
    // owned==false: buffer comes from upstream, nothing to allocate here.
    // size derived from layout when persistent; otherwise the user sets
    // it externally (typically matching upstream geometry).
    if(!e.owned)
      continue;
    int64_t target_size = e.size > 0 ? e.size : 16;
    if(!e.buffer)
    {
      e.buffer = allocateSsbo(rhi, e.name, e.buffer_usage, target_size);
      // Zero-fill the placeholder. Vulkan does NOT initialise VkBuffer
      // memory; on a fresh RenderList the new placeholder lands on a
      // device-memory page with whatever the previous owner left there.
      // For shader inputs that have no producer in the user's graph
      // (e.g. cluster_light_counts / cluster_light_lists when no
      // clustered-lighting compute pass is wired) this placeholder IS
      // the buffer the shader reads from — and the read returns
      // device-memory garbage (e.g. a huge cluster_light_count value
      // makes openpbr's light loop iterate thousands of slots, each
      // returning garbage indices into scene_lights → wildly different
      // colours per resize). Mirrors the sentinel-buffer zero-fill at
      // line 432.
      if(e.buffer)
        RhiClearBuffer::clearBuffer(
            rhi, res, e.buffer, 0, (quint32)target_size);
    }
    if(e.persistent && !e.prev)
    {
      e.prev = allocateSsbo(rhi, e.name + "_prev", "", target_size);
      if(e.prev)
        RhiClearBuffer::clearBuffer(
            rhi, res, e.prev, 0, (quint32)target_size);
    }
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
    // Zero-fill the placeholder. Same Vulkan-doesn't-zero-VkBuffers
    // rationale as the SSBO placeholder above. UBOs have a smaller
    // attack surface (256 B) but a single garbage value here can flip
    // a feature bit in scene_counts or fog params, producing the
    // openpbr-only intermittent lighting glitch on resize.
    RhiClearBuffer::clearBuffer(rhi, res, buf, 0, 256u);
    e.buffer = buf;
    e.owned = true;  // we own this placeholder; bindUpstreamBuffers drops ownership when it swaps.
  }

  // Storage images. Allocator must honor every shape flag the parser
  // captured (is3D / cubemap / is_array) so the bound texture matches the
  // GLSL declaration emitted by isf_emit_image_decl. Cube + array combos
  // are rejected at parse time; this code therefore picks one shape via
  // priority order: cubemap > 3D > array > 2D.
  for(auto& e : store.images)
  {
    if(!e.owned)
      continue;

    QSize sz = renderSize.isValid() ? renderSize : QSize(256, 256);
    QRhiTexture::Format fmt = parseImageFormat(e.format);
    if(fmt == QRhiTexture::UnknownFormat)
      continue; // parseImageFormat already warned
    QRhiTexture::Flags flags = QRhiTexture::UsedWithLoadStore;
    if(e.is3D)
      flags |= QRhiTexture::ThreeDimensional;
    if(e.cubemap)
      flags |= QRhiTexture::CubeMap;
    if(e.is_array)
      flags |= QRhiTexture::TextureArray;

    // Cubes use the size-only newTexture overload; QRhi infers face_count=6
    // from the CubeMap flag. width must equal height (cube face is square)
    // — we size both axes to the smaller of renderSize for safety.
    if(e.cubemap)
    {
      const int edge = std::min(sz.width(), sz.height());
      sz = QSize(edge, edge);
    }
    const int arrayLayers = e.layers > 0 ? e.layers : 4; // matches doc default

    auto make_tex = [&](const char* suffix) -> QRhiTexture* {
      QRhiTexture* t = nullptr;
      if(e.cubemap)
        t = rhi.newTexture(fmt, sz, 1, flags);
      else if(e.is3D)
        t = rhi.newTexture(
            fmt, sz.width(), sz.height(),
            e.depth > 0 ? e.depth : 16, 1, flags);
      else if(e.is_array)
        t = rhi.newTextureArray(fmt, arrayLayers, sz, 1, flags);
      else
        t = rhi.newTexture(fmt, sz, 1, flags);
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
      if(buf == e.buffer)
        continue; // unchanged — nothing to do

      if(!e.owned)
      {
        e.buffer = buf;
        if(srb && e.binding >= 0)
          replaceBuffer(*srb, e.binding, buf);
      }
      else if(e.access == "read_only")
      {
        if(e.owned && e.buffer)
          e.buffer->deleteLater();
        e.owned = false;
        e.buffer = buf;
        if(srb && e.binding >= 0)
          replaceBuffer(*srb, e.binding, buf);
      }
    }
    else if(!e.owned && store.sentinelBuffer && !port->edges.empty())
    {
      // Disconnect: we were borrowing an upstream buffer (!e.owned), the
      // user had wired the port (port->edges non-empty), and the upstream
      // is now gone (fetchUpstream returned nullptr). The prior upstream's
      // QRhiBuffer was deleteLater'd when its node tore down, so the SRB
      // binding now points at a dangling pointer. Adopt the sentinel
      // zero-buffer so reads return zeros and the descriptor remains
      // valid (Vulkan validation requires a live resource at every
      // binding slot). Stays !owned — sentinel lifetime is owned by
      // GraphicsStorageResources::release().
      //
      // The port->edges.empty() guard is critical for entries that are
      // bound from the upstream geometry's auxiliary_buffers list (the
      // pattern ScenePreprocessor uses for scene_lights / world_transforms
      // / per_draws / scene_materials / scene_counts / scene_light_indices
      // / camera UBO / env UBO into flattened-scene shaders). Those have
      // input_port_index >= 0 but no port edges — bindUpstreamBuffersFrom-
      // Geometry restores the binding immediately after this function.
      // Without the guard, the sentinel temporarily clobbered them and
      // (worse) flipped their state in a way that confused subsequent
      // frames.
      if(e.buffer != store.sentinelBuffer)
      {
        e.buffer = store.sentinelBuffer;
        if(srb && e.binding >= 0)
          replaceBuffer(*srb, e.binding, store.sentinelBuffer);
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
    if(found == e.buffer)
      continue;  // unchanged — nothing to do

    if(found)
    {
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
    else if(!e.owned && store.sentinelBuffer && !port->edges.empty())
    {
      // Disconnect path mirroring the SSBO loop above: the upstream UBO
      // went away (e.g. its producer node was deleted), and we were
      // borrowing its buffer. Bind the sentinel so the SRB descriptor
      // stays valid; reads return predictable zeros. Note that any
      // owned placeholder allocated in ensureStorageResources is kept
      // — we don't destroy it here, since the next reconnect will adopt
      // the new upstream and we'd just have to re-create the
      // placeholder. The sentinel takeover is transient.
      //
      // The port->edges.empty() guard mirrors the SSBO branch above:
      // entries bound via the geometry name-match path (the camera UBO
      // and env UBO from ScenePreprocessor) have no port edges; the
      // sentinel must not fire for them — bindUpstreamBuffersFrom-
      // Geometry restores them immediately after this function returns.
      if(e.buffer != store.sentinelBuffer)
      {
        e.buffer = store.sentinelBuffer;
        if(srb && e.binding >= 0)
        {
          replaceBuffer(*srb, e.binding, store.sentinelBuffer);
          ubo_srb_changed = true;
        }
      }
    }
  }
  // No trailing srb->create() — replaceBuffer() now uses the
  // updateResources() fast path, which already rebuilds the backend
  // descriptor set. Re-creating here would tear down the pool slot
  // we just refreshed.
  (void)ubo_srb_changed;
}

void bindUpstreamImagesFromGeometry(
    GraphicsStorageResources& store, const ossia::geometry& geometry,
    QRhiShaderResourceBindings* srb)
{
  // Symmetric to bindUpstreamBuffers' read-only SSBO branch, but for
  // storage images. When a downstream csf_image_input is read_only and the
  // upstream geometry publishes a storage image with the same name on its
  // auxiliary_textures list (e.g. an upstream CSF or RawRaster wrote to it
  // via csf_image_input ACCESS:write_only / read_write), swap our
  // texture pointer to the upstream's published handle and free the
  // auto-allocated placeholder.
  //
  // Without this, every read_only csf_image_input INPUTS reads from its
  // OWN zero-initialised texture instead of the upstream's actual contents
  // — silently broken. The downstream typically wants imageLoad on the
  // upstream's writes (e.g. tile-render output sampled by a composite FS
  // via imageLoad rather than texture()).
  for(auto& e : store.images)
  {
    // Only read_only entries can adopt an upstream texture. write_only and
    // read_write own their textures (the CSF / RawRaster IS the producer).
    if(e.access != "read_only")
      continue;
    if(e.binding < 0)
      continue;

    const auto* aux = geometry.find_auxiliary_texture(e.name);
    if(!aux)
      continue; // No upstream publishing this name — keep placeholder.
    auto* upstream_tex = static_cast<QRhiTexture*>(aux->native_handle);
    if(!upstream_tex)
      continue;

    // Swap the underlying texture pointer when it actually changed —
    // first time the upstream connects, or whenever the producer
    // reallocates (resize, format change, …). Drop the auto-allocated
    // placeholder we owned, adopt the upstream handle. Mark non-owned
    // so later release() / persistent swap don't touch the upstream's
    // lifetime.
    if(upstream_tex != e.texture)
    {
      if(e.owned && e.texture)
        e.texture->deleteLater();
      e.owned = false;
      e.texture = upstream_tex;
    }

    // Patch the SRB unconditionally when provided. Lets a multi-pass /
    // multi-SRB caller invoke this helper once per SRB without
    // re-running the upstream lookup (the early-out above guarantees
    // idempotence). Pairs with the m_passes-per-pass loop in
    // RenderedRawRasterPipelineNode::update.
    if(srb)
      replaceTexture(*srb, e.binding, e.texture);
  }
}

void bindUpstreamBuffersFromGeometry(
    QRhi& rhi, QRhiResourceUpdateBatch& res,
    GraphicsStorageResources& store, const ossia::geometry& geometry,
    QRhiShaderResourceBindings* srb)
{
  // SSBO/UBO sibling of bindUpstreamImagesFromGeometry. INPUTS-declared
  // storage_input / uniform_input may carry the upstream buffer either via
  // a dedicated Buffer port edge (handled by bindUpstreamBuffers) OR
  // name-matched against the upstream geometry's auxiliary_buffers list
  // — exactly the pattern ScenePreprocessor uses to publish scene_lights /
  // world_transforms / per_draws / scene_materials / scene_counts /
  // scene_light_indices / camera UBO / env UBO into a flattened scene
  // shader (classic_pbr et al.).
  //
  // Without this name-match path, those bindings stayed at the 16-byte
  // placeholder ensureStorageResources allocates for owned SSBOs:
  // vertices read pd.transform_slot from a zero PerDraw, multiply by a
  // zero world_transforms[0] matrix, collapse to origin → black scene.
  //
  // `geometry` is already a single ossia::geometry (the caller — typically
  // RenderedRawRasterPipelineNode — unwraps from geometry.meshes->meshes[0]
  // at the call site). Same convention as bindUpstreamImagesFromGeometry.
  const auto& mesh = geometry;

  // Look up the GPU/CPU buffer behind a named aux on the geometry.
  // Returns {handle, byte_size, owned?} — owned means we just allocated +
  // uploaded a CPU buffer (caller must release the prior owned handle).
  struct ResolvedBuffer
  {
    QRhiBuffer* handle{};
    int64_t byte_size{0};
    bool owned{false};
  };
  auto resolve_aux = [&](const std::string& name, bool is_uniform) -> ResolvedBuffer {
    auto* geo_aux = mesh.find_auxiliary(name);
    if(!geo_aux || geo_aux->buffer < 0
       || geo_aux->buffer >= (int)mesh.buffers.size())
      return {};
    const auto& geo_buf = mesh.buffers[geo_aux->buffer];
    if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&geo_buf.data))
    {
      if(!gpu->handle)
        return {};
      return {static_cast<QRhiBuffer*>(gpu->handle),
              geo_aux->byte_size > 0 ? geo_aux->byte_size : gpu->byte_size,
              false};
    }
    else if(auto* cpu = ossia::get_if<ossia::geometry::cpu_buffer>(&geo_buf.data))
    {
      if(!cpu->raw_data || cpu->byte_size <= 0)
        return {};
      const int64_t sz
          = geo_aux->byte_size > 0 ? geo_aux->byte_size : cpu->byte_size;
      const auto usage
          = is_uniform ? QRhiBuffer::UniformBuffer : QRhiBuffer::StorageBuffer;
      auto* buf = rhi.newBuffer(QRhiBuffer::Immutable, usage, sz);
      buf->setName(QByteArray("ISF_aux_geom_") + name.c_str());
      if(!buf->create())
      {
        delete buf;
        return {};
      }
      res.uploadStaticBuffer(buf, 0, sz, cpu->raw_data.get());
      return {buf, sz, true};
    }
    return {};
  };

  for(auto& e : store.ssbos)
  {
    if(e.binding < 0)
      continue;
    // Indirect-draw SSBOs carry no shader binding; handled elsewhere.
    if(!e.buffer_usage.empty())
      continue;
    auto resolved = resolve_aux(e.name, /*is_uniform=*/false);
    if(!resolved.handle || resolved.handle == e.buffer)
      continue;
    // Drop the prior owned placeholder (or prior owned CPU upload) before
    // adopting the new handle.
    if(e.owned && e.buffer)
      e.buffer->deleteLater();
    e.buffer = resolved.handle;
    e.size = resolved.byte_size;
    e.owned = resolved.owned;
    if(srb)
      replaceBuffer(*srb, e.binding, e.buffer);
  }

  for(auto& e : store.ubos)
  {
    if(e.binding < 0)
      continue;
    auto resolved = resolve_aux(e.name, /*is_uniform=*/true);
    if(!resolved.handle || resolved.handle == e.buffer)
      continue;
    if(e.owned && e.buffer)
      e.buffer->deleteLater();
    e.buffer = resolved.handle;
    e.owned = resolved.owned;
    if(srb)
      replaceBuffer(*srb, e.binding, e.buffer);
  }
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

}
