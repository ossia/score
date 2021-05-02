#include "node.hpp"

#include "filternode.hpp"
#include "graph.hpp"
#include "mesh.hpp"
#include "nodes.hpp"
#include "renderer.hpp"
#include "shadercache.hpp"

#include <score/tools/Debug.hpp>

ColorNode::~ColorNode() { }

ProductNode::~ProductNode() { }

NoiseNode::~NoiseNode() { }

FilterNode::~FilterNode() { }

NodeModel::NodeModel() { }

#include <Gfx/Qt5CompatPush> // clang-format: keep

TextureRenderTarget RenderedNode::createRenderTarget(const RenderState& state)
{
  auto sz = state.size;
  if (auto true_sz = renderTargetSize())
  {
    sz = *true_sz;
  }

  m_rt = score::gfx::createRenderTarget(state, QRhiTexture::RGBA8, sz);
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
          SCORE_ASSERT(sampler->create());

          m_samplers.push_back(
              {sampler, renderer.textureTargetForInputPort(*in)});
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
      m_materialUBO = rhi.newBuffer(
          QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
      SCORE_ASSERT(m_materialUBO->create());
    }
  }
}

void RenderedNode::init(Renderer& renderer)
{
  if(!m_rt.renderTarget)
    createRenderTarget(renderer.state);

  auto& rhi = *renderer.state.rhi;

  const auto& mesh = node.mesh();
  if (!m_meshBuffer)
  {
    auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh);
    m_meshBuffer = mbuffer;
    m_idxBuffer = ibuffer;
  }

  m_processUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  m_processUBO->create();

  customInit(renderer);

  if (!m_p.pipeline)
  {
    // Build the pipeline
    m_p = score::gfx::buildPipeline(
        renderer,
        node.mesh(),
        node.m_vertexS,
        node.m_fragmentS,
        m_rt,
        m_processUBO,
        m_materialUBO,
        m_samplers);
  }
}

void RenderedNode::customUpdate(
    Renderer& renderer,
    QRhiResourceUpdateBatch& res)
{
}

void RenderedNode::update(Renderer& renderer, QRhiResourceUpdateBatch& res)
{
  res.updateDynamicBuffer(
      m_processUBO, 0, sizeof(ProcessUBO), &this->node.standardUBO);

  if (m_materialUBO && m_materialSize > 0
      && materialChangedIndex != node.materialChanged)
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

score::gfx::NodeRenderer* NodeModel::createRenderer(Renderer& r) const noexcept
{
  return new RenderedNode{*this};
}

score::gfx::NodeRenderer::NodeRenderer() noexcept { }

score::gfx::NodeRenderer::~NodeRenderer() { }

#include <Gfx/Qt5CompatPop> // clang-format: keep
