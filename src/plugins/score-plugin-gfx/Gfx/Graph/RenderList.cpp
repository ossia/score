
#include <Gfx/Graph/CustomMesh.hpp>
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/tools/Debug.hpp>

//#define RENDERDOC_PROFILING 0
#if defined(RENDERDOC_PROFILING)
#include "renderdoc_app.h"

#include <dlfcn.h>
#endif

#include <iostream>

namespace score::gfx
{

#if defined(RENDERDOC_PROFILING)
auto renderdoc_api = [] {
  RENDERDOC_API_1_6_0* rdoc_api{};
  void* mod = dlopen("/usr/lib/librenderdoc.so", RTLD_NOW | RTLD_LOCAL);
  assert(mod);
  {
    auto RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
    assert(RENDERDOC_GetAPI);
    int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void**)&rdoc_api);
    assert(ret == 1);
    assert(rdoc_api != nullptr);
  }
  return rdoc_api;
}();
#endif

MeshBuffers RenderList::initMeshBuffer(const Mesh& mesh, QRhiResourceUpdateBatch& res)
{
  if(auto it = m_vertexBuffers.find(&mesh); it != m_vertexBuffers.end())
    return it->second;

  MeshBuffers ret = mesh.init(*state.rhi);
  mesh.update(ret, res);
  m_vertexBuffers.insert({const_cast<Mesh*>(&mesh), ret});

  return ret;
}

RenderList::RenderList(OutputNode& output, const std::shared_ptr<RenderState>& state)
    : m_state{state}
    , output{output}
    , state{*m_state}
    , m_samples{state->samples}
{
}

RenderList::~RenderList()
{
  for(auto node : this->nodes)
  {
    node->renderedNodes.erase(this);
    node->renderedNodesChanged();
  }
  for(auto node : renderers)
  {
    delete node;
  }
  renderers.clear();
}

void RenderList::init()
{
  m_ready = false;
  if(!state.rhi)
    return;
  auto& rhi = *state.rhi;

  m_minTexSize = state.rhi->resourceLimit(QRhi::ResourceLimit::TextureSizeMin);
  m_maxTexSize = state.rhi->resourceLimit(QRhi::ResourceLimit::TextureSizeMax);

  m_outputUBO
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(OutputUBO));
  m_outputUBO->setName("RenderList::m_outputUBO");
  m_outputUBO->create();

  m_emptyTexture
      = rhi.newTexture(QRhiTexture::RGBA8, QSize{1, 1}, 1, QRhiTexture::Flag{});
  m_emptyTexture->setName("RenderList::m_emptyTexture");
  m_emptyTexture->create();

  m_lastSize = state.renderSize;

  SCORE_ASSERT(!m_initialBatch);
  m_initialBatch = state.rhi->nextResourceUpdateBatch();
  SCORE_ASSERT(m_initialBatch);
}

QRhiResourceUpdateBatch* RenderList::initialBatch() const noexcept
{
  return m_initialBatch;
}

void RenderList::release()
{
  for(auto node : renderers)
  {
    node->release(*this);
  }

  for(auto bufs : m_vertexBuffers)
  {
    delete bufs.second.mesh;
    delete bufs.second.index;
  }

  m_vertexBuffers.clear();
  for(auto& [k, v] : m_customMeshCache)
  {
    delete v;
  }
  m_customMeshCache.clear();

  delete m_outputUBO;
  m_outputUBO = nullptr;

  delete m_emptyTexture;
  m_emptyTexture = nullptr;

  // If nothing happened
  if(m_initialBatch)
  {
    m_initialBatch->release();
    m_initialBatch = nullptr;
  }

  m_requiresDepth = false;
  m_ready = false;
  m_built = false;
}

bool RenderList::maybeRebuild(bool force)
{
  bool rebuilt = false;
  const QSize outputSize = state.renderSize;
  if(outputSize != m_lastSize || !m_built || force)
  {
    m_built = false;
    release();

    // Now we have the nodes in the order in which they are going to
    // be processed

    init();

    m_requiresDepth = false;
    for(auto node : nodes)
      m_requiresDepth |= node->requiresDepth;

    // We init the nodes in reverse orders as
    // the render targets of subsequent nodes must be initialized
    for(auto node : renderers)
    {
      node->init(*this, *m_initialBatch);
      node->materialChanged = true;
      node->geometryChanged = true;
      node->renderTargetSpecsChanged = true;
    }

    m_lastSize = outputSize;
    m_built = true;
    rebuilt = true;
  }
  return rebuilt;
}

TextureRenderTarget RenderList::renderTargetForOutput(const Edge& edge) const noexcept
{
  if(auto sink_node = edge.sink->node)
    if(auto it = sink_node->renderedNodes.find(this);
       it != sink_node->renderedNodes.end())
    {
      auto renderer = it->second;
      auto tex = renderer->renderTargetForInput(*edge.sink);
      if(tex.renderTarget && tex.renderPass)
        return tex;
    }

  return {};
}

