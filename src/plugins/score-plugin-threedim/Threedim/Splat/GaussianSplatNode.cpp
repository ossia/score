#include "GaussianSplatNode.hpp"

#include <Gfx/Graph/RenderList.hpp>

#include <ossia/network/value/value_conversion.hpp>

#include <QDebug>

#if defined(near)
#undef near
#undef far
#endif

namespace score::gfx
{

GaussianSplatNode::GaussianSplatNode()
{
  qDebug() << "[GaussianSplat] Node created";

  // Input port: Raw splat buffer (256 bytes per splat)
  auto splatBuffer = new Port{this, {}, Types::Buffer, {}};

  // Output port: Rendered image
  auto out = new Port{this, {}, Types::Image, {}};

  input.push_back(splatBuffer);
  output.push_back(out);

  this->requiresDepth = false;
}

GaussianSplatNode::~GaussianSplatNode() = default;

void GaussianSplatNode::process(Message&& msg)
{
  ProcessNode::process(msg.token);

  int32_t p = 0;
  for(const gfx_input& m : msg.input)
  {
    if(auto val = ossia::get_if<ossia::value>(&m))
    {
      switch(p)
      {
        case 1:
          this->modelPosition = ossia::convert<ossia::vec3f>(*val);
          break;
        case 2:
          this->modelRotation = ossia::convert<ossia::vec3f>(*val);
          break;
        case 3:
          this->modelScale = ossia::convert<ossia::vec3f>(*val);
          break;
        case 4:
          this->position = ossia::convert<ossia::vec3f>(*val);
          break;
        case 5:
          this->center = ossia::convert<ossia::vec3f>(*val);
          break;
        case 6:
          this->fov = ossia::convert<float>(*val);
          break;
        case 7:
          this->near = ossia::convert<float>(*val);
          break;
        case 8:
          this->far = ossia::convert<float>(*val);
          break;
      }
    }
    p++;
  }
  this->materialChange();
}

score::gfx::NodeRenderer*
GaussianSplatNode::createRenderer(RenderList& r) const noexcept
{
  qDebug() << "[GaussianSplat] createRenderer called, splatCount=" << splatCount;
  return new GaussianSplatRenderer{*this};
}

GaussianSplatRenderer::GaussianSplatRenderer(const GaussianSplatNode& node)
    : GenericNodeRenderer{node}
    , m_node{node}
{
  qDebug() << "[GaussianSplat] Renderer constructed";
}

GaussianSplatRenderer::~GaussianSplatRenderer() = default;

TextureRenderTarget GaussianSplatRenderer::renderTargetForInput(const Port& p)
{
  return m_inputTarget;
}

// ─────────────────────────────────────────────────────────────────────────────
// Preprocess pipeline: raw 256B splats → compact 64B rendering splats
// ─────────────────────────────────────────────────────────────────────────────

void GaussianSplatRenderer::createPreprocessPipeline(RenderList& renderer)
{
  qDebug() << "[GaussianSplat] createPreprocessPipeline: splatCount="
           << m_node.splatCount
           << "rawBuf=" << (void*)m_rawSplatBuffer;

  if(!renderer.state.rhi->isFeatureSupported(QRhi::Compute))
  {
    qWarning() << "[GaussianSplat] Compute shaders NOT supported!";
    return;
  }

  auto& rhi = *renderer.state.rhi;
  const int64_t splatCount = m_node.splatCount;
  if(splatCount <= 0)
  {
    qWarning() << "[GaussianSplat] splatCount <= 0, skipping preprocess pipeline";
    return;
  }

  // Create compact output buffer (64 bytes per splat)
  const int64_t renderBufSize = splatCount * 64;
  delete m_renderSplatBuffer;
  m_renderSplatBuffer
      = rhi.newBuffer(QRhiBuffer::Immutable, QRhiBuffer::StorageBuffer, renderBufSize);
  if(!m_renderSplatBuffer->create())
  {
    qWarning() << "[GaussianSplat] Failed to create renderSplatBuffer size=" << renderBufSize;
    delete m_renderSplatBuffer;
    m_renderSplatBuffer = nullptr;
    return;
  }
  qDebug() << "[GaussianSplat] renderSplatBuffer created, size=" << renderBufSize;

  // Preprocess uniform buffer
  if(!m_preprocessUniformBuffer)
  {
    m_preprocessUniformBuffer
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 96);
    if(!m_preprocessUniformBuffer->create())
    {
      qWarning() << "[GaussianSplat] Failed to create preprocessUniformBuffer";
      delete m_preprocessUniformBuffer;
      m_preprocessUniformBuffer = nullptr;
      return;
    }
  }

