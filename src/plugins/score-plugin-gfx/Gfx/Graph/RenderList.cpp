
#include <atomic>
#include <ossia/detail/flat_map.hpp>
#include <mutex>
#include <Gfx/Graph/CustomMesh.hpp>
#include <Gfx/Graph/GpuResourceRegistry.hpp>
#include <Gfx/Graph/Mesh.hpp>
#include <Gfx/Graph/MipGeneration.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/PipelineStateHelpers.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <Gfx/Graph/RhiIndirectCompat.hpp>
#include <Gfx/Graph/VertexFallbackPool.hpp>
#include <Gfx/Settings/Model.hpp>

#include <score/tools/Debug.hpp>

#include <QVarLengthArray>

#include <array>
#include <chrono>

//#define RENDERDOC_PROFILING 0
#if defined(RENDERDOC_PROFILING)
#include "renderdoc_app.h"

#include <dlfcn.h>
#endif

#include <iostream>
#include <exception>

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
  // release() is idempotent, and running it here covers the shutdown race where a
  // late onResize spawns a new RenderList through Graph::recreateOutputRenderList
  // after ~Graph has passed its release loop. That RL would otherwise reach
  // ~RenderList with its QRhi resources unfreed, and destroyOutput()'s
  // RenderState::destroy() would call vkDestroyDevice on a device that still owns
  // them.
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

  // Typed placeholders so a shader declaring sampler3D / samplerCube /
  // sampler2DArray / sampler2D can be bound to a matching view before any upstream
  // edge delivers a real texture; otherwise VUID-vkCmdDraw-viewType-07752 fires
  // every frame.
  //
  // create() must succeed: a null handle reaches vkUpdateDescriptorSets as
  // VK_NULL_HANDLE and the NVIDIA driver segfaults dereferencing it in a later
  // vkCmdPipelineBarrier.
  //
  // UsedWithLoadStore on every placeholder: an unconnected input can be a
  // STORAGE IMAGE as easily as a sampled texture -- a CSF declaring
  // `TYPE: image, VISIBILITY: compute` binds one through the same fallback --
  // and Qt asserts when a texture reaches a storage-image binding without the
  // flag:
  //     ASSERT: texD->m_flags.testFlag(QRhiTexture::UsedWithLoadStore)
  //     qrhivulkan.cpp:6336, from QRhiVulkan::setShaderResources
  // The flag only widens the usage bits the backend requests at creation, so it
  // costs nothing when the texture is merely sampled.
  constexpr auto emptyFlags = QRhiTexture::UsedWithLoadStore;

  m_emptyTexture = rhi.newTexture(QRhiTexture::RGBA8, QSize{1, 1}, 1, emptyFlags);
  m_emptyTexture->setName("RenderList::m_emptyTexture");
  SCORE_ASSERT(m_emptyTexture->create());

  m_emptyTexture3D = rhi.newTexture(
      QRhiTexture::RGBA8, 1, 1, 1, 1,
      QRhiTexture::ThreeDimensional | emptyFlags);
  m_emptyTexture3D->setName("RenderList::m_emptyTexture3D");
  SCORE_ASSERT(m_emptyTexture3D->create());

  m_emptyTextureCube = rhi.newTexture(
      QRhiTexture::RGBA8, QSize{1, 1}, 1, QRhiTexture::CubeMap | emptyFlags);
  m_emptyTextureCube->setName("RenderList::m_emptyTextureCube");
  SCORE_ASSERT(m_emptyTextureCube->create());

  // Must use newTextureArray — the 6-arg newTexture() overload is for 3D
  // textures (depth > 1 is a volume slice count, not an array layer count),
  // and QRhi rejects any texture with both ThreeDimensional and TextureArray
  // flags.
  m_emptyTextureArray = rhi.newTextureArray(
      QRhiTexture::RGBA8, /*arraySize*/ 1, QSize(1, 1), 1, emptyFlags);
  m_emptyTextureArray->setName("RenderList::m_emptyTextureArray");
  SCORE_ASSERT(m_emptyTextureArray->create());

  // Allocate the initial resource-update batch before the registry init would, so
  // the empty-texture zero-fills queue into the same batch. Vulkan does not
  // zero-initialise new VkImage memory, and a consumer with no upstream producer
  // for a cubemap or LUT input samples these placeholders directly -- e.g.
  // classic_pbr_openpbr's irradiance_map, prefiltered_map, skybox and brdf_lut.
  //
  // 1x1 RGBA8, 4 bytes per face, ~16 bytes per RL init.
  SCORE_ASSERT(!m_initialBatch);
  m_initialBatch = state.rhi->nextResourceUpdateBatch();
  SCORE_ASSERT(m_initialBatch);
  {
    static const std::array<char, 4> blackPixel{0, 0, 0, 0};
    QRhiTextureSubresourceUploadDescription src(blackPixel.data(), 4);
    src.setSourceSize(QSize{1, 1});
    // 2D
    {
      QRhiTextureUploadEntry e(0, 0, src);
      m_initialBatch->uploadTexture(m_emptyTexture, {e});
    }
    // 3D — one slice
    {
      QRhiTextureUploadEntry e(0, 0, src);
      m_initialBatch->uploadTexture(m_emptyTexture3D, {e});
    }
    // 2D Array — one layer
    {
      QRhiTextureUploadEntry e(0, 0, src);
      m_initialBatch->uploadTexture(m_emptyTextureArray, {e});
    }
    // Cube — six faces
    {
      QRhiTextureUploadDescription cubeDesc;
      QVarLengthArray<QRhiTextureUploadEntry, 6> entries;
      for(int face = 0; face < 6; ++face)
        entries.append(QRhiTextureUploadEntry(face, 0, src));
      cubeDesc.setEntries(entries.cbegin(), entries.cend());
      m_initialBatch->uploadTexture(m_emptyTextureCube, cubeDesc);
    }
  }

  // Scene-graph arena store (camera / light / material / per_draw
  // buffers). Source nodes grab slots from it at construction and
  // write their own packed bytes at their own update(), so
  // ScenePreprocessor never CPU-touches this data in the render path.
  //
  // Persist-across-rebuild contract: the registry is OWNED by the
  // OutputNode (OutputNode::m_registry). On the first RL for this
  // output it is freshly allocated + init()'d; on every subsequent
  // RL rebuild (viewport resize / fallback rebuild path) we adopt
  // the populated state as-is. Skipping the re-init() preserves
  // ~100 MiB of texture-array layers, ~70 K-vertex mesh slabs, every
  // arena buffer (no zero-fill), and all producer slot indices —
  // none of that scene-content data depends on framebuffer size.
  m_registry = &output.acquireRegistry();
  if(!m_registry->isInitialized())
  {
    m_registry->init(rhi, *m_initialBatch);
    // Seed reserved arena slots (e.g. Material slot 0 = default white
    // dielectric). Runs after registry init so the seed lands AFTER the
    // arena zero-fill (uploadStaticBuffer ordering is preserved within
    // the same batch). Idempotent on repeat calls but we gate it here
    // anyway so the explicit upload only happens when the arena was
    // actually re-initialised this RL cycle.
    m_registry->seedDefaults(*m_initialBatch);
  }
  else
  {
    // Reuse path. Arena buffers, texture arrays, mesh slabs and slot
    // generations all carry over from the previous RL on this output.
    // Producers' raw_*_slot members survive (the renderers themselves
    // are recreated on RL rebuild — they re-allocate fresh slots — but
    // the slot-stride / generation-table / free-list state is intact).
    // ScenePreprocessor::init() compares against this same pointer to
    // decide whether to wipe its m_loaderMaterialSlots / m_envSlot
    // bookkeeping; matching pointer → no wipe → no re-allocation churn.
    SCORE_ASSERT(m_registry->boundRhi() == &rhi);
  }

  // Fallback vertex-buffer pool for "REQUIRED: false" VERTEX_INPUTS.
  // Lazy-allocates on first use (remapPipelineVertexInputs side), so
  // zero cost when no shader opts in.
  m_vertexFallbackPool = std::make_unique<VertexFallbackPool>();

  m_lastSize = state.renderSize;
}

QRhiResourceUpdateBatch* RenderList::initialBatch() const noexcept
{
  return m_initialBatch;
}

