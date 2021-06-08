#include <Gfx/Graph/RenderedISFNode.hpp>

namespace score::gfx
{

#include <Gfx/Qt5CompatPush> // clang-format: keep

std::vector<PersistSampler> RenderedISFNode::initPassSamplers(
    ISFNode& n,
    RenderList& renderer,
    int& cur_pos,
    QSize mainTexSize)
{
  std::vector<PersistSampler> samplers;
  QRhi& rhi = *renderer.state.rhi;
  auto& model_passes = n.descriptor().passes;
  for (auto& pass : model_passes)
  {
    const auto fmt
        = (pass.float_storage) ? QRhiTexture::RGBA32F : QRhiTexture::RGBA8;
    auto sampler = rhi.newSampler(
        QRhiSampler::Linear,
        QRhiSampler::Linear,
        QRhiSampler::None,
        QRhiSampler::ClampToEdge,
        QRhiSampler::ClampToEdge);
    sampler->setName("ISFNode::initPassSamplers::sampler");
    sampler->create();

    const QSize texSize
        = (pass.width_expression.empty() && pass.height_expression.empty())
              ? mainTexSize
              : n.computeTextureSize(pass, mainTexSize);

    auto tex = rhi.newTexture(fmt, texSize, 1, QRhiTexture::RenderTarget);
    tex->setName("ISFNode::initPassSamplers::tex");
    SCORE_ASSERT(tex->create());

    // Persistent texture means that frame N can access the output of this pass at frame N-1,
    // thus we need two textures, two render targets...
    if (pass.persistent)
    {
      auto tex2 = rhi.newTexture(fmt, texSize, 1, QRhiTexture::RenderTarget);
      tex2->setName("ISFNode::initPassSamplers::tex2");
      SCORE_ASSERT(tex2->create());

      samplers.push_back({sampler, tex, tex2});
    }
    else
    {
      samplers.push_back({sampler, tex, tex});
    }

    if (!pass.target.empty())
    {
      // If the target has a name, it has an associated size variable
      if (cur_pos % 8 != 0)
        cur_pos += 4;

      *(float*)(n.m_material_data.get() + cur_pos) = texSize.width();
      *(float*)(n.m_material_data.get() + cur_pos + 4) = texSize.height();

      cur_pos += 8;
    }
  }
  return samplers;
}

static std::pair<std::vector<Sampler>, int> initInputSamplers(
    RenderList& renderer,
    const std::vector<Port*>& ports,
    char* materialData)
{
  std::vector<Sampler> samplers;
  QRhi& rhi = *renderer.state.rhi;
  int cur_pos = 0;
  for (Port* in : ports)
  {
    switch (in->type)
    {
      case Types::Empty:
        break;
      case Types::Int:
      case Types::Float:
        cur_pos += 4;
        break;
      case Types::Vec2:
        cur_pos += 8;
        if (cur_pos % 8 != 0)
          cur_pos += 4;
        break;
      case Types::Vec3:
        while (cur_pos % 16 != 0)
        {
          cur_pos += 4;
        }
        cur_pos += 12;
        break;
      case Types::Vec4:
        while (cur_pos % 16 != 0)
        {
          cur_pos += 4;
        }
        cur_pos += 16;
        break;
      case Types::Image:
      {
        auto sampler = rhi.newSampler(
            QRhiSampler::Linear,
            QRhiSampler::Linear,
            QRhiSampler::None,
            QRhiSampler::ClampToEdge,
            QRhiSampler::ClampToEdge);
        sampler->setName("ISFNode::initInputSamplers::sampler");
        SCORE_ASSERT(sampler->create());

        auto texture = renderer.textureTargetForInputPort(*in);
        samplers.push_back({sampler, texture});

        if (cur_pos % 8 != 0)
          cur_pos += 4;

        *(float*)(materialData + cur_pos) = texture->pixelSize().width();
        *(float*)(materialData + cur_pos + 4) = texture->pixelSize().height();

        cur_pos += 8;
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
  for (auto& texture : textures)
  {
    auto sampler = rhi.newSampler(
        QRhiSampler::Linear,
        QRhiSampler::Linear,
        QRhiSampler::None,
        QRhiSampler::ClampToEdge,
        QRhiSampler::ClampToEdge);
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

std::optional<QSize> RenderedISFNode::renderTargetSize() const noexcept
{
  return {};
}

TextureRenderTarget RenderedISFNode::renderTarget() const noexcept
{
  SCORE_ASSERT(!m_passes.empty());
  return m_passes.back().renderTarget;
}

std::pair<Pass, Pass>
RenderedISFNode::createPass(RenderList& renderer, PersistSampler target)
{
  std::pair<Pass, Pass> ret;
  QRhi& rhi = *renderer.state.rhi;

  QRhiBuffer* pubo{};
  pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("ISFNode::createPass::pubo");
  pubo->create();

  auto renderTarget
      = score::gfx::createRenderTarget(renderer.state, target.textures[0]);
  {
    renderTarget.texture->setName("ISFNode::createPass::renderTarget.texture");
    renderTarget.renderTarget->setName("ISFNode::createPass::renderTarget.renderTarget");

    std::vector<Sampler> samplers = m_inputSamplers;
    samplers.insert(
        samplers.end(), m_audioSamplers.begin(), m_audioSamplers.end());
    for (auto& pass : m_passSamplers)
      samplers.push_back({pass.sampler, pass.textures[1]});

    auto pip = score::gfx::buildPipeline(
        renderer,
        n.mesh(),
        n.m_vertexS,
        n.m_fragmentS,
        renderTarget,
        pubo,
        m_materialUBO,
        samplers);
    ret.first = Pass{renderTarget, pip, pubo};
  }

  if (target.textures[1] != target.textures[0])
  {
    ret.second.processUBO = ret.first.processUBO;
    ret.second.p = ret.first.p;
    ret.second.renderTarget
        = score::gfx::createRenderTarget(renderer.state, target.textures[1]);
    ret.second.renderTarget.texture->setName("ISFNode::createPass::ret.second.renderTarget.texture");
    ret.second.renderTarget.renderTarget->setName("ISFNode::createPass::ret.second.renderTarget.renderTarget");

    std::vector<Sampler> samplers = m_inputSamplers;
    samplers.insert(
        samplers.end(), m_audioSamplers.begin(), m_audioSamplers.end());
    for (auto& pass : m_passSamplers)
      samplers.push_back({pass.sampler, pass.textures[0]});

    ret.second.p.srb = score::gfx::createDefaultBindings(
        renderer, ret.second.renderTarget, pubo, m_materialUBO, samplers);
  }
  else
  {
    ret.second = ret.first;
  }

  return ret;
}

void RenderedISFNode::initPasses(RenderList& renderer)
{
  for (auto& pass : m_passSamplers)
  {
    const auto [p1, p2] = createPass(renderer, pass);
    m_passes.push_back(p1);
    m_altPasses.push_back(p2);
  }
}

void RenderedISFNode::init(RenderList& renderer)
{
  QRhi& rhi = *renderer.state.rhi;

  // Create the mesh
  {
    const auto& mesh = n.mesh();
    if (!m_meshBuffer)
    {
      auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh);
      m_meshBuffer = mbuffer;
      m_idxBuffer = ibuffer;
    }
  }

  // Create the material UBO
  m_materialSize = n.m_materialSize;
  if (m_materialSize > 0)
  {
    m_materialUBO = rhi.newBuffer(
        QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    SCORE_ASSERT(m_materialUBO->create());
  }

  // Create the samplers
  auto [samplers, cur_pos]
      = initInputSamplers(renderer, n.input, n.m_material_data.get());
  m_inputSamplers = std::move(samplers);

  m_audioSamplers = initAudioTextures(renderer, n.m_audio_textures);

  m_passSamplers = initPassSamplers(n, renderer, cur_pos, renderer.state.size);

  // Create the passes
  initPasses(renderer);
}

void RenderedISFNode::update(
    RenderList& renderer,
    QRhiResourceUpdateBatch& res)
{
  // Update material
  if (m_materialUBO && m_materialSize > 0
      && materialChangedIndex != n.materialChanged)
  {
    char* data = n.m_material_data.get();
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
    materialChangedIndex = n.materialChanged;
  }

  // Update input textures
  {
    int sampler_i = 0;
    for (Port* in : n.input)
    {
      if (in->type == Types::Image)
      {
        auto new_texture = renderer.textureTargetForInputPort(*in);
        auto cur_texture = m_inputSamplers[sampler_i].texture;
        if (new_texture != cur_texture)
        {
          for (auto& pass : m_passes)
          {
            score::gfx::replaceTexture(*pass.p.srb, cur_texture, new_texture);
            // TODO we must also update the texture size in the material.
          }
        }
        sampler_i++;
      }
    }
  }

  // Update audio textures
  for (auto& audio : n.m_audio_textures)
  {
    m_audioTex.updateAudioTexture(audio, renderer, res, m_passes);
  }

  // Update all the process UBOs
  for (int i = 0, N = m_passes.size(); i < N; i++)
  {
    n.standardUBO.passIndex = i;
    res.updateDynamicBuffer(
        m_passes[i].processUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
  }
}

void RenderedISFNode::releaseWithoutRenderTarget(RenderList& r)
{
  // customRelease
  {
    for (auto& texture : n.m_audio_textures)
    {
      auto it = texture.samplers.find(&r);
      if (it != texture.samplers.end())
      {
        if (auto tex = it->second.texture)
        {
          if (tex != &r.emptyTexture())
            tex->deleteLater();
        }
      }
    }

    bool same = m_passes[0].p.pipeline == m_altPasses[0].p.pipeline;
    for (Pass& pass : m_passes)
    {
      pass.p.release();
      pass.renderTarget.release();
      pass.processUBO->deleteLater();
    }
    if(!same)
    {
      for (Pass& pass : m_altPasses)
      {
        pass.p.release();
        pass.renderTarget.release();
        pass.processUBO->deleteLater();
      }
    }


    m_passes.clear();

    m_altPasses.clear();
  }

  for (auto sampler : m_inputSamplers)
  {
    delete sampler.sampler;
    // texture isdeleted elsewxheree
  }
  m_inputSamplers.clear();
  for (auto sampler : m_audioSamplers)
  {
    delete sampler.sampler;
    // texture isdeleted elsewxheree
  }
  m_audioSamplers.clear();
  for (auto sampler : m_passSamplers)
  {
    delete sampler.sampler;
    // texture isdeleted elsewxheree
  }
  m_passSamplers.clear();

  delete m_materialUBO;
  m_materialUBO = nullptr;

  m_meshBuffer = nullptr;
}

void RenderedISFNode::release(RenderList& r)
{
  releaseWithoutRenderTarget(r);

  // TOOD m_lastPassRT.release();
}

void RenderedISFNode::runPass(
    RenderList& renderer,
    QRhiCommandBuffer& cb,
    QRhiResourceUpdateBatch& res)
{
  // if(m_passes.empty())
  //   return RenderedNode::runPass(renderer, cb, res);

  // Update a first time everything

  // PASSINDEX must be set to the last index
  // FIXME
  n.standardUBO.passIndex = m_passes.size() - 1;

  update(renderer, res);

  auto updateBatch = &res;

  // Draw the passes
  for (const auto& pass : m_passes)
  {
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

      if (texture)
      {
        cb.setViewport(QRhiViewport(
            0,
            0,
            texture->pixelSize().width(),
            texture->pixelSize().height()));
      }
      else
      {
        const auto sz = renderer.state.size;
        cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));
      }

      assert(this->m_meshBuffer);
      assert(this->m_meshBuffer->usage().testFlag(QRhiBuffer::VertexBuffer));
      n.mesh().setupBindings(*this->m_meshBuffer, this->m_idxBuffer, cb);

      cb.draw(n.mesh().vertexCount);
    }
    cb.endPass();

    if (pass.p.pipeline != m_passes.back().p.pipeline)
    {
      // Not the last pass: we have to use another resource batch
      updateBatch = renderer.state.rhi->nextResourceUpdateBatch();
    }
  }

  using namespace std;
  swap(m_passes, m_altPasses);
}

AudioTextureUpload::AudioTextureUpload()
    : m_fft{128}
{
}

void AudioTextureUpload::process(
    AudioTexture& audio,
    QRhiResourceUpdateBatch& res,
    QRhiTexture* rhiTexture)
{
  if (audio.fft)
  {
    processSpectral(audio, res, rhiTexture);
  }
  else
  {
    processTemporal(audio, res, rhiTexture);
  }
}

void AudioTextureUpload::processTemporal(
    AudioTexture& audio,
    QRhiResourceUpdateBatch& res,
    QRhiTexture* rhiTexture)
{
  if (m_scratchpad.size() < audio.data.size())
    m_scratchpad.resize(audio.data.size());
  for (std::size_t i = 0; i < audio.data.size(); i++)
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
    AudioTexture& audio,
    QRhiResourceUpdateBatch& res,
    QRhiTexture* rhiTexture)
{
  if (m_scratchpad.size() < audio.data.size())
    m_scratchpad.resize(audio.data.size() / 2);
  std::size_t bufferSize = audio.data.size() / audio.channels;
  std::size_t fftSize = bufferSize / 2;
  const float norm = 1. / (2. * bufferSize);
  for (int i = 0; i < audio.channels; i++)
  {
    float* inputData = audio.data.data() + i * bufferSize;
    auto spectrum = m_fft.execute(inputData, bufferSize);

    float* outputSpectrum = m_scratchpad.data() + i * fftSize;
    for (std::size_t k = 0; k < fftSize; k++)
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

void AudioTextureUpload::updateAudioTexture(
    AudioTexture& audio,
    RenderList& renderer,
    QRhiResourceUpdateBatch& res,
    std::vector<Pass>& passes)
{
  QRhi& rhi = *renderer.state.rhi;
  bool textureChanged = false;
  auto it = audio.samplers.find(&renderer);
  if (it == audio.samplers.end())
    return;

  auto& [rhiSampler, rhiTexture] = it->second;
  const auto curSz = (rhiTexture) ? rhiTexture->pixelSize() : QSize{};
  int numSamples = curSz.width() * curSz.height();
  if (numSamples != int(audio.data.size()))
  {
    delete rhiTexture;
    rhiTexture = nullptr;
    textureChanged = true;
  }

  if (!rhiTexture)
  {
    if (audio.channels > 0)
    {
      int samples = audio.data.size() / audio.channels;
      int pixelWidth = samples / (audio.fft ? 2 : 1);

      m_fft.reset(samples);

      rhiTexture = rhi.newTexture(
          QRhiTexture::R32F,
          {pixelWidth, audio.channels},
          1,
          QRhiTexture::Flag{});
      rhiTexture->create();
      textureChanged = true;
    }
    else
    {
      rhiTexture = nullptr;
      textureChanged = true;
    }
  }

  if (textureChanged)
  {
    for (auto& pass : passes)
      score::gfx::replaceTexture(
          *pass.p.srb,
          rhiSampler,
          rhiTexture ? rhiTexture : &renderer.emptyTexture());
  }

  if (rhiTexture)
  {
    // Process the audio data
    this->process(audio, res, rhiTexture);
  }
}

#include <Gfx/Qt5CompatPop> // clang-format: keep
}
