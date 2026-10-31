#pragma once
#include <score_plugin_gfx_export.h>

namespace score::gfx
{
class SCORE_PLUGIN_GFX_EXPORT IdleInhibitor
{
public:
  IdleInhibitor();
  ~IdleInhibitor();
  IdleInhibitor(const IdleInhibitor&) = delete;
  IdleInhibitor& operator=(const IdleInhibitor&) = delete;

  static int activeCount() noexcept;
};
}