void RenderList::flushInitialBatch()
{
  if(!m_initialBatch)
    return;

  auto* rhi = state.rhi;
  if(!rhi)
  {
    m_initialBatch = nullptr;
    return;
  }

  if(OffscreenFrame frame{*rhi})
  {
    frame.commands().resourceUpdate(m_initialBatch);
    retireResourcesReleasedOutsideFrame();
    frame.end();
  }
  else
  {
    m_initialBatch->release();
    retireResourcesReleasedOutsideFrame();
  }
  m_initialBatch = nullptr;
}

static bool isInletRenderTarget(const Port& p) noexcept
{
  return p.type == Types::Image
         && (p.flags & Flag::GrabsFromSource) != Flag::GrabsFromSource;
}

static int32_t inputIndex(const Node& node, const Port& p) noexcept
{
  for(std::size_t i = 0; i < node.input.size(); ++i)
    if(node.input[i] == &p)
      return int32_t(i);
  return -1;
}

QSize RenderList::resolveInletSize(
    const Port& in,
    const ossia::small_flat_map<const Port*, RenderTargetSpecs, 16>& resolvedSpecs,
    ossia::flat_set<const Node*>& visiting) const noexcept
{
  if(auto it = resolvedSpecs.find(&in); it != resolvedSpecs.end())
    return it->second.size;

  const Node* node = in.node;
  if(!node || node == &output)
    return state.renderSize;

  const int32_t port = inputIndex(*node, in);
  if(auto it = node->renderTargetSpecs.find(port);
     it != node->renderTargetSpecs.end() && it->second.size)
    return QSize{it->second.size->width, it->second.size->height};

  if(!visiting.insert(node).second)
    return {};
  const QSize downstream = resolveDownstreamSize(node, resolvedSpecs, visiting);
  visiting.erase(node);
  return downstream.isEmpty() ? state.renderSize : downstream;
}

QSize RenderList::resolveDownstreamSize(
    const Node* node,
    const ossia::small_flat_map<const Port*, RenderTargetSpecs, 16>& resolvedSpecs,
    ossia::flat_set<const Node*>& visiting) const noexcept
{
  QSize best{0, 0};

  for(const auto* out_port : node->output)
  {
    for(const auto* edge : out_port->edges)
    {
      const Port* sink = edge->sink;
      if(!sink || !sink->node)
        continue;

      QSize sz;
      if(sink->node == &output)
      {
        sz = state.renderSize;
      }
      else
      {
        if(!isInletRenderTarget(*sink))
          continue;
        if(std::find(nodes.begin(), nodes.end(), sink->node) == nodes.end()
           && sink->node->renderedNodes.find(this) == sink->node->renderedNodes.end())
          continue;
        sz = resolveInletSize(*sink, resolvedSpecs, visiting);
      }

      best = QSize(std::max(best.width(), sz.width()), std::max(best.height(), sz.height()));
    }
  }

  return best;
}

QSize RenderList::resolveDownstreamSize(
    const Node* node,
    const ossia::small_flat_map<const Port*, RenderTargetSpecs, 16>& resolvedSpecs)
    const noexcept
{
  ossia::flat_set<const Node*> visiting;
  visiting.insert(node);
  return resolveDownstreamSize(node, resolvedSpecs, visiting);
}

RenderTargetSpecs
RenderList::resolveInputRenderTargetSpecs(const Node& node, int32_t port) noexcept
{
  auto spec = node.resolveRenderTargetSpecs(port, *this);
  if(&node != &output && !node.hasExplicitRenderTargetSize(port))
  {
    const QSize downstream = resolveDownstreamSize(&node, {});
    if(!downstream.isEmpty())
      spec.size = downstream;
  }
  return spec;
}

static QRhiTexture::Flags inputRenderTargetFlags(const RenderTargetSpecs& spec) noexcept
{
  QRhiTexture::Flags flags{};
  if(spec.mipmap_mode != QRhiSampler::None)
    flags |= QRhiTexture::MipMapped | QRhiTexture::UsedWithGenerateMips;
  return flags;
}

void RenderList::createAllInputRenderTargets()
{
  ossia::small_flat_map<const Port*, RenderTargetSpecs, 16> resolvedSpecs;

  for(auto* node : nodes)
  {
    if(node == &output)
      continue;

    int cur_port = 0;
    for(auto* in : node->input)
    {
      if(isInletRenderTarget(*in))
        resolvedSpecs[in] = resolveInputRenderTargetSpecs(*node, cur_port);
      cur_port++;
    }
  }

  // Step 2: create render targets using resolved specs.
  for(auto& [port, spec] : resolvedSpecs)
  {
    bool wantsDepth = requiresDepth(*port);
    bool wantsSamplableDepth
        = (port->flags & Flag::SamplableDepth) == Flag::SamplableDepth;
    // A mip chain is only worth allocating when the consuming sampler
    // filters across levels; otherwise levels > 0 are storage nothing
    // writes and nothing reads.
    auto rt = score::gfx::createRenderTarget(
        state, spec.format, spec.size, samples(),
        wantsDepth || wantsSamplableDepth, wantsSamplableDepth,
        inputRenderTargetFlags(spec));
    m_inputRenderTargets[port] = std::move(rt);
  }

  ensureSelfFeedbackTargets();
}

void RenderList::onEdgeRemoved(
    Edge& edge, const ossia::hash_set<const Port*>* preserveSinks)
{
  // removeOutputPass / removeInputRenderTarget below destroy resources that a
  // still-pending initial batch may name (e.g. a processUBO uploaded by
  // addOutputPass in this same inter-frame window).
  flushInitialBatch();

  // Notify source renderer
  if(auto src_it = edge.source->node->renderedNodes.find(this);
     src_it != edge.source->node->renderedNodes.end())
  {
    src_it->second->removeOutputPass(*this, edge);
  }

  // Notify sink renderer
  if(auto sink_it = edge.sink->node->renderedNodes.find(this);
     sink_it != edge.sink->node->renderedNodes.end())
  {
    sink_it->second->removeInputEdge(*this, edge);
  }

  // If the sink port has no more edges after this one is removed
  // (called before actual edge destruction, so the edge is still in the list),
  // release the render target — unless the caller has told us a new feed
  // is coming in the same batch. The caller is responsible for only marking
  // sinks whose RT specs will remain valid; a mismatch is picked up later by
  // the rt_changed surgical path in render().
  if(edge.sink->edges.size() <= 1)
  {
    if(!preserveSinks || !preserveSinks->contains(edge.sink))
    {
      // The sink node may stay reachable through other edges: its renderer
      // is kept, and its SRB would keep sampling the RT texture released
      // below (a full rebuild re-binds every SRB; this incremental path
      // must rebind explicitly). Point the sampler back at the empty
      // texture first — including the depth slot for SamplableDepth ports,
      // whose depth texture is released together with the RT.
      if(!((edge.sink->flags & Flag::GrabsFromSource) == Flag::GrabsFromSource))
      {
        if(auto sink_it = edge.sink->node->renderedNodes.find(this);
           sink_it != edge.sink->node->renderedNodes.end())
        {
          const bool samplableDepth
              = (edge.sink->flags & Flag::SamplableDepth) == Flag::SamplableDepth;
          sink_it->second->updateInputTexture(
              *edge.sink, &emptyTextureFor(*edge.sink),
              samplableDepth ? &emptyTexture() : nullptr);
        }
      }
      removeInputRenderTarget(edge.sink);
    }
  }
}

static bool isSelfFed(const Port& p) noexcept
{
  for(auto* edge : p.edges)
    if(edge->sink == &p && edge->source && edge->source->node == p.node)
      return true;
  return false;
}

void RenderList::removeSelfFeedbackTarget(const Port* port)
{
  auto it = m_selfFeedbackTargets.find(port);
  if(it != m_selfFeedbackTargets.end())
  {
    it->second.back.release();
    m_selfFeedbackTargets.erase(it);
  }
}

