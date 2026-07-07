#include <Gfx/Graph/PipelineStateHelpers.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/RenderedISFNode.hpp>
#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/ShaderCache.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>
namespace score::gfx
{

RenderedISFNode::~RenderedISFNode() { }
PassOutput RenderedISFNode::initPassSampler(
    ISFNode& n, const isf::pass& pass, RenderList& renderer, QSize mainTexSize,
    QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  // Volumetric fragment passes: a pass targeting a 3D output (OUTPUTS entry
  // with DEPTH > 1) or carrying a Z expression requires per-slice color
  // attachments / 3D image storage that this node does not wire end-to-end.
  // The ISF parser rejects such shaders up-front (see isf.cpp parse_isf:
  // "fragment-mode ISF with PASSES targeting Z / 3D OUTPUTS"); reaching this
  // point with such a pass means the rejection drifted out of sync.
  if(!pass.z_expression.empty() || [&]{
       for(const auto& out : n.descriptor().outputs)
         if(out.name == pass.target && out.depth > 1) return true;
       return false;
     }())
  {
    qFatal(
        "RenderedISFNode: fragment PASSES with Z / 3D OUTPUTS reached the "
        "renderer; parse-time rejection in isf::parser::parse_isf() should "
        "have prevented this. Target: %s",
        pass.target.c_str());
  }

  // Per-pass FORMAT override takes precedence over the legacy FLOAT flag.
  // Covers the handful of formats useful as intermediate render targets:
  // rgba8 (default), rgba16f (common precision bump), rgba32f, r16f, r32f.
  auto pass_format = [&]() -> QRhiTexture::Format {
    if(pass.format.empty())
      return pass.float_storage ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8;
    std::string f = pass.format;
    for(auto& c : f)
      c = (char)std::tolower((unsigned char)c);
    if(f == "rgba8")    return QRhiTexture::RGBA8;
    if(f == "rgba16f")  return QRhiTexture::RGBA16F;
    if(f == "rgba32f")  return QRhiTexture::RGBA32F;
    if(f == "r8")       return QRhiTexture::R8;
    if(f == "r16f")     return QRhiTexture::R16F;
    if(f == "r32f")     return QRhiTexture::R32F;
    qWarning() << "ISF pass FORMAT" << pass.format.c_str()
               << "not recognised — falling back to RGBA8";
    return QRhiTexture::RGBA8;
  };
  // In all the other cases we create a custom render target
  const auto fmt = pass_format();
  const auto filter = (pass.nearest_filter) ? QRhiSampler::Nearest : QRhiSampler::Linear;
  auto sampler = rhi.newSampler(
      filter, filter, QRhiSampler::None, QRhiSampler::Mirror, QRhiSampler::Mirror);
  sampler->setName("RenderedISFNode::initPassSamplers::sampler");
  sampler->create();

  const QSize texSize = (pass.width_expression.empty() && pass.height_expression.empty())
                            ? mainTexSize
                            : n.computeTextureSize(pass, mainTexSize);

  // Upload a zero clear matching the texture format. Qt can convert, so we
  // pick a plausible source: float32 for floating-point formats, uint8 otherwise.
  const bool is_float_fmt
      = fmt == QRhiTexture::RGBA16F || fmt == QRhiTexture::RGBA32F
        || fmt == QRhiTexture::R16F || fmt == QRhiTexture::R32F;
  QImage clear_texture(
      texSize, is_float_fmt ? QImage::Format_RGBA32FPx4 : QImage::Format_ARGB32);
  clear_texture.fill(0);
  auto tex = rhi.newTexture(fmt, texSize, 1, QRhiTexture::RenderTarget);
  tex->setName("RenderedISFNode::initPassSamplers::tex");
  SCORE_ASSERT(tex->create());
  res.uploadTexture(tex, clear_texture);

  // Persistent texture means that frame N can access the output of this pass at frame N-1,
  // thus we need two textures, two render targets...

  if(pass.persistent)
  {
    auto tex2 = rhi.newTexture(fmt, texSize, 1, QRhiTexture::RenderTarget);
    tex2->setName("RenderedISFNode::initPassSamplers::tex2");
    SCORE_ASSERT(tex2->create());
    res.uploadTexture(tex2, clear_texture);

    return PersistSampler{sampler, tex, tex2};
  }
  else
  {
    return PersistSampler{sampler, tex, tex};
  }
}

std::vector<Sampler> RenderedISFNode::allSamplers(
    ossia::small_vector<PassOutput, 1>& m_passSamplers,
    int mainOrAltPassIndex) const noexcept
{
  SCORE_ASSERT(mainOrAltPassIndex == 0 || mainOrAltPassIndex == 1);
  // Input ports
  std::vector<Sampler> samplers = m_inputSamplers;

  // Audio textures
  samplers.insert(samplers.end(), m_audioSamplers.begin(), m_audioSamplers.end());

  // Pass samplers
  for(auto& pass : m_passSamplers)
  {
    if(auto p = ossia::get_if<PersistSampler>(&pass))
    {
      if(p->sampler)
      {
        samplers.push_back({p->sampler, p->textures[mainOrAltPassIndex]});
      }
    }
  }

  return samplers;
}

RenderedISFNode::RenderedISFNode(const ISFNode& node) noexcept
    : score::gfx::NodeRenderer{node}
    , n{const_cast<ISFNode&>(node)}
{
}

void RenderedISFNode::updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex)
{
  int sampler_idx = 0;
  for(auto* p : node.input)
  {
    if(p == &input)
      break;
    if(p->type == Types::Image)
    {
      sampler_idx++;
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
      for(auto& [e, passes] : m_passes)
      {
        for(auto& pass : passes.passes)
          if(pass.p.srb)
            score::gfx::replaceTexture(*pass.p.srb, sampl.sampler, tex);
        for(auto& pass : passes.altPasses)
          if(pass.p.srb)
            score::gfx::replaceTexture(*pass.p.srb, sampl.sampler, tex);
      }
    }

    if(depthTex
       && (input.flags & Flag::SamplableDepth) == Flag::SamplableDepth
       && sampler_idx + 1 < (int)m_inputSamplers.size())
    {
      auto& depthSampl = m_inputSamplers[sampler_idx + 1];
      if(depthSampl.texture != depthTex)
      {
        depthSampl.texture = depthTex;
        for(auto& [e, passes] : m_passes)
        {
          for(auto& pass : passes.passes)
            if(pass.p.srb)
              score::gfx::replaceTexture(*pass.p.srb, depthSampl.sampler, depthTex);
          for(auto& pass : passes.altPasses)
            if(pass.p.srb)
              score::gfx::replaceTexture(*pass.p.srb, depthSampl.sampler, depthTex);
        }
      }
    }
  }
}

