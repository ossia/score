#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/PipelineStateHelpers.hpp>
#include <Gfx/Graph/ShaderCache.hpp>
#include <Gfx/Graph/Utils.hpp>
#include <Gfx/Graph/VertexFallbackDefaults.hpp>
#include <Gfx/Graph/VertexFallbackPool.hpp>

#include <isf.hpp>

#include <score/tools/Debug.hpp>

#include <QDebug>

namespace score::gfx
{
TextureRenderTarget
createRenderTarget(const RenderState& state, QRhiTexture* tex, int samples, bool depth, bool samplableDepth)
{
  TextureRenderTarget ret;
  SCORE_ASSERT(tex);
  ret.texture = tex;

  // The color "tex" is the resolve target — it is always single-sampled.
  //
  // When samplable depth is requested alongside MSAA we need depth resolve:
  // render into a multisample depth attachment, resolve into a single-sample
  // depth texture that downstream shaders can sample. This requires the
  // QRhi::ResolveDepthStencil feature, which is supported on Vulkan 1.2+ and
  // Metal but NOT on D3D11/12. On unsupported backends we degrade the RT to
  // samples=1 — Vulkan/Metal otherwise reject the render pass for mixed
  // sample counts across attachments.
  int effectiveSamples = samples;
  bool useDepthResolve = false;
  if(samplableDepth && samples > 1)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    useDepthResolve = state.rhi->isFeatureSupported(QRhi::ResolveDepthStencil);
#endif
    if(!useDepthResolve)
    {
      qWarning() << "createRenderTarget: samplable depth + samples=" << samples
                 << "but QRhi::ResolveDepthStencil is unsupported on this backend"
                 << "— degrading this RT to samples=1.";
      effectiveSamples = 1;
    }
  }

  QRhiTextureRenderTargetDescription desc;
  if(effectiveSamples == 1)
  {
    QRhiColorAttachment color0(tex);
    desc.setColorAttachments({color0});
  }
  else
  {
    ret.colorRenderBuffer = state.rhi->newRenderBuffer(
        QRhiRenderBuffer::Color, tex->pixelSize(), effectiveSamples, {}, tex->format());
    ret.colorRenderBuffer->setName("createRenderTarget::ret.colorRenderBuffer");
    SCORE_ASSERT(ret.colorRenderBuffer->create());

    QRhiColorAttachment color0(ret.colorRenderBuffer);
    color0.setResolveTexture(tex);
    desc.setColorAttachments({color0});
  }
  if(samplableDepth)
  {
    // The single-sample depth texture is what downstream shaders sample.
    ret.depthTexture = state.rhi->newTexture(
        QRhiTexture::D32F, tex->pixelSize(), 1,
        QRhiTexture::RenderTarget);
    ret.depthTexture->setName("createRenderTarget::depthTexture");
    SCORE_ASSERT(ret.depthTexture->create());

    if(useDepthResolve)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
      // Multisample depth attachment used during rendering; resolves into
      // ret.depthTexture at endPass(). Owned via ret.msDepthTexture so it
      // is released alongside the rest of the RT.
      ret.msDepthTexture = state.rhi->newTexture(
          QRhiTexture::D32F, tex->pixelSize(), effectiveSamples,
          QRhiTexture::RenderTarget);
      ret.msDepthTexture->setName("createRenderTarget::msDepthTexture");
      SCORE_ASSERT(ret.msDepthTexture->create());

      desc.setDepthTexture(ret.msDepthTexture);
      desc.setDepthResolveTexture(ret.depthTexture);
#endif
    }
    else
    {
      desc.setDepthTexture(ret.depthTexture);
    }
  }
  else if(depth)
  {
    // Reverse-Z project rule: intermediate 3D render targets always use
    // D32F float depth. D24 fixed-point combined with reverse-Z yields
    // strictly worse precision than standard-Z would, so renderbuffer
    // depth is no longer an option here. Stencil is dropped (no shader in
    // the codebase currently uses it — revisit via D32FS8 if needed).
    ret.depthTexture = state.rhi->newTexture(
        QRhiTexture::D32F, tex->pixelSize(), effectiveSamples,
        QRhiTexture::RenderTarget);
    ret.depthTexture->setName("createRenderTarget::depthTexture (D32F, non-samplable)");
    SCORE_ASSERT(ret.depthTexture->create());

    desc.setDepthTexture(ret.depthTexture);
  }

  auto renderTarget = state.rhi->newTextureRenderTarget(desc);
  renderTarget->setName("createRenderTarget::renderTarget");
  SCORE_ASSERT(renderTarget);

  auto renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  renderPass->setName("createRenderTarget::renderPass");
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->create());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  return ret;
}

TextureRenderTarget createRenderTarget(
    const RenderState& state, QRhiTexture::Format fmt, QSize sz, int samples, bool depth,
    bool samplableDepth, QRhiTexture::Flags flags)
{
  // FIXME not every RT needs mipmap / generatemips
  auto texture = state.rhi->newTexture(
      fmt, sz, 1,
      QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore | QRhiTexture::MipMapped
          | QRhiTexture::UsedWithGenerateMips | flags);
  texture->setName("createRenderTarget::texture");
  SCORE_ASSERT(texture->create());
  return createRenderTarget(state, texture, samples, depth, samplableDepth);
}

TextureRenderTarget createRenderTarget(
    const RenderState& state,
    std::span<QRhiTexture* const> colorTextures,
    QRhiTexture* depthTex,
    int samples)
{
  TextureRenderTarget ret;
  SCORE_ASSERT(!colorTextures.empty());

  ret.texture = colorTextures[0];
  for(std::size_t i = 1; i < colorTextures.size(); i++)
    ret.additionalColorTextures.push_back(colorTextures[i]);

  // depthTex is the single-sample resolve target supplied by the caller; if
  // MSAA is requested we need depth-resolve support to keep both. Without it
  // (e.g. D3D11/12) all attachments must share a sample count, so degrade
  // this RT to samples=1 — see the matching comment in the non-MRT overload.
  int effectiveSamples = samples;
  bool useDepthResolve = false;
  if(depthTex && samples > 1)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    useDepthResolve = state.rhi->isFeatureSupported(QRhi::ResolveDepthStencil);
#endif
    if(!useDepthResolve)
    {
      qWarning() << "createRenderTarget(MRT): samplable depth + samples=" << samples
                 << "but QRhi::ResolveDepthStencil is unsupported on this backend"
                 << "— degrading this RT to samples=1.";
      effectiveSamples = 1;
    }
  }

  QList<QRhiColorAttachment> attachments;
  for(auto* tex : colorTextures)
  {
    if(effectiveSamples == 1)
    {
      attachments.append(QRhiColorAttachment(tex));
    }
    else
    {
      auto* rb = state.rhi->newRenderBuffer(
          QRhiRenderBuffer::Color, tex->pixelSize(), effectiveSamples, {}, tex->format());
      rb->setName("createRenderTarget::MRT::colorRB");
      SCORE_ASSERT(rb->create());

      QRhiColorAttachment att(rb);
      att.setResolveTexture(tex);
      attachments.append(att);
    }
  }

  QRhiTextureRenderTargetDescription desc;
  desc.setColorAttachments(attachments.begin(), attachments.end());

  if(depthTex)
  {
    ret.depthTexture = depthTex;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    if(useDepthResolve)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
      // Multisample depth attachment used during rendering, resolves into
      // the caller-supplied depthTex on endPass(). We own msDepthTexture.
      ret.msDepthTexture = state.rhi->newTexture(
          QRhiTexture::D32F, depthTex->pixelSize(), effectiveSamples,
          QRhiTexture::RenderTarget);
      ret.msDepthTexture->setName("createRenderTarget::MRT::msDepthTexture");
      SCORE_ASSERT(ret.msDepthTexture->create());

      desc.setDepthTexture(ret.msDepthTexture);
      desc.setDepthResolveTexture(depthTex);
#endif
    }
    else
    {
      desc.setDepthTexture(depthTex);
    }
#else
    // Qt < 6.6 doesn't support sampleable depth textures in render targets;
    // fall back to a depth renderbuffer (depth won't be sampleable)
    ret.depthRenderBuffer = state.rhi->newRenderBuffer(
        QRhiRenderBuffer::DepthStencil, colorTextures[0]->pixelSize(), effectiveSamples, {},
        QRhiTexture::D32F);
    ret.depthRenderBuffer->setName("createRenderTarget::MRT::depthRB_fallback");
    SCORE_ASSERT(ret.depthRenderBuffer->create());
    desc.setDepthStencilBuffer(ret.depthRenderBuffer);
#endif
  }

  auto renderTarget = state.rhi->newTextureRenderTarget(desc);
  renderTarget->setName("createRenderTarget::MRT::renderTarget");
  SCORE_ASSERT(renderTarget);

  auto renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  renderPass->setName("createRenderTarget::MRT::renderPass");
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->create());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  return ret;
}