  // Compile preprocess shader
  QShader preprocessShader = score::gfx::makeCompute(
      renderer.state, GaussianSplatShaders::preprocess_shader);
  if(!preprocessShader.isValid())
  {
    qWarning() << "[GaussianSplat] preprocess_shader compilation FAILED";
    return;
  }
  qDebug() << "[GaussianSplat] preprocess_shader compiled OK";

  // Cleanup old pipeline
  delete m_preprocessSrb;
  delete m_preprocessPipeline;

  m_preprocessSrb = rhi.newShaderResourceBindings();
  m_preprocessSrb->setBindings({
      QRhiShaderResourceBinding::bufferLoad(
          0, QRhiShaderResourceBinding::ComputeStage, m_rawSplatBuffer),
      QRhiShaderResourceBinding::bufferLoadStore(
          1, QRhiShaderResourceBinding::ComputeStage, m_renderSplatBuffer),
      QRhiShaderResourceBinding::uniformBuffer(
          2, QRhiShaderResourceBinding::ComputeStage, m_preprocessUniformBuffer),
  });
  if(!m_preprocessSrb->create())
  {
    qWarning() << "[GaussianSplat] preprocess SRB creation FAILED";
    return;
  }

  m_preprocessPipeline = rhi.newComputePipeline();
  m_preprocessPipeline->setShaderResourceBindings(m_preprocessSrb);
  m_preprocessPipeline->setShaderStage(
      {QRhiShaderStage::Compute, preprocessShader});
  if(!m_preprocessPipeline->create())
  {
    qWarning() << "[GaussianSplat] preprocess pipeline creation FAILED";
    delete m_preprocessPipeline;
    m_preprocessPipeline = nullptr;
    return;
  }

  qDebug() << "[GaussianSplat] preprocess pipeline created OK";
  m_preprocessResourcesCreated = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sort pipelines
// ─────────────────────────────────────────────────────────────────────────────

void GaussianSplatRenderer::createSortPipelines(RenderList& renderer)
{
  qDebug() << "[GaussianSplat] createSortPipelines";

  if(!renderer.state.rhi->isFeatureSupported(QRhi::Compute))
  {
    qWarning() << "[GaussianSplat] Compute not supported, no sorting";
    return;
  }
  if(!m_renderSplatBuffer)
  {
    qWarning() << "[GaussianSplat] No renderSplatBuffer, cannot create sort pipelines";
    return;
  }

  auto& rhi = *renderer.state.rhi;
  const int64_t splatCount = m_node.splatCount;
  if(splatCount <= 0)
    return;

  const int64_t numWorkgroups
      = (splatCount + SORT_WORKGROUP_SIZE - 1) / SORT_WORKGROUP_SIZE;
  const int64_t keyBufferSize = splatCount * sizeof(uint32_t);
  const int64_t indexBufferSize = splatCount * sizeof(uint32_t);
  // Status buffer: 2 tile counters + 2 passes × numWorkgroups × 256 status entries
  const int64_t statusEntries = 2 + 2 * numWorkgroups * NUM_BUCKETS;
  const int64_t statusBufferSize = statusEntries * sizeof(uint32_t);

  auto createOrResizeBuffer
      = [&](QRhiBuffer*& buf, int64_t size, QRhiBuffer::UsageFlags usage) {
    if(buf && buf->size() >= size)
      return;
    delete buf;
    buf = rhi.newBuffer(QRhiBuffer::Immutable, usage, size);
    buf->create();
  };

  createOrResizeBuffer(
      m_sortKeysBuffer, keyBufferSize, QRhiBuffer::StorageBuffer);
  createOrResizeBuffer(
      m_sortKeysAltBuffer, keyBufferSize, QRhiBuffer::StorageBuffer);
  createOrResizeBuffer(
      m_sortIndicesBuffer, indexBufferSize, QRhiBuffer::StorageBuffer);
  createOrResizeBuffer(
      m_sortIndicesAltBuffer, indexBufferSize, QRhiBuffer::StorageBuffer);
  createOrResizeBuffer(
      m_statusBuffer, statusBufferSize, QRhiBuffer::StorageBuffer);

  // Depth key pass uses its own uniform layout: {mat4 view, uint splatCount, float near, float far, uint pad}
  if(!m_sortUniformBuffer)
  {
    m_sortUniformBuffer
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 256);
    m_sortUniformBuffer->create();
  }

