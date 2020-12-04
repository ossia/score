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
  ready = false;
  if(!state.rhi)
    return;
  auto& rhi = *state.rhi;

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

  for (auto node : renderedNodes)
    node->release(*this);

  for (auto bufs : m_vertexBuffers)
  {
    delete bufs.second.mesh;
    delete bufs.second.index;
  }

  textureTargets.clear();
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
  const QSize outputSize = state.size;
  if (outputSize != lastSize)
  {
    release();

    // Now we have the nodes in the order in which they are going to
    // be processed

    createRenderTargets();

    init();

    for (auto node : renderedNodes)
      node->init(*this);

    lastSize = outputSize;
  }
}

QRhiTexture* Renderer::textureTargetForInputPort(Port& port)
{
  QRhiTexture* texture = m_emptyTexture;
  if (port.edges.empty())
    return texture;

  auto source_node = port.edges[0]->source->node;
  if (!source_node)
    return texture;

  auto renderedNode = source_node->renderedNodes[this];
  auto it = textureTargets.find(renderedNode);
  if(it == textureTargets.end()) {
    qDebug() << "! warning ! output texture requested but not existing." << typeid(renderedNode).name();
    return texture;
  }

  return it->second;
}

void Renderer::createRenderTarget(score::gfx::NodeRenderer& node)
{
  auto tg = node.createRenderTarget(state);
  if(tg.texture)
    textureTargets[&node] = tg.texture;
  else
    qDebug() << typeid(node).name() << "does not have a texture ! ";
}

void Renderer::createRenderTargets()
{
  for (auto node : renderedNodes) {
    createRenderTarget(*node);
  }
}

void Renderer::render(QRhiCommandBuffer& commands)
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