// NOTE on the reinterpret_cast<QRhiShaderResourceBinding::Data*> below (and in
// replaceSampler / replaceTexture / etc.): QRhiShaderResourceBinding stores its
// payload in a private nested ::Data whose only public accessor is the const
// data() method — there is no public mutator. We rebind buffers/samplers/
// textures in-place by casting the binding to its layout-compatible private
// Data. This relies on QRhiShaderResourceBinding being a thin wrapper whose
// first (and only) data member IS that Data struct; that layout has been stable
// across Qt 6.4..dev, but it is NOT a guaranteed/forward-compatible ABI. If a
// future Qt reorders QRhiShaderResourceBinding's members this will silently
// corrupt bindings — revisit if QRhi ever exposes a public mutating accessor.
void replaceBuffer(
    std::vector<QRhiShaderResourceBinding>& tmp, int binding, QRhiBuffer* newBuffer)
{
  // const auto bufType = newBuffer->resourceType();
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->binding == binding)
    {
      switch(d->type)
      {
        case QRhiShaderResourceBinding::Type::UniformBuffer:
          d->u.ubuf.buf = newBuffer;
          break;
        case QRhiShaderResourceBinding::Type::BufferLoad:
        case QRhiShaderResourceBinding::Type::BufferStore:
        case QRhiShaderResourceBinding::Type::BufferLoadStore:
          d->u.sbuf.buf = newBuffer;
          break;
        default:
          break;
      }
    }
  }
}

void replaceSampler(
    std::vector<QRhiShaderResourceBinding>& tmp, int binding, QRhiSampler* newSampler)
{
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->binding == binding)
    {
      if(d->type == QRhiShaderResourceBinding::Type::SampledTexture)
      {
        d->u.stex.texSamplers[0].sampler = newSampler;
      }
    }
  }
}

void replaceTexture(
    std::vector<QRhiShaderResourceBinding>& tmp, int binding, QRhiTexture* newTexture)
{
  // Defensive null-guard — writing a null texture into a
  // sampledTexture / ImageLoad binding crashes the next
  // vkUpdateDescriptorSets. Callers that genuinely want to "detach" a
  // texture should call replaceTexture with an empty-fallback from the
  // RenderList (renderer.emptyTexture() / …Array() / …Cube() / …3D())
  // that matches the sampler's kind. When this is reached with null,
  // leave the existing binding in place so the pass keeps working
  // with whatever it had last.
  if(!newTexture)
    return;
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->binding == binding)
    {
      switch(d->type)
      {
        case QRhiShaderResourceBinding::Type::SampledTexture:
          d->u.stex.texSamplers[0].tex = newTexture;
          break;
        case QRhiShaderResourceBinding::Type::ImageLoad:
        case QRhiShaderResourceBinding::Type::ImageStore:
        case QRhiShaderResourceBinding::Type::ImageLoadStore:
          d->u.simage.tex = newTexture;
          break;
        default:
          break;
      }
    }
  }
}

// The replace*() overloads on QRhiShaderResourceBindings only ever rewrite
// the *resources* inside an existing layout (buffer/texture/sampler pointer
// in the same binding slot). That is the textbook case for QRhi's
// updateResources() fast path: reuse the native descriptor set layout and
// pool slot, bump the generation, let the backend rewrite only the changed
// descriptors. The previous destroy()+create() pattern instead freed the
// pool slot on every live edit — which is what caused the 64-slot batch
// pool to blow up under heavy graph churn.
//
// See qrhivulkan.cpp:8707 (QVkShaderResourceBindings::updateResources).
// All five backends (Vulkan/D3D11/D3D12/Metal/GL) implement the virtual.
void replaceBuffer(QRhiShaderResourceBindings& srb, int binding, QRhiBuffer* newBuffer)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());

  replaceBuffer(tmp, binding, newBuffer);

  srb.setBindings(tmp.begin(), tmp.end());
  srb.updateResources();
}

void replaceSampler(
    QRhiShaderResourceBindings& srb, int binding, QRhiSampler* newSampler)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());

  replaceSampler(tmp, binding, newSampler);

  srb.setBindings(tmp.begin(), tmp.end());
  srb.updateResources();
}

void replaceTexture(
    QRhiShaderResourceBindings& srb, int binding, QRhiTexture* newTexture)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());

  replaceTexture(tmp, binding, newTexture);

  srb.setBindings(tmp.begin(), tmp.end());
  srb.updateResources();
}

void replaceSampler(
    QRhiShaderResourceBindings& srb, QRhiSampler* oldSampler, QRhiSampler* newSampler)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->type == QRhiShaderResourceBinding::Type::SampledTexture)
    {
      SCORE_ASSERT(d->u.stex.count >= 1);
      if(d->u.stex.texSamplers[0].sampler == oldSampler)
      {
        d->u.stex.texSamplers[0].sampler = newSampler;
      }
    }
  }

  srb.setBindings(tmp.begin(), tmp.end());
  srb.updateResources();
}

void replaceSamplerAndTexture(
    QRhiShaderResourceBindings& srb, QRhiSampler* oldSampler, QRhiSampler* newSampler,
    QRhiTexture* newTexture)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->type == QRhiShaderResourceBinding::Type::SampledTexture)
    {
      SCORE_ASSERT(d->u.stex.count >= 1);
      if(d->u.stex.texSamplers[0].sampler == oldSampler)
      {
        d->u.stex.texSamplers[0].sampler = newSampler;
        d->u.stex.texSamplers[0].tex = newTexture;
      }
    }
  }

  srb.setBindings(tmp.begin(), tmp.end());
  srb.updateResources();
}

void replaceTexture(
    QRhiShaderResourceBindings& srb, QRhiSampler* sampler, QRhiTexture* newTexture)
{
  // Defensive null-guard: see the other replaceTexture overload. Null
  // leaves the current binding intact so subsequent setShaderResources
  // calls don't hit vkUpdateDescriptorSets with VK_NULL_HANDLE.
  if(!newTexture)
    return;
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());
  int matches = 0;
  for(QRhiShaderResourceBinding& b : tmp)
  {
    auto d = reinterpret_cast<QRhiShaderResourceBinding::Data*>(&b);
    if(d->type == QRhiShaderResourceBinding::Type::SampledTexture)
    {
      SCORE_ASSERT(d->u.stex.count >= 1);
      if(d->u.stex.texSamplers[0].sampler == sampler)
      {
        d->u.stex.texSamplers[0].tex = newTexture;
        matches++;
      }
    }
  }
  if(matches == 0)
    return;

  srb.setBindings(tmp.begin(), tmp.end());
  srb.updateResources();
}

void replaceTexture(
    QRhiShaderResourceBindings& srb, QRhiTexture* old_tex, QRhiTexture* new_tex)
{
  QVarLengthArray<QRhiShaderResourceBinding> bindings;
  for(auto it = srb.cbeginBindings(); it != srb.cendBindings(); ++it)
  {
    bindings.push_back(*it);

    auto& binding = bindings.back();

    auto& d = *reinterpret_cast<QRhiShaderResourceBinding::Data*>(&binding);
    if(d.type == QRhiShaderResourceBinding::SampledTexture)
    {
      if(d.u.stex.texSamplers[0].tex == old_tex)
      {
        d.u.stex.texSamplers[0].tex = new_tex;
      }
    }
  }
  srb.setBindings(bindings.begin(), bindings.end());
  srb.updateResources();
}

// Unified geometry-attribute lookup, used by raw raster and CSF alike.
// Matches the request (name + optional semantic key) to an upstream
// ossia::geometry::attribute via a 3-stage cascade:
//
//   stage 1 — resolve `semantic_key` (defaults to `name`) via
//             name_to_semantic. If it maps to a known semantic, look that
//             up on the geometry.
//   stage 2 — fall back to a custom-attribute lookup by `name`.
//   stage 3 — display_name match. Catches the case where the user said
//             { NAME: "position", SEMANTIC: "custom" } but only the real
//             position attribute (semantic=position) exists upstream — we
//             still want to bind to it instead of failing.
const ossia::geometry::attribute* findGeometryAttribute(
    const ossia::geometry& geom, std::string_view name, std::string_view semantic_key)
{
  if(semantic_key.empty())
    semantic_key = name;
  const auto sem = ossia::name_to_semantic(semantic_key);

  const ossia::geometry::attribute* match = nullptr;
  if(sem != ossia::attribute_semantic::custom)
    match = geom.find(sem);
  if(!match)
    match = geom.find(name);
  if(!match)
  {
    for(const auto& a : geom.attributes)
    {
      if(ossia::geometry::display_name(a) == name)
      {
        match = &a;
        break;
      }
    }
  }
  return match;
}

