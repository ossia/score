#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/tools/Debug.hpp>

#include <iostream>

namespace score::gfx
{
#include <Gfx/Qt5CompatPush> // clang-format: keep
MeshBuffers RenderList::initMeshBuffer(const Mesh& mesh, QRhiResourceUpdateBatch& res)
{
  if(auto it = m_vertexBuffers.find(&mesh); it != m_vertexBuffers.end())
    return it->second;

  MeshBuffers ret = mesh.init(*state.rhi);
  mesh.update(ret, res);
  m_vertexBuffers.insert({&mesh, ret});

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
  m_flip = state.rhi->isYUpInFramebuffer();

  m_outputUBO
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(OutputUBO));
  m_outputUBO->setName("RenderList::m_outputUBO");
  m_outputUBO->create();

  m_emptyTexture
      = rhi.newTexture(QRhiTexture::RGBA8, QSize{1, 1}, 1, QRhiTexture::Flag{});
  m_emptyTexture->setName("RenderList::m_emptyTexture");
  m_emptyTexture->create();

  m_lastSize = state.renderSize;

  m_initialBatch = state.rhi->nextResourceUpdateBatch();
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

  delete m_outputUBO;
  m_outputUBO = nullptr;

  delete m_emptyTexture;
  m_emptyTexture = nullptr;

  m_ready = false;
  m_built = false;
}

void RenderList::maybeRebuild()
{
  const QSize outputSize = state.renderSize;
  if(outputSize != m_lastSize || !m_built)
  {
    m_built = false;
    release();

    // Now we have the nodes in the order in which they are going to
    // be processed

    init();

    // We init the nodes in reverse orders as
    // the render targets of subsequent nodes must be initialized
    for(auto node : renderers)
    {
      node->init(*this, *this->m_initialBatch);
    }

    m_lastSize = outputSize;
    m_built = true;
  }
}

TextureRenderTarget RenderList::renderTargetForOutput(const Edge& edge) noexcept
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

void RenderList::clearRenderers()
{
  renderers.clear();

  // Necessary so that we re-go through init() on the next frame
  m_built = false;
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
  {
    return;
  }

  // Check if the viewport has changed
  maybeRebuild();

  SCORE_ASSERT(m_outputUBO);
  SCORE_ASSERT(m_emptyTexture);

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

  ossia::small_pod_vector<EdgePair, 4> prevRenderers;
  for(auto it = this->nodes.rbegin(); it != this->nodes.rend(); ++it)
  {
    auto node = *it;
    for(auto input : node->input)
    {
      // For each edge incoming to each image input ports of this node,
      // we render the edge source's content.
      if(input->type == Types::Image)
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

          renderer->update(*this, *updateBatch);
        }

        // For nodes that perform multiple rendering passes,
        // pre-computations in compute shaders, etc... run them now.
        // Most nodes don't do anything there.
        for(auto [edge, renderer] : prevRenderers)
        {
          renderer->runInitialPasses(*this, commands, updateBatch, *edge);
        }

        // Then do the final render of each node on the edge sink's render target
        // We *have* to do that in a single beginPass / endPass as every beginPass
        // issues a clearBuffers command.
        {
          SCORE_ASSERT(node->renderedNodes.find(this) != node->renderedNodes.end());
          auto rt = node->renderedNodes.find(this)->second->renderTargetForInput(*input);
          SCORE_ASSERT(rt.renderTarget);

          commands.beginPass(rt.renderTarget, Qt::black, {1.0f, 0}, updateBatch);

          QRhiResourceUpdateBatch* res{};
          for(auto [edge, node] : prevRenderers)
          {
            node->runRenderPass(*this, commands, *edge);
          }

          // Allow the node to do some actions, for instance if a readback
          // of a node's input is going to be needed.
          {
            SCORE_ASSERT(node->renderedNodes.find(this) != node->renderedNodes.end());
            NodeRenderer* renderer = node->renderedNodes.find(this)->second;

            renderer->inputAboutToFinish(*this, *input, res);
          }
          commands.endPass(res);
        }

        if(node != &output)
          updateBatch = state.rhi->nextResourceUpdateBatch();
      }
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
      // FIXME remove this hack
      score::gfx::Port p;
      score::gfx::Edge dummy{&p, &p};
      output_renderer->update(*this, *updateBatch);
      output_renderer->runInitialPasses(*this, commands, updateBatch, dummy);
      output_renderer->runRenderPass(*this, commands, dummy);
    }

    output_renderer->finishFrame(*this, commands);
  }
}

void RenderList::update(QRhiResourceUpdateBatch& res)
{
  if(!m_ready)
  {
    m_ready = true;

    const auto proj = state.rhi->clipSpaceCorrMatrix();

    m_outputUBOData.texcoordAdjust[0] = 1.f;
    m_outputUBOData.texcoordAdjust[1] = 0.f;
    /*
    if (state.rhi->isYUpInFramebuffer())
    {
      // Vulkan, D3D, Metal
      m_outputUBOData.texcoordAdjust[0] = 1.f;
      m_outputUBOData.texcoordAdjust[1] = 0.f;
    }
    else
    {
      // GL
      m_outputUBOData.texcoordAdjust[0] = -1.f;
      m_outputUBOData.texcoordAdjust[1] = 1.f;
    }
    */

    memcpy(&m_outputUBOData.clipSpaceCorrMatrix[0], proj.data(), sizeof(float) * 16);

    m_outputUBOData.renderSize[0] = this->m_lastSize.width();
    m_outputUBOData.renderSize[1] = this->m_lastSize.height();

    res.updateDynamicBuffer(m_outputUBO, 0, sizeof(OutputUBO), &m_outputUBOData);
  }
}

#include <Gfx/Qt5CompatPop> // clang-format: keep
}