bool RenderList::ensureSelfFeedbackTarget(const Port& in)
{
  if(in.type != Types::Image
     || (in.flags & Flag::GrabsFromSource) == Flag::GrabsFromSource)
    return false;
  if(!in.node || in.node == &output || !isSelfFed(in))
    return false;
  if(m_selfFeedbackTargets.find(&in) != m_selfFeedbackTargets.end())
    return false;
  if(auto rn = in.node->renderedNodes.find(this); rn != in.node->renderedNodes.end())
    if(rn->second->renderTargetForInput(in))
      return false;
  auto front = m_inputRenderTargets.find(&in);
  if(front == m_inputRenderTargets.end() || !front->second.texture)
    return false;

  auto* tex = front->second.texture;
  const bool wantsSamplableDepth
      = (in.flags & Flag::SamplableDepth) == Flag::SamplableDepth;
  auto back = score::gfx::createRenderTarget(
      state, tex->format(), tex->pixelSize(), samples(),
      requiresDepth(in) || wantsSamplableDepth, wantsSamplableDepth, tex->flags());
  if(!back.renderTarget)
  {
    back.release();
    return false;
  }
  m_selfFeedbackTargets[&in] = {std::move(back), false};
  return true;
}

static int copyLayerCount(const QRhiTexture& tex, int level) noexcept
{
  const auto flags = tex.flags();
  if(flags.testFlag(QRhiTexture::CubeMap))
    return 6;
  if(flags.testFlag(QRhiTexture::TextureArray))
    return std::max(1, tex.arraySize());
  if(flags.testFlag(QRhiTexture::ThreeDimensional))
    return std::max(1, tex.depth() >> level);
  return 1;
}

static void copyTextureLevels(
    QRhi& rhi, QRhiResourceUpdateBatch& res, QRhiTexture* dst, QRhiTexture* src)
{
  const int levels = src->flags().testFlag(QRhiTexture::MipMapped)
                         ? rhi.mipLevelsForSize(src->pixelSize())
                         : 1;
  for(int level = 0; level < levels; ++level)
  {
    const QSize size = rhi.sizeForMipLevel(level, src->pixelSize());
    const int layers = copyLayerCount(*src, level);
    for(int layer = 0; layer < layers; ++layer)
    {
      QRhiTextureCopyDescription desc;
      desc.setPixelSize(size);
      desc.setSourceLevel(level);
      desc.setDestinationLevel(level);
      desc.setSourceLayer(layer);
      desc.setDestinationLayer(layer);
      res.copyTexture(dst, src, desc);
    }
  }
}

static bool isDepthFormat(QRhiTexture::Format fmt) noexcept
{
  switch(fmt)
  {
    case QRhiTexture::D16:
    case QRhiTexture::D24:
    case QRhiTexture::D24S8:
    case QRhiTexture::D32F:
    case QRhiTexture::D32FS8:
      return true;
    default:
      return false;
  }
}

static bool sameTextureShape(const QRhiTexture& a, const QRhiTexture& b) noexcept
{
  return a.format() == b.format() && a.pixelSize() == b.pixelSize()
         && a.flags() == b.flags() && a.arraySize() == b.arraySize()
         && a.depth() == b.depth() && a.sampleCount() == b.sampleCount();
}

static QRhiTexture* newTextureLike(QRhi& rhi, const QRhiTexture& tex)
{
  QRhiTexture* ret{};
  if(tex.flags().testFlag(QRhiTexture::ThreeDimensional))
    ret = rhi.newTexture(
        tex.format(), tex.pixelSize().width(), tex.pixelSize().height(), tex.depth(),
        1, tex.flags());
  else if(tex.flags().testFlag(QRhiTexture::TextureArray))
    ret = rhi.newTextureArray(
        tex.format(), tex.arraySize(), tex.pixelSize(), 1, tex.flags());
  else
    ret = rhi.newTexture(tex.format(), tex.pixelSize(), 1, tex.flags());
  if(!ret)
    return nullptr;
  ret->setName("RenderList::selfFeedbackGrab");
  if(!ret->create())
  {
    delete ret;
    return nullptr;
  }
  return ret;
}

static void generateInputMips(
    QRhi& rhi, const TextureRenderTarget& rt, QRhiResourceUpdateBatch*& res)
{
  if(!rt.texture || !rt.texture->flags().testFlag(QRhiTexture::UsedWithGenerateMips))
    return;
  if(!res)
    res = rhi.nextResourceUpdateBatch();
  if(res)
    generateMipsIfAny(*res, rt.texture);
}

QRhiTexture* RenderList::selfFeedbackGrab(const Port& in) const noexcept
{
  if(auto it = m_selfFeedbackGrabs.find(&in); it != m_selfFeedbackGrabs.end())
    return it->second;
  if(auto it = m_selfFeedbackPlaceholders.find(&in);
     it != m_selfFeedbackPlaceholders.end())
    return it->second;
  return nullptr;
}

QRhiTexture& RenderList::emptyTextureFor(const Port& in) const noexcept
{
  if((in.flags & Flag::Cubemap) == Flag::Cubemap)
    return emptyTextureCube();
  if((in.flags & Flag::ThreeDimensional) == Flag::ThreeDimensional)
    return emptyTexture3D();
  if((in.flags & Flag::TextureArray) == Flag::TextureArray)
    return emptyTextureArray();
  return emptyTexture();
}

void RenderList::updateSelfFeedbackGrabs(QRhiResourceUpdateBatch& res)
{
  for(auto it = m_selfFeedbackGrabs.begin(); it != m_selfFeedbackGrabs.end();)
  {
    if(isSelfFed(*it->first))
    {
      ++it;
      continue;
    }
    it->second->deleteLater();
    it = m_selfFeedbackGrabs.erase(it);
  }
  for(auto it = m_selfFeedbackPlaceholders.begin();
      it != m_selfFeedbackPlaceholders.end();)
  {
    if(isSelfFed(*it->first))
      ++it;
    else
      it = m_selfFeedbackPlaceholders.erase(it);
  }

  for(auto* node : nodes)
  {
    if(node == &output)
      continue;
    auto rn = node->renderedNodes.find(this);
    if(rn == node->renderedNodes.end())
      continue;
    for(auto* in : node->input)
    {
      if(in->type != Types::Image
         || (in->flags & Flag::GrabsFromSource) != Flag::GrabsFromSource)
        continue;
      const Port* source{};
      for(auto* edge : in->edges)
      {
        if(edge->sink == in && edge->source && edge->source->node == node)
        {
          source = edge->source;
          break;
        }
      }
      if(!source)
        continue;
      auto* tex = rn->second->textureForOutput(*source);
      if(!tex)
        continue;
      if(tex->sampleCount() > 1 || isDepthFormat(tex->format()))
      {
        if(auto g = m_selfFeedbackGrabs.find(in); g != m_selfFeedbackGrabs.end())
        {
          g->second->deleteLater();
          m_selfFeedbackGrabs.erase(g);
        }
        if(m_selfFeedbackWarned.insert(in).second)
          qWarning() << "RenderList: a node samples its own"
                     << (isDepthFormat(tex->format()) ? "depth" : "multisampled")
                     << "output (format" << int(tex->format()) << ","
                     << tex->sampleCount()
                     << "samples) through a grabbing input; it reads an empty texture";
        auto* empty = &emptyTextureFor(*in);
        auto ph = m_selfFeedbackPlaceholders.find(in);
        if(ph == m_selfFeedbackPlaceholders.end() || ph->second != empty)
        {
          m_selfFeedbackPlaceholders[in] = empty;
          rn->second->updateInputTexture(
              *in, empty, renderTargetForInputPort(*in).depthTexture);
        }
        continue;
      }
      m_selfFeedbackPlaceholders.erase(in);
      if(!tex->flags().testFlag(QRhiTexture::RenderTarget))
        continue;

      auto* snapshot = selfFeedbackGrab(*in);
      if(!snapshot || !sameTextureShape(*snapshot, *tex))
      {
        auto* fresh = newTextureLike(*state.rhi, *tex);
        if(!fresh)
          continue;
        rn->second->updateInputTexture(
            *in, fresh, renderTargetForInputPort(*in).depthTexture);
        if(snapshot)
          snapshot->deleteLater();
        m_selfFeedbackGrabs[in] = fresh;
        snapshot = fresh;
      }
      copyTextureLevels(*state.rhi, res, snapshot, tex);
    }
  }
}

void RenderList::ensureSelfFeedbackTargets()
{
  for(auto* node : nodes)
    for(auto* in : node->input)
      ensureSelfFeedbackTarget(*in);
}

