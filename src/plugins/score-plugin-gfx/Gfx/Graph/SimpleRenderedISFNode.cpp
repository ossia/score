#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/SimpleRenderedISFNode.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

namespace score::gfx
{

SimpleRenderedISFNode::SimpleRenderedISFNode(const ISFNode& node) noexcept
    : score::gfx::NodeRenderer{node}
    , n{const_cast<ISFNode&>(node)}
{
}

void SimpleRenderedISFNode::updateInputTexture(const Port& input, QRhiTexture* tex)
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
    auto& sampl = m_inputSamplers[sampler_idx];
    if(sampl.texture != tex)
    {
      sampl.texture = tex;
      for(auto& [e, pass] : m_passes)
        if(pass.p.srb)
          score::gfx::replaceTexture(*pass.p.srb, sampl.sampler, tex);
    }
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
    const TextureRenderTarget& renderTarget, RenderList& renderer, Edge& edge)
{
  auto& model_passes = n.descriptor().passes;
  SCORE_ASSERT(model_passes.size() == 1);

  QRhi& rhi = *renderer.state.rhi;

  QRhiBuffer* pubo{};
  pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("SimpleRenderedISFNode::initPass::pubo");
  pubo->create();

  // Create the main pass
  try
  {
    auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);
    auto pip = score::gfx::buildPipeline(
        renderer, *m_mesh, v, s, renderTarget, pubo, m_materialUBO, allSamplers());
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

  // Create color and depth textures based on OUTPUTS declarations
  std::vector<QRhiTexture*> colorTextures;
  QRhiTexture* depthTex = nullptr;

  for(const auto& out : outputs)
  {
    if(out.type == "depth")
    {
      depthTex = rhi.newTexture(
          QRhiTexture::D32F, sz, 1,
          QRhiTexture::RenderTarget);
      depthTex->setName(("SimpleRenderedISFNode::MRT::depth::" + out.name).c_str());
      SCORE_ASSERT(depthTex->create());
    }
    else
    {
      auto* tex = rhi.newTexture(
          QRhiTexture::RGBA8, sz, 1,
          QRhiTexture::RenderTarget | QRhiTexture::UsedWithLoadStore);
      tex->setName(("SimpleRenderedISFNode::MRT::color::" + out.name).c_str());
      SCORE_ASSERT(tex->create());
      colorTextures.push_back(tex);
    }
  }

  if(colorTextures.empty())
    return;

  // Create the multi-attachment render target
  m_mrtRenderTarget = createRenderTarget(
      renderer.state,
      std::span<QRhiTexture* const>{colorTextures.data(), colorTextures.size()},
      depthTex,
      renderer.samples());

  // Create the pipeline and pass using this render target
  QRhiBuffer* pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("SimpleRenderedISFNode::initMRTPass::pubo");
  pubo->create();

  try
  {
    auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);
    auto pip = score::gfx::buildPipeline(
        renderer, *m_mesh, v, s, m_mrtRenderTarget, pubo, m_materialUBO, allSamplers());
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

void SimpleRenderedISFNode::initMRTBlitPasses(RenderList& renderer, QRhiResourceUpdateBatch& res)
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

  auto [vertexS, fragmentS] = score::gfx::makeShaders(renderer.state, blit_vs, blit_fs);

  // For each output port, create a blit pass for each downstream edge
  for(auto* output_port : n.output)
  {
    QRhiTexture* srcTex = textureForOutput(*output_port);
    if(!srcTex)
      continue;

    for(Edge* edge : output_port->edges)
    {
      auto rt = renderer.renderTargetForOutput(*edge);
      if(!rt.renderTarget)
        continue;

      QRhiSampler* sampler = renderer.state.rhi->newSampler(
          QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
          QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge);
      sampler->setName("SimpleRenderedISFNode::MRT::blitSampler");
      sampler->create();

      auto pip = score::gfx::buildPipeline(
          renderer, *m_mesh, vertexS, fragmentS, rt, nullptr, nullptr,
          std::array<Sampler, 1>{Sampler{sampler, srcTex}});

      if(pip.pipeline)
      {
        m_passes.emplace_back(edge, Pass{rt, pip, nullptr});
      }
      else
      {
        delete sampler;
      }
    }
  }
}

