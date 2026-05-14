#pragma once
#include <Gfx/Graph/Node.hpp>

namespace score::gfx
{

/**
 * @brief Renderer for a given node.
 */
class SCORE_PLUGIN_GFX_EXPORT NodeRenderer
{
public:
  explicit NodeRenderer(const Node& node)
      : node{node}
  {
  }
  virtual ~NodeRenderer();

  virtual TextureRenderTarget renderTargetForInput(const Port& input);
  virtual BufferView bufferForInput(const Port& input);
  virtual BufferView bufferForOutput(const Port& output);

  //! Returns a texture owned by this renderer for a given output port.
  //! Used for GrabsFromSource ports (e.g. cubemaps) where the upstream
  //! node produces a texture directly rather than rendering into a
  //! downstream-provided render target.
  virtual QRhiTexture* textureForOutput(const Port& output);

  //! Updates the sampler texture for an input port.
  //! Called when the upstream texture may have changed (edge add, RT recreation).
  //! If the port has SamplableDepth and depthTex is non-null, the depth
  //! sampler (immediately after the color sampler) is also updated.
  virtual void updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex = nullptr);

  //! Updates the sampler filter/address settings for an input port.
  //! Called when the render target spec changes (e.g. linear → nearest).
  virtual void updateInputSamplerFilter(
      const Port& input, const RenderTargetSpecs& spec);

  //! Called when all the inbound nodes to a texture input have finished rendering.
  //! Mainly useful to slip in a readback.
  virtual void
  inputAboutToFinish(RenderList& renderer, const Port& p, QRhiResourceUpdateBatch*&);

