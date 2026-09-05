#include <Gfx/Settings/Model.hpp>
#include <QLibrary>

#include <score/gfx/OpenGL.hpp>
#include <score/gfx/Vulkan.hpp>

#include <QGuiApplication>
#include <QQuickWindow>

#include <wobjectimpl.h>
#include <score/tools/Bind.hpp>

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
SETTINGS_PARAMETER_IMPL(Buffers){QStringLiteral("score_plugin_gfx/Buffers"), 3};

static auto list()
{
  return std::tie(
      GraphicsApi, HardwareDecode, DecodingThreads, Samples, Rate, VSync, Buffers);
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  lst += D3D12;
#endif
#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
  // https://github.com/ossia/score/issues/1807
  lst += Metal;
#endif
  return lst;
}

Gfx::Settings::HardwareVideoDecoder::operator QStringList() const noexcept
{
  QStringList lst;
  lst += Auto;
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
#if defined(__arm__) || defined(__aarch64__)
  if(auto c = avcodec_find_decoder_by_name("h264_v4l2m2m"))
    lst += V4L2;
#endif

  if(avcodec_find_decoder_by_name("h264_vdpau"))
    lst += VDPAU;

  if(avcodec_find_decoder_by_name("mjpeg_vaapi"))
    lst += VAAPI;
#endif

#if defined(_WIN32)
  lst += DXVA;
  lst += D3D;
#if LIBAVUTIL_VERSION_INT >= AV_VERSION_INT(58, 29, 100)
  lst += D3D12;
#endif
#endif

#if LIBAVUTIL_VERSION_MAJOR >= 57
  if(av_hwdevice_find_type_by_name("vulkan") != AV_HWDEVICE_TYPE_NONE)
    lst += VulkanVideo;
#endif

  return lst;
}

static void update_qtquick_graphics_api(const score::gfx::GraphicsApi& api)
{
  using enum score::gfx::GraphicsApi;
  switch(api)
  {
    case OpenGL:
      QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
      break;
    case Vulkan:
      QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);
      break;
    case Metal:
      QQuickWindow::setGraphicsApi(QSGRendererInterface::Metal);
      break;
    case D3D11:
      QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
      break;
    case D3D12:
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
      QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D12);
#else
      QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11);
#endif
      break;
    default:
      break;
  }
}

Model::Model(
    const UuidKey<score::SettingsDelegateFactory>& k, QSettings& set,
    const score::ApplicationContext& ctx)
    : score::SettingsDelegateModel{k, nullptr}
{
  score::setupDefaultSettings(set, Parameters::list(), *this);

  // Custom applications can select a decoder without depending on a
  // pre-existing user preference. Keep the regular setting as the fallback.
  if(const auto requested = qEnvironmentVariable("SCORE_VIDEO_DECODING_METHOD");
     !requested.isEmpty())
  {
    const QStringList decoders = HardwareVideoDecoder{};
    for(const auto& decoder : decoders)
    {
      if(decoder.compare(requested, Qt::CaseInsensitive) == 0)
      {
        m_HardwareDecode = decoder;
        break;
      }
    }
  }

  const auto apis = GraphicsApis{};

  const auto platform = QGuiApplication::platformName();
  if(platform == "eglfs")
    m_GraphicsApi = apis.OpenGL;
  else if(platform == "vkkhrdisplay")
    m_GraphicsApi = apis.Vulkan;

  // https://github.com/ossia/score/issues/1807
#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
  m_GraphicsApi = apis.Metal;
#endif

  qputenv("QT3D_RENDERER", "rhi");

  // Latched for the lifetime of the process, because qunsetenv() below removes
  // it after the first read.
  //
  // The application constructs exactly one Model, so it never noticed. A test
  // binary boots a MinimalGUIApplication per Catch2 case, so every Model after
  // the first saw no QSG_RHI_BACKEND and fell through to the platform default
  // -- Metal on macOS, above. A whole suite launched with
  // QSG_RHI_BACKEND=opengl therefore ran OpenGL in its FIRST CASE ONLY and
  // reported every later case as an OpenGL result. Measured on macmini-m1: one
  // binary logged "backend=OpenGL" twice and "backend=Metal" four times in a
  // single QSG_RHI_BACKEND=opengl run, and which case failed moved when the
  // case order was changed with --order lex.
  //
  // Not macOS-specific: the same holds for QSG_RHI_BACKEND=vulkan anywhere.
  static const QString requestedBackend
      = qEnvironmentVariable("QSG_RHI_BACKEND").toLower();

  if(const auto& rhi = requestedBackend; !rhi.isEmpty())
  {
    // User sets QSG_RHI_BACKEND from env: we respect it initially
    if(rhi == "opengl") {
      m_GraphicsApi = apis.OpenGL;
    } else if(rhi == "vulkan") {
      m_GraphicsApi = apis.Vulkan;
    } else if(rhi == "metal") {
      m_GraphicsApi = apis.Metal;
    } else if(rhi == "d3d11") {
      m_GraphicsApi = apis.D3D11;
    } else if(rhi == "d3d12") {
      m_GraphicsApi = apis.D3D12;
    }
  }

  // The backend is applied through QQuickWindow instead of the environment:
  // plug-ins that embed their own Qt build inherit our environment and would
  // pick up a backend their build may not support. e.g. Kontakt 8 statically
  // links a Qt without Vulkan and fails to create its RHI on QSG_RHI_BACKEND=vulkan.
  qunsetenv("QSG_RHI_BACKEND");

  ::bind(*this, Gfx::Settings::Model::p_GraphicsApi{}, this, [this] (const QString& api)
  {
    update_qtquick_graphics_api(this->graphicsApiEnum());
  });
}

