#pragma once
#include <score_plugin_gfx_export.h>

namespace score::gfx
{
//! Keeps the display awake while any holder has acquired it.
class SCORE_PLUGIN_GFX_EXPORT IdleInhibitor
{
public:
  IdleInhibitor() = default;
  ~IdleInhibitor();
  IdleInhibitor(const IdleInhibitor&) = delete;
  IdleInhibitor& operator=(const IdleInhibitor&) = delete;

  void acquire();
  void release();
  bool held() const noexcept { return m_held; }

private:
  bool m_held{};
};
}