void RenderedISFNode::updateInputSamplerFilter(
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
      // Nothing to update. The surgical rt_changed path calls this
      // whenever renderTargetSpecsChanged fires, but filter/address
      // state is often unchanged (the bump was for size or format).
      // Skip the sampler->create() — it would destroy and re-allocate
      // the backend QRhiSampler for no observable reason.
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

std::pair<Pass, Pass> RenderedISFNode::createFinalPass(
    RenderList& renderer, ossia::small_vector<PassOutput, 1>& m_passSamplers,
    const TextureRenderTarget& renderTarget)
{
  std::pair<Pass, Pass> ret;

  static const constexpr auto vertex_shader = R"_(#version 450
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texcoord;

layout(binding = 3) uniform sampler2D y_tex;
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
}
)_";

  static const constexpr auto fragment_shader = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 renderSize;
} renderer;

layout(binding = 3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main ()
{
  vec2 factor = textureSize(y_tex, 0) / renderer.renderSize;
  vec2 ifactor = renderer.renderSize / textureSize(y_tex, 0);
  fragColor = texture(y_tex, v_texcoord);
}
)_";

  auto [vertexS, vertexError] = score::gfx::ShaderCache::get(
      renderer.state, vertex_shader, QShader::VertexStage);
  SCORE_ASSERT(vertexError.isEmpty());

  auto [fragmentS, fragmentError] = score::gfx::ShaderCache::get(
      renderer.state, fragment_shader, QShader::FragmentStage);
  SCORE_ASSERT(fragmentError.isEmpty());

  SCORE_ASSERT(vertexS.isValid() && fragmentS.isValid());

  {
    SCORE_ASSERT(!m_passSamplers.empty());
    auto last_sampler = ossia::get_if<PersistSampler>(&m_passSamplers.back());
    SCORE_ASSERT(last_sampler);
    SCORE_ASSERT(last_sampler->textures[0]);
    SCORE_ASSERT(last_sampler->textures[1]);

    Sampler samplers1[1] = {Sampler{last_sampler->sampler, last_sampler->textures[1]}};
    auto pip = score::gfx::buildPipeline(
        renderer, renderer.defaultTriangle(), vertexS, fragmentS, renderTarget, nullptr,
        m_materialUBO, samplers1);
    ret.first = Pass{renderTarget, pip, nullptr};
    ret.second = ret.first;

    // Then we have to use the textures the "main" passes are rendering
    Sampler samplers2[1] = {Sampler{last_sampler->sampler, last_sampler->textures[0]}};
    ret.second.p.srb = score::gfx::createDefaultBindings(
        renderer, ret.second.renderTarget, nullptr, m_materialUBO, samplers2);
  }

  return ret;
}

