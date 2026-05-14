#include "ISFNode.hpp"

#include <Gfx/Graph/CustomMesh.hpp>
#include <Gfx/Graph/Graph.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/OutputNode.hpp>
#include <Gfx/Graph/RenderList.hpp>
#include <Gfx/Graph/Window.hpp>

#include <score/gfx/Vulkan.hpp>
#include <score/tools/Debug.hpp>

#include <ossia/detail/flat_set.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/ssize.hpp>

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/filtered_graph.hpp>
#include <boost/graph/topological_sort.hpp>

namespace score::gfx
{
template <typename Graph_T, typename IO>
void print_graph(Graph_T& g, IO& stream)
{
#if 0
  std::stringstream s;
  boost::write_graphviz(
      s, g,
      [&](auto& out, auto v) {
    if(g[v])
    {

      out << "[label=\"";
      auto n = g[v];
      if(auto i = dynamic_cast<ISFNode*>(n))
        out << i->m_descriptor.description;
      else
        out << "output";
      out << "\"]";
    }
    else
      out << "[]";
      },
      [](auto&&...) {});

  stream << s.str() << "\n";
#endif
}

using Vertex = score::gfx::Node*;
using GraphImpl = boost::adjacency_list<
    boost::vecS, boost::vecS, boost::directedS, Vertex, Process::CableType>;
using VertexMap = ossia::hash_map<score::gfx::Node*, GraphImpl::vertex_descriptor>;

struct no_delay_edges
{
  const GraphImpl* g{};

