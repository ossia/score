// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "CurveTool.hpp"

#include <Curve/Palette/CurvePalette.hpp>

#include <score/statemachine/GraphicsSceneTool.hpp>

namespace Curve
{
CurveTool::CurveTool(const Curve::ToolPalette& csm)
    : GraphicsSceneTool<Curve::Point>{csm.scene()}
    , m_parentSM{csm}
{
}

const score::DocumentContext& CurveTool::context() const noexcept
{
  return m_parentSM.presenter().context();
}
}
