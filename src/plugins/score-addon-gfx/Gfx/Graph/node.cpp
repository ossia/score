#include "node.hpp"

#include "graph.hpp"
#include "mesh.hpp"
#include "renderer.hpp"
#include <score/tools/Debug.hpp>
NodeModel::NodeModel() { }

void RenderedNode::createRenderTarget(const RenderState& state)
{
  auto sz = state.size;
  if (auto true_sz = renderTargetSize())
  {
    sz = *true_sz;
  }

  m_texture = state.rhi->newTexture(QRhiTexture::RGBA8, sz, 1, QRhiTexture::RenderTarget);
  m_texture->build();

  QRhiColorAttachment color0{m_texture};

  auto renderTarget = state.rhi->newTextureRenderTarget({color0});
  m_renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  ensure(m_renderPass);
  renderTarget->setRenderPassDescriptor(m_renderPass);
  ensure(renderTarget->build());

  this->m_renderTarget = renderTarget;
}

std::optional<QSize> RenderedNode::renderTargetSize() const noexcept
{
  return {};
}

void RenderedNode::customInit(Renderer& renderer) { }

void NodeModel::setShaders(QString vert, QString frag)
{
  QShaderBaker b;

  b.setGeneratedShaders({
      {QShader::SpirvShader, 100},
      {QShader::GlslShader, 330},
      {QShader::HlslShader, QShaderVersion(50)},
      {QShader::MslShader, QShaderVersion(12)},
  });
  b.setGeneratedShaderVariants(
      {QShader::Variant{}, QShader::Variant{}, QShader::Variant{}, QShader::Variant{}});

  b.setSourceString(vert.toLatin1(), QShader::VertexStage);
  m_vertexS = b.bake();
  if(!b.errorMessage().isEmpty()) qDebug() << b.errorMessage();

  b.setSourceString(frag.toLatin1(), QShader::FragmentStage);
  m_fragmentS = b.bake();
  if (!b.errorMessage().isEmpty())
  {
    qDebug() << b.errorMessage();
    qDebug() << frag.toStdString().data();
  }

  if (!m_vertexS.isValid())
    throw std::runtime_error("invalid vertex shader");
  if (!m_fragmentS.isValid())
    throw std::runtime_error("invalid fragment shader");
}

void NodeModel::setShaders(const QShader& vert, const QShader& frag)
{
  m_vertexS = vert;
  m_fragmentS = frag;
}

void RenderedNode::init(Renderer& renderer)
{
  auto& rhi = *renderer.state.rhi;

  auto& input = node.input;

  const auto& mesh = node.mesh();
  if (!m_meshBuffer)
  {
    auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh);
    m_meshBuffer = mbuffer;
    m_idxBuffer = ibuffer;
  }

  m_processUBO = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  m_processUBO->build();

  // Set up shader inputs
  {
    m_materialSize = 0;
    for (auto in : input)
    {
      switch (in->type)
      {
        case Types::Empty:
          break;
        case Types::Int:
        case Types::Float:
          m_materialSize += 4;
          break;
        case Types::Vec2:
          m_materialSize += 8;
          if (m_materialSize % 8 != 0)
            m_materialSize += 4;
          break;
        case Types::Vec3:
          while (m_materialSize % 16 != 0)
          {
            m_materialSize += 4;
          }
          m_materialSize += 12;
          break;
        case Types::Vec4:
          while (m_materialSize % 16 != 0)
          {
            m_materialSize += 4;
          }
          m_materialSize += 16;
          break;
        case Types::Image:
        {
          auto sampler = rhi.newSampler(
              QRhiSampler::Linear,
              QRhiSampler::Linear,
              QRhiSampler::None,
              QRhiSampler::ClampToEdge,
              QRhiSampler::ClampToEdge);
          ensure(sampler->build());

          QRhiTexture* texture = renderer.m_emptyTexture;
          if (!in->edges.empty())
            if (auto source_node = in->edges[0]->source->node)
              if (auto source_rd = source_node->renderedNodes[&renderer])
                if (auto tex = source_rd->m_texture)
                  texture = tex;

          m_samplers.push_back({sampler, texture});
          break;
        }
        case Types::Audio:
          break;
        case Types::Camera:
          m_materialSize += sizeof(ModelCameraUBO);
          break;
      }
    }

    if (m_materialSize > 0)
    {
      m_materialUBO
          = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
      ensure(m_materialUBO->build());
    }
  }

  customInit(renderer);
  // Build the pipeline
  {
    m_ps = rhi.newGraphicsPipeline();
    ensure(m_ps);

    QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
    premulAlphaBlend.enable = true;
    m_ps->setTargetBlends({premulAlphaBlend});

    m_ps->setSampleCount(1);

    m_ps->setDepthTest(false);
    m_ps->setDepthWrite(false);
    // m_ps->setCullMode(QRhiGraphicsPipeline::CullMode::Back);
    // m_ps->setFrontFace(QRhiGraphicsPipeline::FrontFace::CCW);

    m_ps->setShaderStages(
        {{QRhiShaderStage::Vertex, node.m_vertexS},
         {QRhiShaderStage::Fragment, node.m_fragmentS}});

    QRhiVertexInputLayout inputLayout;
    inputLayout.setBindings(mesh.vertexInputBindings.begin(), mesh.vertexInputBindings.end());
    inputLayout.setAttributes(
        mesh.vertexAttributeBindings.begin(), mesh.vertexAttributeBindings.end());
    m_ps->setVertexInputLayout(inputLayout);

    // Shader resource bindings
    m_srb = rhi.newShaderResourceBindings();
    ensure(m_srb);

    QVector<QRhiShaderResourceBinding> bindings;

    const auto bindingStages
        = QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage;

    {
      const auto rendererBinding
          = QRhiShaderResourceBinding::uniformBuffer(0, bindingStages, renderer.m_rendererUBO);
      bindings.push_back(rendererBinding);
    }

    {
      const auto standardUniformBinding
          = QRhiShaderResourceBinding::uniformBuffer(1, bindingStages, m_processUBO);
      bindings.push_back(standardUniformBinding);
    }

    // Bind materials
    if (m_materialUBO)
    {
      const auto materialBinding
          = QRhiShaderResourceBinding::uniformBuffer(2, bindingStages, m_materialUBO);
      bindings.push_back(materialBinding);
    }

    // Bind samplers
    int binding = 3;
    for (auto sampler : this->m_samplers)
    {
      assert(sampler.texture);
      bindings.push_back(QRhiShaderResourceBinding::sampledTexture(
          binding,
          QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
          sampler.texture,
          sampler.sampler));
      binding++;
    }
    m_srb->setBindings(bindings.begin(), bindings.end());
    ensure(m_srb->build());

    m_ps->setShaderResourceBindings(m_srb);

    ensure(m_renderPass);
    m_ps->setRenderPassDescriptor(this->m_renderPass);

    ensure(m_ps->build());
  }
}

