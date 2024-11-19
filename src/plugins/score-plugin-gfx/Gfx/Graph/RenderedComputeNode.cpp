#include <Gfx/Graph/RenderedComputeNode.hpp>
#include <Gfx/Graph/ShaderCache.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>
namespace score::gfx
{
#if 0
RenderedISFNode::~RenderedISFNode() { }
PassOutput RenderedISFNode::initPassSampler(
    ISFNode& n, const isf::pass& pass, RenderList& renderer, int& cur_pos,
    QSize mainTexSize, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;
  // In all the other cases we create a custom render target
  const auto fmt = (pass.float_storage) ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8;
  auto sampler = rhi.newSampler(
      QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None, QRhiSampler::Mirror,
      QRhiSampler::Mirror);
  sampler->setName("ISFNode::initPassSamplers::sampler");
  sampler->create();

  const QSize texSize = (pass.width_expression.empty() && pass.height_expression.empty())
                            ? mainTexSize
                            : n.computeTextureSize(pass, mainTexSize);

  QImage clear_texture(texSize, pass.float_storage ? QImage::Format_RGBA32FPx4 : QImage::Format_ARGB32);
  clear_texture.fill(0);
  auto tex = rhi.newTexture(fmt, texSize, 1, QRhiTexture::RenderTarget);
  tex->setName("ISFNode::initPassSamplers::tex");
  SCORE_ASSERT(tex->create());
  res.uploadTexture(tex, clear_texture);

  if(!pass.target.empty())
  {
    // If the target has a name, it has an associated size variable
    storeTextureRectUniform(n.m_material_data.get(), cur_pos, texSize);
  }

  // Persistent texture means that frame N can access the output of this pass at frame N-1,
  // thus we need two textures, two render targets...

  if(pass.persistent)
  {
    auto tex2 = rhi.newTexture(fmt, texSize, 1, QRhiTexture::RenderTarget);
    tex2->setName("ISFNode::initPassSamplers::tex2");
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

static std::pair<std::vector<Sampler>, int> initInputSamplers(
    RenderList& renderer, const std::vector<Port*>& ports,
    ossia::small_flat_map<const Port*, TextureRenderTarget, 2>& m_rts,
    char* materialData)
{
  std::vector<Sampler> samplers;
  QRhi& rhi = *renderer.state.rhi;
  int cur_pos = 0;

  for(Port* in : ports)
  {
    switch(in->type)
    {
      case Types::Empty:
        break;
      case Types::Int:
      case Types::Float:
        cur_pos += 4;
        break;
      case Types::Vec2:
        cur_pos += 8;
        if(cur_pos % 8 != 0)
          cur_pos += 4;
        break;
      case Types::Vec3:
        while(cur_pos % 16 != 0)
        {
          cur_pos += 4;
        }
        cur_pos += 12;
        break;
      case Types::Vec4:
        while(cur_pos % 16 != 0)
        {
          cur_pos += 4;
        }
        cur_pos += 16;
        break;
      case Types::Image: {
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
        sampler->setName("ISFNode::initInputSamplers::sampler");
        SCORE_ASSERT(sampler->create());

        auto rt = score::gfx::createRenderTarget(
            renderer.state, QRhiTexture::RGBA8, renderer.state.renderSize,
            renderer.samples());
        auto texture = rt.texture;
        samplers.push_back({sampler, texture});

        m_rts[in] = std::move(rt);

        // Allocate some space for the vec4 _imgRect in the uniform
        storeTextureRectUniform(materialData, cur_pos, renderer.state.renderSize);
        break;
      }

      case Types::Audio:
        storeTextureRectUniform(materialData, cur_pos, QSize{1024, 2});
        break;
      default:
        break;
    }
  }
  return {samplers, cur_pos};
}

static std::vector<Sampler>
initAudioTextures(RenderList& renderer, std::list<AudioTexture>& textures)
{
  std::vector<Sampler> samplers;
  QRhi& rhi = *renderer.state.rhi;
  for(auto& texture : textures)
  {
    auto sampler = rhi.newSampler(
        QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
    sampler->setName("ISFNode::initAudioTextures::sampler");
    sampler->create();

    samplers.push_back({sampler, &renderer.emptyTexture()});
    texture.samplers[&renderer] = {sampler, nullptr};
  }
  return samplers;
}

RenderedISFNode::RenderedISFNode(const ISFNode& node) noexcept
    : score::gfx::NodeRenderer{}
    , n{const_cast<ISFNode&>(node)}
{
}

TextureRenderTarget RenderedISFNode::renderTargetForInput(const Port& p)
{
  SCORE_ASSERT(m_rts.find(&p) != m_rts.end());
  return m_rts[&p];
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

    auto pip = score::gfx::buildPipeline(
        renderer, renderer.defaultTriangle(), vertexS, fragmentS, renderTarget, nullptr,
        m_materialUBO,
        std::vector<Sampler>{Sampler{last_sampler->sampler, last_sampler->textures[1]}});
    ret.first = Pass{renderTarget, pip, nullptr};
    ret.second = ret.first;

    // Then we have to use the textures the "main" passes are rendering to
    ret.second.p.srb = score::gfx::createDefaultBindings(
        renderer, ret.second.renderTarget, nullptr, m_materialUBO,
        std::vector<Sampler>{Sampler{last_sampler->sampler, last_sampler->textures[0]}});
  }

  return ret;
}

std::pair<Pass, Pass> RenderedISFNode::createPass(
    RenderList& renderer, ossia::small_vector<PassOutput, 1>& passSamplers,
    PassOutput target, bool previousPassIsPersistent)
{
  std::pair<Pass, Pass> ret;
  QRhi& rhi = *renderer.state.rhi;

  QRhiBuffer* pubo{};
  pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("ISFNode::createPass::pubo");
  pubo->create();

  // Create the main pass
  {
    // Render target for the pass
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
          renderer.state, psampler->textures[0], renderer.samples());
      m_innerPassTargets.push_back(renderTarget);
      renderTarget.texture->setName("ISFNode::createPass::renderTarget.texture");
      renderTarget.renderTarget->setName(
          "ISFNode::createPass::renderTarget.renderTarget");
    }

    auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);
    auto pip = score::gfx::buildPipeline(
        renderer, renderer.defaultTriangle(), v, s, renderTarget, pubo, m_materialUBO,
        allSamplers(passSamplers, 1));
    ret.first = Pass{renderTarget, pip, pubo};
  }

