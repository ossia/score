#include <score/gfx/OpenGL.hpp>

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QRegularExpression>

#if defined(__linux__)
#include <dlfcn.h>
#include <link.h>
#endif

namespace score
{
static QString firstDottedVersion(const QString& text) noexcept
{
  // Two or three dot-separated numbers, as NVIDIA numbers its releases
  // (550.54.14, 610.57.04, and the occasional two-component 470.57).
  static const QRegularExpression re{
      QStringLiteral("(\\d+\\.\\d+(?:\\.\\d+)?)")};
  const auto m = re.match(text);
  return m.hasMatch() ? m.captured(1) : QString{};
}

QString nvidiaKernelDriverVersion(const QString& procVersionText) noexcept
{
  for(const auto& line : procVersionText.split(QLatin1Char('\n')))
  {
    if(!line.contains(QLatin1String("NVRM"), Qt::CaseInsensitive))
      continue;
    // The architecture can itself contain digits ("x86_64"), but never a dotted
    // number, so the first dotted number on the line is the release.
    if(auto v = firstDottedVersion(line); !v.isEmpty())
      return v;
  }
  return {};
}

QString nvidiaGlxLibraryVersion(const QString& libraryPath) noexcept
{
  const auto name = QFileInfo{libraryPath}.fileName();
  static const QLatin1String marker{".so."};
  const auto idx = name.indexOf(marker);
  if(idx < 0)
    return {};
  const auto suffix = name.mid(idx + marker.size());
  // ".so.0" is the stable soname, not a release.
  return suffix.contains(QLatin1Char('.')) ? firstDottedVersion(suffix) : QString{};
}

NvidiaGlxVendorState nvidiaGlxVendorState(
    const QString& kernelVersion, const QString& libraryVersion) noexcept
{
  if(kernelVersion.isEmpty())
    return NvidiaGlxVendorState::NoDriver;
  if(libraryVersion.isEmpty())
    return NvidiaGlxVendorState::Unknown;
  return kernelVersion == libraryVersion ? NvidiaGlxVendorState::Consistent
                                         : NvidiaGlxVendorState::VersionMismatch;
}

NvidiaGlxVendorState nvidiaGlxVendorState() noexcept
{
#if defined(__linux__)
  QString kernelVersion;
  {
    QFile f{QStringLiteral("/proc/driver/nvidia/version")};
    if(!f.open(QIODevice::ReadOnly | QIODevice::Text))
      return NvidiaGlxVendorState::NoDriver;
    kernelVersion
        = nvidiaKernelDriverVersion(QString::fromUtf8(f.readAll()));
  }
  if(kernelVersion.isEmpty())
    return NvidiaGlxVendorState::NoDriver;

  // Ask the dynamic loader where libGLX_nvidia.so.0 actually is, rather than
  // guessing at /usr/lib vs /usr/lib64 vs a multiarch triplet: this is the very
  // file libglvnd dispatches to once __GLX_VENDOR_LIBRARY_NAME says "nvidia".
  // RTLD_LOCAL and an immediate dlclose because nothing here wants its symbols;
  // loading it runs no NVIDIA initialisation that touches the kernel module.
  void* lib = dlopen("libGLX_nvidia.so.0", RTLD_LAZY | RTLD_LOCAL);
  if(!lib)
    return NvidiaGlxVendorState::NotInstalled;

  QString libraryPath;
  if(struct link_map * lm{}; dlinfo(lib, RTLD_DI_LINKMAP, &lm) == 0 && lm && lm->l_name)
    libraryPath = QFileInfo{QString::fromUtf8(lm->l_name)}.canonicalFilePath();
  dlclose(lib);

  return nvidiaGlxVendorState(kernelVersion, nvidiaGlxLibraryVersion(libraryPath));
#else
  return NvidiaGlxVendorState::NoDriver;
#endif
}


namespace
{
struct GLCapabilitiesResult
{
  int major{};
  int minor{};
  int shaderVersion{};
  QSurfaceFormat::RenderableType type{};

