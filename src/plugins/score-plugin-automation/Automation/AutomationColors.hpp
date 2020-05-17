#pragma once
#include <Curve/CurveStyle.hpp>

#include <score_plugin_automation_export.h>

namespace Automation
{
class SCORE_PLUGIN_AUTOMATION_EXPORT Colors
{
public:
  Colors(const score::Skin& s);

  const auto& style() const { return m_style; }

private:
  Curve::Style m_style;
};
}
