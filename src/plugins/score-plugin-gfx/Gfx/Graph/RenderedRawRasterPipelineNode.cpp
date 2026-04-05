#include <Gfx/Graph/RenderedISFSamplerUtils.hpp>
#include <Gfx/Graph/RenderedRawRasterPipelineNode.hpp>
#include <Gfx/Graph/Utils.hpp>

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

void RenderedRawRasterPipelineNode::updateInputTexture(const Port& input, QRhiTexture* tex)
{
  // Find which image-type sampler index this port corresponds to
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
  qWarning() << "RRP ALLOC [processUBO] size=" << sizeof(ProcessUBO);
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

    ps->setSampleCount(renderer.samples());

    m_mesh->preparePipeline(*ps);

    // Override topology and blend after preparePipeline,
    // since the mesh may set its own defaults (e.g. CSF geometry outputs as points)
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

    // Remap vertex inputs by semantic: match shader input variable names
    // to geometry attribute semantics.
    if(auto* geom = m_mesh->semanticGeometry())
    {
      if(!remapPipelineVertexInputs(*ps, v, *geom))
      {
        qDebug() << "RawRaster::initPass: remapPipelineVertexInputs FAILED";
        delete ps;
        delete pubo;
        return;
      }
      qDebug() << "RawRaster::initPass: remapPipelineVertexInputs OK";
    }
    else
    {
      qDebug() << "RawRaster::initPass: no semanticGeometry";
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
    qWarning() << "RRP ALLOC [materialUBO] size=" << m_materialSize;
    m_materialUBO->setName("RenderedRawRasterPipelineNode::init::m_materialUBO");
    SCORE_ASSERT(m_materialUBO->create());
  }

  m_modelUBO
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(float[16]));
  qWarning() << "RRP ALLOC [modelUBO] size=" << sizeof(float[16]);
  m_modelUBO->setName("RenderedRawRasterPipelineNode::init::m_modelUBO");
  SCORE_ASSERT(m_modelUBO->create());

  // Create the samplers
  SCORE_ASSERT(m_passes.empty());
  SCORE_ASSERT(m_inputSamplers.empty());
  SCORE_ASSERT(m_audioSamplers.empty());

  m_inputSamplers = initInputSamplers(this->n, renderer, n.input);

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
    SCORE_ASSERT(m_materialSize >= size_of_pipeline_material);
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
  bool recreateDueToMaterial = mustRecreatePasses;

  // Update the geometry (sync with ModelDisplayNode)

  if(this->geometryChanged)
  {
    if(geometry.meshes)
    {
      const Mesh* prevMesh = m_mesh;
      std::tie(m_mesh, m_meshbufs)
          = renderer.acquireMesh(geometry, res, m_mesh, m_meshbufs);

      this->meshChangedIndex = this->m_mesh->dirtyGeometryIndex;

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
      // Check for standalone indirect draw buffer from Buffer input ports
      if(!m_meshbufs.useIndirectDraw)
      {
        for(auto* port : n.input)
        {
          if(port->type == Types::Buffer && !port->edges.empty())
          {
            auto bv = renderer.bufferForInput(*port->edges.front());
            if(bv.usage == BufferView::Usage::IndirectDraw)
            {
              m_meshbufs.indirectDrawBuffer = bv.handle;
              m_meshbufs.useIndirectDraw = true;
              m_meshbufs.indirectDrawIndexed = false;
              break;
            }
            else if(bv.usage == BufferView::Usage::IndirectDrawIndexed)
            {
              m_meshbufs.indirectDrawBuffer = bv.handle;
              m_meshbufs.useIndirectDraw = true;
              m_meshbufs.indirectDrawIndexed = true;
              break;
            }
          }
        }
      }
#endif

      // Only recreate passes when the mesh object itself changed (different
      // vertex layout / topology). When the same mesh is reused with updated
      // buffer contents (e.g. feedback ping-pong), the existing pipeline is
      // still valid — acquireMesh already updated the buffers in place.
      if(m_mesh != prevMesh || m_passes.empty())
        mustRecreatePasses = true;
    }
    else
    {
      // Geometry removed — need to recreate
      mustRecreatePasses = true;
    }
    this->geometryChanged = false;
  }

  bool recreateDueToGeometry = mustRecreatePasses && !recreateDueToMaterial;

  if(!m_mesh)
  {
    qDebug() << "RawRaster::update: no mesh!";
    return;
  }

  // FIXME is that neeeded?
  // FIXME also not handling geometry_filter dirty geom so far
  bool meshDirty = m_mesh->hasGeometryChanged(meshChangedIndex);
  if(meshDirty)
  {
    mustRecreatePasses = true;
  }

  if(mustRecreatePasses)
  {
    qWarning() << "RRP: recreating passes:"
               << "material=" << recreateDueToMaterial
               << "geometryChanged=" << recreateDueToGeometry
               << "meshDirty=" << meshDirty;
    for(auto& pass : m_passes)
    {
      pass.second.p.release();
      delete pass.second.processUBO;
    }
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