void RenderList::updateSelfFeedbackTargets(QRhiResourceUpdateBatch& res)
{
  for(auto it = m_selfFeedbackTargets.begin(); it != m_selfFeedbackTargets.end();)
  {
    auto front = m_inputRenderTargets.find(it->first);
    const auto* back = it->second.back.texture;
    const bool keep = front != m_inputRenderTargets.end() && front->second.texture
                      && back && isSelfFed(*it->first)
                      && front->second.texture->format() == back->format()
                      && front->second.texture->pixelSize() == back->pixelSize()
                      && front->second.texture->sampleCount() == back->sampleCount();
    if(!keep)
    {
      it->second.back.release();
      it = m_selfFeedbackTargets.erase(it);
      continue;
    }
    if(std::exchange(it->second.written, false))
    {
      auto& back = it->second.back;
      const Port& port = *it->first;
      if((port.flags & Flag::SamplableDepth) == Flag::SamplableDepth
         && front->second.depthTexture && back.depthTexture)
      {
        std::swap(front->second, back);
        if(auto rn = port.node->renderedNodes.find(this);
           rn != port.node->renderedNodes.end())
          rn->second->updateInputTexture(
              port, front->second.texture, front->second.depthTexture);
      }
      else
      {
        copyTextureLevels(*state.rhi, res, front->second.texture, back.texture);
      }
    }
    ++it;
  }

  for(auto* node : nodes)
  {
    for(auto* in : node->input)
    {
      if(!ensureSelfFeedbackTarget(*in))
        continue;
      auto rn = node->renderedNodes.find(this);
      if(rn == node->renderedNodes.end())
        continue;
      for(auto* edge : in->edges)
      {
        if(edge->source->node != node)
          continue;
        rn->second->removeOutputPass(*this, *edge);
        rn->second->addOutputPass(*this, *edge, res);
      }
    }
  }

  updateSelfFeedbackGrabs(res);
}

void RenderList::removeInputRenderTarget(const Port* port)
{
  removeSelfFeedbackTarget(port);
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

  for(auto& [port, fb] : m_selfFeedbackTargets)
  {
    fb.back.release();
  }
  m_selfFeedbackTargets.clear();

  for(auto& [port, tex] : m_selfFeedbackGrabs)
  {
    tex->deleteLater();
  }
  m_selfFeedbackGrabs.clear();
  m_selfFeedbackPlaceholders.clear();

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

  delete m_emptyTexture3D;
  m_emptyTexture3D = nullptr;

  delete m_emptyTextureCube;
  m_emptyTextureCube = nullptr;

  delete m_emptyTextureArray;
  m_emptyTextureArray = nullptr;

  // Persist-across-rebuild contract: do NOT destroy the registry here.
  // It is owned by the OutputNode and survives RL rebuild — the next
  // createRenderList for this output will re-adopt the same instance
  // and skip the (expensive) init() path. The actual QRhi-resource
  // teardown lives in OutputNode::releaseRegistry() which the concrete
  // sink (ScreenNode / BackgroundNode / MultiWindowNode / ...) calls
  // from destroyOutput() before the QRhi itself is freed. Just clear
  // our non-owning pointer so a stale dereference after release() is
  // a clean nullptr crash, not a use-after-free.
  m_registry = nullptr;

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
  retireResourcesReleasedOutsideFrame();

  m_requiresDepth = false;
  m_ready = false;
  m_built = false;
}

// Shared across every RenderList, because a QRhiBuffer pointer is unique
// process-wide and the producer that retires one is routinely on a different
// render list from the consumer that still binds it. A per-list set makes the
// check blind to exactly the cross-list case.
//
// Two tables, one lock:
//
//  * g_adopted holds the buffers some consumer borrowed with owned=false, with
//    a reference count. An owner's releaseBuffer() on a buffer that is still
//    adopted does not free it: it marks the entry orphaned, and the free
//    happens on the last dropAdoptedBuffer(). This is the invariant the
//    liveness check below verifies -- a bound handle names a live object --
//    made true by construction for every consumer that registers.
//
//  * g_retired holds the buffers actually handed to deleteLater(), stamped
//    with a global frame sequence advanced by every render list that
//    completes a frame. A buffer is "retired" -- actually freed -- only once
//    that sequence has moved past its stamp, since deleteLater() holds the
//    object to the end of the frame that released it.
namespace
{
struct RetiredBuffer
{
  QByteArray name;
  uint64_t seq{};
};
struct AdoptedBuffer
{
  int refs{};
  // The owner released it while it was adopted: the last drop frees it.
  bool orphaned{};
};
std::mutex g_retiredMutex;
ossia::flat_map<const QRhiBuffer*, RetiredBuffer> g_retired;
ossia::flat_map<const QRhiBuffer*, AdoptedBuffer> g_adopted;
std::atomic<uint64_t> g_frameSeq{0};

// Under g_retiredMutex. The name is read here, while the object is alive:
// it is what the liveness warning prints after the object is gone.
void recordRetirement(QRhiBuffer* buf)
{
  g_retired.insert_or_assign(
      buf, RetiredBuffer{buf->name(), g_frameSeq.load(std::memory_order_relaxed)});
}
}

void RenderList::noteFrameCompleted() noexcept
{
  const auto seq = g_frameSeq.fetch_add(1, std::memory_order_relaxed) + 1;
  // Bound the registry. An address freed more than a few frames ago may be
  // handed back by the allocator, and a stale hit on a reused address would
  // only cost a needless reallocation, but there is no reason to keep it.
  std::lock_guard lck{g_retiredMutex};
  for(auto it = g_retired.begin(); it != g_retired.end();)
    it = (it->second.seq + 4 < seq) ? g_retired.erase(it) : std::next(it);
}

bool RenderList::isRetiredBuffer(const QRhiBuffer* buf) noexcept
{
  if(!buf)
    return false;
  std::lock_guard lck{g_retiredMutex};
  auto it = g_retired.find(buf);
  return it != g_retired.end()
         && it->second.seq < g_frameSeq.load(std::memory_order_relaxed);
}

bool RenderList::isRetiringBuffer(const QRhiBuffer* buf) noexcept
{
  if(!buf)
    return false;
  std::lock_guard lck{g_retiredMutex};
  return g_retired.find(buf) != g_retired.end();
}

QByteArray RenderList::retiredBufferName(const QRhiBuffer* buf)
{
  std::lock_guard lck{g_retiredMutex};
  auto it = g_retired.find(buf);
  return it != g_retired.end() ? it->second.name : QByteArray{};
}

int RenderList::retiredBufferCount() noexcept
{
  std::lock_guard lck{g_retiredMutex};
  return (int)g_retired.size();
}

void RenderList::noteBufferLive(const QRhiBuffer* buf) noexcept
{
  if(!buf)
    return;
  std::lock_guard lck{g_retiredMutex};
  if(!g_retired.empty())
    g_retired.erase(buf);
}

void RenderList::adoptBuffer(QRhiBuffer* buf)
{
  if(!buf)
    return;
  std::lock_guard lck{g_retiredMutex};
  auto& entry = g_adopted[buf];
  // A retirement at this address is either an earlier object the allocator
  // recycled -- harmless -- or a handle read from geometry whose producer has
  // already freed it, which no reference can save. Say so, but do not count
  // it as a stale binding: the counter is for bindings, and this one may well
  // be a valid handle at a reused address.
  if(entry.refs == 0)
  {
    if(auto it = g_retired.find(buf); it != g_retired.end()
       && it->second.seq < g_frameSeq.load(std::memory_order_relaxed))
      qWarning(
          "score.gfx: adopting buffer %p at an address retired as \"%s\": "
          "either the allocator recycled it or the producer freed it before "
          "this consumer read the handle",
          (const void*)buf, it->second.name.constData());
    entry.orphaned = false;
  }
  entry.refs++;
}

void RenderList::dropAdoptedBuffer(QRhiBuffer* buf)
{
  if(!buf)
    return;
  bool free = false;
  {
    std::lock_guard lck{g_retiredMutex};
    auto it = g_adopted.find(buf);
    if(it == g_adopted.end())
      return;
    if(--it->second.refs > 0)
      return;
    free = it->second.orphaned;
    g_adopted.erase(it);
    if(free)
      recordRetirement(buf);
  }
  if(free)
    releaseResource(buf);
}

