#pragma once
#include <Process/ProcessContext.hpp>

#include <Curve/CurveStyle.hpp>
#include <Curve/Process/CurveProcessPresenter.hpp>

#include <Mapping/MappingModel.hpp>
#include <Mapping/MappingView.hpp>

#include <verdigris>

namespace Mapping
{
class LayerPresenter final : public Curve::CurveProcessPresenter<ProcessModel, LayerView>
{
  W_OBJECT(LayerPresenter)
public:
  LayerPresenter(
      const Curve::Style& style, const ProcessModel& layer, LayerView* view,
      const Process::Context& context, QObject* parent)
      : CurveProcessPresenter{style, layer, view, context, parent}
  {
  }

  void parentGeometryChanged() override
  {
    QRectF rect = m_view->boundingRect();
    m_curve.view().setRect(rect);

    m_curve.view().setDefaultWidth(rect.width());
    m_curve.setRect(rect);
  }
};
}
