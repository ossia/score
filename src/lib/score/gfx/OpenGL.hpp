#pragma once

#include <QSurfaceFormat>
#include <private/qshader_p.h>

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

  QShaderVersion qShaderVersion;

  void setupFormat(QSurfaceFormat& fmt);

private:
  int glShaderVersion() noexcept;
};
}