  // If necessary create the alternative pass
  {
    if(auto rt = ossia::get_if<TextureRenderTarget>(&target))
    {
      // Non-persistent last pass
      // assert (!persistent);
      ret.second = ret.first;

      if(previousPassIsPersistent)
      {
        // Then we have to use the textures the "main" passes are rendering to
        ret.second.p.srb = score::gfx::createDefaultBindings(
            renderer, ret.second.renderTarget, pubo, m_materialUBO,
            allSamplers(passSamplers, 0));
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
            renderer.state, psampler->textures[1], renderer.samples());
        m_innerPassTargets.push_back(ret.second.renderTarget);
        ret.second.renderTarget.texture->setName(
            "ISFNode::createPass::ret.second.renderTarget.texture");
        ret.second.renderTarget.renderTarget->setName(
            "ISFNode::createPass::ret.second.renderTarget.renderTarget");

        // We necessarily use the main pass rendered-to samplers
        ret.second.p.srb = score::gfx::createDefaultBindings(
            renderer, ret.second.renderTarget, pubo, m_materialUBO,
            allSamplers(passSamplers, 0));
      }
      else
      {
        ret.second = ret.first;
        if(previousPassIsPersistent)
        {
          // Then we have to use the textures the "main" passes are rendering to
          ret.second.p.srb = score::gfx::createDefaultBindings(
              renderer, ret.second.renderTarget, pubo, m_materialUBO,
              allSamplers(passSamplers, 0));
        }
      }
    }
  }
  return ret;
}