std::pair<Pass, Pass> RenderedISFNode::createPass(
    RenderList& renderer, ossia::small_vector<PassOutput, 1>& passSamplers,
    PassOutput target, const isf::pass& modelPass,
    bool previousPassIsPersistent)
{
  std::pair<Pass, Pass> ret;
  QRhi& rhi = *renderer.state.rhi;

  QRhiBuffer* pubo{};
  pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("RenderedISFNode::createPass::pubo");
  pubo->create();

  // Compute effective pipeline state: global default + per-pass override.
  const auto eff_state
      = mergeState(n.descriptor().default_state, modelPass.override_state);

  // Build the extra-binding list (storage + optional multiview UBO).
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
  const std::span<QRhiShaderResourceBinding> extras{
      extraRhiBindings.data(), (std::size_t)extraRhiBindings.size()};

  // Create the main pass
  {
    // Render target for the pass
    bool createdRt{};
    TextureRenderTarget renderTarget;
    if(auto rt = ossia::get_if<TextureRenderTarget>(&target))
    {
      // Final render target
      renderTarget = *rt;
    }
    else if(auto psampler = ossia::get_if<PersistSampler>(&target))
    {
      // Intermediary pass
      renderTarget = score::gfx::createRenderTarget(
          renderer.state, psampler->textures[0], renderer.samples(), false);
      m_innerPassTargets.push_back(renderTarget);
      renderTarget.texture->setName("RenderedISFNode::createPass::renderTarget.texture");
      renderTarget.renderTarget->setName(
          "RenderedISFNode::createPass::renderTarget.renderTarget");
      createdRt = true;
    }

    try
    {
      auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);
      const auto mainSamplers = allSamplers(passSamplers, 1);
      auto pip = score::gfx::buildPipelineWithState(
          renderer, renderer.defaultTriangle(), v, s, renderTarget, pubo, m_materialUBO,
          mainSamplers,
          extras,
          eff_state,
          n.descriptor().multiview_count);

      ret.first = Pass{renderTarget, pip, pubo};
    }
    catch(...)
    {
      pubo->destroy();
      delete pubo;
      if(createdRt)
      {
        renderTarget.release();
        m_innerPassTargets.pop_back();
      }
      return {};
    }
  }

  // If necessary create the alternative pass
  {
    if([[maybe_unused]] auto rt = ossia::get_if<TextureRenderTarget>(&target))
    {
      // Non-persistent last pass
      // assert (!persistent);
      ret.second = ret.first;

      if(previousPassIsPersistent)
      {
        // Then we have to use the textures the "main" passes are rendering to
        ret.second.p.srb = score::gfx::createDefaultBindings(
            renderer, ret.second.renderTarget, pubo, m_materialUBO,
            allSamplers(passSamplers, 0), extras);
      }
    }
    else if(auto psampler = ossia::get_if<PersistSampler>(&target))
    {
      if(psampler->textures[1] != psampler->textures[0])
      {
        // This pass is a persistent pass, thus we need to alternate our render target
        // as we can't use a texture both as sampler and render target
        ret.second.processUBO = ret.first.processUBO;
        ret.second.p = ret.first.p;
        ret.second.renderTarget = score::gfx::createRenderTarget(
            renderer.state, psampler->textures[1], renderer.samples(), false);
        m_innerPassTargets.push_back(ret.second.renderTarget);
        ret.second.renderTarget.texture->setName(
            "RenderedISFNode::createPass::ret.second.renderTarget.texture");
        ret.second.renderTarget.renderTarget->setName(
            "RenderedISFNode::createPass::ret.second.renderTarget.renderTarget");

        // We necessarily use the main pass rendered-to samplers
        ret.second.p.srb = score::gfx::createDefaultBindings(
            renderer, ret.second.renderTarget, pubo, m_materialUBO,
            allSamplers(passSamplers, 0), extras);
      }
      else
      {
        ret.second = ret.first;
        if(previousPassIsPersistent)
        {
          // Then we have to use the textures the "main" passes are rendering to
          ret.second.p.srb = score::gfx::createDefaultBindings(
              renderer, ret.second.renderTarget, pubo, m_materialUBO,
              allSamplers(passSamplers, 0), extras);
        }
      }
    }
  }
  return ret;
}

