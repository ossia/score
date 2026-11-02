#pragma once
#include <Gfx/Graph/NodeRenderer.hpp>
#include <Gfx/Graph/Utils.hpp>

#include <Threedim/ModelDisplay/ModelSceneDrawer.hpp>

#include <ossia/detail/hash_map.hpp>

#include <Threedim/GeometryViewer/SceneSummary.hpp>

#include <QMatrix4x4>

#include <array>
#include <memory>
#include <optional>
#include <vector>

namespace Gfx::GeometryViewer
{
struct GeometryViewerNode;

using SceneView = score::gfx::ModelSceneView;

/**
 * Draws every primitive of a scene with its world transform, in the shading
 * modes of the Geometry viewer, and the lights and cameras of the scene as
 * line gizmos. Part of the viewer's renderer, on the render thread.
 *
 * The scene is walked when it changes, not every frame. A mesh is uploaded
 * once, and kept for as long as the buffers of its primitive stay the same:
 * a scene whose transforms only move does not upload anything again.
 */
class SceneDrawer final : public score::gfx::ModelSceneDrawer
{
public:
  explicit SceneDrawer(const GeometryViewerNode& node);
  ~SceneDrawer() override;
  SceneDrawer(const SceneDrawer&) = delete;
  SceneDrawer& operator=(const SceneDrawer&) = delete;

  bool setScene(const ossia::scene_spec& scene) override;

  //! The scene needs more than the single geometry the renderer draws on its
  //! own: several primitives, transforms, lights or cameras
  bool active() const noexcept override { return m_active; }

  const ossia::aabb& bounds() const noexcept override { return m_desc.bounds; }

  void update(
      score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res,
      score::gfx::PassMap& passes, QRhiBuffer* processUBO, QRhiBuffer* cameraUBO,
      const SceneView& view) override;

  void draw(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb,
      score::gfx::Edge& edge) override;

  //! The gizmos and the outline of the selected object
  bool hasLines() const noexcept override { return m_lineVertices > 0 || m_wantsLines; }
  void drawLines(
      score::gfx::RenderList& renderer, QRhiCommandBuffer& cb, score::gfx::Edge& edge,
      QRhiGraphicsPipeline* lines, QRhiShaderResourceBindings* srb) override;

  void release() override;

private:
  struct MeshEntry
  {
    //! Keeps the geometry the mesh of the RenderList was made from alive
    ossia::geometry_spec spec;
    const score::gfx::Mesh* mesh{};
    score::gfx::MeshBuffers buffers;
    //! Which attributes the shader reads: normal, uv, colour
    uint8_t attributes{};
    //! The vertex layout, for sharing pipelines
    uint64_t layout{};
    //! The geometry its producer published, whose contents may change in
    //! place: acquired every frame, as the single geometry is
    bool live{};
    //! Seen in the last walk
    bool used{};
  };

  struct TextureEntry
  {
    std::shared_ptr<const ossia::texture_source> source;
    QRhiTexture* texture{};
    //! Belongs to whoever produced the scene
    bool borrowed{};
    bool used{};
    bool failed{};
  };

  struct Draw
  {
    uint64_t mesh{};
    QMatrix4x4 world;
    ossia::material_component_ptr material;
    //! Keys of its base colour and emissive textures, 0 for none
    uint64_t texture{}, emissive{};
    //! Of the bindings for these textures
    uint64_t bindings{};
  };

  struct EdgePass
  {
    score::gfx::Edge* edge{};
    QRhiRenderPassDescriptor* renderPass{};
    score::gfx::TextureRenderTarget target;
    //! By pipeline key
    ossia::hash_map<uint64_t, score::gfx::Pipeline> pipelines;
    //! By the textures they bind, see Draw::bindings
    ossia::hash_map<uint64_t, QRhiShaderResourceBindings*> bindings;
  };

  void walk();
  void publish();
  uint64_t meshFor(const ossia::mesh_primitive& prim);
  uint64_t meshFor(const ossia::geometry_spec& spec, std::size_t index);
  uint64_t textureFor(const ossia::texture_ref& ref);

  bool ensureShaders(score::gfx::RenderList& renderer, int variant);
  uint64_t pipelineKey(const MeshEntry& m) const noexcept;
  score::gfx::Pipeline makePipeline(
      score::gfx::RenderList& renderer, const MeshEntry& m, const EdgePass& pass,
      QRhiShaderResourceBindings* srb);
  QRhiShaderResourceBindings* bindingsFor(
      score::gfx::RenderList& renderer, EdgePass& pass, const Draw& draw,
      QRhiBuffer* processUBO, QRhiBuffer* cameraUBO);
  QRhiTexture* textureOf(uint64_t key) const noexcept;
  void uploadTextures(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res);
  void uploadDraws(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res, const SceneView& view);
  void uploadLines(score::gfx::RenderList& renderer, QRhiResourceUpdateBatch& res, const SceneView& view);
  void releasePasses();

  const GeometryViewerNode& m_node;

  ossia::scene_spec m_scene;
  const ossia::scene_state* m_state{};
  int64_t m_version{-1}, m_dirty{-1};
  bool m_active{};

  SceneDescription m_desc;
  uint32_t m_serial{};
  bool m_summaryPending{};
  std::vector<Draw> m_draws;
  ossia::hash_map<uint64_t, MeshEntry> m_meshes;
  ossia::hash_map<uint64_t, TextureEntry> m_textures;

  //! Shaders by variant: normal, uv, colour, points
  std::array<std::optional<std::pair<QShader, QShader>>, 16> m_shaders;
  std::array<bool, 16> m_shaderFailed{};

  ossia::small_vector<EdgePass, 2> m_passes;
  QRhiBuffer* m_drawUBO{};
  QRhiBuffer* m_lightingUBO{};
  QRhiSampler* m_sampler{};
  int64_t m_drawStride{};
  int64_t m_drawCapacity{};
  bool m_drawsDirty{true};
  //! The pipelines and bindings of the draws may be missing
  bool m_passesDirty{true};
  QMatrix4x4 m_drawsModel, m_linesModel;
  std::vector<char> m_drawData;
  int m_mode{-1};

  QRhiBuffer* m_lineBuffer{};
  int64_t m_lineCapacity{};
  int m_lineVertices{};
  bool m_wantsLines{};
  bool m_linesDirty{true};
  bool m_gizmos{};
  int64_t m_selected{-1};
  std::vector<float> m_lineData;
};
}
