#include <score/gfx/OpenGL.hpp>

#include <QDebug>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

namespace score
{
namespace
{
struct GLCapabilitiesResult
{
  int major{};
  int minor{};
  int shaderVersion{};
  QSurfaceFormat::RenderableType type{};

  // Only pinDefaultOpenGLFormat() cares: a probe that could not make a context
  // reports the format it asked for, which is nothing to pin on.
  bool usableContext{};

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
      usableContext = true;
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

#ifndef QT_NO_OPENGL
const GLCapabilitiesResult& glCapabilities()
{
  static const GLCapabilitiesResult res;
  return res;
}
#endif
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
  const GLCapabilitiesResult& res = glCapabilities();
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

void pinDefaultOpenGLFormat() noexcept
{
#ifndef QT_NO_OPENGL
  // Same list setup_opengl() skips: no GL, or a crash in QOffscreenSurface::create.
  const auto plat = QGuiApplication::platformName();
  if(plat == "minimal" || plat == "offscreen" || plat == "vnc" || plat == "wasm")
    return;

  // Pinning a format the probe could not verify would put its fallback -- a
  // CoreProfile 2.0 -- on every window in the process.
  if(!glCapabilities().usableContext)
    return;

  QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
  GLCapabilities{}.setupFormat(fmt);
  QSurfaceFormat::setDefaultFormat(fmt);
#endif
}
}
