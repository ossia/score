#include <Gfx/Graph/RenderedISFNode.hpp>
#include <Gfx/Graph/ShaderCache.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>
namespace score::gfx
{

#include <Gfx/Qt5CompatPush> // clang-format: keep

static void storeTextureRectUniform(char* buffer, int& cur_pos, QSize texSize)
{
  while(cur_pos % 16 != 0)
  {
    cur_pos += 4;
  }

  *(float*)(buffer + cur_pos + 0) = 0.0f;
  *(float*)(buffer + cur_pos + 4) = 0.0f;
  *(float*)(buffer + cur_pos + 8) = texSize.width();
  *(float*)(buffer + cur_pos + 12) = texSize.height();

  cur_pos += 16;
}

RenderedISFNode::~RenderedISFNode() { }
PassOutput RenderedISFNode::initPassSampler(
    ISFNode& n, const isf::pass& pass, RenderList& renderer, int& cur_pos,
    QSize mainTexSize)
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

  auto tex = rhi.newTexture(fmt, texSize, 1, QRhiTexture::RenderTarget);
  tex->setName("ISFNode::initPassSamplers::tex");
  SCORE_ASSERT(tex->create());

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
  vec2 texcoordAdjust;
  vec2 renderSize;
};

out gl_PerVertex { vec4 gl_Position; };

void main()
{
  v_texcoord = texcoord;
  gl_Position = clipSpaceCorrMatrix * vec4(position.xy, 0.0, 1.);
}
)_";

  static const constexpr auto fragment_shader = R"_(#version 450
layout(std140, binding = 0) uniform renderer_t {
  mat4 clipSpaceCorrMatrix;
  vec2 texcoordAdjust;
  vec2 renderSize;
};

layout(binding=3) uniform sampler2D y_tex;

layout(location = 0) in vec2 v_texcoord;
layout(location = 0) out vec4 fragColor;