QImage RenderList::adaptImage(const QImage& frame)
{
  auto res = resizeTexture(frame, m_minTexSize, m_maxTexSize);
  return res;
  //if(m_flip)
  //  res = std::move(res).mirrored();
  //return res;
}

RenderList::Buffers RenderList::acquireMesh(
    const ossia::geometry_spec& spec, QRhiResourceUpdateBatch& res, const Mesh* current,
    MeshBuffers currentbufs) noexcept
{
  // 1. Try to find mesh from the exact same geometry
  const auto& [p, f] = spec;
  if(auto it = m_customMeshCache.find(spec); it != m_customMeshCache.end())
  {
    auto m = const_cast<CustomMesh*>(safe_cast<const CustomMesh*>(it->second));

    if(m)
    {
      auto meshbufs_it = this->m_vertexBuffers.find(m);
      SCORE_ASSERT(meshbufs_it != this->m_vertexBuffers.end());
      auto mb = meshbufs_it->second;

      // FIX the thraed-unsafety: basically, we need to
      // have some level of double- or triple-buffering
      if(auto cur_idx = p->dirty_index; m->dirtyGeometryIndex != cur_idx)
      {
        m->reload(*p, f);
        m->update(mb, res);
        // FIXME atomic !!
        if(cur_idx > m->dirtyGeometryIndex)
          m->dirtyGeometryIndex = cur_idx;
      }

      return {m, mb};
    }
  }

  // 2. If not found try to see if the mesh is already used
  for(const auto& [k, v] : m_customMeshCache)
  {
    if(v == current)
    {
      auto m = const_cast<CustomMesh*>(safe_cast<const CustomMesh*>(current));

      if(m)
      {
        auto meshbufs_it = this->m_vertexBuffers.find(m);
        SCORE_ASSERT(meshbufs_it != this->m_vertexBuffers.end());
        SCORE_ASSERT(meshbufs_it->second.index == currentbufs.index);
        SCORE_ASSERT(meshbufs_it->second.mesh == currentbufs.mesh);
        auto mb = currentbufs;
        auto cur_idx = p->dirty_index;

        m->reload(*p, f);
        m->update(mb, res);
        // FIXME atomic !!
        if(cur_idx > m->dirtyGeometryIndex)
          m->dirtyGeometryIndex = cur_idx;
        return {m, mb};
      }
    }
  }

  // 3. Really not found, we allocate a new mesh for good
  auto m = new CustomMesh{*p, f};
  auto meshbufs = initMeshBuffer(*m, res);

  this->m_customMeshCache[{p, f}] = m;
  return {m, meshbufs};
}

void RenderList::clearRenderers()
{
  renderers.clear();

  // Necessary so that we re-go through init() on the next frame
  m_built = false;
}

QSize RenderList::renderSize(const Edge* e) const noexcept
{
  if(!e)
    return this->m_state->renderSize;

  auto rt = this->renderTargetForOutput(*e);
  if(!rt.texture)
    return this->m_state->renderSize;

  return rt.texture->pixelSize();
}

const Mesh& RenderList::defaultQuad() const noexcept
{
  static const TexturedQuad m{true};
  return m;
  /*
  if(!m_flip)
  {
    static const TexturedQuad m{true};
    return m;
  }
  else
  {
    static const TexturedQuad m{true};
    return m;
  }
  */
}

const Mesh& RenderList::defaultTriangle() const noexcept
{
  static const TexturedTriangle m{false};
  return m;
  /*
  if(!m_flip)
  {
    static const TexturedTriangle m{true};
    return m;
  }
  else
  {
    static const TexturedTriangle m{true};
    return m;
  }
  */
}

