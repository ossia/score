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

  virtual TextureRenderTarget renderTargetForInput(const Port& input) = 0;
  virtual BufferView bufferForInput(const Port& input);
  virtual BufferView bufferForOutput(const Port& output);

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

  void checkForChanges()
  {
    materialChanged = node.hasMaterialChanged(materialChangedIndex);
    renderTargetSpecsChanged
        = node.hasRenderTargetChanged(renderTargetSpecsChangedIndex);
  }

  // FIXME this will change when we have a proper scene node
  void process(int32_t port, const ossia::geometry_spec& v);
  virtual void process(int32_t port, const ossia::transform3d& v);

  const Node& node;

  /**
   * @brief The geometry to use
   *
   * If not set, then a relevant default geometry for the node
   * will be used, e.g. a full-screen quad or triangle
   */
  ossia::geometry_spec geometry;

  int32_t nodeId{-1};
  bool materialChanged{false};
  bool geometryChanged{false};
  bool renderTargetSpecsChanged{false};

private:
  int64_t materialChangedIndex{-1};
  int64_t renderTargetSpecsChangedIndex{-1};
};

using PassMap = ossia::small_vector<std::pair<Edge*, Pipeline>, 2>;
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

  TextureRenderTarget renderTargetForInput(const Port& p) override;
  virtual ~GenericNodeRenderer() { }

  ossia::small_vector<Sampler, 8> m_samplers;

  QShader m_vertexS;
  QShader m_fragmentS;

  // Pipeline
  PassMap m_p;

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
};

}
