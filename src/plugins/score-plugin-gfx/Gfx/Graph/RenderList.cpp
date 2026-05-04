
#include <Gfx/Graph/CustomMesh.hpp>
#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/VertexFallbackPool.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/tools/Debug.hpp>

#include <chrono>

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

  auto& rhi = *state.rhi;
  MeshBuffers ret = mesh.init(rhi);
  mesh.update(rhi, ret, res);
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
  // Defensive: run release() here too. The normal path is Graph::~Graph
  // calling release() on every RL before the destructor fires, but a
  // late onResize during app shutdown can spawn a brand-new RL (via
  // Graph::recreateOutputRenderList) after the ~Graph loop has already
  // moved past the release step. That new RL reaches ~RenderList
  // without anyone having freed its QRhi resources — by the time the
  // shared_ptr drops, the output node's destroyOutput() is next in
  // line, calling RenderState::destroy() → vkDestroyDevice on a device
  // that still owns the new RL's empty textures, InvertYRenderer's
  // render target, etc. (observed as VUID-vkDestroyDevice-device-05137
  // leaks of a handful of VkImages + views + one render pass +
  // framebuffer). release() is idempotent, so calling it again when
  // the Graph already did is a no-op.
  release();
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
  SCORE_ASSERT(m_outputUBO->create());

  // Typed placeholders so that a shader declaring sampler3D / samplerCube /
  // sampler2DArray / sampler2D can be bound to a view of the matching type
  // before any upstream edge has delivered a real texture. Without these,
  // Vulkan's VUID-vkCmdDraw-viewType-07752 fires ("VkImageViewType is
  // VK_IMAGE_VIEW_TYPE_2D but OpTypeImage has Dim=3D") every frame until
  // an upstream texture arrives — and forever if no edge ever connects.
  //
  // create() must succeed here: a null handle reaches vkUpdateDescriptorSets
  // as VK_NULL_HANDLE and the NVIDIA driver segfaults while dereferencing
  // it in a later vkCmdPipelineBarrier. Assert the typed fallbacks exist.
  m_emptyTexture
      = rhi.newTexture(QRhiTexture::RGBA8, QSize{1, 1}, 1, QRhiTexture::Flag{});
  m_emptyTexture->setName("RenderList::m_emptyTexture");
  SCORE_ASSERT(m_emptyTexture->create());

  m_emptyTexture3D = rhi.newTexture(
      QRhiTexture::RGBA8, 1, 1, 1, 1,
      QRhiTexture::ThreeDimensional);
  m_emptyTexture3D->setName("RenderList::m_emptyTexture3D");
  SCORE_ASSERT(m_emptyTexture3D->create());

  m_emptyTextureCube = rhi.newTexture(
      QRhiTexture::RGBA8, QSize{1, 1}, 1, QRhiTexture::CubeMap);
  m_emptyTextureCube->setName("RenderList::m_emptyTextureCube");
  SCORE_ASSERT(m_emptyTextureCube->create());

  // Must use newTextureArray — the 6-arg newTexture() overload is for 3D
  // textures (depth > 1 is a volume slice count, not an array layer count),
  // and QRhi rejects any texture with both ThreeDimensional and TextureArray
  // flags. Passing TextureArray to the 3D overload happened to be tolerated
  // by earlier Qt builds on some backends but hits an assertion under the
  // current validation path.
  m_emptyTextureArray = rhi.newTextureArray(
      QRhiTexture::RGBA8, /*arraySize*/ 1, QSize(1, 1));
  m_emptyTextureArray->setName("RenderList::m_emptyTextureArray");
  SCORE_ASSERT(m_emptyTextureArray->create());

  // Scene-graph arena store (camera / light / material / per_draw
  // buffers). Allocated here; source nodes grab slots from it at
  // construction and write their own packed bytes at their own
  // update(), so ScenePreprocessor never CPU-touches this data in the
  // render path.
  m_registry = std::make_unique<GpuResourceRegistry>();
  m_registry->init(rhi);

  // Fallback vertex-buffer pool for "REQUIRED: false" VERTEX_INPUTS.
  // Lazy-allocates on first use (remapPipelineVertexInputs side), so
  // zero cost when no shader opts in.
  m_vertexFallbackPool = std::make_unique<VertexFallbackPool>();

  m_lastSize = state.renderSize;

  SCORE_ASSERT(!m_initialBatch);
  m_initialBatch = state.rhi->nextResourceUpdateBatch();
  SCORE_ASSERT(m_initialBatch);

  // Seed reserved arena slots (e.g. Material slot 0 = default white
  // dielectric). Must run AFTER the batch is allocated since the upload
  // is recorded into it; runs BEFORE any consumer reads the arena
  // (downstream nodes' init() calls happen on subsequent frames).
  m_registry->seedDefaults(*m_initialBatch);
}