  virtual void init(RenderList& renderer, QRhiResourceUpdateBatch& res) = 0;
  virtual void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge)
      = 0;

  virtual void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge);

  virtual void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge);

  virtual void release(RenderList&) = 0;

  /**
   * @name Incremental lifecycle API
   *
   * These methods enable dynamic graph editing by splitting the init/release
   * lifecycle into edge-independent state and per-edge passes.
   *
   * Renderers that override these are incrementally updateable: adding or
   * removing an output edge only creates/destroys one pass, without touching
   * the rest of the renderer's GPU resources.
   *
   * Default implementations are no-ops for backward compatibility.
   * @{
   */

  /// Initialize edge-independent state: material UBO, samplers, mesh, shaders.
  /// Called once when the renderer enters a RenderList.
  virtual void initState(RenderList& renderer, QRhiResourceUpdateBatch& res);

  /// Release edge-independent state.
  /// Called once when the renderer leaves a RenderList.
  virtual void releaseState(RenderList& renderer);

  /// Create a pass for a new output edge (pipeline, SRB, processUBO).
  virtual void addOutputPass(
      RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res);

  /// Remove the pass for a removed output edge.
  /// Pure-virtual: every concrete renderer must explicitly handle edge
  /// removal. Sinks (OutputNodeRenderer) and data-only renderers that
  /// store no per-edge GPU state can override with an empty body.
  virtual void removeOutputPass(RenderList& renderer, Edge& edge) = 0;

  /// Notify the renderer that a new input edge was connected.
  /// Typically updates sampler textures or geometry bindings.
  virtual void
  addInputEdge(RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res);

  /// Notify the renderer that an input edge was disconnected.
  virtual void removeInputEdge(RenderList& renderer, Edge& edge);

  /// Check if this renderer already has an output pass for the given edge.
  virtual bool hasOutputPassForEdge(Edge& edge) const;

  /// Seed downstream consumers once at init-time with this renderer's
  /// current outputs. Default no-op. Halp scene/geometry producers (Camera,
  /// EnvironmentLoader, Light, …) override this to run their
  /// operator()() once during reconciliation and immediately push the
  /// result into each downstream sink's per-port scene cache — without
  /// this, a live-inserted producer's output wouldn't reach the sink's
  /// `m_portScenes` until the next render frame's upstream scan fires the
  /// producer's runInitialPasses, which can arrive too late relative to
  /// the sink's own frame-start cache snapshot and produce the
  /// "Camera inserted live has no effect until stop/restart" symptom.
  virtual void seedInitialOutputs(RenderList& renderer);

  /** @} */

  void checkForChanges()
  {
    // Use |= to preserve flags set externally (e.g. by reconciliation
    // or maybeRebuild). The flag is cleared by the renderer's update()
    // after processing, preventing infinite re-uploads.
    materialChanged |= node.hasMaterialChanged(materialChangedIndex);
    renderTargetSpecsChanged |= node.hasRenderTargetChanged(renderTargetSpecsChangedIndex);
  }

  /// Sync only the render target spec index without touching materialChanged.
  /// Used after initState() so the first render's checkForChanges() sees a
  /// material mismatch (triggering initial upload) but not a spurious rt_changed.
  void syncRenderTargetIndex()
  {
    node.hasRenderTargetChanged(renderTargetSpecsChangedIndex);
    renderTargetSpecsChanged = false;
  }

  void process(int32_t port, const ossia::geometry_spec& v);
  void process(int32_t port, const ossia::scene_spec& v);
  virtual void process(int32_t port, const ossia::transform3d& v);

  /// Source-aware overloads. `source_key` is an opaque identity of the
  /// upstream output port that produced this data (typically `edge.source`).
  /// Multiple producers converging on the same sink port each get their own
  /// storage slot, so their scenes accumulate additively instead of
  /// overwriting each other. Callers that don't care pass nullptr — all such
  /// callers then share a single per-port slot (legacy behavior).
  void process(int32_t port, const ossia::geometry_spec& v, const void* source_key);
  void process(int32_t port, const ossia::scene_spec& v, const void* source_key);

  /// Find the first geometry stored on the given sink port (across all
  /// sources). Legacy single-producer-per-port consumers use this to
  /// preserve pre-multi-producer behavior without caring who produced it.
  const ossia::geometry_spec* findGeometryByPort(int32_t port) const
  {
    for(const auto& [k, v] : m_portGeometries)
      if(k.first == port)
        return &v;
    return nullptr;
  }

  /// Enumerate every scene_spec published on `port` (across all sources).
  /// Populated for ALL geometry/scene edges — raw geometry_spec deliveries
  /// are auto-wrapped into scene_specs and cached (see m_wrapCache), so the
  /// scene_state_ptr returned here is stable across frames when the input
  /// doesn't actually change. Callers doing scene-broadcast iterate this
  /// and check scene_state::dirty_index + state pointer for invalidation.
  template <typename F>
  void forEachSceneOnPort(int32_t port, F&& fn) const
  {
    for(const auto& [k, v] : m_portScenes)
      if(k.first == port && v.state)
        fn(v);
  }

private:
  /// Recompute `this->scene` from the current per-port inputs, reusing the
  /// memoized merge when the set of input scene_state pointers is unchanged.
  void rebuildMergedScene();

