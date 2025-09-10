#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/RenderedVSANode.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

namespace score::gfx
{

static const constexpr auto quad_vertex_shader = R"_(#version 450
layout(location = 0) in vec2 position;
out gl_PerVertex { vec4 gl_Position; };

void main() {
  gl_Position = vec4(position, 0.0, 1.);
}
)_";

static const constexpr auto quad_fragment_shader = R"_(#version 450
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform bgcolor_t {
  vec4 color;
} bgcolor;
void main() {
  fragColor = bgcolor.color;
}
)_";
SimpleRenderedVSANode::SimpleRenderedVSANode(const ISFNode& node) noexcept
    : score::gfx::NodeRenderer{node}
    , n{const_cast<ISFNode&>(node)}
{
}

TextureRenderTarget SimpleRenderedVSANode::renderTargetForInput(const Port& p)
{
  SCORE_ASSERT(m_rts.find(&p) != m_rts.end());
  return m_rts[&p];
}

std::vector<Sampler> SimpleRenderedVSANode::allSamplers() const noexcept
{
  // Input ports
  std::vector<Sampler> samplers = m_inputSamplers;

  // Audio textures
  samplers.insert(samplers.end(), m_audioSamplers.begin(), m_audioSamplers.end());

  return samplers;
}

void SimpleRenderedVSANode::initPass(
    const TextureRenderTarget& renderTarget, RenderList& renderer, Edge& edge,
    QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  // Background color quad pass
  static const auto& bg_mesh = PlainTriangle::instance();
  QRhiGraphicsPipeline* bg_pip = rhi.newGraphicsPipeline();
  QRhiBuffer* bg_ubo
      = rhi.newBuffer(QRhiBuffer::Static, QRhiBuffer::UniformBuffer, 4 * sizeof(float));
  QRhiShaderResourceBindings* bg_srb = rhi.newShaderResourceBindings();
  MeshBuffers bg_tri;
  {
    bg_ubo->create();
    bg_srb->setBindings({QRhiShaderResourceBinding::uniformBuffer(
        0, QRhiShaderResourceBinding::FragmentStage, bg_ubo)});
    bg_srb->create();

    auto [v, f] = score::gfx::makeShaders(
        renderer.state, quad_vertex_shader, quad_fragment_shader);
    bg_pip->setShaderStages(
        {{QRhiShaderStage::Vertex, v}, {QRhiShaderStage::Fragment, f}});
    bg_pip->setRenderPassDescriptor(renderTarget.renderPass);
    bg_pip->setDepthTest(false);
    bg_pip->setDepthWrite(false);
    bg_pip->setSampleCount(renderer.samples());
    bg_mesh.preparePipeline(*bg_pip);
    bg_tri = renderer.initMeshBuffer(bg_mesh, res);
    bg_pip->setRenderPassDescriptor(renderTarget.renderPass);
    bg_pip->setShaderResourceBindings(bg_srb);

    SCORE_ASSERT(bg_pip->create());
  }

  // Main rendering
  auto& model_passes = n.descriptor().passes;
  SCORE_ASSERT(model_passes.size() == 1);

  QRhiBuffer* pubo{};
  pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("SimpleRenderedVSANode::initPass::pubo");
  pubo->create();

  // Create the main pass
  try
  {
    auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);
    auto pip = score::gfx::buildPipeline(
        renderer, *m_mesh, v, s, renderTarget, pubo, m_materialUBO, allSamplers());
    if(pip.pipeline)
    {
      QRhiGraphicsPipeline::TargetBlend t{};
      t.enable = true;
      pip.pipeline->destroy();
      pip.pipeline->setTargetBlends({t});
      pip.pipeline->create();
      m_passes.emplace_back(
          &edge, Pass{renderTarget, pip, pubo}, bg_pip, bg_srb, bg_ubo, bg_tri);
    }
    else
      delete pubo;
  }
  catch(...)
  {
  }
}

