#pragma once
#include <Process/ProcessFlags.hpp>

#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/span.hpp>

#include <private/qrhi_p.h>

#include <score_plugin_gfx_export.h>

namespace score::gfx
{
struct MeshBuffers
{
  QRhiBuffer* mesh{};
  QRhiBuffer* index{};
};
/**
 * @brief Data model for meshes.
 */
struct SCORE_PLUGIN_GFX_EXPORT Mesh
{
public:
  explicit Mesh();
  virtual ~Mesh();

  enum Flag
  {
    HasPosition = SCORE_FLAG(1),
    HasTexCoord = SCORE_FLAG(2),
    HasColor = SCORE_FLAG(3),
    HasNormals = SCORE_FLAG(4),
    HasTangents = SCORE_FLAG(5),
  };
  using Flags = QFlags<Flag>;

  [[nodiscard]] virtual Flags flags() const noexcept = 0;

  [[nodiscard]] virtual MeshBuffers init(QRhi& rhi) const noexcept = 0;

  virtual void update(MeshBuffers& bufs, QRhiResourceUpdateBatch& cb) const noexcept = 0;
  virtual void preparePipeline(QRhiGraphicsPipeline& pip) const noexcept = 0;
  virtual void draw(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept = 0;

  /** @brief A basic vertex shader that is going to work with this mesh. */
  virtual const char* defaultVertexShader() const noexcept = 0;

protected:
  /*
*/
private:
  Mesh(const Mesh&) = delete;
  Mesh(Mesh&&) = delete;
  Mesh& operator=(const Mesh&) = delete;
  Mesh& operator=(Mesh&&) = delete;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Mesh::Flags);

struct SCORE_PLUGIN_GFX_EXPORT BasicMesh : Mesh
{
  using Mesh::Mesh;
  [[nodiscard]] virtual MeshBuffers init(QRhi& rhi) const noexcept override;
  void update(MeshBuffers& bufs, QRhiResourceUpdateBatch& cb) const noexcept override;
  void preparePipeline(QRhiGraphicsPipeline& pip) const noexcept override;
  void draw(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;
  virtual void
  setupBindings(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept = 0;

  using pip = QRhiGraphicsPipeline;
  pip::Topology topology = pip::Topology::TriangleStrip;
  pip::CullMode cullMode = pip::CullMode::None;
  pip::FrontFace frontFace = pip::FrontFace::CW;

  ossia::small_vector<QRhiVertexInputBinding, 2> vertexBindings;
  ossia::small_vector<QRhiVertexInputAttribute, 2> vertexAttributes;

  tcb::span<const float> vertexArray;
  int vertexCount{};
};

/**
 * @brief A mesh with only position attributes.
 */
struct SCORE_PLUGIN_GFX_EXPORT PlainMesh : BasicMesh
{
  explicit PlainMesh(tcb::span<const float> vtx, int count);
  [[nodiscard]] Flags flags() const noexcept override { return HasPosition; }
  const char* defaultVertexShader() const noexcept override;
  void
  setupBindings(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;
};

/**
 * @brief A mesh with positions and texture coordinates.
 */
struct SCORE_PLUGIN_GFX_EXPORT TexturedMesh : BasicMesh
{
  explicit TexturedMesh(tcb::span<const float> vtx, int count);
  [[nodiscard]] Flags flags() const noexcept override
  {
    return HasPosition | HasTexCoord;
  }

  const char* defaultVertexShader() const noexcept override;
};

/**
 * @brief A triangle mesh with only positions.
 */
struct SCORE_PLUGIN_GFX_EXPORT PlainTriangle final : PlainMesh
{
  static const constexpr float data[] = {-1, -1, 3, -1, -1, 3};

  explicit PlainTriangle();
  static const PlainTriangle& instance() noexcept;
};

/**
 * @brief A triangle mesh with positions and texture coordinates.
 *
 * This is the main mesh being used for rendering full-screen effects.
 */
struct SCORE_PLUGIN_GFX_EXPORT TexturedTriangle final : TexturedMesh
{
  static const constexpr float data[] = {// positions
                                         -1, -1, 3, -1, -1, 3,
                                         // tex coords
                                         0, 0, 2, 0, 0, 2};
  static const constexpr float flipped_y_data[] = {// positions
                                                   -1, -1, 3, -1, -1, 3,
                                                   // tex coords
                                                   0, 2, 2, 2, 0, 0};

  explicit TexturedTriangle(bool flipped = false);

  void
  setupBindings(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;
};

/**
 * @brief A quad mesh with positions and texture coordinates.
 *
 */
struct SCORE_PLUGIN_GFX_EXPORT TexturedQuad final : TexturedMesh
{
  static const constexpr float data[] = {// positions
                                         -1, -1, +1, -1, -1, +1, +1, +1,
                                         // tex coords
                                         0, 0, 1, 0, 0, 1, 1, 1};

  static const constexpr float flipped_y_data[] = {// positions
                                                   -1, -1, +1, -1, -1, +1, +1, +1,
                                                   // tex coords
                                                   0, 1, 1, 1, 0, 0, 1, 0};

  explicit TexturedQuad(bool flipped = false);

  void
  setupBindings(const MeshBuffers& bufs, QRhiCommandBuffer& cb) const noexcept override;
};

}