int RenderList::adoptedBufferCount() noexcept
{
  std::lock_guard lck{g_retiredMutex};
  return (int)g_adopted.size();
}

void RenderList::releaseBuffer(QRhiBuffer* buf)
{
  if(!buf)
    return;

  for(auto& vb : m_vertexBuffers)
  {
    for(auto& stored_buffer : vb.second.buffers)
    {
      if(stored_buffer.handle != buf)
        continue;

      // Owned entries are deleted by release().
      if(stored_buffer.owned)
        return;

      // Borrowed entry: the producer is releasing the handle right now, so the
      // pool must stop pointing at it. update_vbo reallocates a null slot.
      stored_buffer.handle = nullptr;
    }
  }

  {
    std::lock_guard lck{g_retiredMutex};
    // Still adopted somewhere: the owner is done with it, the consumer is
    // not. The last drop frees it; until then it stays a live object holding
    // the last contents its owner wrote.
    if(auto it = g_adopted.find(buf); it != g_adopted.end() && it->second.refs > 0)
    {
      it->second.orphaned = true;
      return;
    }
    recordRetirement(buf);
  }
  // Don't call destroy() immediately — the buffer may still be referenced
  // by pending uploadStaticBuffer operations in the current frame's batch.
  // deleteLater() defers destruction to the next beginFrame(), ensuring
  // the GPU handle stays valid for all queued operations this frame.
  releaseResource(buf);
}

namespace
{
std::mutex g_parkedMutex;
std::vector<std::pair<QRhi*, QRhiResource*>> g_parked;

std::vector<QRhiResource*> takeParked(const QRhi* rhi)
{
  std::vector<QRhiResource*> out;
  std::lock_guard lck{g_parkedMutex};
  for(auto it = g_parked.begin(); it != g_parked.end();)
  {
    if(it->first == rhi)
    {
      out.push_back(it->second);
      it = g_parked.erase(it);
    }
    else
      ++it;
  }
  return out;
}
}

void RenderList::releaseResource(QRhiResource* res)
{
  if(!res)
    return;
  QRhi* rhi = res->rhi();
  if(!rhi || rhi->isRecordingFrame())
  {
    res->deleteLater();
    return;
  }
  {
    std::lock_guard lck{g_parkedMutex};
    g_parked.emplace_back(rhi, res);
  }
  rhi->addCleanupCallback(&g_parked, [](QRhi* rhi) {
    for(auto* r : takeParked(rhi))
      delete r;
  });
}

void RenderList::retireResourcesReleasedOutsideFrame() noexcept
{
  if(!state.rhi)
    return;
  for(auto* r : takeParked(state.rhi))
    r->deleteLater();
}

static std::atomic_int g_staleBindings{0};

int RenderList::staleBindingCount() noexcept
{
  return g_staleBindings.load(std::memory_order_relaxed);
}
void RenderList::resetStaleBindingCount() noexcept
{
  g_staleBindings.store(0, std::memory_order_relaxed);
}

bool RenderList::strictBindingsEnabled() noexcept
{
  static const bool on = qEnvironmentVariableIsSet("SCORE_GFX_STRICT_BINDINGS");
  return on;
}

bool RenderList::hasRetiredBinding(const QRhiShaderResourceBindings& srb) noexcept
{
  for(auto it = srb.cbeginBindings(); it != srb.cendBindings(); ++it)
  {
    const auto& d
        = *reinterpret_cast<const QRhiShaderResourceBinding::Data*>(&(*it));
    const QRhiBuffer* b = nullptr;
    switch(d.type)
    {
      case QRhiShaderResourceBinding::UniformBuffer:
        b = d.u.ubuf.buf;
        break;
      case QRhiShaderResourceBinding::BufferLoad:
      case QRhiShaderResourceBinding::BufferStore:
      case QRhiShaderResourceBinding::BufferLoadStore:
        b = d.u.sbuf.buf;
        break;
      default:
        continue;
    }
    if(isRetiringBuffer(b))
      return true;
  }
  return false;
}

bool RenderList::checkBindingsLive(
    const QRhiShaderResourceBindings& srb, const char* where) const noexcept
{
  bool ok = true;
  for(auto it = srb.cbeginBindings(); it != srb.cendBindings(); ++it)
  {
    // Same access pattern as replaceBuffer() in Utils.cpp: QRhi exposes no
    // public reader for a binding's payload.
    const auto& d
        = *reinterpret_cast<const QRhiShaderResourceBinding::Data*>(&(*it));
    const QRhiBuffer* b = nullptr;
    switch(d.type)
    {
      case QRhiShaderResourceBinding::UniformBuffer:
        b = d.u.ubuf.buf;
        break;
      case QRhiShaderResourceBinding::BufferLoad:
      case QRhiShaderResourceBinding::BufferStore:
      case QRhiShaderResourceBinding::BufferLoadStore:
        b = d.u.sbuf.buf;
        break;
      default:
        continue;
    }
    if(isRetiredBuffer(b))
    {
      ok = false;
      g_staleBindings.fetch_add(1, std::memory_order_relaxed);
      qWarning(
          "score.gfx: %s binds buffer %p \"%s\" at binding %d, which was "
          "retired: the producer released it and this consumer never re-read "
          "the handle",
          where ? where : "(unknown)", (const void*)b,
          retiredBufferName(b).constData(), d.binding);
    }
  }
  return ok;
}