void RenderedISFNode::initPasses(
    const TextureRenderTarget& rt, RenderList& renderer, Edge& edge, QSize mainTexSize,
    QRhiResourceUpdateBatch& res)
{
  Passes passes;

  auto& model_passes = n.descriptor().passes;
  SCORE_ASSERT(model_passes.size() > 0);

  for(auto& pass : model_passes)
  {
    const bool last_pass = &pass == &model_passes.back();
    if(last_pass && !pass.persistent)
    {
      // If the last pass is not persistent we render directly
      // on the provided render target
      passes.samplers.push_back(rt);
    }
    else
    {
      auto sampler = initPassSampler(n, pass, renderer, mainTexSize, res);
      passes.samplers.push_back(sampler);
    }
  }

  // Lazily compute the storage-binding offset now that pass-samplers are
  // known. Each PersistSampler entry in passes.samplers consumes one sampler
  // binding in the shader reflection (input_samplers + audio_samplers +
  // pass_samplers). Only do this once per node lifetime — m_firstStorageBinding
  // stays >= 0 on subsequent edges, but ensureStorageResources is idempotent
  // and must run so that any resize reallocates the buffers.
  if(m_firstStorageBinding < 0)
  {
    int passSamplerCount = 0;
    for(auto& s : passes.samplers)
      if(ossia::get_if<PersistSampler>(&s))
        passSamplerCount++;

    const int firstStorageBinding
        = 3 + (int)m_inputSamplers.size() + (int)m_audioSamplers.size()
          + passSamplerCount;
    m_firstStorageBinding = firstStorageBinding;
    collectGraphicsStorageResources(n.descriptor(), firstStorageBinding, m_storage);

    // Allocate the multiview UBO when MULTIVIEW >= 2 is declared.
    if(n.descriptor().multiview_count >= 2)
    {
      QRhi& rhi = *renderer.state.rhi;
      const int mvCount = n.descriptor().multiview_count;
      m_multiViewUBO = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer,
          sizeof(float[16]) * mvCount);
      m_multiViewUBO->setName("RenderedISFNode::multiview_ubo");
      SCORE_ASSERT(m_multiViewUBO->create());
    }
  }

  // Ensure storage buffers/images exist. Safe to call per edge: it's idempotent
  // and resizes to match renderSize. Then borrow any upstream-provided UBOs /
  // read-only SSBOs (no SRB patch here — SRBs don't exist yet).
  ensureStorageResources(
      *renderer.state.rhi, res, renderer, n.descriptor(), m_storage,
      renderer.state.renderSize);
  bindUpstreamBuffers(renderer, n.input, m_storage);

  bool previousPassIsPersistent = false;
  for(std::size_t i = 0; i < passes.samplers.size(); i++)
  {
    auto& pass = passes.samplers[i];
    const auto [p1, p2]
        = createPass(renderer, passes.samplers, pass, model_passes[i],
                     previousPassIsPersistent);
    if(p1.p.pipeline)
    {
      passes.passes.push_back(p1);
      passes.altPasses.push_back(p2);

      previousPassIsPersistent = model_passes[i].persistent;
    }
    else
    {
      int n = passes.passes.size();
      for(int i = 0; i < n; i++)
      {
        auto& pass = passes.passes[i];
        auto& altpass = passes.altPasses[i];

        if(pass.processUBO)
          pass.processUBO->deleteLater();

        if(auto p = ossia::get_if<PersistSampler>(&passes.samplers[i]))
          delete p->sampler;

        // Release altpass SRB only if it's a different object
        if(altpass.p.srb && altpass.p.srb != pass.p.srb)
        {
          altpass.p.srb->deleteLater();
        }
        altpass.p.srb = nullptr;

        // Release the main pass pipeline+srb
        pass.p.release();
      }

      return;
    }
  }

  SCORE_ASSERT(passes.passes.size() == passes.samplers.size());

  if(previousPassIsPersistent)
  {
    // We have to add a last pass that will blit on the output render target
    const auto [p1, p2] = createFinalPass(renderer, passes.samplers, rt);
    passes.samplers.push_back(rt);
    passes.passes.push_back(p1);
    passes.altPasses.push_back(p2);

    SCORE_ASSERT(passes.passes.size() >= 2);
  }

  m_passes.emplace_back(&edge, std::move(passes));
}

void RenderedISFNode::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  initState(renderer, res);

  for(Edge* edge : n.output[0]->edges)
    addOutputPass(renderer, *edge, res);
}

void RenderedISFNode::initState(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  // Create the mesh
  {
    m_mesh = this->n.descriptor().default_vertex_shader ? &renderer.defaultTriangle()
                                                        : &renderer.defaultQuad();
    if(m_meshBuffer.buffers.empty())
    {
      m_meshBuffer = renderer.initMeshBuffer(*m_mesh, res);
    }
  }

  // Create the material UBO
  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    m_materialUBO->setName("RenderedISFNode::init::m_materialUBO");
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

  m_initialized = true;
}

void RenderedISFNode::addOutputPass(
    RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  auto rt = renderer.renderTargetForOutput(edge);
  if(rt.renderTarget)
  {
    initPasses(rt, renderer, edge, renderer.renderSize(&edge), res);
  }
}

