#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/OutputNode.hpp>

namespace score::gfx
{
#include <Gfx/Qt5CompatPush> // clang-format: keep
MeshBuffers RenderList::initMeshBuffer(const Mesh& mesh)
{
  if (auto it = m_vertexBuffers.find(&mesh); it != m_vertexBuffers.end())
    return it->second;

  auto& rhi = *state.rhi;
  auto mesh_buf = rhi.newBuffer(
      QRhiBuffer::Immutable,
      QRhiBuffer::VertexBuffer,
      mesh.vertexArray.size() * sizeof(float));
  mesh_buf->setName("RenderList::mesh_buf");
  mesh_buf->create();


  QRhiBuffer* idx_buf{};
  if (!mesh.indexArray.empty())
  {
    idx_buf = rhi.newBuffer(
        QRhiBuffer::Immutable,
        QRhiBuffer::IndexBuffer,
        mesh.indexArray.size() * sizeof(unsigned int));
    idx_buf->setName("RenderList::idx_buf");
    idx_buf->create();
  }

  MeshBuffers ret{mesh_buf, idx_buf};
  m_vertexBuffers.insert({&mesh, ret});
  buffersToUpload.push_back({&mesh, ret});
  return ret;
}

RenderList::RenderList(OutputNode& output, const RenderState& state)
    : output{output}
    , state{state}
{
  output.setRenderer(this);
}

RenderList::~RenderList()
{
  for (auto node : this->nodes)
  {
    node->renderedNodes.erase(this);
  }
  for (auto node : renderers)
  {
    delete node;
  }
  renderers.clear();
}

void RenderList::init()
{
  m_ready = false;
  if (!state.rhi)
    return;
  auto& rhi = *state.rhi;

  m_outputUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(OutputUBO));
  m_outputUBO->setName("RenderList::m_outputUBO");
  m_outputUBO->create();

  m_emptyTexture = rhi.newTexture(
      QRhiTexture::RGBA8, QSize{1, 1}, 1, QRhiTexture::Flag{});
  m_emptyTexture->setName("RenderList::m_emptyTexture");
  m_emptyTexture->create();
}

void RenderList::release()
{
  for (auto node : renderers)
  {
    node->release(*this);
  }

  for (auto bufs : m_vertexBuffers)
  {
    delete bufs.second.mesh;
    delete bufs.second.index;
  }

  m_vertexBuffers.clear();
  buffersToUpload.clear();

  delete m_outputUBO;
  m_outputUBO = nullptr;

  delete m_emptyTexture;
  m_emptyTexture = nullptr;

  m_ready = false;
}

void RenderList::maybeRebuild()
{
  const QSize outputSize = state.size;
  if (outputSize != m_lastSize)
  {
    release();

    // Now we have the nodes in the order in which they are going to
    // be processed

    init();

    // We init the nodes in reverse orders as
    // the render targets of subsequent nodes must be initialized
    for (auto node : renderers)
    {
      node->init(*this);
    }

    m_lastSize = outputSize;
  }
}

TextureRenderTarget RenderList::renderTargetForOutput(const Edge& edge) noexcept
{
  if (auto sink_node = edge.sink->node)
    if (auto renderer = sink_node->renderedNodes[this])
      {
        auto tex = renderer->renderTargetForInput(*edge.sink);
        if(tex.renderTarget && tex.renderPass)
          return tex;
      }

  return {};
}


void RenderList::render(QRhiCommandBuffer& commands)
{
  if (renderers.size() <= 1)
  {
    return;
  }

  // Check if the viewport has changed
  maybeRebuild();

  auto updateBatch = state.rhi->nextResourceUpdateBatch();
  update(*updateBatch);

  // For each texture input port
  //  For all previous node
  //   Update
  //  Begin pass
  //   For all previous node
  //    Render
  //  End pass

  struct EdgePair {
    Edge* edge;
    NodeRenderer* node;
  };

  ossia::small_pod_vector<EdgePair, 4> prevRenderers;
  for(auto it = this->nodes.rbegin(); it !=this->nodes.rend(); ++it)
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

          SCORE_ASSERT(src->node->renderedNodes.find(this) != src->node->renderedNodes.end());
          auto renderer = src->node->renderedNodes[this];
          prevRenderers.push_back({edge, renderer});

          renderer->update(*this, *updateBatch);
        }

        // For nodes that perform multiple rendering passes,
        // pre-computations in compute shaders, etc... run them now.
        // Most nodes don't do anything there.
        for(auto [edge, node] : prevRenderers)
        {
          node->runInitialPasses(*this, commands, updateBatch, *edge);
        }

        // Then do the final render of each node on the edge sink's render target
        // We *have* to do that in a single beginPass / endPass as every beginPass
        // issues a clearBuffers command.
        {
          SCORE_ASSERT(node->renderedNodes.find(this) != node->renderedNodes.end());
          auto rt = node->renderedNodes[this]->renderTargetForInput(*input);
          SCORE_ASSERT(rt.renderTarget);

          commands.beginPass(rt.renderTarget, Qt::black, {1.0f, 0}, updateBatch);

          QRhiResourceUpdateBatch* res{};
          for(auto [edge, node] : prevRenderers)
          {
            auto new_res = node->runRenderPass(*this, commands, *edge);

            // Used for readbacks
            if(res && new_res)
            {
              new_res->merge(res);
              res->release();
            }
            else if(new_res && !res)
            {
              res = new_res;
            }
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
    SCORE_ASSERT(dynamic_cast<OutputNodeRenderer*>(this->output.renderedNodes.begin()->second));
    auto output_renderer = static_cast<OutputNodeRenderer*>(this->output.renderedNodes.begin()->second);
    output_renderer->finishFrame(*this, commands);
  }
}

void RenderList::update(QRhiResourceUpdateBatch& res)
{
  if (!m_ready)
  {
    m_ready = true;

    const auto proj = state.rhi->clipSpaceCorrMatrix();

    if (!state.rhi->isYUpInFramebuffer())
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

    memcpy(&m_outputUBOData.clipSpaceCorrMatrix[0], proj.data(), sizeof(float) * 16);

    m_outputUBOData.renderSize[0] = this->m_lastSize.width();
    m_outputUBOData.renderSize[1] = this->m_lastSize.height();

    res.updateDynamicBuffer(m_outputUBO, 0, sizeof(OutputUBO), &m_outputUBOData);
  }

  if (Q_UNLIKELY(!buffersToUpload.empty()))
  {
    for (auto [mesh, buf] : buffersToUpload)
    {
      res.uploadStaticBuffer(
          buf.mesh, 0, buf.mesh->size(), mesh->vertexArray.data());
      if (buf.index)
        res.uploadStaticBuffer(
            buf.index, 0, buf.index->size(), mesh->indexArray.data());
    }

    buffersToUpload.clear();
  }
}

#include <Gfx/Qt5CompatPop> // clang-format: keep
}