bool remapPipelineVertexInputs(
    QRhiGraphicsPipeline& pip, const QShader& vertexShader,
    const ossia::geometry& geom)
{
  const auto& shader_inputs = vertexShader.description().inputVariables();
  if(shader_inputs.empty())
    return true;

  QVarLengthArray<QRhiVertexInputAttribute> remappedAttrs;

  for(const auto& shader_var : shader_inputs)
  {
    const std::string_view var_name(shader_var.name.constData(), shader_var.name.size());
    // Same lookup CSF uses — the explicit-SEMANTIC override is plumbed
    // separately by callers that have access to the descriptor (see the
    // overload below). Here, only the GLSL var name is available, so the
    // semantic key defaults to it.
    const auto* match = findGeometryAttribute(geom, var_name, var_name);

    if(!match)
      return false;

    // binding/format/offset from GEOMETRY, location from SHADER
    remappedAttrs.append(QRhiVertexInputAttribute(
        match->binding, shader_var.location,
        static_cast<QRhiVertexInputAttribute::Format>(match->format),
        match->byte_offset));
  }

  // Override vertex input layout, keeping the bindings (stride/classification)
  QRhiVertexInputLayout inputLayout;
  const auto& prevLayout = pip.vertexInputLayout();
  inputLayout.setBindings(prevLayout.cbeginBindings(), prevLayout.cendBindings());
  inputLayout.setAttributes(remappedAttrs.begin(), remappedAttrs.end());
  pip.setVertexInputLayout(inputLayout);
  return true;
}

bool remapPipelineVertexInputs(
    QRhiGraphicsPipeline& pip, const QShader& vertexShader,
    const ossia::geometry& geom, const isf::descriptor& desc)
{
  const auto& shader_inputs = vertexShader.description().inputVariables();
  if(shader_inputs.empty())
    return true;

  // Build a fast NAME → SEMANTIC override map from the descriptor's
  // VERTEX_INPUTS so we honour explicit user intent. Anything not in the
  // map falls through to name-as-semantic-key behaviour.
  ossia::small_flat_map<std::string_view, std::string_view, 16> overrides;
  for(const auto& vi : desc.vertex_inputs)
    if(!vi.semantic.empty())
      overrides[vi.name] = vi.semantic;

  QVarLengthArray<QRhiVertexInputAttribute> remappedAttrs;
  for(const auto& shader_var : shader_inputs)
  {
    const std::string_view var_name(shader_var.name.constData(), shader_var.name.size());
    std::string_view sem_key = var_name;
    if(auto it = overrides.find(var_name); it != overrides.end())
      sem_key = it->second;

    const auto* match = findGeometryAttribute(geom, var_name, sem_key);
    if(!match)
      return false;

    remappedAttrs.append(QRhiVertexInputAttribute(
        match->binding, shader_var.location,
        static_cast<QRhiVertexInputAttribute::Format>(match->format),
        match->byte_offset));
  }

  QRhiVertexInputLayout inputLayout;
  const auto& prevLayout = pip.vertexInputLayout();
  inputLayout.setBindings(prevLayout.cbeginBindings(), prevLayout.cendBindings());
  inputLayout.setAttributes(remappedAttrs.begin(), remappedAttrs.end());
  pip.setVertexInputLayout(inputLayout);
  return true;
}

namespace
{

// Convert the parser's attribute_type enumerator to the lowercase GLSL
// type name the VertexFallbackDefaults resolver expects. Only the
// fallback-eligible scalar / vec2 / vec3 / vec4 entries map to a
// non-empty string; everything else (mat*, integer / sampler / image
// types) returns empty, which the caller treats as "REQUIRED:false on
// unsupported type" and fails pipeline-build.
std::string_view declTypeFromAttributeType(isf::attribute_type t) noexcept
{
  switch(t)
  {
    case isf::attribute_type::Float: return "float";
    case isf::attribute_type::Vec2:  return "vec2";
    case isf::attribute_type::Vec3:  return "vec3";
    case isf::attribute_type::Vec4:  return "vec4";
    default: return {};
  }
}

} // namespace

bool remapPipelineVertexInputs(
    QRhiGraphicsPipeline& pip, const QShader& vertexShader,
    const ossia::geometry& geom, const isf::descriptor& desc,
    QRhi& rhi, VertexFallbackPool& pool, QRhiResourceUpdateBatch& batch,
    FallbackBindingPlan& outPlan)
{
  outPlan.clear();

  const auto& shader_inputs = vertexShader.description().inputVariables();
  if(shader_inputs.empty())
    return true;

  // Build a fast NAME → descriptor-entry map so every shader input can
  // cheaply look up its REQUIRED / DEFAULT / SEMANTIC metadata. Shader
  // reflection order is driver-dependent; we don't rely on it matching
  // descriptor declaration order.
  ossia::small_flat_map<std::string_view, const isf::vertex_input*, 16> descByName;
  for(const auto& vi : desc.vertex_inputs)
    descByName[vi.name] = &vi;

  // Start from whatever bindings the pipeline already has (the mesh's
  // per-vertex + per-instance buffers). Fallback slots get appended at
  // the end; their binding_index in the extended vector is the index
  // the draw-path then binds the fallback buffer at.
  QVarLengthArray<QRhiVertexInputBinding> bindings;
  {
    const auto& prev = pip.vertexInputLayout();
    for(auto it = prev.cbeginBindings(); it != prev.cendBindings(); ++it)
      bindings.append(*it);
  }

  QVarLengthArray<QRhiVertexInputAttribute> remappedAttrs;
  for(const auto& shader_var : shader_inputs)
  {
    const std::string_view var_name(
        shader_var.name.constData(), shader_var.name.size());

    // Resolve the semantic key the same way the 3-arg overload does —
    // SEMANTIC field wins when set, else NAME is used.
    std::string_view sem_key = var_name;
    auto descIt = descByName.find(var_name);
    const isf::vertex_input* descEntry
        = (descIt != descByName.end()) ? descIt->second : nullptr;
    if(descEntry && !descEntry->semantic.empty())
      sem_key = descEntry->semantic;

    if(const auto* match = findGeometryAttribute(geom, var_name, sem_key))
    {
      remappedAttrs.append(QRhiVertexInputAttribute(
          match->binding, shader_var.location,
          static_cast<QRhiVertexInputAttribute::Format>(match->format),
          match->byte_offset));
      continue;
    }

    // Miss. Strict mode (no descriptor entry or REQUIRED=true) fails.
    if(!descEntry || descEntry->required)
    {
      qDebug() << "remapPipelineVertexInputs: required VERTEX_INPUT '"
               << QString::fromUtf8(var_name.data(), (int)var_name.size())
               << "' has no matching attribute on upstream geometry";
      return false;
    }

    // Optional path — synthesise a fallback buffer. Two failure modes
    // still reject the pipeline build:
    //   - declared GLSL TYPE is unsupported (mat4 / integer / sampler)
    //   - the semantic has no whitelist neutral AND the shader did not
    //     supply DEFAULT in its JSON header
    const std::string_view decl_type = declTypeFromAttributeType(descEntry->type);
    if(decl_type.empty())
    {
      qDebug() << "remapPipelineVertexInputs: optional VERTEX_INPUT '"
               << QString::fromUtf8(var_name.data(), (int)var_name.size())
               << "' uses a type (mat4 / integer / sampler) that is not"
                  " supported by the v1 fallback path; bind a real"
                  " attribute or declare it REQUIRED: true";
      return false;
    }

    const auto sem = ossia::name_to_semantic(sem_key);
    auto spec = resolveVertexFallback(sem, decl_type, descEntry->default_val);
    if(!spec)
    {
      qDebug() << "remapPipelineVertexInputs: optional VERTEX_INPUT '"
               << QString::fromUtf8(var_name.data(), (int)var_name.size())
               << "' (semantic '"
               << QString::fromUtf8(sem_key.data(), (int)sem_key.size())
               << "') has no whitelist default and no explicit DEFAULT"
                  " was provided in the JSON header";
      return false;
    }

    const auto fallbackEntry = pool.acquire(rhi, batch, *spec);
    if(!fallbackEntry.buffer)
    {
      qDebug() << "remapPipelineVertexInputs: failed to allocate fallback"
                  " buffer for VERTEX_INPUT '"
               << QString::fromUtf8(var_name.data(), (int)var_name.size())
               << "'";
      return false;
    }

    // Append a PerInstance step_rate=1 binding to the layout, pointing
    // at a fresh binding index. Semantically: "one instance's worth of
    // this attribute is packed into a single-element buffer, broadcast
    // to every vertex and every instance of the draw".
    const int new_binding_index = bindings.size();
    bindings.append(QRhiVertexInputBinding(
        fallbackEntry.stride,
        QRhiVertexInputBinding::PerInstance,
        /*stepRate=*/1));

    remappedAttrs.append(QRhiVertexInputAttribute(
        new_binding_index, shader_var.location,
        static_cast<QRhiVertexInputAttribute::Format>(fallbackEntry.format),
        /*offset=*/0));

    outPlan.slots.push_back(
        FallbackBindingPlan::Slot{
            .binding_index = new_binding_index,
            .buffer = fallbackEntry.buffer});
  }

  QRhiVertexInputLayout inputLayout;
  inputLayout.setBindings(bindings.begin(), bindings.end());
  inputLayout.setAttributes(remappedAttrs.begin(), remappedAttrs.end());
  pip.setVertexInputLayout(inputLayout);
  return true;
}