void RenderedISFNode::addInputEdge(
    RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res)
{
  if(edge.sink->type == Types::Image)
  {
    // Find upstream texture through the upstream renderer's textureForOutput().
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

void RenderedISFNode::removeInputEdge(RenderList& renderer, Edge& edge)
{
  if(edge.sink && edge.sink->type == Types::Image)
  {
    // Swap image-sampler bindings to empty-texture placeholders so the SRB
    // never holds pointers to the just-released upstream renderer's
    // textures. Mirrors SimpleRenderedISFNode::removeInputEdge — same
    // dangling VkImageView / end-of-frame barrier crash applies to the
    // multi-pass ISF renderer whenever a cable is cut at runtime. Include
    // the depth companion when the port declared DEPTH: true.
    const bool hasDepthCompanion
        = (edge.sink->flags & Flag::SamplableDepth) == Flag::SamplableDepth;
    QRhiTexture* depthFallback
        = hasDepthCompanion ? &renderer.emptyTexture() : nullptr;
    updateInputTexture(*edge.sink, &renderer.emptyTexture(), depthFallback);
  }
}

void RenderedISFNode::removeOutputPass(RenderList& renderer, Edge& edge)
{
  auto it = ossia::find_if(m_passes, [&](auto& p) { return p.first == &edge; });
  if(it != m_passes.end())
  {
    auto& [passes, altPasses, passSamplers] = it->second;

    std::size_t num = passes.size();
    for(std::size_t i = 0; i < num; i++)
    {
      auto& pass = passes[i];
      auto& altpass = altPasses[i];
      auto& sampler = passSamplers[i];

      if(pass.p.srb != altpass.p.srb)
      {
        altpass.p.srb->deleteLater();
      }

      pass.p.release();

      if(pass.processUBO)
        pass.processUBO->deleteLater();

      if(auto p = ossia::get_if<PersistSampler>(&sampler))
      {
        delete p->sampler;
      }
    }

    m_passes.erase(it);
  }
}

bool RenderedISFNode::hasOutputPassForEdge(Edge& edge) const
{
  return ossia::find_if(m_passes, [&](const auto& p) { return p.first == &edge; })
         != m_passes.end();
}

void RenderedISFNode::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  // Pipeline creation may have legitimately failed and cleaned up.
  if(m_passes.empty())
    return;

  // passIndex gets set per-pass in the processUBO update loop below; no
  // need to seed a value here (previous code used m_passes.size() — which
  // is the edge count, not the pass count — and was then overwritten).
  n.standardUBO.frameIndex++;

  // Update audio textures
  bool audioChanged = false;
  std::size_t audio_idx = 0;
  for(auto& audio : n.m_audio_textures)
  {
    if(std::optional<Sampler> sampl
       = m_audioTex.updateAudioTexture(audio, renderer, n.m_material_data.get(), res))
    {
      // Audio texture changed, this means the material needs update
      audioChanged = true;

      auto& [rhiSampler, tex, fb_] = *sampl;
      // Keep m_audioSamplers[i].texture in sync with the live GPU texture so
      // any later pipeline rebuild (rt_changed path in RenderList::render
      // calling removeOutputPass + addOutputPass) uses the live binding
      // instead of the placeholder empty texture.
      if(audio_idx < m_audioSamplers.size())
        m_audioSamplers[audio_idx].texture = tex;

      for(auto& [e, p] : m_passes)
      {
        for(auto& pass : p.passes)
          score::gfx::replaceTexture(
              *pass.p.srb, rhiSampler, tex ? tex : &renderer.emptyTexture());
        for(auto& pass : p.altPasses)
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

  // Re-bind upstream UBOs / read-only SSBOs on every pass's SRB. Cables can
  // be added or replaced after init, so this runs every frame. Both the main
  // and alt chains hold independent descriptor sets referencing the same
  // storage resources; both must be patched. bindUpstreamBuffers is
  // idempotent when the pointer already matches.
  for(auto& [e, p] : m_passes)
  {
    for(auto& pass : p.passes)
      if(pass.p.srb)
        bindUpstreamBuffers(renderer, n.input, m_storage, pass.p.srb);
    for(auto& pass : p.altPasses)
      if(pass.p.srb)
        bindUpstreamBuffers(renderer, n.input, m_storage, pass.p.srb);
  }

  // Update all the process UBOs

  for(auto& [e, p] : m_passes)
  {
    SCORE_ASSERT(p.samplers.size() == p.passes.size());
    for(int i = 0, N = p.passes.size(); i < N; i++)
    {
      auto& pass = p.passes[i];
      auto& passoutput = p.samplers[i];
      if(pass.processUBO)
      {
        n.standardUBO.passIndex = i;
        if(i < N - 1)
        {
          // renderSize is size of the pass output
          auto persist = ossia::get_if<PersistSampler>(&passoutput);
          SCORE_ASSERT(persist);
          SCORE_ASSERT(persist->textures[0]);
          const auto sz = persist->textures[0]->pixelSize();
          n.standardUBO.renderSize[0] = sz.width();
          n.standardUBO.renderSize[1] = sz.height();
        }
        else
        {
          // Last pass, it's a normal render target
          auto rt = ossia::get_if<TextureRenderTarget>(&passoutput);
          SCORE_ASSERT(rt);
          SCORE_ASSERT(rt->texture);
          const auto sz = rt->texture->pixelSize();
          n.standardUBO.renderSize[0] = sz.width();
          n.standardUBO.renderSize[1] = sz.height();
        }

        res.updateDynamicBuffer(
            pass.processUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
      }
    }
  }
}

void RenderedISFNode::release(RenderList& r)
{
  releaseState(r);
}

void RenderedISFNode::releaseState(RenderList& r)
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

    for(auto& [edge, allPasses] : m_passes)
    {
      auto& [passes, altPasses, passSamplers] = allPasses;

      std::size_t num = passes.size();
      for(std::size_t i = 0; i < num; i++)
      {
        auto& pass = passes[i];
        auto& altpass = altPasses[i];
        auto& sampler = passSamplers[i];

        if(pass.p.srb != altpass.p.srb)
        {
          altpass.p.srb->deleteLater();
        }

        pass.p.release();

        if(pass.processUBO)
          pass.processUBO->deleteLater();

        if(auto p = ossia::get_if<PersistSampler>(&sampler))
        {
          delete p->sampler;
        }
      }
    }

    m_passes.clear();
  }

  for(auto rt : m_innerPassTargets)
    rt.release();
  m_innerPassTargets.clear();

  for(auto sampler : m_inputSamplers)
  {
    delete sampler.sampler;
  }
  m_inputSamplers.clear();
  for(auto sampler : m_audioSamplers)
  {
    delete sampler.sampler;
  }
  m_audioSamplers.clear();

  delete m_materialUBO;
  m_materialUBO = nullptr;

  m_meshBuffer = {};

  // Release storage resources (owned SSBOs + storage images).
  m_storage.release();
  m_firstStorageBinding = -1;
  m_lastStorageSwapFrame = -1;

  if(m_multiViewUBO)
  {
    m_multiViewUBO->deleteLater();
    m_multiViewUBO = nullptr;
  }

  m_initialized = false;
}

void RenderedISFNode::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& updateBatch,
    Edge& edge)
{
  // TODO !! shared passes ?
  // If we have multiple output we likely want the same "state" across all outputs
  // but that does not work if we have e.g. particle compute shaders: each output
  // will make the particles move (or we are duplicating all the particle data)

  // Even with a single output if a node renders to two "edges"..

  // Check if we just have one pass (thus nothing to render here).
  if(this->m_passes[0].second.passes.size() == 1)
    return;

  auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.first == &edge; });
  // Maybe the shader could not be created
  if(it == this->m_passes.end())
    return;

  auto& [passes, altPasses, _] = it->second;

  // Draw the passes
  for(auto it = passes.begin(), end = passes.end() - 1; it != end; ++it)
  {
    auto& pass = *it;
    SCORE_ASSERT(pass.renderTarget.renderTarget);
    SCORE_ASSERT(pass.p.pipeline);
    SCORE_ASSERT(pass.p.srb);
    // TODO : combine all the uniforms..

    auto rt = pass.renderTarget.renderTarget;
    auto pipeline = pass.p.pipeline;
    auto srb = pass.p.srb;
    auto texture = pass.renderTarget.texture;

    // Note: updateBatch ownership transfers to QRhi on beginPass; per-pass
    // state (pipeline/srb/processUBO/renderTarget) is owned by m_passes and
    // released in releaseState() / removeOutputPass(). Nothing to free here.
    cb.beginPass(rt, Qt::black, {0.0f, 0}, updateBatch);
    updateBatch = nullptr;
    {
      cb.setGraphicsPipeline(pipeline);
      cb.setShaderResources(srb);

      if(texture)
      {
        cb.setViewport(QRhiViewport(
            0, 0, texture->pixelSize().width(), texture->pixelSize().height()));
      }
      else
      {
        const auto sz = renderer.state.renderSize;
        cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));
      }

      SCORE_ASSERT(!this->m_meshBuffer.buffers.empty());
      SCORE_ASSERT(this->m_meshBuffer.buffers[0].handle->usage().testFlag(
          QRhiBuffer::VertexBuffer));
      m_mesh->draw(m_meshBuffer, cb);
    }
    cb.endPass();

    // Not the last pass: we have to use another resource batch
    updateBatch = renderer.state.rhi->nextResourceUpdateBatch();
  }
}