QRhiResourceUpdateBatch* RenderList::initialBatch() const noexcept
{
  return m_initialBatch;
}

QSize RenderList::resolveDownstreamSize(
    const Node* node,
    const ossia::small_flat_map<const Port*, RenderTargetSpecs, 16>& resolvedSpecs)
    const noexcept
{
  QSize best{0, 0};

  for(const auto* out_port : node->output)
  {
    for(const auto* edge : out_port->edges)
    {
      const Port* sink = edge->sink;

      // Case 1: sink is the output node — use its render size.
      if(sink->node == &output)
      {
        best = QSize(
            std::max(best.width(), state.renderSize.width()),
            std::max(best.height(), state.renderSize.height()));
        continue;
      }

      // Case 2: sink port was already resolved (downstream, processed earlier
      // in reverse topological order).
      if(auto it = resolvedSpecs.find(sink); it != resolvedSpecs.end())
      {
        best = QSize(
            std::max(best.width(), it->second.size.width()),
            std::max(best.height(), it->second.size.height()));
        continue;
      }

      // Case 3: sink has a renderer that provides its own RT
      // (e.g. Crousti nodes overriding renderTargetForInput).
      if(auto rn_it = sink->node->renderedNodes.find(this);
         rn_it != sink->node->renderedNodes.end())
      {
        auto tex = rn_it->second->renderTargetForInput(*sink);
        if(tex.texture)
        {
          auto sz = tex.texture->pixelSize();
          best = QSize(
              std::max(best.width(), sz.width()),
              std::max(best.height(), sz.height()));
          continue;
        }
      }
    }
  }

  return best; // {0,0} if no downstream found — caller keeps renderSize fallback
}

void RenderList::createAllInputRenderTargets()
{
  // Phase 1: resolve specs in reverse topological order (sinks first).
  // This ensures downstream RTs are resolved before upstream ones,
  // so that nodes without explicit sizes inherit the downstream size
  // instead of defaulting to the global output resolution.
  ossia::small_flat_map<const Port*, RenderTargetSpecs, 16> resolvedSpecs;

  for(auto it = nodes.rbegin(); it != nodes.rend(); ++it)
  {
    auto* node = *it;
    // Output node manages its own RT via its renderer
    if(node == &output)
      continue;

    int cur_port = 0;
    for(auto* in : node->input)
    {
      if(in->type == Types::Image
         && (in->flags & Flag::GrabsFromSource) != Flag::GrabsFromSource)
      {
        auto spec = node->resolveRenderTargetSpecs(cur_port, *this);

        // If no explicit size, inherit from downstream.
        if(!node->hasExplicitRenderTargetSize(cur_port))
        {
          QSize downstream = resolveDownstreamSize(node, resolvedSpecs);
          if(!downstream.isEmpty())
            spec.size = downstream;
          // else: keep renderer.state.renderSize (ultimate fallback)
        }

        resolvedSpecs[in] = spec;
      }
      cur_port++;
    }
  }

  // Phase 2: create render targets using resolved specs.
  for(auto& [port, spec] : resolvedSpecs)
  {
    bool wantsDepth = requiresDepth(*port);
    bool wantsSamplableDepth
        = (port->flags & Flag::SamplableDepth) == Flag::SamplableDepth;
    auto rt = score::gfx::createRenderTarget(
        state, spec.format, spec.size, samples(),
        wantsDepth || wantsSamplableDepth, wantsSamplableDepth);
    m_inputRenderTargets[port] = std::move(rt);
  }
}