Pipeline buildPipeline(
    const RenderList& renderer, const Mesh& mesh, const QShader& vertexS,
    const QShader& fragmentS, const TextureRenderTarget& rt,
    QRhiShaderResourceBindings* srb)
{
  auto& rhi = *renderer.state.rhi;
  auto ps = rhi.newGraphicsPipeline();
  ps->setName("buildPipeline::ps");
  SCORE_ASSERT(ps);

  QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
  premulAlphaBlend.enable = true;
  premulAlphaBlend.srcColor = QRhiGraphicsPipeline::BlendFactor::SrcAlpha;
  premulAlphaBlend.dstColor = QRhiGraphicsPipeline::BlendFactor::OneMinusSrcAlpha;
  premulAlphaBlend.srcAlpha = QRhiGraphicsPipeline::BlendFactor::SrcAlpha;
  premulAlphaBlend.dstAlpha = QRhiGraphicsPipeline::BlendFactor::OneMinusSrcAlpha;

  // MRT: one blend state per color attachment
  int numColorAttachments = rt.colorAttachmentCount();
  QList<QRhiGraphicsPipeline::TargetBlend> blends;
  for(int i = 0; i < std::max(1, numColorAttachments); i++)
    blends.append(premulAlphaBlend);
  ps->setTargetBlends(blends.begin(), blends.end());

  // Use the render target's actual sample count whenever it can be queried,
  // NOT renderer.samples(). The two can differ when an RT was degraded
  // (e.g. samplable-depth + MSAA without depth-resolve support) — in that
  // case the pipeline must agree with the RT or Vulkan will reject the
  // render pass. When only renderPass is set (e.g. MultiWindowNode passes
  // a placeholder rt that targets a swap chain), sampleCount() returns -1
  // and we have to trust the renderlist value.
  const int rtSamplesQueried = rt.sampleCount();
  const int pipelineSamples = (rtSamplesQueried > 0) ? rtSamplesQueried : renderer.samples();
  if(rtSamplesQueried > 0 && rtSamplesQueried != renderer.samples())
  {
    qWarning() << "buildPipeline: RT sampleCount=" << rtSamplesQueried
               << "differs from renderer.samples()=" << renderer.samples()
               << "— pipeline will use" << pipelineSamples;
  }
  ps->setSampleCount(pipelineSamples);

  mesh.preparePipeline(*ps);

  // Remap vertex inputs by semantic if the mesh provides semantic geometry.
  // This matches shader input variable names to geometry attribute semantics,
  // so that locations are determined by the shader, not by the geometry producer.
  if(auto* geom = mesh.semanticGeometry())
  {
    if(!remapPipelineVertexInputs(*ps, vertexS, *geom))
    {
      qDebug() << "Warning! Shader requires attributes not present in mesh";
      delete ps;
      return {nullptr, srb};
    }
  }

  // FIXME does that check make sense?
  if(!renderer.anyNodeRequiresDepth())
  {
    ps->setDepthTest(false);
    ps->setDepthWrite(false);
  }

  ps->setShaderStages(
      {{QRhiShaderStage::Vertex, vertexS}, {QRhiShaderStage::Fragment, fragmentS}});

  ps->setShaderResourceBindings(srb);

  SCORE_ASSERT(rt.renderPass);
  ps->setRenderPassDescriptor(rt.renderPass);

  if(!ps->create())
  {
    qDebug() << "Warning! Pipeline not created";
    delete ps;
    ps = nullptr;
  }
  return {ps, srb};
}

QRhiShaderResourceBindings* createDefaultBindings(
    const RenderList& renderer, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, std::span<const Sampler> samplers,
    std::span<QRhiShaderResourceBinding> additionalBindings)
{
  auto& rhi = *renderer.state.rhi;
  // Shader resource bindings
  auto srb = rhi.newShaderResourceBindings();
  SCORE_ASSERT(srb);

  QVarLengthArray<QRhiShaderResourceBinding, 8> bindings;

  const auto bindingStages = QRhiShaderResourceBinding::VertexStage
                             | QRhiShaderResourceBinding::FragmentStage;

  {
    const auto rendererBinding = QRhiShaderResourceBinding::uniformBuffer(
        0, bindingStages, &renderer.outputUBO());
    bindings.push_back(rendererBinding);
  }

  if(processUBO)
  {
    const auto standardUniformBinding
        = QRhiShaderResourceBinding::uniformBuffer(1, bindingStages, processUBO);
    bindings.push_back(standardUniformBinding);
  }

  // Bind materials
  if(materialUBO)
  {
    const auto materialBinding
        = QRhiShaderResourceBinding::uniformBuffer(2, bindingStages, materialUBO);
    bindings.push_back(materialBinding);
  }

  // Bind samplers. Null texture sources → substitute with the view-type-matched
  // empty texture carried by `Sampler::fallback` (2D / Array / Cube / 3D).
  // This keeps the SRB valid so the pipeline does not crash during
  // vkUpdateDescriptorSets when an optional shader input has no upstream
  // producer — the pass will simply sample the default fallback and render a
  // neutral value (opaque black / transparent) for that slot. Required inputs
  // that truly need content are the shader author's responsibility; the
  // invariant here is "missing ⇒ render something safe, never crash".
  //
  // If `sampler.fallback` is null, the slot intent is assumed sampler2D
  // (the 99 % case) and we use `RenderList::emptyTexture()`. Call sites
  // that create Samplers for sampler3D / samplerCube / sampler2DArray
  // slots MUST populate `fallback` with the typed empty texture — otherwise
  // Vulkan will still reject the binding with a view-type mismatch when
  // the 2D fallback kicks in.
  int binding = 3;
  for(auto sampler : samplers)
  {
    auto actual_texture = sampler.texture;

    // Multi-pass feedback short: can't sample the RT we're writing to.
    if(actual_texture && actual_texture == rt.texture)
      actual_texture = nullptr;

    if(!actual_texture)
      actual_texture = sampler.fallback ? sampler.fallback
                                        : &renderer.emptyTexture();

    bindings.push_back(
        QRhiShaderResourceBinding::sampledTexture(
            binding,
            QRhiShaderResourceBinding::VertexStage
                | QRhiShaderResourceBinding::FragmentStage,
            actual_texture, sampler.sampler));
    binding++;
  }

  for(auto& other : additionalBindings)
  {
    bindings.push_back(other);
  }

  srb->setBindings(bindings.begin(), bindings.end());
  SCORE_ASSERT(srb->create());
  return srb;
}

Pipeline buildPipeline(
    const RenderList& renderer, const Mesh& mesh, const QShader& vertexS,
    const QShader& fragmentS, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, std::span<const Sampler> samplers,
    std::span<QRhiShaderResourceBinding> additionalBindings)
{
  auto bindings = createDefaultBindings(
      renderer, rt, processUBO, materialUBO, samplers, additionalBindings);
  return buildPipeline(renderer, mesh, vertexS, fragmentS, rt, bindings);
}