  // Fused sort pass uniforms (8 uints = 32 bytes)
  if(!m_sortPassUniformBuffer)
  {
    m_sortPassUniformBuffer
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 32);
    m_sortPassUniformBuffer->create();
  }

  // Clear pass uniforms (4 uints = 16 bytes)
  if(!m_clearUniformBuffer)
  {
    m_clearUniformBuffer
        = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, 16);
    m_clearUniformBuffer->create();
  }

  // Compile compute shaders
  QShader depthKeyShader = score::gfx::makeCompute(
      renderer.state, GaussianSplatShaders::depth_key_shader);
  QShader clearShader = score::gfx::makeCompute(
      renderer.state, GaussianSplatShaders::clear_shader);
  QShader fusedSortShader = score::gfx::makeCompute(
      renderer.state, GaussianSplatShaders::fused_sort_shader);

  if(!depthKeyShader.isValid())
    qWarning() << "[GaussianSplat] depth_key_shader compilation FAILED";
  if(!clearShader.isValid())
    qWarning() << "[GaussianSplat] clear_shader compilation FAILED";
  if(!fusedSortShader.isValid())
    qWarning() << "[GaussianSplat] fused_sort_shader compilation FAILED";

  // Depth key pipeline — reads from compact m_renderSplatBuffer
  delete m_depthKeySrb;
  delete m_depthKeyPipeline;

  m_depthKeySrb = rhi.newShaderResourceBindings();
  m_depthKeySrb->setBindings({
      QRhiShaderResourceBinding::bufferLoad(
          0, QRhiShaderResourceBinding::ComputeStage, m_renderSplatBuffer),
      QRhiShaderResourceBinding::bufferLoadStore(
          1, QRhiShaderResourceBinding::ComputeStage, m_sortKeysBuffer),
      QRhiShaderResourceBinding::bufferLoadStore(
          2, QRhiShaderResourceBinding::ComputeStage, m_sortIndicesBuffer),
      QRhiShaderResourceBinding::uniformBuffer(
          3, QRhiShaderResourceBinding::ComputeStage, m_sortUniformBuffer),
  });
  m_depthKeySrb->create();

  m_depthKeyPipeline = rhi.newComputePipeline();
  m_depthKeyPipeline->setShaderResourceBindings(m_depthKeySrb);
  m_depthKeyPipeline->setShaderStage(
      {QRhiShaderStage::Compute, depthKeyShader});
  if(!m_depthKeyPipeline->create())
    qWarning() << "[GaussianSplat] depthKey pipeline creation FAILED";

  // Clear pipeline — zeros the status buffer
  delete m_clearSrb;
  delete m_clearPipeline;

  m_clearSrb = rhi.newShaderResourceBindings();
  m_clearSrb->setBindings({
      QRhiShaderResourceBinding::bufferLoadStore(
          0, QRhiShaderResourceBinding::ComputeStage, m_statusBuffer),
      QRhiShaderResourceBinding::uniformBuffer(
          1, QRhiShaderResourceBinding::ComputeStage, m_clearUniformBuffer),
  });
  m_clearSrb->create();

  m_clearPipeline = rhi.newComputePipeline();
  m_clearPipeline->setShaderResourceBindings(m_clearSrb);
  m_clearPipeline->setShaderStage(
      {QRhiShaderStage::Compute, clearShader});
  if(!m_clearPipeline->create())
    qWarning() << "[GaussianSplat] clear pipeline creation FAILED";

  // Fused sort pipeline (ping-pong: separate read/write buffers)
  delete m_sortSrb;
  delete m_sortSrbAlt;
  delete m_sortPipeline;

  // Even passes: read keys/indices → write keysAlt/indicesAlt
  m_sortSrb = rhi.newShaderResourceBindings();
  m_sortSrb->setBindings({
      QRhiShaderResourceBinding::bufferLoad(
          0, QRhiShaderResourceBinding::ComputeStage, m_sortKeysBuffer),
      QRhiShaderResourceBinding::bufferLoad(
          1, QRhiShaderResourceBinding::ComputeStage, m_sortIndicesBuffer),
      QRhiShaderResourceBinding::bufferLoadStore(
          2, QRhiShaderResourceBinding::ComputeStage, m_sortKeysAltBuffer),
      QRhiShaderResourceBinding::bufferLoadStore(
          3, QRhiShaderResourceBinding::ComputeStage, m_sortIndicesAltBuffer),
      QRhiShaderResourceBinding::bufferLoadStore(
          4, QRhiShaderResourceBinding::ComputeStage, m_statusBuffer),
      QRhiShaderResourceBinding::uniformBuffer(
          5, QRhiShaderResourceBinding::ComputeStage, m_sortPassUniformBuffer),
  });
  m_sortSrb->create();

  // Odd passes: read keysAlt/indicesAlt → write keys/indices
  m_sortSrbAlt = rhi.newShaderResourceBindings();
  m_sortSrbAlt->setBindings({
      QRhiShaderResourceBinding::bufferLoad(
          0, QRhiShaderResourceBinding::ComputeStage, m_sortKeysAltBuffer),
      QRhiShaderResourceBinding::bufferLoad(
          1, QRhiShaderResourceBinding::ComputeStage, m_sortIndicesAltBuffer),
      QRhiShaderResourceBinding::bufferLoadStore(
          2, QRhiShaderResourceBinding::ComputeStage, m_sortKeysBuffer),
      QRhiShaderResourceBinding::bufferLoadStore(
          3, QRhiShaderResourceBinding::ComputeStage, m_sortIndicesBuffer),
      QRhiShaderResourceBinding::bufferLoadStore(
          4, QRhiShaderResourceBinding::ComputeStage, m_statusBuffer),
      QRhiShaderResourceBinding::uniformBuffer(
          5, QRhiShaderResourceBinding::ComputeStage, m_sortPassUniformBuffer),
  });
  m_sortSrbAlt->create();

  m_sortPipeline = rhi.newComputePipeline();
  m_sortPipeline->setShaderResourceBindings(m_sortSrb);
  m_sortPipeline->setShaderStage(
      {QRhiShaderStage::Compute, fusedSortShader});
  if(!m_sortPipeline->create())
    qWarning() << "[GaussianSplat] sort pipeline creation FAILED";

  m_sortResourcesCreated = true;
  m_lastSplatCount = splatCount;
  qDebug() << "[GaussianSplat] Sort pipelines created OK, workgroups=" << numWorkgroups;
}

