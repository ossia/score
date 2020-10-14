#pragma once
#include <Curve/CurveModel.hpp>
#include <Curve/CurvePresenter.hpp>
#include <Curve/CurveView.hpp>
#include <Curve/Palette/CurvePalette.hpp>

#include <score/statemachine/CommonSelectionState.hpp>

namespace Curve
{
class ToolPalette;
class SelectionState : public CommonSelectionState
{
private:
  QPointF m_initialPoint;
  QPointF m_movePoint;

  const Curve::ToolPalette& m_parentSM;
  View& m_view;

public:
  SelectionState(
      score::SelectionStack& stack,
      const Curve::ToolPalette& parentSM,
      View& view,
      QState* parent)
      : CommonSelectionState{stack, &view, parent}, m_parentSM{parentSM}, m_view{view}
  {
  }

  const QPointF& initialPoint() const { return m_initialPoint; }
  const QPointF& movePoint() const { return m_movePoint; }

  void on_pressAreaSelection() override { m_initialPoint = m_parentSM.scenePoint; }

  void on_moveAreaSelection() override
  {
    m_movePoint = m_parentSM.scenePoint;
    auto rect = QRectF{m_view.mapFromScene(m_initialPoint), m_view.mapFromScene(m_movePoint)}
                    .normalized();

    m_view.setSelectionArea(rect);
    setSelectionArea(rect);
  }

  void on_releaseAreaSelection() override
  {
    if (m_parentSM.scenePoint == m_initialPoint)
    {
      dispatcher.deselect();
      auto proc = (IdentifiedObjectAbstract*)m_parentSM.model().parent();
      dispatcher.select(Selection{proc});
    }
    m_view.setSelectionArea(QRectF{});
  }

  void on_deselect() override
  {
    dispatcher.deselect();
    m_view.setSelectionArea(QRectF{});
  }

private:
  void setSelectionArea(QRectF scene_area)
  {
    using namespace std;
    Selection sel;

    for (const auto& point : m_parentSM.presenter().points())
    {
      if (point.shape().translated(point.pos()).intersects(scene_area))
      {
        sel.append(point.model());
      }
    }

    for (const auto& segment : m_parentSM.presenter().segments())
    {
      if (segment.shape().translated(segment.pos()).intersects(scene_area))
      {
        sel.append(segment.model());
      }
    }

    dispatcher.select(
        filterSelections(sel, m_parentSM.model().selectedChildren(), multiSelection()));
  }
};
}
