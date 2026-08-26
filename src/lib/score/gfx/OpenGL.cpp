#include <score/gfx/OpenGL.hpp>

#include <QDebug>
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
    ctx.create();
    ctx.makeCurrent(&surf);

    major = ctx.format().majorVersion();
    minor = ctx.format().minorVersion();
    type = ctx.format().renderableType();
    shaderVersion = glShaderVersion();
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
