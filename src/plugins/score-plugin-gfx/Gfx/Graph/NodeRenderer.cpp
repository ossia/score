#include <Gfx/Graph/CustomMesh.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

namespace score::gfx
{

#include <Gfx/Qt5CompatPush> // clang-format: keep

void defaultPassesInit(
    PassMap& passes, const std::vector<Edge*>& edges, RenderList& renderer,
    const Mesh& mesh, const QShader& v, const QShader& f, QRhiBuffer* processUBO,
    QRhiBuffer* matUBO, const std::vector<Sampler>& samplers)
{
  SCORE_ASSERT(passes.empty());
  for(Edge* edge : edges)
  {
    auto rt = renderer.renderTargetForOutput(*edge);
    if(rt.renderTarget)
    {
      auto pip = score::gfx::buildPipeline(
          renderer, mesh, v, f, rt, processUBO, matUBO, samplers);
      if(pip.pipeline)
        passes.emplace_back(edge, pip);
    }
  }
}

void defaultRenderPass(
    RenderList& renderer, const Mesh& mesh, const MeshBuffers& bufs,
    QRhiCommandBuffer& cb, Edge& edge, PassMap& passes)
{
  auto it
      = ossia::find_if(passes, [ptr = &edge](const auto& p) { return p.first == ptr; });
  if(it != passes.end())
  {
    const auto sz = renderer.state.renderSize;

    cb.setGraphicsPipeline(it->second.pipeline);
    cb.setShaderResources(it->second.srb);
    cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

    mesh.draw(bufs, cb);
  }
  else
  {
    qDebug() << "Could not find matching pipeline for draw";
  }
}

void quadRenderPass(
    RenderList& renderer, const MeshBuffers& bufs, QRhiCommandBuffer& cb, Edge& edge,
    PassMap& passes)
{
  auto it
      = ossia::find_if(passes, [ptr = &edge](const auto& p) { return p.first == ptr; });
  SCORE_ASSERT(it != passes.end());
  {
    const auto sz = renderer.state.renderSize;
    cb.setGraphicsPipeline(it->second.pipeline);
    cb.setShaderResources(it->second.srb);
    cb.setViewport(QRhiViewport(0, 0, sz.width(), sz.height()));

    const auto& mesh = renderer.defaultQuad();
    mesh.draw(bufs, cb);
  }
}

TextureRenderTarget GenericNodeRenderer::renderTargetForInput(const Port& p)
{
  SCORE_TODO;
  return {};
}

void GenericNodeRenderer::defaultMeshInit(
    RenderList& renderer, const Mesh& mesh, QRhiResourceUpdateBatch& res)
{
  m_mesh = &mesh;
  if(!m_meshbufs.mesh)
  {
    m_meshbufs = renderer.initMeshBuffer(mesh, res);
  }
}

void GenericNodeRenderer::processUBOInit(RenderList& renderer)
{
  auto& rhi = *renderer.state.rhi;
  m_processUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ProcessUBO));
  m_processUBO->setName("GenericNodeRenderer::m_processUBO");
  m_processUBO->create();
}

void GenericNodeRenderer::defaultPassesInit(RenderList& renderer, const Mesh& mesh)
{
  score::gfx::defaultPassesInit(
      m_p, this->node.output[0]->edges, renderer, mesh, m_vertexS, m_fragmentS,
      m_processUBO, m_material.buffer, m_samplers);
}

void GenericNodeRenderer::defaultPassesInit(
    RenderList& renderer, const Mesh& mesh, const QShader& v, const QShader& f)
{
  score::gfx::defaultPassesInit(
      m_p, this->node.output[0]->edges, renderer, mesh, v, f, m_processUBO,
      m_material.buffer, m_samplers);
}

void GenericNodeRenderer::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  m_mesh = &renderer.defaultTriangle();
  auto& mesh = *m_mesh;
  defaultMeshInit(renderer, mesh, res);
  processUBOInit(renderer);

  m_material.init(renderer, node.input, m_samplers);

  defaultPassesInit(renderer, mesh);
}

void GenericNodeRenderer::defaultUBOUpdate(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  res.updateDynamicBuffer(m_processUBO, 0, sizeof(ProcessUBO), &this->node.standardUBO);

  if(m_material.buffer && m_material.size > 0)
  {
    if(node.hasMaterialChanged(materialChangedIndex))
    {
      char* data = node.m_materialData.get();
      res.updateDynamicBuffer(m_material.buffer, 0, m_material.size, data);
    }
  }
}

void GenericNodeRenderer::defaultMeshUpdate(
    RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  if(node.hasGeometryChanged(geometryChangedIndex))
  {
    if(auto m = const_cast<CustomMesh*>(dynamic_cast<const CustomMesh*>(m_mesh)))
    {
      m->reload(*node.geometry);
      m_mesh->update(m_meshbufs, res);
      // m_meshbufs = renderer.updateDynamicMeshBuffer(*m_mesh, res);
    }
    else
    {
      m_mesh = new CustomMesh{*node.geometry};
      m_meshbufs = renderer.initMeshBuffer(*m_mesh, res);
    }
  }
}

void GenericNodeRenderer::update(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  defaultMeshUpdate(renderer, res);
  defaultUBOUpdate(renderer, res);
}

void GenericNodeRenderer::defaultRelease(RenderList&)
{
  for(auto sampler : m_samplers)
  {
    delete sampler.sampler;
    // texture isdeleted elsewxheree
  }
  m_samplers.clear();

  delete m_processUBO;
  m_processUBO = nullptr;

  delete m_material.buffer;
  m_material.buffer = nullptr;

  for(auto& pass : m_p)
    pass.second.release();
  m_p.clear();

  // FIXME Check that they get released?
  // We should have a refcount for this
  m_meshbufs = {};
}

void NodeRenderer::runInitialPasses(
    RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res, Edge& e)
{
}

void NodeRenderer::runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) {
}

void GenericNodeRenderer::defaultRenderPass(
    RenderList& renderer, const Mesh& mesh, QRhiCommandBuffer& cb, Edge& edge)
{
  defaultRenderPass(renderer, mesh, cb, edge, m_p);
}

void GenericNodeRenderer::defaultRenderPass(
    RenderList& renderer, const Mesh& mesh, QRhiCommandBuffer& cb, Edge& edge,
    PassMap& passes)
{
  score::gfx::defaultRenderPass(renderer, mesh, m_meshbufs, cb, edge, passes);
}

void GenericNodeRenderer::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  const auto& mesh = renderer.defaultTriangle();
  defaultRenderPass(renderer, mesh, cb, edge);
}

void GenericNodeRenderer::release(RenderList& r)
{
  defaultRelease(r);
}

score::gfx::NodeRenderer::NodeRenderer() noexcept { }

score::gfx::NodeRenderer::~NodeRenderer() { }

void NodeRenderer::inputAboutToFinish(
    RenderList& renderer, const Port& p, QRhiResourceUpdateBatch*&)
{
}

#include <Gfx/Qt5CompatPop> // clang-format: keep

}
