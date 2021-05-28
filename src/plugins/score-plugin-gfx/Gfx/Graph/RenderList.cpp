#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/RenderList.hpp>

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
  mesh_buf->create();

  QRhiBuffer* idx_buf{};
  if (!mesh.indexArray.empty())
  {
    idx_buf = rhi.newBuffer(
        QRhiBuffer::Immutable,
        QRhiBuffer::IndexBuffer,
        mesh.indexArray.size() * sizeof(unsigned int));
    idx_buf->create();
  }

  MeshBuffers ret{mesh_buf, idx_buf};
  m_vertexBuffers.insert({&mesh, ret});
  buffersToUpload.push_back({&mesh, ret});
  return ret;
}

void RenderList::init()
{
  ready = false;
  if (!state.rhi)
    return;
  auto& rhi = *state.rhi;

  m_rendererUBO = rhi.newBuffer(
      QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(ScreenUBO));
  m_rendererUBO->create();

  m_emptyTexture = rhi.newTexture(
      QRhiTexture::RGBA8, QSize{1, 1}, 1, QRhiTexture::Flag{});
  m_emptyTexture->create();
}

void RenderList::release()
{
  for (auto node : renderedNodes)
    node->release(*this);

  for (auto bufs : m_vertexBuffers)
  {
    delete bufs.second.mesh;
    delete bufs.second.index;
  }

  m_vertexBuffers.clear();
  buffersToUpload.clear();

  delete m_rendererUBO;
  m_rendererUBO = nullptr;

  delete m_emptyTexture;
  m_emptyTexture = nullptr;

  ready = false;
}

void RenderList::maybeRebuild()
{
  const QSize outputSize = state.size;
  if (outputSize != lastSize)
  {
    release();

    // Now we have the nodes in the order in which they are going to
    // be processed

    init();

    for (auto node : renderedNodes)
      node->init(*this);

    lastSize = outputSize;
  }
}

QRhiTexture* RenderList::textureTargetForInputPort(Port& port)
{
  QRhiTexture* texture = m_emptyTexture;
  if (port.edges.empty())
    return texture;

  auto source_node = port.edges[0]->source->node;
  if (!source_node)
    return texture;

  auto renderedNode = source_node->renderedNodes[this];
  if (!renderedNode)
    return texture;

  if (auto tex = renderedNode->renderTarget().texture)
    return tex;
  else
    return texture;
}

void RenderList::render(QRhiCommandBuffer& commands)
{
  if (renderedNodes.size() <= 1)
  {
    return;
  }
  // Check if the viewport has changed
  maybeRebuild();

  auto updateBatch = state.rhi->nextResourceUpdateBatch();
  update(*updateBatch);

  for (std::size_t i = 0; i < renderedNodes.size(); i++)
  {
    renderedNodes[i]->runPass(*this, commands, *updateBatch);

    if (i < renderedNodes.size() - 1)
      updateBatch = state.rhi->nextResourceUpdateBatch();
  }
}

void RenderList::update(QRhiResourceUpdateBatch& res)
{
  if (!ready)
  {
    ready = true;

    const auto proj = state.rhi->clipSpaceCorrMatrix();

    if (!state.rhi->isYUpInFramebuffer())
    {
      // Vulkan, D3D, Metal
      screenUBO.texcoordAdjust[0] = 1.f;
      screenUBO.texcoordAdjust[1] = 0.f;
    }
    else
    {
      // GL
      screenUBO.texcoordAdjust[0] = -1.f;
      screenUBO.texcoordAdjust[1] = 1.f;
    }

    memcpy(&screenUBO.clipSpaceCorrMatrix[0], proj.data(), sizeof(float) * 16);

    screenUBO.renderSize[0] = this->lastSize.width();
    screenUBO.renderSize[1] = this->lastSize.height();

    res.updateDynamicBuffer(m_rendererUBO, 0, sizeof(ScreenUBO), &screenUBO);
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