public:

  const Node& node;

  /**
   * @brief The geometry to use
   *
   * If not set, then a relevant default geometry for the node
   * will be used, e.g. a full-screen quad or triangle.
   *
   * For backward compatibility with single-port nodes (GenericNodeRenderer,
   * RenderedRawRasterPipelineNode, etc.), this holds the last received geometry.
   * Multi-geometry-port nodes (CSF) should use m_portGeometries instead.
   */
  ossia::geometry_spec geometry;

  /// Per-(port, source) geometry storage. Multi-keyed so multiple upstream
  /// producers converging on the same sink port each get their own slot
  /// (additive merge rather than overwrite). The source_key is the upstream
  /// output Port pointer (opaque void*); nullptr is a valid single-slot key
  /// for legacy callers.
  using PortSourceKey = std::pair<int32_t, const void*>;
  ossia::small_flat_map<PortSourceKey, ossia::geometry_spec, 4> m_portGeometries;

  /**
   * @brief The scene to use (when receiving scene_spec data).
   *
   * When a geometry_spec is received, it is auto-wrapped into a scene_spec
   * so that downstream scene-aware renderers can always work with scenes.
   * Backward-compat renderers continue reading the `geometry` field.
   */
  ossia::scene_spec scene;

  /// Per-(port, source) scene storage. See m_portGeometries comment.
  ossia::small_flat_map<PortSourceKey, ossia::scene_spec, 4> m_portScenes;

  /// Merge cache: the set of (scene_state pointer, version) pairs we
  /// last merged, and the resulting merged scene_spec. Keyed on BOTH
  /// pointer and version because halp-style producers (Camera,
  /// Environment, Light, …) keep a stable `m_state`
  /// shared_ptr and mutate its contents in place — keying on pointer
  /// alone would return a stale cached merge even after a slider moved.
  /// The version monotonically bumps on each producer update, so
  /// (ptr, version) changes whenever content changes.
  using MergeCacheKey = std::pair<const ossia::scene_state*, int64_t>;
  ossia::small_vector<MergeCacheKey, 4> m_mergeCacheInputs;
  ossia::scene_spec m_mergeCacheOutput;

  /// Cache the wrap_geometry_as_scene result per geometry_spec so a
  /// geometry source re-pushing the same geometry_spec every frame
  /// produces a stable wrapped-scene shared_ptr (otherwise every frame
  /// produces a new wrapper → merge cache miss → full re-upload).
  ossia::small_flat_map<
      PortSourceKey, std::pair<ossia::geometry_spec, ossia::scene_spec>, 4>
      m_wrapCache;

  int32_t nodeId{-1};
  bool materialChanged{false};
  bool geometryChanged{false};
  bool sceneChanged{false};
  bool renderTargetSpecsChanged{false};

  /// Guard for idempotent release — prevents double-release of GPU resources.
  /// Set to true at end of init(), cleared at start of release().
  bool m_initialized{false};

private:
  int64_t materialChangedIndex{-1};
  int64_t renderTargetSpecsChangedIndex{-1};
};

struct Pass
{
  // User-declared ctors (including the implicit ones made explicit
  // here) suppress -Wmissing-field-initializers on the many call sites
  // that brace-init this struct with three arguments — the fallback
  // plan is always default-constructed into an empty list, which is
  // exactly what non-fallback pipelines need. Removing aggregate-init
  // eligibility is intentional; the tradeoff is one line per call
  // site (if they want to set fallback_bindings, they assign after).
  Pass() = default;
  Pass(TextureRenderTarget rt, Pipeline pi, QRhiBuffer* ubo)
      : renderTarget{std::move(rt)}, p{pi}, processUBO{ubo} {}

  TextureRenderTarget renderTarget;
  Pipeline p;
  QRhiBuffer* processUBO{};
  // Bindings for "REQUIRED: false" VERTEX_INPUTS that had no matching
  // upstream attribute when this pass's pipeline was built. Empty for
  // pipelines where the shader is strict-matched (the common case).
  // Consumed by the draw path: each slot's buffer is bound at its
  // `binding_index` in the vertex-input array before the draw call.
  // The buffers themselves are owned by VertexFallbackPool — the plan
  // holds non-owning pointers.
  FallbackBindingPlan fallback_bindings;

  void release()
  {
    p.release();
    if(processUBO)
    {
      processUBO->deleteLater();
      processUBO = nullptr;
    }
    fallback_bindings.clear();
    // renderTarget NOT released here — owned by RenderList
  }
};

using PassMap = ossia::small_vector<std::pair<Edge*, Pass>, 2>;
SCORE_PLUGIN_GFX_EXPORT
void defaultPassesInit(
    PassMap& passes, const std::vector<Edge*>& edges, RenderList& renderer,
    const Mesh& mesh, const QShader& v, const QShader& f, QRhiBuffer* processUBO,
    QRhiBuffer* matUBO, std::span<const Sampler> samplers,
    std::span<QRhiShaderResourceBinding> additionalBindings = {});

