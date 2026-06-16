#include <Gfx/Graph/PipelineStateHelpers.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/SimpleRenderedISFNode.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

namespace score::gfx
{

static const constexpr auto blit_vs = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;
layout(location = 0) out vec2 v_texcoord;

layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = renderer.clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
#if defined(QSHADER_HLSL) || defined(QSHADER_MSL)
  gl_Position.y = - gl_Position.y;
#endif
}
)_";

static const constexpr auto blit_fs = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(binding = 3) uniform sampler2D blitTexture;
layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main() { fragColor = texture(blitTexture, v_texcoord); }
)_";

SimpleRenderedISFNode::SimpleRenderedISFNode(const ISFNode& node) noexcept
    : score::gfx::NodeRenderer{node}
    , n{const_cast<ISFNode&>(node)}
{
}

void SimpleRenderedISFNode::updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex)
{
  int sampler_idx = 0;
  for(auto* p : node.input)
  {
    if(p == &input)
      break;
    if(p->type == Types::Image)
    {
      sampler_idx++;
      // Skip the depth sampler that follows ports with SamplableDepth
      if((p->flags & Flag::SamplableDepth) == Flag::SamplableDepth)
        sampler_idx++;
    }
  }

  if(sampler_idx < (int)m_inputSamplers.size())
  {
    auto& sampl = m_inputSamplers[sampler_idx];
    if(sampl.texture != tex)
    {
      sampl.texture = tex;
      for(auto& [e, pass] : m_passes)
        if(pass.p.srb)
          score::gfx::replaceTexture(*pass.p.srb, sampl.sampler, tex);
    }

    // Update the depth sampler if the port has SamplableDepth
    if(depthTex
       && (input.flags & Flag::SamplableDepth) == Flag::SamplableDepth
       && sampler_idx + 1 < (int)m_inputSamplers.size())
    {
      auto& depthSampl = m_inputSamplers[sampler_idx + 1];
      if(depthSampl.texture != depthTex)
      {
        depthSampl.texture = depthTex;
        for(auto& [e, pass] : m_passes)
          if(pass.p.srb)
            score::gfx::replaceTexture(*pass.p.srb, depthSampl.sampler, depthTex);
      }
    }
  }
}

void SimpleRenderedISFNode::updateInputSamplerFilter(
    const Port& input, const RenderTargetSpecs& spec)
{
  int sampler_idx = 0;
  for(auto* p : node.input)
  {
    if(p == &input)
      break;
    if(p->type == Types::Image)
      sampler_idx++;
  }

  if(sampler_idx < (int)m_inputSamplers.size())
  {
    auto* sampler = m_inputSamplers[sampler_idx].sampler;
    if(sampler->magFilter() == spec.mag_filter
       && sampler->minFilter() == spec.min_filter
       && sampler->mipmapMode() == spec.mipmap_mode
       && sampler->addressU() == spec.address_u
       && sampler->addressV() == spec.address_v
       && sampler->addressW() == spec.address_w)
    {
      // See RenderedISFNode::updateInputSamplerFilter — skip the
      // sampler->create() when nothing actually needs updating.
      return;
    }
    sampler->setMagFilter(spec.mag_filter);
    sampler->setMinFilter(spec.min_filter);
    sampler->setMipmapMode(spec.mipmap_mode);
    sampler->setAddressU(spec.address_u);
    sampler->setAddressV(spec.address_v);
    sampler->setAddressW(spec.address_w);
    sampler->create();
  }
}

QRhiTexture* SimpleRenderedISFNode::textureForOutput(const Port& output)
{
  if(!m_hasMRT)
    return nullptr;

  // Find which output port index this is
  const auto& outputs = n.descriptor().outputs;
  for(int i = 0; i < (int)n.output.size() && i < (int)outputs.size(); i++)
  {
    if(n.output[i] == &output)
    {
      if(outputs[i].type == "depth")
        return m_mrtRenderTarget.depthTexture;

      // Color output: index 0 = primary texture, 1+ = additional
      int colorIdx = 0;
      for(int j = 0; j < i; j++)
        if(outputs[j].type != "depth")
          colorIdx++;

      if(colorIdx == 0)
        return m_mrtRenderTarget.texture;
      else if(colorIdx - 1 < (int)m_mrtRenderTarget.additionalColorTextures.size())
        return m_mrtRenderTarget.additionalColorTextures[colorIdx - 1];
    }
  }
  return nullptr;
}

