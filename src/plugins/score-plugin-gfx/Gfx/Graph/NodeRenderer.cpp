#include <Gfx/Graph/CustomMesh.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <score/tools/Debug.hpp>

#include <ossia/detail/algorithms.hpp>

namespace score::gfx
{

void defaultPassesInit(
    PassMap& passes, const std::vector<Edge*>& edges, RenderList& renderer,
    const Mesh& mesh, const QShader& v, const QShader& f, QRhiBuffer* processUBO,
    QRhiBuffer* matUBO, std::span<const Sampler> samplers,
    std::span<QRhiShaderResourceBinding> additionalBindings)
{
  SCORE_ASSERT(passes.empty());
  for(Edge* edge : edges)
  {
    auto rt = renderer.renderTargetForOutput(*edge);
    if(rt.renderTarget)
    {
      auto pip = score::gfx::buildPipeline(
          renderer, mesh, v, f, rt, processUBO, matUBO, samplers, additionalBindings);
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
    const auto sz = renderer.renderSize(&edge);
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
    const auto sz = renderer.renderSize(&edge);
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
  if(this->node.output[0]->type == score::gfx::Types::Image)
  {
    score::gfx::defaultPassesInit(
        m_p, this->node.output[0]->edges, renderer, mesh, m_vertexS, m_fragmentS,
        m_processUBO, m_material.buffer, m_samplers);
  }
}

void GenericNodeRenderer::defaultPassesInit(
    RenderList& renderer, const Mesh& mesh, const QShader& v, const QShader& f,
    std::span<QRhiShaderResourceBinding> additionalBindings)
{
  if(this->node.output[0]->type == score::gfx::Types::Image)
  {
    score::gfx::defaultPassesInit(
        m_p, this->node.output[0]->edges, renderer, mesh, v, f, m_processUBO,
        m_material.buffer, m_samplers, additionalBindings);
  }
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
  auto& n = static_cast<const score::gfx::NodeModel&>(this->node);
  res.updateDynamicBuffer(m_processUBO, 0, sizeof(ProcessUBO), &n.standardUBO);

  if(m_material.buffer && m_material.size > 0)
  {
    if(materialChanged)
    {
      char* data = n.m_materialData.get();
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
  auto& n = static_cast<const score::gfx::NodeModel&>(node);
  if(geometryChanged && n.geometry.meshes)
  {
    std::tie(m_mesh, m_meshbufs)
        = renderer.acquireMesh(n.geometry, res, m_mesh, m_meshbufs);
  }
}

void GenericNodeRenderer::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, score::gfx::Edge* e)
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

score::gfx::NodeRenderer::~NodeRenderer() { }

QRhiBuffer *NodeRenderer::bufferForInput(const Port &input) { return nullptr; }

QRhiBuffer *NodeRenderer::bufferForOutput(const Port &output) { return nullptr; }

void NodeRenderer::inputAboutToFinish(
    RenderList& renderer, const Port& p, QRhiResourceUpdateBatch*&)
{
}


}
