#pragma once
#include <Curve/CurveStyle.hpp>
#include <Curve/Process/CurveProcessPresenter.hpp>
#include <Interpolation/InterpolationProcess.hpp>
#include <Interpolation/InterpolationView.hpp>

namespace Interpolation
{
class Presenter final : public Curve::CurveProcessPresenter<ProcessModel, View>
{
    Q_OBJECT
public:
  Presenter(
      const Curve::Style& style,
      const ProcessModel& layer,
      View* view,
      const Process::ProcessPresenterContext& context,
      QObject* parent)
      : CurveProcessPresenter{style, layer, view, context, parent}
  {
    con(m_layer, &ProcessModel::addressChanged, this,
        &Presenter::on_nameChanges);
    con(m_layer.metadata(), &iscore::ModelMetadata::NameChanged,
        this, &Presenter::on_nameChanges);
    con(m_layer, &ProcessModel::tweenChanged, this,
        &Presenter::on_tweenChanges);

    m_view->setDisplayedName(m_layer.prettyName());
    m_view->showName(true);
    on_tweenChanges(m_layer.tween());
    con(layer.curve(), &Curve::Model::curveReset,
        this, [&] {
      on_tweenChanges(layer.tween());
    });
  }

private:
  void setFullView() override
  {
    m_curve.setBoundedMove(false);
  }

  void on_nameChanges()
  {
    m_view->setDisplayedName(m_layer.prettyName());
  }

  void on_tweenChanges(bool b)
  {
    for (Curve::SegmentView& seg : m_curve.segments())
    {
      if (seg.model().start().x() != 0.)
      {
        seg.setTween(false);
      }
      else
      {
        seg.setTween(b);
      }
    }
  }

};
}