std::vector<Sampler> SimpleRenderedISFNode::allSamplers() const noexcept
{
  // Input ports
  std::vector<Sampler> samplers = m_inputSamplers;

  // Audio textures
  samplers.insert(samplers.end(), m_audioSamplers.begin(), m_audioSamplers.end());

  return samplers;
}

void SimpleRenderedISFNode::initPass(
    const TextureRenderTarget& renderTarget, RenderList& renderer, Edge& edge,
    QRhiResourceUpdateBatch& res)
{
  auto& model_passes = n.descriptor().passes;
  SCORE_ASSERT(model_passes.size() == 1);

  QRhi& rhi = *renderer.state.rhi;

  QRhiBuffer* pubo{};
  pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("SimpleRenderedISFNode::initPass::pubo");
  pubo->create();

  // Allocate storage resources (SSBOs + images) declared in the shader.
  // Reuse the caller's `res` batch rather than allocating a fresh one —
  // the earlier `rhi.nextResourceUpdateBatch()` here was never released
  // or submitted (the "tmp gets merged at next endFrame" comment was
  // wrong: QRhi does NOT auto-reclaim unreleased batches). That leaked
  // one pool slot per addOutputPass call, which exhausts the 64-slot
  // pool after ~60 resize cycles under X11 async resize where each
  // resize tick rebuilds the RenderList (and thus re-inits every ISF
  // renderer's passes) without any intervening frame.
  ensureStorageResources(
      rhi, res, renderer, n.descriptor(), m_storage, renderer.state.renderSize);
  bindUpstreamBuffers(renderer, n.input, m_storage);

  // Build the extra-binding list (storage + multiview UBO).
  auto extraRhiBindings = buildExtraBindings(m_storage);
  if(m_multiViewUBO)
  {
    // Multiview UBO binds right after storage resources.
    int mvBinding = m_firstStorageBinding;
    for(const auto& e : m_storage.ssbos)
    {
      if(e.binding >= 0) mvBinding = std::max(mvBinding, e.binding + 1);
      if(e.prev_binding >= 0) mvBinding = std::max(mvBinding, e.prev_binding + 1);
    }
    for(const auto& e : m_storage.images)
      if(e.binding >= 0) mvBinding = std::max(mvBinding, e.binding + 1);

    extraRhiBindings.append(QRhiShaderResourceBinding::uniformBuffer(
        mvBinding,
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
        m_multiViewUBO));
  }

  // Compute effective pipeline state: global default + per-pass override.
  auto eff_state = mergeState(
      n.descriptor().default_state, model_passes[0].override_state);

  // Create the main pass
  try
  {
    auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);
    auto pip = score::gfx::buildPipelineWithState(
        renderer, *m_mesh, v, s, renderTarget, pubo, m_materialUBO, allSamplers(),
        std::span<QRhiShaderResourceBinding>(
            extraRhiBindings.data(), (std::size_t)extraRhiBindings.size()),
        eff_state,
        n.descriptor().multiview_count);
    if(pip.pipeline)
    {
      m_passes.emplace_back(&edge, Pass{renderTarget, pip, pubo});
    }
    else
    {
      delete pubo;
    }
  }
  catch(...)
  {
    delete pubo;
  }
}

