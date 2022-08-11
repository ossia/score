#pragma once
#include <ossia/detail/small_vector.hpp>
#include <ossia/detail/span.hpp>

#include <private/qrhi_p.h>

#include <score_plugin_gfx_export.h>

namespace score::gfx
{
/**
 * @brief Data model for meshes.
 */
struct SCORE_PLUGIN_GFX_EXPORT Mesh
{
  explicit Mesh();
  virtual ~Mesh();

  /** @brief Setup bindings according to the input triangle data */
  virtual void setupBindings(
      QRhiBuffer& vtxData, QRhiBuffer* idxData,
      QRhiCommandBuffer& cb) const noexcept = 0;

  /** @brief A basic vertex shader that is going to work with this mesh. */
  virtual const char* defaultVertexShader() const noexcept = 0;

  ossia::small_vector<QRhiVertexInputBinding, 2> vertexInputBindings;
  ossia::small_vector<QRhiVertexInputAttribute, 2> vertexAttributeBindings;

  tcb::span<const float> vertexArray;
  tcb::span<const unsigned int> indexArray;
  int vertexCount{};
  int indexCount{};

private:
  Mesh(const Mesh&) = delete;
  Mesh(Mesh&&) = delete;
  Mesh& operator=(const Mesh&) = delete;
  Mesh& operator=(Mesh&&) = delete;
};

/**
 * @brief A mesh with only position attributes.
 */
struct SCORE_PLUGIN_GFX_EXPORT PlainMesh : Mesh
{
  explicit PlainMesh(tcb::span<const float> vtx, int count);

  void setupBindings(QRhiBuffer& vtxData, QRhiBuffer* idxData, QRhiCommandBuffer& cb)
      const noexcept override;

  const char* defaultVertexShader() const noexcept override;
};

/**
 * @brief A mesh with positions and texture coordinates.
 */
struct SCORE_PLUGIN_GFX_EXPORT TexturedMesh : Mesh
{
  explicit TexturedMesh(tcb::span<const float> vtx, int count);

  const char* defaultVertexShader() const noexcept override;
};

/**
 * @brief A mesh with positions, texture coordinates, and normals.
 */
struct SCORE_PLUGIN_GFX_EXPORT TextureNormalMesh : Mesh
{
  explicit TextureNormalMesh(
      tcb::span<const float> vtx, tcb::span<const unsigned int> idx, int count);

  void setupBindings(QRhiBuffer& vtxData, QRhiBuffer* idxData, QRhiCommandBuffer& cb)
      const noexcept override;

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

  void setupBindings(QRhiBuffer& vtxData, QRhiBuffer* idxData, QRhiCommandBuffer& cb)
      const noexcept override;
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

  void setupBindings(QRhiBuffer& vtxData, QRhiBuffer* idxData, QRhiCommandBuffer& cb)
      const noexcept override;
};

struct MeshBuffers
{
  QRhiBuffer* mesh{};
  QRhiBuffer* index{};
};
}
