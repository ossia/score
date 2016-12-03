#pragma once
#include <Curve/CurveStyle.hpp>

namespace Mapping
{
class Colors
{
public:
  Colors();

  const auto& style() const
  {
    return m_style;
  }

private:
  Curve::Style m_style;
};
}