void SimpleRenderedISFNode::initMRTPass(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;
  const auto& outputs = n.descriptor().outputs;
  QSize sz = renderer.state.renderSize;

  // Detect layered / multiview rendering needs.
  int maxLayers = 1;
  for(const auto& out : outputs)
    if(out.layers > maxLayers)
      maxLayers = out.layers;
  const int mvCount = n.descriptor().multiview_count;
  const bool wantMultiview
      = mvCount >= 2 && renderer.state.caps.multiview;
  if(wantMultiview && mvCount > maxLayers)
    maxLayers = mvCount;

  // Per-OUTPUT sample count: MSAA must be uniform across all colour
  // attachments of a render pass, so pick the highest SAMPLES requested by
  // any OUTPUT and use it as the render pass's sample count. Clamped later
  // against QRhi::supportedSampleCounts() in createRenderTarget.
  //
  // IMPORTANT: the textures we allocate below stay SINGLE-SAMPLE — they
  // are the RESOLVE TARGETS. createRenderTarget(mrtSamples) allocates
  // multi-sample colorRenderBuffer attachments internally and wires each
  // of these textures as its resolve destination (Vulkan contract: a
  // resolve target must be single-sample). Downstream shaders sample the
  // already-resolved single-sample textures, so there's no MSAA stride
  // mismatch. (Previous code called setSampleCount(mrtSamples) on these
  // textures, which produced MSAA storage sampled as if it were
  // single-sample — visible as evenly-spaced horizontal stripes
  // proportional to the sample count.)
  int mrtSamples = std::max(renderer.samples(), 1);
  for(const auto& out : outputs)
    mrtSamples = std::max(mrtSamples, out.samples);

  // Create color and depth textures based on OUTPUTS declarations
  std::vector<QRhiTexture*> colorTextures;
  QRhiTexture* depthTex = nullptr;

  for(const auto& out : outputs)
  {
    if(out.type == "depth")
    {
      auto depthFmt = parseOutputFormat(out.format, QRhiTexture::D32F);
      QRhiTexture::Flags dflags = QRhiTexture::RenderTarget;
      if(maxLayers > 1)
      {
        dflags |= QRhiTexture::TextureArray;
        depthTex = rhi.newTextureArray(depthFmt, maxLayers, sz, 1, dflags);
      }
      else
      {
        depthTex = rhi.newTexture(depthFmt, sz, 1, dflags);
      }
      depthTex->setName(("SimpleRenderedISFNode::MRT::depth::" + out.name).c_str());
      SCORE_ASSERT(depthTex->create());
    }
    else
    {
      auto fmt = parseOutputFormat(out.format, QRhiTexture::RGBA8);
      QRhiTexture::Flags flags = QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore;
      const int layers = std::max({1, out.layers, (wantMultiview ? mvCount : 1)});
      QRhiTexture* tex = nullptr;
      if(layers > 1)
      {
        flags |= QRhiTexture::TextureArray;
        tex = rhi.newTextureArray(fmt, layers, sz, 1, flags);
      }
      else
      {
        tex = rhi.newTexture(fmt, sz, 1, flags);
      }
      tex->setName(("SimpleRenderedISFNode::MRT::color::" + out.name).c_str());
      SCORE_ASSERT(tex->create());
      colorTextures.push_back(tex);
    }
  }

  // Depth-only shader: the only output is depth.
  if(colorTextures.empty() && depthTex)
  {
    // Build the RT AROUND the node-owned depth texture (which may be a
    // TextureArray when maxLayers > 1). The previous code asked
    // createDepthOnlyRenderTarget to allocate its own depth texture and then
    // deleted it — but the render pass still referenced it (use-after-free),
    // and textureForOutput() returned a texture that was never rendered to.
    m_mrtRenderTarget = createDepthOnlyRenderTarget(
        renderer.state, depthTex, mrtSamples, /*samplableDepth=*/true);
  }
  else if(wantMultiview && !colorTextures.empty())
  {
    // Attach ALL color textures so attachments == pipeline blend targets.
    m_mrtRenderTarget = createMultiViewRenderTarget(
        renderer.state,
        std::span<QRhiTexture* const>{colorTextures.data(), colorTextures.size()},
        mvCount, depthTex, mrtSamples);
  }
  else if(maxLayers > 1 && !colorTextures.empty())
  {
    // Pick layer 0 by default; per-pass LAYER is handled by the pass loop.
    // Attach ALL color textures so attachments == pipeline blend targets.
    m_mrtRenderTarget = createLayeredRenderTarget(
        renderer.state,
        std::span<QRhiTexture* const>{colorTextures.data(), colorTextures.size()},
        0, depthTex, mrtSamples);
  }
  else if(!colorTextures.empty())
  {
    m_mrtRenderTarget = createRenderTarget(
        renderer.state,
        std::span<QRhiTexture* const>{colorTextures.data(), colorTextures.size()},
        depthTex,
        mrtSamples);
  }
  else
  {
    return;
  }

  // Create the pipeline and pass using this render target
  QRhiBuffer* pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("SimpleRenderedISFNode::initMRTPass::pubo");
  pubo->create();

  // Extra bindings: storage + multiview UBO (same as initPass).
  auto extraRhiBindings = buildExtraBindings(m_storage);
  if(m_multiViewUBO)
  {
    int mvBinding = m_firstStorageBinding;
    for(const auto& e : m_storage.ssbos)
    {
      if(e.binding >= 0) mvBinding = std::max(mvBinding, e.binding + 1);
      if(e.prev_binding >= 0) mvBinding = std::max(mvBinding, e.prev_binding + 1);
    }
    for(const auto& e : m_storage.images)
      if(e.binding >= 0) mvBinding = std::max(mvBinding, e.binding + 1);

    extraRhiBindings.append(QRhiShaderResourceBinding::uniformBuffer(
        mvBinding,
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
        m_multiViewUBO));
  }

  const auto& passes = n.descriptor().passes;
  auto eff_state = mergeState(
      n.descriptor().default_state,
      passes.empty() ? isf::pipeline_state{} : passes[0].override_state);

  try
  {
    auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);
    auto pip = score::gfx::buildPipelineWithState(
        renderer, *m_mesh, v, s, m_mrtRenderTarget, pubo, m_materialUBO, allSamplers(),
        std::span<QRhiShaderResourceBinding>(
            extraRhiBindings.data(), (std::size_t)extraRhiBindings.size()),
        eff_state,
        wantMultiview ? mvCount : 0);
    if(pip.pipeline)
    {
      // Use nullptr edge — MRT passes are shared across all output edges
      m_passes.emplace_back(nullptr, Pass{m_mrtRenderTarget, pip, pubo});
    }
    else
    {
      delete pubo;
    }
  }
  catch(...)
  {
    delete pubo;
  }
}