void main ()
{
  vec2 factor = textureSize(y_tex, 0) / renderSize;
  vec2 ifactor = renderSize / textureSize(y_tex, 0);
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
    QSize mainTexSize)
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
          initPassSampler(n, pass, renderer, cur_pos, mainTexSize));
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
      initPasses(rt, renderer, *edge, cur_pos, renderer.state.renderSize);
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

  // Update material
  if(m_materialUBO && m_materialSize > 0 && n.hasMaterialChanged(materialChangedIndex))
  {
    char* data = n.m_material_data.get();
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
  }

  // Update audio textures
  for(auto& audio : n.m_audio_textures)
  {
    if(std::optional<Sampler> sampl
       = m_audioTex.updateAudioTexture(audio, renderer, res))
    {
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
#include <Gfx/Qt5CompatPush> // clang-format: keep

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

#include <Gfx/Qt5CompatPop> // clang-format: keep
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

AudioTextureUpload::AudioTextureUpload()
    : m_fft{128}
{
}

void AudioTextureUpload::process(
    AudioTexture& audio, QRhiResourceUpdateBatch& res, QRhiTexture* rhiTexture)
{
  if(audio.fft)
  {
    processSpectral(audio, res, rhiTexture);
  }
  else
  {
    processTemporal(audio, res, rhiTexture);
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

  // Copy it
  QRhiTextureSubresourceUploadDescription subdesc(
      m_scratchpad.data(), audio.data.size() * sizeof(float));
  QRhiTextureUploadEntry entry{0, 0, subdesc};
  QRhiTextureUploadDescription desc{entry};
  res.uploadTexture(rhiTexture, desc);
}

void AudioTextureUpload::processSpectral(
    AudioTexture& audio, QRhiResourceUpdateBatch& res, QRhiTexture* rhiTexture)
{
  if(m_scratchpad.size() < audio.data.size())
    m_scratchpad.resize(audio.data.size() / 2);
  std::size_t bufferSize = audio.data.size() / audio.channels;
  std::size_t fftSize = bufferSize / 2;
  const float norm = 1. / (2. * bufferSize);
  for(int i = 0; i < audio.channels; i++)
  {
    float* inputData = audio.data.data() + i * bufferSize;
    auto spectrum = m_fft.execute(inputData, bufferSize);

    float* outputSpectrum = m_scratchpad.data() + i * fftSize;
    for(std::size_t k = 0; k < fftSize; k++)
    {
      outputSpectrum[k] = 0.5f + spectrum[k][0] * norm;
    }
  }

  // Copy it
  QRhiTextureSubresourceUploadDescription subdesc(
      m_scratchpad.data(), (audio.data.size() / 2) * sizeof(float));
  QRhiTextureUploadEntry entry{0, 0, subdesc};
  QRhiTextureUploadDescription desc{entry};
  res.uploadTexture(rhiTexture, desc);
}

std::optional<Sampler> AudioTextureUpload::updateAudioTexture(
    AudioTexture& audio, RenderList& renderer, QRhiResourceUpdateBatch& res)
{
#include <Gfx/Qt5CompatPush> // clang-format: keep
  QRhi& rhi = *renderer.state.rhi;
  bool textureChanged = false;
  auto it = audio.samplers.find(&renderer);
  if(it == audio.samplers.end())
    return {};

  auto& [rhiSampler, rhiTexture] = it->second;
  const auto curSz = (rhiTexture) ? rhiTexture->pixelSize() : QSize{};
  int numSamples = curSz.width() * curSz.height();
  if(numSamples != int(audio.data.size()))
  {
    delete rhiTexture;
    rhiTexture = nullptr;
    textureChanged = true;
  }

  if(!rhiTexture)
  {
    if(audio.channels > 0)
    {
      int samples = audio.data.size() / audio.channels;
      int pixelWidth = samples / (audio.fft ? 2 : 1);

      m_fft.reset(samples);

      rhiTexture = rhi.newTexture(
          QRhiTexture::R32F, {pixelWidth, audio.channels}, 1, QRhiTexture::Flag{});
      rhiTexture->create();
      textureChanged = true;
    }
    else
    {
      rhiTexture = nullptr;
      textureChanged = true;
    }
  }

  if(rhiTexture)
  {
    // Process the audio data
    this->process(audio, res, rhiTexture);
  }

  if(textureChanged)
  {
    return it->second;
  }
  else
  {
    return {};
  }
}

#include <Gfx/Qt5CompatPop> // clang-format: keep
}

namespace score::gfx
{

#include <Gfx/Qt5CompatPush> // clang-format: keep
SimpleRenderedISFNode::SimpleRenderedISFNode(const ISFNode& node) noexcept
    : score::gfx::NodeRenderer{}
    , n{const_cast<ISFNode&>(node)}
{
}

TextureRenderTarget SimpleRenderedISFNode::renderTargetForInput(const Port& p)
{
  SCORE_ASSERT(m_rts.find(&p) != m_rts.end());
  return m_rts[&p];
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
    const TextureRenderTarget& renderTarget, RenderList& renderer, Edge& edge)
{
  auto& model_passes = n.descriptor().passes;
  SCORE_ASSERT(model_passes.size() == 1);

  QRhi& rhi = *renderer.state.rhi;

  QRhiBuffer* pubo{};
  pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("SimpleRenderedISFNode::createPass::pubo");
  pubo->create();

  // Create the main pass
  auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);
  auto pip = score::gfx::buildPipeline(
      renderer, *m_mesh, v, s, renderTarget, pubo, m_materialUBO, allSamplers());
  m_passes.emplace_back(&edge, Pass{renderTarget, pip, pubo});
}

void SimpleRenderedISFNode::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
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

  for(Edge* edge : n.output[0]->edges)
  {
    auto rt = renderer.renderTargetForOutput(*edge);
    if(rt.renderTarget)
    {
      initPass(rt, renderer, *edge);
    }
  }
}

void SimpleRenderedISFNode::update(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  SCORE_ASSERT(m_passes.size() > 0);

  n.standardUBO.passIndex = 0;

  // Update material
  if(m_materialUBO && m_materialSize > 0 && n.hasMaterialChanged(materialChangedIndex))
  {
    char* data = n.m_material_data.get();
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
  }

  // Update audio textures
  if(!n.m_audio_textures.empty())
    m_audioTex.emplace();

  for(auto& audio : n.m_audio_textures)
  {
    if(std::optional<Sampler> sampl
       = m_audioTex->updateAudioTexture(audio, renderer, res))
    {
      auto& [rhiSampler, tex] = *sampl;
      for(auto& [e, pass] : m_passes)
      {
        score::gfx::replaceTexture(
            *pass.p.srb, rhiSampler, tex ? tex : &renderer.emptyTexture());
      }
    }
  }

  // Update all the process UBOs

  for(auto& [e, pass] : m_passes)
  {
    res.updateDynamicBuffer(
        pass.processUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
  }
}

void SimpleRenderedISFNode::release(RenderList& r)
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

void SimpleRenderedISFNode::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& updateBatch,
    Edge& edge)
{
}

void SimpleRenderedISFNode::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.first == &edge; });
  SCORE_ASSERT(it != this->m_passes.end());
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
      else
      {
        const auto sz = renderer.state.renderSize;
        cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));
      }

      m_mesh->draw({this->m_meshBuffer, this->m_idxBuffer}, cb);
    }
  }
}

SimpleRenderedISFNode::~SimpleRenderedISFNode() { }

#include <Gfx/Qt5CompatPop> // clang-format: keep
}