void RenderedISFNode::initPasses(
    const TextureRenderTarget& rt, RenderList& renderer, Edge& edge, int& cur_pos,
    QSize mainTexSize, QRhiResourceUpdateBatch& res)
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
      passes.samplers.push_back(
          initPassSampler(n, pass, renderer, cur_pos, mainTexSize, res));
    }
  }

  bool previousPassIsPersistent = false;
  for(std::size_t i = 0; i < passes.samplers.size(); i++)
  {
    auto& pass = passes.samplers[i];
    const auto [p1, p2]
        = createPass(renderer, passes.samplers, pass, previousPassIsPersistent);
    passes.passes.push_back(p1);
    passes.altPasses.push_back(p2);

    previousPassIsPersistent = model_passes[i].persistent;
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
  QRhi& rhi = *renderer.state.rhi;

  // Create the mesh
  {
    m_mesh = this->n.descriptor().default_vertex_shader ? &renderer.defaultTriangle()
                                                        : &renderer.defaultQuad();
    if(!m_meshBuffer)
    {
      auto [mbuffer, ibuffer] = renderer.initMeshBuffer(*m_mesh, res);
      m_meshBuffer = mbuffer;
      m_idxBuffer = ibuffer;
    }
  }

  // Create the material UBO
  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    SCORE_ASSERT(m_materialUBO->create());
  }

  // Create the samplers
  SCORE_ASSERT(m_rts.empty());
  SCORE_ASSERT(m_passes.empty());
  SCORE_ASSERT(m_inputSamplers.empty());
  SCORE_ASSERT(m_audioSamplers.empty());

  auto [samplers, cur_pos]
      = initInputSamplers(renderer, n.input, m_rts, n.m_material_data.get());
  m_inputSamplers = std::move(samplers);

  m_audioSamplers = initAudioTextures(renderer, n.m_audio_textures);

  // Create the passes

  int pos_before_passes = cur_pos;
  for(Edge* edge : n.output[0]->edges)
  {
    cur_pos = pos_before_passes;
    auto rt = renderer.renderTargetForOutput(*edge);
    if(rt.renderTarget)
    {
      initPasses(rt, renderer, *edge, cur_pos, renderer.state.renderSize, res);
    }
  }
}

void RenderedISFNode::update(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  SCORE_ASSERT(m_passes.size() > 0);

  // PASSINDEX must be set to the last index
  // FIXME

  // FIXME should be -2 if last pass is persistent
  if(n.m_descriptor.passes.back().persistent)
    n.standardUBO.passIndex = m_passes.size() - 2;
  else
    n.standardUBO.passIndex = m_passes.size() - 1;

  // Update audio textures
  for(auto& audio : n.m_audio_textures)
  {
    if(std::optional<Sampler> sampl
       = m_audioTex.updateAudioTexture(audio, renderer, n.m_material_data.get(), res))
    {
      // Audio texture changed, this means the material needs update
      materialChangedIndex = -1;

      auto& [rhiSampler, tex] = *sampl;
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
  }

  // Update material
  if(m_materialUBO && m_materialSize > 0 && n.hasMaterialChanged(materialChangedIndex))
  {
    char* data = n.m_material_data.get();
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
  }

  // Update all the process UBOs

  for(auto& [e, p] : m_passes)
  {
    for(int i = 0, N = p.passes.size(); i < N; i++)
    {
      auto& pass = p.passes[i];
      if(pass.processUBO)
      {
        n.standardUBO.passIndex = i;

        res.updateDynamicBuffer(
            pass.processUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
      }
    }
  }
}

void RenderedISFNode::release(RenderList& r)
{
  // customRelease
  {

    for(auto [edge, rt] : m_rts)
    {
      rt.release();
    }
    m_rts.clear();

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

      std::size_t n = passes.size();
      for(std::size_t i = 0; i < n; i++)
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
          // TODO check texture deletion ???
          // texture isdeleted elsewxheree
        }
        else
        {
          // It's the render target of another node, do not touch it
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
    // texture isdeleted elsewxheree
  }
  m_inputSamplers.clear();
  for(auto sampler : m_audioSamplers)
  {
    delete sampler.sampler;
    // texture isdeleted elsewxheree
  }
  m_audioSamplers.clear();

  delete m_materialUBO;
  m_materialUBO = nullptr;

  m_meshBuffer = nullptr;

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
  SCORE_ASSERT(!this->m_passes.empty());
  if(this->m_passes[0].second.passes.size() == 1)
  {
    return;
  }

  auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.first == &edge; });
  SCORE_ASSERT(it != this->m_passes.end());
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

    // TODO need to free stuff
    cb.beginPass(rt, Qt::black, {1.0f, 0}, updateBatch);
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

      assert(this->m_meshBuffer);
      assert(this->m_meshBuffer->usage().testFlag(QRhiBuffer::VertexBuffer));
      m_mesh->draw({this->m_meshBuffer, this->m_idxBuffer}, cb);
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
  SCORE_ASSERT(it != this->m_passes.end());
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

    // TODO need to free stuff
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

      m_mesh->draw({this->m_meshBuffer, this->m_idxBuffer}, cb);
    }
  }

  using namespace std;
  swap(passes, altPasses);
}
#endif
}