void SimpleRenderedISFNode::initMRTBlitPass(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge& edge)
{
  QRhiTexture* srcTex = textureForOutput(*edge.source);
  if(!srcTex)
    return;

  auto rt = renderer.renderTargetForOutput(edge);
  if(!rt.renderTarget)
    return;

  auto [vertexS, fragmentS] = score::gfx::makeShaders(renderer.state, blit_vs, blit_fs);

  QRhiSampler* sampler = renderer.state.rhi->newSampler(
      QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
      QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
  sampler->setName("SimpleRenderedISFNode::MRT::blitSampler");
  sampler->create();
  m_blitSamplersByEdge[&edge] = sampler;

  auto pip = score::gfx::buildPipeline(
      renderer, *m_mesh, vertexS, fragmentS, rt, nullptr, nullptr,
      std::array<Sampler, 1>{Sampler{sampler, srcTex}});

  if(pip.pipeline)
  {
    m_passes.emplace_back(&edge, Pass{rt, pip, nullptr});
  }
  else
  {
    m_blitSamplersByEdge.erase(&edge);
    delete sampler;
  }
}

void SimpleRenderedISFNode::initMRTBlitPasses(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  // For each output port, create a blit pass for each downstream edge
  for(auto* output_port : n.output)
  {
    for(Edge* edge : output_port->edges)
    {
      initMRTBlitPass(renderer, res, *edge);
    }
  }
}

void SimpleRenderedISFNode::initState(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  // Create the mesh
  {
    m_mesh = this->n.descriptor().default_vertex_shader ? &renderer.defaultTriangle()
                                                        : &renderer.defaultQuad();

    if(m_meshBuffer.buffers.empty())
    {
      m_meshBuffer = renderer.initMeshBuffer(*m_mesh, res);
      SCORE_ASSERT(!m_meshBuffer.buffers.empty());
    }
  }

  // Create the material UBO and upload initial data
  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    m_materialUBO->setName("SimpleRenderedISFNode::init::m_materialUBO");
    SCORE_ASSERT(m_materialUBO->create());
    if(n.m_material_data)
      res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, n.m_material_data.get());
  }

  // Create the samplers
  SCORE_ASSERT(m_passes.empty());
  SCORE_ASSERT(m_inputSamplers.empty());
  SCORE_ASSERT(m_audioSamplers.empty());

  m_inputSamplers = initInputSamplers(this->n, renderer, n.input, &n.descriptor());

  m_audioSamplers = initAudioTextures(renderer, n.m_audio_textures);

  // Collect graphics-visible storage buffers and images declared in the
  // shader (storage_input with visibility=fragment/vertex/both, or
  // csf_image_input with non-compute visibility). Bindings start right
  // after the sampler bindings.
  {
    const int firstStorageBinding
        = 3 + (int)m_inputSamplers.size() + (int)m_audioSamplers.size();
    m_firstStorageBinding = firstStorageBinding;
    collectGraphicsStorageResources(n.descriptor(), firstStorageBinding, m_storage);
  }

  // Allocate the multiview UBO when MULTIVIEW >= 2 is declared.
  if(n.descriptor().multiview_count >= 2)
  {
    const int mvCount = n.descriptor().multiview_count;
    m_multiViewUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
        sizeof(float[16]) * mvCount);
    m_multiViewUBO->setName("SimpleRenderedISFNode::multiview_ubo");
    SCORE_ASSERT(m_multiViewUBO->create());
  }

  // Count outputs to determine if we need MRT
  {
    const auto& outputs = n.descriptor().outputs;
    int colorCount = 0;
    bool hasDepth = false;
    bool hasLayered = false;
    for(const auto& out : outputs)
    {
      if(out.type == "depth")
        hasDepth = true;
      else
        colorCount++;
      if(out.layers > 1)
        hasLayered = true;
    }
    // MRT is needed for multiple color attachments, depth output, or layered
    // output (TextureArray). Multiview also requires the MRT path.
    m_hasMRT = colorCount > 1 || hasDepth || hasLayered
               || n.descriptor().multiview_count >= 2;
  }

  m_initialized = true;
}