void SimpleRenderedVSANode::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  m_prevFormat = *(int64_t*)n.input[1]->value;
  // Create the mesh
  {
    auto m = new DummyMesh(3); // Set in update
    switch(m_prevFormat)
    {
      case 0:
        m->topology = QRhiGraphicsPipeline::Topology::Points;
        break;
      case 1:
        m->topology = QRhiGraphicsPipeline::Topology::LineStrip;
        break;
      case 2:
      case 3:
        m->topology = QRhiGraphicsPipeline::Topology::Lines;
        break;
      case 4:
        m->topology = QRhiGraphicsPipeline::Topology::TriangleStrip;
        break;
      case 5:
        m->topology = QRhiGraphicsPipeline::Topology::TriangleFan;
        break;
      case 6:
        m->topology = QRhiGraphicsPipeline::Topology::Triangles;
        break;
    }

    m_mesh = m;
  }

  // Create the material UBO
  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    m_materialUBO->setName("SimpleRenderedVSANode::init::m_materialUBO");
    SCORE_ASSERT(m_materialUBO->create());
  }

  // Create the samplers
  SCORE_ASSERT(m_rts.empty());
  SCORE_ASSERT(m_passes.empty());
  SCORE_ASSERT(m_inputSamplers.empty());
  SCORE_ASSERT(m_audioSamplers.empty());

  auto [samplers, cur_pos]
      = initInputSamplers(this->n, renderer, n.input, m_rts, n.m_material_data.get());
  m_inputSamplers = std::move(samplers);

  m_audioSamplers = initAudioTextures(renderer, n.m_audio_textures);

  // Create the passes

  for(Edge* edge : n.output[0]->edges)
  {
    auto rt = renderer.renderTargetForOutput(*edge);
    if(rt.renderTarget)
    {
      initPass(rt, renderer, *edge, res);
    }
  }
}

void SimpleRenderedVSANode::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  {
    SCORE_ASSERT(n.input.size() >= 2);
    const auto primitiveType = *(int64_t*)n.input[1]->value;
    if(primitiveType != m_prevFormat)
    {
      release(renderer);
      init(renderer, res);
    }
  }

  const auto count = *(float*)n.input[0]->value;
  static_cast<DummyMesh*>(m_mesh)->vertexCount = count;

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
      for(auto& pass : m_passes)
      {
        score::gfx::replaceTexture(
            *pass.main_pass.p.srb, rhiSampler, tex ? tex : &renderer.emptyTexture());
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
  for(auto& pass : m_passes)
  {
    float color[4]{
        (float)n.m_descriptor.background_color[0],
        (float)n.m_descriptor.background_color[1],
        (float)n.m_descriptor.background_color[2],
        (float)n.m_descriptor.background_color[3]};
    res.updateDynamicBuffer(pass.background_ubo, 0, 4 * sizeof(float), color);
    res.updateDynamicBuffer(
        pass.main_pass.processUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
  }
}

void SimpleRenderedVSANode::release(RenderList& r)
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

    for(auto& pass : m_passes)
    {
      pass.main_pass.p.release();

      if(pass.main_pass.processUBO)
        pass.main_pass.processUBO->deleteLater();

      pass.background_pipeline->destroy();
      pass.background_pipeline->deleteLater();

      pass.background_srb->destroy();
      pass.background_srb->deleteLater();

      pass.background_ubo->destroy();
      pass.background_ubo->deleteLater();
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

  delete m_mesh;
  m_mesh = nullptr;
}

void SimpleRenderedVSANode::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& updateBatch,
    Edge& edge)
{
}

void SimpleRenderedVSANode::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.edge == &edge; });
  // Maybe the shader could not be created
  if(it == this->m_passes.end())
    return;

  auto& pass = it->main_pass;
  auto texture = pass.renderTarget.texture;
  /*
  // Draw the background color
  {
    auto pipeline = it->background_pipeline;

    cb.setGraphicsPipeline(pipeline);
    cb.setShaderResources(it->background_srb);
    cb.setViewport(
        QRhiViewport(0, 0, texture->pixelSize().width(), texture->pixelSize().height()));

    const QRhiCommandBuffer::VertexInput bindings[] = {{it->background_tri.mesh, 0}};

    cb.setVertexInput(0, 1, bindings, 0);
    cb.draw(3);
  }*/
  // Draw the last pass
  {

    {
      SCORE_ASSERT(pass.renderTarget.renderTarget);
      SCORE_ASSERT(pass.p.pipeline);
      SCORE_ASSERT(pass.p.srb);
      // TODO : combine all the uniforms..

      auto pipeline = pass.p.pipeline;
      auto srb = pass.p.srb;

      // TODO need to free stuff
      {
        cb.setGraphicsPipeline(pipeline);
        cb.setShaderResources(srb);
        cb.setViewport(QRhiViewport(
            0, 0, texture->pixelSize().width(), texture->pixelSize().height()));

        m_mesh->draw({}, cb);
      }
    }
  }
}

SimpleRenderedVSANode::~SimpleRenderedVSANode() { }

}
