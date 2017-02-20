#pragma once
#include <Curve/Point/CurvePointView.hpp>
#include <Curve/Segment/CurveSegmentView.hpp>
#include <QQuickPaintedItem>
#include <QPoint>
#include <iscore/statemachine/GraphicsSceneTool.hpp>

#include <Curve/Palette/CurvePoint.hpp>

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
      const QQuickItem* pressedItem,
      PointFun pt_fun,
      SegmentFun seg_fun,
      NothingFun nothing_fun) const
  {
    if (!pressedItem)
    {
      nothing_fun();
      return;
    }

    auto obj = qobject_cast<const GraphicsItem*>(pressedItem);
    if(!obj)
    {
      nothing_fun();
      return;
    }
    switch (obj->type())
    {
      case PointView::static_type():
      {
        auto pt = safe_cast<const PointView*>(obj);
        if (pt->contains(pt->mapFromScene(scenePoint)))
          pt_fun(pt);
        break;
      }

      case SegmentView::static_type():
      {
        auto segt = safe_cast<const SegmentView*>(obj);
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

  const Curve::ToolPalette& m_parentSM;
};
}