void SimpleRenderedISFNode::addOutputPass(RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  if(m_hasMRT)
  {
    // Create the shared MRT internal render target on first output edge
    if(m_mrtRenderTarget.texture == nullptr)
    {
      initMRTPass(renderer, res);
    }

    // Create the blit pass for this single edge
    initMRTBlitPass(renderer, res, edge);
  }
  else
  {
    auto rt = renderer.renderTargetForOutput(edge);
    if(rt.renderTarget)
    {
      initPass(rt, renderer, edge, res);
    }
  }
}

void SimpleRenderedISFNode::removeOutputPass(RenderList& renderer, Edge& edge)
{
  // Find and erase the pass for this edge
  auto it = ossia::find_if(m_passes, [&](auto& p) { return p.first == &edge; });
  if(it != m_passes.end())
  {
    it->second.p.release();
    if(it->second.processUBO)
      it->second.processUBO->deleteLater();
    m_passes.erase(it);
  }

  if(m_hasMRT)
  {
    // Release the blit sampler for this edge
    auto sit = m_blitSamplersByEdge.find(&edge);
    if(sit != m_blitSamplersByEdge.end())
    {
      delete sit->second;
      m_blitSamplersByEdge.erase(sit);
    }

    // If no more blit passes remain (only the shared MRT pass with nullptr edge),
    // release MRT resources
    bool hasBlitPasses = false;
    for(auto& [e, pass] : m_passes)
    {
      if(e != nullptr)
      {
        hasBlitPasses = true;
        break;
      }
    }
    if(!hasBlitPasses)
    {
      // Remove the shared MRT pass
      auto mrtIt = ossia::find_if(m_passes, [](auto& p) { return p.first == nullptr; });
      if(mrtIt != m_passes.end())
      {
        mrtIt->second.p.release();
        if(mrtIt->second.processUBO)
          mrtIt->second.processUBO->deleteLater();
        m_passes.erase(mrtIt);
      }
      m_mrtRenderTarget.release();
    }
  }
}

bool SimpleRenderedISFNode::hasOutputPassForEdge(Edge& edge) const
{
  return ossia::find_if(m_passes, [&](const auto& p) { return p.first == &edge; })
         != m_passes.end();
}

void SimpleRenderedISFNode::releaseState(RenderList& r)
{
  if(!m_initialized)
    return;

  // Release all remaining passes
  {
    for(auto& texture : n.m_audio_textures)
    {
      auto it = texture.samplers.find(&r);
      if(it != texture.samplers.end())
      {
        if(auto tex = it->second.texture)
        {
          if(tex != &r.emptyTexture())
            tex->deleteLater();
        }
      }
    }

    for(auto& [edge, pass] : m_passes)
    {
      pass.p.release();

      if(pass.processUBO)
      {
        pass.processUBO->deleteLater();
      }
    }

    m_passes.clear();
  }

  for(auto sampler : m_inputSamplers)
  {
    delete sampler.sampler;
    // texture is deleted elsewhere
  }
  m_inputSamplers.clear();
  for(auto sampler : m_audioSamplers)
  {
    delete sampler.sampler;
    // texture is deleted elsewhere
  }
  m_audioSamplers.clear();
  for(auto& [edge, sampler] : m_blitSamplersByEdge)
  {
    delete sampler;
  }
  m_blitSamplersByEdge.clear();

  delete m_materialUBO;
  m_materialUBO = nullptr;

  m_meshBuffer = {}; // Freed in RenderList

  // Release MRT render target (textures are owned by us)
  if(m_hasMRT)
  {
    m_mrtRenderTarget.release();
    m_hasMRT = false;
  }

  // Release storage resources (owned SSBOs + storage images).
  m_storage.release();

  if(m_multiViewUBO)
  {
    m_multiViewUBO->deleteLater();
    m_multiViewUBO = nullptr;
  }

  m_initialized = false;
}