Pipeline buildPipelineWithState(
    const RenderList& renderer, const Mesh& mesh, const QShader& vertexS,
    const QShader& fragmentS, const TextureRenderTarget& rt, QRhiBuffer* processUBO,
    QRhiBuffer* materialUBO, std::span<const Sampler> samplers,
    std::span<QRhiShaderResourceBinding> extraBindings,
    const isf::pipeline_state& state,
    int multiViewCount,
    bool useShadingRate)
{
  auto& rhi = *renderer.state.rhi;
  auto srb = createDefaultBindings(
      renderer, rt, processUBO, materialUBO, samplers, extraBindings);

  auto ps = rhi.newGraphicsPipeline();
  ps->setName("buildPipelineWithState::ps");
  SCORE_ASSERT(ps);

  // Plan 09 S6: VRS opt-in. Only applies when the backend supports
  // variable-rate shading (cap set in ScreenNode::populateCaps). The
  // actual shading-rate map or per-draw rate is set on the render
  // target / command buffer; the pipeline just needs the flag.
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  if(useShadingRate && renderer.state.caps.variableRateShading)
  {
    ps->setFlags(ps->flags() | QRhiGraphicsPipeline::UsesShadingRate);
  }
#endif

  const bool depthAvailable
      = (rt.depthTexture != nullptr) || (rt.depthRenderBuffer != nullptr)
        || (rt.msDepthTexture != nullptr);
  const bool wantsDepthByDefault = renderer.anyNodeRequiresDepth();

  // Sample count handling (same as buildPipeline()).
  const int rtSamplesQueried = rt.sampleCount();
  const int pipelineSamples
      = (rtSamplesQueried > 0) ? rtSamplesQueried : renderer.samples();
  ps->setSampleCount(pipelineSamples);

  mesh.preparePipeline(*ps);

  // Seed legacy premul-alpha blend on every color attachment so that shaders
  // which declare a partial PIPELINE_STATE (e.g. only DEPTH_TEST) don't
  // silently lose the historical default blend mode. applyPipelineState
  // overrides per-attachment blends only when the shader sets BLEND.
  {
    QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
    premulAlphaBlend.enable = true;
    premulAlphaBlend.srcColor = QRhiGraphicsPipeline::BlendFactor::SrcAlpha;
    premulAlphaBlend.dstColor = QRhiGraphicsPipeline::BlendFactor::OneMinusSrcAlpha;
    premulAlphaBlend.srcAlpha = QRhiGraphicsPipeline::BlendFactor::SrcAlpha;
    premulAlphaBlend.dstAlpha = QRhiGraphicsPipeline::BlendFactor::OneMinusSrcAlpha;
    const int n = std::max(1, rt.colorAttachmentCount());
    QVarLengthArray<QRhiGraphicsPipeline::TargetBlend, 4> blends;
    blends.reserve(n);
    for(int i = 0; i < n; ++i)
      blends.push_back(premulAlphaBlend);
    ps->setTargetBlends(blends.begin(), blends.end());
  }

  // Apply pipeline_state: depth, cull, front-face, blend (per-attachment),
  // stencil, polygon mode, line width. Only fields explicitly set in `state`
  // override the seeded defaults above + mesh.preparePipeline()'s setup.
  applyPipelineState(
      *ps, state, rt.colorAttachmentCount(), depthAvailable, wantsDepthByDefault);

  // Semantic vertex input remapping (same as buildPipeline()).
  if(auto* geom = mesh.semanticGeometry())
  {
    if(!remapPipelineVertexInputs(*ps, vertexS, *geom))
    {
      qDebug() << "Warning! Shader requires attributes not present in mesh";
      delete ps;
      return {nullptr, srb};
    }
  }

  ps->setShaderStages(
      {{QRhiShaderStage::Vertex, vertexS}, {QRhiShaderStage::Fragment, fragmentS}});
  ps->setShaderResourceBindings(srb);

  SCORE_ASSERT(rt.renderPass);
  ps->setRenderPassDescriptor(rt.renderPass);

  // Multiview: on Vulkan/GL the multiViewCount is picked up from the render
  // pass descriptor's color attachment (see createMultiViewRenderTarget), but
  // D3D12 ViewInstancing and Metal vertex amplification read it from the
  // pipeline itself via QRhiGraphicsPipeline::multiViewCount(). So we must set
  // it explicitly here for those backends to produce correct multiview output.
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  if(multiViewCount > 1 && renderer.state.caps.multiview)
    ps->setMultiViewCount(multiViewCount);
#else
  (void)multiViewCount;
#endif

  if(!ps->create())
  {
    qDebug() << "Warning! Pipeline not created";
    delete ps;
    ps = nullptr;
  }
  return {ps, srb};
}

std::pair<QShader, QShader> makeShaders(const RenderState& v, QString vert, QString frag)
{
  auto [vertexS, vertexError] = ShaderCache::get(v, vert.toUtf8(), QShader::VertexStage);
  if(!vertexError.isEmpty())
  {
    qWarning() << "Vertex shader bake failed:" << vertexError;
    qWarning().noquote() << vert;
  }

  auto [fragmentS, fragmentError]
      = ShaderCache::get(v, frag.toUtf8(), QShader::FragmentStage);
  if(!fragmentError.isEmpty())
  {
    qWarning() << "Fragment shader bake failed:" << fragmentError;
    qWarning().noquote() << frag;
  }

  // QShaderBaker is configured with setPerTargetCompilation(true), so a
  // failure on the only requested target leaves errorMessage() non-empty
  // even when the QShader itself is "valid" via some intermediate variant.
  // Treat any non-empty error as fatal so backend-specific bake failures
  // (e.g. SPIRV-Cross HLSL refusing gl_NumWorkGroups) are not silent.
  if(!vertexError.isEmpty() || !vertexS.isValid())
    throw std::runtime_error("invalid vertex shader");
  if(!fragmentError.isEmpty() || !fragmentS.isValid())
    throw std::runtime_error("invalid fragment shader");

  return {vertexS, fragmentS};
}

// TODO move to ShaderCache
QShader makeCompute(const RenderState& v, QString compute)
{
  auto [computeS, computeError]
      = ShaderCache::get(v, compute.toUtf8(), QShader::ComputeStage);
  if(!computeError.isEmpty())
  {
    qWarning() << "Compute shader bake failed:" << computeError;
    qWarning().noquote() << compute;
  }

  if(!computeError.isEmpty() || !computeS.isValid())
    throw std::runtime_error("invalid compute shader");
  return computeS;
}

void DefaultShaderMaterial::init(
    RenderList& renderer, const std::vector<Port*>& input,
    ossia::small_vector<Sampler, 8>& samplers)
{
  auto& rhi = *renderer.state.rhi;

  // Set up shader inputs
  {
    size = 0;
    for(auto in : input)
    {
      switch(in->type)
      {
        case Types::Empty:
          break;
        case Types::Int:
        case Types::Float:
          size += 4;
          break;
        case Types::Vec2:
          size += 8;
          if(size % 8 != 0)
            size += 4;
          break;
        case Types::Vec3:
          while(size % 16 != 0)
          {
            size += 4;
          }
          size += 12;
          break;
        case Types::Vec4:
          while(size % 16 != 0)
          {
            size += 4;
          }
          size += 16;
          break;
        case Types::Image: {
          SCORE_TODO;
          /*
          auto sampler = rhi.newSampler(
              QRhiSampler::Linear,
              QRhiSampler::Linear,
              QRhiSampler::None,
              QRhiSampler::ClampToEdge,
              QRhiSampler::ClampToEdge);
          sampler->setName("DefaultShaderMaterial::sampler");
          SCORE_ASSERT(sampler->create());

          samplers.push_back(
              {sampler, renderer.textureTargetForInputPort(*in)});
*/
          break;
        }
        case Types::Audio:
          break;
        case Types::Geometry:
          break;
        case Types::Camera:
          size += sizeof(ModelCameraUBO);
          break;
      }
    }

    if(size > 0)
    {
      buffer = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, size);
      buffer->setName("DefaultShaderMaterial::buffer");
      SCORE_ASSERT(buffer->create());
    }
  }
}

QSize resizeTextureSize(QSize sz, int min, int max) noexcept
{
  if(sz.width() >= min && sz.height() >= min && sz.width() <= max && sz.height() <= max)
  {
    return sz;
  }
  else
  {
    // To prevent division by zero
    if(sz.width() < 1)
    {
      sz.rwidth() = 1;
    }
    if(sz.height() < 1)
    {
      sz.rheight() = 1;
    }

    // Rescale to max dimension by maintaining aspect ratio
    if(sz.width() > max && sz.height() > max)
    {
      qreal factor = max / qreal(std::max(sz.width(), sz.height()));
      sz.rwidth() *= factor;
      sz.rheight() *= factor;
    }
    else if(sz.width() > max)
    {
      qreal factor = (qreal)max / sz.width();
      sz.rwidth() *= factor;
      sz.rheight() *= factor;
    }
    else if(sz.height() > max)
    {
      qreal factor = (qreal)max / sz.height();
      sz.rwidth() *= factor;
      sz.rheight() *= factor;
    }

    // In case we rescaled below min
    if(sz.width() < min)
    {
      sz.rwidth() = min;
    }
    if(sz.height() < min)
    {
      sz.rheight() = min;
    }
  }
  return sz;
}

QImage resizeTexture(const QImage& img, int min, int max) noexcept
{
  QSize sz = img.size();
  QSize rescaled = resizeTextureSize(sz, min, max);
  if(rescaled == sz || sz.width() == 0 || sz.height() == 0)
    return img;

  return img.scaled(rescaled, Qt::KeepAspectRatio);
}