void RenderedISFNode::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.first == &edge; });
  // Maybe the shader could not be created
  if(it == this->m_passes.end())
    return;
  auto& [passes, altPasses, _] = it->second;

  // Draw the last pass
  const auto& pass = passes.back();
  {
    SCORE_ASSERT(pass.renderTarget.renderTarget);
    SCORE_ASSERT(pass.p.pipeline);
    SCORE_ASSERT(pass.p.srb);
    // TODO : combine all the uniforms..

    auto pipeline = pass.p.pipeline;
    auto srb = pass.p.srb;
    auto texture = pass.renderTarget.texture;

    // No allocations in this scope: this function records draw calls into a
    // command buffer already opened by RenderList::render(). updateBatch is
    // managed by the caller; per-pass state lives in m_passes and is released
    // in releaseState() / removeOutputPass().
    {
      cb.setGraphicsPipeline(pipeline);
      cb.setShaderResources(srb);

      if(texture)
      {
        cb.setViewport(QRhiViewport(
            0, 0, texture->pixelSize().width(), texture->pixelSize().height()));
      }
      else
      {
        const auto sz = renderer.state.renderSize;
        cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));
      }

      m_mesh->draw(m_meshBuffer, cb);
    }
  }

  using namespace std;
  swap(passes, altPasses);

  // Persistent-storage ping-pong. Mutate the shared state exactly once per
  // frame, then re-apply bindings to every SRB across every edge/chain so
  // each draw next frame sees the swapped pointers. Patching only one SRB
  // would leave others referencing stale buffers and read wrong data.
  if(m_lastStorageSwapFrame != renderer.frame)
  {
    m_lastStorageSwapFrame = renderer.frame;
    swapPersistentSSBOsState(m_storage);
    for(auto& [e, p] : m_passes)
    {
      const std::size_t num = p.passes.size();
      for(std::size_t i = 0; i < num; i++)
      {
        auto* mainSrb = p.passes[i].p.srb;
        if(mainSrb)
          reapplyStorageBindings(m_storage, *mainSrb);
        // altPass's SRB aliases the main one for non-persistent passes; skip
        // the second reapply in that case — replaceBuffer is idempotent but
        // srb->create() is not free.
        auto* altSrb = p.altPasses[i].p.srb;
        if(altSrb && altSrb != mainSrb)
          reapplyStorageBindings(m_storage, *altSrb);
      }
    }
  }
}