void SimpleRenderedISFNode::addInputEdge(RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  if(edge.sink->type == Types::Image)
  {
    // Find upstream texture
    if(auto it = edge.source->node->renderedNodes.find(&renderer);
       it != edge.source->node->renderedNodes.end())
    {
      if(auto* tex = it->second->textureForOutput(*edge.source))
      {
        auto rt = renderer.renderTargetForInputPort(*edge.sink);
        updateInputTexture(*edge.sink, tex, rt.depthTexture);
      }
    }
  }
}

void SimpleRenderedISFNode::removeInputEdge(RenderList& renderer, Edge& edge)
{
  if(edge.sink->type == Types::Image)
  {
    // Ports declared with DEPTH: true have a second sampler binding for the
    // `_depth` companion. When the cable is removed, the upstream renderer
    // is often released immediately after — so the depth sampler's cached
    // QRhiTexture* becomes a dangling pointer. Pass an empty-texture
    // placeholder for the depth side too so the SRB never holds a freed
    // VkImageView. Without this, vkUpdateDescriptorSets / end-of-frame
    // pipeline barrier both crash on the stale handle.
    const bool hasDepthCompanion
        = (edge.sink->flags & Flag::SamplableDepth) == Flag::SamplableDepth;
    QRhiTexture* depthFallback
        = hasDepthCompanion ? &renderer.emptyTexture() : nullptr;
    updateInputTexture(*edge.sink, &renderer.emptyTexture(), depthFallback);
  }
}

void SimpleRenderedISFNode::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  initState(renderer, res);

  for(auto* out_port : n.output)
    for(auto* edge : out_port->edges)
      addOutputPass(renderer, *edge, res);
}

void SimpleRenderedISFNode::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  m_mrtRenderedThisFrame = false;

  n.standardUBO.passIndex = 0;
  n.standardUBO.frameIndex++;
  auto sz = renderer.renderSize(edge);
  n.standardUBO.renderSize[0] = sz.width();
  n.standardUBO.renderSize[1] = sz.height();

  // Update audio textures
  if(!n.m_audio_textures.empty() && !m_audioTex)
  {
    m_audioTex.emplace();
  }

  bool audioChanged = false;
  std::size_t audio_idx = 0;
  for(auto& audio : n.m_audio_textures)
  {
    if(std::optional<Sampler> sampl
       = m_audioTex->updateAudioTexture(audio, renderer, n.m_material_data.get(), res))
    {
      // Texture changed -> material changed
      audioChanged = true;

      auto& [rhiSampler, tex, fb_] = *sampl;
      // Keep m_audioSamplers[i].texture in sync with the live GPU texture so
      // any later pipeline rebuild (e.g. rt_changed path in RenderList::render
      // triggering removeOutputPass + addOutputPass) uses the live binding
      // instead of the placeholder empty texture.
      if(audio_idx < m_audioSamplers.size())
        m_audioSamplers[audio_idx].texture = tex;

      for(auto& [e, pass] : m_passes)
      {
        score::gfx::replaceTexture(
            *pass.p.srb, rhiSampler, tex ? tex : &renderer.emptyTexture());
      }
    }
    ++audio_idx;
  }

  // Update material
  if(m_materialUBO && m_materialSize > 0 && (materialChanged || audioChanged))
  {
    char* data = n.m_material_data.get();
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
  }
  materialChanged = false;

  // Reset event ports now that the UBO has captured their pulse value.
  // If anything fired, force next frame's upload so the reset-to-zero
  // propagates out through the normally-gated upload path.
  if(n.resetEventPortsAfterFrame())
    materialChanged = true;

  // Re-bind upstream buffers (UBOs / read-only SSBOs sourced from upstream
  // ports). Cables can be added or replaced after init, so this must run
  // every frame. We pass each pass's SRB so that buffer swaps patch the
  // descriptor set in place; without this, uniform_input cables connected
  // post-init never reach the shader and the placeholder UBO stays bound
  // (zero-filled → degenerate matrices on the GPU).
  for(auto& [e, pass] : m_passes)
  {
    bindUpstreamBuffers(renderer, n.input, m_storage, pass.p.srb);
  }

  // Update all the process UBOs
  for(auto& [e, pass] : m_passes)
  {
    if(pass.processUBO)
      res.updateDynamicBuffer(
          pass.processUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
  }
}

