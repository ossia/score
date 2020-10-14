#pragma once
#include <Curve/Palette/CurvePoint.hpp>
#include <Curve/Point/CurvePointView.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <score/statemachine/CommonSelectionState.hpp>
#include <score/statemachine/GraphicsSceneTool.hpp>

#include <QGraphicsItem>
#include <QPoint>

namespace Curve
{
class ToolPalette;

class CurveTool : public GraphicsSceneTool<Curve::Point>
{
public:
  CurveTool(const Curve::ToolPalette&);

protected:
  template <typename PointFun, typename SegmentFun, typename NothingFun>
  void mapTopItem(
      QPointF scenePoint,
      const QGraphicsItem* pressedItem,
      PointFun pt_fun,
      SegmentFun seg_fun,
      NothingFun nothing_fun) const
  {
    if (!pressedItem)
    {
      nothing_fun();
      return;
    }

    switch (pressedItem->type())
    {
      case PointView::Type:
      {
        auto pt = safe_cast<const PointView*>(pressedItem);
        if (pt->contains(pt->mapFromScene(scenePoint)))
          pt_fun(pt);
        break;
      }

      case SegmentView::Type:
      {
        auto segt = safe_cast<const SegmentView*>(pressedItem);
        if (segt->contains(segt->mapFromScene(scenePoint)))
        {
          seg_fun(segt);
        }
        break;
      }

      default:
      {
        nothing_fun();
        break;
      }
    }
  }

  template <typename Model>
  void select(
      const Model& model,
      const Selection& selected,
      bool multi = CommonSelectionState::multiSelection())
  {
    score::SelectionDispatcher{context().selectionStack}.select(
        filterSelections(&model, selected, multi));
  }

  const Curve::ToolPalette& m_parentSM;
  const score::DocumentContext& context() const noexcept;
};
}
