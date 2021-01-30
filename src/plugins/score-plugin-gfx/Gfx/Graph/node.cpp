#include "node.hpp"

#include "graph.hpp"
#include "mesh.hpp"
#include "renderer.hpp"
#include "shadercache.hpp"
#include <score/tools/Debug.hpp>
NodeModel::NodeModel() { }

namespace score::gfx
{
TextureRenderTarget createRenderTarget(const RenderState& state, QSize sz)
{
  TextureRenderTarget ret;
  ret.texture = state.rhi->newTexture(QRhiTexture::RGBA8, sz, 1, QRhiTexture::RenderTarget);
  ret.texture->build();

  QRhiColorAttachment color0{ret.texture};

  auto renderTarget = state.rhi->newTextureRenderTarget({color0});
  SCORE_ASSERT(renderTarget);

  auto renderPass = renderTarget->newCompatibleRenderPassDescriptor();
  SCORE_ASSERT(renderPass);

  renderTarget->setRenderPassDescriptor(renderPass);
  SCORE_ASSERT(renderTarget->build());

  ret.renderTarget = renderTarget;
  ret.renderPass = renderPass;
  return ret;
}

void replaceTexture(QRhiShaderResourceBindings& srb, QRhiSampler* sampler, QRhiTexture* newTexture)
{
  std::vector<QRhiShaderResourceBinding> tmp;
  tmp.assign(srb.cbeginBindings(), srb.cendBindings());
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

  srb.release();
  srb.setBindings(tmp.begin(), tmp.end());
  srb.build();
}

Pipeline buildPipeline(
      const Renderer& renderer,
      const Mesh& mesh,
      const QShader& vertexS, const QShader& fragmentS,
      const TextureRenderTarget& rt,
      QRhiBuffer* m_processUBO,
      QRhiBuffer* m_materialUBO,
      const std::vector<Sampler>& samplers)
{
  auto& rhi = *renderer.state.rhi;
  auto ps = rhi.newGraphicsPipeline();
  SCORE_ASSERT(ps);

  QRhiGraphicsPipeline::TargetBlend premulAlphaBlend;
  premulAlphaBlend.enable = true;
  ps->setTargetBlends({premulAlphaBlend});

  ps->setSampleCount(1);

  ps->setDepthTest(false);
  ps->setDepthWrite(false);
  // m_ps->setCullMode(QRhiGraphicsPipeline::CullMode::Back);
  // m_ps->setFrontFace(QRhiGraphicsPipeline::FrontFace::CCW);

  ps->setShaderStages(
        {{QRhiShaderStage::Vertex, vertexS},
         {QRhiShaderStage::Fragment, fragmentS}});

  QRhiVertexInputLayout inputLayout;
  inputLayout.setBindings(mesh.vertexInputBindings.begin(), mesh.vertexInputBindings.end());
  inputLayout.setAttributes(
        mesh.vertexAttributeBindings.begin(), mesh.vertexAttributeBindings.end());
  ps->setVertexInputLayout(inputLayout);

  // Shader resource bindings
  auto srb = rhi.newShaderResourceBindings();
  SCORE_ASSERT(srb);

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
  for (auto sampler : samplers)
  {
    assert(sampler.texture);
    bindings.push_back(QRhiShaderResourceBinding::sampledTexture(
                         binding,
                         QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                         sampler.texture,
                         sampler.sampler));
    binding++;
  }
  srb->setBindings(bindings.begin(), bindings.end());
  SCORE_ASSERT(srb->build());

  ps->setShaderResourceBindings(srb);

  SCORE_ASSERT(rt.renderPass);
  ps->setRenderPassDescriptor(rt.renderPass);

  SCORE_ASSERT(ps->build());
  return {ps, srb};
}
}

TextureRenderTarget RenderedNode::createRenderTarget(const RenderState& state)
{
  auto sz = state.size;
  if (auto true_sz = renderTargetSize())
  {
    sz = *true_sz;
  }

  m_rt = score::gfx::createRenderTarget(state, sz);
  return m_rt;
}

std::optional<QSize> RenderedNode::renderTargetSize() const noexcept
{
  return {};
}

void RenderedNode::customInit(Renderer& renderer)
{
  defaultShaderMaterialInit(renderer);
}

std::pair<QShader, QShader> makeShaders(QString vert, QString frag)
{
  auto [vertexS, vertexError] = ShaderCache::get(vert.toUtf8(), QShader::VertexStage);
  if(!vertexError.isEmpty())
      qDebug() << vertexError;

  auto [fragmentS, fragmentError] = ShaderCache::get(frag.toUtf8(), QShader::FragmentStage);
  if (!fragmentError.isEmpty())
  {
    qDebug() << fragmentError;
    qDebug() << frag.toStdString().data();
  }

  if (!vertexS.isValid())
    throw std::runtime_error("invalid vertex shader");
  if (!fragmentS.isValid())
    throw std::runtime_error("invalid fragment shader");

  return {vertexS, fragmentS};
}

QShader makeCompute(QString compute)
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

  b.setSourceString(compute.toLatin1(), QShader::ComputeStage);
  QShader shader = b.bake();

  if (!b.errorMessage().isEmpty())
  {
    qDebug() << b.errorMessage();
  }

  if (!shader.isValid())
    throw std::runtime_error("invalid compute shader");
  return shader;
}

void NodeModel::setShaders(const QShader& vert, const QShader& frag)
{
  m_vertexS = vert;
  m_fragmentS = frag;
}

void RenderedNode::defaultShaderMaterialInit(Renderer& renderer)
{
  auto& rhi = *renderer.state.rhi;

  auto& input = node.input;
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
          SCORE_ASSERT(sampler->build());

          m_samplers.push_back({sampler, renderer.textureTargetForInputPort(*in)});
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
      SCORE_ASSERT(m_materialUBO->build());
    }
  }
}

void RenderedNode::init(Renderer& renderer)
{
  auto& rhi = *renderer.state.rhi;

  const auto& mesh = node.mesh();
  if (!m_meshBuffer)
  {
    auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh);
    m_meshBuffer = mbuffer;
    m_idxBuffer = ibuffer;
  }

  m_processUBO = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  m_processUBO->build();

  customInit(renderer);

  if(!m_p.pipeline)
  {
    // Build the pipeline
    m_p = score::gfx::buildPipeline(renderer, node.mesh(), node.m_vertexS, node.m_fragmentS, m_rt, m_processUBO, m_materialUBO, m_samplers);
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

  m_p.release();

  m_meshBuffer = nullptr;
}

void RenderedNode::runPass(
    Renderer& renderer,
    QRhiCommandBuffer& cb,
    QRhiResourceUpdateBatch& updateBatch)
{
  update(renderer, updateBatch);

  cb.beginPass(m_rt.renderTarget, Qt::black, {1.0f, 0}, &updateBatch);
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


void RenderedNode::release(Renderer& r)
{
  releaseWithoutRenderTarget(r);
  m_rt.release();
}

NodeModel::~NodeModel() { }

score::gfx::NodeRenderer* NodeModel::createRenderer() const noexcept
{
  return new RenderedNode{*this};
}


score::gfx::NodeRenderer::NodeRenderer() noexcept
{

}

score::gfx::NodeRenderer::~NodeRenderer()
{

}
