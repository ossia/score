#pragma once
#include "CurveTool.hpp"

#include <Curve/Palette/CommandObjects/MovePointCommandObject.hpp>
#include <Curve/Palette/CurvePoint.hpp>

#include <QPoint>

namespace Curve
{
class ToolPalette;
class OngoingState;
class SelectionState;

class SmartTool final : public Curve::CurveTool
{
public:
  explicit SmartTool(Curve::ToolPalette& sm, const score::DocumentContext& context);

  void on_pressed(QPointF scene, Curve::Point sp);
  void on_moved(QPointF scene, Curve::Point sp);
  void on_released(QPointF scene, Curve::Point sp);

private:
  Curve::SelectionState* m_state{};
  Curve::OngoingState* m_moveState{};

  bool m_nothingPressed = false;
  MovePointCommandObject m_co;
};
}
