#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/RenderedRawRasterPipelineNode.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

namespace score::gfx
{

RenderedRawRasterPipelineNode::RenderedRawRasterPipelineNode(
    const ISFNode& node) noexcept
    : score::gfx::NodeRenderer{node}
    , n{const_cast<ISFNode&>(node)}
{
}

TextureRenderTarget RenderedRawRasterPipelineNode::renderTargetForInput(const Port& p)
{
  SCORE_ASSERT(m_rts.find(&p) != m_rts.end());
  return m_rts[&p];
}

std::vector<Sampler> RenderedRawRasterPipelineNode::allSamplers() const noexcept
{
  // Input ports
  std::vector<Sampler> samplers = m_inputSamplers;

  // Audio textures
  samplers.insert(samplers.end(), m_audioSamplers.begin(), m_audioSamplers.end());

  return samplers;
}

void RenderedRawRasterPipelineNode::initPass(
    const TextureRenderTarget& renderTarget, RenderList& renderer, Edge& edge)
{
  auto& model_passes = n.descriptor().passes;
  SCORE_ASSERT(model_passes.size() == 1);

  QRhi& rhi = *renderer.state.rhi;

  QRhiBuffer* pubo{};
  pubo = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  pubo->setName("RenderedRawRasterPipelineNode::initPass::pubo");
  pubo->create();

  // Create the main pass
  try
  {
    auto [v, s] = score::gfx::makeShaders(renderer.state, n.m_vertexS, n.m_fragmentS);

    auto& mat
        = *reinterpret_cast<PipelineChangingMaterial*>(m_prevPipelineChangingMaterial);

    int max_binding = 3;
    auto samplers = allSamplers();
    if(!samplers.empty())
      max_binding += samplers.size();
    auto model_ubo_binding = QRhiShaderResourceBinding::uniformBuffer(
        max_binding,
        QRhiShaderResourceBinding::StageFlag::VertexStage
            | QRhiShaderResourceBinding::StageFlag::FragmentStage,
        m_modelUBO);
    auto bindings = createDefaultBindings(
        renderer, renderTarget, pubo, m_materialUBO, allSamplers(),
        std::span<QRhiShaderResourceBinding>(&model_ubo_binding, 1));

    auto& rhi = *renderer.state.rhi;
    auto ps = rhi.newGraphicsPipeline();
    ps->setName("RenderedRawRasterPipelineNode::initPass::ps");
    SCORE_ASSERT(ps);

    QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
    premulAlphaBlend.enable = mat.enable_blend;
    premulAlphaBlend.srcColor = mat.src_color;
    premulAlphaBlend.dstColor = mat.dst_color;
    premulAlphaBlend.opColor = mat.op_color;
    premulAlphaBlend.srcAlpha = mat.src_alpha;
    premulAlphaBlend.dstAlpha = mat.dst_alpha;
    premulAlphaBlend.opAlpha = mat.op_alpha;
    ps->setTargetBlends({premulAlphaBlend});
    switch(mat.mode)
    {
      default:
      case 0:
        ps->setTopology(QRhiGraphicsPipeline::Triangles);
        break;
      case 1:
        ps->setTopology(QRhiGraphicsPipeline::Points);
        break;
      case 2:
        ps->setTopology(QRhiGraphicsPipeline::Lines);
        break;
    }

    ps->setSampleCount(renderer.samples());

    m_mesh->preparePipeline(*ps);

    // Check compatibility of shader and mesh
    {
      const auto& mesh_vars = ps->vertexInputLayout();
      const int mesh_bindings = mesh_vars.bindingCount();
      for(const auto& shader_var : v.description().inputVariables())
      {
        bool found = false;
        for(int i = 0; i < mesh_vars.attributeCount(); i++)
        {
          const auto& attr = mesh_vars.attributeAt(i);
          if(attr->location() == shader_var.location)
          {
            if(attr->binding() >= 0 && attr->binding() < mesh_bindings)
            {
              found = true;
              break;
            }
          }
        }
        if(!found)
        {
          delete ps;
          delete pubo;
          return;
        }
      }
    }

    ps->setDepthTest(true);
    ps->setDepthWrite(true);

    ps->setShaderStages({{QRhiShaderStage::Vertex, v}, {QRhiShaderStage::Fragment, s}});

    ps->setShaderResourceBindings(bindings);

    SCORE_ASSERT(renderTarget.renderPass);
    ps->setRenderPassDescriptor(renderTarget.renderPass);

    if(!ps->create())
    {
      qDebug() << "Warning! Pipeline not created";
      delete ps;
      ps = nullptr;
    }

    Pipeline pip = {ps, bindings};
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

void RenderedRawRasterPipelineNode::init(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  // Create the mesh
  {
    if(geometry.meshes)
    {
      std::tie(m_mesh, m_meshbufs)
          = renderer.acquireMesh(geometry, res, m_mesh, m_meshbufs);
    }
    else
    {
      if(m_mesh)
      {
        if(m_meshbufs.buffers.empty())
        {
          m_meshbufs = renderer.initMeshBuffer(*m_mesh, res);
        }
      }
    }
  }

  // Create the material UBO
  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    m_materialUBO->setName("RenderedRawRasterPipelineNode::init::m_materialUBO");
    SCORE_ASSERT(m_materialUBO->create());
  }

  m_modelUBO
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(float[16]));
  m_modelUBO->setName("RenderedRawRasterPipelineNode::init::m_modelUBO");
  SCORE_ASSERT(m_modelUBO->create());

  // Create the samplers
  SCORE_ASSERT(m_rts.empty());
  SCORE_ASSERT(m_passes.empty());
  SCORE_ASSERT(m_inputSamplers.empty());
  SCORE_ASSERT(m_audioSamplers.empty());

  m_inputSamplers = initInputSamplers(this->n, renderer, n.input, m_rts);

  m_audioSamplers = initAudioTextures(renderer, n.m_audio_textures);

  if(!m_mesh)
    return;

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

bool RenderedRawRasterPipelineNode::updateMaterials(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  bool mustRecreatePasses = false;
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
    SCORE_ASSERT(m_materialSize > size_of_pipeline_material);
    if(std::memcmp(data, this->m_prevPipelineChangingMaterial, size_of_pipeline_material)
       != 0)
    {
      mustRecreatePasses = true;
      std::copy_n(data, size_of_pipeline_material, this->m_prevPipelineChangingMaterial);
    }
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
  }
  return mustRecreatePasses;
}

void RenderedRawRasterPipelineNode::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  // Update node materials. This must be before any initial return,
  // otherwise we miss the materialsChanged
  bool mustRecreatePasses = updateMaterials(renderer, res, edge);