  GLCapabilitiesResult()
  {
#ifndef QT_NO_OPENGL
    QOffscreenSurface surf;
    surf.create();

    QOpenGLContext ctx;
    auto fmt = ctx.format();
    auto requested_format = qEnvironmentVariable("SCORE_OPENGL_FORMAT").toLower().trimmed();
    if(requested_format.endsWith("gles"))
    {
      fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    }
    else if(requested_format.endsWith("gl"))
    {
      fmt.setRenderableType(QSurfaceFormat::OpenGL);
      fmt.setProfile(QSurfaceFormat::CoreProfile);
    }
    else
    {
#if (defined(__arm__) || defined(__aarch64__)) && !defined(_WIN32) && !defined(__APPLE__)
      fmt.setRenderableType(QSurfaceFormat::OpenGLES);
#else
      fmt.setRenderableType(QSurfaceFormat::OpenGL);
      fmt.setProfile(QSurfaceFormat::CoreProfile);
#endif
    }

    ctx.setFormat(fmt);

    // Every step here can fail -- a headless session, a software rasteriser
    // that cannot make a drawable, a driver that refuses the requested
    // profile. Reading the format back and calling GL entry points anyway
    // means querying an unusable context, so keep the format we asked for
    // rather than what an uncreated context reports.
    const bool surfaceOk = surf.isValid();
    const bool contextOk = surfaceOk && ctx.create() && ctx.makeCurrent(&surf);
    if(!contextOk)
    {
      major = fmt.majorVersion();
      minor = fmt.minorVersion();
      type = fmt.renderableType();
      shaderVersion = glShaderVersion();
      qWarning() << "score: no usable OpenGL context for capability probing"
                 << "(surface" << surfaceOk << "); keeping the requested format"
                 << major << minor << shaderVersion;
    }
    else
    {
      major = ctx.format().majorVersion();
      minor = ctx.format().minorVersion();
      type = ctx.format().renderableType();
      shaderVersion = glShaderVersion();
      ctx.doneCurrent();
    }
#endif
    qDebug() << "Available GL context: " << major << minor << shaderVersion << type;
  }

  int glShaderVersion() noexcept
  {
    switch(type)
    {
      case QSurfaceFormat::OpenGLES: {
        if(major >= 3)
        {
          return major * 100 + minor * 10;
        }
        else
        {
          return 100;
        }
      }
      case QSurfaceFormat::OpenGL: {
        if(major > 3 || (major == 3 && minor >= 3))
        {
          return major * 100 + minor * 10;
        }
        else if(major == 3)
        {
          switch(minor)
          {
            case 2:
              return 150;
            case 1:
              return 120; // Technically 140 but Rhi looks for 120
            case 0:
              return 120; // Technically 140 but Rhi looks for 120
          }
        }
        else if(major == 2)
        {
          switch(minor)
          {
            case 1:
              return 120;
            case 0:
              return 110;
          }
        }
        else
        {
          return 120;
        }
      }
      default: {
        return major * 100 + minor * 10;
      }
    }
  }
};
}

void setupDefaultOpenGLFormat() noexcept
{
#ifndef QT_NO_OPENGL
  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
#if(defined(__arm__) || defined(__aarch64__)) && !defined(_WIN32) && !defined(__APPLE__)
  fmt.setRenderableType(QSurfaceFormat::OpenGLES);
  fmt.setSwapInterval(1);
  fmt.setVersion(3, 2);
  QSurfaceFormat::setDefaultFormat(fmt);
#elif defined(__APPLE__)
  // Apple ships exactly two GL profiles and Qt's default is the wrong one:
  //   legacy   -> "2.1 Metal - 90.5", GLSL 1.20
  //   4.1 core -> "4.1 Metal - 90.5", GLSL 4.10
  // A core profile below 3.2 does not exist there: ask for one and the legacy
  // context comes back.
  fmt.setRenderableType(QSurfaceFormat::OpenGL);
  fmt.setProfile(QSurfaceFormat::CoreProfile);
  fmt.setSwapInterval(1);
  fmt.setVersion(4, 1);
  QSurfaceFormat::setDefaultFormat(fmt);
#endif
#endif
}

GLCapabilities::GLCapabilities()
{
#ifndef QT_NO_OPENGL
  static const GLCapabilitiesResult res;
  major = res.major;
  minor = res.minor;
  shaderVersion = res.shaderVersion;
  type = res.type;

#if __has_include(<private/qshader_p.h>)
  qShaderVersion.setVersion(shaderVersion);

  if(type == QSurfaceFormat::OpenGLES)
    qShaderVersion.setFlags(QShaderVersion::GlslEs);
#endif

#endif
}

void GLCapabilities::setupFormat(QSurfaceFormat& fmt)
{
  fmt.setMajorVersion(major);
  fmt.setMinorVersion(minor);

  if(type == QSurfaceFormat::OpenGLES)
  {
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
  }
  else if(type == QSurfaceFormat::OpenGL)
  {
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
  }
  else
  {
#if (defined(__arm__) || defined(__aarch64__)) && !defined(_WIN32) && !defined(__APPLE__)
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
#else
    fmt.setRenderableType(QSurfaceFormat::OpenGL);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
#endif
  }
}
}