void RenderedNode::customUpdate(Renderer& renderer, QRhiResourceUpdateBatch& res) { }

void RenderedNode::update(Renderer& renderer, QRhiResourceUpdateBatch& res)
{
  res.updateDynamicBuffer(m_processUBO, 0, sizeof(ProcessUBO), &this->node.standardUBO);

  if (m_materialUBO && m_materialSize > 0 && materialChangedIndex != node.materialChanged)
  {
    char* data = node.m_materialData.get();
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
    materialChangedIndex = node.materialChanged;
  }

  customUpdate(renderer, res);
}

void RenderedNode::customRelease(Renderer&) { }

void RenderedNode::releaseWithoutRenderTarget(Renderer& r)
{
  customRelease(r);

  for (auto sampler : m_samplers)
  {
    delete sampler.sampler;
    // texture isdeleted elsewxheree
  }
  m_samplers.clear();

  delete m_processUBO;
  m_processUBO = nullptr;

  delete m_materialUBO;
  m_materialUBO = nullptr;
  m_materialSize = 0;

  delete m_ps;
  m_ps = nullptr;

  delete m_srb;
  m_srb = nullptr;

  m_meshBuffer = nullptr;
}

void RenderedNode::runPass(
    Renderer& renderer,
    QRhiCommandBuffer& cb,
    QRhiResourceUpdateBatch& updateBatch)
{
  update(renderer, updateBatch);

  cb.beginPass(m_renderTarget, Qt::black, {1.0f, 0}, &updateBatch);
  {
    const auto sz = renderer.state.size;
    cb.setGraphicsPipeline(pipeline());
    cb.setShaderResources(resources());
    cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

    assert(this->m_meshBuffer);
    assert(this->m_meshBuffer->usage().testFlag(QRhiBuffer::VertexBuffer));
    node.mesh().setupBindings(*this->m_meshBuffer, this->m_idxBuffer, cb);

    cb.draw(node.mesh().vertexCount);
  }

  cb.endPass();
}

void RenderedNode::replaceTexture(QRhiSampler* sampler, QRhiTexture* newTexture)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(m_srb->cbeginBindings(), m_srb->cendBindings());
  for (QRhiShaderResourceBinding& b : tmp)
  {
    if (b.data()->type == QRhiShaderResourceBinding::Type::SampledTexture)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
      SCORE_ASSERT(b.data()->u.stex.count >= 1);
      if (b.data()->u.stex.texSamplers[0].sampler == sampler)
      {
        b.data()->u.stex.texSamplers[0].tex = newTexture;
      }
#else
      if (b.data()->u.stex.sampler == sampler)
      {
        b.data()->u.stex.tex = newTexture;
      }
#endif
    }
  }

  m_srb->release();
  m_srb->setBindings(tmp.begin(), tmp.end());
  m_srb->build();
}

void RenderedNode::release(Renderer& r)
{
  releaseWithoutRenderTarget(r);

  if (m_texture)
  {
    delete m_texture;
    m_texture = nullptr;

    delete m_renderPass;
    m_renderPass = nullptr;

    delete m_renderTarget;
    m_renderTarget = nullptr;
  }
}
