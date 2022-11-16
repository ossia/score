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
  // 2 things to separate:
  // - A mesh's geometry can change because it's a kind of dynamic mesh
  //   which gets changed live
  // - The mesh did not change but we must update it here
  //   because e.g. the connected node changed
  // Uploaded meshes must be stored in the renderer.
  // We know they can be freed when shared_ptr of mesh goes to zero

  // Note: idea for maintaining consistency between engine and UI thread:
  // adding markers that indicate the messages for a frame.
  // e.g. special message "frame 2353 messages start .... 2353 end" and a variable that indicates
  // the last fully written frame so that wwith peek() we can check that we are going to get only
  // the messages relevant for a frame.
  // Or... just put all of one frame's message in one vector and push that one at the end of the audio frame.
  if(node.hasGeometryChanged(geometryChangedIndex) && node.geometry)
  {
    std::tie(m_mesh, m_meshbufs) = renderer.acquireMesh(node.geometry, res);
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