namespace score::gfx
{

SimpleRenderedComputeNode::SimpleRenderedComputeNode(const ISFNode& node) noexcept
    : score::gfx::NodeRenderer{}
    , n{const_cast<ISFNode&>(node)}
{
}

TextureRenderTarget SimpleRenderedComputeNode::renderTargetForInput(const Port& p)
{
  SCORE_ASSERT(m_rts.find(&p) != m_rts.end());
  return m_rts[&p];
}

std::vector<Sampler> SimpleRenderedComputeNode::allSamplers() const noexcept
{
  // Input ports
  std::vector<Sampler> samplers = m_inputSamplers;

  // Audio textures
  samplers.insert(samplers.end(), m_audioSamplers.begin(), m_audioSamplers.end());

  return samplers;
}

void SimpleRenderedComputeNode::initPass(
    const TextureRenderTarget& renderTarget, RenderList& renderer, Edge& edge)
{
  auto& model_passes = n.descriptor().passes;
  SCORE_ASSERT(model_passes.size() == 1);

  QRhi& rhi = *renderer.state.rhi;

  QRhiBuffer* pubo{};
  pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("SimpleRenderedComputeNode::createPass::pubo");
  pubo->create();

  // Create the main pass
  auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);
  auto pip = score::gfx::buildPipeline(
      renderer, *m_mesh, v, s, renderTarget, pubo, m_materialUBO, allSamplers());
  m_passes.emplace_back(&edge, Pass{renderTarget, pip, pubo});
}

void SimpleRenderedComputeNode::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  // Create the global shared inputs
  m_srb = initBindings(renderer);
  m_pipeline = createComputePipeline(renderer);
  m_pipeline->setShaderResourceBindings(m_srb);

  SCORE_ASSERT(m_srb->create());
  SCORE_ASSERT(m_pipeline->create());

#if 0
  // Create the mesh
  {
    m_mesh = this->n.descriptor().default_vertex_shader ? &renderer.defaultTriangle()
                                                        : &renderer.defaultQuad();

    if(!m_meshBuffer)
    {
      auto [mbuffer, ibuffer] = renderer.initMeshBuffer(*m_mesh, res);
      m_meshBuffer = mbuffer;
      m_idxBuffer = ibuffer;
    }
  }

  // Create the material UBO
  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    SCORE_ASSERT(m_materialUBO->create());
  }

  // Create the samplers
  SCORE_ASSERT(m_rts.empty());
  SCORE_ASSERT(m_passes.empty());
  SCORE_ASSERT(m_inputSamplers.empty());
  SCORE_ASSERT(m_audioSamplers.empty());

  auto [samplers, cur_pos]
      = initInputSamplers(renderer, n.input, m_rts, n.m_material_data.get());
  m_inputSamplers = std::move(samplers);

  m_audioSamplers = initAudioTextures(renderer, n.m_audio_textures);

  // Create the passes

  for(Edge* edge : n.output[0]->edges)
  {
    auto rt = renderer.renderTargetForOutput(*edge);
    if(rt.renderTarget)
    {
      initPass(rt, renderer, *edge);
    }
  }
#endif
}

void SimpleRenderedComputeNode::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  SCORE_ASSERT(m_passes.size() > 0);

