#include "renderer.hpp"

#include "mesh.hpp"

MeshBuffers Renderer::initMeshBuffer(const Mesh& mesh)
{
  if (auto it = m_vertexBuffers.find(&mesh); it != m_vertexBuffers.end())
    return it->second;

  auto& rhi = *state.rhi;
  auto mesh_buf = rhi.newBuffer(
      QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, mesh.vertexArray.size() * sizeof(float));
  mesh_buf->build();

  QRhiBuffer* idx_buf{};
  if (!mesh.indexArray.empty())
  {
    idx_buf = rhi.newBuffer(
        QRhiBuffer::Immutable,
        QRhiBuffer::IndexBuffer,
        mesh.indexArray.size() * sizeof(unsigned int));
    idx_buf->build();
  }

  MeshBuffers ret{mesh_buf, idx_buf};
  m_vertexBuffers.insert({&mesh, ret});
  buffersToUpload.push_back({&mesh, ret});
  return ret;
}

void Renderer::init()
{
  auto& rhi = *state.rhi;
  ready = false;

  m_rendererUBO = rhi.newBuffer(
#if defined(_WIN32)
      QRhiBuffer::Dynamic,
#else
      QRhiBuffer::Immutable,
#endif
      QRhiBuffer::UniformBuffer,
      sizeof(ScreenUBO));
  m_rendererUBO->build();

  m_emptyTexture = rhi.newTexture(QRhiTexture::RGBA8, QSize{1, 1}, 1, QRhiTexture::Flag{});
  m_emptyTexture->build();
}

void Renderer::release()
{
  for (int i = 0; i < renderedNodes.size(); i++)
    renderedNodes[i]->release(*this);

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

void Renderer::maybeRebuild()
{
  const QSize outputSize = state.swapChain->currentPixelSize();
  if (outputSize != lastSize)
  {
    release();

    // Now we have the nodes in the order in which they are going to
    // be processed

    // For each, we create a render target
    for (int i = 0; i < renderedNodes.size() - 1; i++)
    {
      auto node = renderedNodes[i];
      node->createRenderTarget(state);
    }

    // Except the last one which is going to render to screen
    renderedNodes.back()->setScreenRenderTarget(state);

    init();
    for (int i = 0; i < renderedNodes.size(); i++)
      renderedNodes[i]->init(*this);

    lastSize = outputSize;
  }
}

void Renderer::render()
{
  if (renderedNodes.size() <= 1)
    return;
  const auto commands = state.swapChain->currentFrameCommandBuffer();

  // Check if the viewport has changed
  maybeRebuild();

  auto updateBatch = state.rhi->nextResourceUpdateBatch();
  update(*updateBatch);

  for (int i = 0; i < renderedNodes.size(); i++)
  {
    renderedNodes[i]->runPass(*this, *commands, *updateBatch);

    if (i < renderedNodes.size() - 1)
      updateBatch = state.rhi->nextResourceUpdateBatch();
  }
}

void Renderer::update(QRhiResourceUpdateBatch& res)
{
  if (!ready)
  {
    ready = true;

    const auto proj = state.rhi->clipSpaceCorrMatrix();

    if (!state.rhi->isYUpInFramebuffer())
    {
      screenUBO.texcoordAdjust[0] = 1.f;
      screenUBO.texcoordAdjust[1] = 0.f;
    }
    else
    {
      screenUBO.texcoordAdjust[0] = -1.f;
      screenUBO.texcoordAdjust[1] = 1.f;
    }
    memcpy(&screenUBO.clipSpaceCorrMatrix[0], proj.data(), sizeof(float) * 16);

    screenUBO.renderSize[0] = this->lastSize.width();
    screenUBO.renderSize[1] = this->lastSize.height();

#if defined(_WIN32)
    res.updateDynamicBuffer(m_rendererUBO, 0, sizeof(ScreenUBO), &screenUBO);
#else
    res.uploadStaticBuffer(m_rendererUBO, 0, sizeof(ScreenUBO), &screenUBO);
#endif
  }

  if (Q_UNLIKELY(!buffersToUpload.empty()))
  {
    for (auto [mesh, buf] : buffersToUpload)
    {
      res.uploadStaticBuffer(buf.mesh, 0, buf.mesh->size(), mesh->vertexArray.data());
      if (buf.index)
        res.uploadStaticBuffer(buf.index, 0, buf.index->size(), mesh->indexArray.data());
    }

    buffersToUpload.clear();
  }
}
