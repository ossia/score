#pragma once

#include <QSurfaceFormat>
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

private:
  int glShaderVersion() noexcept;
};
}
