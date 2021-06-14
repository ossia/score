#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/tools/Debug.hpp>

namespace score::gfx
{

#include <Gfx/Qt5CompatPush> // clang-format: keep

/*
TextureRenderTarget
GenericNodeRenderer::createRenderTarget(const RenderState& state)
{
  auto sz = state.size;
  if (auto true_sz = renderTargetSize())
  {
    sz = *true_sz;
  }

  m_rt = score::gfx::createRenderTarget(state, QRhiTexture::RGBA8, sz);
  m_rt.texture->setName("GenericNodeRenderer::m_rt.texture");
  m_rt.renderPass->setName("GenericNodeRenderer::m_rt.renderPass");
  m_rt.renderTarget->setName("GenericNodeRenderer::m_rt.renderTarget");
  return m_rt;
}

std::optional<QSize> GenericNodeRenderer::renderTargetSize() const noexcept
{
  return {};
}
*/

TextureRenderTarget GenericNodeRenderer::renderTargetForInput(const Port& p)
{
  SCORE_TODO;
  return { };
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
  // if (!m_rt.renderTarget)
  //   createRenderTarget(renderer.state);

  auto& rhi = *renderer.state.rhi;

  const auto& mesh = node.mesh();
  if (!m_meshBuffer)
  {
    auto [mbuffer, ibuffer] = renderer.initMeshBuffer(mesh);
    m_meshBuffer = mbuffer;
    m_idxBuffer = ibuffer;
    if(m_meshBuffer)
      m_meshBuffer->setName("GenericNodeRenderer::m_meshBuffer");
    if(m_idxBuffer)
      m_idxBuffer->setName("GenericNodeRenderer::m_idxBuffer");
  }

  m_processUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  m_processUBO->setName("GenericNodeRenderer::m_processUBO");
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
        renderer.renderTargetForOutput(*this->node.output[0]),
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

  // Update input textures
  /*

  {
    int sampler_i = 0;
    for (Port* in : this->node.input)
    {
      if (in->type == Types::Image)
      {
        auto new_texture = renderer.textureTargetForInputPort(*in);
        auto cur_texture = m_samplers[sampler_i].texture;
        if (new_texture != cur_texture)
        {
          score::gfx::replaceTexture(*this->m_p.srb, cur_texture, new_texture);
        }
        sampler_i++;
      }
    }
  }

  */
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
  //auto m_rt = renderer.renderTargetForOutput(*this->node.output[0]);
  //cb.beginPass(m_rt.renderTarget, Qt::black, {1.0f, 0}, &updateBatch);
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

  //cb.endPass();
}

void GenericNodeRenderer::release(RenderList& r)
{
  releaseWithoutRenderTarget(r);
  //m_rt.release();
}

score::gfx::NodeRenderer::NodeRenderer() noexcept { }

score::gfx::NodeRenderer::~NodeRenderer() { }

#include <Gfx/Qt5CompatPop> // clang-format: keep

}
