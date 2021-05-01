#include <Gfx/Settings/Model.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Gfx::Settings::Model)
namespace Gfx::Settings
{
namespace Parameters
{

/* logic to restore when it works well with all backends
#if defined(Q_OS_WIN)
  return GraphicsApi::D3D11;
#elif defined(Q_OS_DARWIN)
  return GraphicsApi::Metal;
#elif QT_CONFIG(vulkan)
  const QString platformName = QGuiApplication::platformName().toLower();
  if(platformName.contains("gl") || platformName.contains("wayland") || platformName.isEmpty())
  {
    return GraphicsApi::OpenGL;
  }

  return GraphicsApi::Vulkan;
#else
  return GraphicsApi::OpenGL;
#endif
*/

SETTINGS_PARAMETER_IMPL(GraphicsApi)
{
  QStringLiteral("score_plugin_gfx/GraphicsApi"), GraphicsApis{}.OpenGL
};

SETTINGS_PARAMETER_IMPL(Rate){QStringLiteral("score_plugin_gfx/Rate"), 60.0};
SETTINGS_PARAMETER_IMPL(VSync){QStringLiteral("score_plugin_gfx/VSync"), true};

static auto list()
{
  return std::tie(GraphicsApi, Rate, VSync);
}
}

Gfx::Settings::GraphicsApis::operator QStringList() const noexcept {
  QStringList lst;
#ifndef QT_NO_OPENGL
  lst += OpenGL;
#endif

#if QT_CONFIG(vulkan)
  lst += Vulkan;
#endif

#ifdef Q_OS_WIN
  lst += D3D11;
#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
  lst += Metal;
#endif
  return lst;
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

GraphicsApi Model::graphicsApiEnum() const noexcept
{
  const auto apis = GraphicsApis{};
  if(m_GraphicsApi == apis.Vulkan)
  {
    return Vulkan;
  }
  else if(m_GraphicsApi == apis.Metal)
  {
    return Metal;
  }
  else if(m_GraphicsApi == apis.D3D11)
  {
    return D3D11;
  }
  else
  {
    return OpenGL;
  }
}

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, GraphicsApi)
SCORE_SETTINGS_PARAMETER_CPP(double, Model, Rate)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, VSync)


}