#pragma once

#include <QSurfaceFormat>
#if __has_include (<private/qshader_p.h>)
#include <private/qshader_p.h>
#endif

#include <score_lib_base_export.h>

namespace score
{
struct SCORE_LIB_BASE_EXPORT GLCapabilities
{
public:
  GLCapabilities();

  int major{};
  int minor{};
  int shaderVersion{};
  QSurfaceFormat::RenderableType type{};

#if __has_include (<private/qshader_p.h>)
  QShaderVersion qShaderVersion;
#endif

  void setupFormat(QSurfaceFormat& fmt);

private:
  int glShaderVersion() noexcept;
};
}
