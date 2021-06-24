#include <score/gfx/OpenGL.hpp>

#include <QDebug>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>

namespace score
{
GLCapabilities::GLCapabilities()
{
#ifndef QT_NO_OPENGL
  QOffscreenSurface surf;
  surf.create();

  QOpenGLContext ctx;
  ctx.create();
  ctx.makeCurrent(&surf);

  major = ctx.format().majorVersion();
  minor = ctx.format().minorVersion();
  type = ctx.format().renderableType();
  shaderVersion = glShaderVersion();

#endif
  qDebug() << "Available GL context: " << major << minor << shaderVersion
           << type;
}

int GLCapabilities::glShaderVersion() noexcept
{
  switch (type)
  {
    case QSurfaceFormat::OpenGLES:
    {
      if (major >= 3)
      {
        return major * 100 + minor * 10;
      }
      else
      {
        return 100;
      }
    }
    case QSurfaceFormat::OpenGL:
    {
      if (major >= 3 && minor >= 3)
      {
        return major * 100 + minor * 10;
      }
      else if (major == 3)
      {
        switch (minor)
        {
          case 2:
            return 150;
          case 1:
            return 120; // Technically 140 but Rhi looks for 120
          case 0:
            return 120; // Technically 140 but Rhi looks for 120
        }
      }
      else if (major == 2)
      {
        switch (minor)
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
    default:
    {
      return major * 100 + minor * 10;
    }
  }
}
}
