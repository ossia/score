#pragma once
#include <algorithm>
#include <cmath>

namespace score::gfx::paircode
{
//! A bit shown as a pattern and its inverse, as a camera pixel saw them: one
//! when I > Iinv, clear when |I - Iinv| reaches `minContrast` of the pixel's
//! on - off span. Gray-code structured light and LED blink codes both read
//! their bits so.
struct Bit
{
  bool one{};
  bool clear{};
  float contrast{};
};

inline Bit read(double I, double Iinv, double span, double minContrast) noexcept
{
  const double c = std::abs(I - Iinv) / std::max(span, 1e-6);
  return {I > Iinv, c >= minContrast, float(c)};
}
}