// ─────────────────────────────────────────────────────────────────────────────
// Render pipeline
// ─────────────────────────────────────────────────────────────────────────────

void GaussianSplatRenderer::createRenderPipeline(RenderList& renderer)
{
  qDebug() << "[GaussianSplat] createRenderPipeline: renderSplatBuf="
           << (void*)m_renderSplatBuffer
           << "sortIndicesBuf=" << (void*)m_sortIndicesBuffer
           << "enableSorting=" << m_node.enableSorting;

  if(!m_renderSplatBuffer)
  {
    qWarning() << "[GaussianSplat] No renderSplatBuffer, cannot create render pipeline";
    return;
  }

  delete m_bindings;
  delete m_pipeline;
  m_bindings = nullptr;
  m_pipeline = nullptr;

  auto& rhi = *renderer.state.rhi;

  auto [vertex, fragment] = score::gfx::makeShaders(
      renderer.state, GaussianSplatShaders::vertex_shader,
      GaussianSplatShaders::fragment_shader);

  if(!vertex.isValid())
    qWarning() << "[GaussianSplat] vertex_shader compilation FAILED";
  if(!fragment.isValid())
    qWarning() << "[GaussianSplat] fragment_shader compilation FAILED";

  // All 3 bindings must always be present (the shader declares them all).
  QRhiBuffer* indicesBuf = (m_sortIndicesBuffer && m_node.enableSorting)
                               ? m_sortIndicesBuffer
                               : m_dummyStorageBuffer;

  qDebug() << "[GaussianSplat] Render bindings: b0=renderSplat("
           << m_renderSplatBuffer->size() << ") b1=indices("
           << indicesBuf->size() << ") b2=uniform("
           << m_uniformBuffer->size() << ")";

  m_bindings = rhi.newShaderResourceBindings();
  m_bindings->setBindings({
      QRhiShaderResourceBinding::bufferLoad(
          0, QRhiShaderResourceBinding::VertexStage, m_renderSplatBuffer),
      QRhiShaderResourceBinding::bufferLoad(
          1, QRhiShaderResourceBinding::VertexStage, indicesBuf),
      QRhiShaderResourceBinding::uniformBuffer(
          2, QRhiShaderResourceBinding::VertexStage, m_uniformBuffer),
  });
  if(!m_bindings->create())
  {
    qWarning() << "[GaussianSplat] Render SRB creation FAILED";
    return;
  }

  m_pipeline = rhi.newGraphicsPipeline();
  m_pipeline->setName("GaussianSplat::pipeline");

  m_pipeline->setShaderStages(
      {{QRhiShaderStage::Vertex, vertex},
       {QRhiShaderStage::Fragment, fragment}});

  // No vertex input — quad vertices generated in shader
  QRhiVertexInputLayout inputLayout;
  m_pipeline->setVertexInputLayout(inputLayout);

  m_pipeline->setTopology(QRhiGraphicsPipeline::Triangles);
  m_pipeline->setCullMode(QRhiGraphicsPipeline::None);
  m_pipeline->setSampleCount(renderer.samples());
  // Depth test + write: provides correct occlusion as a safety net.
  // Framework clears depth to 1.0 (far), so all valid splats pass initially.
  // With back-to-front sorting, depth test always passes (each splat is closer).
  // Without sorting, depth write ensures near splats occlude far ones.
  m_pipeline->setDepthTest(true);
  m_pipeline->setDepthWrite(true);

  // Front-to-back "under" compositing (premultiplied alpha).
  // Mathematically equivalent to back-to-front "over", but much more stable:
  // sort-order errors among back splats are hidden by accumulated front alpha.
  // Under: result = src * (1 - dst.alpha) + dst
  QRhiGraphicsPipeline::TargetBlend blend;
  blend.enable = true;
  blend.srcColor = QRhiGraphicsPipeline::OneMinusDstAlpha;
  blend.dstColor = QRhiGraphicsPipeline::One;
  blend.srcAlpha = QRhiGraphicsPipeline::OneMinusDstAlpha;
  blend.dstAlpha = QRhiGraphicsPipeline::One;
  m_pipeline->setTargetBlends({blend});

  m_pipeline->setShaderResourceBindings(m_bindings);

  bool foundRenderPass = false;
  for(auto* edge : node.output[0]->edges)
  {
    auto rt = renderer.renderTargetForOutput(*edge);
    if(rt.renderTarget)
    {
      m_pipeline->setRenderPassDescriptor(rt.renderPass);
      foundRenderPass = true;
      break;
    }
  }
  if(!foundRenderPass)
    qWarning() << "[GaussianSplat] No render pass descriptor found from output edges!";

  if(!m_pipeline->create())
  {
    qWarning() << "[GaussianSplat] Render pipeline creation FAILED";
    delete m_pipeline;
    m_pipeline = nullptr;
    return;
  }

  qDebug() << "[GaussianSplat] Render pipeline created OK";
}