  // Update the geometry (sync with ModelDisplayNode)

  if(this->geometryChanged)
  {
    if(geometry.meshes)
    {
      std::tie(m_mesh, m_meshbufs)
          = renderer.acquireMesh(geometry, res, m_mesh, m_meshbufs);

      this->meshChangedIndex = this->m_mesh->dirtyGeometryIndex;
    }
    mustRecreatePasses = true;
    this->geometryChanged = false;
  }

  if(!m_mesh)
    return;

  // FIXME is that neeeded?
  // FIXME also not handling geometry_filter dirty geom so far
  if(m_mesh->hasGeometryChanged(meshChangedIndex))
  {
    mustRecreatePasses = true;
  }

  if(mustRecreatePasses)
  {
    for(auto& pass : m_passes)
      pass.second.p.release();
    m_passes.clear();

    for(Edge* edge : n.output[0]->edges)
    {
      auto rt = renderer.renderTargetForOutput(*edge);
      if(rt.renderTarget)
      {
        initPass(rt, renderer, *edge);
      }
    }
  }

  n.standardUBO.passIndex = 0;
  n.standardUBO.frameIndex++;
  auto sz = renderer.renderSize(edge);
  n.standardUBO.renderSize[0] = sz.width();
  n.standardUBO.renderSize[1] = sz.height();

  // Update all the process UBOs
  for(auto& [e, pass] : m_passes)
  {
    res.updateDynamicBuffer(
        pass.processUBO, 0, sizeof(ProcessUBO), &this->n.standardUBO);
  }

  res.updateDynamicBuffer(m_modelUBO, 0, sizeof(float[16]), m_modelTransform.matrix);
}

void RenderedRawRasterPipelineNode::release(RenderList& r)
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
      {
        pass.processUBO->deleteLater();
      }
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

  delete m_modelUBO;
  m_modelUBO = nullptr;
}

void RenderedRawRasterPipelineNode::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& cb, QRhiResourceUpdateBatch*& updateBatch,
    Edge& edge)
{
}

void RenderedRawRasterPipelineNode::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  auto it = ossia::find_if(this->m_passes, [&](auto& p) { return p.first == &edge; });
  // Maybe the shader could not be created
  if(it == this->m_passes.end())
    return;
  if(!m_mesh)
    return;
  if(this->m_meshbufs.buffers.empty())
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
      cb.setViewport(QRhiViewport(
          0, 0, texture->pixelSize().width(), texture->pixelSize().height()));

      m_mesh->draw(this->m_meshbufs, cb);
    }
  }
}

void RenderedRawRasterPipelineNode::process(int32_t port, const ossia::transform3d& v)
{
  m_modelTransform = v;
}

RenderedRawRasterPipelineNode::~RenderedRawRasterPipelineNode() { }

}