void SimpleRenderedISFNode::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
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

  // Create the material UBO
  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    m_materialUBO->setName("SimpleRenderedISFNode::init::m_materialUBO");
    SCORE_ASSERT(m_materialUBO->create());
  }

  // Create the samplers
  SCORE_ASSERT(m_passes.empty());
  SCORE_ASSERT(m_inputSamplers.empty());
  SCORE_ASSERT(m_audioSamplers.empty());

  m_inputSamplers = initInputSamplers(this->n, renderer, n.input);

  m_audioSamplers = initAudioTextures(renderer, n.m_audio_textures);

  // Create the passes
  // Count outputs to determine if we need MRT
  {
    const auto& outputs = n.descriptor().outputs;
    int colorCount = 0;
    bool hasDepth = false;
    for(const auto& out : outputs)
    {
      if(out.type == "depth")
        hasDepth = true;
      else
        colorCount++;
    }
    // MRT is only needed for multiple color attachments or depth output
    m_hasMRT = colorCount > 1 || hasDepth;
  }

  if(m_hasMRT)
  {
    // MRT: create internal render target, render in runInitialPasses,
    // then blit to downstream render targets in runRenderPass
    initMRTPass(renderer, res);

    // Create blit passes for each downstream edge across all output ports
    initMRTBlitPasses(renderer, res);
  }
  else
  {
    // Default single-output path (also handles OUTPUTS with a single color)
    if(n.output[0]->edges.empty())
      qDebug(" WTF EMPTY");
    for(Edge* edge : n.output[0]->edges)
    {
      auto rt = renderer.renderTargetForOutput(*edge);
      if(rt.renderTarget)
      {
        initPass(rt, renderer, *edge);
      }
      else
      {
        qDebug("WTF NO RT");
      }
    }
  }
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
  for(auto& audio : n.m_audio_textures)
  {
    if(std::optional<Sampler> sampl
       = m_audioTex->updateAudioTexture(audio, renderer, n.m_material_data.get(), res))
    {
      // Texture changed -> material changed
      audioChanged = true;

      auto& [rhiSampler, tex] = *sampl;
      for(auto& [e, pass] : m_passes)
      {
        score::gfx::replaceTexture(
            *pass.p.srb, rhiSampler, tex ? tex : &renderer.emptyTexture());
      }
    }
  }

  // Update material
  if(m_materialUBO && m_materialSize > 0 && (materialChanged || audioChanged))
  {
    char* data = n.m_material_data.get();
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
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
  // customRelease
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

  delete m_materialUBO;
  m_materialUBO = nullptr;

  m_meshBuffer = {}; // Freed in RenderList

  // Release MRT render target (textures are owned by us)
  if(m_hasMRT)
  {
    m_mrtRenderTarget.release();
    m_hasMRT = false;
  }
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
      pass.renderTarget.renderTarget, Qt::transparent, {1.0f, 0}, updateBatch);
  updateBatch = nullptr;

  cb.setGraphicsPipeline(pass.p.pipeline);
  cb.setShaderResources(pass.p.srb);

  auto* tex = pass.renderTarget.texture;
  cb.setViewport(QRhiViewport(
      0, 0, tex->pixelSize().width(), tex->pixelSize().height()));

  m_mesh->draw(this->m_meshBuffer, cb);

  cb.endPass();
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
  {
    qDebug(" NO PASS FOUND");
    return;
  }

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
      cb.setViewport(QRhiViewport(
          0, 0, texture->pixelSize().width(), texture->pixelSize().height()));

      m_mesh->draw(this->m_meshBuffer, cb);
    }
  }
}

SimpleRenderedISFNode::~SimpleRenderedISFNode() { }

}