// ─────────────────────────────────────────────────────────────────────────────
// Init / Update
// ─────────────────────────────────────────────────────────────────────────────

void GaussianSplatRenderer::init(RenderList& renderer, QRhiResourceUpdateBatch& res)
{
  qDebug() << "[GaussianSplat] init: splatCount=" << m_node.splatCount
           << "enableSorting=" << m_node.enableSorting
           << "shDegree=" << m_node.shDegree;

  auto& rhi = *renderer.state.rhi;

  qDebug() << "[GaussianSplat] RHI backend:"
           << rhi.backendName()
           << "compute=" << rhi.isFeatureSupported(QRhi::Compute);

  // Create input render target
  auto rt_spec = m_node.resolveRenderTargetSpecs(0, renderer);
  auto sampler = rhi.newSampler(
      rt_spec.min_filter, rt_spec.mag_filter, QRhiSampler::Linear,
      rt_spec.address_u, rt_spec.address_v, rt_spec.address_w);
  sampler->setName("GaussianSplat::sampler");
  sampler->create();

  m_inputTarget = score::gfx::createRenderTarget(
      renderer.state, rt_spec.format, rt_spec.size, renderer.samples(),
      true, // renderer.requiresDepth(),
      QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips);

  m_samplers.push_back({sampler, m_inputTarget.texture});

  // Render uniform buffer
  const int64_t uniformSize = 3 * 64 + 16;
  m_uniformBuffer
      = rhi.newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, uniformSize);
  m_uniformBuffer->create();

  // Dummy storage buffer
  m_dummyStorageBuffer
      = rhi.newBuffer(QRhiBuffer::Immutable, QRhiBuffer::StorageBuffer, 16);
  m_dummyStorageBuffer->create();

  // Default mesh (required by base class)
  const auto& mesh = renderer.defaultQuad();
  defaultMeshInit(renderer, mesh, res);

  qDebug() << "[GaussianSplat] init complete";
}