void RenderList::onEdgeRemoved(
    Edge& edge, const ossia::hash_set<const Port*>* preserveSinks)
{
  // Notify source renderer
  if(auto src_it = edge.source->node->renderedNodes.find(this);
     src_it != edge.source->node->renderedNodes.end())
  {
    src_it->second->removeOutputPass(*this, edge);
  }

  // Notify sink renderer (needs a batch for potential resource updates)
  if(auto sink_it = edge.sink->node->renderedNodes.find(this);
     sink_it != edge.sink->node->renderedNodes.end())
  {
    sink_it->second->removeInputEdge(*this, edge);
  }

  // If the sink port has no more edges after this one is removed
  // (called before actual edge destruction, so the edge is still in the list),
  // release the render target — unless the caller has told us a new feed
  // is coming in the same batch. Inserting a filter between A and B would
  // otherwise destroy B's input RT here, only for reconcile to immediately
  // re-allocate an RT with the same spec at the same slot. The caller is
  // responsible for only marking sinks whose RT specs will remain valid;
  // a mismatch is picked up later by the rt_changed surgical path in
  // render().
  if(edge.sink->edges.size() <= 1)
  {
    if(!preserveSinks || !preserveSinks->contains(edge.sink))
      removeInputRenderTarget(edge.sink);
  }
}

void RenderList::removeInputRenderTarget(const Port* port)
{
  auto it = m_inputRenderTargets.find(port);
  if(it != m_inputRenderTargets.end())
  {
    it->second.release();
    m_inputRenderTargets.erase(it);
  }
}

TextureRenderTarget RenderList::renderTargetForInputPort(const Port& p) const noexcept
{
  auto it = m_inputRenderTargets.find(&p);
  if(it != m_inputRenderTargets.end())
    return it->second;
  return {};
}

