#include <Gfx/Settings/Model.hpp>

#include <score/gfx/Vulkan.hpp>

#include <wobjectimpl.h>

extern "C" {
#include <libavcodec/codec.h>
}
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
#elif QT_HAS_VULKAN
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

SETTINGS_PARAMETER_IMPL(GraphicsApi){
    QStringLiteral("score_plugin_gfx/GraphicsApi"), GraphicsApis{}.OpenGL};

SETTINGS_PARAMETER_IMPL(HardwareDecode){
    QStringLiteral("score_plugin_gfx/HardwareDecode"), "None"};
SETTINGS_PARAMETER_IMPL(Rate){QStringLiteral("score_plugin_gfx/Rate"), 60.0};
SETTINGS_PARAMETER_IMPL(Samples){QStringLiteral("score_plugin_gfx/Samples"), 1};
SETTINGS_PARAMETER_IMPL(DecodingThreads){
    QStringLiteral("score_plugin_gfx/DecodingThreads"), 2};
SETTINGS_PARAMETER_IMPL(VSync){QStringLiteral("score_plugin_gfx/VSync"), true};

static auto list()
{
  return std::tie(GraphicsApi, HardwareDecode, DecodingThreads, Samples, Rate, VSync);
}
}

Gfx::Settings::GraphicsApis::operator QStringList() const noexcept
{
  QStringList lst;
#ifndef QT_NO_OPENGL
  lst += OpenGL;
#endif

#if QT_HAS_VULKAN
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

Gfx::Settings::HardwareVideoDecoder::operator QStringList() const noexcept
{
  QStringList lst;
  lst += None;

  if(avcodec_find_decoder_by_name("mjpeg_qsv")
     || avcodec_find_decoder_by_name("h264_qsv"))
    lst += QSV;

  if(avcodec_find_decoder_by_name("mjpeg_cuvid")
     || avcodec_find_decoder_by_name("h264_cuvid"))
    lst += CUDA;

#if defined(__APPLE__)
  lst += VideoToolbox;
#endif

#if defined(__linux__)
  if(auto c = avcodec_find_decoder_by_name("h264_vdpau"))
    lst += VDPAU;

  if(auto c = avcodec_find_decoder_by_name("mjpeg_vaapi"))
    lst += VAAPI;
#endif

#if defined(_WIN32)
  if(auto c = avcodec_find_decoder_by_name("h264_dxva2"))
    lst += DXVA;

  if(auto c = avcodec_find_decoder_by_name("h264_d3d11va2"))
    lst += D3D;
#endif
  return lst;
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

score::gfx::GraphicsApi Model::graphicsApiEnum() const noexcept
{
  const auto apis = GraphicsApis{};
  if(m_GraphicsApi == apis.Vulkan)
  {
    return score::gfx::Vulkan;
  }
  else if(m_GraphicsApi == apis.Metal)
  {
    return score::gfx::Metal;
  }
  else if(m_GraphicsApi == apis.D3D11)
  {
    return score::gfx::D3D11;
  }
  else
  {
    return score::gfx::OpenGL;
  }
}

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, GraphicsApi)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, HardwareDecode)
SCORE_SETTINGS_PARAMETER_CPP(double, Model, Rate)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, Samples)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, DecodingThreads)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, VSync)

}