QSizeF computeScaleForMeshSizing(ScaleMode mode, QSizeF viewport, QSizeF texture)
{
  if(viewport.isEmpty() || texture.isEmpty())
    return QSizeF{1., 1.};
  switch(mode)
  {
    case score::gfx::ScaleMode::BlackBars: {
      const auto new_tex_size
          = viewport.scaled(texture, Qt::AspectRatioMode::KeepAspectRatioByExpanding);
      return {
          texture.width() / new_tex_size.width(),
          texture.height() / new_tex_size.height()};
    }
    case score::gfx::ScaleMode::Fill: {
      double correct_ratio_w = 2. * texture.width() / viewport.width();
      double correct_ratio_h = 2. * texture.height() / viewport.height();
      if(texture.width() >= viewport.width() && texture.height() >= viewport.height())
      {
        double rw = viewport.width() / texture.width();
        double rh = viewport.height() / texture.height();
        double min = std::max(rw, rh) / 2.;

        return {correct_ratio_w * min, correct_ratio_h * min};
      }
      const auto new_tex_size1
          = viewport.scaled(texture, Qt::AspectRatioMode::KeepAspectRatio);
      return {
          texture.width() / new_tex_size1.width(),
          texture.height() / new_tex_size1.height()};
    }
    case score::gfx::ScaleMode::Original: {
      return {texture.width() / viewport.width(), texture.height() / viewport.height()};
    }
    case score::gfx::ScaleMode::Stretch:
    default:
      return {1., 1.};
  }
}

QSizeF
computeScaleForTexcoordSizing(ScaleMode mode, QSizeF renderSize, QSizeF textureSize)
{
  if(renderSize.isEmpty() || textureSize.isEmpty())
    return QSizeF{1., 1.};
  switch(mode)
  {
    // Fits the viewport at the original texture aspect ratio, with black bars around
    case score::gfx::ScaleMode::BlackBars: {
      const auto textureAspect = textureSize.width() / textureSize.height();
      const auto renderAspect = renderSize.width() / renderSize.height();

      if(textureAspect > renderAspect)
        return {1.0, textureAspect / renderAspect};
      else
        return {renderAspect / textureAspect, 1.0};
    }

    // Fits the viewport by stretching
    case score::gfx::ScaleMode::Stretch:
      return {1., 1.};

    // Fits the viewport by filling, maintaining aspect ratio and cropping if necessary
    case score::gfx::ScaleMode::Fill: {
      const auto textureAspect = textureSize.width() / textureSize.height();
      const auto renderAspect = renderSize.width() / renderSize.height();

      if(textureAspect > renderAspect)
        return {renderAspect / textureAspect, 1.0};
      else
        return {1.0, textureAspect / renderAspect};
    }

    case score::gfx::ScaleMode::Original: {
      return {
          renderSize.width() / textureSize.width(),
          renderSize.height() / textureSize.height()};
    }
    default:
      return {};
  }
}

std::vector<Sampler> initInputSamplers(
    const score::gfx::Node& node, RenderList& renderer, const std::vector<Port*>& ports,
    const isf::descriptor* desc)
{
  std::vector<Sampler> samplers;
  QRhi& rhi = *renderer.state.rhi;

  // Per-port sampler-config lookup. The descriptor's `inputs` list is in
  // 1:1 order with the Port array constructed by ISFNode's visitor, so
  // we can walk it in lockstep and capture each image_input's
  // sampler_config. Used by the GrabsFromSource branch below to honor
  // shader-declared WRAP/FILTER on array / 3D textures (without this,
  // those hardcoded to ClampToEdge — which broke any glTF whose UVs
  // went outside [0,1]).
  std::vector<const isf::sampler_config*> port_sampler_cfg(ports.size(), nullptr);
  if(desc)
  {
    const std::size_t N = std::min(ports.size(), desc->inputs.size());
    for(std::size_t i = 0; i < N; ++i)
    {
      const auto& inp = desc->inputs[i];
      if(auto* im = ossia::get_if<isf::image_input>(&inp.data))
        port_sampler_cfg[i] = &im->sampler;
      else if(auto* cm = ossia::get_if<isf::cubemap_input>(&inp.data))
        port_sampler_cfg[i] = &cm->sampler;
    }
  }

  int cur_port = 0;
  for(Port* in : ports)
  {
    switch(in->type)
    {
      case Types::Image: {
        if((in->flags & Flag::GrabsFromSource) == Flag::GrabsFromSource)
        {
          // GrabsFromSource: the upstream node owns the texture (e.g. cubemap).
          // We don't create a render target — just grab the texture pointer
          // from the source renderer and create a sampler for it.
          QRhiTexture* srcTex = nullptr;

          for(auto* edge : in->edges)
          {
            if(auto* src_node = edge->source->node)
            {
              if(auto src_it = src_node->renderedNodes.find(&renderer);
                 src_it != src_node->renderedNodes.end())
              {
                if(auto* src_renderer = src_it->second)
                {
                  srcTex = src_renderer->textureForOutput(*edge->source);
                  break;
                }
              }
            }
          }

          // Pick a view-type-compatible placeholder when the upstream hasn't
          // produced a texture yet. Binding a 2D view to a sampler3D /
          // samplerCube / sampler2DArray shader input triggers
          // VUID-vkCmdDraw-viewType-07752 at every draw until a real texture
          // flows in (and forever if no edge ever connects).
          QRhiTexture* fallback = nullptr;
          if((in->flags & Flag::Cubemap) == Flag::Cubemap)
            fallback = &renderer.emptyTextureCube();
          else if((in->flags & Flag::ThreeDimensional) == Flag::ThreeDimensional)
            fallback = &renderer.emptyTexture3D();
          else if((in->flags & Flag::TextureArray) == Flag::TextureArray)
            fallback = &renderer.emptyTextureArray();
          else
            fallback = &renderer.emptyTexture();
          if(!srcTex)
            srcTex = fallback;

          // Honour the shader-declared sampler config when present
          // (WRAP / FILTER / MIPMAP_MODE / COMPARE / …). Falls back to
          // the historical Linear+ClampToEdge sampler when the
          // descriptor wasn't passed or the input had no sampler block.
          QRhiSampler* sampler = nullptr;
          if(cur_port < (int)port_sampler_cfg.size() && port_sampler_cfg[cur_port])
          {
            sampler = score::gfx::makeSampler(rhi, *port_sampler_cfg[cur_port]);
          }
          else
          {
            sampler = rhi.newSampler(
                QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::Linear,
                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
            SCORE_ASSERT(sampler->create());
          }
          sampler->setName("initInputSamplers::grabs_sampler");

          samplers.push_back({sampler, srcTex, fallback});
        }
        else
        {
          // Look up the pre-created render target from the RenderList
          auto rt = renderer.renderTargetForInputPort(*in);
          // View-type-matched fallback when the render target has no
          // texture yet (no upstream producer wired). Same reasoning as
          // the GrabsFromSource branch above: binding a sampler2D view
          // into a sampler2DArray / samplerCube / sampler3D shader slot
          // triggers Vulkan validation errors (VUID-…-viewType-07752)
          // every frame and in some drivers crashes outright. Pick the
          // empty texture whose view kind matches the shader's
          // declared sampler type.
          QRhiTexture* fallback = nullptr;
          if((in->flags & Flag::Cubemap) == Flag::Cubemap)
            fallback = &renderer.emptyTextureCube();
          else if((in->flags & Flag::ThreeDimensional) == Flag::ThreeDimensional)
            fallback = &renderer.emptyTexture3D();
          else if((in->flags & Flag::TextureArray) == Flag::TextureArray)
            fallback = &renderer.emptyTextureArray();
          else
            fallback = &renderer.emptyTexture();
          QRhiTexture* texture = rt.texture ? rt.texture : fallback;

          auto spec = node.resolveRenderTargetSpecs(cur_port, renderer);
          auto sampler = rhi.newSampler(
              spec.mag_filter, spec.min_filter, spec.mipmap_mode, spec.address_u,
              spec.address_v, spec.address_w);
          sampler->setName("initInputSamplers::sampler");
          SCORE_ASSERT(sampler->create());

          samplers.push_back({sampler, texture, fallback});

          // If this port has sampleable depth, add depth sampler
          if((in->flags & Flag::SamplableDepth) == Flag::SamplableDepth)
          {
            auto depthSampler = rhi.newSampler(
                QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
                QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
            depthSampler->setName("initInputSamplers::depth_sampler");
            SCORE_ASSERT(depthSampler->create());

            auto* depthTex = rt.depthTexture ? rt.depthTexture : &renderer.emptyTexture();
            samplers.push_back({depthSampler, depthTex, &renderer.emptyTexture()});
          }
        }
        break;
      }

      default:
        break;
    }
    cur_port++;
  }
  return samplers;
}

// ---------------------------------------------------------------------------
// New render-target overloads (depth-only, layered, multiview)
// ---------------------------------------------------------------------------

TextureRenderTarget createDepthOnlyRenderTarget(
    const RenderState& state, QSize sz, int samples, bool samplableDepth,
    QRhiTexture::Format depthFmt)
{
  TextureRenderTarget ret;
  ret.texture = nullptr;
  ret.arrayLayers = 1;

  // Depth resolve for MSAA sampleable depth — matches the main overload.
  int effectiveSamples = samples;
  bool useDepthResolve = false;
  if(samplableDepth && samples > 1)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    useDepthResolve = state.rhi->isFeatureSupported(QRhi::ResolveDepthStencil);
#endif
    if(!useDepthResolve)
    {
      qWarning() << "createDepthOnlyRenderTarget: samplable depth + samples="
                 << samples
                 << "unsupported on this backend; degrading to samples=1.";
      effectiveSamples = 1;
    }
  }

  // Allocate the sampleable depth texture (what downstream shaders sample).
  if(samplableDepth)
  {
    ret.depthTexture = state.rhi->newTexture(
        depthFmt, sz, 1, QRhiTexture::RenderTarget);
    ret.depthTexture->setName("createDepthOnlyRenderTarget::depthTexture");
    SCORE_ASSERT(ret.depthTexture->create());

    if(useDepthResolve)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
      ret.msDepthTexture = state.rhi->newTexture(
          depthFmt, sz, effectiveSamples, QRhiTexture::RenderTarget);
      ret.msDepthTexture->setName("createDepthOnlyRenderTarget::msDepthTexture");
      SCORE_ASSERT(ret.msDepthTexture->create());
#endif
    }
  }
  else
  {
    ret.depthRenderBuffer = state.rhi->newRenderBuffer(
        QRhiRenderBuffer::DepthStencil, sz, effectiveSamples);
    ret.depthRenderBuffer->setName("createDepthOnlyRenderTarget::depthRB");
    SCORE_ASSERT(ret.depthRenderBuffer->create());
  }

  // Some backends (notably GL ES) REQUIRE a color attachment — allocate a
  // 1×1 dummy color texture that never gets written to. The depth-only RT
  // stores it in dummyColorTexture (owned, released with the RT).
  //
  // On desktop Vulkan/Metal/D3D a depth-only RT is usually accepted without
  // a color attachment. We always allocate the dummy for portability —
  // the memory cost (4 bytes) is negligible.
  ret.dummyColorTexture = state.rhi->newTexture(
      QRhiTexture::RGBA8, QSize(1, 1), effectiveSamples, QRhiTexture::RenderTarget);
  ret.dummyColorTexture->setName("createDepthOnlyRenderTarget::dummyColor");
  SCORE_ASSERT(ret.dummyColorTexture->create());

  QRhiTextureRenderTargetDescription desc;
  {
    QRhiColorAttachment color0(ret.dummyColorTexture);
    desc.setColorAttachments({color0});
  }

  if(samplableDepth)
  {
    if(useDepthResolve)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
      desc.setDepthTexture(ret.msDepthTexture);
      desc.setDepthResolveTexture(ret.depthTexture);
#else
      desc.setDepthTexture(ret.depthTexture);
#endif
    }
    else
    {
      desc.setDepthTexture(ret.depthTexture);
    }
  }
  else
  {
    desc.setDepthStencilBuffer(ret.depthRenderBuffer);
  }

  auto* renderTarget = state.rhi->newTextureRenderTarget(desc);
  renderTarget->setName("createDepthOnlyRenderTarget::rt");
  SCORE_ASSERT(renderTarget);

  auto* renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  renderPass->setName("createDepthOnlyRenderTarget::rp");
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->create());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  return ret;
}