void RenderList::release()
{
  for(auto node : renderers)
  {
    node->release(*this);
  }

  for(auto& [port, rt] : m_inputRenderTargets)
  {
    rt.release();
  }
  m_inputRenderTargets.clear();

  for(auto& bufs : m_vertexBuffers)
  {
    for(auto& b : bufs.second.buffers)
    {
      // Only delete buffers this RenderList owns. Borrowed gpu_buffer
      // handles (e.g., the scene preprocessor's MDI arena buffers, the
      // GpuResourceRegistry's arena buffers wrapped as gpu_buffer in the
      // emitted geometry) are destroyed by their original producer and
      // must NOT be raw-deleted here — otherwise the later
      // registry->destroy() hits a freed pointer in
      // QRhiResource::deleteLater.
      if(b.owned && b.handle)
        delete b.handle;
    }
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

  // The 3 typed empty-texture placeholders are also allocated in init()
  // but were originally missing from the release path — they leaked on
  // every maybeRebuild cycle (ASan flagged both createRenderList's and
  // maybeRebuild's init() call sites).
  delete m_emptyTexture3D;
  m_emptyTexture3D = nullptr;

  delete m_emptyTextureCube;
  m_emptyTextureCube = nullptr;

  delete m_emptyTextureArray;
  m_emptyTextureArray = nullptr;

  // Destroy the GPU arena registry before the QRhi goes away — its
  // buffers reference the same rhi, and we want them torn down while
  // the backend is still valid. Route through the RenderList-aware
  // overload so arena buffers go through releaseBuffer(), matching
  // every other QRhiBuffer lifetime in the pipeline.
  if(m_registry)
  {
    m_registry->destroy(*this);
    m_registry.reset();
  }

  if(m_vertexFallbackPool)
  {
    m_vertexFallbackPool->release();
    m_vertexFallbackPool.reset();
  }

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

void RenderList::releaseBuffer(QRhiBuffer* buf)
{
  if(!buf)
    return;

  for(auto& vb : m_vertexBuffers)
  {
    // It will be deleted later.
    for(auto& stored_buffer : vb.second.buffers)
      if(stored_buffer.handle == buf)
        return;
  }

  // Don't call destroy() immediately — the buffer may still be referenced
  // by pending uploadStaticBuffer operations in the current frame's batch.
  // deleteLater() defers destruction to the next beginFrame(), ensuring
  // the GPU handle stays valid for all queued operations this frame.
  buf->deleteLater();
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

    // Create all input render targets centrally before any node init().
    // This ensures RTs are available regardless of init order,
    // which is required for delayed (feedback) edges.
    createAllInputRenderTargets();

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
  // Check renderer's own override first (output nodes, Crousti/halp renderers
  // that manage their own render targets)
  if(auto sink_node = edge.sink->node)
    if(auto it = sink_node->renderedNodes.find(this);
       it != sink_node->renderedNodes.end())
    {
      auto renderer = it->second;
      auto tex = renderer->renderTargetForInput(*edge.sink);
      if(tex.renderTarget && tex.renderPass)
        return tex;
    }

  // Fall through to centralized render target map.
  // This covers nodes whose renderers don't manage their own RTs
  // (ISF, CSF, etc.) and delayed (feedback) edges where the sink
  // renderer may not have been init'd yet.
  if(auto it = m_inputRenderTargets.find(edge.sink); it != m_inputRenderTargets.end())
    if(it->second.renderTarget && it->second.renderPass)
      return it->second;

  return {};
}

BufferView RenderList::bufferForInput(const Edge& edge) const noexcept
{
  if(auto source_node = edge.source->node)
    if(auto source_it = source_node->renderedNodes.find(this);
       source_it != source_node->renderedNodes.end())
      if(auto source_renderer = source_it->second)
        return source_renderer->bufferForOutput(*edge.source);

  return {};
}
BufferView RenderList::bufferForOutput(const Edge& edge) const noexcept
{
  if(auto sink_node = edge.sink->node)
    if(auto sink_it = sink_node->renderedNodes.find(this);
       sink_it != sink_node->renderedNodes.end())
      if(auto sink_renderer = sink_it->second)
        return sink_renderer->bufferForInput(*edge.source);

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
  auto& rhi = *state.rhi;
  // 1. Try to find mesh from the exact same geometry
  const auto& [p, f] = spec;

  auto dump_bufs = [](const char* tag, CustomMesh* m, const MeshBuffers& mb) {
    if(!::score::gfx::buftrace_enabled())
      return;
    QDebug d = qDebug().nospace();
    d << "[BUFTRACE] " << tag << " mesh=" << (void*)m
      << " bufs.size=" << (qsizetype)mb.buffers.size() << " [";
    for(std::size_t i = 0; i < mb.buffers.size(); ++i)
    {
      if(i)
        d << ",";
      d << (void*)mb.buffers[i].handle;
    }
    d << "] indirect=" << (void*)mb.indirectDrawBuffer;
  };

  if(auto it = m_customMeshCache.find(spec); it != m_customMeshCache.end())
  {
    if(auto m = const_cast<CustomMesh*>(safe_cast<const CustomMesh*>(it->second)))
    {
      auto meshbufs_it = this->m_vertexBuffers.find(m);
      SCORE_ASSERT(meshbufs_it != this->m_vertexBuffers.end());
      auto& mb = meshbufs_it->second;

      if(auto cur_idx = p->dirty_index; m->dirtyGeometryIndex != cur_idx)
      {
        BUFTRACE() << "acquireMesh PATH 1a: dirty_index "
                   << m->dirtyGeometryIndex << "->" << cur_idx
                   << " mesh=" << (void*)m
                   << " spec=" << (void*)p.get();
        dump_bufs("  before reload", m, mb);
        m->reload(*p, f);
        m->update(rhi, mb, res);
        dump_bufs("  after reload", m, mb);
        for(auto& mesh: p->meshes) {
          for(auto& buf : mesh.buffers) {
            buf.dirty = false;
          }
        }

        // FIXME atomic !!
        m->dirtyGeometryIndex = cur_idx;
      }
      else
      {
        bool dirty = false;
        for(auto& mesh: p->meshes) {
          for(auto& buf : mesh.buffers) {
            dirty |= buf.dirty;
          }
        }

        if(dirty)
        {
          BUFTRACE() << "acquireMesh PATH 1b: buf.dirty mesh=" << (void*)m;
          dump_bufs("  before reload", m, mb);
          m->reload(*p, f);
          m->update(rhi, mb, res);
          dump_bufs("  after reload", m, mb);
          for(auto& mesh: p->meshes) {
            for(auto& buf : mesh.buffers) {
              buf.dirty = false;
            }
          }
        }
      }

      return {m, mb};
    }
  }

  // 2. If not found try to see if the mesh is already used
  for(auto it = m_customMeshCache.begin(); it != m_customMeshCache.end(); ++it)
  {
    if(it->second == current)
    {
      if(auto m = const_cast<CustomMesh*>(safe_cast<const CustomMesh*>(current)))
      {
        auto meshbufs_it = this->m_vertexBuffers.find(m);
        SCORE_ASSERT(meshbufs_it != this->m_vertexBuffers.end());
        auto& mb = currentbufs;
        auto cur_idx = p->dirty_index;

        BUFTRACE() << "acquireMesh PATH 2 (reuse): mesh=" << (void*)m
                   << " old_spec=" << (void*)it->first.meshes.get()
                   << " new_spec=" << (void*)p.get();
        dump_bufs("  before reload", m, mb);
        m->reload(*p, f);
        m->update(rhi, mb, res);
        dump_bufs("  after reload", m, mb);

        for(auto& mesh: p->meshes) {
          for(auto& buf : mesh.buffers) {
            buf.dirty = false;
          }
        }

        m->dirtyGeometryIndex = cur_idx;

        // Sync the vertex buffer cache so that path 1 on subsequent frames
        // picks up the updated handles (especially gpu_buffer pointers that
        // were replaced rather than resized in-place).
        meshbufs_it->second = mb;

        // Re-key: erase stale entry and insert under the new geometry_spec
        // to prevent cache growth from feedback loops creating new shared_ptrs each frame.
        m_customMeshCache.erase(it);
        m_customMeshCache[spec] = m;

        return {m, mb};
      }
    }
  }

  // 3. Really not found, we allocate a new mesh for good
  BUFTRACE() << "acquireMesh PATH 3 (fresh): spec=" << (void*)p.get();
  auto m = new CustomMesh{*p, f};
  auto meshbufs = initMeshBuffer(*m, res);

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
  // Check for well-known _indirect_draw auxiliary buffer convention
  if(!meshbufs.useIndirectDraw && !p->meshes.empty())
  {
    const auto& mesh = p->meshes[0];
    if(auto* aux = mesh.find_auxiliary("_indirect_draw"))
    {
      if(aux->buffer >= 0 && aux->buffer < (int)mesh.buffers.size())
      {
        const auto& buf_data = mesh.buffers[aux->buffer].data;
        if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&buf_data))
        {
          if(gpu->handle)
          {
            meshbufs.indirectDrawBuffer = static_cast<QRhiBuffer*>(gpu->handle);
            meshbufs.useIndirectDraw = true;
            meshbufs.indirectDrawIndexed = false;
          }
        }
      }
    }
    else if(auto* aux_idx = mesh.find_auxiliary("_indirect_draw_indexed"))
    {
      if(aux_idx->buffer >= 0 && aux_idx->buffer < (int)mesh.buffers.size())
      {
        const auto& buf_data = mesh.buffers[aux_idx->buffer].data;
        if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&buf_data))
        {
          if(gpu->handle)
          {
            meshbufs.indirectDrawBuffer = static_cast<QRhiBuffer*>(gpu->handle);
            meshbufs.useIndirectDraw = true;
            meshbufs.indirectDrawIndexed = true;
          }
        }
      }
    }
  }
