#pragma once
#include <score/gfx/OpenGL.hpp>
#include <Gfx/Graph/RenderState.hpp>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QShaderBaker>
#else
#include <QtShaderTools/private/qshaderbaker_p.h>
#endif

#include <unordered_map>

namespace score::gfx
{
/**
 * @brief Cache of baked QShader instances
 */
struct ShaderCache
{
public:
  /**
   * @brief Get a QShader from a source string.
   *
   * @return If there is an error message, it will be in the QString part of the pair.
   */
  static const std::pair<QShader, QString>&
  get(const RenderState& v, const QByteArray& shader, QShader::Stage stage);
  static const std::pair<QShader, QString>&
  get(GraphicsApi api, const QShaderVersion& v, const QByteArray& shader, QShader::Stage stage);

private:
  ShaderCache();

  struct Baker {
    explicit Baker(GraphicsApi api, const QShaderVersion& v);

    GraphicsApi api;
    QShaderVersion version;
    QShaderBaker baker;
    std::unordered_map<QByteArray, std::pair<QShader, QString>> shaders;
  };

  std::vector<std::unique_ptr<Baker>> m_bakers;
};
}
