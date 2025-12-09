#include <Gfx/Graph/GeometryFilterNode.hpp>
#include <Gfx/Graph/GeometryFilterNodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/tools/Debug.hpp>
namespace score::gfx
{

GeometryFilterNodeRenderer::GeometryFilterNodeRenderer(
    const GeometryFilterNode& node) noexcept
    : score::gfx::NodeRenderer{node}
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

  m_materialSize = node().m_materialSize;
  if(m_materialSize > 0)
  {
    m_materialUBO
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, m_materialSize);
    m_materialUBO->setName("GeometryFilterNodeRenderer.ubo");
    SCORE_ASSERT(m_materialUBO->create());
  }
}

void GeometryFilterNodeRenderer::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  // Update material
  if(m_materialUBO
     && m_materialSize > 0) // && n.hasMaterialChanged(materialChangedIndex))
  {
    char* data = node().m_material_data.get();
    res.updateDynamicBuffer(m_materialUBO, 0, m_materialSize, data);
  }
}

void GeometryFilterNodeRenderer::release(RenderList& r)
{
  delete m_materialUBO;
  m_materialUBO = nullptr;
}

void GeometryFilterNodeRenderer::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
    Edge& edge)
{
  // Update geometry & transform on the next nodes

  // 2. Push to next node
  // FIXME this should be for the renderer of edge, not the node, since
  // geometries can have gpu buffers

  auto& parent = this->node();
  auto edge_sink = edge.sink;
  if(auto pnode = dynamic_cast<score::gfx::ProcessNode*>(edge_sink->node))
  {

    auto rendered_node = pnode->renderedNodes.find(&renderer);
    SCORE_ASSERT(rendered_node != pnode->renderedNodes.end());

    auto it = std::find(
        edge_sink->node->input.begin(), edge_sink->node->input.end(), edge_sink);
    SCORE_ASSERT(it != edge_sink->node->input.end());
    int n = it - edge_sink->node->input.begin();

    outputGeometry.meshes = geometry.meshes;

    if(!outputGeometry.filters)
    {
      outputGeometry.filters = std::make_shared<ossia::geometry_filter_list>();
    }
    if(geometry.filters)
    {
      outputGeometry.filters->filters.assign(
          geometry.filters->filters.begin(), geometry.filters->filters.end());
    }
    else
    {
      outputGeometry.filters->filters.clear();
    }

    outputGeometry.filters->filters.push_back(
        ossia::geometry_filter{this->id, parent.m_index, parent.m_shader, 1});

    rendered_node->second->process(n, this->outputGeometry);

    // 3. Same for transform3d

    {
      auto parent_tform_idx = parent.m_dirtyTransformIndex;
      if(this->m_dirtyTransformIndex != parent_tform_idx)
      {
        this->m_dirtyTransformIndex = parent_tform_idx;
        pnode->process(n, parent.m_transform);
      }
    }
  }
}

void GeometryFilterNodeRenderer::runRenderPass(
    RenderList&, QRhiCommandBuffer& commands, Edge& edge)
{
  // nothing
}

GeometryFilterNode& GeometryFilterNodeRenderer::node() const noexcept
{
  auto& nc = const_cast<score::gfx::Node&>(score::gfx::NodeRenderer::node);
  return static_cast<GeometryFilterNode&>(nc);
}
}