AudioTextureUpload::AudioTextureUpload()
    : m_fft{128}
{
}

void AudioTextureUpload::process(
    AudioTexture& audio, QRhiResourceUpdateBatch& res, QRhiTexture* rhiTexture)
{
  switch(audio.mode)
  {
    case AudioTexture::Mode::Waveform:
      processTemporal(audio, res, rhiTexture);
      break;
    case AudioTexture::Mode::FFT:
      processSpectral(audio, res, rhiTexture);
      break;
    case AudioTexture::Mode::Histogram:
      processHistogram(audio, res, rhiTexture);
      break;
  }
}

void AudioTextureUpload::processTemporal(
    AudioTexture& audio, QRhiResourceUpdateBatch& res, QRhiTexture* rhiTexture)
{
  if(m_scratchpad.size() < audio.data.size())
    m_scratchpad.resize(audio.data.size());
  for(std::size_t i = 0; i < audio.data.size(); i++)
  {
    m_scratchpad[i] = 0.5f + audio.data[i] / 2.f;
  }

  // Copy it. Texture layout is samples × channels (width × height).
  QRhiTextureSubresourceUploadDescription subdesc(
      m_scratchpad.data(), audio.data.size() * sizeof(float));
  if(audio.channels > 0)
  {
    const int samples_per_channel = int(audio.data.size()) / audio.channels;
    subdesc.setSourceSize(QSize(samples_per_channel, audio.channels));
  }
  QRhiTextureUploadEntry entry{0, 0, subdesc};
  QRhiTextureUploadDescription desc{entry};
  res.uploadTexture(rhiTexture, desc);
}

void AudioTextureUpload::processHistogram(
    AudioTexture& audio, QRhiResourceUpdateBatch& res, QRhiTexture* rhiTexture)
{
  // Size of the audio input buffer
  std::size_t audioInputBufferSize = audio.data.size() / audio.channels;

  // Effective size of the FFT data we want to use (skips DC and nyquist bins;
  // this also matches the texture width picked in updateAudioTexture).
  if(audioInputBufferSize < 4)
    return;
  std::size_t fftSize = audioInputBufferSize / 2 - 2;
  m_scratchpad.resize(240 * fftSize);
  if(m_scratchpad.empty())
    return;

  // 1. Rotate the scratchpad by one row (fftSize)
  auto channel_begin = m_scratchpad.begin();
  std::rotate(channel_begin, m_scratchpad.end() - fftSize, m_scratchpad.end());

  {
    const float dbmax = 0.f;
    const float dbmin = -100.f;
    const float byte_norm = 255.f / (dbmax - dbmin);
    const float norm = 2.f / (fftSize);

    // Histogram treats channel 0 as the source — it's a scrolling
    // spectrogram display and summing / interleaving channels would blur
    // the visualisation. Explicitly use i=0 rather than the old
    // `for(int i = 0; i < 1; i++)` single-iteration loop.
    const int i = 0;
    {
      float* inputData = audio.data.data() + i * audioInputBufferSize;
      double current_window_value = 0.;

      // Basic triangular window function on the audio buffer
      double window_increment = 1. / (audioInputBufferSize / 2);
      for(int s = 0; s < (int)(audioInputBufferSize / 2); s++)
      {
        inputData[s] *= current_window_value;
        current_window_value += window_increment;
      }
      for(int s = (int)(audioInputBufferSize / 2); s < (int)audioInputBufferSize; s++)
      {
        current_window_value -= window_increment;
        inputData[s] *= current_window_value;
      }

      // Compute fft. Spectrum is in CCs format — index 0 is DC, the last
      // coefficient is nyquist. Skip both.
      auto spectrum = m_fft.execute(inputData, audioInputBufferSize);

      float* outputSpectrum = m_scratchpad.data();

      // Fill all fftSize slots of the new row. Previously the loop bounds
      // (k=1..fftSize-1) left the last two pixels of each row untouched,
      // leaking stale data from a 240-frame-old row into every output.
      for(std::size_t k = 0; k < fftSize; k++)
      {
        const std::size_t bin = k + 1; // bins 1..fftSize (skip DC at 0)
        const float float_magnitude
            = std::sqrt(
                  spectrum[bin][0] * spectrum[bin][0]
                  + spectrum[bin][1] * spectrum[bin][1])
              * norm;
        const float float_db = 20.f * std::log10(std::max(float_magnitude, 1e-10f));

        const float magnitude_byte = (float_db - dbmin) * byte_norm;

        // R32F texture with values scaled to [0; 1]
        outputSpectrum[k] = std::clamp(magnitude_byte, 0.f, 255.f) / 255.f;
      }
    }
  }
  // Copy it. setSourceSize makes the upload strides explicit so Qt RHI
  // never second-guesses the row pitch — processSpectral sets it, keeping
  // the histogram path aligned avoids a subtle inconsistency in validation.
  QRhiTextureSubresourceUploadDescription subdesc(
      m_scratchpad.data(), m_scratchpad.size() * sizeof(float));
  subdesc.setSourceSize(QSize((int)fftSize, 240));
  QRhiTextureUploadEntry entry{0, 0, subdesc};
  QRhiTextureUploadDescription desc{entry};
  res.uploadTexture(rhiTexture, desc);
}