void RenderList::render(QRhiCommandBuffer& commands, bool force)
{
  if(renderers.size() <= 1 && !force)
    return;

  bool rt_changed = false;
  for(auto* renderer : renderers)
  {
    renderer->checkForChanges();

    // If a render target changes most likely we have
    // to rebuild render passes as there's no way to simply
    // update a render target from e.g. a texture format to another
    rt_changed |= renderer->renderTargetSpecsChanged;
  }

  SCORE_ASSERT(m_outputUBO);
  SCORE_ASSERT(m_emptyTexture);

  bool rebuilt = maybeRebuild(false);
  QRhiResourceUpdateBatch* updateBatch{};
  if(m_initialBatch)
  {
    updateBatch = m_initialBatch;
    m_initialBatch = nullptr;
  }
  else
  {
    updateBatch = state.rhi->nextResourceUpdateBatch();
  }

  if(rt_changed && !rebuilt)
  {
    for(auto node : renderers)
    {
      node->release(*this);
    }
    for(auto node : renderers)
    {
      node->init(*this, *updateBatch);
      node->materialChanged = true;
      node->geometryChanged = true;
      node->renderTargetSpecsChanged = true;
    }
  }
  // Check if the viewport has changed

  update(*updateBatch);

  // For each texture input port
  //  For all previous node
  //   Update
  //  Begin pass
  //   For all previous node
  //    Render
  //  End pass

  struct EdgePair
  {
    Edge* edge;
    NodeRenderer* node;
  };

#if defined(RENDERDOC_PROFILING)
  if(renderdoc_api)
    renderdoc_api->StartFrameCapture(NULL, NULL);
#endif

  ossia::small_pod_vector<EdgePair, 4> prevRenderers;
  for(auto it = this->nodes.rbegin(); it != this->nodes.rend(); ++it)
  {
    bool node_was_rendered = false;
    auto node = *it;
    for(auto input : node->input)
    {
      // For each edge incoming to each image input ports of this node,
      // we render the edge source's content.
      if(input->type == Types::Image && !input->edges.empty())
      {
        prevRenderers.clear();
        prevRenderers.reserve(input->edges.size());

        // First update them all and store them in prevRenderers (saves a couple lookups)
        for(auto edge : input->edges)
        {
          auto src = edge->source;
          SCORE_ASSERT(src);

          SCORE_ASSERT(
              src->node->renderedNodes.find(this) != src->node->renderedNodes.end());
          NodeRenderer* renderer = src->node->renderedNodes.find(this)->second;
          prevRenderers.push_back({edge, renderer});

          renderer->update(*this, *updateBatch, edge);
        }

        commands.resourceUpdate(updateBatch);
        updateBatch = state.rhi->nextResourceUpdateBatch();

        // For nodes that perform multiple rendering passes,
        // pre-computations in compute shaders, etc... run them now.
        // Most nodes don't do anything there.
        for(auto [edge, prev_renderer] : prevRenderers)
        {
          commands.resourceUpdate(updateBatch);
          updateBatch = state.rhi->nextResourceUpdateBatch();

          prev_renderer->runInitialPasses(*this, commands, updateBatch, *edge);
        }

        // Then do the final render of each node on the edge sink's render target
        // We *have* to do that in a single beginPass / endPass as every beginPass
        // issues a clearBuffers command.
        {
          auto rendered = node->renderedNodes.find(this);
          SCORE_ASSERT(rendered != node->renderedNodes.end());

          NodeRenderer* renderer = rendered->second;
          auto rt = renderer->renderTargetForInput(*input);

          SCORE_ASSERT(rt.renderTarget);

          commands.beginPass(rt.renderTarget, Qt::black, {1.0f, 0}, updateBatch);
          updateBatch = nullptr;

          QRhiResourceUpdateBatch* res{};
          for(auto [edge, prev_renderer] : prevRenderers)
          {
            prev_renderer->runRenderPass(*this, commands, *edge);
          }

          // Allow the node to do some actions, for instance if a readback
          // of a node's input is going to be needed.
          {
            renderer->inputAboutToFinish(*this, *input, res);
          }
          commands.endPass(res);
          res = nullptr;
        }

        if(node != &output)
        {
          SCORE_ASSERT(!updateBatch);
          updateBatch = state.rhi->nextResourceUpdateBatch();
          SCORE_ASSERT(updateBatch);
        }
        node_was_rendered = true;
      }
    }

    if(!node_was_rendered)
    {
      // Pure computation node - we only run update
      NodeRenderer* renderer = node->renderedNodes.find(this)->second;

      renderer->update(*this, *updateBatch, nullptr);
    }
  }

  // Finally the output node may have some rendering to do too
  {
    SCORE_ASSERT(!this->output.renderedNodes.empty());
    SCORE_ASSERT(
        dynamic_cast<OutputNodeRenderer*>(this->output.renderedNodes.begin()->second));
    auto output_renderer
        = static_cast<OutputNodeRenderer*>(this->output.renderedNodes.begin()->second);

    if(this->output.configuration().outputNeedsRenderPass)
    {
      if(!updateBatch)
      {
        updateBatch = state.rhi->nextResourceUpdateBatch();
        SCORE_ASSERT(updateBatch);
      }

      // FIXME remove this hack
      score::gfx::Port p;
      score::gfx::Edge dummy{&p, &p};
      output_renderer->update(*this, *updateBatch, nullptr);
      output_renderer->runInitialPasses(*this, commands, updateBatch, dummy);
      output_renderer->runRenderPass(*this, commands, dummy);
    }

    output_renderer->finishFrame(*this, commands, updateBatch);

    if(updateBatch)
      updateBatch->release();
  }

#if defined(RENDERDOC_PROFILING)
  if(renderdoc_api)
    renderdoc_api->EndFrameCapture(NULL, NULL);
#endif

  frame++;
}

void RenderList::update(QRhiResourceUpdateBatch& res)
{
  if(!m_ready)
  {
    m_ready = true;

    const auto proj = state.rhi->clipSpaceCorrMatrix();

    memcpy(&m_outputUBOData.clipSpaceCorrMatrix[0], proj.data(), sizeof(float) * 16);

    m_outputUBOData.renderSize[0] = this->m_lastSize.width();
    m_outputUBOData.renderSize[1] = this->m_lastSize.height();

    res.updateDynamicBuffer(m_outputUBO, 0, sizeof(OutputUBO), &m_outputUBOData);
  }
}

}