SCORE_PLUGIN_GFX_EXPORT
void defaultRenderPass(
    RenderList& renderer, const Mesh& mesh, const MeshBuffers& bufs,
    QRhiCommandBuffer& cb, Edge& edge, PassMap& passes);

SCORE_PLUGIN_GFX_EXPORT
void quadRenderPass(
    RenderList& renderer, const MeshBuffers& bufs, QRhiCommandBuffer& cb, Edge& edge,
    PassMap& passes);

/**
 * @brief Generic renderer.
 *
 * Used for the common case of a single pass node with a vertex & fragment shader,
 * samplers, and a single render target.
 */
class SCORE_PLUGIN_GFX_EXPORT GenericNodeRenderer : public score::gfx::NodeRenderer
{
public:
  GenericNodeRenderer(const NodeModel& node) noexcept
      : NodeRenderer{node}
  {
  }

  virtual ~GenericNodeRenderer() { }

  ossia::small_vector<Sampler, 8> m_samplers;

  QShader m_vertexS;
  QShader m_fragmentS;

  // Pipeline
  PassMap m_p;

  // Per-renderer pipeline cache, keyed by QRhiRenderPassDescriptor pointer.
  // Edges targeting the same QRhiRenderTarget (and therefore the same
  // rp-desc pointer) share one QRhiGraphicsPipeline — the pipeline object
  // is bound to an rp-desc layout, not to the RT object itself, and QRhi
  // guarantees the same pipeline can be used with any RT whose rp-desc
  // isCompatible with the pipeline's. Looking up by pointer (rather than
  // by serialized format) is the conservative choice: a pointer match
  // means "same rp-desc, same owning RT alive" and cannot collide with a
  // stale entry because a freshly allocated rp-desc always sits at a
  // different address than one that was just destroyed via deleteLater.
  //
  // Ownership: Pass::p.pipeline is NON-OWNING — the actual QRhiGraphicsPipeline
  // lives in this cache. Pass::p.srb is still per-edge and owned by the Pass.
  // GenericNodeRenderer::removeOutputPass and releaseState take care of
  // nulling Pass::p.pipeline before calling Pipeline::release() so it
  // does not try to deleteLater() a pointer we still own here.
  ossia::small_vector<
      std::pair<QRhiRenderPassDescriptor*, QRhiGraphicsPipeline*>, 2>
      m_pipelineCache;

  MeshBuffers m_meshbufs;

  QRhiBuffer* m_processUBO{};

  DefaultShaderMaterial m_material;

  const score::gfx::Mesh* m_mesh{};

  // Render loop
  void
  defaultMeshInit(RenderList& renderer, const Mesh& mesh, QRhiResourceUpdateBatch& res);
  void processUBOInit(RenderList& renderer);
  void defaultPassesInit(RenderList& renderer, const Mesh& mesh);
  void defaultPassesInit(
      RenderList& renderer, const Mesh& mesh, const QShader& v, const QShader& f,
      std::span<QRhiShaderResourceBinding> additionalBindings = {});

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;

  void initState(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void releaseState(RenderList& renderer) override;
  void addOutputPass(
      RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override;
  void removeOutputPass(RenderList& renderer, Edge& edge) override;
  bool hasOutputPassForEdge(Edge& edge) const override;

  void defaultUBOUpdate(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void defaultMeshUpdate(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;

  void defaultRelease(RenderList&);
  void release(RenderList&) override;

  void defaultRenderPass(
      RenderList&, const Mesh& mesh, QRhiCommandBuffer& commands, Edge& edge);

  void defaultRenderPass(
      RenderList&, const Mesh& mesh, QRhiCommandBuffer& commands, Edge& edge,
      PassMap& passes);

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

  void updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex = nullptr) override;
};

}