#if 0
  n.standardUBO.passIndex = 0;

  // Update audio textures
  if(!n.m_audio_textures.empty() && !m_audioTex)
  {
    m_audioTex.emplace();
  }

  for(auto& audio : n.m_audio_textures)
  {
    if(std::optional<Sampler> sampl
       = m_audioTex->updateAudioTexture(audio, renderer, n.m_material_data.get(), res))
    {
      // Texture changed -> material changed
      materialChangedIndex = -1;

      auto& [rhiSampler, tex] = *sampl;
      for(auto& [e, pass] : m_passes)
      {
        score::gfx::replaceTexture(
            *pass.p.srb, rhiSampler, tex ? tex : &renderer.emptyTexture());
      }
    }
  }

  // Update material
  if(m_materialUBO && m_materialSize > 0 && n.hasMaterialChanged(materialChangedIndex))
  {
    char* data = n.m_material_data.get();
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
  }

  // Update all the process UBOs

  for(auto& [e, pass] : m_passes)
  {
    res.updateDynamicBuffer(
        pass.processUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
  }
#endif
}

void SimpleRenderedComputeNode::release(RenderList& r)
{
  // customRelease
  {
    for(auto [edge, rt] : m_rts)
    {
      rt.release();
    }
    m_rts.clear();

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
        pass.processUBO->deleteLater();
    }

    m_passes.clear();
  }
#if 0

  for(auto sampler : m_inputSamplers)
  {
    delete sampler.sampler;
    // texture isdeleted elsewxheree
  }
  m_inputSamplers.clear();
  for(auto sampler : m_audioSamplers)
  {
    delete sampler.sampler;
    // texture isdeleted elsewxheree
  }
  m_audioSamplers.clear();

  delete m_materialUBO;
  m_materialUBO = nullptr;

  m_meshBuffer = nullptr;
#endif
  // Release the allocated pipelines
  if(m_srb)
    m_srb->deleteLater();
  if(m_pipeline)
    m_pipeline->deleteLater();
  m_srb = nullptr;
  m_pipeline = nullptr;
}

void SimpleRenderedComputeNode::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& updateBatch,
    Edge& edge)
{
  auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.first == &edge; });
  SCORE_ASSERT(it != this->m_passes.end());
  auto& pass = it->second;
  // Apply the controls

  // Run the compute shader
  {
    SCORE_ASSERT(this->m_pipeline);
    SCORE_ASSERT(this->m_pipeline->shaderResourceBindings());
    for(auto& promise : this->state.dispatch())
    {
      using ret_type = decltype(promise.feedback_value);
      gpp::qrhi::handle_dispatch<GpuComputeRenderer, ret_type> handler{
          *this, *renderer.state.rhi, cb, res, *this->m_pipeline};
      promise.feedback_value = visit(handler, promise.current_command);
    }
  }

// Clear the readbacks
#if QT_VERSION < QT_VERSION_CHECK(6, 6, 0)
  for(auto rb : this->bufReadbacks)
    delete rb;
  this->bufReadbacks.clear();
#endif
  for(auto rb : this->texReadbacks)
    delete rb;
  this->texReadbacks.clear();

  // Copy the data to the model node
}

void SimpleRenderedComputeNode::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
}

QRhiTexture* SimpleRenderedComputeNode::createInput(
    RenderList& renderer, int k, QRhiTexture::Format fmt, QSize size)
{
  // auto port = parent.input[k];
  // static constexpr auto flags
  //     = QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore;
  // auto texture = renderer.state.rhi->newTexture(fmt, size, 1, flags);
  // SCORE_ASSERT(texture->create());
  // m_rts[port]
  //     = score::gfx::createRenderTarget(renderer.state, texture, renderer.samples());
  // return texture;
}