void AudioTextureUpload::processSpectral(
    AudioTexture& audio, QRhiResourceUpdateBatch& res, QRhiTexture* rhiTexture)
{
  std::size_t audioBufferSize = audio.data.size() / audio.channels;
  std::size_t fftSize = audioBufferSize / 2;
  std::size_t outputSize = fftSize * audio.channels;

  if(m_scratchpad.size() < outputSize)
    m_scratchpad.resize(outputSize);

  const float norm = 1. / (2. * audioBufferSize);
  for(int i = 0; i < audio.channels; i++)
  {
    float* inputData = audio.data.data() + i * audioBufferSize;
    auto spectrum = m_fft.execute(inputData, audioBufferSize);

    float* outputSpectrum = m_scratchpad.data() + i * fftSize;
    for(std::size_t k = 0; k < fftSize; k++)
    {
      outputSpectrum[k]
          = 1.
            * std::sqrt(
                spectrum[k][0] * spectrum[k][0] + spectrum[k][1] * spectrum[k][1])
            * norm;
    }
  }

  // Copy it
  QRhiTextureSubresourceUploadDescription subdesc(
      m_scratchpad.data(), m_scratchpad.size() * sizeof(float));
  subdesc.setSourceSize(QSize(fftSize, audio.channels));

  QRhiTextureUploadEntry entry{0, 0, subdesc};
  QRhiTextureUploadDescription desc{entry};
  res.uploadTexture(rhiTexture, desc);
}

std::optional<Sampler> AudioTextureUpload::updateAudioTexture(
    AudioTexture& audio, RenderList& renderer, char* materialData,
    QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;
  bool textureChanged = false;
  auto it = audio.samplers.find(&renderer);
  if(it == audio.samplers.end())
  {
    return {};
  }

  auto& [rhiSampler, rhiTexture, fb_] = it->second;

  // The texture the shader wants for the current (mode, samples, channels)
  // triple. Previously the detection compared `curSz.w * curSz.h` against
  // `audio.data.size()` — correct for Waveform (a W=samples × H=channels
  // layout has pixel_count == raw_sample_count), but completely wrong for
  // FFT (half the pixels) and Histogram (H is hard-coded 240 so pixel count
  // bears no relation to the raw audio buffer). The mismatch meant every
  // frame saw "size changed → destroy+recreate the texture", which also
  // forced a full SRB rebuild via replaceTexture in the caller and
  // thrashed the FFT planner's reset() cache.
  const bool has_data = audio.channels > 0 && !audio.data.empty();
  int samples = 0;
  QSize desired{1, 1};
  if(has_data)
  {
    samples = int(audio.data.size()) / audio.channels;
    if(samples % 2 != 0)
      samples++;
    switch(audio.mode)
    {
      case AudioTexture::Mode::Waveform:
        desired = {samples, audio.channels};
        break;
      case AudioTexture::Mode::FFT:
        desired = {std::max(1, samples / 2), audio.channels};
        break;
      case AudioTexture::Mode::Histogram:
        // Histogram is a scrolling spectrogram: rows = frames of FFT history.
        desired = {std::max(1, samples / 2 - 2), 240};
        break;
    }
  }

  const QSize curSz = rhiTexture ? rhiTexture->pixelSize() : QSize{};
  if(curSz != desired || !rhiTexture)
  {
    if(has_data)
    {
      m_fft.reset(samples);

      if(rhiTexture)
      {
        // destroy()+create() on the same QRhiTexture wrapper swaps the
        // native handle (VkImage / ID3D12Resource / MTLTexture). Flag
        // the change so the caller re-runs replaceTexture to refresh
        // the SRB's descriptor set binding.
        rhiTexture->destroy();
        rhiTexture->setPixelSize(desired);
        rhiTexture->create();
        textureChanged = true;
      }
      else
      {
        rhiTexture = rhi.newTexture(
            QRhiTexture::R32F, desired, 1, QRhiTexture::Flag{});
        rhiTexture->setName("AudioTextureUpload::rhiTexture");
        auto created = rhiTexture->create();
        SCORE_ASSERT(created);
        textureChanged = true;
      }
    }
    else
    {
      if(rhiTexture)
      {
        // Audio went quiet: drop our texture and fall back to the
        // RenderList's shared emptyTexture via the caller. Never resize
        // the stored rhiTexture in-place — when that pointer aliased
        // `&renderer.emptyTexture()` (old no-data init path) a resize
        // would have destroyed the shared empty texture used by every
        // unbound sampler in every node on this RenderList.
        rhiTexture->destroy();
        rhiTexture->deleteLater();
        rhiTexture = nullptr;
        textureChanged = true;
      }
      // else: stays nullptr; caller already bound emptyTexture on a
      // previous pass. No need to re-fire replaceTexture.
    }
  }

  if(rhiTexture)
  {
    auto sz = rhiTexture->pixelSize();
    if(sz.width() * sz.height() > 1)
      this->process(audio, res, rhiTexture);
  }

  if(textureChanged)
    return it->second;
  else
    return {};
}

}