TextureRenderTarget createLayeredRenderTarget(
    const RenderState& state, QRhiTexture* colorTextureArray, int renderLayer,
    QRhiTexture* depthTex, int samples)
{
  TextureRenderTarget ret;
  SCORE_ASSERT(colorTextureArray);
  SCORE_ASSERT(renderLayer >= 0);

  ret.texture = colorTextureArray;
  ret.arrayLayers = std::max(colorTextureArray->arraySize(), 1);
  ret.renderLayer = renderLayer;

  QRhiTextureRenderTargetDescription desc;
  {
    QRhiColorAttachment color0(colorTextureArray);
    color0.setLayer(renderLayer);
    desc.setColorAttachments({color0});
  }

  if(depthTex)
  {
    ret.depthTexture = depthTex;
    // For layered rendering with a depth *array* texture, we'd need to set
    // the layer too. We expect a single shared 2D depth texture in most
    // cases, which is fine.
    desc.setDepthTexture(depthTex);
  }

  auto* renderTarget = state.rhi->newTextureRenderTarget(desc);
  renderTarget->setName("createLayeredRenderTarget::rt");
  SCORE_ASSERT(renderTarget);

  auto* renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  renderPass->setName("createLayeredRenderTarget::rp");
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->create());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  (void)samples;
  return ret;
}

TextureRenderTarget createMultiViewRenderTarget(
    const RenderState& state, QRhiTexture* colorTextureArray, int multiViewCount,
    QRhiTexture* depthTextureArray, int samples)
{
  TextureRenderTarget ret;
  SCORE_ASSERT(colorTextureArray);
  SCORE_ASSERT(multiViewCount >= 2);

  ret.texture = colorTextureArray;
  ret.arrayLayers = std::max(colorTextureArray->arraySize(), multiViewCount);
  ret.multiViewCount = multiViewCount;

  QRhiTextureRenderTargetDescription desc;
  {
    QRhiColorAttachment color0(colorTextureArray);
    // Render to layers [0..multiViewCount-1] via gl_ViewIndex.
    color0.setLayer(0);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    color0.setMultiViewCount(multiViewCount);
#endif
    desc.setColorAttachments({color0});
  }

  if(depthTextureArray)
  {
    ret.depthTexture = depthTextureArray;
    desc.setDepthTexture(depthTextureArray);
  }

  auto* renderTarget = state.rhi->newTextureRenderTarget(desc);
  renderTarget->setName("createMultiViewRenderTarget::rt");
  SCORE_ASSERT(renderTarget);

  auto* renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  renderPass->setName("createMultiViewRenderTarget::rp");
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->create());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  (void)samples;
  return ret;
}

TextureRenderTarget createDepthOnlyRenderTarget(
    const RenderState& state, QRhiTexture* externalDepthTexture, int samples,
    bool samplableDepth)
{
  // Like createDepthOnlyRenderTarget(sz, ...) but builds the RT AROUND a
  // caller-supplied depth texture instead of allocating (and the old buggy
  // call site then immediately deleting) an internal one. The supplied
  // texture may be a plain 2D depth texture or a TextureArray (layered /
  // shadow-cascade depth) — in both cases QRhi attaches layer 0 by default
  // for a depth-only pass, which is what we want here.
  //
  // Ownership: `externalDepthTexture` becomes `ret.depthTexture` and is
  // released with the RT (TextureRenderTarget::release()), matching the
  // ownership the previous (broken) code implied.
  TextureRenderTarget ret;
  SCORE_ASSERT(externalDepthTexture);
  ret.texture = nullptr;
  ret.arrayLayers = std::max(externalDepthTexture->arraySize(), 1);

  // Depth resolve for MSAA sampleable depth — matches the sz overload.
  int effectiveSamples = samples;
  bool useDepthResolve = false;
  if(samplableDepth && samples > 1)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    useDepthResolve = state.rhi->isFeatureSupported(QRhi::ResolveDepthStencil);
#endif
    if(!useDepthResolve)
    {
      qWarning() << "createDepthOnlyRenderTarget(external): samplable depth + samples="
                 << samples
                 << "unsupported on this backend; degrading to samples=1.";
      effectiveSamples = 1;
    }
  }

  ret.depthTexture = externalDepthTexture;

  if(useDepthResolve)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    ret.msDepthTexture = state.rhi->newTexture(
        externalDepthTexture->format(), externalDepthTexture->pixelSize(),
        effectiveSamples, QRhiTexture::RenderTarget);
    ret.msDepthTexture->setName(
        "createDepthOnlyRenderTarget(external)::msDepthTexture");
    SCORE_ASSERT(ret.msDepthTexture->create());
#endif
  }

  // Some backends (notably GL ES) REQUIRE a color attachment — same dummy
  // 1×1 color texture as the sz overload.
  ret.dummyColorTexture = state.rhi->newTexture(
      QRhiTexture::RGBA8, QSize(1, 1), effectiveSamples, QRhiTexture::RenderTarget);
  ret.dummyColorTexture->setName(
      "createDepthOnlyRenderTarget(external)::dummyColor");
  SCORE_ASSERT(ret.dummyColorTexture->create());

  QRhiTextureRenderTargetDescription desc;
  {
    QRhiColorAttachment color0(ret.dummyColorTexture);
    desc.setColorAttachments({color0});
  }

  if(useDepthResolve)
  {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    desc.setDepthTexture(ret.msDepthTexture);
    desc.setDepthResolveTexture(ret.depthTexture);
#else
    desc.setDepthTexture(ret.depthTexture);
#endif
  }
  else
  {
    desc.setDepthTexture(ret.depthTexture);
  }

  auto* renderTarget = state.rhi->newTextureRenderTarget(desc);
  renderTarget->setName("createDepthOnlyRenderTarget(external)::rt");
  SCORE_ASSERT(renderTarget);

  auto* renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  renderPass->setName("createDepthOnlyRenderTarget(external)::rp");
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->create());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  return ret;
}