#if 0
QRhiShaderResourceBinding
SimpleRenderedComputeNode::initBinding(score::gfx::RenderList& renderer, int field)
{
  static constexpr auto bindingStages = QRhiShaderResourceBinding::ComputeStage;
  if constexpr(requires { F::ubo; })
  {
    auto it = createdUbos.find(F::binding());
    QRhiBuffer* buffer = it != createdUbos.end() ? it->second : nullptr;
    return QRhiShaderResourceBinding::uniformBuffer(F::binding(), bindingStages, buffer);
  }
  else if constexpr(requires { F::image2D; })
  {
    auto tex_it = createdTexs.find(F::binding());
    QRhiTexture* tex
        = tex_it != createdTexs.end() ? tex_it->second : &renderer.emptyTexture();

    if constexpr(requires { F::load; } && requires { F::store; })
      return QRhiShaderResourceBinding::imageLoadStore(
          F::binding(), bindingStages, tex, 0);
    else if constexpr(requires { F::readonly; })
      return QRhiShaderResourceBinding::imageLoad(F::binding(), bindingStages, tex, 0);
    else if constexpr(requires { F::writeonly; })
      return QRhiShaderResourceBinding::imageStore(F::binding(), bindingStages, tex, 0);
    else
      static_assert(F::load || F::store);
  }
  else if constexpr(requires { F::buffer; })
  {
    auto it = createdUbos.find(F::binding());
    QRhiBuffer* buf = it != createdUbos.end() ? it->second : nullptr;

    if constexpr(requires { F::load; } && requires { F::store; })
      return QRhiShaderResourceBinding::bufferLoadStore(
          F::binding(), bindingStages, buf);
    else if constexpr(requires { F::load; })
      return QRhiShaderResourceBinding::bufferLoad(F::binding(), bindingStages, buf);
    else if constexpr(requires { F::store; })
      return QRhiShaderResourceBinding::bufferStore(F::binding(), bindingStages, buf);
    else
      static_assert(F::load || F::store);
  }
  else
  {
    static_assert(F::nope);
    throw;
  }
}
#endif

QRhiShaderResourceBindings*
SimpleRenderedComputeNode::initBindings(score::gfx::RenderList& renderer)
{
  auto& rhi = *renderer.state.rhi;
  // Shader resource bindings
  auto srb = rhi.newShaderResourceBindings();
  SCORE_ASSERT(srb);

  QVarLengthArray<QRhiShaderResourceBinding, 8> bindings;

#if 0
  using bindings_type = decltype(Node_T::layout::bindings);
  boost::pfr::for_each_field(
      bindings_type{}, [&](auto f) { bindings.push_back(initBinding(renderer, f)); });
#endif
  srb->setBindings(bindings.begin(), bindings.end());
  return srb;
}

QRhiComputePipeline*
SimpleRenderedComputeNode::createComputePipeline(score::gfx::RenderList& renderer)
{
  auto& rhi = *renderer.state.rhi;
  auto compute = rhi.newComputePipeline();
#if 0
  auto cs = score::gfx::makeCompute(renderer.state, ""); // FIXME
#endif
  compute->setShaderStage(QRhiShaderStage(QRhiShaderStage::Compute, cs));

  return compute;
}

#if 0

  void init_input(score::gfx::RenderList& renderer, auto field)
  {
    //using input_type = std::decay_t<F>;
  }

  template <std::size_t Idx, typename F>
    requires avnd::image_port<F>
  void init_input(score::gfx::RenderList& renderer, avnd::field_reflection<Idx, F> field)
  {
    using bindings_type = decltype(Node_T::layout::bindings);
    using image_type = std::decay_t<decltype(bindings_type{}.*F::image())>;
    auto tex = createInput(
        renderer, sampler_k++, gpp::qrhi::textureFormat<image_type>(),
        renderer.state.renderSize);

    using sampler_type = typename avnd::member_reflection<F::image()>::member_type;
    createdTexs[sampler_type::binding()] = tex;
  }

  template <std::size_t Idx, typename F>
    requires avnd::uniform_port<F>
  void init_input(score::gfx::RenderList& renderer, avnd::field_reflection<Idx, F> field)
  {
    using ubo_type = typename avnd::member_reflection<F::uniform()>::class_type;

    // We must mark the UBO to construct.
    if(createdUbos.find(ubo_type::binding()) != createdUbos.end())
      return;

    auto ubo = renderer.state.rhi->newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, gpp::std140_size<ubo_type>());
    ubo->create();

    createdUbos[ubo_type::binding()] = ubo;
  }
#endif
SimpleRenderedComputeNode::~SimpleRenderedComputeNode() { }

}
