#include <Gfx/Graph/GeometryFilterNode.hpp>
#include <Gfx/Graph/GeometryFilterNodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/tools/Debug.hpp>
namespace score::gfx
{

GeometryFilterNodeRenderer::GeometryFilterNodeRenderer(
    const GeometryFilterNode& node) noexcept
    : score::gfx::NodeRenderer{}
    , n{const_cast<GeometryFilterNode&>(node)}
{
}

GeometryFilterNodeRenderer::~GeometryFilterNodeRenderer() { }

TextureRenderTarget GeometryFilterNodeRenderer::renderTargetForInput(const Port& p)
{
  return {};
}

void GeometryFilterNodeRenderer::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  QRhi& rhi = *renderer.state.rhi;

  m_materialSize = n.m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    SCORE_ASSERT(m_materialUBO->create());
  }
}

void GeometryFilterNodeRenderer::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  // Update material
  if(m_materialUBO
     && m_materialSize > 0) // && n.hasMaterialChanged(materialChangedIndex))
  {
    char* data = n.m_material_data.get();
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
  }
}

void GeometryFilterNodeRenderer::release(RenderList& r)
{
  delete m_materialUBO;
  m_materialUBO = nullptr;
}

void GeometryFilterNodeRenderer::runInitialPasses(
    RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res, Edge& edge)
{
  // nothing
}

void GeometryFilterNodeRenderer::runRenderPass(
    RenderList&, QRhiCommandBuffer& commands, Edge& edge)
{
  // nothing
}
}