void SimpleRenderedISFNode::release(RenderList& r)
{
  releaseState(r);
}

void SimpleRenderedISFNode::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& updateBatch,
    Edge& edge)
{
  if(!m_hasMRT || m_passes.empty())
    return;

  // Only render once per frame even if multiple downstream nodes trigger us
  if(m_mrtRenderedThisFrame)
    return;
  m_mrtRenderedThisFrame = true;

  // MRT: render into our internal multi-attachment render target
  auto& pass = m_passes[0].second;

  SCORE_ASSERT(pass.renderTarget.renderTarget);
  SCORE_ASSERT(pass.p.pipeline);
  SCORE_ASSERT(pass.p.srb);

  cb.beginPass(
      pass.renderTarget.renderTarget, Qt::transparent, {0.0f, 0}, updateBatch);
  updateBatch = nullptr;

  cb.setGraphicsPipeline(pass.p.pipeline);
  cb.setShaderResources(pass.p.srb);

  auto* tex = pass.renderTarget.texture ? pass.renderTarget.texture
              : pass.renderTarget.depthTexture;
  if(tex)
  {
    cb.setViewport(QRhiViewport(
        0, 0, tex->pixelSize().width(), tex->pixelSize().height()));
  }

  drawMeshWithOptionalIndirect(*m_mesh, this->m_meshBuffer, cb);

  cb.endPass();

  // Persistent SSBO ping-pong: swap current and previous for next frame.
  if(pass.p.srb)
    swapPersistentSSBOs(m_storage, *pass.p.srb);
}

void SimpleRenderedISFNode::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  // MRT nodes render to their internal target in runInitialPasses,
  // then blit the appropriate texture here.
  if(m_hasMRT)
  {
    // Find which output port this edge comes from, and get the texture
    QRhiTexture* srcTex = textureForOutput(*edge.source);
    if(!srcTex)
      return;

    // Find the blit pass for this edge
    auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.first == &edge; });
    if(it == this->m_passes.end())
      return;

    auto& pass = it->second;
    SCORE_ASSERT(pass.renderTarget.renderTarget);
    SCORE_ASSERT(pass.p.pipeline);
    SCORE_ASSERT(pass.p.srb);

    // Update the sampler to point to the correct MRT texture
    // (The SRB was created with the first color texture, update if needed)

    cb.setGraphicsPipeline(pass.p.pipeline);
    cb.setShaderResources(pass.p.srb);

    auto* tex = pass.renderTarget.texture;
    cb.setViewport(QRhiViewport(
        0, 0, tex->pixelSize().width(), tex->pixelSize().height()));

    m_mesh->draw(this->m_meshBuffer, cb);
    return;
  }

  auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.first == &edge; });
  // Maybe the shader could not be created
  if(it == this->m_passes.end())
    return;

  auto& pass = it->second;

  // Draw the last pass
  {
    SCORE_ASSERT(pass.renderTarget.renderTarget);
    SCORE_ASSERT(pass.p.pipeline);
    SCORE_ASSERT(pass.p.srb);
    // TODO : combine all the uniforms..

    auto pipeline = pass.p.pipeline;
    auto srb = pass.p.srb;
    auto texture = pass.renderTarget.texture;

    // TODO need to free stuff
    {
      cb.setGraphicsPipeline(pipeline);
      cb.setShaderResources(srb);
      if(texture)
      {
        cb.setViewport(QRhiViewport(
            0, 0, texture->pixelSize().width(), texture->pixelSize().height()));
      }

      drawMeshWithOptionalIndirect(*m_mesh, this->m_meshBuffer, cb);
    }

    // Persistent SSBO ping-pong for next frame.
    swapPersistentSSBOs(m_storage, *srb);
  }
}

SimpleRenderedISFNode::~SimpleRenderedISFNode() { }

}
