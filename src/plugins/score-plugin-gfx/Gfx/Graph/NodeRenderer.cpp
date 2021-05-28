#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/tools/Debug.hpp>

namespace score::gfx
{

#include <Gfx/Qt5CompatPush> // clang-format: keep

TextureRenderTarget
GenericNodeRenderer::createRenderTarget(const RenderState& state)
{
  auto sz = state.size;
  if (auto true_sz = renderTargetSize())
  {
    sz = *true_sz;
  }

  m_rt = score::gfx::createRenderTarget(state, QRhiTexture::RGBA8, sz);
  return m_rt;
}

std::optional<QSize> GenericNodeRenderer::renderTargetSize() const noexcept
{
  return {};
}

void GenericNodeRenderer::customInit(RenderList& renderer)
{
  m_material.init(renderer, node.input, m_samplers);
}

void NodeModel::setShaders(const QShader& vert, const QShader& frag)
{
  m_vertexS = vert;
  m_fragmentS = frag;
}

void GenericNodeRenderer::init(RenderList& renderer)
{
  if (!m_rt.renderTarget)
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
        m_material.buffer,
        m_samplers);
  }
}

void GenericNodeRenderer::customUpdate(
    RenderList& renderer,
    QRhiResourceUpdateBatch& res)
{
}

void GenericNodeRenderer::update(
    RenderList& renderer,
    QRhiResourceUpdateBatch& res)
{
  res.updateDynamicBuffer(
      m_processUBO, 0, sizeof(ProcessUBO), &this->node.standardUBO);

  if (m_material.buffer && m_material.size > 0
      && materialChangedIndex != node.materialChanged)
  {
    char* data = node.m_materialData.get();
    res.updateDynamicBuffer(m_material.buffer, 0, m_material.size, data);
    materialChangedIndex = node.materialChanged;
  }

  customUpdate(renderer, res);
}

void GenericNodeRenderer::customRelease(RenderList&) { }

void GenericNodeRenderer::releaseWithoutRenderTarget(RenderList& r)
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

  delete m_material.buffer;
  m_material.buffer = nullptr;

  m_p.release();

  m_meshBuffer = nullptr;
}

void GenericNodeRenderer::runPass(
    RenderList& renderer,
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

void GenericNodeRenderer::release(RenderList& r)
{
  releaseWithoutRenderTarget(r);
  m_rt.release();
}

score::gfx::NodeRenderer::NodeRenderer() noexcept { }

score::gfx::NodeRenderer::~NodeRenderer() { }

#include <Gfx/Qt5CompatPop> // clang-format: keep

}
