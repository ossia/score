#pragma once
#include <Process/Dataflow/CableData.hpp>

#include <Gfx/Graph/Node.hpp>
#include <Gfx/Graph/RenderList.hpp>

#include <ossia/detail/algorithms.hpp>

#include <score_plugin_gfx_export.h>
namespace Gfx
{
class AssetTable;
}
namespace score::gfx
{
class OutputNode;
class Window;
/**
 * @brief Represents a graph of renderers.
 */
struct SCORE_PLUGIN_GFX_EXPORT Graph
{
  /**
   * @brief Create a render graph.
   */
  explicit Graph();

  ~Graph();

  /**
   * @brief Add a node to the graph.
   */
  void addNode(score::gfx::Node* n);

  /**
   * @brief Remove a node from the graph.
   */
  void removeNode(score::gfx::Node* n);

  /**
   * @brief Add an edge between two nodes.
   */
  void addEdge(Port* source, Port* sink, Process::CableType t);

  /**
   * @brief Remove an edge between two nodes.
   */
  void removeEdge(Port* source, Port* sink);

  /// Remove a node's renderers from all render lists.
  void removeNodeFromRenderLists(Node* node);

  /// Incrementally remove a non-output node: notify renderers of each
  /// edge being removed, delete edges from m_edges, release the node's
  /// renderers, retopological sort affected render lists, remove from m_nodes.
  void removeNodeAndEdges(Node* node);

  /// Called when an edge is removed from the graph.
  ///
  /// @param preserveSinks Optional set of sink Ports whose input render
  ///   target should be kept alive even if this edge was their only feed.
  ///   GfxContext::incrementalEdgeUpdate uses this to bridge the brief
  ///   "sink has 0 edges" window that appears during a mid-batch filter
  ///   insertion (A→B removed, A→F and F→B added in the same batch).
  ///   Without this, B's input RT would be destroyed and immediately
  ///   re-allocated with the same spec.
  void
  onEdgeRemoved(Edge& edge, const ossia::hash_set<const Port*>* preserveSinks = nullptr);

  /// For an added edge, update the sink renderer's input sampler
  /// to point to the (possibly new) render target texture.
  void updateSinkSampler(Edge& edge);

  /// Create missing passes and update samplers for ALL edges in ALL render lists.
  void createAllMissingPasses();
  void updateAllSinkSamplers();

  /// For an added edge, create the output pass on the source renderer
  /// if it exists but doesn't already have a pass for this edge.
  void createPassForEdgeIfMissing(Edge& edge);

  /// After all edges have been added/removed, reconcile all render lists:
  /// retopological sort, create renderers for newly-reachable nodes,
  /// create render targets and passes, remove unreachable nodes.
  void reconcileAllRenderLists();

  /**
   * @brief Remove all edges.
   */
  void clearEdges();

  /**
   * @brief For each output node, create the sequence of render events that will be called.
   */
  void createAllRenderLists(GraphicsApi graphicsApi);

  /**
   * @brief Create a sequence of render events for a single output node
   */
  void createSingleRenderList(score::gfx::OutputNode& node, GraphicsApi graphicsApi);

  /**
   * @brief Free the render list of an output node if any
   */
  void destroyOutputRenderList(score::gfx::OutputNode& node);

  /**
   * @brief Recreate the connections between renderers when edges changed.
   */
  void relinkGraph();

  /**
   * @brief True if the graph supports being driven by the screen vertical synchronization.
   */
  bool canDoVSync() const noexcept;

  const std::vector<std::shared_ptr<RenderList>>& renderLists() const noexcept
  {
    return m_renderers;
  }

  std::span<OutputNode* const> outputs() const noexcept
  {
    return m_outputs;
  }

  /**
   * @brief Inject the session-wide AssetTable (Plan 09 S1).
   *
   * GfxContext owns the AssetTable and calls this once at graph
   * construction. All RenderLists subsequently created by this
   * Graph receive the pointer via their constructor, so the
   * preprocessor can hit the content-hash cache when decoding
   * texture_source / buffer_resource payloads.
   *
   * Null is allowed (tests, early teardown) — consumers guard.
   */
  void setAssetTable(Gfx::AssetTable* a) noexcept { m_assetTable = a; }
  Gfx::AssetTable* assetTable() const noexcept { return m_assetTable; }

private:
  /// Re-run topological sort for a render list and rebuild renderer ordering.
  void retopologicalSort(RenderList& rl);

  void initializeOutput(OutputNode* output, GraphicsApi graphicsApi);
  void createOutputRenderList(OutputNode& output);
  void recreateOutputRenderList(OutputNode& output);
  std::shared_ptr<RenderList>
  createRenderList(OutputNode*, std::shared_ptr<RenderState> state);

  std::vector<std::shared_ptr<RenderList>> m_renderers;
  std::vector<std::shared_ptr<Window>> m_unused_windows;

  std::vector<score::gfx::Node*> m_nodes;
  std::vector<Edge*> m_edges;

  std::vector<OutputNode*> m_outputs;

  // Session-wide decode cache. Non-owning; GfxContext owns the
  // actual AssetTable. May be null in tests or during teardown.
  Gfx::AssetTable* m_assetTable{};
};
}