void GaussianSplatRenderer::update(
    RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
{
  const int64_t splatCount = m_node.splatCount;

  // Check for raw splat buffer input
  bool bufferChanged = false;
  if(!m_node.input.empty() && m_node.input[0])
  {
    auto* inputPort = m_node.input[0];
    if(!inputPort->edges.empty())
    {
      auto* inputEdge = inputPort->edges[0];
      if(inputEdge && inputEdge->source)
      {
        QRhiBuffer* newBuffer{};
        score::gfx::NodeRenderer* src_renderer
            = inputEdge->source->node->renderedNodes.at(&renderer);
        if(src_renderer)
        {
          auto bv = src_renderer->bufferForOutput(*inputEdge->source);
          newBuffer = bv.handle;
        }
        if(newBuffer != m_rawSplatBuffer)
        {
          qDebug() << "[GaussianSplat] update: raw buffer changed,"
                   << "old=" << (void*)m_rawSplatBuffer
                   << "new=" << (void*)newBuffer
                   << "size=" << newBuffer->size();
          m_rawSplatBuffer = newBuffer;
          ((GaussianSplatNode&)this->node).splatCount
              = newBuffer ? newBuffer->size() / 256 : 0;
          bufferChanged = true;
          qDebug() << "[GaussianSplat] Loaded splats:"
                   << ((GaussianSplatNode&)this->node).splatCount;
        }
      }
      else
      {
        // Log only once
        static bool logged = false;
        if(!logged)
        {
          qDebug() << "[GaussianSplat] update: input edge exists but no value."
                   << "source=" << (void*)(inputEdge ? inputEdge->source : nullptr);
          logged = true;
        }
      }
    }
    else
    {
      static bool logged = false;
      if(!logged)
      {
        qDebug() << "[GaussianSplat] update: input port has no edges";
        logged = true;
      }
    }
  }
  else
  {
    static bool logged = false;
    if(!logged)
    {
      qDebug() << "[GaussianSplat] update: no input ports";
      logged = true;
    }
  }

  // Recreate compute/render pipelines when buffer or count changes
  if(bufferChanged || splatCount != m_lastSplatCount)
  {
    qDebug() << "[GaussianSplat] update: rebuilding pipelines,"
             << "bufferChanged=" << bufferChanged
             << "splatCount=" << splatCount
             << "lastSplatCount=" << m_lastSplatCount
             << "rawBuf=" << (void*)m_rawSplatBuffer;

    if(m_rawSplatBuffer && splatCount > 0)
    {
      createPreprocessPipeline(renderer);
      if(m_node.enableSorting)
        createSortPipelines(renderer);
      createRenderPipeline(renderer);
    }
    else
    {
      qDebug() << "[GaussianSplat] update: cannot build pipelines (no buffer or count=0)";
    }
    m_lastSplatCount = splatCount;
  }

  // Compute view and projection matrices from camera parameters
  auto& state = renderer.state;

  // Build model matrix from position/rotation/scale
  QMatrix4x4 model;
  model.translate(
      m_node.modelPosition[0], m_node.modelPosition[1], m_node.modelPosition[2]);
  model.rotate(m_node.modelRotation[0], 1, 0, 0); // pitch
  model.rotate(m_node.modelRotation[1], 0, 1, 0); // yaw
  model.rotate(m_node.modelRotation[2], 0, 0, 1); // roll
  model.scale(m_node.modelScale[0], m_node.modelScale[1], m_node.modelScale[2]);

  QMatrix4x4 view;
  view.lookAt(
      QVector3D{m_node.position[0], m_node.position[1], m_node.position[2]},
      QVector3D{m_node.center[0], m_node.center[1], m_node.center[2]},
      QVector3D{0, 1, 0});

  // modelView bakes the model transform so shaders don't need a separate model matrix
  QMatrix4x4 modelView = view * model;

  QMatrix4x4 proj;
  const float aspect
      = float(state.renderSize.width()) / float(state.renderSize.height());
  proj.perspective(m_node.fov, aspect, m_node.near, m_node.far);

  QMatrix4x4 clip = renderer.state.rhi->clipSpaceCorrMatrix();

  struct
  {
    float viewport[2];
    float _pad0;
    uint32_t useSorting;
  } tail;

  tail.viewport[0] = float(state.renderSize.width());
  tail.viewport[1] = float(state.renderSize.height());
  tail._pad0 = 0.f;
  tail.useSorting = m_node.enableSorting && m_sortResourcesCreated ? 1u : 0u;

  char buf[3 * 64 + 16];
  memcpy(buf, modelView.constData(), 64);
  memcpy(buf + 64, proj.constData(), 64);
  memcpy(buf + 128, clip.constData(), 64);
  memcpy(buf + 192, &tail, 16);

  res.updateDynamicBuffer(m_uniformBuffer, 0, sizeof(buf), buf);

  // Update preprocess uniforms
  if(m_preprocessUniformBuffer && m_rawSplatBuffer)
  {
    struct
    {
      float viewMatrix[16];
      float camPos[3];
      uint32_t splatCount;
      uint32_t shDegree;
      float scaleMod;
      uint32_t _pad0;
      uint32_t _pad1;
    } ppUniforms;

    memcpy(ppUniforms.viewMatrix, modelView.constData(), 64);

    // Camera position in model space for SH evaluation
    QVector3D worldCamPos{m_node.position[0], m_node.position[1], m_node.position[2]};
    QVector3D modelCamPos = model.inverted().map(worldCamPos);
    ppUniforms.camPos[0] = modelCamPos.x();
    ppUniforms.camPos[1] = modelCamPos.y();
    ppUniforms.camPos[2] = modelCamPos.z();
    ppUniforms.splatCount = splatCount;
    ppUniforms.shDegree = m_node.shDegree;
    ppUniforms.scaleMod = m_node.scaleFactor;

    res.updateDynamicBuffer(
        m_preprocessUniformBuffer, 0, sizeof(ppUniforms), &ppUniforms);
  }

  // Update sort uniforms
  if(m_sortUniformBuffer && m_node.enableSorting)
  {
    struct
    {
      float viewMatrix[16];
      uint32_t splatCount;
      float nearPlane;
      float farPlane;
      uint32_t _pad;
    } sortUniforms;

    memcpy(sortUniforms.viewMatrix, modelView.constData(), 64);
    sortUniforms.splatCount = splatCount;
    sortUniforms.nearPlane = m_node.near;
    sortUniforms.farPlane = m_node.far;

    res.updateDynamicBuffer(
        m_sortUniformBuffer, 0, sizeof(sortUniforms), &sortUniforms);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Compute passes: preprocess → sort
// ─────────────────────────────────────────────────────────────────────────────

void GaussianSplatRenderer::runInitialPasses(
    RenderList& renderer, QRhiCommandBuffer& cb,
    QRhiResourceUpdateBatch*& res, Edge& edge)
{
  const int64_t splatCount = m_node.splatCount;
  if(splatCount <= 0 || !m_rawSplatBuffer)
  {
    static bool logged = false;
    if(!logged)
    {
      qDebug() << "[GaussianSplat] runInitialPasses: SKIPPED (splatCount="
               << splatCount << "rawBuf=" << (void*)m_rawSplatBuffer << ")";
      logged = true;
    }
    return;
  }

  const int64_t numWorkgroups
      = (splatCount + SORT_WORKGROUP_SIZE - 1) / SORT_WORKGROUP_SIZE;

  // ── Pass 1: SH preprocess (raw → compact) ────────────────────────────
  if(m_preprocessResourcesCreated && m_preprocessPipeline)
  {
    cb.beginComputePass(res);

    cb.setComputePipeline(m_preprocessPipeline);
    cb.setShaderResources(m_preprocessSrb);
    cb.dispatch(numWorkgroups, 1, 1);

    cb.endComputePass();
  }
  else
  {
    static bool logged = false;
    if(!logged)
    {
      qDebug() << "[GaussianSplat] runInitialPasses: preprocess SKIPPED"
               << "(created=" << m_preprocessResourcesCreated
               << "pipeline=" << (void*)m_preprocessPipeline << ")";
      logged = true;
    }
  }

  // ── Pass 2..N: Depth sort ─────────────────────────────────────────────
  if(!m_node.enableSorting || !m_sortResourcesCreated || !m_depthKeyPipeline
     || !m_clearPipeline)
  {
    static bool loggedSkip = false;
    if(!loggedSkip)
    {
      qDebug() << "[GaussianSplat] SORT SKIPPED:"
               << "enableSorting=" << m_node.enableSorting
               << "sortResourcesCreated=" << m_sortResourcesCreated
               << "depthKeyPipeline=" << (void*)m_depthKeyPipeline
               << "clearPipeline=" << (void*)m_clearPipeline;
      loggedSkip = true;
    }
    return;
  }

  auto& rhi = *renderer.state.rhi;

  // Generate depth keys from compact buffer
  cb.beginComputePass(res);

  cb.setComputePipeline(m_depthKeyPipeline);
  cb.setShaderResources(m_depthKeySrb);
  cb.dispatch(numWorkgroups, 1, 1);

  cb.endComputePass();

  // ── Clear status buffer (one dispatch for both sort passes) ──────────
  {
    const int64_t statusEntries = 2 + 2 * numWorkgroups * NUM_BUCKETS;
    struct { uint32_t count; uint32_t _p0, _p1, _p2; } clearUniforms;
    clearUniforms.count = statusEntries;
    clearUniforms._p0 = clearUniforms._p1 = clearUniforms._p2 = 0;

    res = rhi.nextResourceUpdateBatch();
    res->updateDynamicBuffer(
        m_clearUniformBuffer, 0, sizeof(clearUniforms), &clearUniforms);

    const int64_t clearWGs = (statusEntries + SORT_WORKGROUP_SIZE - 1) / SORT_WORKGROUP_SIZE;
    cb.beginComputePass(res);
    res = nullptr;
    cb.setComputePipeline(m_clearPipeline);
    cb.setShaderResources(m_clearSrb);
    cb.dispatch(clearWGs, 1, 1);
    cb.endComputePass();
  }

  // ── Fused radix sort: 2 passes over the top 16 bits (depth key) ────
  // Each pass is a single dispatch using decoupled lookback.
  // Status buffer layout:
  //   [0]: tile counter for pass 0
  //   [1]: tile counter for pass 1
  //   [2 .. 2+N*256-1]: status entries for pass 0
  //   [2+N*256 .. 2+2*N*256-1]: status entries for pass 1
  for(int pass = 0; pass < 2; ++pass)
  {
    const uint32_t bitOffset = 16 + pass * RADIX_BITS;
    const uint32_t statusOffset = 2 + pass * numWorkgroups * NUM_BUCKETS;
    const uint32_t tileCounterIdx = pass;

    struct
    {
      uint32_t splatCount;
      uint32_t bitOffset;
      uint32_t numWorkgroups;
      uint32_t statusOffset;
      uint32_t tileCounterIdx;
      uint32_t _pad0, _pad1, _pad2;
    } sortPassUniforms;
    sortPassUniforms.splatCount = splatCount;
    sortPassUniforms.bitOffset = bitOffset;
    sortPassUniforms.numWorkgroups = numWorkgroups;
    sortPassUniforms.statusOffset = statusOffset;
    sortPassUniforms.tileCounterIdx = tileCounterIdx;
    sortPassUniforms._pad0 = sortPassUniforms._pad1 = sortPassUniforms._pad2 = 0;

    if(!res)
      res = rhi.nextResourceUpdateBatch();
    res->updateDynamicBuffer(
        m_sortPassUniformBuffer, 0, sizeof(sortPassUniforms), &sortPassUniforms);

    cb.beginComputePass(res);
    res = nullptr;
    cb.setComputePipeline(m_sortPipeline);
    cb.setShaderResources(pass % 2 == 0 ? m_sortSrb : m_sortSrbAlt);
    cb.dispatch(numWorkgroups, 1, 1);
    cb.endComputePass();
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// Render pass
// ─────────────────────────────────────────────────────────────────────────────

void GaussianSplatRenderer::runRenderPass(
    RenderList& renderer, QRhiCommandBuffer& cb, Edge& edge)
{
  if(!m_pipeline || !m_renderSplatBuffer)
  {
    static bool logged = false;
    if(!logged)
    {
      qDebug() << "[GaussianSplat] runRenderPass: SKIPPED (pipeline="
               << (void*)m_pipeline
               << "renderBuf=" << (void*)m_renderSplatBuffer << ")";
      logged = true;
    }
    return;
  }

  const int64_t splatCount = m_node.splatCount;
  if(splatCount <= 0)
    return;

  static int frameCount = 0;
  if(frameCount++ % 300 == 0)
  {
    bool sortActive = m_node.enableSorting && m_sortResourcesCreated;
    qDebug() << "[GaussianSplat] runRenderPass: drawing"
             << splatCount << "splats (frame" << frameCount << ")"
             << "sorting=" << sortActive
             << "preprocessOK=" << m_preprocessResourcesCreated
             << "sortOK=" << m_sortResourcesCreated
             << "viewport=" << renderer.state.renderSize;
  }

  cb.setGraphicsPipeline(m_pipeline);
  cb.setShaderResources(m_bindings);
  cb.setViewport(
      QRhiViewport{
          0, 0, (float)renderer.state.renderSize.width(),
          (float)renderer.state.renderSize.height()});

  // 6 vertices (2 triangles) per splat, instanced
  cb.draw(6, splatCount, 0, 0);
}

// ─────────────────────────────────────────────────────────────────────────────
// Cleanup
// ─────────────────────────────────────────────────────────────────────────────

void GaussianSplatRenderer::release(RenderList& r)
{
  qDebug() << "[GaussianSplat] release";

  m_inputTarget.release();

  for(auto& sampler : m_samplers)
    delete sampler.sampler;
  m_samplers.clear();

  // Render
  delete m_uniformBuffer;
  delete m_dummyStorageBuffer;
  delete m_pipeline;
  delete m_bindings;
  m_uniformBuffer = nullptr;
  m_dummyStorageBuffer = nullptr;
  m_pipeline = nullptr;
  m_bindings = nullptr;

  // Preprocess
  delete m_renderSplatBuffer;
  delete m_preprocessUniformBuffer;
  delete m_preprocessPipeline;
  delete m_preprocessSrb;
  m_renderSplatBuffer = nullptr;
  m_preprocessUniformBuffer = nullptr;
  m_preprocessPipeline = nullptr;
  m_preprocessSrb = nullptr;
  m_preprocessResourcesCreated = false;

  // Sort
  delete m_sortKeysBuffer;
  delete m_sortKeysAltBuffer;
  delete m_sortIndicesBuffer;
  delete m_sortIndicesAltBuffer;
  delete m_statusBuffer;
  delete m_sortUniformBuffer;
  delete m_sortPassUniformBuffer;
  delete m_clearUniformBuffer;
  delete m_depthKeyPipeline;
  delete m_clearPipeline;
  delete m_sortPipeline;
  delete m_depthKeySrb;
  delete m_clearSrb;
  delete m_sortSrb;
  delete m_sortSrbAlt;
  m_sortKeysBuffer = nullptr;
  m_sortKeysAltBuffer = nullptr;
  m_sortIndicesBuffer = nullptr;
  m_sortIndicesAltBuffer = nullptr;
  m_statusBuffer = nullptr;
  m_sortUniformBuffer = nullptr;
  m_sortPassUniformBuffer = nullptr;
  m_clearUniformBuffer = nullptr;
  m_depthKeyPipeline = nullptr;
  m_clearPipeline = nullptr;
  m_sortPipeline = nullptr;
  m_depthKeySrb = nullptr;
  m_clearSrb = nullptr;
  m_sortSrb = nullptr;
  m_sortSrbAlt = nullptr;
  m_sortResourcesCreated = false;

  m_rawSplatBuffer = nullptr;
}

} // namespace score::gfx
