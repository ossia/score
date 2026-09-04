#pragma once
#include <Gfx/Graph/RenderState.hpp>

#include <score/gfx/OpenGL.hpp>
#include <score/tools/std/StringHash.hpp>

#include <ossia/detail/hash_map.hpp>

#if __has_include(<QtShaderTools/rhi/qshaderbaker.h>)
#include <QtShaderTools/rhi/qshaderbaker.h>
#else
#include <QtShaderTools/private/qshaderbaker_p.h>
#endif

namespace score::gfx
{

/**
 * @brief The identifier a shader declares that collides with an HLSL intrinsic,
 *        or empty if there is none.
 *
 * SPIRV-Cross rewrites GLSL builtins to their HLSL spellings but leaves the
 * user's variable names alone, so `float frac` becomes `float frac = frac(...)`
 * and only Direct3D rejects it. Checked before baking for D3D so the author is
 * told which name to change, instead of getting an fxc error about a line that
 * looks correct.
 */
SCORE_PLUGIN_GFX_EXPORT
QByteArray hlslIntrinsicCollision(const QByteArray& src) noexcept;

/**
 * @brief Cache of baked QShader instances
 */
struct SCORE_PLUGIN_GFX_EXPORT ShaderCache
{
public:
  /**
   * @brief Get a QShader from a source string.
   *
   * @return If there is an error message, it will be in the QString part of the pair.
   */
  static const std::pair<QShader, QString>& get(
      const RenderState& v, const QByteArray& shader, QShader::Stage stage,
      int multiViewCount = 0);
  static const std::pair<QShader, QString>& get(
      GraphicsApi api, const QShaderVersion& v, const QByteArray& shader,
      QShader::Stage stage, int multiViewCount = 0);

private:
  ShaderCache();

  struct Baker
  {
    explicit Baker(GraphicsApi api, const QShaderVersion& v, int multiViewCount);

    GraphicsApi api;
    QShaderVersion version;
    int multiViewCount{};
    QShaderBaker baker;
    ossia::hash_map<QByteArray, std::pair<QShader, QString>> shaders;
  };

  std::vector<std::unique_ptr<Baker>> m_bakers;
};
}