bool RenderList::maybeRebuild(bool force)
{
  bool rebuilt = false;
  const QSize outputSize = state.renderSize;
  if(outputSize != m_lastSize || !m_built || force)
  {
    // Drain the in-flight command buffer before the mid-frame release()+init().
    //
    // maybeRebuild runs from renderInternal, inside Window::render's
    // beginFrame/endFrame brackets. release() deletes SRBs, samplers and UBOs that
    // the resource-update batch already queued into cbD->commands may reference, as
    // may ScenePreprocessor's beginExternal/copyBuffer/endExternal block, which
    // flushes cbD->commands into the VkCommandBuffer synchronously.
    //
    // finish() mid-frame is documented and supported: it submits the partial CB,
    // waits on the queue, then restarts a fresh CB on the same slot, leaving the
    // queue empty and the resources safe to tear down.
    //
    // Triggers only on the first frame after a resize or forced rebuild.
    //
    // Unconditional, not gated on isRecordingFrame(): a surface resize goes
    // exposeEvent() -> resizeSwapChain() -> onResize() and rebuilds from
    // OUTSIDE a frame, where skipping the drain releases nodes while already
    // submitted frames are still executing, so a descriptor set outlives the
    // texture it points at -- an MMU fault on a GPU read from an unmapped
    // address in a fragment shader.
    //
    // QRhi::finish() is documented as callable "inside and outside of a frame,
    // but not inside a pass", and outside one it both waits on the queue and
    // "executes all deferred operations, like ... resource releases" -- which
    // is precisely what has to happen before release() runs.
    if(state.rhi)
      state.rhi->finish();

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

  if(edge.source && edge.source->node == edge.sink->node)
    if(auto it = m_selfFeedbackTargets.find(edge.sink); it != m_selfFeedbackTargets.end())
      if(it->second.back.renderTarget && it->second.back.renderPass)
        return it->second.back;

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

QRhiTexture::Format imageTextureFormat(const QRhi& rhi) noexcept
{
  return rhi.isTextureFormatSupported(QRhiTexture::BGRA8) ? QRhiTexture::BGRA8
                                                          : QRhiTexture::RGBA8;
}

QImage adaptImageFormat(QImage img, QRhiTexture::Format fmt)
{
  const bool premultiplied = img.format() == QImage::Format_ARGB32_Premultiplied
                             || img.format() == QImage::Format_RGBA8888_Premultiplied;

  QImage::Format wanted{};
  if(fmt == QRhiTexture::BGRA8)
    wanted = premultiplied ? QImage::Format_ARGB32_Premultiplied : QImage::Format_ARGB32;
  else
    wanted = premultiplied ? QImage::Format_RGBA8888_Premultiplied
                           : QImage::Format_RGBA8888;

  if(img.format() != wanted)
    img.convertTo(wanted);
  return img;
}

QImage RenderList::adaptImage(const QImage& frame)
{
  auto res = resizeTexture(frame, m_minTexSize, m_maxTexSize);
  return adaptImageFormat(std::move(res), imageTextureFormat(*state.rhi));
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
  // Well-known _indirect_draw / _indirect_draw_indexed auxiliary buffer
  // convention.
  //
  // Both auxiliaries carry 5-word records at a uniform stride of 20, but the
  // word ORDER is the GPU ABI and differs between them, because
  // QRhiDrawIndirectCommand is 4 u32 and QRhiDrawIndexedIndirectCommand is 5:
  //
  //   _indirect_draw_indexed  { indexCount, instanceCount, firstIndex,
  //                             baseVertex, firstInstance }
  //                           == QRhiDrawIndexedIndirectCommand
  //
  //   _indirect_draw          { vertexCount, instanceCount, firstVertex,
  //                             firstInstance, baseVertex (unused) }
  //                           words 0..3 == QRhiDrawIndirectCommand
  //
  // The producer picks the shape (libisf emits the matching GLSL struct for a
  // geometry resource's INDIRECT block; ScenePreprocessor writes it directly),
  // so both are GPU-safe here. Feeding the indexed order to drawIndirect()
  // makes the GPU read firstInstance out of word 3 while the CPU readback path
  // reads word 4.
  if(!meshbufs.useIndirectDraw && !p->meshes.empty())
  {
    const auto& mesh = p->meshes[0];
    if(auto* aux_idx = mesh.find_auxiliary("_indirect_draw_indexed"))
    {
      if(aux_idx->buffer >= 0 && aux_idx->buffer < (int)mesh.buffers.size())
      {
        const auto& buf_data = mesh.buffers[aux_idx->buffer].data;
        if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&buf_data))
        {
          if(gpu->handle)
          {
            // Derive the command count from the aux region size; without it
            // the count defaults to 1 and only the first command is drawn.
            // enableIndirectDraw() supplies the stride, which must never be
            // omitted -- see its comment.
            const int64_t off = std::max<int64_t>(0, aux_idx->byte_offset);
            const int64_t avail = (aux_idx->byte_size > 0)
                ? aux_idx->byte_size
                : (int64_t)gpu->byte_size - off;
            meshbufs.enableIndirectDraw(
                static_cast<QRhiBuffer*>(gpu->handle), true, avail,
                (quint32)off);
          }
        }
      }
    }
    else if(auto* aux_nonidx = mesh.find_auxiliary("_indirect_draw"))
    {
      // Non-indexed: words 0..3 of each record are a native
      // QRhiDrawIndirectCommand, so drawIndirect() at stride 20 reads
      // firstInstance from the right word.
      if(aux_nonidx->buffer >= 0 && aux_nonidx->buffer < (int)mesh.buffers.size())
      {
        const auto& buf_data = mesh.buffers[aux_nonidx->buffer].data;
        if(auto* gpu = ossia::get_if<ossia::geometry::gpu_buffer>(&buf_data))
        {
          if(gpu->handle)
          {
            const int64_t off = std::max<int64_t>(0, aux_nonidx->byte_offset);
            const int64_t avail = (aux_nonidx->byte_size > 0)
                ? aux_nonidx->byte_size
                : (int64_t)gpu->byte_size - off;
            meshbufs.enableIndirectDraw(
                static_cast<QRhiBuffer*>(gpu->handle), false, avail,
                (quint32)off);
            if(meshbufs.indirectDrawCount == 0)
              meshbufs.indirectDrawCount = 1;
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

bool RenderList::resizeSwapchainSizedTargets(QSize newOutputSize, QSize newRenderSize)
{
  // Bail to fallback if there's nothing to resize. The fallback
  // (recreateOutputRenderList) handles initial output setup.
  if(newOutputSize.width() <= 0 || newOutputSize.height() <= 0)
    return false;
  if(newRenderSize.width() <= 0 || newRenderSize.height() <= 0)
    return false;
  if(renderers.empty())
    return false;

  // Already at the right size — no-op success. Avoids a wasted
  // round-trip through maybeRebuild when Qt fires multiple onResize
  // callbacks for the same final size. m_lastSize tracks state.renderSize
  // (see markBuilt / maybeRebuild), so it is the render size that has to
  // match it, not the swapchain size.
  if(newRenderSize == m_lastSize && newOutputSize == state.outputSize)
    return true;

  // m_lastSize deliberately keeps its OLD value so maybeRebuild's
  // `outputSize != m_lastSize` check fires on the next render frame.
  state.renderSize = newRenderSize;
  state.outputSize = newOutputSize;
  m_built = false;  // forces maybeRebuild's release+init on next frame

  return true;
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

static void update_date_for_shaders(float (&date)[4]) noexcept {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto sys_days_tp = floor<days>(now);
  const year_month_day ymd{sys_days_tp};
  const auto time_of_day = duration_cast<seconds>(now - sys_days_tp);

  date[0] = (int32_t)ymd.year();
  date[1] = (uint32_t)ymd.month();
  date[2] = (uint32_t)ymd.day();
  date[3] = (uint32_t)time_of_day.count();
}

void RenderList::render(QRhiCommandBuffer& commands, bool force) noexcept
{
  try
  {
    renderImpl(commands, force);
  }
  catch(const std::exception& e)
  {
    if((m_renderFailures++ % 600) == 0)
      qWarning() << "RenderList::render: aborted this frame:" << e.what()
                 << "(occurrence" << m_renderFailures << ")";
  }
  catch(...)
  {
    if((m_renderFailures++ % 600) == 0)
      qWarning() << "RenderList::render: aborted this frame: unknown exception"
                 << "(occurrence" << m_renderFailures << ")";
  }

  // Outside the try: every once-per-frame gate in the graph keys on this
  // counter, so a frame that aborts must still advance it or those nodes
  // never run again.
  frame++;
  noteFrameCompleted();
}

void RenderList::renderImpl(QRhiCommandBuffer& commands, bool force)
{
  update_date_for_shaders(this->currentDate);

  if(renderers.size() <= 1 && !force)
    return;

  // Cleared on every exit path so currentCommandBuffer() can never hand a
  // stale pointer to a strategy recording outside a frame.
  struct CommandScope
  {
    QRhiCommandBuffer*& slot;
    ~CommandScope() { slot = nullptr; }
  } commandScope{m_currentCommands};
  m_currentCommands = &commands;

  // Frame counter and wall-clock timer: the frame header carries the time since
  // the previous render() entry, covering CPU record plus any synchronous GPU
  // waits inside setShaderResources / beginPass.
  //
  // The GPU time is read CB-wide and attributed to the "frame" label; QRhi only
  // exposes CB-scoped timings, and lastCompletedGpuTime() returns the PREVIOUS
  // frame's elapsed time, which the panel reports as such.
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
  // Diagnostic frame number: the per-instance `frame` member (incremented at
  // the end of render()), so it is attributed to THIS RenderList rather than
  // to a process- or thread-global counter.
  const int64_t frameNumber = this->frame;
  static const bool no_ts = qEnvironmentVariableIsSet("SCORE_NO_GPU_TIMESTAMPS");
  if(state.caps.timestamps && !no_ts)
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
    static thread_local int s_flushCoolDown = 0;
    const auto stats = state.rhi->statistics();
    const qint64 delta_ns = stats.totalPipelineCreationTime - s_lastPsoCreationNs;
    s_lastPsoCreationNs = stats.totalPipelineCreationTime;
    const double delta_ms = double(delta_ns) / 1'000'000.0;

    // Tick the cooldown every frame, not only inside the stall branch: it must
    // count frames rather than stalls for the ~5s throttle to elapse in wall
    // time.
    if(s_flushCoolDown > 0)
      --s_flushCoolDown;

    if(delta_ms > 10.0)
    {
      qWarning().noquote().nospace()
          << "[GPU] PSO compile stall on frame " << frameNumber
          << ": " << delta_ms << " ms — consider prewarming preset pipelines.";

      // Mid-session pipeline-cache flush: a stall means fresh PSOs were just
      // compiled, so persist the cache and spare the next launch the same
      // compilation even if score crashes. Throttled to once per ~5s (300
      // frames at 60 Hz) so compile-heavy scenes do not churn the cache file.
      if(s_flushCoolDown <= 0 && state.savePipelineCache)
      {
        state.savePipelineCache();
        s_flushCoolDown = 300;
      }
    }
    // Also record into the timings panel so it shows up next to frame
    // time. Zero deltas are filtered out by GpuTimings::record.
    m_gpuTimings.record("pso_compile", delta_ms);
  }
#endif
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
  if(!updateBatch)
  {
    qWarning("RenderList::render: resource update batch pool exhausted");
    return;
  }
  retireResourcesReleasedOutsideFrame();

  // Only on unwinding: the success path hands the batch to endPass() or to
  // finishFrame() and nulls it, so releasing unconditionally here would
  // double-release. The pool holds 64 slots and render() swallows, so a
  // throw that leaked one per frame would black the renderer out in a second.
  struct ReleaseBatchOnThrow
  {
    QRhiResourceUpdateBatch*& batch;
    int depth = std::uncaught_exceptions();
    ~ReleaseBatchOnThrow()
    {
      if(batch && std::uncaught_exceptions() > depth)
      {
        batch->release();
        batch = nullptr;
      }
    }
  } releaseBatchOnThrow{updateBatch};

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

    // Pass 2: intermediate nodes whose input render targets no longer match
    // their resolved specs (their own settings, or a downstream size change)
    for(auto* renderer : renderers)
    {
      // Skip output node (handled above)
      if(&renderer->node == &output)
        continue;
      const bool ownSpecsChanged = renderer->renderTargetSpecsChanged;

      // Phase A: scan ports, recreate input RTs whose specs changed,
      // and collect the changed-port set so phase C only re-adds
      // upstream passes for those.
      QVarLengthArray<Port*, 4> changedPorts;
      int cur_port = 0;
      for(auto* in : renderer->node.input)
      {
        if(in->type == Types::Image
           && (in->flags & Flag::GrabsFromSource) != Flag::GrabsFromSource)
        {
          auto newSpec = resolveInputRenderTargetSpecs(renderer->node, cur_port);
          auto oldIt = m_inputRenderTargets.find(in);

          const auto newFlags = inputRenderTargetFlags(newSpec);
          bool specChanged = false;
          if(oldIt != m_inputRenderTargets.end())
          {
            auto* oldTex = oldIt->second.texture;
            if(oldTex)
              specChanged = (oldTex->format() != newSpec.format)
                         || (oldTex->pixelSize() != newSpec.size)
                         || ((oldTex->flags()
                              & (QRhiTexture::MipMapped
                                 | QRhiTexture::UsedWithGenerateMips))
                             != newFlags);
          }

          // Always update sampler filter settings when specs changed
          // (filter/address changes don't require RT recreation)
          if(ownSpecsChanged)
            renderer->updateInputSamplerFilter(*in, newSpec);

          if(specChanged)
          {
            changedPorts.append(in);

            // Remove upstream passes that target this port
            for(auto* edge : in->edges)
            {
              auto src_it = edge->source->node->renderedNodes.find(this);
              if(src_it != edge->source->node->renderedNodes.end())
                src_it->second->removeOutputPass(*this, *edge);
            }

            // Recreate the render target
            removeSelfFeedbackTarget(in);
            oldIt->second.release();
            bool wantsDepth = requiresDepth(*in);
            bool wantsSamplableDepth
                = (in->flags & Flag::SamplableDepth) == Flag::SamplableDepth;
            oldIt->second = score::gfx::createRenderTarget(
                state, newSpec.format, newSpec.size, samples(),
                wantsDepth || wantsSamplableDepth, wantsSamplableDepth, newFlags);
            ensureSelfFeedbackTarget(*in);
          }
        }
        cur_port++;
      }

      // Phase B: if ANY input RT actually changed shape, the renderer's
      // INTERNAL size-dependent state (intermediate RTs, MRT,
      // persistent AUX, depth/MSAA attachments sized to output, etc.)
      // is stale and needs re-init: without it the input RT is recreated
      // at the new size while the renderer's own internal RTs stay at the
      // old one. initState wires up samplers against the current
      // m_inputRenderTargets, so no separate updateInputTexture pass is
      // needed.
      //
      // Phase C: re-add upstream passes ONLY for the ports whose RT
      // was recreated (others kept their existing passes intact in
      // phase A). Done after Phase B so the upstream's addOutputPass
      // sees this renderer's freshly-built per-pass state.
      if(!changedPorts.empty())
      {
        renderer->releaseState(*this);
        renderer->initState(*this, *updateBatch);
        renderer->checkForChanges();
        renderer->materialChanged = true;
        renderer->geometryChanged = true;

        // releaseState() cleared this renderer's OWN output passes (the
        // per-edge m_p / m_passes list) together with its input-dependent
        // state, but initState() deliberately does NOT recreate them — only
        // init() does, via addOutputPass. Rebuild them here exactly as
        // init() does, otherwise this intermediate node silently stops
        // producing into its (unchanged) downstream sinks after a runtime
        // render-target-spec change. Phase C below only re-adds the UPSTREAM
        // producers' passes that feed this node's changed input ports, which
        // is a disjoint set from this node's own output passes.
        for(auto* out : renderer->node.output)
        {
          if(out->type != Types::Image)
            continue;
          for(auto* edge : out->edges)
            renderer->addOutputPass(*this, *edge, *updateBatch);
        }

        for(auto* in : changedPorts)
        {
          for(auto* edge : in->edges)
          {
            auto src_it = edge->source->node->renderedNodes.find(this);
            if(src_it != edge->source->node->renderedNodes.end())
              src_it->second->addOutputPass(*this, *edge, *updateBatch);
          }
        }
      }

      renderer->renderTargetSpecsChanged = false;
    }
  }
  // Check if the viewport has changed

  update(*updateBatch);

  updateSelfFeedbackTargets(*updateBatch);

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

    if(!updateBatch)
    {
      updateBatch = state.rhi->nextResourceUpdateBatch();
      if(!updateBatch)
      {
        qWarning("RenderList: resource update batch pool exhausted");
        return;
      }
    }

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
      if(!updateBatch)
      {
        qWarning("RenderList: resource update batch pool exhausted");
        return;
      }
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
        if(!updateBatch)
        {
          qWarning("RenderList: resource update batch pool exhausted");
          return;
        }

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
      {
        // An image input with no incoming edge must still have its render
        // target cleared each frame; otherwise removing a cable while both
        // endpoints stay alive leaves the last frame stuck in the consumer
        // (e.g. a mixer keeps compositing a source that was disconnected).
        if(input->type == Types::Image
           && (input->flags & Flag::GrabsFromSource) != Flag::GrabsFromSource)
        {
          auto rendered = node->renderedNodes.find(this);
          SCORE_ASSERT(rendered != node->renderedNodes.end());
          NodeRenderer* renderer = rendered->second;

          auto rt = renderer->renderTargetForInput(*input);
          if(!rt)
            rt = renderTargetForInputPort(*input);
          if(rt)
          {
            QColor bg = (it + 1 == this->nodes.rend() ? Qt::black : Qt::transparent);
            commands.beginPass(
                rt.renderTarget, bg,
                {depthClearForCompare(QRhiGraphicsPipeline::Greater), 0},
                updateBatch);
            updateBatch = nullptr;
            generateInputMips(*state.rhi, rt, updateBatch);
            commands.endPass(updateBatch);
            updateBatch = nullptr;

            if(node != &output)
            {
              updateBatch = state.rhi->nextResourceUpdateBatch();
              if(!updateBatch)
              {
                qWarning("RenderList: resource update batch pool exhausted");
                return;
              }
            }
          }
        }
        continue;
      }

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
            auto* srcTex = edge->source->node == node ? selfFeedbackGrab(*input)
                                                      : nullptr;
            if(!srcTex)
              srcTex = prev_renderer->textureForOutput(*edge->source);
            if(srcTex)
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
              if(!updateBatch)
              {
                qWarning("RenderList::render: resource update batch pool "
                         "exhausted");
                return;
              }
              continue;
            }
            NodeRenderer* renderer = rendered->second;

            auto rt = renderer->renderTargetForInput(*input);
            if(!rt)
            {
              if(auto fb = m_selfFeedbackTargets.find(input);
                 fb != m_selfFeedbackTargets.end())
              {
                rt = fb->second.back;
                fb->second.written = true;
              }
              else
              {
                rt = renderTargetForInputPort(*input);
              }
            }
            if(rt)
            {
              QColor bg = (it + 1 == this->nodes.rend() ? Qt::black : Qt::transparent);
              auto passCompare = QRhiGraphicsPipeline::Greater;
              bool compareSeen = false, compareMixed = false;
              for(auto [edge, prev_renderer] : prevRenderers)
              {
                const auto c = prev_renderer->depthCompare();
                if(!compareSeen)
                {
                  passCompare = c;
                  compareSeen = true;
                }
                else if(c != passCompare)
                {
                  compareMixed = true;
                }
              }
              if(compareMixed)
              {
                if(!m_warnedMixedDepthCompare)
                {
                  m_warnedMixedDepthCompare = true;
                  qWarning() << "RenderList: nodes drawing into one pass declare "
                                "different DEPTH_COMPARE; clearing depth for the "
                                "reverse-Z default";
                }
                passCompare = QRhiGraphicsPipeline::Greater;
              }
              commands.beginPass(
                  rt.renderTarget, bg, {depthClearForCompare(passCompare), 0},
                  updateBatch);
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
              generateInputMips(*state.rhi, rt, updateBatch);
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
          if(!updateBatch)
          {
            qWarning("RenderList: resource update batch pool exhausted");
            return;
          }
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
          if(!updateBatch)
          {
            qWarning("RenderList: resource update batch pool exhausted");
            return;
          }
        }
      }
    }
  }

  // Finally the output node may have some rendering to do too
  {
    if(this->output.renderedNodes.empty())
    {
      // updateBatch must be returned before bailing out — otherwise the
      // pool slot stays pinned until the QRhi is destroyed.
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
        if(!updateBatch)
        {
          qWarning("RenderList: resource update batch pool exhausted");
          return;
        }
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

//! Backend + device identity, printed for EVERY backend.
//!
//! Qt's own qt.rhi.general output names the device on exactly one backend: the
//! OpenGL one, in qrhigles2.cpp's "OpenGL VENDOR: %s RENDERER: %s VERSION: %s".
//! Vulkan prints "Using imported physical device '<name>' ... vendor 0x.. device
//! 0x.. type N", D3D11 and D3D12 print adapter lines of their own shape, and
//! none of them contains the word RENDERER, so anything that identifies the GPU
//! by reading Qt's log gets an empty string off every backend but GL.
//!
//! QRhi::driverInfo() is the portable answer: deviceName, vendorId, deviceId and
//! deviceType are filled in by all of them (Qt >= 6.4). One line, one format,
//! every backend, so a frame can always be attributed to what produced it.
static void logDeviceIdentity(QRhi& rhi)
{
  const auto info = rhi.driverInfo();
  const char* type = "unknown";
  switch(info.deviceType)
  {
    case QRhiDriverInfo::UnknownDevice:
      type = "unknown";
      break;
    case QRhiDriverInfo::IntegratedDevice:
      type = "integrated";
      break;
    case QRhiDriverInfo::DiscreteDevice:
      type = "discrete";
      break;
    case QRhiDriverInfo::ExternalDevice:
      type = "external";
      break;
    case QRhiDriverInfo::VirtualDevice:
      type = "virtual";
      break;
    case QRhiDriverInfo::CpuDevice:
      type = "cpu";
      break;
  }

  qDebug().noquote().nospace()
      << "score.gfx: RHI device: backend=" << rhi.backendName() << " device=\""
      << QString::fromUtf8(info.deviceName) << "\" vendorId=0x"
      << QString::number(info.vendorId, 16) << " deviceId=0x"
      << QString::number(info.deviceId, 16) << " deviceType=" << type;
}

void RenderState::Caps::populate(QRhi& rhi)
{
  logDeviceIdentity(rhi);

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
  drawIndirect = rhi.isFeatureSupported(QRhi::DrawIndirect);
  drawIndirectMulti = rhi.isFeatureSupported(QRhi::DrawIndirectMulti);
  // A GPU indirect draw reads its {indexCount, instanceCount, firstIndex,
  // baseVertex, firstInstance} words out of a buffer the CPU never inspects, so
  // a stale QRhiBuffer* recorded into drawIndexedIndirect() is not a wrong
  // picture: it is an out-of-bounds index fetch and a lost device. The CPU
  // fallback path in CustomMesh::draw() issues the same draws from
  // cpu_draw_commands, where the counts are visible and bounded.
  //
  // SCORE_GFX_NO_GPU_INDIRECT=1 forces that fallback on a backend that does
  // support DrawIndirect. It is how a VK_ERROR_DEVICE_LOST gets attributed:
  // if the loss survives the switch the indirect buffer was not the source.
  if(qEnvironmentVariableIntValue("SCORE_GFX_NO_GPU_INDIRECT") > 0)
  {
    drawIndirect = false;
    drawIndirectMulti = false;
  }
  // Finer-grained rung switches, same idea and same testing story as
  // SCORE_GFX_NO_GPU_INDIRECT above: each one forces the next rung of the
  // fallback ladder (see the Caps declaration) on hardware that supports the
  // better one, so the fallbacks stay exercisable instead of rotting until a
  // distro build hits them. NO_GPU_INDIRECT_MULTI declines the single-call
  // multi-draw, making the draw loop issue one drawCount=1 indirect draw per
  // command.
  if(qEnvironmentVariableIntValue("SCORE_GFX_NO_GPU_INDIRECT_MULTI") > 0)
    drawIndirectMulti = false;
#endif

  // 6.13-era API, detected rather than version-gated — the reference SDK
  // builds are Qt 6.12 + cherry-picked 6.13 RHI changes, so QT_VERSION lies
  // there (see RhiIndirectCompat.hpp). On Qt 6.4 and stock 6.12 these
  // evaluate to false at compile time.
  drawIndirectCount = score::gfx::rhiSupportsDrawIndirectCount(rhi);
  dispatchIndirect = score::gfx::rhiSupportsDispatchIndirect(rhi);
  if(qEnvironmentVariableIntValue("SCORE_GFX_NO_GPU_INDIRECT") > 0
     || qEnvironmentVariableIntValue("SCORE_GFX_NO_GPU_INDIRECT_COUNT") > 0)
    drawIndirectCount = false;
  if(qEnvironmentVariableIntValue("SCORE_GFX_NO_GPU_DISPATCH_INDIRECT") > 0)
    dispatchIndirect = false;
#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0)
  instanceIndexIncludesBaseInstance
      = rhi.isFeatureSupported(QRhi::InstanceIndexIncludesBaseInstance);
  depthClamp = rhi.isFeatureSupported(QRhi::DepthClamp);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  variableRateShading = rhi.isFeatureSupported(QRhi::VariableRateShading);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
  textureViewFormat = rhi.isFeatureSupported(QRhi::TextureViewFormat);
  resolveDepthStencil = rhi.isFeatureSupported(QRhi::ResolveDepthStencil);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
  multiview = rhi.isFeatureSupported(QRhi::MultiView)
              && !qEnvironmentVariableIsSet("SCORE_GFX_DISABLE_MULTIVIEW");
#endif

  timestamps = rhi.isFeatureSupported(QRhi::Timestamps);
  tessellation = rhi.isFeatureSupported(QRhi::Tessellation);
  geometryShader = rhi.isFeatureSupported(QRhi::GeometryShader);
  baseInstance = rhi.isFeatureSupported(QRhi::BaseInstance);
  pipelineCacheDataLoadSave = rhi.isFeatureSupported(QRhi::PipelineCacheDataLoadSave);

  // One greppable line with the final (post-kill-switch) indirect rung caps.
  // GfxIndirectFallbackLadder.cpp captures this through a message handler as
  // its positive control that each SCORE_GFX_NO_GPU_* switch actually
  // propagated into the caps a session renders with — without it a broken
  // switch would leave the better rung active and the fallback untested,
  // with identical (correct) pixels hiding the failure.
  qDebug("score.gfx: RHI indirect caps: drawIndirect=%d multi=%d count=%d "
         "dispatchIndirect=%d baseInstance=%d instIdxInclBase=%d",
         int(drawIndirect), int(drawIndirectMulti), int(drawIndirectCount),
         int(dispatchIndirect), int(baseInstance),
         int(instanceIndexIncludesBaseInstance));
}
}
