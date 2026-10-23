#include "ReadbackStats.hpp"

namespace score::gfx
{
ReadbackStats& readbackStats() noexcept
{
  static ReadbackStats stats;
  return stats;
}
}