#endif

  this->m_customMeshCache[{p, f}] = m;
  return {m, meshbufs};
}

void RenderList::clearRenderers()
{
  renderers.clear();

  // Necessary so that we re-go through init() on the next frame
  m_built = false;
}

bool RenderList::requiresDepth(const Port& p) const noexcept
{
  for(auto& edge : p.edges)
    if(edge->source->node->requiresDepth)
      return true;

  return false;
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

  // Frame counter + wall-clock timer for diagnostics. Emits the frame
  // header with the time since the previous render() entry so the pasted
  // log shows per-frame cost. Values include CPU record + any synchronous
  // GPU waits inside setShaderResources / beginPass etc., i.e. roughly
  // the wall-time equivalent of "how fast is this pipeline".
  // Plan 09 S6: per-frame GPU-time + PSO-stall observability. Read the
  // CB-wide GPU time for the most recently COMPLETED frame and attribute
  // it to the "frame" label; the per-pass breakdown is a QRhi follow-up
  // (current API only exposes CB-scoped timings).
  //
  // One-frame staleness is a QRhi contract: `lastCompletedGpuTime()`
  // returns the PREVIOUS frame's elapsed GPU time, not the in-progress
  // one. The panel reports it as such.
  static int s_frameNumber = 0;
  ++s_frameNumber;
  if(state.caps.timestamps)
  {
    const double last_ms = commands.lastCompletedGpuTime();
    if(last_ms > 0.0)
      m_gpuTimings.record("frame", last_ms);
  }
  // PSO stall telemetry: sample totalPipelineCreationTime, compute the
  // delta since last frame. A spike > 10 ms means a new PSO compiled
  // on the hot path — usually a cold cache or new preset variant.
  if(state.rhi)
  {
    static thread_local qint64 s_lastPsoCreationNs = 0;
    const auto stats = state.rhi->statistics();
    const qint64 delta_ns = stats.totalPipelineCreationTime - s_lastPsoCreationNs;
    s_lastPsoCreationNs = stats.totalPipelineCreationTime;
    const double delta_ms = double(delta_ns) / 1'000'000.0;
    if(delta_ms > 10.0)
    {
      qWarning().noquote().nospace()
          << "[GPU] PSO compile stall on frame " << s_frameNumber
          << ": " << delta_ms << " ms — consider prewarming preset pipelines.";

      // Plan 09 S6: mid-session pipeline-cache flush. When a stall
      // hits we've just compiled one or more fresh PSOs — good time
      // to persist the cache so the same compilation doesn't have to
      // happen again on next launch, even if score crashes. Throttled
      // to at most once per ~5s (300 frames at 60 Hz) to avoid
      // churning the cache file on prolonged compile-heavy scenes.
      static thread_local int s_flushCoolDown = 0;
      if(s_flushCoolDown <= 0 && state.savePipelineCache)
      {
        state.savePipelineCache();
        s_flushCoolDown = 300;
      }
      if(s_flushCoolDown > 0)
        --s_flushCoolDown;
    }
    // Also record into the timings panel so it shows up next to frame
    // time. Zero deltas are filtered out by GpuTimings::record.
    m_gpuTimings.record("pso_compile", delta_ms);
  }
  m_gpuTimings.tickFrame();

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
    // Surgical render target update: only recreate the specific RTs and
    // passes that actually changed, rather than destroying everything.
    //
    // Process output node first (its RT size/format determines upstream defaults),
    // then intermediate nodes.

    // Pass 1: output node
    if(auto out_it = output.renderedNodes.find(this);
       out_it != output.renderedNodes.end())
    {
      auto* outRenderer = out_it->second;
      if(outRenderer->renderTargetSpecsChanged)
      {
        // Output renderer owns its RT — re-init it.
        outRenderer->releaseState(*this);
        outRenderer->initState(*this, *updateBatch);
        outRenderer->checkForChanges();
        outRenderer->materialChanged = true;
        outRenderer->geometryChanged = true;
        outRenderer->renderTargetSpecsChanged = false;

        // Recreate upstream passes that target the output's input ports.
        for(auto* in : output.input)
        {
          for(auto* edge : in->edges)
          {
            auto src_it = edge->source->node->renderedNodes.find(this);
            if(src_it != edge->source->node->renderedNodes.end())
            {
              src_it->second->removeOutputPass(*this, *edge);
              src_it->second->addOutputPass(*this, *edge, *updateBatch);
            }
          }
        }
      }
    }

    // Pass 2: intermediate nodes with changed RT specs
    for(auto* renderer : renderers)
    {
      if(!renderer->renderTargetSpecsChanged)
        continue;
      // Skip output node (handled above)
      if(&renderer->node == &output)
        continue;

      int cur_port = 0;
      for(auto* in : renderer->node.input)
      {
        if(in->type == Types::Image
           && (in->flags & Flag::GrabsFromSource) != Flag::GrabsFromSource)
        {
          auto newSpec = renderer->node.resolveRenderTargetSpecs(cur_port, *this);
          auto oldIt = m_inputRenderTargets.find(in);

          bool specChanged = false;
          if(oldIt != m_inputRenderTargets.end())
          {
            auto* oldTex = oldIt->second.texture;
            if(oldTex)
              specChanged = (oldTex->format() != newSpec.format)
                         || (oldTex->pixelSize() != newSpec.size);
          }

          // Always update sampler filter settings when specs changed
          // (filter/address changes don't require RT recreation)
          renderer->updateInputSamplerFilter(*in, newSpec);

          if(specChanged)
          {
            // Remove upstream passes that target this port
            for(auto* edge : in->edges)
            {
              auto src_it = edge->source->node->renderedNodes.find(this);
              if(src_it != edge->source->node->renderedNodes.end())
                src_it->second->removeOutputPass(*this, *edge);
            }

            // Recreate the render target
            oldIt->second.release();
            bool wantsDepth = requiresDepth(*in);
            bool wantsSamplableDepth
                = (in->flags & Flag::SamplableDepth) == Flag::SamplableDepth;
            oldIt->second = score::gfx::createRenderTarget(
                state, newSpec.format, newSpec.size, samples(),
                wantsDepth || wantsSamplableDepth, wantsSamplableDepth);

            // Recreate upstream passes with the new RT
            for(auto* edge : in->edges)
            {
              auto src_it = edge->source->node->renderedNodes.find(this);
              if(src_it != edge->source->node->renderedNodes.end())
                src_it->second->addOutputPass(*this, *edge, *updateBatch);
            }

            // Update this node's own sampler to point to the new RT texture.
            // The sampler was created during initState() pointing to the old
            // texture which was just released.
            if(oldIt->second.texture)
              renderer->updateInputTexture(*in, oldIt->second.texture, oldIt->second.depthTexture);
          }
        }
        cur_port++;
      }

      renderer->renderTargetSpecsChanged = false;
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
  static thread_local ossia::flat_set<const score::gfx::Node*> updated_nodes;
  updated_nodes.clear();

  const auto prepare_render
      = [this, &prevRenderers, &commands, &updateBatch](score::gfx::Port* input) {
    prevRenderers.clear();
    prevRenderers.reserve(input->edges.size());

    // First update them all and store them in prevRenderers (saves a couple lookups)
    for(auto edge : input->edges)
    {
      auto src = edge->source;
      if(!src)
        continue;

      auto rn_it = src->node->renderedNodes.find(this);
      if(rn_it == src->node->renderedNodes.end())
        continue; // Source node has no renderer in this RL (transient during incremental update)

      NodeRenderer* prev_renderer = rn_it->second;

      prevRenderers.push_back({edge, prev_renderer});

      prev_renderer->update(*this, *updateBatch, edge);
      updated_nodes.insert(&prev_renderer->node);
    }

    if(prevRenderers.size() == 0)
    {
      if(updateBatch)
      {
        commands.resourceUpdate(updateBatch);
      }
      updateBatch = state.rhi->nextResourceUpdateBatch();
      SCORE_ASSERT(updateBatch);
    }
    else
    {
      // For nodes that perform multiple rendering passes,
      // pre-computations in compute shaders, etc... run them now.
      // Most nodes don't do anything there.
      for(auto [edge, prev_renderer] : prevRenderers)
      {
        if(updateBatch)
        {
          commands.resourceUpdate(updateBatch);
        }
        updateBatch = state.rhi->nextResourceUpdateBatch();
        SCORE_ASSERT(updateBatch);

        prev_renderer->runInitialPasses(*this, commands, updateBatch, *edge);
      }
    }
  };

  // We render each node for this frame in-order (first to last / source before sink)
  for(auto it = this->nodes.rbegin(); it != this->nodes.rend(); ++it)
  {
    auto node = *it;
    for(auto input : node->input)
    {
      // For each edge incoming to each image input ports of this node,
      // we render the edge source's content.
      if(input->edges.empty())
        continue;

      if(input->type == Types::Image)
      {
        const bool grabs = (input->flags & Flag::GrabsFromSource) == Flag::GrabsFromSource;

        prepare_render(input);

        if(grabs)
        {
          // GrabsFromSource: upstream already produced the texture
          // in runInitialPasses. No render pass needed.
          // Update the downstream node's sampler to point to the
          // upstream's current texture (it may have changed since init).
          auto rendered = node->renderedNodes.find(this);
          if(rendered == node->renderedNodes.end())
            continue;
          NodeRenderer* sink_renderer = rendered->second;

          for(auto [edge, prev_renderer] : prevRenderers)
          {
            if(auto* srcTex = prev_renderer->textureForOutput(*edge->source))
            {
              auto rt = renderTargetForInputPort(*input);
              sink_renderer->updateInputTexture(*input, srcTex, rt.depthTexture);
            }
          }

          if(updateBatch)
          {
            commands.resourceUpdate(updateBatch);
            updateBatch = nullptr;
          }
        }
        else
        {
          // Then do the final render of each node on the edge sink's render target
          // We *have* to do that in a single beginPass / endPass as every beginPass
          // issues a clearBuffers command.
          {
            auto rendered = node->renderedNodes.find(this);
            if(rendered == node->renderedNodes.end())
            {
              if(updateBatch)
              {
                commands.resourceUpdate(updateBatch);
                updateBatch = nullptr;
              }
              updateBatch = state.rhi->nextResourceUpdateBatch();
              continue;
            }
            NodeRenderer* renderer = rendered->second;

            auto rt = renderer->renderTargetForInput(*input);
            if(!rt)
              rt = renderTargetForInputPort(*input);
            if(rt)
            {
              QColor bg = (it + 1 == this->nodes.rend() ? Qt::black : Qt::transparent);
              commands.beginPass(rt.renderTarget, bg, {0.0f, 0}, updateBatch);
              updateBatch = nullptr;

              // FIXME z-sort
              for(auto [edge, prev_renderer] : prevRenderers)
              {
                prev_renderer->runRenderPass(*this, commands, *edge);
              }

              // Allow the node to do some actions, for instance if a readback
              // of a node's input is going to be needed.
              {
                renderer->inputAboutToFinish(*this, *input, updateBatch);
              }
              commands.endPass(updateBatch);
              updateBatch = nullptr;
            }
            else
            {
              commands.resourceUpdate(updateBatch);
              updateBatch = nullptr;
            }
          }
        }

        if(node != &output)
        {
          SCORE_ASSERT(!updateBatch);
          updateBatch = state.rhi->nextResourceUpdateBatch();
          SCORE_ASSERT(updateBatch);
        }
      }
      else if(input->type == Types::Buffer || input->type == Types::Geometry || input->type == Types::Scene)
      {
        prepare_render(input);

        {
          auto rendered = node->renderedNodes.find(this);
          if(rendered == node->renderedNodes.end())
            continue;
          NodeRenderer* renderer = rendered->second;

          if(updateBatch)
          {
            commands.resourceUpdate(updateBatch);
            updateBatch = nullptr;
          }

          renderer->inputAboutToFinish(*this, *input, updateBatch);

          if(updateBatch)
          {
            commands.resourceUpdate(updateBatch);
            updateBatch = nullptr;
          }
        }

        if(node != &output)
        {
          SCORE_ASSERT(!updateBatch);
          updateBatch = state.rhi->nextResourceUpdateBatch();
          SCORE_ASSERT(updateBatch);
        }
      }
    }
  }

  // Finally the output node may have some rendering to do too
  {
    if(this->output.renderedNodes.empty())
    {
      // Pool-leak fix: updateBatch was allocated earlier in the render
      // loop (line 769 or via the per-edge prepare_render path) and
      // must be returned before bailing out — otherwise the pool slot
      // stays pinned until the QRhi is destroyed, and during rapid
      // resize this condition can fire many times in succession.
      if(updateBatch) { updateBatch->release(); updateBatch = nullptr; }
      return;
    }
    auto output_renderer
        = dynamic_cast<OutputNodeRenderer*>(this->output.renderedNodes.begin()->second);
    if(!output_renderer)
    {
      if(updateBatch) { updateBatch->release(); updateBatch = nullptr; }
      return;
    }

    if(this->output.configuration().outputNeedsRenderPass)
    {
      if(!updateBatch)
      {
        updateBatch = state.rhi->nextResourceUpdateBatch();
        SCORE_ASSERT(updateBatch);
      }

      // FIXME remove this hack
      score::gfx::Port p;
      score::gfx::Edge dummy{&p, &p, Process::CableType::ImmediateGlutton};
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
    m_outputUBOData.sampleCount = m_samples;

    res.updateDynamicBuffer(m_outputUBO, 0, sizeof(OutputUBO), &m_outputUBOData);
  }
}

}
