#pragma once
#include <Gfx/Graph/GPUBufferScatter.hpp>
#include <Gfx/Graph/ISFNode.hpp>
#include <Gfx/Graph/NodeRenderer.hpp>

#include <ossia/detail/small_flat_map.hpp>
#include <ossia/detail/small_vector.hpp>

namespace ossia { class math_expression; }

namespace score::gfx
{

struct RenderedCSFNode : score::gfx::NodeRenderer
{
  explicit RenderedCSFNode(const ISFNode& node) noexcept;

  virtual ~RenderedCSFNode();

  void updateInputTexture(const Port& input, QRhiTexture* tex, QRhiTexture* depthTex = nullptr) override;
  QRhiTexture* textureForOutput(const Port& output) override;

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  void release(RenderList& r) override;

  void initState(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void releaseState(RenderList& renderer) override;
  void addOutputPass(
      RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override;
  void removeOutputPass(RenderList& renderer, Edge& edge) override;
  bool hasOutputPassForEdge(Edge& edge) const override;
  void
  addInputEdge(RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res) override;
  void removeInputEdge(RenderList& renderer, Edge& edge) override;

  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

private:
  void initComputeSRBAndPasses(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void createComputePipeline(RenderList& renderer);
  void createGraphicsPass(const TextureRenderTarget& rt, RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res);
  void updateDescriptorSet(RenderList& renderer, Edge& edge);
  std::vector<Sampler> allSamplers() const noexcept;

  // Expression evaluation helper
  void registerCommonExpressionVariables(
      ossia::math_expression& e, ossia::small_pod_vector<double, 16>& data) const;

  // Image management
  std::optional<QSize> getImageSize(const isf::csf_image_input&) const noexcept;
  QSize computeTextureSize(const isf::csf_image_input& img) const noexcept;

  // Buffer management methods
  int calculateStorageBufferSize(std::span<const isf::storage_input::layout_field> layout, int arrayCount) const;
  BufferView createStorageBuffer(
      RenderList& renderer, const QString& name, const QString& access, int size);
  void updateStorageBuffers(RenderList& renderer, QRhiResourceUpdateBatch& res);
  void recreateShaderResourceBindings(RenderList& renderer, QRhiResourceUpdateBatch& res);

  // Single source of truth for the CSF compute SRB binding list. Walks the
  // descriptor's INPUTS / RESOURCES / AUXILIARIES in order and emits one
  // QRhiShaderResourceBinding per shader binding slot. Both
  // initComputeSRBAndPasses (init path) and recreateShaderResourceBindings
  // (re-emit path) call this so the two paths can never drift in their
  // emission order, indices, or fallback-on-missing-resource policy.
  // Binding 1 (ProcessUBO) is left as a nullptr placeholder; each caller
  // patches it per-pass. Output: appended to `bindings`.
  void buildComputeSrbBindings(
      RenderList& renderer, QRhiResourceUpdateBatch& res,
      QList<QRhiShaderResourceBinding>& bindings);
  int getArraySizeFromUI(const QString& bufferName) const;
  QString updateShaderWithImageFormats(QString current);

  // Geometry buffer management
  void updateGeometryBindings(RenderList& renderer, QRhiResourceUpdateBatch& res);

  void pushOutputGeometry(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge& edge);
  int resolveCountExpression(
      const std::string& expr, const isf::geometry_input& geo,
      const std::string& fieldName) const;
  int resolveDispatchExpression(const std::string& expr) const;

  BufferView bufferForOutput(const score::gfx::Port& output) override;

  struct ComputePass
  {
    QRhiComputePipeline* pipeline{};
    QRhiShaderResourceBindings* srb{};
    QRhiBuffer* processUBO{};
    // Hash of the last bindings vector applied to `srb`. Compared in
    // recreateShaderResourceBindings to skip a destroy+setBindings+
    // create cycle when the bindings haven't actually changed since the
    // previous frame. 0 = "never built / unknown" — first call always
    // rebuilds. See RenderedCSFNode.cpp recreateShaderResourceBindings.
    size_t srbBindingsHash{0};
  };

  struct GraphicsPass
  {
    Pipeline pipeline;
    QRhiSampler* outputSampler{};
    MeshBuffers meshBuffers;
  };

  ossia::small_vector<std::pair<Edge*, ComputePass>, 2> m_computePasses;
  ossia::small_vector<std::pair<Edge*, GraphicsPass>, 2> m_graphicsPasses;

  ISFNode& n;

  std::vector<Sampler> m_inputSamplers;

  // Storage buffers for compute shaders
  struct StorageBuffer
  {
    QRhiBuffer* buffer{};
    int64_t size{};
    int64_t lastKnownSize{}; // For dynamic resizing
    QString name;
    QString access; // "read_only", "write_only", "read_write"
    std::vector<isf::storage_input::layout_field> layout; // For size calculation
    bool owned{true}; // false when buffer comes from geometry auxiliary
    std::string buffer_usage; // "", "indirect_draw", "indirect_draw_indexed"
  };
  std::vector<StorageBuffer> m_storageBuffers; // Contains both ins and outs

  // Only outs, matched with index in m_storageBuffers
  std::vector<std::pair<const score::gfx::Port*, int>> m_outStorageBuffers;

  // Storage images for compute shaders
  struct StorageImage
  {
    QRhiTexture* texture{};
    QRhiTexture* read_texture{}; //!< Previous-frame slot, only when persistent
    QString name;
    QString access; // "read_only", "write_only", "read_write"
    QRhiTexture::Format format{QRhiTexture::RGBA8};
    bool is3D{false};
    bool isCube{false};           //!< Writable cubemap (imageCube)
    bool persistent{false};       //!< Ping-pong this image across frames
    bool pending_initial_copy{false}; //!< First frame: _prev reads from `texture` too
    bool generate_mips{false};    //!< Run QRhi::generateMips after compute passes

    // Recorded binding slots in the compute SRB so that end-of-frame
    // swapping can call replaceTexture() without having to re-walk the
    // descriptor layout.
    int binding{-1};
    int prev_binding{-1};
  };
  std::vector<StorageImage> m_storageImages;

  // Only outs, matched with index in m_storageImages
  std::vector<std::pair<const score::gfx::Port*, int>> m_outStorageImages;

  // Geometry input bindings: SoA SSBOs created from incoming ossia::geometry
  struct GeometryBinding
  {
    // One SSBO per declared attribute in the geometry_input
    struct AttributeSSBO
    {
      QRhiBuffer* buffer{};       // GPU SSBO for this attribute (write target / primary)
      QRhiBuffer* read_buffer{};  // Separate read buffer for ping-pong (nullptr = use buffer for both)
      int64_t size{};             // Current buffer size in bytes
      bool owned{true};           // true = we created it; false = referencing upstream gpu_buffer
      std::string name;           // e.g. "position", "velocity"
      std::string access;         // "read_only", "write_only", "read_write"
      bool per_instance{false};   // true = sized by instance_count, false = sized by vertex_count
      const void* lastUploadSrc{};// CPU data pointer from last upload (for dedup)

      // GPU scatter state (used when format conversion is needed)
      QRhiBuffer* scatterStaging{};      // Staging SSBO for raw CPU data
      int64_t scatterStagingSize{};
      GPUBufferScatter::PreparedOp scatterOp;
      GPUBufferScatter::Params scatterParams;
      bool scatterPending{false};        // true = needs dispatch this frame
    };

    // Structured SSBOs (or UBOs) that travel with the geometry (matched
    // by name against ossia::geometry::auxiliary_buffer entries). The
    // `is_uniform` flag mirrors the AUXILIARY request's kind: when true,
    // the buffer is bound as a std140 uniform block via
    // QRhiShaderResourceBinding::uniformBuffer; when false, as an std430
    // SSBO via bufferLoad / bufferStore / bufferLoadStore.
    struct AuxiliarySSBO
    {
      QRhiBuffer* buffer{};       // GPU SSBO/UBO (write target / primary)
      QRhiBuffer* read_buffer{};  // Separate read buffer for ping-pong (nullptr = use buffer for both)
      int64_t size{};
      bool owned{true};
      bool is_uniform{false};     // true = std140 UBO, false = std430 SSBO
      std::string name;
      std::string access;
      std::vector<isf::storage_input::layout_field> layout;
      std::string size_expr; // expression for flexible array count, may contain $USER
    };

    // Auxiliary textures that travel with the geometry (resolved from
    // ossia::geometry::auxiliary_textures by name). Either sampled
    // (sampler*) or storage-image (image*). Shape-matched placeholder
    // used as fallback when no match exists on the incoming geometry.
    struct AuxiliaryTexture
    {
      QRhiSampler* sampler{};   // null for storage-image entries
      QRhiTexture* texture{};   // current bound handle (placeholder or upstream)
      QRhiTexture* placeholder{}; // shape-matched empty from RenderList
      std::string name;
      int binding{-1};          // assigned at SRB build
      bool is_storage{false};
      std::string access;       // "read_only" / "write_only" / "read_write"

      // True when this binding allocated `texture` itself (write_only /
      // read_write storage image declared as a nested aux on a geometry
      // input — same lifecycle role as m_storageImages plays for top-
      // level csf_image_input outputs). Owned textures:
      //   - skip the per-frame upstream-resolution overwrite (we own
      //     the data, no upstream contributes);
      //   - get pushed into out_geo.auxiliary_textures by name so
      //     downstream consumers can resolve the live handle;
      //   - get deleted on release().
      bool owned{false};
    };

    std::vector<AttributeSSBO> attribute_ssbos;
    std::vector<AuxiliarySSBO> auxiliary_ssbos;
    std::vector<AuxiliaryTexture> auxiliary_textures;
    std::string input_name;    // RESOURCES[].NAME (e.g. "geoIn", "geoOut") — used by PER_VERTEX/PER_INSTANCE TARGET filtering
    int vertex_count{0};       // Number of elements (vertices) in the geometry
    int instance_count{1};      // Number of instances
    int input_port_index{-1};   // Input port index for this binding (-1 = no input port, e.g. write_only generator)
    bool has_output{false};     // true if any attribute is writable
    bool has_vertex_count_spec{false};   // true if vertex_count expression is set
    bool has_instance_count_spec{false}; // true if instance_count expression is set
    bool is_feedback_receiver{false};    // true = uses ping-pong double buffering for read_write attrs
    bool pending_initial_copy{false};    // first frame after read_buffer allocated: use same-buffer mode, then copy buffer→read_buffer

    // GPU buffers allocated by COPY_FROM (CPU→GPU upload). Owned by this binding,
    // must be released via renderer.releaseBuffer() since they escape into output geometry.
    std::vector<QRhiBuffer*> copyFromBuffers;

    // Persistent output geometry — reused across frames to avoid per-frame shared_ptr allocation.
    // Updated in-place; dirty_index incremented when structure or handles change.
    ossia::geometry_spec outputGeometry;
    int prev_vertex_count{-1};   // Track structural changes
    int prev_instance_count{-1};
    int prev_attribute_count{-1};
    int prev_upstream_attr_count{-1};

    QRhiBuffer* indirectBuffer{};       // StorageBuffer (+ IndirectBuffer on Qt 6.12+)
    int64_t indirectBufferSize{};
    int indirectCountResult{0};         // Resolved command count
    std::string indirectCountExpr;      // Expression string for dynamic re-resolve
    bool uses_indirect_draw{false};
  };
  std::vector<GeometryBinding> m_geometryBindings;

  QRhiBuffer* m_materialUBO{};
  int m_materialSize{};

  // Output texture for compute shader results
  QRhiTexture* m_outputTexture{};
  QRhiTexture::Format m_outputFormat{QRhiTexture::RGBA8};

  // Compute shader specifics
  QRhiComputePipeline* m_computePipeline{}; // Points to first pass pipeline (backward compat)
  QShader m_computeShader;
  QString m_computeShaderSource; // Template with ISF_LOCAL_SIZE_X/Y/Z placeholders
  std::vector<QRhiComputePipeline*> m_perPassPipelines; // One entry per pass (may share pipelines)
  std::vector<QRhiComputePipeline*> m_ownedPipelines;   // Unique pipelines for cleanup
  bool m_pipelinesDirty{true};

  // GPU buffer scatter (format conversion on GPU)
  GPUBufferScatter m_gpuScatter;
  bool m_gpuScatterAvailable{false};

  // True once at least one frame's worth of upstream rendering has happened
  // for this renderer's input textures. Used to gate generateMips() so we
  // don't trip a Vulkan validation error on freshly-allocated textures whose
  // layout is still PREINITIALIZED. Reset on init() / after release() so a
  // RenderList rebuild starts the cycle over.
  bool m_inputsHaveBeenWritten{false};
};

}