  bool operator()(const boost::graph_traits<GraphImpl>::edge_descriptor& e) const
  {
    switch((*g)[e])
    {
      case Process::CableType::ImmediateGlutton:
      case Process::CableType::ImmediateStrict:
        return true;
      default:
        return false;
    }
  }
};

static void graphwalk(
    score::gfx::Node* node, std::vector<score::gfx::Node*>& list, GraphImpl& g,
    VertexMap& m, ossia::flat_set<score::gfx::Node*>& visited)
{
  auto sink_desc = m[node];
  for(auto inputs : node->input)
  {
    for(auto edge : inputs->edges)
    {
      auto* src_node = edge->source->node;
      if(visited.insert(src_node).second)
      {
        list.push_back(src_node);

        auto src_desc = boost::add_vertex(src_node, g);
        m[src_node] = src_desc;
        boost::add_edge(src_desc, sink_desc, edge->type, g);
      }
      else
      {
        auto src_desc = m[src_node];
        boost::add_edge(src_desc, sink_desc, edge->type, g);
      }
    }
  }
}

static void graphwalk(std::vector<score::gfx::Node*>& model_nodes)
{
  GraphImpl g;
  VertexMap m;
  ossia::flat_set<score::gfx::Node*> visited;

  auto k = boost::add_vertex(model_nodes.front(), g);
  m[model_nodes.front()] = k;
  visited.insert(model_nodes.front());

  std::size_t processed = 0;
  while(processed != model_nodes.size())
  {
    graphwalk(model_nodes[processed], model_nodes, g, m, visited);
    processed++;
  }

  ossia::int_vector topo_order;
  topo_order.reserve(model_nodes.size());

  try
  {
    model_nodes.clear();
    auto view = boost::filtered_graph(g, no_delay_edges{&g});
    boost::topological_sort(view, std::back_inserter(topo_order));
    for(auto it = topo_order.begin(); it != topo_order.end(); ++it)
    {
      auto e = *it;
      SCORE_ASSERT(g[e]);
      model_nodes.push_back(g[e]);
    }
  }
  catch(const std::exception& e)
  {
    qDebug() << "Invalid gfx graph: " << e.what();
  }
}

void Graph::createAllRenderLists(GraphicsApi graphicsApi)
{
#if QT_HAS_VULKAN
  if(graphicsApi == Vulkan)
  {
    if(!staticVulkanInstance())
    {
      qWarning("Failed to create Vulkan instance, switching to OpenGL");
      graphicsApi = OpenGL;
    }
  }
#endif

  for(auto output : m_outputs)
  {
    output->stopRendering();
  }

  for(auto node : m_nodes)
  {
    node->renderedNodes.clear();
    node->renderedNodesChanged();
  }

  for(auto& renderer : m_renderers)
  {
    renderer->release();
  }

  m_renderers.clear();
  m_outputs.clear();

  ossia::flat_set<OutputNode*> parent_nodes;
  for(auto node : m_nodes)
  {
    if(auto out = dynamic_cast<OutputNode*>(node))
    {
      m_outputs.push_back(out);
      if(auto ptr = out->configuration().parent)
        parent_nodes.insert(ptr);
    }
  }

  // For multi-viewport renders
  for(auto node : parent_nodes)
  {
    if(!ossia::contains(m_outputs, node))
    {
      m_outputs.push_back(node);
    }
  }

  m_renderers.reserve(ossia::max(16, std::ssize(m_outputs)));

  for(auto output : m_outputs)
  {
    initializeOutput(output, graphicsApi);
    output->startRendering();
  }
}

void Graph::createSingleRenderList(
    score::gfx::OutputNode& output, GraphicsApi graphicsApi)
{
#if QT_HAS_VULKAN
  if(graphicsApi == Vulkan)
  {
    if(!staticVulkanInstance())
    {
      qWarning("Failed to create Vulkan instance, switching to OpenGL");
      graphicsApi = OpenGL;
    }
  }
#endif

  initializeOutput(&output, graphicsApi);
  output.startRendering();
}

void Graph::createOutputRenderList(OutputNode& output)
try
{
  if(output.renderState())
  {
    if(auto rl = createRenderList(&output, output.renderState()))
      m_renderers.push_back(std::move(rl));
  }
}
catch(...)
{
}

void Graph::recreateOutputRenderList(OutputNode& output)
{
  auto it = ossia::find_if(
      m_renderers, [rend = output.renderer()](const std::shared_ptr<RenderList>& r) {
        return r.get() == rend;
      });

  if(it != m_renderers.end())
  {
    std::shared_ptr<RenderList>& renderer = *it;
    if(renderer.get() == output.renderer())
    {
      // Pre-condition: recreateOutputRenderList MUST be called outside
      // any active beginFrame/endFrame block. The Window::resize ->
      // resizeSwapChain -> onResize -> here chain is invoked at the
      // top of Window::render BEFORE beginFrame (Window.cpp:148-151),
      // so this should always hold. Assert it to catch any future
      // path that triggers the resize from inside a render frame.
      if(auto rs = output.renderState(); rs && rs->rhi)
        SCORE_ASSERT(!rs->rhi->isRecordingFrame());

      // Drain the GPU before tearing down the old RenderList. release()
      // walks every renderer and triggers a torrent of delete /
      // deleteLater on QRhi objects (textures, samplers, buffers,
      // SRBs, pipelines). On Vulkan, sibling outputs (BackgroundNode's
      // beginOffscreenFrame, MultiWindowNode per-window CBs, the
      // resizing window's own previous-frame CB) may still hold those
      // resources in pending state. Without this drain, the next time
      // ScenePreprocessor's runInitialPasses records vkCmdCopyBuffer /
      // vkCmdPipelineBarrier into a CB the rhi believes is fresh,
      // validation fires (-recording / -in-use), eventual device loss.
      //
      // FIX-A added rhi->finish() inside ScreenNode::destroyOutput and
      // BackgroundNode::destroyOutput, but the
      // `Window::resize → onResize → recreateOutputRenderList` path
      // never enters those — it tears down the RenderList directly.
      if(auto rs = output.renderState(); rs && rs->rhi)
      {
        auto* rhi = rs->rhi;
        rhi->finish();

        // Force a no-op offscreen frame on each frame slot so BOTH
        // cmdPools are reset symmetrically. QRhi-Vulkan's finish()
        // resets only `cmdPool[currentFrameSlot]`
        // (qrhivulkan.cpp:2617-2629); the OTHER slot's pool stays
        // untouched. If a sibling output (BackgroundNode /
        // PreviewNode / MultiWindowNode) drives its own
        // beginOffscreenFrame on a separate timer, its
        // ensureCommandPoolForNewFrame on the un-reset slot finds
        // CBs still in pending state from the pre-resize era →
        // vkResetCommandPool VUID-00040, then vkBeginCommandBuffer
        // on active CB, eventual device loss in vkQueueSubmit.
        // The cascade fires ~16 frames after resize because that's
        // when the sibling timer happens to phase-align with the
        // un-drained slot.
        //
        // beginOffscreenFrame advances currentFrameSlot
        // (qrhivulkan.cpp:3025-3031) and resets the new slot's pool;
        // endOffscreenFrame waits on ofr.cmdFence (drains every
        // queued CB before the fence signals). Two iterations cover
        // QVK_FRAMES_IN_FLIGHT=2.
        for(int i = 0; i < 2; ++i)
        {
          QRhiCommandBuffer* cb{};
          if(rhi->beginOffscreenFrame(&cb) == QRhi::FrameOpSuccess)
            rhi->endOffscreenFrame();
        }
      }
      auto old_renderer = renderer;
      old_renderer->release();
      old_renderer.reset();

      auto new_renderer = createRenderList(&output, output.renderState());

      renderer = new_renderer;

      if(!renderer)
      {
        output.setRenderer({});
        it = m_renderers.erase(it);
      }
    }
    else
    {
    }
  }
}

void Graph::initializeOutput(OutputNode* output, GraphicsApi graphicsApi)
{
  output->updateGraphicsAPI(graphicsApi);
  if(!output->canRender() || !output->renderState())
  {
    auto onReady = [this, output] {
      if(output->canRender())
        createOutputRenderList(*output);
    };

    auto onResize = [this, output] {
      // FAST-PATH: pure viewport resize. Skip the full RL rebuild
      // (release+createRenderList) — its cost (pipeline compiles,
      // ScenePreprocessor REBUILD, mesh slab + texture array
      // re-upload, every preprocessor SSBO from cap=0) is wasted
      // when only the framebuffer size changed. Instead, mark every
      // renderer's RT specs as dirty so the existing rt_changed
      // surgical block in renderInternal recreates only the
      // swapchain-sized RTs + rebinds the downstream samplers.
      // Persistent GpuResourceRegistry + persistent ScenePreprocessor
      // caches mean none of the heavier work is needed for a pure
      // size change.
      //
      // Returns false if it cannot handle the change (no renderers
      // yet, invalid size); the fallback below covers initial setup
      // and any future "format / sample-count change" path.
      if(auto* rl = output->renderer())
        if(auto rs = output->renderState(); rs)
          if(rl->resizeSwapchainSizedTargets(rs->outputSize))
            return;
      recreateOutputRenderList(*output);
    };

    // TODO only works for one output !!
    output->createOutput({.graphicsApi = graphicsApi, .onReady = onReady, .onResize = onResize});
  }
  else
  {
    createOutputRenderList(*output);
    // output->window->state.hasSwapChain = true;
  }
}

void Graph::relinkGraph()
{
  for(auto r_it = m_renderers.begin(); r_it != m_renderers.end();)
  {
    auto& r = **r_it;

    assert(!r.nodes.empty());

    auto out = r.nodes.back();
    r.nodes.clear();
    r.nodes.push_back(out);

    r.clearRenderers();

    auto& model_nodes = r.nodes;
    {
      // In which order do we want to render stuff
      graphwalk(model_nodes);

      if(model_nodes.size() > 1)
      {
        bool invalid_renderlist = false;
        // Acquire a resource update batch for both brand-new renderers
        // (whose init() uploads material UBOs, creates samplers, etc.) and
        // reused renderers that we just released (whose init() must recreate
        // freed resources). Without reinitialising the reused path, a
        // second execution after stop/start leaves every reused renderer
        // in its released state forever.
        QRhiResourceUpdateBatch* batch = r.state.rhi
            ? r.state.rhi->nextResourceUpdateBatch()
            : nullptr;
        for(auto node : model_nodes)
        {
          score::gfx::NodeRenderer* rn{};
          auto it = node->renderedNodes.find(&r);
          const bool is_new = (it == node->renderedNodes.end());
          if(is_new)
          {
            if((rn = node->createRenderer(r)))
            {
              rn->nodeId = node->nodeId;
              node->renderedNodes.emplace(&r, rn);

              node->renderedNodesChanged();
            }
            else
            {
              invalid_renderlist = true;
              break;
            }
          }
          else
          {
            rn = it->second;
            SCORE_ASSERT(rn);
            rn->release(r);
          }
          SCORE_ASSERT(rn);
          if(batch)
            rn->init(r, *batch);
          r.renderers.push_back(rn);
        }

        // Fold the batch into the RenderList's initial batch so its uploads
        // (vertex buffers, placeholder UBOs, samplers) land before the first
        // render frame. `merge` copies entries but doesn't release `batch`
        // back to the pool — release it explicitly, or we leak a pool slot
        // per relinkGraph call and eventually exhaust the 64-slot pool.
        if(batch)
        {
          if(r.initialBatch())
          {
            r.initialBatch()->merge(batch);
            batch->release();
          }
          else
          {
            r.setInitialBatch(batch);
          }
        }

        // If a node couldn't be recreated, we skip the whole thing
        if(invalid_renderlist)
        {
          r.output.setRenderer({});
          r_it = m_renderers.erase(r_it);
          break;
        }
      }
      else if(model_nodes.size() == 1)
      {
        SCORE_ASSERT(
            model_nodes[0]->renderedNodes.find(&r)
            != model_nodes[0]->renderedNodes.end());
        auto rn = model_nodes[0]->renderedNodes.find(&r)->second;
        SCORE_ASSERT(rn);
        rn->release(r);
      }
    }
    r.output.onRendererChange();

    ++r_it;
  }

  if(m_outputs.size() > m_renderers.size())
  {
    // Try to recreate missing ones
    for(auto& output : m_outputs)
    {
      if(!output->renderer() && output->canRender())
      {
        createOutputRenderList(*output);
      }
    }
  }
}

bool Graph::canDoVSync() const noexcept
{
  return m_outputs.size() == 1
         && m_outputs[0]->configuration().supportsVSync;
}

static bool createNodeRenderer(score::gfx::Node& node, RenderList& r)
{
  // Register the node with the renderer
  if(auto rn = node.createRenderer(r))
  {
    rn->nodeId = node.nodeId;
    r.renderers.push_back(rn);

    // Register the rendered nodes with their parents
    SCORE_ASSERT(node.renderedNodes.find(&r) == node.renderedNodes.end());
    node.renderedNodes.emplace(&r, rn);
    node.renderedNodesChanged();
    return true;
  }

  return false;
}

std::shared_ptr<RenderList>
Graph::createRenderList(OutputNode* output, std::shared_ptr<RenderState> state)
{
  auto ptr = std::make_shared<RenderList>(*output, state);
  // Forward the session-wide AssetTable (if any) so ScenePreprocessor
  // and other renderers can hit the content-hash decode cache
  // instead of decoding every texture per-RenderList. Plan 09 S1.
  ptr->setAssetTable(m_assetTable);
  state->renderer = ptr;
  output->setRenderer(ptr);
#if 0
  for(auto& model : m_nodes)
    qDebug() << "Model: " << typeid(*model).name();
  for(auto node : m_nodes)
  {
    qDebug() << node->nodeId << typeid(*node).name();
    for(auto inlet : node->input)
    {
      qDebug() << "Inlet: " << magic_enum::enum_name(inlet->type) << inlet->edges.size();
      for(auto edge : inlet->edges) {
        qDebug() << edge->source->node << " => "<< edge->sink->node;
      }
    }
    for(auto outlet : node->output)
    {
      qDebug() << "Outlet: " << magic_enum::enum_name(outlet->type) << outlet->edges.size();
      for(auto edge : outlet->edges) {
        qDebug() << edge->source->node << " => "<< edge->sink->node;
      }
    }
  }
  for(auto edge  : m_edges)
  {
    qDebug() << "Edge:" << edge->source->node << " => "<< edge->sink->node;
  }
#endif

  RenderList& r = *ptr;
  auto& model_nodes = r.nodes;

  {
    model_nodes.push_back(output);

    // In which order do we want to render stuff
    graphwalk(model_nodes);

    // Now we have the nodes in the order in which they are going to
    // be init'd (e.g. output node first to create the render targets)
    // We create renderers for each of them
    for(auto node : model_nodes)
    {
      if(!createNodeRenderer(*node, r))
      {
        output->setRenderer(nullptr);
        return {};
      }
    }
  }

  output->onRendererChange();
  {
    r.init();

    // Compute m_requiresDepth from the node graph BEFORE
    // createAllInputRenderTargets — RT creation reads it. Mirrors
    // maybeRebuild's recompute at RenderList.cpp:484-486.
    {
      bool requiresDepth = false;
      for(auto node : r.nodes)
        requiresDepth |= node->requiresDepth;
      r.markRequiresDepth(requiresDepth);
    }

    // Create all input render targets centrally before any node init().
    // This ensures RTs are available regardless of init order
    // (matches what maybeRebuild does).
    r.createAllInputRenderTargets();

    // Always init all renderers, even when only the output node exists.
    // This ensures the output renderer's internal render target (e.g.
    // ScaledRenderer::m_inputTarget) is created and available for
    // incremental edge additions later.
    auto batch = r.initialBatch();
    for(auto node : r.renderers)
    {
      node->init(r, *batch);
      // Sync change indices so the first render frame doesn't see
      // a spurious rt_changed. Between init and the first render,
      // update_inputs() can deliver render_target_spec messages that
      // increment the node's counter. Without syncing, the renderer's
      // stale index (-1) mismatches → rt_changed triggers → release+init
      // Sync change indices to prevent spurious rt_changed, then set
      // materialChanged and geometryChanged so the first update() uploads
      // data and processes geometry. This matches what the old maybeRebuild()
      // did. renderTargetSpecsChanged is left false (synced) to prevent
      // the destructive rt_changed block from triggering.
      node->checkForChanges();
      node->materialChanged = true;
      node->geometryChanged = true;
      node->renderTargetSpecsChanged = false;
    }

    // Mark built. Skips the wasteful + previously-dangerous mid-frame
    // release()+init() that maybeRebuild(false) would otherwise fire on
    // the first render frame. Without this, every viewport resize did
    // a full RenderList teardown TWICE in quick succession (once here,
    // once on the next frame in maybeRebuild) -- multi-second resizes
    // for non-trivial scenes. The mid-frame teardown was also the root
    // of the CB-cascade chased through commits 51400fc37 / 5b2da1d48 /
    // 7f9f1e36a. The safety net (C2 drain in maybeRebuild) stays in
    // place for forced rebuilds and the actual size-change cycle in
    // maybeRebuild on subsequent frames.
    //
    // The historical concerns the previous comment cited (null
    // processUBO in MRT blit passes, feedback ISF persistent textures,
    // surgical rt_changed handling) were all fixed in their respective
    // commits. The two missing pieces vs maybeRebuild's release+init
    // (m_requiresDepth recompute, markBuilt) are now done here.
    r.markBuilt();
  }

  return ptr;
}

void Graph::removeNodeFromRenderLists(Node* node)
{
  for(auto& [rl, renderer] : node->renderedNodes)
  {
    renderer->releaseState(*rl);
    delete renderer;

    ossia::remove_erase(rl->renderers, renderer);
    ossia::remove_erase(rl->nodes, node);
  }

  node->renderedNodes.clear();
  node->renderedNodesChanged();
}

void Graph::removeNodeAndEdges(Node* node)
{
  // 1. For each edge involving this node, notify the render lists
  //    so that upstream/downstream renderers clean up their passes.
  //    Must happen BEFORE edge deletion (onEdgeRemoved reads the edge).
  for(auto* edge : m_edges)
  {
    if(edge->source->node == node || edge->sink->node == node)
    {
      // Notify affected render lists
      Node* other = (edge->source->node == node)
                         ? edge->sink->node
                         : edge->source->node;

      for(auto& rl : m_renderers)
      {
        if(ossia::contains(rl->nodes, other)
           || ossia::contains(rl->nodes, node))
        {
          rl->onEdgeRemoved(*edge);
        }
      }
    }
  }

  // 2. Delete all edges involving this node from m_edges.
  //    Edge destructor removes from source->edges and sink->edges.
  for(auto it = m_edges.begin(); it != m_edges.end();)
  {
    Edge* edge = *it;
    if(edge->source->node == node || edge->sink->node == node)
    {
      delete edge;
      it = m_edges.erase(it);
    }
    else
    {
      ++it;
    }
  }

  // 3. Release the node's own renderers from all render lists.
  removeNodeFromRenderLists(node);

  // 4. Retopological sort all affected render lists and notify outputs.
  for(auto& rl : m_renderers)
  {
    retopologicalSort(*rl);
    rl->output.onRendererChange();
  }

  // Note: does NOT remove from m_nodes — the caller (GfxContext::remove_node)
  // handles that via Graph::removeNode().
}

void Graph::onEdgeRemoved(
    Edge& edge, const ossia::hash_set<const Port*>* preserveSinks)
{
  Node* source = edge.source->node;

  for(auto& rl : m_renderers)
  {
    // Only act on render lists that contain the source node
    if(!ossia::contains(rl->nodes, source))
      continue;

    // Delegate to the render list (must happen before edge destruction)
    rl->onEdgeRemoved(edge, preserveSinks);

    // Do NOT retopological-sort or destroy unreachable renderers here.
    // Removals are processed before additions in incrementalEdgeUpdate.
    // A node that becomes temporarily unreachable during removal may become
    // reachable again when additions are processed. Destroying its renderer
    // would lose runtime state (mesh data, video frames, etc.) that can't
    // be trivially recreated.
    //
    // reconcileAllRenderLists() runs after all adds/removes and handles
    // the final reachability check, renderer cleanup, and retopo sort.
  }
}

void Graph::createPassForEdgeIfMissing(Edge& edge)
{
  Node* source = edge.source->node;

  for(auto& rl : m_renderers)
  {
    // Check if the source node has a renderer in this render list
    auto rn_it = source->renderedNodes.find(rl.get());
    if(rn_it == source->renderedNodes.end())
      continue;

    auto* renderer = rn_it->second;

    // Check if the sink node is also in this render list
    if(!ossia::contains(rl->nodes, edge.sink->node))
      continue;

    // Check if a pass already exists for this edge
    if(renderer->hasOutputPassForEdge(edge))
      continue;

    // Ensure the sink port has a render target (if needed)
    Port* sink = edge.sink;
    if(sink->type == Types::Image
       && (sink->flags & Flag::GrabsFromSource) != Flag::GrabsFromSource
       && sink->node != &rl->output)
    {
      if(rl->renderTargetForInputPort(*sink).renderTarget == nullptr)
      {
        int cur_port = 0;
        for(auto* in : sink->node->input)
        {
          if(in == sink)
            break;
          cur_port++;
        }
        auto spec = sink->node->resolveRenderTargetSpecs(cur_port, *rl);
        if(!sink->node->hasExplicitRenderTargetSize(cur_port))
        {
          ossia::small_flat_map<const Port*, RenderTargetSpecs, 16> emptySpecs;
          QSize downstream = rl->resolveDownstreamSize(sink->node, emptySpecs);
          if(!downstream.isEmpty())
            spec.size = downstream;
        }
        bool wantsDepth = rl->requiresDepth(*sink);
        bool wantsSamplableDepth
            = (sink->flags & Flag::SamplableDepth) == Flag::SamplableDepth;
        auto rt = createRenderTarget(
            rl->state, spec.format, spec.size, rl->samples(),
            wantsDepth || wantsSamplableDepth, wantsSamplableDepth);
        rl->m_inputRenderTargets[sink] = std::move(rt);
      }
    }

    // Create the output pass on the source renderer.
    // Allocate a fresh batch, collect `addOutputPass`'s updates, then
    // either promote it to the RL's initial batch or merge + release.
    // QRhiResourceUpdateBatch::merge does NOT release the source batch
    // — without the explicit release() the 64-slot pool exhausts after
    // enough edges (e.g. when a live-connected shader triggers
    // createAllMissingPasses over a large scene graph) and the next
    // nextResourceUpdateBatch() returns null → crash on merge.
    auto* batch = rl->state.rhi->nextResourceUpdateBatch();
    if(!batch)
      continue;
    renderer->addOutputPass(*rl, edge, *batch);

    if(rl->initialBatch())
    {
      rl->initialBatch()->merge(batch);
      batch->release();
    }
    else
    {
      rl->setInitialBatch(batch);
    }
  }
}

void Graph::createAllMissingPasses()
{
  for(auto* edge : m_edges)
    createPassForEdgeIfMissing(*edge);
}

void Graph::updateAllSinkSamplers()
{
  for(auto* edge : m_edges)
    updateSinkSampler(*edge);
}

void Graph::updateSinkSampler(Edge& edge)
{
  Port* sink = edge.sink;
  if(sink->type != Types::Image)
    return;

  // GrabsFromSource ports don't have a render target — they need the
  // upstream's QRhiTexture directly via textureForOutput(). This path
  // covers cubemaps, 3D textures, AND texture arrays (e.g.
  // ScenePreprocessor's base_color_array feeding classic_pbr_textured).
  // Without this, the sink keeps binding emptyTexture (2D, single-layer)
  // into what the shader expects as sampler2DArray → Vulkan validation
  // error VUID-vkCmdDrawIndexed-viewType-07752, nothing renders.
  if((sink->flags & Flag::GrabsFromSource) == Flag::GrabsFromSource)
  {
    Port* source = edge.source;
    if(!source || !source->node)
      return;
    for(auto& rl : m_renderers)
    {
      auto sink_rn_it = sink->node->renderedNodes.find(rl.get());
      if(sink_rn_it == sink->node->renderedNodes.end())
        continue;
      auto src_rn_it = source->node->renderedNodes.find(rl.get());
      if(src_rn_it == source->node->renderedNodes.end())
        continue;
      if(auto* tex = src_rn_it->second->textureForOutput(*source))
        sink_rn_it->second->updateInputTexture(*sink, tex);
    }
    return;
  }

  for(auto& rl : m_renderers)
  {
    auto sink_rn_it = sink->node->renderedNodes.find(rl.get());
    if(sink_rn_it == sink->node->renderedNodes.end())
      continue;

    // For output nodes, the RT comes from the renderer itself
    if(sink->node == &rl->output)
    {
      auto rt = sink_rn_it->second->renderTargetForInput(*sink);
      if(rt.texture)
        sink_rn_it->second->updateInputTexture(*sink, rt.texture, rt.depthTexture);
    }
    else
    {
      // For intermediate nodes, the RT comes from the centralized map
      auto rt = rl->renderTargetForInputPort(*sink);
      if(rt.texture)
        sink_rn_it->second->updateInputTexture(*sink, rt.texture, rt.depthTexture);
    }
  }
}

void Graph::reconcileAllRenderLists()
{
  for(auto& rl : m_renderers)
  {
    // 1. Re-walk the graph from output to discover all reachable nodes.
    auto* outputNode = rl->nodes.front();
    rl->nodes.clear();
    rl->nodes.push_back(outputNode);
    graphwalk(rl->nodes);

    // 2. Find nodes that are newly reachable (no renderer yet)
    //    and nodes that are no longer reachable (have renderer but not in walk).
    ossia::flat_set<Node*> reachable(rl->nodes.begin(), rl->nodes.end());
    // Collect all nodes that have renderers for this RL
    std::vector<Node*> nodesWithRenderers;
    for(auto* node : m_nodes)
    {
      if(node->renderedNodes.find(rl.get()) != node->renderedNodes.end())
        nodesWithRenderers.push_back(node);
    }

    // 3. Remove renderers for nodes no longer reachable.
    for(auto* node : nodesWithRenderers)
    {
      if(!reachable.contains(node))
      {
        auto rn_it = node->renderedNodes.find(rl.get());
        if(rn_it != node->renderedNodes.end())
        {
          auto* renderer = rn_it->second;
          BUFTRACE() << "reconcile: releasing unreachable renderer="
                     << (void*)renderer
                     << " node_id=" << node->nodeId
                     << " (any downstream node still referencing this "
                        "renderer's buffers via process() caches will see "
                        "stale pointers → ASan target)";
          renderer->releaseState(*rl);
          delete renderer;
          node->renderedNodes.erase(rn_it);
          node->renderedNodesChanged();
        }
      }
    }

    // 4. Ensure render targets exist for all input ports BEFORE creating
    //    renderers. initState() → initInputSamplers() looks up the RT
    //    texture — if the RT doesn't exist yet, the sampler gets emptyTexture
    //    and the SRB will have wrong bindings.
    for(auto* node : rl->nodes)
    {
      if(node == &rl->output)
        continue;
      int cur_port = 0;
      for(auto* in : node->input)
      {
        if(in->type == Types::Image
           && (in->flags & Flag::GrabsFromSource) != Flag::GrabsFromSource)
        {
          if(rl->renderTargetForInputPort(*in).renderTarget == nullptr)
          {
            // Create the missing render target
            auto spec = node->resolveRenderTargetSpecs(cur_port, *rl);
            if(!node->hasExplicitRenderTargetSize(cur_port))
            {
              ossia::small_flat_map<const Port*, RenderTargetSpecs, 16> emptySpecs;
              QSize downstream = rl->resolveDownstreamSize(node, emptySpecs);
              if(!downstream.isEmpty())
                spec.size = downstream;
            }
            bool wantsDepth = rl->requiresDepth(*in);
            bool wantsSamplableDepth
                = (in->flags & Flag::SamplableDepth) == Flag::SamplableDepth;
            auto rt = createRenderTarget(
                rl->state, spec.format, spec.size, rl->samples(),
                wantsDepth || wantsSamplableDepth, wantsSamplableDepth);
            rl->m_inputRenderTargets[in] = std::move(rt);
          }
        }
        cur_port++;
      }
    }

    // 5. Create renderers for newly-reachable nodes (AFTER render targets
    //    exist so that initState → initInputSamplers finds the correct textures).
    QRhiResourceUpdateBatch* batch = rl->state.rhi->nextResourceUpdateBatch();
    bool batchUsed = false;

    for(auto* node : rl->nodes)
    {
      if(node->renderedNodes.find(rl.get()) == node->renderedNodes.end())
      {
        if(auto* rn = node->createRenderer(*rl))
        {
          rn->nodeId = node->nodeId;
          node->renderedNodes.emplace(rl.get(), rn);
          node->renderedNodesChanged();

          // All renderers now implement initState(). Pass creation for
          // individual edges is handled by createPassForEdgeIfMissing
          // after reconciliation, ensuring all renderers + RTs exist first.
          rn->initState(*rl, *batch);
          rn->checkForChanges();
          rn->materialChanged = true;
          rn->geometryChanged = true;
          rn->renderTargetSpecsChanged = false;

          // Seed downstream consumers with this newly-created renderer's
          // outputs so live-inserted scene producers (Camera, Environment,
          // Light) don't need a full stop/restart to take
          // effect. Default no-op for everything else.
          rn->seedInitialOutputs(*rl);

          batchUsed = true;
        }
      }
    }

    // 6. Pass creation is now handled entirely by createPassForEdgeIfMissing
    //    in incrementalEdgeUpdate, after reconciliation completes and all
    //    renderers + RTs exist. No sweep needed here.

    // 7. Rebuild renderers vector from node order.
    //    Also sync change indices for ALL renderers (not just newly created)
    //    to prevent spurious rt_changed on the first render frame.
    //    Without this, existing renderers whose nodes received process()
    //    messages (via update_inputs) between reconciliation and rendering
    //    could have stale indices, triggering a full release+init in the
    //    rt_changed block — which destroys the feedback ISF's persistent textures.
    rl->renderers.clear();
    // Filter nodes to only those with renderers
    std::vector<score::gfx::Node*> validNodes;
    validNodes.reserve(rl->nodes.size());
    for(auto* node : rl->nodes)
    {
      auto rn_it = node->renderedNodes.find(rl.get());
      if(rn_it != node->renderedNodes.end())
      {
        validNodes.push_back(node);
        auto* rn = rn_it->second;
        rl->renderers.push_back(rn);

        // Sync change indices and prevent spurious rt_changed
        rn->checkForChanges();
        rn->renderTargetSpecsChanged = false;
      }
    }
    rl->nodes = std::move(validNodes);

    // 8. Submit batch and notify output. `merge()` copies entries but
    // does NOT release the source batch, so we have to do it ourselves
    // — otherwise the 64-slot pool leaks one slot per reconcile.
    if(batchUsed)
    {
      if(rl->initialBatch())
      {
        rl->initialBatch()->merge(batch);
        batch->release();
      }
      else
      {
        rl->setInitialBatch(batch);
      }
    }
    else
    {
      batch->release();
    }

    rl->output.onRendererChange();
  }
}

void Graph::retopologicalSort(RenderList& rl)
{
  // Save the output node (always first in the list)
  auto* outputNode = rl.nodes.front();

  // Clear and re-walk
  rl.nodes.clear();
  rl.nodes.push_back(outputNode);
  graphwalk(rl.nodes);

  // Rebuild renderers vector from the new node order.
  // Only include nodes that actually have a renderer for this RenderList.
  // Nodes discovered by the graph walk but without renderers (e.g. just
  // added to the graph but not yet processed by reconcileAllRenderLists) are excluded
  // from both lists to prevent the render loop from asserting.
  rl.renderers.clear();
  std::vector<score::gfx::Node*> valid_nodes;
  valid_nodes.reserve(rl.nodes.size());
  for(auto* node : rl.nodes)
  {
    auto it = node->renderedNodes.find(&rl);
    if(it != node->renderedNodes.end())
    {
      valid_nodes.push_back(node);
      rl.renderers.push_back(it->second);
    }
  }
  rl.nodes = std::move(valid_nodes);
}

Graph::Graph() { }

Graph::~Graph()
{
  for(auto& renderer : m_renderers)
  {
    renderer->release();
  }

  for(auto out : m_outputs)
  {
    out->destroyOutput();
  }

  // Belt-and-braces: any OutputNode registered via addNode but not yet
  // promoted into m_outputs (e.g. preview outputs added via
  // createSingleRenderList without a subsequent createAllRenderLists)
  // would otherwise leak its swapchain / RPD on shutdown.
  for(auto* n : m_nodes)
  {
    if(auto* out = dynamic_cast<OutputNode*>(n))
    {
      if(!ossia::contains(m_outputs, out))
        out->destroyOutput();
    }
  }

  clearEdges();
}

void Graph::addNode(Node* n)
{
  m_nodes.push_back(n);
}

void Graph::removeNode(Node* n)
{
  ossia::remove_erase(m_nodes, n);
}

void Graph::clearEdges()
{
  for(auto edge : m_edges)
  {
    delete edge;
  }
  m_edges.clear();
}

void Graph::addEdge(Port* source, Port* sink, Process::CableType t)
{
  auto it = ossia::find_if(
      m_edges, [=](Edge* e) { return e->source == source && e->sink == sink; });

  if(it == m_edges.end())
  {
    m_edges.push_back(new Edge{source, sink, t});
  }
  else
  {
    (*it)->type = t;
#if defined(SCORE_DEBUG)
    qDebug() << "Tried to add edge between " << source << sink << "\n   ==> "
             << typeid(*source->node).name() << typeid(*sink->node).name();
#endif
  }
}

void Graph::removeEdge(Port* source, Port* sink)
{
  auto it = ossia::find_if(
      m_edges, [=](Edge* e) { return e->source == source && e->sink == sink; });
  if(it != m_edges.end())
  {
    delete *it;
    m_edges.erase(it);
  }
}

void Graph::destroyOutputRenderList(score::gfx::OutputNode& output)
{
  auto it = ossia::find_if(
      m_renderers, [rend = output.renderer()](const std::shared_ptr<RenderList>& r) {
        return r.get() == rend;
      });

  if(it != m_renderers.end())
  {
    std::shared_ptr<RenderList>& renderer = *it;
    if(renderer.get() == output.renderer())
    {
      renderer->release();
      renderer.reset();

      output.setRenderer({});
      it = m_renderers.erase(it);
    }
    else
    {
    }
  }

  output.destroyOutput();
  ossia::remove_erase(m_outputs, &output);
}

}
