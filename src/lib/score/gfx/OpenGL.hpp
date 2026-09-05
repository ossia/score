#pragma once

#include <QSurfaceFormat>
#if __has_include(<private/qshader_p.h>)
#include <private/qshader_p.h>
#endif

#include <score_lib_base_export.h>

namespace score
{
//! Install the default QSurfaceFormat score's RHI needs, where that can be
//! decided without probing (Apple, embedded GLES). No-op elsewhere: desktop GL
//! is probed in the application.
//!
//! MUST run before the first QOpenGLContext or QWindow exists -- it is the
//! DEFAULT format that decides which profile Apple hands out, and
//! score::gfx::Window builds its own format from QSurfaceFormat::defaultFormat().
//!
//! Lives here rather than in main.cpp because the TESTS need it too: the app
//! sets its profile in setup_opengl(), which no test bootstrap reaches, so
//! every test touching the OpenGL backend on macOS ran on Apple's legacy
//! profile -- GL 2.1 / GLSL 1.20 -- instead of the 4.1 core the app itself
//! uses. On that context QRhi has no texture arrays and no 3D textures, so
//! RenderList creation throws and no window ever presents a frame.
SCORE_LIB_BASE_EXPORT void setupDefaultOpenGLFormat() noexcept;

struct SCORE_LIB_BASE_EXPORT GLCapabilities
{
public:
  GLCapabilities();

  int major{};
  int minor{};
  int shaderVersion{};
  QSurfaceFormat::RenderableType type{};

#if __has_include(<private/qshader_p.h>)
  QShaderVersion qShaderVersion;
#endif

  void setupFormat(QSurfaceFormat& fmt);
};
}