int Model::resolveSamples(score::gfx::GraphicsApi api) const noexcept
{
  // Clamp the user setting against per-API minima. Hardware-level clamping
  // (vs. QRhi::supportedSampleCounts()) happens later, in createRenderState
  // once the QRhi instance exists, since the Settings model has no access
  // to a backend at this point.
  int s = m_Samples < 1 ? 1 : m_Samples;
  if(api == score::gfx::D3D12 && s < 2)
    s = 2; // D3D12 swap chains require at least 2 samples in QRhi
  return s;
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
  else if(m_GraphicsApi == apis.D3D12)
  {
    return score::gfx::D3D12;
  }
  else
  {
    return score::gfx::OpenGL;
  }
}

/**
 * @brief The HLSL version to bake for Direct3D 12, decided by whether DXC is
 *        actually loadable.
 *
 * Qt compiles shader model 6.x through DXC, which it loads from
 * dxcompiler.dll at runtime. That library is not part of Windows: it ships
 * with the DirectX Shader Compiler release, and the ossia SDK only began
 * carrying it recently. Asking for 6.x without it is not a silent
 * degradation -- Qt's search takes the first shader model it finds and stops,
 * so a 6.x blob makes it fail where 5.0 would have worked.
 *
 * Probing the DLL rather than assuming it keeps a score built against a newer
 * SDK working on a machine with an older one, and vice versa. Cached: the
 * answer cannot change within a process, and this is called per shader.
 */
static QShaderVersion d3d12ShaderVersion() noexcept
{
#if defined(_WIN32)
  static const QShaderVersion cached = [] {
    // Same name Qt passes to LoadLibrary; if this resolves, Qt's will too.
    QLibrary dxc{QStringLiteral("dxcompiler")};
    if(dxc.load())
    {
      dxc.unload();
      // 6.1 is the floor for SV_ViewID, which is what multiview needs. Higher
      // models buy nothing score currently asks for, and every step up
      // narrows the set of drivers that will accept the result.
      return QShaderVersion(61);
    }
    qDebug() << "score: dxcompiler.dll not found, baking HLSL 5.0 for D3D12. "
                "Shader model 6 features (multiview, wave intrinsics) are "
                "unavailable until the DirectX shader compiler runtime is "
                "installed alongside the application.";
    return QShaderVersion(50);
  }();
  return cached;
#else
  return QShaderVersion(50);
#endif
}

QShaderVersion shaderVersionForAPI(score::gfx::GraphicsApi api) noexcept
{
  switch(api)
  {
    case score::gfx::OpenGL:
      return score::GLCapabilities{}.qShaderVersion;

    case score::gfx::Vulkan:
      // Note: QShaderVersion still hardcoded to 100 in qrhvulkan.cpp as of qt 6.9
      return QShaderVersion(100);

    case score::gfx::Metal:
      return QShaderVersion(12);

    case score::gfx::D3D11:
      // fxc caps at 5.1 and Qt's D3D11 backend looks up exactly {HlslShader,
      // 50} (qrhid3d11.cpp), so this is the only version it can use.
      return QShaderVersion(50);

    case score::gfx::D3D12:
      // D3D12 searches shader models 6.7 down to 5.0 and takes the FIRST it
      // finds (qrhid3d12.cpp, compileHlslShaderSource). Anything at or above
      // 6.0 is compiled through DXC, which Qt loads from dxcompiler.dll at
      // runtime -- and because the search BREAKS at the first hit, baking 6.x
      // on a machine without that DLL does not fall back to 5.0, it fails.
      //
      // So the version is chosen by whether the runtime is actually there.
      // Present: 6.1, which is what SV_ViewID (multiview) needs and D3D11 can
      // never provide. Absent: 5.0, exactly as before.
      return d3d12ShaderVersion();

    default:
      return {};
  }
  return {};
}

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, GraphicsApi)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, HardwareDecode)
SCORE_SETTINGS_PARAMETER_CPP(double, Model, Rate)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, Samples)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, DecodingThreads)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, VSync)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, Buffers)
}