TextureRenderTarget createLayeredRenderTarget(
    const RenderState& state, std::span<QRhiTexture* const> colorTextures,
    int renderLayer, QRhiTexture* depthTex, int samples)
{
  // Multi-attachment (MRT) layered variant: attaches ALL color textures to
  // the render pass so the pipeline blend-state count (driven by
  // rt.colorAttachmentCount()) agrees with the actual attachment count.
  // Attaching only color[0] while the pipeline declares N blend targets is a
  // Vulkan pipeline-create validation error AND silently drops outputs 1..N.
  TextureRenderTarget ret;
  SCORE_ASSERT(!colorTextures.empty());
  SCORE_ASSERT(colorTextures[0]);
  SCORE_ASSERT(renderLayer >= 0);

  ret.texture = colorTextures[0];
  for(std::size_t i = 1; i < colorTextures.size(); i++)
    ret.additionalColorTextures.push_back(colorTextures[i]);
  ret.arrayLayers = std::max(colorTextures[0]->arraySize(), 1);
  ret.renderLayer = renderLayer;

  QList<QRhiColorAttachment> attachments;
  for(auto* tex : colorTextures)
  {
    QRhiColorAttachment att(tex);
    // Layered textures select the rendered layer; plain 2D color textures in
    // a mixed MRT keep their (single) layer 0 and ignore this.
    if(tex->arraySize() > 1)
      att.setLayer(renderLayer);
    attachments.append(att);
  }

  QRhiTextureRenderTargetDescription desc;
  desc.setColorAttachments(attachments.begin(), attachments.end());

  if(depthTex)
  {
    ret.depthTexture = depthTex;
    desc.setDepthTexture(depthTex);
  }

  auto* renderTarget = state.rhi->newTextureRenderTarget(desc);
  renderTarget->setName("createLayeredRenderTarget(MRT)::rt");
  SCORE_ASSERT(renderTarget);

  auto* renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  renderPass->setName("createLayeredRenderTarget(MRT)::rp");
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->create());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  (void)samples;
  return ret;
}

TextureRenderTarget createMultiViewRenderTarget(
    const RenderState& state, std::span<QRhiTexture* const> colorTextures,
    int multiViewCount, QRhiTexture* depthTextureArray, int samples)
{
  // Multi-attachment (MRT) multiview variant: attaches ALL color textures
  // (each a TextureArray with >= multiViewCount layers) with per-attachment
  // setMultiViewCount, so attachments == pipeline blend targets. See the
  // layered overload above for why attaching only color[0] is a bug.
  TextureRenderTarget ret;
  SCORE_ASSERT(!colorTextures.empty());
  SCORE_ASSERT(colorTextures[0]);
  SCORE_ASSERT(multiViewCount >= 2);

  ret.texture = colorTextures[0];
  for(std::size_t i = 1; i < colorTextures.size(); i++)
    ret.additionalColorTextures.push_back(colorTextures[i]);
  ret.arrayLayers = std::max(colorTextures[0]->arraySize(), multiViewCount);
  ret.multiViewCount = multiViewCount;

  QList<QRhiColorAttachment> attachments;
  for(auto* tex : colorTextures)
  {
    QRhiColorAttachment att(tex);
    // Render to layers [0..multiViewCount-1] via gl_ViewIndex.
    att.setLayer(0);
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    att.setMultiViewCount(multiViewCount);
#endif
    attachments.append(att);
  }

  QRhiTextureRenderTargetDescription desc;
  desc.setColorAttachments(attachments.begin(), attachments.end());

  if(depthTextureArray)
  {
    ret.depthTexture = depthTextureArray;
    desc.setDepthTexture(depthTextureArray);
  }

  auto* renderTarget = state.rhi->newTextureRenderTarget(desc);
  renderTarget->setName("createMultiViewRenderTarget(MRT)::rt");
  SCORE_ASSERT(renderTarget);

  auto* renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  renderPass->setName("createMultiViewRenderTarget(MRT)::rp");
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->create());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  (void)samples;
  return ret;
}

QRhiTexture::Format parseOutputFormat(
    const std::string& fmt, QRhiTexture::Format fallback) noexcept
{
  std::string f = fmt;
  for(auto& c : f)
    c = (char)std::tolower((unsigned char)c);
  if(f == "rgba8")   return QRhiTexture::RGBA8;
  if(f == "bgra8")   return QRhiTexture::BGRA8;
  if(f == "r8")      return QRhiTexture::R8;
  if(f == "rg8")     return QRhiTexture::RG8;
  if(f == "r16")     return QRhiTexture::R16;
  if(f == "rg16")    return QRhiTexture::RG16;
  if(f == "r16f")    return QRhiTexture::R16F;
  if(f == "r32f")    return QRhiTexture::R32F;
  if(f == "rgba16f") return QRhiTexture::RGBA16F;
  if(f == "rgba32f") return QRhiTexture::RGBA32F;
  if(f == "d16")     return QRhiTexture::D16;
  if(f == "d24")     return QRhiTexture::D24;
  if(f == "d24s8")   return QRhiTexture::D24S8;
  if(f == "d32f")    return QRhiTexture::D32F;
  return fallback;
}

// ---------------- makeSampler -----------------------------------------------
namespace
{
static QRhiSampler::Filter parseFilter(const std::string& s, QRhiSampler::Filter def)
{
  if(s.empty()) return def;
  std::string v = s;
  for(auto& c : v) c = (char)tolower(c);
  if(v == "nearest") return QRhiSampler::Nearest;
  if(v == "linear")  return QRhiSampler::Linear;
  if(v == "none")    return QRhiSampler::None;
  return def;
}
static QRhiSampler::AddressMode parseAddress(const std::string& s, QRhiSampler::AddressMode def)
{
  if(s.empty()) return def;
  std::string v = s;
  for(auto& c : v) c = (char)tolower(c);
  for(auto& c : v) if(c == '-') c = '_';
  if(v == "repeat")                                return QRhiSampler::Repeat;
  if(v == "clamp" || v == "clamp_to_edge")         return QRhiSampler::ClampToEdge;
  if(v == "mirror" || v == "mirrored_repeat")      return QRhiSampler::Mirror;
  //#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  //  if(v == "mirror_once" || v == "mirror_clamp_to_edge")
  //    return QRhiSampler::MirrorOnce;
  //#endif
  return def;
}
static QRhiSampler::CompareOp parseCompare(const std::string& s)
{
  if(s.empty()) return QRhiSampler::Never;
  std::string v = s;
  for(auto& c : v) c = (char)tolower(c);
  for(auto& c : v) if(c == '-') c = '_';
  if(v == "never")                                    return QRhiSampler::Never;
  if(v == "less")                                     return QRhiSampler::Less;
  if(v == "equal")                                    return QRhiSampler::Equal;
  if(v == "less_equal"   || v == "lequal")            return QRhiSampler::LessOrEqual;
  if(v == "greater")                                  return QRhiSampler::Greater;
  if(v == "not_equal"    || v == "neq")               return QRhiSampler::NotEqual;
  if(v == "greater_equal"|| v == "gequal")            return QRhiSampler::GreaterOrEqual;
  if(v == "always")                                   return QRhiSampler::Always;
  return QRhiSampler::Never;
}
}

QRhiSampler* makeSampler(QRhi& rhi, const isf::sampler_config& cfg)
{
  const auto defaultLinear = QRhiSampler::Linear;
  auto base = parseFilter(cfg.filter, defaultLinear);
  auto minF = parseFilter(cfg.min_filter, base);
  auto magF = parseFilter(cfg.mag_filter, base);
  auto mipF = parseFilter(cfg.mipmap_mode, QRhiSampler::None);

  const auto defaultWrap = QRhiSampler::ClampToEdge;
  auto baseWrap = parseAddress(cfg.wrap, defaultWrap);
  auto wrapU = parseAddress(cfg.wrap_s, baseWrap);
  auto wrapV = parseAddress(cfg.wrap_t, baseWrap);
  auto wrapW = parseAddress(cfg.wrap_r, baseWrap);

  auto* s = rhi.newSampler(magF, minF, mipF, wrapU, wrapV, wrapW);
  s->setTextureCompareOp(parseCompare(cfg.compare));
  s->create();
  return s;
}
}
