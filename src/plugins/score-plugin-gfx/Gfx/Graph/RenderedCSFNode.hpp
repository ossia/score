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

  void updateInputTexture(const Port& input, QRhiTexture* tex) override;
  QRhiTexture* textureForOutput(const Port& output) override;

  void init(RenderList& renderer, QRhiResourceUpdateBatch& res) override;
  void update(RenderList& renderer, QRhiResourceUpdateBatch& res, Edge* edge) override;
  void release(RenderList& r) override;

  void runInitialPasses(
      RenderList&, QRhiCommandBuffer& commands, QRhiResourceUpdateBatch*& res,
      Edge& edge) override;

  void runRenderPass(RenderList&, QRhiCommandBuffer& commands, Edge& edge) override;

private:
  void initComputePass(const TextureRenderTarget& rt, RenderList& renderer, Edge& edge, QRhiResourceUpdateBatch& res);
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
    QString name;
    QString access; // "read_only", "write_only", "read_write"
    QRhiTexture::Format format{QRhiTexture::RGBA8};
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

    // Structured SSBOs that travel with the geometry (matched by name
    // against ossia::geometry::auxiliary_buffer entries).
    struct AuxiliarySSBO
    {
      QRhiBuffer* buffer{};
      int64_t size{};
      bool owned{true};
      std::string name;
      std::string access;
      std::vector<isf::storage_input::layout_field> layout;
      std::string size_expr; // expression for flexible array count, may contain $USER
    };

    std::vector<AttributeSSBO> attribute_ssbos;
    std::vector<AuxiliarySSBO> auxiliary_ssbos;
    int vertex_count{0};       // Number of elements (vertices) in the geometry
    int instance_count{1};      // Number of instances
    int input_port_index{-1};   // Input port index for this binding (-1 = no input port, e.g. write_only generator)
    bool has_output{false};     // true if any attribute is writable
    bool has_vertex_count_spec{false};   // true if vertex_count expression is set
    bool has_instance_count_spec{false}; // true if instance_count expression is set
    bool is_feedback_receiver{false};    // true = uses ping-pong double buffering for read_write attrs
    bool pending_initial_copy{false};    // first frame after read_buffer allocated: use same-buffer mode, then copy buffer→read_buffer

    // Persistent output geometry — reused across frames to avoid per-frame shared_ptr allocation.
    // Updated in-place; dirty_index incremented when structure or handles change.
    ossia::geometry_spec outputGeometry;
    int prev_vertex_count{-1};   // Track structural changes
    int prev_instance_count{-1};
    int prev_attribute_count{-1};
    int prev_upstream_attr_count{-1};

#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
    QRhiBuffer* indirectDrawBuffer{};   // StorageBuffer | IndirectBuffer for GPU-driven draw args
    bool uses_indirect_draw{false};     // true when geometry_input has INDIRECT_DRAW: true
    bool indirect_draw_indexed{false};  // true for drawIndexedIndirect, false for drawIndirect
#endif
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
  std::vector<QRhiComputePipeline*> m_perPassPipelines; // One pipeline per pass (different local_size)
  bool m_pipelinesDirty{true};

  // GPU buffer scatter (format conversion on GPU)
  GPUBufferScatter m_gpuScatter;
  bool m_gpuScatterAvailable{false};
};

}
